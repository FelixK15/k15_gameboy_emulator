**Note that this project is currently in development. Features are still missing, stuff is still being build.**

## What does this project try to do? (ELI5)
This project tries to emulate the Nintendo GameBoy and Nintendo GameBoy Color game system hardware accurately so that games for these game systems can be played on hardware
that is natively not able to do so.

![image](https://user-images.githubusercontent.com/7531672/120161123-42fea680-c1f7-11eb-91d4-3522f0989919.png)

## How do I build this software locally?

Currently the codebase can only be build on a Windows machine with Microsoft Visual Studio installed.
If these requirements are met, simply run the `build.bat` - This should spawn a `cl.exe` instance which will compile
the project and output a `k15_win32_gb_emulator.exe`

## How do I navigate the codebase?

The win32 entry point and interface is located in the `k15_win32_gb_emulator.cpp` file.
All Nintendo GameBoy hardware emulation code can be found in `k15_gb_emulator.h` with the main entry point being `runGBEmulator()`.

## Can I run this emulator locally?

Yes, although it's currently only being tested with tetris. If you want to run tetris using this emulator though copy your tetris rom file
next to the compiled exe and rename it to "rom.gb". After that you should be able to start the emulator and run tetris.

Input on keyboard:
arrow keys  =     digi pad
ctrl        =     start
alt         =     select
a           =     a
b           =     b

## What do I have to do to port this to platform X?

Inside `k15_win32_gb_emulator.cpp` you can find the Win32 platform layer for orientation.
Basically, all you have to do is create an emulator instance by first providing the API with a block of memory.
The required memory size can be calculated by calling `calculateGBEmulatorInstanceMemoryRequirementsInBytes()`.

An emulator instance can be created by calling `createGBEmulatorInstance()` (this function will take the aforemention memory block).
Each emulator instance is independent from one another (goal would be link cable multiplayer using multiple instances in a single process)

After an emulator instance could be created, load a game rom using `loadGBEmulatorInstanceRom()` (this rom data should be provided as a memory blob
either by mapping the rom file or by copying the rom file content to a memory buffer).

Now, call `runGBEmulatorInstance()` each frame and check with `hasGBEmulatorInstanceHitVBlank()` if the GameBoy frame has finished. By calling `getGBEmulatorInstanceFrameBuffer()` you can get a pointer to the framebuffer data (attention: the pixel format is still in gameboy format - 2bpp. To convert to RGB8 call `convertGBFrameBufferToRGB8Buffer()`).

To set the joystick state of the emulator instance, call `setGBEmulatorInstanceJoypadState()` (this function is thread-safe so that input code can 
high frequently poll asynchronously for smaller input lag). 

State loading/saving can be done using `calculateGBEmulatorStateSizeInBytes()`, `storeGBEmulatorInstanceState()` and `loadGBEmulatorInstanceState()`.
For an example of how to use the API please take a look at `k15_gb_emulator_ui.cpp`, specificially the `doEmulatorStateSaveLoadView()` function.

In case the emulator isn't run on a 60Hz monitor, the monitor refresh rate can be propagated to the emulator using the `setGBEmulatorInstanceMonitorRefreshRate()` function to get correct timings if additionally also using vsync.

## Current State and Goals

- [x] Implement opcodes to get into tetris copyright text
- [x] Display video ram and show the tetris tileset
- [x] Emulate correct frame timings independend of monitor refresh rate
- [x] Implement opcodes to get into tetris menu and start a game
- [x] Test Tetris
- [x] Add IMGUI dependency to be able to add debug features
  - [x] Add breakpoints
  - [x] Add conditional breakpoints
  - [x] Add code stepping
  - [x] Add memory inspection
  - [x] Add VRAM viewer
- [ ] Implement rom-switching
  - [x] MBC1
  - [ ] MBC3
  - [ ] MBC5
- [x] implement window support
- [x] implement background scrolling
- [x] complete sprite features
  - [x] 10 sprite per line limit
  - [x] x flip 
  - [x] y flip
  - [x] support for different palettes
- [ ] verify correctness with Blargg's Gameboy hardware test ROMs
  - [x] pass instr_timing
  - [ ] pass interrupt_time
  - [ ] pass mem_timing
  - [ ] pass cpu_instrs
- [ ] Implement Nintendo GameBoy Color features
- [ ] Implement sound

Currently the emulator is purely run on the CPU, meaning that the Nintendo GameBoy CPU as well as the PPU are being emulated purely on the host CPU.
The final image from the emulator will be blit to the screen by the GPU to utilize programmable scaling options and filters.
