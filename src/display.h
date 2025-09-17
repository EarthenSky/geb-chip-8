#ifndef DISPLAY_H
#define DISPLAY_H

#include <array>
#include <stdexcept>
#include <format>

#include <SDL3/SDL.h>

namespace Chip8 {
    class Display {
    private:
        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;

        // the edge size of a pixel rendered on the native display
        const float scale_factor = 4.0;

    public:
        Display() {
            if (!SDL_Init(SDL_INIT_VIDEO))
                throw std::runtime_error(std::format("SDL_Init error: {}\n", SDL_GetError()));

            if (!SDL_CreateWindowAndRenderer(
                "Chip8 Display",
                (int)(Chip8::Display::SCREEN_WIDTH  * this->scale_factor),
                (int)(Chip8::Display::SCREEN_HEIGHT * this->scale_factor),
                0, &this->window, &this->renderer
            ))
                throw std::runtime_error(std::format("SDL_CreateWindowAndRenderer error: {}\n", SDL_GetError()));

            SDL_SetRenderVSync(this->renderer, SDL_RENDERER_VSYNC_ADAPTIVE);
        }

        void render_buffer() {
            // TODO: later, consider a more efficient way to send this data to the gpu & render it
            for (uint16_t y = 0; y < Chip8::Display::SCREEN_HEIGHT; y++) {
                for (uint16_t x = 0; x < Chip8::Display::SCREEN_WIDTH; x++) {
                    bool px_active = buffer[x + y * Chip8::Display::SCREEN_WIDTH];
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

        constexpr static uint16_t SCREEN_WIDTH  = 64;
        constexpr static uint16_t SCREEN_HEIGHT = 32;
        std::array<bool, SCREEN_WIDTH * SCREEN_HEIGHT> buffer;

        ~Display() {
            SDL_DestroyRenderer(this->renderer);
            SDL_DestroyWindow(this->window);
            SDL_Quit();
        }
    };
}

// Chip8Display chip8_display;

#endif