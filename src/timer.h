#ifndef TIMER_H
#define TIMER_H

#include <chrono>

namespace Chip8 {
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
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                current - this->timestamp
            ).count();

            // separate this into 1s amounts (no error) and smaller amounts
            // (there are a few steady_clock tick errors b/c 16667 != 16666.66666...)
            size_t ticks_elapsed = 60 * (duration / (1000*1000)) + ((duration % (1000*1000)) / 16667);
            if (ticks_elapsed >= (size_t)this->_value) {
                return 0;
            } else {
                return this->_value - (uint8_t)ticks_elapsed;
            }
        }
        void set(uint8_t new_value) {
            this->_value = new_value;
            this->timestamp = std::chrono::steady_clock::now();
        }
    };
}

#endif