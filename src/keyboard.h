#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <atomic>
#include <thread>
#include <stop_token>
#include <optional>

#include <SDL3/SDL.h>

#include "types.h"
#include "geblib.h"

namespace Chip8 {
    enum Key {
        K0 = 0,
        K1, K2, K3,
        K4, K5, K6,
        K7, K8, K9,
        KA, KB, KC,
        KD, KE, KF
    };

    Key key_from_u4(u4 x) {
        return static_cast<Key>((size_t)x);
    }

    class Keyboard {
    private:
        // never need to update more than 1 key at once
        // true means down
        std::array<std::atomic<bool>, 16> keyboard_state;
        
        std::jthread poll_events_thread;

        GebLib::Threading::ChannelCoordinator<Key> key_channel;

        void poll_events(std::stop_token stop_token) {
            while (true) {
                SDL_Event event;
                while (SDL_PollEvent(&event)) {
                    if (stop_token.stop_requested())
                        return;

                    if (event.type == SDL_EVENT_QUIT) {
                        std::cout << "Exiting...\n";
                        exit(1);
                    } else if (
                        event.type == SDL_EVENT_KEY_DOWN
                        || event.type == SDL_EVENT_KEY_UP
                    ) {
                        // https://wiki.libsdl.org/SDL3/SDL_Keycode
                        size_t key_i;
                        auto key = event.key.key;
                        if (key >= SDLK_0 && key <= SDLK_9) {
                            key_i = 0x0 + (key - SDLK_0);
                        } else if (key >= SDLK_A && key <= SDLK_F) {
                            key_i = 0xa + (key - SDLK_A);
                        } else {
                            // invalid keydown doesn't lock mutex
                            continue;
                        }

                        if (event.type == SDL_EVENT_KEY_DOWN)
                            key_channel.send_if_requested(static_cast<Key>(key_i));

                        this->keyboard_state[key_i] = (event.type == SDL_EVENT_KEY_DOWN);
                    }
                }

                // In the worst case, sleep may wait up to 15ms, which is still 60hz, so we should be fine!
                // In the best case, we get 1000/(0.5) = 2000hz, which is super
                std::this_thread::sleep_for(std::chrono::microseconds(500));
            }
        }

    public:
        Keyboard() : poll_events_thread([this](std::stop_token token) { this->poll_events(token); }) {}
        ~Keyboard() = default;

        bool is_key_pressed(Key key) const {
            return keyboard_state[static_cast<size_t>(key)];
        }

        Key block_until_next_keypress() {
            // blocks the next keyboard event from starting, or waits until it is done
            return this->key_channel.request();
        }
    };

}

#endif