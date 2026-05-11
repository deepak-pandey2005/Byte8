#include <SDL2/SDL.h>
#include "chip8.h"

int main(int argc, char **argv)
{
    Chip8 chip8;

    chip8_init(&chip8);

    chip8_load_rom(&chip8, argv[1]);

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *window =
        SDL_CreateWindow(
            "CHIP-8",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            640,
            320,
            0
        );

    SDL_Renderer *renderer =
        SDL_CreateRenderer(window, -1, 0);

    while (1)
    {
        chip8_cycle(&chip8);

        SDL_SetRenderDrawColor(renderer, 0,0,0,255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer,255,255,255,255);

        for (int y = 0; y < 32; y++)
        {
            for (int x = 0; x < 64; x++)
            {
                if (chip8.display[y * 64 + x])
                {
                    SDL_Rect rect = {
                        x * 10,
                        y * 10,
                        10,
                        10
                    };

                    SDL_RenderFillRect(renderer, &rect);
                }
            }
        }

        SDL_RenderPresent(renderer);

        SDL_Delay(2);
    }
}
