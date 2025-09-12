# geb chip 8
- my very own chip 8 emulator

## setup
- `git clone <this repo> --recurse-submodules`
// - cd vendored
// - `git clone https://github.com/libsdl-org/SDL` (SDL3)
- follow instructions at top of CMakeLists.txt

## .chip8 format
- specifies program memory starting at 0x200
- each line is how many bytes the operation is
- each line may end with a hexadecimal marker in parens specifying the expected starting address of this execution.
- ex: `(0x2f1)`