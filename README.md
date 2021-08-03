**Note that this project is currently in development. Features are still missing, stuff is still being build.**

## What does this project try to do? (ELI5)
This project tries to emulate the Nintendo GameBoy and Nintendo GameBoy Color game system hardware accurately so that games for these game systems can be played on hardware
that is natively not able to do so.

![image](https://user-images.githubusercontent.com/7531672/127722959-08906958-a2d4-4002-ab79-d5f5692cdc5c.png)
![image](https://user-images.githubusercontent.com/7531672/128100368-f75af3f2-ef31-401f-b329-b24052612a08.png)
![image](https://user-images.githubusercontent.com/7531672/127790229-6ecf85f2-b8c5-4b1b-b496-d01e370be5a8.png)
![image](https://user-images.githubusercontent.com/7531672/127790240-f8dc9de6-71e9-449d-bbf2-f958d01d918e.png)
![image](https://user-images.githubusercontent.com/7531672/128100405-4dfe06cc-08b5-436b-9d18-e0119b732418.png)

## How do I build this software locally?

Currently the codebase can only be build on a Windows machine with Microsoft Visual Studio installed.
If these requirements are met, simply run the `build_msvc_cl.bat` or `build_msvc_clang` - This should spawn a `cl.exe` or `clang.exe` instance which will compile
the project and output the executible. In case you want to build a debug version, pass the argument `debug` to the build scripts.

## How do I navigate the codebase?

The win32 entry point `WinMain()` and interface is located in the `k15_win32_gb_emulator.cpp` file.
All Nintendo GameBoy hardware emulation code can be found in `k15_gb_emulator.h` with the main entry point being `runGBEmulatorForCycles()`.

## Can I run this emulator locally?

Yes, prebuild release can be found [here](https://github.com/FelixK15/k15_gameboy_emulator/releases)
Input is supported via keyboard and xinput gamepads.

Input on keyboard:
* arrow keys  -     digi pad
* return      -     start
* space       -     select
* a           -     a
* b           -     b

input on xinput pad:
* digi pad    -     digi pad
* x,a         -     a
* y,b         -     b

## What do I have to do to port this to platform X?

Inside `k15_win32_gb_emulator.cpp` you can find the Win32 platform layer for orientation.
Basically, all you have to do is create an emulator instance by first providing the API with a block of memory.
The required memory size can be calculated by calling `calculateGBEmulatorMemoryRequirementsInBytes()`.

An emulator instance can be created by calling `createGBEmulatorInstance()` (this function will take the aforemention memory block).
Each emulator instance is independent from one another (goal would be link cable multiplayer using multiple instances in a single process)

After an emulator instance could be created, load a game rom using `loadGBEmulatorRom()` (this rom data should be provided as a memory blob
either by mapping the rom file or by copying the rom file content to a memory buffer).

Now, call `runGBEmulatorForCycles()` each frame and check the return value for the flag `K15_GB_VBLANK_EVENT_FLAG` (indicating, that the GameBoy frame has finished). By calling `getGBEmulatorFrameBuffer()` you can get a pointer to the framebuffer data (attention: the pixel format is still in gameboy format - 2bpp. To convert to RGB8 call `convertGBFrameBufferToRGB8Buffer()`).

To set the joystick state of the emulator instance, call `setGBEmulatorJoypadState()` (this function is thread-safe so that input code can 
high frequently poll asynchronously for smaller input lag). 

State loading/saving can be done using `calculateGBEmulatorStateSizeInBytes()`, `storeGBEmulatorState()` and `loadGBEmulatorState()`.
For an example of how to use the API please take a look at `k15_win32_gb_emulator.cpp`, specificially the `loadStateInSlot()` and `saveStateInSlot()` functions.

## Current State and Goals

- [x] Emulate correct frame timings independend of monitor refresh rate
- [ ] Implement rom-switching
  - [x] MBC1
  - [ ] MBC3
  - [x] MBC5
- [x] implement window support
- [x] implement background scrolling
- [x] complete sprite features
  - [x] 10 sprite per line limit
  - [x] x flip 
  - [x] y flip
  - [x] support for different palettes
  - [ ] corrct 16x8 sprite handling
- [ ] verify correctness with Blargg's Gameboy hardware test ROMs
  - [x] pass instr_timing
  - [ ] pass interrupt_time
  - [ ] pass mem_timing
  - [x] pass cpu_instrs
- [ ] Implement Nintendo GameBoy Color features
- [ ] Implement sound

Currently the emulator is purely run on the CPU, meaning that the Nintendo GameBoy CPU as well as the PPU are being emulated purely on the host CPU.
The final image from the emulator will be blit to the screen by the GPU to utilize programmable scaling options and filters.
