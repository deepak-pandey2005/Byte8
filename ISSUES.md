# Byte8 Issue Backlog & Roadmap

This document outlines **10 structured engineering issues** for the Byte8 CHIP-8 ecosystem, covering the emulator core, assembler compiler, disassembler, web player, user interface, and developer tooling. Each issue includes detailed specifications, affected components, implementation steps, and acceptance criteria.

---

## Table of Contents

1. [Issue #1: Interactive Terminal / TUI Debugger (`--debug`)](#issue-1-interactive-terminal--tui-debugger---debug)
2. [Issue #2: Execution Breakpoints and Memory Watchpoints (`--break`, `--watch`)](#issue-2-execution-breakpoints-and-memory-watchpoints---break---watch)
3. [Issue #3: SUPER-CHIP (SCHIP-8) Extended Opcode & Hi-Res Mode (128x64)](#issue-3-super-chip-schip-8-extended-opcode--hi-res-mode-128x64)
4. [Issue #4: Disassembler Intelligent Label Reconstruction Pass](#issue-4-disassembler-intelligent-label-reconstruction-pass)
5. [Issue #5: Compiler Multi-Line Parameterized Macro System (`#macro` / `#endmacro`)](#issue-5-compiler-multi-line-parameterized-macro-system-macro--endmacro)
6. [Issue #6: Dynamic Keypad Rebinding & Custom Layout Profiles (`--keymap`)](#issue-6-dynamic-keypad-rebinding--custom-layout-profiles---keymap)
7. [Issue #7: Arbitrary Hex Color Theming & CRT Scanline Post-Processing](#issue-7-arbitrary-hex-color-theming--crt-scanline-post-processing)
8. [Issue #8: Native Gamepad & Controller Support via SDL2 GameController API](#issue-8-native-gamepad--controller-support-via-sdl2-gamecontroller-api)
9. [Issue #9: Authentic Retro Game Implementation: Space Invaders in Byte8 Assembly](#issue-9-authentic-retro-game-implementation-space-invaders-in-byte8-assembly)
10. [Issue #10: Automated CI/CD Pipeline with GitHub Actions & Pages Deployment](#issue-10-automated-cicd-pipeline-with-github-actions--pages-deployment)

---

### Issue #1: Interactive Terminal / TUI Debugger (`--debug`)
- **Labels**: `enhancement`, `tooling`, `debugger`, `cli`
- **Component**: [src/main.c](file:///home/deepak/OPENSOURCE/Byte8/src/main.c), [src/chip8.c](file:///home/deepak/OPENSOURCE/Byte8/src/chip8.c), [src/chip8.h](file:///home/deepak/OPENSOURCE/Byte8/src/chip8.h)
- **Status**: Open

#### Problem & Motivation
Currently, developers testing CHIP-8 binaries must run the emulator in real-time. If a program exhibits a logic bug or crash, there is no way to pause at a specific instruction, inspect registers, or view the stack and memory contents interactively.

#### Proposed Solution
Add an interactive debugging mode activated via `--debug` or `-d` CLI flag. When active:
- Emulation starts in a paused state or pauses when hitting a debug key (`F10` or `Tab`).
- A terminal-based or SDL-rendered HUD displays:
  - **Program Counter (PC)** and current opcode with mnemonic disassembly.
  - **Registers**: `V0` through `VF`, Index Register `I`, Stack Pointer `SP`, Delay Timer `DT`, Sound Timer `ST`.
  - **Call Stack**: Top 16 levels of call addresses.
  - **Next 5 instructions** queued after the current PC.
- Interactive CLI / Hotkey controls:
  - `s` or `F10`: Single-step instruction.
  - `c` or `F5`: Continue running full-speed until next break or pause.
  - `m <addr> [len]`: Dump memory hex view around `<addr>`.
  - `r`: Print all register values.
  - `q`: Terminate emulator.

#### Implementation Steps
1. In `src/chip8.h`, add debugger control fields: `bool debug_mode`, `bool single_step_flag`.
2. In `src/chip8.c`, provide a helper function `chip8_dump_state(const Chip8* chip8)` and `chip8_disasm_opcode(uint16_t opcode, char* out_buf, size_t buf_size)`.
3. In `src/main.c`, parse `--debug` / `-d` argument. If enabled, run a non-blocking terminal input thread or process keyboard commands during SDL event polling.
4. Integrate single-step execution within the main SDL loop.

#### Acceptance Criteria
- Running `./chip8 roms/pong.ch8 --debug` drops into the interactive debugger.
- Pressing `s` advances exactly one instruction and prints the updated PC, opcode, and modified register.
- Pressing `c` resumes real-time execution.
- Pressing `m 0x200 32` prints a 32-byte formatted hex dump from address `0x0200`.

---

### Issue #2: Execution Breakpoints and Memory Watchpoints (`--break`, `--watch`)
- **Labels**: `enhancement`, `debugger`, `core`
- **Component**: [src/chip8.c](file:///home/deepak/OPENSOURCE/Byte8/src/chip8.c), [src/chip8.h](file:///home/deepak/OPENSOURCE/Byte8/src/chip8.h), [src/main.c](file:///home/deepak/OPENSOURCE/Byte8/src/main.c)
- **Status**: Open

#### Problem & Motivation
Developers writing complex CHIP-8 assembly programs cannot easily isolate memory corruption or infinite loops without knowing when a specific memory address or register is mutated.

#### Proposed Solution
Implement hardware-like breakpoints and watchpoints in the virtual machine core:
- Breakpoints: Halts execution whenever `PC == target_address`.
- Watchpoints: Halts execution whenever memory at `address` or register `VX` is read or written.
- Add CLI parameters `--break <hex_addr>` (e.g. `--break 0x250`) and `--watch <reg/addr>` (e.g. `--watch V3`, `--watch 0x300`).

#### Technical Specification
- Add breakpoint array and watchpoint structures:
  ```c
  typedef struct {
      uint16_t address;
      bool is_active;
  } Breakpoint;

  typedef struct {
      uint16_t address; // 0x000-0xFFF for memory, 0x1000+X for VX
      uint8_t last_value;
      bool break_on_write;
      bool break_on_read;
      bool is_active;
  } Watchpoint;
  ```
- In `chip8_cycle()`, check whether `chip8->pc` matches any active breakpoint before opcode fetch.
- On match, trigger `chip8->is_paused = true` and print notification to console with current state.

#### Acceptance Criteria
- `--break 0x220` halts execution immediately when PC reaches `0x0220`.
- Terminal output logs `[BREAKPOINT] Hit address 0x0220 at cycle NNNN`.
- Unpausing (`Space` or `c`) continues execution until the next breakpoint.

---

### Issue #3: SUPER-CHIP (SCHIP-8) Extended Opcode & Hi-Res Mode (128x64)
- **Labels**: `enhancement`, `feature`, `core`, `graphics`
- **Component**: [src/chip8.c](file:///home/deepak/OPENSOURCE/Byte8/src/chip8.c), [src/chip8.h](file:///home/deepak/OPENSOURCE/Byte8/src/chip8.h), [compiler.c](file:///home/deepak/OPENSOURCE/Byte8/compiler.c), [disassembler.c](file:///home/deepak/OPENSOURCE/Byte8/disassembler.c)
- **Status**: Open

#### Problem & Motivation
Byte8 currently implements the standard COSMAC VIP CHIP-8 specification (64x32 resolution). Many classic and modern homebrew games require SUPER-CHIP 1.1 (SCHIP) extensions for 128x64 high resolution and 16x16 sprites.

#### Proposed Solution
Extend `Chip8` to optionally support SCHIP-8 mode:
- **Display Buffer**: Expand to 128x64 pixels (with backward-compatible 64x32 mode scaling).
- **SCHIP Opcodes**:
  - `00CN`: Scroll display down by N lines.
  - `00FB`: Scroll display right by 4 pixels.
  - `00FC`: Scroll display left by 4 pixels.
  - `00FD`: Exit CHIP-8 interpreter.
  - `00FE`: Disable extended screen mode (switch back to 64x32).
  - `00FF`: Enable extended screen mode (switch to 128x64).
  - `DXY0`: Draw 16x16 sprite at `(VX, VY)`.
  - `FX30`: Set `I = address of 10-byte font sprite for digit VX`.
  - `FX75`: Store `V0` through `VX` into RPL user flags (persistent flags).
  - `FX85`: Load `V0` through `VX` from RPL user flags.
- Include 10-byte high-resolution font sprites (0-9) in interpreter memory (address `0x050` - `0x09F`).

#### Acceptance Criteria
- SCHIP test ROMs and games (e.g. *Spacefight 2091*, *Car*) render at 128x64 without unknown opcode errors.
- Both 64x32 standard and 128x64 high-resolution modes render smoothly with correct aspect ratio.
- Compiler and Disassembler recognize `high`, `low`, `scroll_down N`, `scroll_left`, `scroll_right`, `font16 VX`, etc.

---

### Issue #4: Disassembler Intelligent Label Reconstruction Pass
- **Labels**: `enhancement`, `disassembler`, `tooling`
- **Component**: [disassembler.c](file:///home/deepak/OPENSOURCE/Byte8/disassembler.c)
- **Status**: Open

#### Problem & Motivation
The current disassembler outputs raw hexadecimal addresses for jumps, subroutine calls, and memory loads (e.g., `jump 0x228`, `call 0x310`). When reverse engineering a binary, this requires manual math to locate jump targets and makes recompilation fragile if instructions are inserted or removed.

#### Proposed Solution
Upgrade `disassembler.c` to a two-pass disassembler:
- **Pass 1 (Symbolic Address Discovery)**:
  - Traverse all opcodes from `0x0200` to end of ROM.
  - Record targets of `1NNN` (`jump NNN`), `2NNN` (`call NNN`), and `BNNN` (`jump_v0 NNN`).
  - Store discovered target addresses in a sorted symbol table.
- **Pass 2 (Annotated Code Generation)**:
  - If the current address is in the symbol table, emit a label before the instruction (e.g., `lbl_0228:`).
  - Disassemble branch instructions with symbolic label names (e.g., `jump lbl_0228` instead of `jump 0x0228`).
  - Distinguish between executable code and data blocks (`db`) for unaligned jumps or raw sprite data.

#### Acceptance Criteria
- Running `./disassembler roms/pong.ch8 -o roms/pong_reconstructed.txt` produces valid labels (`lbl_XXXX:`).
- Compiling `pong_reconstructed.txt` with `./compiler` reproduces the exact identical binary as original `pong.ch8`.
- No raw magic addresses in jump or call instructions.

---

### Issue #5: Compiler Multi-Line Parameterized Macro System (`#macro` / `#endmacro`)
- **Labels**: `enhancement`, `compiler`, `syntax`
- **Component**: [compiler.c](file:///home/deepak/OPENSOURCE/Byte8/compiler.c)
- **Status**: Open

#### Problem & Motivation
Byte8 assembly code currently supports basic single-line `#define` constant substitutions. Writing complex game logic (like 16-bit math, multi-byte memory transfers, or drawing multidimensional sprites) requires repeated boilerplate instructions throughout the code.

#### Proposed Solution
Implement a multi-line macro preprocessor directive:
```assembly
#macro DRAW_ENTITY(reg_x, reg_y, sprite_label, height)
    set I, sprite_label
    draw reg_x, reg_y, height
#endmacro

; Usage in game loop:
DRAW_ENTITY(V0, V1, player_sprite, 5)
```
- The compiler preprocessor expands macros before Pass 1 parsing.
- Arguments `$1`, `$2`, or named arguments are substituted into the macro body.
- Support nested macro expansions (up to 8 levels deep) with recursive detection to prevent infinite loops.

#### Acceptance Criteria
- Macro definition and invocation compiles properly without syntax errors.
- Compiles the expanded instructions at the exact byte offsets as manual repetition.
- Detailed error messages if macro argument counts mismatch or if `#macro` is missing `#endmacro`.

---

### Issue #6: Dynamic Keypad Rebinding & Custom Layout Profiles (`--keymap`)
- **Labels**: `enhancement`, `input`, `ux`
- **Component**: [src/main.c](file:///home/deepak/OPENSOURCE/Byte8/src/main.c), [src/chip8.h](file:///home/deepak/OPENSOURCE/Byte8/src/chip8.h)
- **Status**: Open

#### Problem & Motivation
The CHIP-8 uses a 4x4 hexadecimal keypad (`0` through `F`). Currently, keybindings in `src/main.c` are hardcoded to standard US QWERTY:
```
1 2 3 C      ->  1 2 3 4
4 5 6 D      ->  Q W E R
7 8 9 E      ->  A S D F
A 0 B F      ->  Z X C V
```
Users with AZERTY, QWERTZ, Dvorak, or Colemak keyboards or who prefer arrow-key or WASD layouts cannot comfortably play games like Pong or Snake.

#### Proposed Solution
Add customizable key mapping:
- Provide command-line option `--keymap <profile_or_string>`:
  - Preset profiles: `--keymap qwerty`, `--keymap azerty`, `--keymap wasd`.
  - Custom mapping string of 16 characters or key symbols.
- Optional keymap configuration file (`~/.byte8_keymap` or `./keymap.ini`).
- On-screen keypad mapping help in title bar or overlay when pressing `F1`.

#### Acceptance Criteria
- Launching with `./chip8 roms/snake.ch8 --keymap azerty` uses AZERTY keys (`&`, `é`, `"`, `'`, `A`, `Z`, `E`, `R`, etc.).
- Invalid keymaps display friendly syntax errors listing valid key identifiers.

---

### Issue #7: Arbitrary Hex Color Theming & CRT Scanline Post-Processing
- **Labels**: `enhancement`, `graphics`, `ux`
- **Component**: [src/main.c](file:///home/deepak/OPENSOURCE/Byte8/src/main.c), [web/index.html](file:///home/deepak/OPENSOURCE/Byte8/web/index.html)
- **Status**: Open

#### Problem & Motivation
Byte8 currently has 5 hardcoded color presets (Classic Green, Amber CRT, Cyberpunk, Black & White, Game Boy). Users cannot select custom brand colors or arbitrary hex palettes (e.g. Dracula, Solarized, Nord, Catppuccin). Furthermore, pixel art on modern high-DPI screens can look overly sharp without optional retro scanline filtering.

#### Proposed Solution
1. **Hex Color Arguments**:
   - Add CLI options `--fg #RRGGBB` (or `--foreground`) and `--bg #RRGGBB` (or `--background`).
   - Validate hex color strings and apply them dynamically to SDL rendering.
2. **CRT Scanline / Phosphor Glow Shader/Filter**:
   - Add `--crt` or `--scanlines` CLI flag.
   - Render alternate horizontal scanline overlays with 15-25% alpha darkening in SDL renderer.
   - Provide corresponding controls in the Web UI (`web/index.html`) using HTML5 canvas filters or CSS scanline overlays.

#### Acceptance Criteria
- `./chip8 roms/pong.ch8 --fg #50FA7B --bg #282A36 --scanlines` renders with custom Dracula colors and visible CRT scanline lines.
- Web UI includes color pickers for foreground and background colors.

---

### Issue #8: Native Gamepad & Controller Support via SDL2 GameController API
- **Labels**: `enhancement`, `input`, `hardware`
- **Component**: [src/main.c](file:///home/deepak/OPENSOURCE/Byte8/src/main.c)
- **Status**: Open

#### Problem & Motivation
CHIP-8 games are arcade and console experiences, but Byte8 currently only accepts keyboard input. Users cannot play with Xbox, PlayStation, Nintendo Switch, or generic USB/Bluetooth gamepads.

#### Proposed Solution
Implement gamepad support using SDL2's `SDL_GameController` subsystem:
- Initialize `SDL_INIT_GAMECONTROLLER` during SDL startup.
- Handle `SDL_CONTROLLERDEVICEADDED` and `SDL_CONTROLLERDEVICEREMOVED` hotplug events.
- Map controller inputs to CHIP-8 keys:
  - D-Pad Up / Analog Up -> Key `2` (Up)
  - D-Pad Down / Analog Down -> Key `8` (Down)
  - D-Pad Left / Analog Left -> Key `4` (Left)
  - D-Pad Right / Analog Right -> Key `6` (Right)
  - A / Cross Button -> Key `5` (Action / Fire)
  - B / Circle Button -> Key `6` or `E`
  - Start Button -> Pause / Resume (`P`)
  - Back / Select Button -> Reset (`F5` or State reload)
- Log connected controller name on startup (e.g., `[INPUT] Controller connected: Xbox One Wireless Controller`).

#### Acceptance Criteria
- Connecting any standard USB/Bluetooth controller allows full navigation and gameplay in *Pong* and *Snake*.
- Hotplugging (connecting/disconnecting while emulator is running) works gracefully without crashes or freezes.

---

### Issue #9: Authentic Retro Game Implementation: Space Invaders in Byte8 Assembly
- **Labels**: `enhancement`, `game`, `assembly`, `roms`
- **Component**: [roms/](file:///home/deepak/OPENSOURCE/Byte8/roms/), [compiler.c](file:///home/deepak/OPENSOURCE/Byte8/compiler.c)
- **Status**: Open

#### Problem & Motivation
The repository contains `pong.txt`, `snake.txt`, and `demo.txt`, demonstrating the assembler's capabilities. However, a multi-entity shooter game with collision physics, sound effects, alien march animations, player projectiles, and scoring would provide an ultimate test for the compiler, emulator, and disassembler.

#### Proposed Solution
Create a fully documented, original Byte8 assembly program in `roms/invaders.txt` and compiled binary `roms/invaders.ch8`:
- **Game Mechanics**:
  - Player ship moved left/right with keys `4` and `6` (or `A` and `D`).
  - Fire laser projectile using key `5` (or `W`).
  - Fleet of moving alien invaders that march across and step down when hitting wall edges.
  - Collision detection between player missile and aliens (clearing alien sprite and incrementing score).
  - Game over when aliens reach the player's vertical threshold or player is hit.
  - Buzzer sound timer (`ST`) trigger on laser fire and alien destruction.
- **Code Structure**:
  - Clean modular assembly with labels, constant definitions (`PLAYER_Y`, `SPRITE_HEIGHT`), and inline comments.
  - Packaged and pre-compiled in `roms/` with compilation verified in `tests/run_tests.sh`.

#### Acceptance Criteria
- `roms/invaders.txt` compiles cleanly with `./compiler roms/invaders.txt roms/invaders.ch8`.
- `roms/invaders.ch8` executes at 60 FPS in Byte8 native and web player without glitches or crashes.
- Disassembling and reassembling passes round-trip verification tests.

---

### Issue #10: Automated CI/CD Pipeline with GitHub Actions & Pages Deployment
- **Labels**: `infrastructure`, `ci-cd`, `devops`
- **Component**: `.github/workflows/ci.yml`, [Makefile](file:///home/deepak/OPENSOURCE/Byte8/Makefile)
- **Status**: Open

#### Problem & Motivation
Currently, code compilation, memory leak checks, round-trip tests, and WebAssembly builds are run manually on local developer machines. Pull requests or commits cannot be automatically verified against regressions or cross-platform compilation breaks.

#### Proposed Solution
Create a production-grade GitHub Actions CI workflow in `.github/workflows/ci.yml`:
1. **Linux Build & Unit Tests Matrix**:
   - Ubuntu latest runner.
   - Install `libsdl2-dev` and build tools.
   - Run `make clean && make all`.
   - Run `make test` (validating 59+ opcode tests, compiler tests, disassembler round-trips).
2. **WebAssembly Build**:
   - Setup Emscripten (`emsetup`).
   - Run `emcc` target to compile `web/chip8.wasm`.
3. **Automated GitHub Pages Deployment**:
   - On pushes to `main`, automatically deploy the `web/` directory (HTML, JS, and compiled WASM) to GitHub Pages so users can play Byte8 online directly in browser.
4. **Static Analysis & Sanitizers**:
   - Build with `-fsanitize=address,undefined` to catch memory safety violations automatically.

#### Acceptance Criteria
- Pushing to repository triggers GitHub Actions workflow.
- All builds and tests pass cleanly with zero warnings or errors.
- Web emulator is continuously deployed and accessible on GitHub Pages.
