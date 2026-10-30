#include "chip8.h"
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define AUDIO_SAMPLE_RATE 44100
#define PI 3.14159265358979323846

typedef enum {
  WAVE_SQUARE,
  WAVE_SINE,
  WAVE_TRIANGLE,
  WAVE_SAWTOOTH
} WaveformType;

typedef struct {
  bool playing;
  int sample_index;
  int frequency;
  int volume;           // 0 to 100
  WaveformType waveform;
} AudioState;

typedef struct {
  const char *name;
  uint8_t bg_r, bg_g, bg_b;
  uint8_t fg_r, fg_g, fg_b;
} ColorTheme;

static const ColorTheme COLOR_THEMES[] = {
  { "classic",  0,   0,   0,   255, 255, 255 }, // Classic black & white
  { "matrix",   5,  18,   5,     0, 255,  70 }, // Terminal matrix phosphor green
  { "amber",   24,  14,   2,   255, 176,   0 }, // Vintage amber monochrome CRT
  { "gameboy", 15,  56,  15,   155, 188,  15 }, // Nintendo Game Boy green tint
  { "cyber",   16,  12,  36,     0, 255, 204 }, // Synthwave cyan on dark purple
};
#define THEME_COUNT ((int)(sizeof(COLOR_THEMES) / sizeof(COLOR_THEMES[0])))

typedef struct {
  const char *rom_path;
  int scale;
  int clock_speed; // in Hz (default: 540)
  int fps;         // in Hz (default: 60)
  bool mute;
  int volume;      // 0 to 100 (default: 50)
  int pitch;       // in Hz (default: 440)
  WaveformType waveform; // square, sine, triangle, sawtooth
  ColorTheme theme;
} EmulatorConfig;

