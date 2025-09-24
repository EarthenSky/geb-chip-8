# Chip-8 Devlog

Chip-8 is a programming language / virtual machine for writing small games, originally developed in the mid-1970s. This is a short devlog for making and optimizing a chip-8 interpreter.

# ACT I

## first steps

http://devernay.free.fr/hacks/chip8/C8TECH10.HTM is a lovely specification that details the machine and the instructions. It's important to note that there is no "official" specification for Chip-8, so you have to be sure the program you're running agrees with this specification. https://en.wikipedia.org/wiki/CHIP-8 is a good second source.

We'll be writing the interpreter in C++20, using SDL3 to render the screen.

A few related devlogs
- https://austinmorlan.com/posts/chip8_emulator/#16-bit-program-counter
- https://tobiasvl.github.io/blog/write-a-chip-8-emulator/

## memory

In Chip-8, the interpreter, program, and general memory all share the same address space. There are 4096 bytes of addressable memory, the first 512 of which were reserved for the interpreter. As an added bonus, the whole thing fits in the L1 cache of decades old processors!

```cpp
std::array<uint8_t, 4096> memory;
```

When we want to run a Chip-8 program, we'll write the instructions byte by byte into memory. YES YOU'RE RIGHT, THIS IS DANGEROUS! Since we're sharing program and general memory, programs can modify their own code. In reality since Chip-8 programs are so small, it's not a massive problem (whew). Although, it can be a little bonus!

## instructions

All Chip-8 instructions are 2 bytes wide. After a non-branching instruction is executed, the program counter will be incremented.

```cpp
uint16_t program_counter;
```

In the following headers, each `x`, `y`, and `z` refer to a 4 input bits in the instruction.

### `SYS addr` [0xxx]

Oh jeez, starting off with a confusing one. It looks like **this instruction enables certain machines to run a SYStem's native executable procedure.** Imagining it on modern computing devices, it would allow us to call into 4096 bytes of procedures written in x86-64 assembly. The details of how this would work aren't immediately clear to me, but it would make a program necessarily non-portable, so we'll consider this a no-op, as seems to be common practice.

// TODO: maybe makes more sense to raise a runtime error?

```cpp
void sys(uint16_t address) {
    program_counter += 1;
}
```

### `CLS` [00e0]

**This instruction CLears the Screen.** The Chip-8 display is made of 64 by 32 monochrome pixels. Thus, we interpret "clear" to mean that all pixels should be turned to the off state. What colour a pixel is in the off vs on state is up to the implementation. I think it's reasonable to have the off state be dark, and the on state to be light.

```cpp
#include <algorithm>
void cls() {
    std::ranges::fill(chip8_display.buffer, false);
    chip8_display.render_buffer();
    program_counter += 1;
}
```

