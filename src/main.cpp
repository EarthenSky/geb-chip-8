#include <iostream>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h> // redefines main for portability reasons

#include "timer.h"
#include "speaker.h"
#include "display.h"
#include "keyboard.h"
// #include "emulator.h"

// TODO: next, add emulator & instructions, then move all this (probably) into that module

Chip8::Timer60hz sound_timer;
Chip8::Timer60hz delay_timer;

Chip8::Keyboard keyboard;

int main(int argc, char *argv[]) {
    sound_timer.set(0);
    delay_timer.set(0);

    std::cout << "hello!" << std::endl;
    return 0;
}