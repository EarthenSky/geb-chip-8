#ifndef DISPLAY_H
#define DISPLAY_H

#include <algorithm>
#include <array>
#include <format>
#include <stdexcept>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>

#include "timer.h"

namespace Chip8 {
    constexpr static uint16_t SCREEN_WIDTH  = 64;
    constexpr static uint16_t SCREEN_HEIGHT = 32;

    namespace SDL3 {
        class Display {
        private:
            SDL_Window* window = nullptr;
            SDL_Renderer* renderer = nullptr;

            // the edge size of a pixel rendered on the native display
            const float scale_factor = 4.0;

        public:
            Display() {
                // zero init the display buffer!
                std::ranges::fill(this->buffer, 0);

                if (!SDL_CreateWindowAndRenderer(
                    "Chip8 Display",
                    (int)(SCREEN_WIDTH  * this->scale_factor),
                    (int)(SCREEN_HEIGHT * this->scale_factor),
                    0, &this->window, &this->renderer
                ))
                    throw std::runtime_error(std::format("SDL_CreateWindowAndRenderer error: {}\n", SDL_GetError()));

                SDL_SetRenderVSync(this->renderer, SDL_RENDERER_VSYNC_ADAPTIVE);
            }

            void render_buffer() {
                // TODO: later, consider a more efficient way to send this data to the gpu & render it
                for (uint16_t y = 0; y < SCREEN_HEIGHT; y++) {
                    for (uint16_t x = 0; x < SCREEN_WIDTH; x++) {
                        bool px_active = buffer[x + y * SCREEN_WIDTH];
                        if (px_active)
                            SDL_SetRenderDrawColor(this->renderer, 255, 255, 255, 255);
                        else
                            SDL_SetRenderDrawColor(this->renderer, 25, 25, 25, 255);

                        const SDL_FRect pixel_bounds = {
                            x * this->scale_factor,
                            y * this->scale_factor,
                            this->scale_factor, this->scale_factor
                        };
                        SDL_RenderFillRect(this->renderer, &pixel_bounds);
                    }
                }

                SDL_RenderPresent(renderer);
            }

            std::array<bool, SCREEN_WIDTH * SCREEN_HEIGHT> buffer;

            ~Display() {
                SDL_DestroyRenderer(this->renderer);
                SDL_DestroyWindow(this->window);
            }
        };
   
        class Speaker {
        private:
            constexpr static SDL_AudioSpec OUTPUT_SPEC = {
                SDL_AUDIO_U8,
                1,
                // TODO: increase sample rate so our sfx sounds more like a square wave!
                400 * 64, // 48000
            };

            SDL_AudioDeviceID device_id = 0;
            SDL_AudioStream* out_stream = nullptr;

            Timer60hz& sound_timer;

            // SDLCALL
            static void out_stream_callback(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount) {
                if (additional_amount <= 0)
                    return;

                Speaker* self = (Speaker*) userdata;
                uint8_t value = self->sound_timer.value();

                std::vector<uint8_t> samples(additional_amount);
                std::ranges::fill(samples, 0xff / 2);

                // 800hz sample rate, 60hz timer
                // each timer value represents the same time-period as 6.66666... samples
                size_t limit = std::min((size_t) (value * (OUTPUT_SPEC.freq / 60.0)), samples.size());
                for (size_t i = 0; i < limit; i++) {
                    // Since our sample rate is 400hz, we should expect to get a 200hz wave
                    // that is somewhat sine/square shaped.
                    // The actual shape depends on SDL3's built-in resampler.
                    if (i % (64*2) < (32*2)) {
                        samples[i] = 0x00;
                    } else {
                        samples[i] = 0xff;
                    }
                }

                // TODO: next: is this still required?
                // TODO: remember the last queued sample to decide on phase of the wave (for now, we'll just get lil blips and
                // that's okay)

                if(!SDL_PutAudioStreamData(stream, samples.data(), additional_amount))
                    throw std::runtime_error(std::format("SDL_PutAudioStreamData failed with: {}", SDL_GetError()));
            }

        public:
            Speaker(Timer60hz& sound_timer) : sound_timer(sound_timer) {
                this->device_id = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
                if (this->device_id == 0)
                    throw std::runtime_error(std::format("SDL_OpenAudioDevice failed with: {}", SDL_GetError()));

                std::cout << "Opened Audio Device: " << SDL_GetAudioDeviceName(this->device_id) << std::endl;

                this->out_stream = SDL_CreateAudioStream(&Speaker::OUTPUT_SPEC, nullptr);
                if (this->out_stream == nullptr)
                    throw std::runtime_error(std::format("SDL_CreateAudioStream failed with: {}", SDL_GetError()));

                if (!SDL_BindAudioStream(this->device_id, this->out_stream))
                    throw std::runtime_error(std::format("SDL_BindAudioStream failed with: {}", SDL_GetError()));

                // TODO: how is this implemented internally? With sleep?
                SDL_SetAudioStreamGetCallback(this->out_stream, out_stream_callback, (void*)this);
            }
            ~Speaker() {
                SDL_DestroyAudioStream(this->out_stream);
                SDL_CloseAudioDevice(this->device_id);
            }
        };

        /// @brief performs init and cleanup of the device
        class Lifetime {
        public:
            Lifetime() {
                if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
                    throw std::runtime_error(std::format("SDL_Init error: {}\n", SDL_GetError()));
            }
            ~Lifetime() {
                SDL_Quit();
            }
        };
    }

    // TODO: eventually move this to its own file & turn it into an interface when we care more about
    // targeting web as well as desktop 
    class Device {
    private:
        Chip8::SDL3::Lifetime lifetime;

    public:
        Chip8::SDL3::Speaker speaker;
        Chip8::SDL3::Display display;

        Device(Timer60hz& sound_timer) : speaker(sound_timer) {}

        // TODO: maybe move the keyboard here too
    };

}

#endif