// Audio Callback synthesizing the selected waveform with volume scaling
static void audio_callback(void *userdata, Uint8 *stream, int len) {
  AudioState *state = (AudioState *)userdata;
  int16_t *buffer = (int16_t *)stream;
  int samples = len / (int)sizeof(int16_t);

  // Maximum 16-bit signed audio magnitude scaled by volume percentage
  double max_amplitude = (32767.0 * (state->volume / 100.0)) * 0.35; // gentle default level
  int period = AUDIO_SAMPLE_RATE / state->frequency;
  if (period < 1) period = 1;

  for (int i = 0; i < samples; i++) {
    if (state->playing && state->volume > 0) {
      double sample_val = 0.0;
      int phase = state->sample_index % period;
      double normalized_phase = (double)phase / (double)period; // 0.0 to 1.0

      switch (state->waveform) {
      case WAVE_SQUARE:
        sample_val = (normalized_phase < 0.5) ? max_amplitude : -max_amplitude;
        break;

      case WAVE_SINE:
        sample_val = sin(normalized_phase * 2.0 * PI) * max_amplitude;
        break;

      case WAVE_TRIANGLE:
        if (normalized_phase < 0.5) {
          sample_val = (-1.0 + 4.0 * normalized_phase) * max_amplitude;
        } else {
          sample_val = (3.0 - 4.0 * normalized_phase) * max_amplitude;
        }
        break;

      case WAVE_SAWTOOTH:
        sample_val = (-1.0 + 2.0 * normalized_phase) * max_amplitude;
        break;
      }

      buffer[i] = (int16_t)sample_val;
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
  printf("General Options:\n");
  printf("  --scale <num>          Pixel scale factor (default: 10, e.g. 15 for 960x480)\n");
  printf("  --clock-speed <hz>     CPU clock speed in Hz (default: 540)\n");
  printf("  --fps <num>            Display/timer refresh rate in Hz (default: 60)\n");
  printf("  --theme <name>         Color theme (classic, matrix, amber, gameboy, cyber)\n");
  printf("\nAudio Options:\n");
  printf("  --volume <0-100>       Audio buzzer volume (default: 50)\n");
  printf("  --pitch <hz>           Buzzer pitch frequency in Hz (default: 440)\n");
  printf("  --wave <type>          Waveform: square, sine, triangle, sawtooth (default: square)\n");
  printf("  --mute                 Disable audio buzzer\n");
  printf("\nHelp:\n");
  printf("  -h, --help             Show this help message\n");
}

static bool parse_theme(const char *name, ColorTheme *out_theme) {
  for (int i = 0; i < THEME_COUNT; i++) {
    if (strcasecmp(name, COLOR_THEMES[i].name) == 0) {
      *out_theme = COLOR_THEMES[i];
      return true;
    }
  }
  return false;
}

static bool parse_waveform(const char *name, WaveformType *out_wave) {
  if (strcasecmp(name, "square") == 0) {
    *out_wave = WAVE_SQUARE;
    return true;
  } else if (strcasecmp(name, "sine") == 0) {
    *out_wave = WAVE_SINE;
    return true;
  } else if (strcasecmp(name, "triangle") == 0) {
    *out_wave = WAVE_TRIANGLE;
    return true;
  } else if (strcasecmp(name, "sawtooth") == 0 || strcasecmp(name, "saw") == 0) {
    *out_wave = WAVE_SAWTOOTH;
    return true;
  }
  return false;
}

static int parse_args(int argc, char **argv, EmulatorConfig *config) {
  config->rom_path = NULL;
  config->scale = 10;
  config->clock_speed = 540;
  config->fps = 60;
  config->mute = false;
  config->volume = 50;
  config->pitch = 440;
  config->waveform = WAVE_SQUARE;
  config->theme = COLOR_THEMES[0]; // default: classic

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      print_usage(argv[0]);
      return 0;
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
    } else if (strcmp(argv[i], "--theme") == 0 && i + 1 < argc) {
      const char *tname = argv[++i];
      if (!parse_theme(tname, &config->theme)) {
        fprintf(stderr, "Error: Unknown theme '%s'. Valid themes: classic, matrix, amber, gameboy, cyber\n", tname);
        return -1;
      }
    } else if (strcmp(argv[i], "--volume") == 0 && i + 1 < argc) {
      config->volume = atoi(argv[++i]);
      if (config->volume < 0 || config->volume > 100) {
        fprintf(stderr, "Error: --volume must be between 0 and 100.\n");
        return -1;
      }
    } else if (strcmp(argv[i], "--pitch") == 0 && i + 1 < argc) {
      config->pitch = atoi(argv[++i]);
      if (config->pitch < 40 || config->pitch > 5000) {
        fprintf(stderr, "Error: --pitch must be between 40 and 5000 Hz.\n");
        return -1;
      }
    } else if (strcmp(argv[i], "--wave") == 0 && i + 1 < argc) {
      const char *wname = argv[++i];
      if (!parse_waveform(wname, &config->waveform)) {
        fprintf(stderr, "Error: Unknown waveform '%s'. Valid waves: square, sine, triangle, sawtooth\n", wname);
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

  return 1;
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
  AudioState audio_state = {
    .playing = false,
    .sample_index = 0,
    .frequency = config.pitch,
    .volume = config.volume,
    .waveform = config.waveform
  };
  SDL_AudioDeviceID audio_device = 0;

  if (!config.mute && config.volume > 0) {
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

  const char *wave_names[] = { "Square", "Sine", "Triangle", "Sawtooth" };
  char window_title[160];
  snprintf(window_title, sizeof(window_title),
           "Byte8 CHIP-8 Emulator [%s | %d Hz, %d FPS | Vol: %d%% %s]",
           config.theme.name, config.clock_speed, config.fps,
           config.mute ? 0 : config.volume, wave_names[config.waveform]);

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

  printf("Running %s at %d Hz, %d FPS, theme='%s', scale=%dx (Window: %dx%d)\n",
         config.rom_path, config.clock_speed, config.fps, config.theme.name, config.scale,
         win_width, win_height);
  printf("Audio: %s (Pitch: %d Hz, Volume: %d%%, Wave: %s)\n",
         config.mute ? "Disabled [Muted]" : "Enabled",
         config.pitch, config.mute ? 0 : config.volume, wave_names[config.waveform]);

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
      SDL_SetRenderDrawColor(renderer,
                             config.theme.bg_r,
                             config.theme.bg_g,
                             config.theme.bg_b,
                             255);
      SDL_RenderClear(renderer);

      SDL_SetRenderDrawColor(renderer,
                             config.theme.fg_r,
                             config.theme.fg_g,
                             config.theme.fg_b,
                             255);
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
