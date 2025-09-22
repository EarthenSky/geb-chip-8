#ifndef EMULATOR_H
#define EMULATOR_H

#include <algorithm>
#include <random>

#include "types.h"

#include "display.h"
#include "keyboard.h"
#include "timer.h"
#include "speaker.h"

namespace Chip8 {
    class Emulator {
    private:
        Display display;
        Keyboard keyboard;

        Timer60hz sound_timer;
        Timer60hz delay_timer;

        // Speaker speaker;

        std::array<uint8_t, 4096> memory;

        uint16_t program_counter;

        // store the address the interpreter should return to after a subroutine
        std::array<uint16_t, 16> stack_frames;
        uint8_t stack_pointer = 0;

        static const size_t NUM_GP_REGISTERS = 16;
        std::array<uint8_t, NUM_GP_REGISTERS> gp_registers;

        uint16_t i_register;

        // setup prng
        std::random_device rand_dev;
        std::default_random_engine prng_engine;
        std::uniform_int_distribution<uint8_t> random_u8_dist;

        static const uint8_t SPRITE_WIDTH = 8;

        static const uint16_t BUILT_IN_CHAR_STARTING_ADDRESS = 0x100;

        Emulator() : prng_engine(rand_dev()) {
            sound_timer.set(0);
            delay_timer.set(0);

            using Sprite = std::array<uint8_t, 5>;
            std::vector<Sprite> data = {
                {0xf0, 0x90, 0x90, 0x90, 0xf0},
                {0x20, 0x60, 0x20, 0x20, 0x70},
                {0xf0, 0x10, 0xf0, 0x80, 0xf0},
                {0xf0, 0x10, 0xf0, 0x10, 0xf0},
                {0x90, 0x90, 0xf0, 0x10, 0x10},
                {0xf0, 0x80, 0xf0, 0x10, 0xf0},
                {0xf0, 0x80, 0xf0, 0x90, 0xf0},
                {0xf0, 0x10, 0x20, 0x40, 0x40},
                {0xf0, 0x90, 0xf0, 0x90, 0xf0},
                {0xf0, 0x90, 0xf0, 0x10, 0xf0},
                {0xf0, 0x90, 0xf0, 0x90, 0x90},
                {0xe0, 0x90, 0xe0, 0x90, 0xe0},
                {0xf0, 0x80, 0x80, 0x80, 0xf0},
                {0xe0, 0x90, 0x90, 0x90, 0xe0},
                {0xf0, 0x80, 0xf0, 0x80, 0xf0},
                {0xf0, 0x80, 0xf0, 0x80, 0x80}
            };
            for (size_t i = 0; i < data.size(); i++) {
                std::ranges::copy(
                    data[i],
                    memory.begin() + BUILT_IN_CHAR_STARTING_ADDRESS + i * (Sprite{}.size() * sizeof Sprite::value_type)
                );
            }
        }

        // 00e0
        void sys(uint16_t address) {
            this->program_counter += 1;
        }

        // 
        void cls() {
            std::ranges::fill(this->display.buffer, false);
            this->display.render_buffer();
            this->program_counter += 1;
        }

        // 00ee
        void ret() {
            if (this->stack_pointer == 0)
                throw std::runtime_error("cannot RET when stack is empty");

            this->program_counter = this->stack_frames[this->stack_pointer];
            this->stack_pointer -= 1;
        }

        // 1xxx
        void jp(uint16_t address) {
            this->program_counter = address;
        }

        // 2xxx
        void call(uint16_t target_address) {
            if (this->stack_pointer > 15)
                throw std::runtime_error("cannot CALL when stack is full (stack overflow!)");

            // TODO: what if program_counter is 4096-1 ? do we overflow, or raise an error?
            this->stack_frames[this->stack_pointer] = this->program_counter + 1;
            this->stack_pointer += 1;
            this->program_counter = target_address;
        }

        // 3xyy
        void skip_equal(u4 reg, uint8_t value) {
            if (this->gp_registers[reg] == value) {
                this->program_counter += 2;
            } else {
                this->program_counter += 1;
            }
        }

