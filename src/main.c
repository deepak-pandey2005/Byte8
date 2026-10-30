#include "chip8.h"
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AUDIO_SAMPLE_RATE 44100
#define BEEP_FREQUENCY 440 // 440 Hz standard pitch
#define BEEP_VOLUME 3000   // 16-bit audio volume level

typedef struct {
  bool playing;
  int sample_index;
} AudioState;

typedef struct {
  const char *rom_path;
  int scale;
  int clock_speed; // in Hz (default: 540)
  int fps;         // in Hz (default: 60)
  bool mute;
} EmulatorConfig;

// SDL Audio Callback generating a smooth 440 Hz square wave beep
static void audio_callback(void *userdata, Uint8 *stream, int len) {
  AudioState *state = (AudioState *)userdata;
  int16_t *buffer = (int16_t *)stream;
  int samples = len / (int)sizeof(int16_t);

  for (int i = 0; i < samples; i++) {
    if (state->playing) {
      int period = AUDIO_SAMPLE_RATE / BEEP_FREQUENCY;
      if ((state->sample_index % period) < (period / 2)) {
        buffer[i] = BEEP_VOLUME;
      } else {
        buffer[i] = -BEEP_VOLUME;
      }
      state->sample_index++;
    } else {
      buffer[i] = 0;
      state->sample_index = 0;
    }
  }
}

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

static void print_usage(const char *prog_name) {
  printf("Byte8 CHIP-8 Emulator\n");
  printf("Usage: %s <ROM file> [options]\n\n", prog_name);
  printf("Options:\n");
  printf("  --scale <num>        Pixel scale factor (default: 10, e.g. 15 for 960x480)\n");
  printf("  --clock-speed <hz>   CPU clock speed in Hz (default: 540)\n");
  printf("  --fps <num>          Display/timer refresh rate in Hz (default: 60)\n");
  printf("  --mute               Disable audio buzzer\n");
  printf("  -h, --help           Show this help message\n");
}

static int parse_args(int argc, char **argv, EmulatorConfig *config) {
  config->rom_path = NULL;
  config->scale = 10;
  config->clock_speed = 540;
  config->fps = 60;
  config->mute = false;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      print_usage(argv[0]);
      return 0; // Exit successfully with help
    } else if (strcmp(argv[i], "--scale") == 0 && i + 1 < argc) {
      config->scale = atoi(argv[++i]);
      if (config->scale < 1 || config->scale > 50) {
        fprintf(stderr, "Error: --scale must be between 1 and 50.\n");
        return -1;
      }
    } else if (strcmp(argv[i], "--clock-speed") == 0 && i + 1 < argc) {
      config->clock_speed = atoi(argv[++i]);
      if (config->clock_speed < 10 || config->clock_speed > 50000) {
        fprintf(stderr, "Error: --clock-speed must be between 10 and 50000 Hz.\n");
        return -1;
      }
    } else if (strcmp(argv[i], "--fps") == 0 && i + 1 < argc) {
      config->fps = atoi(argv[++i]);
      if (config->fps < 10 || config->fps > 240) {
        fprintf(stderr, "Error: --fps must be between 10 and 240.\n");
        return -1;
      }
    } else if (strcmp(argv[i], "--mute") == 0) {
      config->mute = true;
    } else if (argv[i][0] != '-') {
      if (!config->rom_path) {
        config->rom_path = argv[i];
      } else {
        fprintf(stderr, "Unexpected extra argument: %s\n", argv[i]);
        return -1;
      }
    } else {
      fprintf(stderr, "Unknown option: %s\n", argv[i]);
      print_usage(argv[0]);
      return -1;
    }
  }

  if (!config->rom_path) {
    print_usage(argv[0]);
    return -1;
  }

  return 1; // Success with valid ROM
}

int main(int argc, char **argv) {
  EmulatorConfig config;
  int parse_res = parse_args(argc, argv, &config);
  if (parse_res <= 0) {
    return (parse_res == 0) ? 0 : 1;
  }

  Chip8 chip8;
  chip8_init(&chip8);
  chip8_load_rom(&chip8, config.rom_path);

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
    fprintf(stderr, "SDL Init failed: %s\n", SDL_GetError());
    return 1;
  }

  // Audio setup
  AudioState audio_state = { .playing = false, .sample_index = 0 };
  SDL_AudioDeviceID audio_device = 0;

  if (!config.mute) {
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = AUDIO_SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 512;
    want.callback = audio_callback;
    want.userdata = &audio_state;

    audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (audio_device != 0) {
      SDL_PauseAudioDevice(audio_device, 0);
    }
  }

  int win_width = VIDEO_WIDTH * config.scale;
  int win_height = VIDEO_HEIGHT * config.scale;

  char window_title[128];
  snprintf(window_title, sizeof(window_title),
           "Byte8 CHIP-8 Emulator [%d Hz, %d FPS]",
           config.clock_speed, config.fps);

  SDL_Window *window = SDL_CreateWindow(
      window_title,
      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      win_width, win_height, SDL_WINDOW_SHOWN);

  if (!window) {
    fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
    if (audio_device) SDL_CloseAudioDevice(audio_device);
    SDL_Quit();
    return 1;
  }

  SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) {
    renderer = SDL_CreateRenderer(window, -1, 0);
  }

  int cycles_per_frame = config.clock_speed / config.fps;
  if (cycles_per_frame < 1) cycles_per_frame = 1;

  double timer_interval_ms = 1000.0 / config.fps;
  Uint32 last_timer_update = SDL_GetTicks();
  bool running = true;

  printf("Running %s at %d Hz CPU, %d FPS, %dx scale (Window: %dx%d)%s\n",
         config.rom_path, config.clock_speed, config.fps, config.scale,
         win_width, win_height, config.mute ? " [Muted]" : "");

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
    for (int i = 0; i < cycles_per_frame; i++) {
      chip8_cycle(&chip8);
    }

    // Timer decrement at configured rate (standard 60 Hz)
    Uint32 current_time = SDL_GetTicks();
    if ((double)(current_time - last_timer_update) >= timer_interval_ms) {
      chip8_update_timers(&chip8);
      last_timer_update = current_time;

      if (audio_device != 0) {
        SDL_LockAudioDevice(audio_device);
        audio_state.playing = (chip8.sound_timer > 0);
        SDL_UnlockAudioDevice(audio_device);
      }
    }

    // Render if screen updated
    if (chip8.draw_flag) {
      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
      SDL_RenderClear(renderer);

      SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
      for (int y = 0; y < VIDEO_HEIGHT; y++) {
        for (int x = 0; x < VIDEO_WIDTH; x++) {
          if (chip8.gfx[y * VIDEO_WIDTH + x]) {
            SDL_Rect rect = {x * config.scale, y * config.scale, config.scale, config.scale};
            SDL_RenderFillRect(renderer, &rect);
          }
        }
      }
      SDL_RenderPresent(renderer);
      chip8.draw_flag = false;
    }

    SDL_Delay(1);
  }

  if (audio_device != 0) {
    SDL_CloseAudioDevice(audio_device);
  }
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
