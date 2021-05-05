**Note that this project is currently in development. Features are still missing, stuff is still being build.**

## What does this project try to do? (ELI5)
This project tries to emulate the Nintendo GameBoy and Nintendo GameBoy Color game system hardware accurately so that games for these game systems can be played on hardware
that is natively not able to do so.

## How do I build this software locally?

Currently the codebase can only be build on a Windows machine with Microsoft Visual Studio installed.
If these requirements are met, simply run the `build.bat` - This should spawn a `cl.exe` instance which will compile
the project and output a `k15_win32_gb_emulator.exe`

## How do I navigate the codebase?

The win32 entry point and interface is located in the `k15_win32_gb_emulator.cpp` file.
All Nintendo GameBoy hardware emulation code can be found in `k15_gb_emulator.h` with the main entry point being `runSingleInstruction()`.

## Current State and Goals

- [x] Implement opcodes to get into tetris copyright text
- [x] Display video ram and show the tetris tileset
- [ ] Implement opcodes to get into tetris menu and start a game
- [ ] Test Tetris
- [x] Add IMGUI dependency to be able to add debug features
  - [ ] Add breakpoints
  - [ ] Add conditional breakpoints
  - [ ] Add code stepping
  - [ ] Add memory inspection
  - [ ] Add VRAM viewer
- [ ] Implement rom-switching
  - [ ] MBC1
  - [ ] MBC3
  - [ ] MBC5
- [ ] verify correctness with Blargg's Gameboy hardware test ROMs
- [ ] Implement Nintendo GameBoy Color features
- [ ] Implement sound

Currently the emulator is purely run on the CPU, meaning that the Nintendo GameBoy CPU as well as the PPU are being emulated purely on the host CPU.
The final image from the emulator will be blit to the screen by the GPU to utilize programmable scaling options and filters.
