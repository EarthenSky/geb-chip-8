#include <iostream>
#include <fstream>
#include <filesystem>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h> // redefines main for portability reasons

#include "emulator.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cout << "ERROR: only accepts a single argument: path to a .chip8 file" << std::endl;
        exit(1);
    }
    
    try {
        std::filesystem::path file_path(argv[1]);
        size_t size = std::filesystem::file_size(file_path);
        std::string program_string(size, '\0');
        std::ifstream chip8_file(file_path);
        chip8_file.read(program_string.data(), size);

        Chip8::Emulator<false> emulator;
        if (!emulator.load_program(program_string)) {
            std::cout << "ERROR: invalid program. please fix error before running again" << std::endl;
            exit(1);
        }

        // the user doesn't have a great way of stopping the program aside from killing the process, but oh well!
        emulator.block_run();

        std::cout << "Press any key to exit..." << std::endl; 
        std::string _line;
        std::getline(std::cin, _line);
        return 0;
    } catch (const std::exception& e) {
        std::cout << "ERROR: " << e.what() << std::endl;
        exit(1);
    } catch (...) {
        std::cout << "ERROR: got unknown exception" << std::endl;
        exit(2);
    }
}