        // 4xyy
        void skip_not_equal(u4 reg, uint8_t value) {
            if (this->gp_registers[reg] != value) {
                this->program_counter += 2;
            } else {
                this->program_counter += 1;
            }
        }

        // 5xy0
        void skip_equal_reg(u4 reg_a, u4 reg_b) {
            if (gp_registers[reg_a] == gp_registers[reg_b]) {
                this->program_counter += 2;
            } else {
                this->program_counter += 1;
            }
        }

        // 6xyy
        void load(u4 reg, uint8_t value) {
            this->gp_registers[reg] = value;
            this->program_counter += 1;
        }

        // 7xkk
        void increment(u4 reg, uint8_t value) {
            this->gp_registers[reg] += value;
            this->program_counter += 1;
        }

        // 8xy0
        void load_reg(u4 reg_a, u4 reg_b) {
            this->gp_registers[reg_a] = this->gp_registers[reg_b];
            this->program_counter += 1;
        }

        // 8xy1
        void bitwise_or(u4 reg_a, u4 reg_b) {
            this->gp_registers[reg_a] |= this->gp_registers[reg_b];
            this->program_counter += 1;
        }

        // 8xy2
        void bitwise_and(u4 reg_a, u4 reg_b) {
            this->gp_registers[reg_a] &= this->gp_registers[reg_b];
            this->program_counter += 1;
        }

        // 8xy3
        void bitwise_xor(u4 reg_a, u4 reg_b) {
            this->gp_registers[reg_a] ^= this->gp_registers[reg_b];
            this->program_counter += 1;
        }

        // 8xy4
        void carry_increment_reg(u4 reg_a, u4 reg_b) {
            this->gp_registers[reg_a] += this->gp_registers[reg_b];
            this->gp_registers[0xf] = ((uint16_t) this->gp_registers[reg_a] + (uint16_t) this->gp_registers[reg_b]) > 0xff;
            this->program_counter += 1;
        }

        // 8xy5
        void carry_decrement_reg(u4 reg_a, u4 reg_b) {
            // NOTE: some sources say this should be > while others say >=. I'm using >= b/c it 
            // makes more sense wrt underflow.
            this->gp_registers[0xf] = this->gp_registers[reg_a] >= this->gp_registers[reg_b];
            this->gp_registers[reg_a] -= this->gp_registers[reg_b];
            this->program_counter += 1;
        }

        // 8x_6
        void shift_right(u4 reg) {
            this->gp_registers[reg] >>= 1;
            // don't worry about endianness b/c it's just 1 byte
            this->gp_registers[0xf] = 0x01 & this->gp_registers[reg];
            this->program_counter += 1;
        }

        // 8xy7
        void subtract_reversed(u4 reg_a, u4 reg_b) {
            this->gp_registers[0xf] = this->gp_registers[reg_b] >= this->gp_registers[reg_a];
            this->gp_registers[reg_a] = this->gp_registers[reg_b] - this->gp_registers[reg_a];
            this->program_counter += 1;
        }

        // 8x_e
        void shift_left(u4 reg) {
            this->gp_registers[0xf] = 0x80 & this->gp_registers[reg] != 0;
            this->gp_registers[reg] <<= 1;
            this->program_counter += 1;
        }

        // 9xy0
        void skip_not_equal_reg(u4 reg_a, u4 reg_b) {
            if (this->gp_registers[reg_a] != this->gp_registers[reg_b]) {
                this->program_counter += 2;
            } else {
                this->program_counter += 1;
            }
        }

        // axxx
        void load_address(uint16_t address) {
            this->i_register = address;
            this->program_counter += 1;
        }

        // bxxx
        void jump_reg0(uint16_t address) {
            // TODO: what to do if we get outside the range of addresses?
            this->program_counter = (uint16_t)this->gp_registers[0] + address;
        }

        // cxyy
        void random_int(u4 reg, uint8_t value) {
            this->gp_registers[reg] = this->random_u8_dist(this->prng_engine) & value;
            this->program_counter += 1;
        }