See [sdl integration](##sdl-integration) for the definition of `chip8_display`.

### `RET` [00ee]

Chip-8 supports calling functions using a 16 address stack. Thus, function calls may be nested 16 deep. **This instruction RETurns from a subroutine**, decrementing the stack pointer and moving the instruction pointer to the address popped from the stack. The new address will be the original call site of the function.

```cpp
// store the address the interpreter should return to after a subroutine
std::array<uint16_t, 16> stack_frames;
uint8_t stack_pointer = 0;

void ret() {
    if (stack_pointer == 0)
        throw std::runtime_error("cannot RET when stack is empty");

    program_counter = stack_frames[stack_pointer];
    stack_pointer -= 1;
}
```

### `JP addr` [1xxx]

**This instructions JumPs to the provided address.**

```cpp
void jp(uint16_t address) {
    program_counter = address;
}
```

### `CALL addr` [2xxx]

**This instruction CALLs the function beginning at the provided address.**

```cpp
void call(uint16_t target_address) {
    if (stack_pointer > 15)
        throw std::runtime_error("cannot CALL when stack is full (stack overflow!)");

    // TODO: what if program_counter is 4096-1 ? do we overflow, or raise an error?
    stack_frames[stack_pointer] = program_counter + 1;
    stack_pointer += 1;
    program_counter = target_address;
}
```

### `SE Vx value` [3xyy]

Chip-8 has 16 general purpose registers. **This command Skips the next instruction if register x Equals value (`Vx = value`).**

```cpp
const size_t NUM_GP_REGISTERS = 16;
uint8_t gp_registers[NUM_GP_REGISTERS];

void skip_equal(uint8_t reg, uint8_t value) {
    if (reg > 15)
        throw std::runtime_error("invalid register number");

    if (gp_registers[reg] == value) {
        program_counter += 2;
    } else {
        program_counter += 1;
    }
}
```

### `SNE Vx value` [4xyy]

**This command Skips the next instruction if register x does Not Equal value (`Vx != value`).**

```cpp
void skip_not_equal(uint8_t reg, uint8_t value) {
    if (reg > 15)
        throw std::runtime_error("invalid register number");

    if (gp_registers[reg] != value) {
        program_counter += 2;
    } else {
        program_counter += 1;
    }
}
```

### `SE Vx Vy` [5xy0]

**This command Skips the next instruction if register x Equals register y.**

```cpp
void skip_equal_reg(uint8_t reg_a, uint8_t reg_b) {
    if (reg_a > 15 || reg_b > 15)
        throw std::runtime_error("invalid register number");

    if (gp_registers[reg_a] == gp_registers[reg_b]) {
        program_counter += 2;
    } else {
        program_counter += 1;
    }
}
```

### `LD Vx value` [6xyy]

**This command LoaDs value into register x.**

```cpp
void load(uint8_t reg, uint8_t value) {
    if (reg > 15)
        throw std::runtime_error("invalid register number");

    gp_registers[reg] = value;
    program_counter += 1;
}
```

### `ADD Vx, value` [7xkk]

**This command increments register x by (ADDing) value.**

```cpp
void increment(uint8_t reg, uint8_t value) {
    if (reg > 15)
        throw std::runtime_error("invalid register number");

    gp_registers[reg] += value;
    program_counter += 1;
}
```

### `LD Vx Vy` [8xy0]

**This command LoaDs the value of register y into register x.**

```cpp
void load_reg(uint8_t reg_a, uint8_t reg_b) {
    if (reg_a > 15 || reg_b > 15)
        throw std::runtime_error("invalid register number");

    gp_registers[reg_a] = gp_registers[reg_b];
    program_counter += 1;
}
```

### `OR Vx Vy` [8xy1]
### `AND Vx Vy` [8xy2]
### `XOR Vx Vy` [8xy3]

**This command stores of the result of bitwise OR between registers x and y into x.**
**This command stores of the result of bitwise AND between registers x and y into x.**
**This command stores of the result of bitwise XOR between registers x and y into x.**

```cpp
void bitwise_or(uint8_t reg_a, uint8_t reg_b) {
    if (reg_a > 15 || reg_b > 15)
        throw std::runtime_error("invalid register number");

    gp_registers[reg_a] |= gp_registers[reg_b];
    program_counter += 1;
}
```

```cpp
void bitwise_and(uint8_t reg_a, uint8_t reg_b) {
    if (reg_a > 15 || reg_b > 15)
        throw std::runtime_error("invalid register number");

    gp_registers[reg_a] &= gp_registers[reg_b];
    program_counter += 1;
}
```

```cpp
void bitwise_xor(uint8_t reg_a, uint8_t reg_b) {
    if (reg_a > 15 || reg_b > 15)
        throw std::runtime_error("invalid register number");

    gp_registers[reg_a] ^= gp_registers[reg_b];
    program_counter += 1;
}
```

### `ADD Vx Vy` [8xy4]

**This command increments register x by (ADDing) register y, setting register 0xf to be the carry (overflow) bit.**

```cpp
void carry_increment_reg(uint8_t reg_a, uint8_t reg_b) {
    if (reg_a > 15 || reg_b > 15)
        throw std::runtime_error("invalid register number");

    gp_registers[reg_a] += gp_registers[reg_b];
    gp_registers[0xf] = ((uint16_t) gp_registers[reg_a] + (uint16_t) gp_registers[reg_b]) > 0xff;
    program_counter += 1;
}
```

### `SUB Vx Vy` [8xy5]

**This command decreases register x by (SUBtracting) register x, setting register 0xf to 1 if there is no underflow.**

```cpp
void carry_decrement_reg(uint8_t reg_a, uint8_t reg_b) {
    if (reg_a > 15 || reg_b > 15)
        throw std::runtime_error("invalid register number");

    // NOTE: some sources say this should be > while others say >=. I'm using >= b/c it 
    // makes more sense wrt underflow.
    gp_registers[0xf] = gp_registers[reg_a] >= gp_registers[reg_b];
    gp_registers[reg_a] -= gp_registers[reg_b];
    program_counter += 1;
}
```

### `SHR Vx` [8x_6]

**This command performs a SHift Right to register x, setting register 0xf to something weird.**

```cpp
void shift_right(uint8_t reg) {
    if (reg > 15)
        throw std::runtime_error("invalid register number");

    gp_registers[reg_a] >>= 1;
    // don't worry about endianness b/c it's just 1 byte
    gp_registers[0xf] = 0x01 & gp_registers[reg_a];
    program_counter += 1;
}
```

### `SUBN Vx Vy` [8xy7]

**This command sets register x equal to register y SUBtracted by register x, setting register 0xf to 1 if there is no underflow. This is the iNverse of 8xy5.**

```cpp
void subtract_reversed(uint8_t reg_a, uint8_t reg_b) {
    if (reg_a > 15 || reg_b > 15)
        throw std::runtime_error("invalid register number");

    gp_registers[0xf] = gp_registers[reg_b] >= gp_registers[reg_a];
    gp_registers[reg_a] = gp_registers[reg_b] - gp_registers[reg_a];
    program_counter += 1;
}
```

### `SHL Vx` [8x_e]

**This command performs a SHift Left to register x, setting register 0xf to something weird.**

```cpp
void shift_left(uint8_t reg) {
    if (reg > 15)
        throw std::runtime_error("invalid register number");
    
    gp_registers[0xf] = 0x80 & gp_registers[reg] != 0;
    gp_registers[reg] <<= 1;
    program_counter += 1;
}
```

### `SNE Vx Vy` [9xy0]

**This command Skips the next instruction if register x and y are Not Equal.**

```cpp
void skip_not_equal_reg(uint8_t reg_a, uint8_t reg_b) {
    if (reg_a > 15 || reg_b > 15)
        throw std::runtime_error("invalid register number");
    
    if (gp_registers[reg_a] != gp_registers[reg_b]) {
        program_counter += 2;
    } else {
        program_counter += 1;
    }
}
```

### `LD I addr` [axxx]

Chip-8 has a single 16 bit register, the i register, used for loading addresses. **This command LoaDs the provided address into the i register.**

```cpp
uint16_t i_register;
void load_address(uint16_t address) {
    i_register = address;
    program_counter += 1;
}
```

### `JP V0 addr` [bxxx]

**This command JumPs to the location addr + register 0.**

```cpp
void jump_reg0(uint16_t address) {
    // TODO: what to do if we get outside the range of addresses?
    program_counter = (uint16_t)gp_registers[0] + address;
}
```

### `RND Vx value` [cxyy]

**This command generates a RaNDom integer in `[0, 255]`, which is then applied bitwise and against value. The result is stored in register x.**

```cpp
#include <random>

// setup prng
std::random_device rand_dev;
std::default_random_engine prng_engine(rand_dev());
std::uniform_int_distribution<uint8_t> random_u8_dist;

void random_int(uint8_t reg, uint8_t value) {
    if (reg > 15)
        throw std::runtime_error("invalid register number");

    gp_registers[reg] = random_u8_dist(prng_engine) & value;
    program_counter += 1;
}
```

### `DRW Vx Vy value` [dxyz]

Displaying of pixels in Chip-8 occurs via sprites. Sprites are written to memory, then blitted to the screen. Sprites in Chip-8 are made of bytes, where each byte is a row of pixels of length 8, and may be up to 15 bytes in total. Thus, sprites always have width 8.

**This command DRaWs a sprite made of value bytes, starting at the memory location stored in register i, at the screen coordinates stored in registers x and y. Set register 0xf to 1 if any pixels are erased, 0 otherwise.**

```cpp
const uint8_t SPRITE_WIDTH = 8;

// NOTE: these are all u4
// TODO: create a better type?
void draw(uint8_t reg_x, uint8_t reg_y, uint8_t value) {
    if (reg_x > 15 || reg_y > 15)
        throw std::runtime_error("invalid register number");

    // assume no pixels are modified, unless we observe it
    gp_registers[0xf] = 0;

    // upper left position
    uint8_t ul_xpos = gp_registers[reg_x];
    uint8_t ul_ypos = gp_registers[reg_y];
    for (size_t row_i = 0; row_i < value; row_i++) {
        size_t row = memory[i_register + row_i];
        for (size_t bit_i = 0; bit_i < SPRITE_WIDTH; bit_i++) {
            // sprites wrap around the display
            auto xpos = (ul_xpos + bit_i) % Chip8Display::SCREEN_WIDTH;
            auto ypos = (ul_ypos + row_i) % Chip8Display::SCREEN_HEIGHT;
            
            bool before = chip8_display.buffer[xpos + ypos * Chip8Display::SCREEN_WIDTH];
            bool after = (chip8_display.buffer[xpos + ypos * Chip8Display::SCREEN_WIDTH] ^= (row & (0x80 >> bit_i)) != 0);
            if (before && !after)
                gp_registers[0xf] = 1;
        }
    }

    chip8_display.render_buffer();

    // TODO: is there a fancy way to do this & the 0xf register thingy using only a single thread (& fancy function)?
    /* std::transform(
        screen.begin() + screen_index,
        screen.begin() + screen_index + value,
        memory.begin() + i_register,
        screen.begin() + screen_index,
        [](uint8_t x, uint8_t y){ return x ^ y; }
    );*/

    program_counter += 1;
}
```

See [sdl integration](##sdl-integration) for the definition of `chip8_display`.

### `SKP Vx` [ex9e]

Computers for which Chip-8 was originally developed had 16-key hexadecimal keypads. **This command Skips the next instruction if a Key with the value in register x is Pressed.**

```cpp
void skip_if_key_press(uint8_t reg) {
    if (reg > 15)
        throw std::runtime_error("invalid register number");

    if (chip8_keyboard.is_key_pressed(gp_registers[reg] % 16)) {
        program_counter += 2;
    } else {
        program_counter += 1;
    }
}
```

See [sdl integration](##sdl-integration) for the definition of `chip8_keyboard`.

### `SKNP Vx` [exa1]

**This command Skip the next instruction if a Key with the value in register x is Not Pressed.**

```cpp
void skip_if_not_key_press(uint8_t reg) {
    if (reg > 15)
        throw std::runtime_error("invalid register number");

    if (!chip8_keyboard.is_key_pressed(gp_registers[reg] % 16)) {
        program_counter += 2;
    } else {
        program_counter += 1;
    }
}
```

### `LD Vx DT` [fx07]

Chip-8 has two 8-bit timer registers: the delay and sound timers. At a rate of 60hz, the value in the delay and sound timers will decrease by 1, if they are not already 0. **This command LoaDs the value from the delay timer into register x.**

```cpp
void load_from_delay_timer(uint8_t reg) {
    if (reg > 15)
        throw std::runtime_error("invalid register number");

    gp_registers[reg] = delay_timer.value();
    program_counter += 1;
}
```

See [sdl integration](##sdl-integration) for the definition of `delay_timer`.

### `LD Vx NextKey` [fx0a]

**This command waits (blocks), then LoaDs the value of the next key press into register x.**

```cpp
void load_from_next_keypress(uint8_t reg) {
    if (reg > 15)
        throw std::runtime_error("invalid register number");

    uint8_t key = chip8_keyboard.block_until_next_keypress(true);
    if (key > 15)
        throw std::runtime_error("invalid key value");

    gp_registers[reg] = key;
    program_counter += 1;
}
```

### `LD DT Vx` [fx15]

**This command LoaDs the delay timer with the value of register x.**

```cpp
void set_delay(uint8_t reg) {
    if (reg > 15)
        throw std::runtime_error("invalid register number");

    delay_timer.set(gp_registers[reg]);
    program_counter += 1;
}
```

### `LD ST Vx` [fx18]

**This command LoaDs the sound timer with the value of register x.**

```cpp
void set_sound(uint8_t reg) {
    if (reg > 15)
        throw std::runtime_error("invalid register number");

    sound_timer.set(gp_registers[reg]);
    program_counter += 1;
}
```

See [sdl integration](##sdl-integration) for the definition of `sound_timer`.

### `ADD I Vx` [fx1e]

**This command increments the i register by the value in register x.**

```cpp
void increment_i_reg(uint8_t reg) {
    if (reg > 15)
        throw std::runtime_error("invalid register number");
    
    i_register += gp_registers[reg];
    program_counter += 1;
}
```

### `LD I Sprite[Vx]` [fx29]

Chip-8 has some built-in sprites for the 16 hex characters 0x0 through 0xf, stored in the interpreter's memory area. **This command LoaDs the i register with the address of sprite for the digit stored in register x.**

```cpp
const uint16_t BUILT_IN_CHAR_STARTING_ADDRESS = 0x100;
std::ranges::copy({0xf0, 0x90, 0x90, 0x90, 0xf0}, memory.begin() + BUILT_IN_CHAR_STARTING_ADDRESS + 5 * 0x0);
std::ranges::copy({0x20, 0x60, 0x20, 0x20, 0x70}, memory.begin() + BUILT_IN_CHAR_STARTING_ADDRESS + 5 * 0x1);
std::ranges::copy({0xf0, 0x10, 0xf0, 0x80, 0xf0}, memory.begin() + BUILT_IN_CHAR_STARTING_ADDRESS + 5 * 0x2);
std::ranges::copy({0xf0, 0x10, 0xf0, 0x10, 0xf0}, memory.begin() + BUILT_IN_CHAR_STARTING_ADDRESS + 5 * 0x3);
std::ranges::copy({0x90, 0x90, 0xf0, 0x10, 0x10}, memory.begin() + BUILT_IN_CHAR_STARTING_ADDRESS + 5 * 0x4);
std::ranges::copy({0xf0, 0x80, 0xf0, 0x10, 0xf0}, memory.begin() + BUILT_IN_CHAR_STARTING_ADDRESS + 5 * 0x5);
std::ranges::copy({0xf0, 0x80, 0xf0, 0x90, 0xf0}, memory.begin() + BUILT_IN_CHAR_STARTING_ADDRESS + 5 * 0x6);
std::ranges::copy({0xf0, 0x10, 0x20, 0x40, 0x40}, memory.begin() + BUILT_IN_CHAR_STARTING_ADDRESS + 5 * 0x7);
std::ranges::copy({0xf0, 0x90, 0xf0, 0x90, 0xf0}, memory.begin() + BUILT_IN_CHAR_STARTING_ADDRESS + 5 * 0x8);
std::ranges::copy({0xf0, 0x90, 0xf0, 0x10, 0xf0}, memory.begin() + BUILT_IN_CHAR_STARTING_ADDRESS + 5 * 0x9);
std::ranges::copy({0xf0, 0x90, 0xf0, 0x90, 0x90}, memory.begin() + BUILT_IN_CHAR_STARTING_ADDRESS + 5 * 0xa);
std::ranges::copy({0xe0, 0x90, 0xe0, 0x90, 0xe0}, memory.begin() + BUILT_IN_CHAR_STARTING_ADDRESS + 5 * 0xb);
std::ranges::copy({0xf0, 0x80, 0x80, 0x80, 0xf0}, memory.begin() + BUILT_IN_CHAR_STARTING_ADDRESS + 5 * 0xc);
std::ranges::copy({0xe0, 0x90, 0x90, 0x90, 0xe0}, memory.begin() + BUILT_IN_CHAR_STARTING_ADDRESS + 5 * 0xd);
std::ranges::copy({0xf0, 0x80, 0xf0, 0x80, 0xf0}, memory.begin() + BUILT_IN_CHAR_STARTING_ADDRESS + 5 * 0xe);
std::ranges::copy({0xf0, 0x80, 0xf0, 0x80, 0x80}, memory.begin() + BUILT_IN_CHAR_STARTING_ADDRESS + 5 * 0xf);

void load_sprite(uint8_t reg) {
    if (reg > 15)
        throw std::runtime_error("invalid register number");

    i_register = BUILT_IN_CHAR_STARTING_ADDRESS + 5 * (gp_registers[reg] % 16);
    program_counter += 1;
}
```

### `LDB Vx` [fx33]

**This command LoaDs the Binary coded decimal (BCD) representation of the value in register x into memory locations I, I+1, I+2.**

```cpp
void load_bcd(uint8_t reg) {
    if (reg > 15)
        throw std::runtime_error("invalid register number");

    // TODO: should I check whether the memory location is valid?
    memory[i_register] = gp_registers[reg] % 10;
    memory[i_register+1] = (gp_registers[reg] % 100 - gp_registers[reg] % 10) / 10;
    memory[i_register+2] = (gp_registers[reg] - gp_registers[reg] % 100) / 100;
    program_counter += 1;
}
```

### `LDRM Vx` [fx55]

**This command LoaDs Registers 0 through x to Memory, starting at the address stored in the i register.**

```cpp
void load_reg_to_mem(uint8_t reg_final) {
    if (reg_final > 15)
        throw std::runtime_error("invalid register number");

    // TODO: should I check whether the memory locations are all valid?
    for (uint8_t i = 0; i <= reg_final; i++) {
        memory[i_register+i] = gp_registers[i];
    }
    program_counter += 1;
}
```

### `LDMR Vx` [fx65]

**This command LoaDs values from memory starting at the i register, into registers 0 through x.**

```cpp
void load_mem_to_reg(uint8_t reg_final) {
    if (reg_final > 15)
        throw std::runtime_error("invalid register number");

    // TODO: should I check whether the memory locations are all valid?
    for (uint8_t i = 0; i <= reg_final; i++) {
        gp_registers[i] = memory[i_register+i];
    }
    program_counter += 1;
}
```

## sdl integration

Next, we need to be able to:
1. Render our display
2. Get keyboard input
3. Run timers
4. Play our sfx when the sound timer is non-zero

### (1) render display

The only blocking instruction is `fx0a`, so we can update the display whenever it's modified. Since the timers run at 60hz, we only need to be careful that no instruction takes a significant portion of that time, otherwise graphics will render weirdly. If we limit each instruction to take no longer than 1000ms/60hz, that gives roughly 16ms per operation. An easy target!

In the following snippet we setup SDL3, then create a simple a class with an interface allowing the display buffer to be modified and rendered.

```cpp
#include <SDL3/SDL.h>

class Chip8Display {
private:
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    // the edge size of a pixel rendered on the native display
    const size_t scale_factor = 4;

public:
    Chip8Display() {
        if (!SDL_Init(SDL_INIT_VIDEO))
            throw std::runtime_error(std::format("SDL_Init error: {}\n", SDL_GetError()));

        if (!SDL_CreateWindowAndRenderer(
            "Chip8 Display",
            Chip8Display::SCREEN_WIDTH  * this->scale_factor,
            Chip8Display::SCREEN_HEIGHT * this->scale_factor,
            0, &this->window, &this->renderer
        ))
            throw std::runtime_error(std::format("SDL_CreateWindowAndRenderer error: {}\n", SDL_GetError()));

        SDL_SetRenderVSync(this->renderer, SDL_RENDERER_VSYNC_ADAPTIVE);
    }

    void render_buffer() {
        // TODO: later, consider a more efficient way to send this data to the gpu & render it
        for (uint16_t y = 0; y < Chip8Display::SCREEN_HEIGHT; y++) {
            for (uint16_t x = 0; x < Chip8Display::SCREEN_WIDTH; x++) {
                bool px_active = buffer[x + y * Chip8Display::SCREEN_WIDTH];
                if (px_active)
                    SDL_SetRenderDrawColor(this->renderer, 255, 255, 255, 255);
                else
                    SDL_SetRenderDrawColor(this->renderer, 25, 25, 25, 255);

                const SDL_FRect pixel_bounds{
                    x * this->scale_factor,
                    y * this->scale_factor,
                    this->scale_factor, this->scale_factor
                };
                SDL_RenderFillRect(this->renderer, &pixel_bounds);
            }
        }

        SDL_RenderPresent(renderer);
    }

    const uint16_t SCREEN_WIDTH = 64;
    const uint16_t SCREEN_HEIGHT = 32;
    std::array<bool, SCREEN_WIDTH * SCREEN_HEIGHT> buffer;

    ~Chip8Display() {
        SDL_DestroyRenderer(this->renderer);
        SDL_DestroyWindow(this->window);
        SDL_Quit();
    }
};

Chip8Display chip8_display;
```

The above class addresses the 2 rendering instructions: `00e0` and `dxyz`.

### (2) get keyboard input

Despite that hexadecimal keypads were originally organized in a square (kinda like a phone), we will simply map by key. Surely we can add explicit keybinding later.

In the following snippet we create a keyboard class with a function to request the next key-down, and a function to get the current state. Since we don't want our input queue to ever get too full, we spawn a thread to deal with it in (mostly) realtime. We then create a short single-channel class to ease communication between the two.

```cpp
#include <atomic>
#include <thread>
#include <optional>

enum Key {
    K0 = 0,
    K1, K2, K3,
    K4, K5, K6,
    K7, K8, K9,
    KA, KB, KC,
    KD, KE, KF
};

/// @brief a channel allowing a consumer to request the next item from the producer, and the producer to decide when to
/// accept a request.
template <typename T>
class ChannelCoordinator {
private:
    std::atomic<bool> is_request_pending = false;
    std::optional<T> message = std::nullopt;

    std::mutex channel_lock;
    std::condition_variable wait_for_response;

public:
    ChannelCoordinator() {}

    T request() {
        std::unique_lock lock(this->channel_lock);
        this->is_request_pending = true;
        this->wait_for_response.wait(lock, [&message]{ return message.has_value(); });

        // hold this lock until the end of this function in case another request is made in between & we skip a response!
        T response = this->message.value();
        this->message = std::nullopt;
        return response;
    }

    /// @brief will only send & copy data if requested, otherwise will not
    void send_if_requested(T&& data) {
        std::lock_guard lock(this->channel_lock);
        if (this->is_request_pending) {
            this->next_key = std::optional(std::forward<T>(data));
            this->is_request_pending = false;
            this->wait_for_response.notify_one();
        }
    }
};

class Chip8Keyboard {
private:
    // never need to update more than 1 key at once
    // true means down
    std::array<std::atomic<bool>, 16> keyboard_state;
    
    std::jthread poll_events_thread(poll_events);

    ChannelCoordinator<Key> key_channel;

    void poll_events(std::stop_token stop_token) {
        while (true) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (stop_token.stop_requested())
                    return;

                if (event->type == SDL_EVENT_QUIT) {
                    std::cout << std::string("Exiting..." + std::endl);
                    exit(1);
                } else if (
                    event->type == SDL_EVENT_KEY_DOWN
                    || event->type == SDL_EVENT_KEY_UP
                ) {
                    // https://wiki.libsdl.org/SDL3/SDL_Keycode
                    usize_t key_i;
                    auto key = event->key.key;
                    if (key >= SDLK_0 && key <= SDLK_9) {
                        key_i = 0x0 + (key - SDLK_0);
                    } else if (key >= SDLK_a && key <= SDLK_f) {
                        key_i = 0xa + (key - SDLK_a);
                    } else {
                        // invalid keydown doesn't lock mutex
                        continue;
                    }

                    if (event->type == SDL_EVENT_KEY_DOWN)
                        key_channel.send_if_requested(static_cast<Key>(key_i));

                    this->keyboard_state[key_i] = (event->type == SDL_EVENT_KEY_DOWN);
                }
            }

            // In the worst case, sleep may wait up to 15ms, which is still 60hz, so we should be fine!
            // In the best case, we get 1000/(0.5) = 2000hz, which is super
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    }

public:
    Chip8Keyboard() = default;
    ~Chip8Keyboard() = default;

    bool is_key_pressed(Key key) const {
       return keyboard_state[static_cast<usize_t>(key)];
    }

    Key block_until_next_keypress() {
        // blocks the next keybaord event from starting, or waits until it is done
        return this->key_channel.request();
    }
};

Chip8Keyboard chip8_keyboard;
```

The above class addresses the 3 keyboard instructions: `ex9e`, `exa1`, and `fx0a`.

### (3) Run timers

Instead of sleeping the current thread, we maintain a timestamp for when operations are applied. Thus, we can figure out what value the timer "should be" based on how much time has elapsed. This is much less overhead, which is great! 

The following snippet creates both the sound and delay timers.

```cpp
#include <chrono>

class Timer60hz {
private:
    uint8_t _value;
    std::chrono::time_point<std::chrono::steady_clock> timestamp;

public:
    uint8_t value() {
        if (this->_value == 0)
            return this->_value;

        // timer should decrease by 60 per 1000 * 1000 us
        // we can reasonably round this to 16667 us per value
        auto current = std::chrono::steady_clock::now();
        auto duration = duration_cast<std::chrono::microseconds>(current - this->timestamp).count();
        // separate this into 1s amounts (no error) and smaller amounts (few tick errors b/c 16667 != 16666.66666...)
        size_t value_elapsed = 60 * (duration / (1000*1000)) + ((duration % (1000*1000)) / 16667);
        if (value_elapsed >= duration) {
            return 0;
        } else {
            return this->_value - value_elapsed;
        }
    }
    void set(uint8_t new_value) {
        this->_value = new_value;
        this->timestamp = std::chrono::steady_clock::now();
    }
};

Timer60hz sound_timer;
Timer60hz delay_timer;
sound_timer.set(0);
delay_timer.set(0);
```

The above class addresses the 3 timer instructions: `fx07`, `fx15`, and `fx18`.

### (4) Play sounds

A 200hz square wave sounds lovely!

The following speaker class registers a callback to an SDL3 stream, in which it supplies data based on the current state of the sound timer, held by reference. It's possible that the sound register can be modified quickly enough that the callback doesn't notice and doesn't update the changes to the speaker, however the errors will be too small for people to notice as they will (likely) on the order of several ms (depending on how frequently audio data is requested).

// TODO: It's not clear how many ms it will be. Actually, it could be really long; even on the order of 300ms! TODO: do some testing, or look further into 
// https://github.com/libsdl-org/SDL/blob/9ad04ff69e2868f2ad947365727f33ff74851802/src/audio/SDL_audiocvt.c#L1362C34-L1362C61

```cpp
#include <SDL3/SDL_audio.h>
#include <algorithm>

class Chip8Speaker {
private:
    const static SDL_AudioSpec OUTPUT_SPEC = {
        SDL_AUDIO_U8,
        1,
        // TODO: increase sample rate so our sfx sounds more like a square wave!
        400
    };

    SDL_AudioDeviceID device_id = 0;
    SDL_AudioStream* out_stream = nullptr;

    Timer60hz& sound_timer;

    void out_stream_callback(void*, SDL_AudioStream *stream, int additional_amount, int total_amount) {
        if (additional_amount == 0)
            return;

        uint8_t value = this->sound_timer.value();

        std::array<uint8_t, additional_amount> samples;
        std::ranges::fill(samples, 0);

        // 400hz sample rate, 60hz timer
        // each timer value represents the same time-period as 6.66666... samples
        size_t limit = std::min((size_t) (value * (400.0 / 60.0)), samples.size());
        for (size_t i = 0; i < limit; i++) {
            // Since our sample rate is 400hz, we should expect to get a 200hz wave that is somewhat sine/square shaped.
            // The actual shape depends on SDL3's built-in resampler.
            if (i % 2 == 0)
                samples[i] == 0x40;
        }

        // TODO: remember the last queued sample to decide on phase of the wave (for now, we'll just get lil blips and
        // that's okay)

        SDL_PutAudioStreamData(stream, samples.data(), samples.size() * sizeof(uint8_t));
    }

public:
    Chip8Speaker(Timer60hz& sound_timer) : sound_timer(sound_timer) {
        this->device_id = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
        if (this->device_id == 0)
            throw std::runtime_error(std::format("SDL_OpenAudioDevice failed with: {}", SDL_GetError()));

        this->out_stream = SDL_CreateAudioStream(nullptr, &Chip8Speaker::OUTPUT_SPEC);
        if (this->out_stream == nullptr)
            throw std::runtime_error(std::format("SDL_CreateAudioStream failed with: {}", SDL_GetError()));

        if (!SDL_BindAudioStream(this->device_id, this->out_stream))
            throw std::runtime_error(std::format("SDL_BindAudioStream failed with: {}", SDL_GetError()));

        SDL_SetAudioStreamGetCallback(this->out_stream, out_stream_callback, nullptr);
    }
    ~Chip8Speaker() {
        SDL_DestroyAudioStream(this->out_stream);
        SDL_CloseAudioDevice(this->device_id);
    }
};
```

## put it all together!

Now that we have all our separate components let's put them all together, then provide an interface that accepts a program in memory.

### add all instructions to a class

The emulator class will be the root of our abstraction, allowing the emulator to be embedded in all sorts of situations.

Improvements:
- I realized I could remove a lot of error checking by using bitfields to make my own unsigned 4 bit integer, `u4`.
- Fixed a big error where I forgot that instructions are 2 bytes, not 1! Thus, the program counter will increase by 2 bytes by default, not 1. However, this also brings to light another implicit requirement: "In memory, the first byte of each instruction should be located at an even addresses". Since sprites may take up an odd number of bytes, this is a requirement to program writers! We'll touch on this later when we get to static analysis, but for now we can just note it.
  - TODO: maybe we'll wait until we see what existing programs do!

### execute instructions

After the laborious task of adding all the instructions into the `Emulator` class, we now need a function that breaks down an instruction's arguments and passes them to their function. Since instructions are exactly 2 bytes we can implement the following function:

```cpp
void evaluate_instruction(uint16_t instruction) {
    if (instruction & 0xf000 == 0x0000) {
        this->sys(instruction & 0x0fff);
    } else if (instruction == 0x00e0) {
        this->cls();
    }
    // ...
}
```

### compilation bugs

I originally wrote all the components without testing, so the following are all the interesting bugs I found:
- Namespaces!
  - Not exactly a bug, but I noticed all the subsystems were named similarly, so I was able to refactor them into namespaces.
- `std::array` size
  - I was treating `std::array` as a constant sized dynamically allocated memory segment, when in reality it's a fixed size memory segment! In some cases, I had to replace it with `std::vector` ... TODO: this
- improper use of default member initializers
  - For `const static` members, only integral and enum types can be initialized in the class definition. This is due to quirks of the compiler and how the data is stored. However, we can bypass this by using `constexpr` instead, which is much more flexible.
- function pointers can't be members
  - if you want to pass a function pointer to a member function to a c procedure, it's gotta be a static!
- wrong passing of a member function
  - when initializing `std::jthread`, I tried to pass the member function by pointer. This fails for two reasons. Firstly, the class name is required. Secondly, for member functions, there is no [function-to-pointer](https://en.cppreference.com/w/cpp/language/implicit_cast.html) implicit conversion. Instead, you have a member function designator which needs to explicitly have its address taken to get a function pointer.
  - HOWEVER, even this doesn't work, likely due to the `std::stop_token` parameter, so we have to resort to using a lambda `[this](std::stop_token token) { this->poll_events(token); }`.

### interpreter

Passing each instruction as a byte is sufficient, but comments are helpful! We can throw together a quick preprocessing function that removes single line comments & skips whitespace lines, while producing an array of bytes.

// TODO: this


### interface

Oh yeah, it's finally time. Now we've gotta load our program and execute each instruction.

WARNING! This next snippet of code is going to be as difficult as the concepts behind agentic coding!

```cpp
while (this->excute_instructions) {
    // TODO: wait... if instructions are two bytes, then we have to load them two bytes at a time...
    // does this mean the program_counter has to increment by 2 bytes by default? (yeah I think so...)
    this->evaluate_instruction(this->memory[this->program_counter] * 256 + this->memory[this->program_counter + 1])
}
```

Joke... funny...

## basic tests

Okay, now we need to write some tests to make sure everything works as expected. Firstly, we want some unit tests to prove each new bit of functionality works properly. 

### `draw_letters.chip8`

```sh
0x6103 // V1 = 0x03
0xf129 // I = letter_sprite[V1]
// TODO: draw the sprites out to write 314
```

### `audio_test.chip8`

I really wasn't confident with the behaviour of SDL's audio latency, and since I didn't want to go platform-specific in order to get some kind of realtime thread for buffering audio, I left it with the current implementation.

// ? In the end, I had to increase the sample rate so that the error/latency wasn't off by so much!  

```sh

```

### `full_coverage.chip8`

Now, we just need a program that manages to use every operation, but actually use it!

```sh

```

## The Final Exam

We're done! This means it's time to start running some programs real programs. Luckily for us, there have been a few game jams full of programs written in Chip8 for us play with.

// TODO: this

## We're not done yet, are we?

No way, we've still got a lot more to do! Let's start by consulting the list.

- static analysis?
- benchmarks
- profiling
- optimization
- target wasm
- design & render 3D console
- stereophonic sound (rotate the console?)

Looks good!

# ACT II

## a simple assembler language?

// TODO: what do people, do

## TODO: consider static analysis

Can we perform limited static analysis to detect any of the runtime errors we've implemented in the instructions above?

// TODO: I'd like to at least find some of the more basic bugs!!!

## benchmarks

// Let's evaluate the performance of our implementation, then compare it against some others!


## profiling & optimization time!

// TODO: estimate using our machine how long we expect our program to take, and ask why it is taking longer given the profile.
// What's the slowest part?

## compile to web

Uh, not sure how this works but we're probably gonna have to write totally separate Display, Speaker, and Keyboard classes that use Web interfaces.

# ACT III

It is now finally time for...

## the game jam of ultimate sadness

This project has taken long enough that I'm now the biggest Chip-8 connoisseur out there. Thus, it's finally time to make a game worth playing, since everything up until now has very much felt like toys. 


