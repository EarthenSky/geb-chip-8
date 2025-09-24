#ifndef EMULATOR_H
#define EMULATOR_H

#include <algorithm>
#include <random>

#include "types.h"
#include "geblib.h"

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
        std::uniform_int_distribution<unsigned int> random_u8_dist;

        static const uint8_t SPRITE_WIDTH = 8;

        static const uint16_t BUILT_IN_CHAR_STARTING_ADDRESS = 0x100;

        static const size_t INSTRUCTION_SIZE = 2;

        // 0xxx
        void sys(uint16_t address) {
            this->program_counter += INSTRUCTION_SIZE;
        }

        // 00e0
        void cls() {
            std::ranges::fill(this->display.buffer, false);
            this->display.render_buffer();
            this->program_counter += INSTRUCTION_SIZE;
        }

        // 00ee
        void ret() {
            if (this->stack_pointer == 0)
                throw std::runtime_error("cannot RET when stack is empty");

            this->program_counter = this->stack_frames[this->stack_pointer];
            this->stack_pointer -= 1;
        }

        // 1xxx
        // TODO: do I want a u12 type?
        void jp(uint16_t address) {
            this->program_counter = address;
        }

        // 2xxx
        void call(uint16_t target_address) {
            if (this->stack_pointer > 15)
                throw std::runtime_error("cannot CALL when stack is full (stack overflow!)");
            else if (target_address < this->memory.size() - 1)
                // TODO: what if program_counter is 4096-1 ? do we overflow, or raise an error?
                // don't overflow; instead raise an error
                // TODO: static analysis
                throw std::runtime_error("call address outside of working memory area");
                
            this->stack_frames[this->stack_pointer] = this->program_counter + INSTRUCTION_SIZE;
            this->stack_pointer += 1;
            this->program_counter = target_address;
        }

        // 3xyy
        void skip_equal(u4 reg, uint8_t value) {
            if (this->gp_registers[reg] == value) {
                this->program_counter += 2 * INSTRUCTION_SIZE;
            } else {
                this->program_counter += INSTRUCTION_SIZE;
            }
        }

        // 4xyy
        void skip_not_equal(u4 reg, uint8_t value) {
            if (this->gp_registers[reg] != value) {
                this->program_counter += 2 * INSTRUCTION_SIZE;
            } else {
                this->program_counter += INSTRUCTION_SIZE;
            }
        }

        // 5xy0
        void skip_equal_reg(u4 reg_a, u4 reg_b) {
            if (gp_registers[reg_a] == gp_registers[reg_b]) {
                this->program_counter += 2 * INSTRUCTION_SIZE;
            } else {
                this->program_counter += INSTRUCTION_SIZE;
            }
        }

        // 6xyy
        void load(u4 reg, uint8_t y) {
            this->gp_registers[reg] = y;
            this->program_counter += INSTRUCTION_SIZE;
        }

        // 7xyy
        void add(u4 reg, uint8_t y) {
            this->gp_registers[reg] += y;
            this->program_counter += INSTRUCTION_SIZE;
        }

        // 8xy0
        void load_reg(u4 reg_a, u4 reg_b) {
            this->gp_registers[reg_a] = this->gp_registers[reg_b];
            this->program_counter += INSTRUCTION_SIZE;
        }

        // 8xy1
        void bitwise_or(u4 reg_a, u4 reg_b) {
            this->gp_registers[reg_a] |= this->gp_registers[reg_b];
            this->program_counter += INSTRUCTION_SIZE;
        }

        // 8xy2
        void bitwise_and(u4 reg_a, u4 reg_b) {
            this->gp_registers[reg_a] &= this->gp_registers[reg_b];
            this->program_counter += INSTRUCTION_SIZE;
        }

        // 8xy3
        void bitwise_xor(u4 reg_a, u4 reg_b) {
            this->gp_registers[reg_a] ^= this->gp_registers[reg_b];
            this->program_counter += INSTRUCTION_SIZE;
        }

        // 8xy4
        void carry_add_reg(u4 reg_a, u4 reg_b) {
            this->gp_registers[reg_a] += this->gp_registers[reg_b];
            this->gp_registers[0xf] = ((uint16_t) this->gp_registers[reg_a] + (uint16_t) this->gp_registers[reg_b]) > 0xff;
            this->program_counter += INSTRUCTION_SIZE;
        }

        // 8xy5
        void carry_sub_reg(u4 reg_a, u4 reg_b) {
            // NOTE: some sources say this should be > while others say >=. I'm using >= b/c it 
            // makes more sense wrt underflow.
            this->gp_registers[0xf] = this->gp_registers[reg_a] >= this->gp_registers[reg_b];
            this->gp_registers[reg_a] -= this->gp_registers[reg_b];
            this->program_counter += INSTRUCTION_SIZE;
        }

        // 8x_6
        void shift_right(u4 reg) {
            this->gp_registers[reg] >>= 1;
            // don't worry about endianness b/c it's just 1 byte
            this->gp_registers[0xf] = 0x01 & this->gp_registers[reg];
            this->program_counter += INSTRUCTION_SIZE;
        }

        // 8xy7
        void subtract_reversed(u4 reg_a, u4 reg_b) {
            this->gp_registers[0xf] = this->gp_registers[reg_b] >= this->gp_registers[reg_a];
            this->gp_registers[reg_a] = this->gp_registers[reg_b] - this->gp_registers[reg_a];
            this->program_counter += INSTRUCTION_SIZE;
        }

        // 8x_e
        void shift_left(u4 reg) {
            this->gp_registers[0xf] = (0x80 & this->gp_registers[reg]) != 0;
            this->gp_registers[reg] <<= 1;
            this->program_counter += INSTRUCTION_SIZE;
        }

        // 9xy0
        void skip_not_equal_reg(u4 reg_a, u4 reg_b) {
            if (this->gp_registers[reg_a] != this->gp_registers[reg_b]) {
                this->program_counter += 2 * INSTRUCTION_SIZE;
            } else {
                this->program_counter += INSTRUCTION_SIZE;
            }
        }

        // axxx
        void load_address(uint16_t address) {
            this->i_register = address;
            this->program_counter += INSTRUCTION_SIZE;
        }

        // bxxx
        void jump_reg0(uint16_t address_offset) {
            // TODO: what to do if we get outside the range of addresses?
            // Q: when we increment the program_counter by 1, does it increase by 2 bytes?
            // A: pretty sure no
            this->program_counter = (uint16_t)this->gp_registers[0] + address_offset;
        }

        // cxyy
        void random_int(u4 reg, uint8_t y) {
            this->gp_registers[reg] = this->random_u8_dist(this->prng_engine) & y;
            this->program_counter += INSTRUCTION_SIZE;
        }

        // dxyz
        void draw_sprite(u4 reg_x, u4 reg_y, u4 value) {
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

            // TODO: is there a fancy way to do this & the 0xf register thingy using only a single thread (& std::transform or similar)?

            this->display.render_buffer();
            this->program_counter += INSTRUCTION_SIZE;
        }

        // ex9e
        void skip_if_key_press(u4 reg) {
            auto key = static_cast<Key>(this->gp_registers[reg] % 16);
            if (this->keyboard.is_key_pressed(key)) {
                this->program_counter += 2 * INSTRUCTION_SIZE;
            } else {
                this->program_counter += INSTRUCTION_SIZE;
            }
        }

        // exa1
        void skip_if_not_key_press(u4 reg) {
            auto key = static_cast<Key>(this->gp_registers[reg] % 16);
            if (!this->keyboard.is_key_pressed(key)) {
                this->program_counter += 2 * INSTRUCTION_SIZE;
            } else {
                this->program_counter += INSTRUCTION_SIZE;
            }
        }

        // fx07
        void load_from_delay_timer(u4 reg) {
            this->gp_registers[reg] = this->delay_timer.value();
            this->program_counter += INSTRUCTION_SIZE;
        }

        // fx0a
        void load_from_next_keypress(u4 reg) {
            auto key = this->keyboard.block_until_next_keypress();
            this->gp_registers[reg] = key;
            this->program_counter += INSTRUCTION_SIZE;
        }

        // fx15
        void set_delay(u4 reg) {
            this->delay_timer.set(this->gp_registers[reg]);
            this->program_counter += INSTRUCTION_SIZE;
        }

        // fx18
        void set_sound(u4 reg) {
            this->sound_timer.set(this->gp_registers[reg]);
            this->program_counter += INSTRUCTION_SIZE;
        }

        // fx1e
        void increment_i_reg(u4 reg) {
            this->i_register += this->gp_registers[reg];
            this->program_counter += INSTRUCTION_SIZE;
        }

        // fx29
        void load_sprite(u4 reg) {
            this->i_register = BUILT_IN_CHAR_STARTING_ADDRESS + 5 * (this->gp_registers[reg] % 16);
            this->program_counter += INSTRUCTION_SIZE;
        }

        // fx33
        void load_bcd(u4 reg) {
            if (i_register < this->memory.size() - 2)
                throw std::runtime_error("address outside of working memory area");

            // i_register should behave like a normal u16, except that it fails when trying to write or read to invalid memory locations
            // TODO: ensure these checks happen everywhere
            this->memory[i_register] = this->gp_registers[reg] % 10;
            this->memory[i_register+1] = (this->gp_registers[reg] % 100 - this->gp_registers[reg] % 10) / 10;
            this->memory[i_register+2] = (this->gp_registers[reg] - this->gp_registers[reg] % 100) / 100;

            this->program_counter += INSTRUCTION_SIZE;
        }

        // fx55
        void load_reg_to_mem(u4 reg_final) {
            // TODO: should I check whether the memory locations are all valid? yes
            // TODO: how to get ++ or += 1 to work w/ u4?
            for (u4 i = 0; i <= reg_final; i++) {
                this->memory[i_register+i] = this->gp_registers[i];
            }
            this->program_counter += INSTRUCTION_SIZE;
        }

        // fx65
        void load_mem_to_reg(u4 reg_final) {
            if (reg_final > 15)
                throw std::runtime_error("invalid register number");

            // TODO: should I check whether the memory locations are all valid? yes
            for (u4 i = 0; i <= reg_final; i++) {
                this->gp_registers[i] = this->memory[i_register+i];
            }
            this->program_counter += INSTRUCTION_SIZE;
        }

    private:
        void evaluate_instruction(uint16_t instruction) {
            using GebLib::get_nibble;

            if (instruction & 0xf000 == 0x0000) {
                this->sys(instruction & 0x0fff);
            } else if (instruction == 0x00e0) {
                this->cls();
            } else if (instruction == 0x00ee) {
                this->ret();
            } else if (instruction & 0xf000 == 0x1000) {
                this->jp(instruction & 0x0fff);
            } else if (instruction & 0xf000 == 0x2000) {
                this->call(instruction & 0x0fff);
            } else if (instruction & 0xf000 == 0x3000) {
                u4 x = get_nibble(instruction, 1);
                uint8_t y = instruction & 0x00ff;
                this->skip_equal(x, y);
            } else if (instruction & 0xf000 == 0x4000) {
                u4 x = get_nibble(instruction, 1);
                uint8_t y = instruction & 0x00ff;
                this->skip_not_equal(x, y);
            } else if (instruction & 0xf00f == 0x5000) {
                u4 x = get_nibble(instruction, 1);
                u4 y = get_nibble(instruction, 2);
                this->skip_equal_reg(x, y);
            } else if (instruction & 0xf000 == 0x6000) {
                u4 x = get_nibble(instruction, 1);
                uint8_t y = instruction & 0x00ff;
                this->load(x, y);
            } else if (instruction & 0xf000 == 0x7000) {
                u4 x = get_nibble(instruction, 1);
                uint8_t y = instruction & 0x00ff;
                this->add(x, y);
            } else if (instruction & 0xf00f == 0x8000) {
                u4 x = get_nibble(instruction, 1);
                u4 y = get_nibble(instruction, 2);
                this->load_reg(x, y);
            } else if (instruction & 0xf00f == 0x8001) {
                u4 x = get_nibble(instruction, 1);
                u4 y = get_nibble(instruction, 2);
                this->bitwise_or(x, y);
            } else if (instruction & 0xf00f == 0x8002) {
                u4 x = get_nibble(instruction, 1);
                u4 y = get_nibble(instruction, 2);
                this->bitwise_and(x, y);
            } else if (instruction & 0xf00f == 0x8003) {
                u4 x = get_nibble(instruction, 1);
                u4 y = get_nibble(instruction, 2);
                this->bitwise_xor(x, y);
            } else if (instruction & 0xf00f == 0x8004) {
                u4 x = get_nibble(instruction, 1);
                u4 y = get_nibble(instruction, 2);
                this->carry_add_reg(x, y);
            } else if (instruction & 0xf00f == 0x8005) {
                u4 x = get_nibble(instruction, 1);
                u4 y = get_nibble(instruction, 2);
                this->carry_sub_reg(x, y);
            } else if (instruction & 0xf00f == 0x8006) {
                u4 x = get_nibble(instruction, 1);
                this->shift_right(x);
            } else if (instruction & 0xf00f == 0x8007) {
                u4 x = get_nibble(instruction, 1);
                u4 y = get_nibble(instruction, 2);
                this->subtract_reversed(x, y);
            } else if (instruction & 0xf00f == 0x800e) {
                u4 x = get_nibble(instruction, 1);
                this->shift_left(x);
            } else if (instruction & 0xf00f == 0x9000) {
                u4 x = get_nibble(instruction, 1);
                u4 y = get_nibble(instruction, 2);
                this->skip_not_equal_reg(x, y);
            } else if (instruction & 0xf000 == 0xa000) {
                this->load_address(instruction & 0x0fff);
            } else if (instruction & 0xf000 == 0xb000) {
                this->jump_reg0(instruction & 0x0fff);
            } else if (instruction & 0xf000 == 0xc000) {
                u4 x = get_nibble(instruction, 1);
                uint8_t y = instruction & 0x00ff;
                this->random_int(x, y);
            } else if (instruction & 0xf000 == 0xd000) {
                u4 x = get_nibble(instruction, 1);
                u4 y = get_nibble(instruction, 2);
                u4 z = get_nibble(instruction, 3);
                this->draw_sprite(x, y, z);
            } else if (instruction & 0xf0ff == 0xe09e) {
                u4 x = get_nibble(instruction, 1);
                this->skip_if_key_press(x);
            } else if (instruction & 0xf0ff == 0xf007) {
                u4 x = get_nibble(instruction, 1);
                this->load_from_delay_timer(x);
            } else if (instruction & 0xf0ff == 0xf00a) {
                u4 x = get_nibble(instruction, 1);
                this->load_from_next_keypress(x);
            } else if (instruction & 0xf0ff == 0xf015) {
                u4 x = get_nibble(instruction, 1);
                this->set_delay(x);
            } else if (instruction & 0xf0ff == 0xf018) {
                u4 x = get_nibble(instruction, 1);
                this->set_sound(x);
            } else if (instruction & 0xf0ff == 0xf01e) {
                u4 x = get_nibble(instruction, 1);
                this->increment_i_reg(x);
            } else if (instruction & 0xf0ff == 0xf029) {
                u4 x = get_nibble(instruction, 1);
                this->load_sprite(x);
            } else if (instruction & 0xf0ff == 0xf033) {
                u4 x = get_nibble(instruction, 1);
                this->load_bcd(x);
            } else if (instruction & 0xf0ff == 0xf055) {
                u4 x = get_nibble(instruction, 1);
                this->load_reg_to_mem(x);
            } else if (instruction & 0xf0ff == 0xf065) {
                u4 x = get_nibble(instruction, 1);
                this->load_mem_to_reg(x);
            } else {
                throw std::runtime_error(std::format("Hit unknown instruction: {:x}", instruction));
            }
        }

    public:
        Emulator() : prng_engine(rand_dev()), random_u8_dist(0, 255) {
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
    };
}

#endif