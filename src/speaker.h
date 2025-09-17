#ifndef SPEAKER_H
#define SPEAKER_H

#include <algorithm>
#include <array>
#include <vector>
#include <stdexcept>

#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>
//#include <SDL3/SDL_begin_code.h>

#include "timer.h"

namespace Chip8 {
    class Speaker {
    private:
        constexpr static SDL_AudioSpec OUTPUT_SPEC = {
            SDL_AUDIO_U8,
            1,
            // TODO: increase sample rate so our sfx sounds more like a square wave!
            400
        };

        SDL_AudioDeviceID device_id = 0;
        SDL_AudioStream* out_stream = nullptr;

        Timer60hz& sound_timer;

        // SDLCALL
        static void out_stream_callback(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount) {
            if (additional_amount == 0)
                return;

            Speaker* self = (Speaker*) userdata;
            uint8_t value = self->sound_timer.value();

            std::vector<uint8_t> samples(additional_amount);
            std::ranges::fill(samples, 0);

            // 400hz sample rate, 60hz timer
            // each timer value represents the same time-period as 6.66666... samples
            size_t limit = std::min((size_t) (value * (400.0 / 60.0)), samples.size());
            for (size_t i = 0; i < limit; i++) {
                // Since our sample rate is 400hz, we should expect to get a 200hz wave that is somewhat sine/square shaped.
                // The actual shape depends on SDL3's built-in resampler.
                if (i % 2 == 0)
                    samples[i] = 0x40;
            }

            // TODO: remember the last queued sample to decide on phase of the wave (for now, we'll just get lil blips and
            // that's okay)

            SDL_PutAudioStreamData(stream, samples.data(), samples.size() * sizeof(uint8_t));
        }

    public:
        Speaker(Timer60hz& sound_timer) : sound_timer(sound_timer) {
            this->device_id = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
            if (this->device_id == 0)
                throw std::runtime_error(std::format("SDL_OpenAudioDevice failed with: {}", SDL_GetError()));

            this->out_stream = SDL_CreateAudioStream(nullptr, &Speaker::OUTPUT_SPEC);
            if (this->out_stream == nullptr)
                throw std::runtime_error(std::format("SDL_CreateAudioStream failed with: {}", SDL_GetError()));

            if (!SDL_BindAudioStream(this->device_id, this->out_stream))
                throw std::runtime_error(std::format("SDL_BindAudioStream failed with: {}", SDL_GetError()));

            SDL_SetAudioStreamGetCallback(this->out_stream, out_stream_callback, (void*)this);
        }
        ~Speaker() {
            SDL_DestroyAudioStream(this->out_stream);
            SDL_CloseAudioDevice(this->device_id);
        }
    };
}

#endif