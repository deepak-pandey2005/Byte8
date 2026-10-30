#include "chip8.h"
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdio.h>

// CHIP-8 Hex Keypad Map:
// 1 2 3 C      -> 1 2 3 4
// 4 5 6 D      -> Q W E R
// 7 8 9 E  ==> -> A S D F
// A 0 B F      -> Z X C V
static int map_sdl_key_to_chip8(SDL_Keycode key) {
  switch (key) {
  case SDLK_1: return 0x1;
  case SDLK_2: return 0x2;
  case SDLK_3: return 0x3;
  case SDLK_4: return 0xC;

  case SDLK_q: return 0x4;
  case SDLK_w: return 0x5;
  case SDLK_e: return 0x6;
  case SDLK_r: return 0xD;

  case SDLK_a: return 0x7;
  case SDLK_s: return 0x8;
  case SDLK_d: return 0x9;
  case SDLK_f: return 0xE;

  case SDLK_z: return 0xA;
  case SDLK_x: return 0x0;
  case SDLK_c: return 0xB;
  case SDLK_v: return 0xF;

  default: return -1;
  }
}

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: %s <ROM file>\n", argv[0]);
    return 1;
  }

  Chip8 chip8;
  chip8_init(&chip8);
  chip8_load_rom(&chip8, argv[1]);

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
    fprintf(stderr, "SDL Init failed: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Window *window = SDL_CreateWindow(
      "Byte8 CHIP-8 Emulator",
      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      640, 320, SDL_WINDOW_SHOWN);

  if (!window) {
    fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) {
    renderer = SDL_CreateRenderer(window, -1, 0);
  }

  // CHIP-8 timer frequency is 60 Hz (~16.66 ms per tick)
  // CPU clock runs ~500 to 700 Hz (around 8 to 10 cycles per timer tick)
  const int CYCLES_PER_FRAME = 9;
  Uint32 last_timer_update = SDL_GetTicks();
  bool running = true;

  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        running = false;
      } else if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_ESCAPE) {
          running = false;
        }
        int k = map_sdl_key_to_chip8(event.key.keysym.sym);
        if (k != -1) {
          chip8.keypad[k] = 1;
        }
      } else if (event.type == SDL_KEYUP) {
        int k = map_sdl_key_to_chip8(event.key.keysym.sym);
        if (k != -1) {
          chip8.keypad[k] = 0;
        }
      }
    }

    // Run CPU cycles for this frame slice
    for (int i = 0; i < CYCLES_PER_FRAME; i++) {
      chip8_cycle(&chip8);
    }

    // 60 Hz timer decrement
    Uint32 current_time = SDL_GetTicks();
    if (current_time - last_timer_update >= 16) {
      chip8_update_timers(&chip8);
      last_timer_update = current_time;
    }

    // Render if screen changed
    if (chip8.draw_flag) {
      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
      SDL_RenderClear(renderer);

      SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
      for (int y = 0; y < VIDEO_HEIGHT; y++) {
        for (int x = 0; x < VIDEO_WIDTH; x++) {
          if (chip8.gfx[y * VIDEO_WIDTH + x]) {
            SDL_Rect rect = {x * 10, y * 10, 10, 10};
            SDL_RenderFillRect(renderer, &rect);
          }
        }
      }
      SDL_RenderPresent(renderer);
      chip8.draw_flag = false;
    }

    SDL_Delay(1); // Relax CPU
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
