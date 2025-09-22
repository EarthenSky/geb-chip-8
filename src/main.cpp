#include <iostream>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h> // redefines main for portability reasons

#include "emulator.h"

int main(int argc, char *argv[]) {
    Chip8::Emulator emulator;

    std::cout << "hello!" << std::endl;
    return 0;
}