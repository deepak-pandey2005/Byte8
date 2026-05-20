#include "chip8.h"
#include <SDL2/SDL.h>

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: %s <ROM file>\n", argv[0]);
    return 1;
  }

  Chip8 chip8;
  chip8_init(&chip8);
  chip8_load_rom(&chip8, argv[1]);

  SDL_Init(SDL_INIT_VIDEO);

  SDL_Window *window = SDL_CreateWindow("CHIP-8", SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED, 640, 320, 0);

  SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);

  while (1) {
    // Simple event loop to handle window closing smoothly
    SDL_Event event;
    if (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        break;
      }
    }

    chip8_cycle(&chip8);

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
      chip8.draw_flag = false; // Reset the flag
    }

    SDL_Delay(2); // Keeps CPU usage in check
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