        // dxyz
        void draw(u4 reg_x, u4 reg_y, u4 value) {
            // assume no pixels are modified, unless we observe it
            this->gp_registers[0xf] = 0;

            // upper left position
            uint8_t ul_xpos = this->gp_registers[reg_x];
            uint8_t ul_ypos = this->gp_registers[reg_y];
            for (size_t row_i = 0; row_i < value; row_i++) {
                uint8_t row = this->memory[i_register + row_i];
                for (size_t bit_i = 0; bit_i < SPRITE_WIDTH; bit_i++) {
                    // sprites wrap around the display
                    auto xpos = (ul_xpos + bit_i) % Display::SCREEN_WIDTH;
                    auto ypos = (ul_ypos + row_i) % Display::SCREEN_HEIGHT;
                    
                    bool before = this->display.buffer[xpos + ypos * Display::SCREEN_WIDTH];
                    bool after = (this->display.buffer[xpos + ypos * Display::SCREEN_WIDTH] ^= (row & (0x80 >> bit_i)) != 0);
                    if (before && !after)
                        this->gp_registers[0xf] = 1;
                }
            }

            this->display.render_buffer();

            // TODO: is there a fancy way to do this & the 0xf register thingy using only a single thread (& fancy function)?
            /* std::transform(
                screen.begin() + screen_index,
                screen.begin() + screen_index + value,
                memory.begin() + i_register,
                screen.begin() + screen_index,
                [](uint8_t x, uint8_t y){ return x ^ y; }
            );*/

            this->program_counter += 1;
        }

        // ex9e
        void skip_if_key_press(u4 reg) {
            auto key = key_from_u4(
                u4(this->gp_registers[reg])
            );
            if (this->keyboard.is_key_pressed(key)) {
                this->program_counter += 2;
            } else {
                this->program_counter += 1;
            }
        }

        // exa1
        void skip_if_not_key_press(u4 reg) {
            auto key = key_from_u4(
                u4(this->gp_registers[reg])
            );
            if (!this->keyboard.is_key_pressed(key)) {
                this->program_counter += 2;
            } else {
                this->program_counter += 1;
            }
        }

        // fx07
        void load_from_delay_timer(u4 reg) {
            this->gp_registers[reg] = this->delay_timer.value();
            this->program_counter += 1;
        }

        // fx0a
        void load_from_next_keypress(u4 reg) {
            auto key = this->keyboard.block_until_next_keypress();
            this->gp_registers[reg] = key;
            this->program_counter += 1;
        }

        // fx15
        void set_delay(u4 reg) {
            this->delay_timer.set(this->gp_registers[reg]);
            this->program_counter += 1;
        }

        // fx18
        void set_sound(u4 reg) {
            this->sound_timer.set(this->gp_registers[reg]);
            this->program_counter += 1;
        }

        // fx1e
        void increment_i_reg(u4 reg) {
            this->i_register += this->gp_registers[reg];
            this->program_counter += 1;
        }

        // fx29
        void load_sprite(u4 reg) {
            this->i_register = BUILT_IN_CHAR_STARTING_ADDRESS + 5 * (this->gp_registers[reg] % 16);
            this->program_counter += 1;
        }

### `LDB Vx` [fx33]

**This command LoaDs the Binary coded decimal (BCD) representation of the value in register x into memory locations I, I+1, I+2.**

```cpp
void load_bcd(uint8_t reg) {
    if (reg > 15)
        throw std::runtime_error("invalid register number");

    // TODO: should I check whether the memory location is valid?
    memory[i_register] = this->gp_registers[reg] % 10;
    memory[i_register+1] = (this->gp_registers[reg] % 100 - this->gp_registers[reg] % 10) / 10;
    memory[i_register+2] = (this->gp_registers[reg] - this->gp_registers[reg] % 100) / 100;
    this->program_counter += 1;
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
        memory[i_register+i] = this->gp_registers[i];
    }
    this->program_counter += 1;
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
        this->gp_registers[i] = memory[i_register+i];
    }
    this->program_counter += 1;
}
```

    };
}

#endif