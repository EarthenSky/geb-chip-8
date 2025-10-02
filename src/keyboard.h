#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <atomic>
#include <stop_token>

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

    class Keyboard {
    private:
        // never need to update more than 1 key at once
        // true means down
        std::array<std::atomic<bool>, 16> keyboard_state;
        
        GebLib::Threading::ChannelCoordinator<Key> key_channel;

    public:
        /// @returns whether the event loop is probably empty. The suggestion guarantees when 
        /// false that all events in the queue cannot be older than a few operations or a single (probably) context
        /// switch. Thus, if false, it's safe to sleep a little.
        bool poll_events(size_t max_events=64) {
            SDL_Event event;
            size_t i;
            for (i = 0; i < max_events && SDL_PollEvent(&event); i++) {
                if (event.type == SDL_EVENT_QUIT) {
                    std::cout << "Got SDL exit event. Exiting...\n";
                    exit(1);
                } else if (
                    (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP)
                    && !event.key.repeat
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

                    std::cout << "GOT INPUT. key = " << key_i << " key_down = " << (event.type == SDL_EVENT_KEY_DOWN) << std::endl; 
                    this->keyboard_state[key_i] = (event.type == SDL_EVENT_KEY_DOWN);
                }
            }

            return i < max_events;
        }

        void poll_until_any_keypress() {
            SDL_Event event;
            while (true) {
                while (SDL_PollEvent(&event)) {
                    if (event.type == SDL_EVENT_QUIT) {
                        std::cout << "Got SDL exit event. Exiting...\n";
                        exit(1);
                    } else if (event.type == SDL_EVENT_KEY_DOWN) {
                        return;
                    }
                }

                std::this_thread::sleep_for(std::chrono::microseconds(500));
            }
        }

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