# Chip-8 Devlog

Chip-8 is a programming language / virtual machine for writing small games, originally developed in the mid-1970s. This is a short devlog for making and optimizing a chip-8 interpreter.

## first steps

http://devernay.free.fr/hacks/chip8/C8TECH10.HTM is a lovely specification that details the machine and the instructions. It's important to note that there is no "official" specification for Chip-8, so you have to be sure the program you're running agrees with this specification. https://en.wikipedia.org/wiki/CHIP-8 is a good second source.

We'll be writing the interpreter in C++20, using SDL3 to render the screen.

## memory

In Chip-8, the interpreter, program, and general memory all share the same address space. There are 4096 bytes of addressable memory, the first 512 of which were reserved for the interpreter. As an added bonus, the whole thing fits in the L1 cache of decades old processors!

```cpp
std::array<uint8_t, 4096> memory;
```

When we want to run a Chip-8 program, we'll write the instructions byte by byte into memory. YES YOU'RE RIGHT, THIS IS DANGEROUS! Since we're sharing program and general memory, programs can modify their own code. In reality since Chip-8 programs are so small, it's not a massive problem (whew). Although, it can be a little bonus!

## keyboard

// TODO: write this when I have more context

## display

// TODO: ditto

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

Computers for which Chip-8 was originally developed had a 16-key hexadecimal keypad. **This command Skips the next instruction if a Key with the value in register x is Pressed.**

// TODO: use SDL3 to poll for key presses

```cpp
// std::array<bool, 16> keys_pressed;

void skip_if_key_press(uint8_t reg) {
    if (reg > 15)
        throw std::runtime_error("invalid register number");

    if (keys_pressed[gp_registers[reg] % 16]) {
        program_counter += 2;
    } else {
        program_counter += 1;
    }
}
```

### `SKNP Vx` [exa1]

**This command Skip the next instruction if a Key with the value in register x is Not Pressed.**

// TODO: use SDL3 to poll for key presses

```cpp
void skip_if_not_key_press(uint8_t reg) {
    if (reg > 15)
        throw std::runtime_error("invalid register number");

    if (!keys_pressed[gp_registers[reg] % 16]) {
        program_counter += 2;
    } else {
        program_counter += 1;
    }
}
```

### `LD Vx DT` [Fx07]

Chip-8 has two 8-bit timer registers: the delay and sound timers. At a rate of 60hz, the value in the delay and sound timers will decrease by 1, if they are not already 0. **This command LoaDs the value from the delay timer into register x.**

```cpp
uint8_t delay_register = 0;
uint8_t sound_register = 0;

void load_from_delay_reg(uint8_t reg) {
    if (reg > 15)
        throw std::runtime_error("invalid register number");

    gp_registers[reg] = delay_register;
    program_counter += 1;
}
```

### `LD Vx NextKey` [fx0a]

**This command waits (blocks), then LoaDs the value of the next key press into register x.**

```cpp
// TODO: create this keyboard class!
Geb::SDL3_Keyboard keyboard;

void load_from_next_keypress(uint8_t reg) {
    if (reg > 15)
        throw std::runtime_error("invalid register number");

    // TODO: filter out invalid keys
    uint8_t key = keyboard.block_until_keypress(true);
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

    delay_register = gp_registers[reg];
    program_counter += 1;
}
```

### `LD ST Vx` [fx18]

**This command LoaDs the sound timer with the value of register x.**

```cpp
void set_sound(uint8_t reg) {
    if (reg > 15)
        throw std::runtime_error("invalid register number");

    sound_register = gp_registers[reg];
    program_counter += 1;
}
```

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
3. Play our sfx when the sound timer is non-zero

### (1) render display

The only blocking instruction is `fx0a`, so we can update the display whenever it's modified. Since the timers run at 60hz, we only need to be careful that no instruction takes a significant portion of that time, otherwise graphics will render weirdly. If we limit each instruction to take no longer than 1000ms/60hz, that gives roughly 16ms per operation. An easy target!

In the following snippet we setup SDL3, then create a simple a class with an interface allowing the display buffer to be modified and rendered.

```cpp
#include <SDL3/SDL.h>
// redefines main for portability reasons
#include <SDL3/SDL_main.h>

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

The above class addresses the only two rendering instructions: `00e0` and `dxyz`.

### (2) get keyboard input

// TODO: ensure the following works alright! 

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

class Chip8Keyboard {
private:
    // never need to update more than 1 key at once
    // true means down
    std::array<std::atomic<bool>, 16> keyboard_state;
    
    // TODO: is there a good way to encapsulate this?
    std::atomic<bool> keyboard_thread_is_active = true;
    std::jthread poll_events_thread(poll_events);

    // TODO: can we encapsulate this in an easier structure?
    std::atomic<bool> write_next_keyboard_event = false;
    std::optional<Key> next_key;
    std::mutex processing_key_down;
    std::condition_variable wait_for_next_key;

    void poll_events() {
        SDL_Event event;
        while (true) {
            while (SDL_PollEvent(&event)) {
                if (!keyboard_thread_is_active)
                    return;

                if (event->type == SDL_EVENT_QUIT) {
                    // TODO: is std::cout thread safe? 
                    std::cout << "Exiting..." << std::endl;
                    exit(1);
                } else if (event->type == SDL_EVENT_KEY_DOWN) {
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

                    // TODO: skip all non-keyboard events.
                    std::lock_guard lock(processing_key_down);
                    if (write_next_keyboard_event) {
                        next_key = std::optional(static_cast<Key>(key_i));
                        wait_for_next_key.notify_one();
                    }

                    this->keyboard_state[key_i] = true;

                } else if (event->type == SDL_EVENT_KEY_UP) {
                    auto key = event->key.key;
                    if (key >= SDLK_0 && key <= SDLK_9) {
                        this->keyboard_state[0x0 + (key - SDLK_0)] = false;
                    } else if (key >= SDLK_a && key <= SDLK_f) {
                        this->keyboard_state[0xa + (key - SDLK_a)] = false;
                    }
                }

            }

            // In the worst case, sleep may wait up to 15ms, which is still 60hz, so we should be fine!
            // In the best case, we get 1000/(0.5) = 2000hz, which is super
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
        
    }

public:
    Chip8Keyboard() {}

    ~Chip8Keyboard() {
        keyboard_thread_is_active = false;
    }

    bool is_key_pressed(Key key) const {
       // Q: should we wait until this instruction to poll events, or do we do so in the background? 
       // I would prefer to do it only during this thread; but it depends on the size of the queue!

       // actually, no matter the size of the queue, we want to immediately poll and accept key
       // presses until we block.
    }

    Key block_until_next_key() {
        // this lock blocks the next keybaord event from starting, or waits until it is done.
        std::unique_lock lock(processing_key_down);
        should_write_next_keyboard_event = true;
        wait_for_next_key.wait(lock, [&next_key]{ return next_key.has_value(); });

        // hold the lock until the end of this very short function in case another keyboard is polled in between & we
        // miss a key! 
        Key result = this->next_key.value();
        this->next_key = std::nullopt;
        return result;
    }
};


```

## put it all together!

Now that we have all our instruction snippets, lets put them all in a class, and provide an interface that accepts a program.

...

We'll also want to figure out what structure

## basic tests

## display

## sounds

## TODO: consider static analysis

Can we perform limited static analysis to detect any of the runtime errors we've implemented in the instructions above?

## benchmarks



## profiling


## optimization time!

TODO: estimate using our machine how long we expect our program to take, and ask why it is taking longer given the profile.

## compile to web

## next time: game jam of ultimate sadness
