#include "chip8.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>

static const uint8_t chip8_fontset[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

static const uint8_t schip_fontset[100] = {
    0x3C, 0x7E, 0xE7, 0xC3, 0xC3, 0xC3, 0xC3, 0xE7, 0x7E, 0x3C, // 0
    0x18, 0x38, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x7E, // 1
    0x7E, 0xFF, 0x87, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xFF, 0xFF, // 2
    0x7E, 0xFF, 0x87, 0x06, 0x1C, 0x06, 0x03, 0x87, 0xFF, 0x7E, // 3
    0x0C, 0x1C, 0x3C, 0x6C, 0xCC, 0xFE, 0xFF, 0x0C, 0x0C, 0x1E, // 4
    0xFF, 0xFF, 0xC0, 0xC0, 0xFC, 0xFE, 0x07, 0x03, 0xFF, 0xFC, // 5
    0x3E, 0x7C, 0xE0, 0xC0, 0xFC, 0xFE, 0xC7, 0xC3, 0x7E, 0x3C, // 6
    0xFF, 0xFF, 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x60, 0x60, // 7
    0x3C, 0x7E, 0xC3, 0xE7, 0x7E, 0x7E, 0xE7, 0xC3, 0x7E, 0x3C, // 8
    0x3C, 0x7E, 0xC3, 0xE3, 0x7F, 0x3F, 0x03, 0x07, 0x3E, 0x1C  // 9
};

void chip8_init(Chip8 *chip8) {
  memset(chip8, 0, sizeof(Chip8));
  chip8->pc = 0x200; // CHIP-8 programs start at 0x200
  chip8->video_width = VIDEO_WIDTH;
  chip8->video_height = VIDEO_HEIGHT;
  chip8->hires_mode = false;
  chip8->exit_flag = false;

  // Seed the random number generator
  srand((unsigned int)time(NULL));

  // Load standard fontset at 0x050 and SCHIP fontset at 0x0A0
  memcpy(&chip8->memory[FONTSET_START_ADDRESS], chip8_fontset, sizeof(chip8_fontset));
  memcpy(&chip8->memory[SCHIP_FONTSET_START_ADDRESS], schip_fontset, sizeof(schip_fontset));
}

bool chip8_load_rom(Chip8 *chip8, const char *filename) {
  printf("LOADING : %s \n", filename);
  FILE *file = fopen(filename, "rb");

  if (!file) {
    fprintf(stderr, "FAILED TO OPEN ROM: %s\n", filename);
    return false;
  }

  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  rewind(file);

  printf("ROM SIZE: %ld bytes\n", size);

  if (size <= 0 || size > (4096 - 0x200)) {
    fprintf(stderr, "ERROR: Invalid ROM size (%ld bytes)\n", size);
    fclose(file);
    return false;
  }

  size_t result = fread(&chip8->memory[0x200], 1, size, file);
  if (result != (size_t)size) {
    fprintf(stderr, "READING ERROR\n");
    fclose(file);
    return false;
  }

  fclose(file);
  printf("ROM loaded successfully\n");
  return true;
}

#define BYTE8_SAVESTATE_MAGIC 0x38455459 // "BYTE" in little-endian
#define BYTE8_SAVESTATE_VERSION 1

typedef struct {
  uint32_t magic;
  uint32_t version;
  Chip8 state;
} SaveStateHeader;

bool chip8_save_state(const Chip8 *chip8, const char *filename) {
  FILE *f = fopen(filename, "wb");
  if (!f) {
    fprintf(stderr, "Error: Could not open savestate file '%s' for writing.\n", filename);
    return false;
  }

  SaveStateHeader header;
  header.magic = BYTE8_SAVESTATE_MAGIC;
  header.version = BYTE8_SAVESTATE_VERSION;
  memcpy(&header.state, chip8, sizeof(Chip8));

  size_t written = fwrite(&header, sizeof(SaveStateHeader), 1, f);
  fclose(f);

  if (written != 1) {
    fprintf(stderr, "Error: Incomplete write to savestate file '%s'.\n", filename);
    return false;
  }

  return true;
}

bool chip8_load_state(Chip8 *chip8, const char *filename) {
  FILE *f = fopen(filename, "rb");
  if (!f) {
    fprintf(stderr, "Error: Could not open savestate file '%s' for reading.\n", filename);
    return false;
  }

  SaveStateHeader header;
  size_t read = fread(&header, sizeof(SaveStateHeader), 1, f);
  fclose(f);

  if (read != 1) {
    fprintf(stderr, "Error: Incomplete read from savestate file '%s'.\n", filename);
    return false;
  }

  if (header.magic != BYTE8_SAVESTATE_MAGIC) {
    fprintf(stderr, "Error: Invalid savestate magic header in '%s'.\n", filename);
    return false;
  }

  if (header.version != BYTE8_SAVESTATE_VERSION) {
    fprintf(stderr, "Error: Unsupported savestate version %u in '%s'.\n", header.version, filename);
    return false;
  }

  memcpy(chip8, &header.state, sizeof(Chip8));
  chip8->draw_flag = true; // Request screen redraw after state restore
  return true;
}

void chip8_update_timers(Chip8 *chip8) {
  if (chip8->delay_timer > 0) {
    chip8->delay_timer--;
  }
  if (chip8->sound_timer > 0) {
    chip8->sound_timer--;
  }
}

void chip8_cycle(Chip8 *chip8) {
  chip8->cycle_count++;

  /************************************************************
   * 1. FETCH STAGE
   ************************************************************/
  chip8->opcode = (chip8->memory[chip8->pc] << 8) | chip8->memory[chip8->pc + 1];
  chip8->pc += 2;

  uint16_t op  = chip8->opcode;
  uint8_t  vx  = (op & 0x0F00) >> 8;
  uint8_t  vy  = (op & 0x00F0) >> 4;
  uint8_t  n   =  op & 0x000F;
  uint8_t  nn  =  op & 0x00FF;
  uint16_t nnn =  op & 0x0FFF;

  /************************************************************
   * 2. DECODE + EXECUTE
   ************************************************************/
  switch (op & 0xF000) {

  case 0x0000:
    if ((op & 0x00F0) == 0x00C0) {
      // 00CN: Scroll display down by N lines
      int n_lines = op & 0x000F;
      int w = chip8->video_width;
      int h = chip8->video_height;
      for (int y = h - 1; y >= n_lines; y--) {
        memcpy(&chip8->gfx[y * w], &chip8->gfx[(y - n_lines) * w], w);
      }
      for (int y = 0; y < n_lines && y < h; y++) {
        memset(&chip8->gfx[y * w], 0, w);
      }
      chip8->draw_flag = true;
    } else {
      switch (nn) {
      // 00E0 → Clear screen
      case 0x00E0:
        memset(chip8->gfx, 0, sizeof(chip8->gfx));
        chip8->draw_flag = true;
        break;

      // 00EE → Return from subroutine
      case 0x00EE:
        if (chip8->sp > 0) {
          chip8->sp--;
          chip8->pc = chip8->stack[chip8->sp];
        } else {
          fprintf(stderr, "Stack underflow!\n");
        }
        break;

      // 00FB → Scroll right 4 pixels
      case 0x00FB: {
        int w = chip8->video_width;
        int h = chip8->video_height;
        for (int y = 0; y < h; y++) {
          for (int x = w - 1; x >= 4; x--) {
            chip8->gfx[y * w + x] = chip8->gfx[y * w + (x - 4)];
          }
          for (int x = 0; x < 4 && x < w; x++) {
            chip8->gfx[y * w + x] = 0;
          }
        }
        chip8->draw_flag = true;
      } break;

      // 00FC → Scroll left 4 pixels
      case 0x00FC: {
        int w = chip8->video_width;
        int h = chip8->video_height;
        for (int y = 0; y < h; y++) {
          for (int x = 0; x < w - 4; x++) {
            chip8->gfx[y * w + x] = chip8->gfx[y * w + (x + 4)];
          }
          for (int x = w - 4; x < w; x++) {
            if (x >= 0) chip8->gfx[y * w + x] = 0;
          }
        }
        chip8->draw_flag = true;
      } break;

      // 00FD → Exit interpreter
      case 0x00FD:
        chip8->exit_flag = true;
        break;

      // 00FE → Disable extended screen mode (64x32)
      case 0x00FE:
        chip8->hires_mode = false;
        chip8->video_width = VIDEO_WIDTH;
        chip8->video_height = VIDEO_HEIGHT;
        memset(chip8->gfx, 0, sizeof(chip8->gfx));
        chip8->draw_flag = true;
        break;

      // 00FF → Enable extended screen mode (128x64)
      case 0x00FF:
        chip8->hires_mode = true;
        chip8->video_width = SCHIP_VIDEO_WIDTH;
        chip8->video_height = SCHIP_VIDEO_HEIGHT;
        memset(chip8->gfx, 0, sizeof(chip8->gfx));
        chip8->draw_flag = true;
        break;

      default:
        // 0NNN → Call RCA 1802 machine language routine at NNN (legacy no-op in modern interpreters)
        printf("[INFO] Legacy machine routine 0x%04X ignored\n", op);
        break;
      }
    }
    break;

  // 1NNN → Jump to address NNN
  case 0x1000:
    chip8->pc = nnn;
    break;

  // 2NNN → Call subroutine at NNN
  case 0x2000:
    if (chip8->sp < STACK_SIZE) {
      chip8->stack[chip8->sp] = chip8->pc;
      chip8->sp++;
      chip8->pc = nnn;
    } else {
      fprintf(stderr, "Stack overflow!\n");
    }
    break;

  // 3XNN → Skip next instruction if VX == NN
  case 0x3000:
    if (chip8->V[vx] == nn) {
      chip8->pc += 2;
    }
    break;

  // 4XNN → Skip next instruction if VX != NN
  case 0x4000:
    if (chip8->V[vx] != nn) {
      chip8->pc += 2;
    }
    break;

  // 5XY0 → Skip next instruction if VX == VY
  case 0x5000:
    if (n == 0 && chip8->V[vx] == chip8->V[vy]) {
      chip8->pc += 2;
    }
    break;

  // 6XNN → Set VX = NN
  case 0x6000:
    chip8->V[vx] = nn;
    break;

  // 7XNN → VX += NN (no carry flag)
  case 0x7000:
    chip8->V[vx] += nn;
    break;

  // 8XY* → Arithmetic and Logic
  case 0x8000:
    switch (n) {
    // 8XY0 → VX = VY
    case 0x0:
      chip8->V[vx] = chip8->V[vy];
      break;

    // 8XY1 → VX |= VY
    case 0x1:
      chip8->V[vx] |= chip8->V[vy];
      break;

    // 8XY2 → VX &= VY
    case 0x2:
      chip8->V[vx] &= chip8->V[vy];
      break;

    // 8XY3 → VX ^= VY
    case 0x3:
      chip8->V[vx] ^= chip8->V[vy];
      break;

    // 8XY4 → VX += VY, VF = carry
    case 0x4: {
      uint16_t sum = (uint16_t)chip8->V[vx] + (uint16_t)chip8->V[vy];
      chip8->V[0xF] = (sum > 0xFF) ? 1 : 0;
      chip8->V[vx] = (uint8_t)(sum & 0xFF);
    } break;

    // 8XY5 → VX -= VY, VF = NOT borrow
    case 0x5: {
      uint8_t not_borrow = (chip8->V[vx] >= chip8->V[vy]) ? 1 : 0;
      chip8->V[vx] -= chip8->V[vy];
      chip8->V[0xF] = not_borrow;
    } break;

    // 8XY6 → VX >>= 1, VF = dropped bit
    case 0x6: {
      uint8_t lsb = chip8->V[vx] & 0x01;
      chip8->V[vx] >>= 1;
      chip8->V[0xF] = lsb;
    } break;

    // 8XY7 → VX = VY - VX, VF = NOT borrow
    case 0x7: {
      uint8_t not_borrow = (chip8->V[vy] >= chip8->V[vx]) ? 1 : 0;
      chip8->V[vx] = chip8->V[vy] - chip8->V[vx];
      chip8->V[0xF] = not_borrow;
    } break;

    // 8XYE → VX <<= 1, VF = MSB before shift
    case 0xE: {
      uint8_t msb = (chip8->V[vx] & 0x80) >> 7;
      chip8->V[vx] <<= 1;
      chip8->V[0xF] = msb;
    } break;

    default:
      printf("[WARN] Unknown 0x8000 opcode: 0x%04X\n", op);
      break;
    }
    break;

  // 9XY0 → Skip next instruction if VX != VY
  case 0x9000:
    if (n == 0 && chip8->V[vx] != chip8->V[vy]) {
      chip8->pc += 2;
    }
    break;

  // ANNN → Set I register to NNN
  case 0xA000:
    chip8->I = nnn;
    break;

  // BNNN → Jump to address NNN + V0
  case 0xB000:
    chip8->pc = nnn + chip8->V[0];
    break;

  // CXNN → VX = rand() & NN
  case 0xC000:
    chip8->V[vx] = (rand() % 256) & nn;
    break;

  // DXYN → Draw sprite
  case 0xD000: {
    uint8_t x = chip8->V[vx];
    uint8_t y = chip8->V[vy];
    chip8->V[0xF] = 0;
    int w = chip8->video_width;
    int h = chip8->video_height;

    if (n == 0) {
      // 16x16 sprite in SCHIP mode: 16 rows of 2 bytes each (32 bytes)
      for (int row = 0; row < 16; row++) {
        uint16_t sprite_row = (chip8->memory[chip8->I + (row * 2)] << 8) |
                               chip8->memory[chip8->I + (row * 2) + 1];
        int py = (y + row) % h;
        for (int col = 0; col < 16; col++) {
          if (sprite_row & (0x8000 >> col)) {
            int px = (x + col) % w;
            int index = px + (py * w);
            if (chip8->gfx[index] == 1) {
              chip8->V[0xF] = 1;
            }
            chip8->gfx[index] ^= 1;
          }
        }
      }
    } else {
      // Standard 8xN sprite
      for (int row = 0; row < n; row++) {
        uint8_t sprite = chip8->memory[chip8->I + row];
        int py = (y + row) % h;
        for (int col = 0; col < 8; col++) {
          if (sprite & (0x80 >> col)) {
            int px = (x + col) % w;
            int index = px + (py * w);
            if (chip8->gfx[index] == 1) {
              chip8->V[0xF] = 1;
            }
            chip8->gfx[index] ^= 1;
          }
        }
      }
    }
    chip8->draw_flag = true;
  } break;

  // EX9E & EXA1 → Keyboard skips
  case 0xE000:
    switch (nn) {
    // EX9E: Skip if key in VX is pressed
    case 0x009E:
      if (chip8->V[vx] < 16 && chip8->keypad[chip8->V[vx]]) {
        chip8->pc += 2;
      }
      break;

    // EXA1: Skip if key in VX is NOT pressed
    case 0x00A1:
      if (chip8->V[vx] < 16 && !chip8->keypad[chip8->V[vx]]) {
        chip8->pc += 2;
      }
      break;

    default:
      printf("[WARN] Unknown 0xE000 opcode: 0x%04X\n", op);
      break;
    }
    break;

  // FX** family
  case 0xF000:
    switch (nn) {
    // FX07: Set VX = delay_timer
    case 0x0007:
      chip8->V[vx] = chip8->delay_timer;
      break;

    // FX0A: Wait for key press and store key in VX
    case 0x000A: {
      bool key_pressed = false;
      for (int k = 0; k < 16; k++) {
        if (chip8->keypad[k]) {
          chip8->V[vx] = (uint8_t)k;
          key_pressed = true;
          break;
        }
      }
      // If no key pressed, repeat this instruction next cycle
      if (!key_pressed) {
        chip8->pc -= 2;
      }
    } break;

    // FX15: Set delay_timer = VX
    case 0x0015:
      chip8->delay_timer = chip8->V[vx];
      break;

    // FX18: Set sound_timer = VX
    case 0x0018:
      chip8->sound_timer = chip8->V[vx];
      break;

    // FX1E: I += VX
    case 0x001E:
      chip8->I += chip8->V[vx];
      break;

    // FX29: Set I = address of font character for VX
    case 0x0029:
      chip8->I = FONTSET_START_ADDRESS + ((chip8->V[vx] & 0x0F) * 5);
      break;

    // FX30: Set I = address of 10-byte font character for VX (SCHIP)
    case 0x0030:
      chip8->I = SCHIP_FONTSET_START_ADDRESS + ((chip8->V[vx] & 0x0F) * 10);
      break;

    // FX33: Store BCD representation of VX at I, I+1, I+2
    case 0x0033: {
      uint8_t val = chip8->V[vx];
      chip8->memory[chip8->I]     = val / 100;
      chip8->memory[chip8->I + 1] = (val / 10) % 10;
      chip8->memory[chip8->I + 2] = val % 10;
    } break;

    // FX55: Dump registers V0 through VX into memory starting at I
    case 0x0055:
      for (int i = 0; i <= vx; i++) {
        chip8->memory[chip8->I + i] = chip8->V[i];
      }
      break;

    // FX65: Load registers V0 through VX from memory starting at I
    case 0x0065:
      for (int i = 0; i <= vx; i++) {
        chip8->V[i] = chip8->memory[chip8->I + i];
      }
      break;

    // FX75: Save V0 through VX into RPL user flags (SCHIP)
    case 0x0075:
      for (int i = 0; i <= vx && i < 16; i++) {
        chip8->rpl_flags[i] = chip8->V[i];
      }
      break;

    // FX85: Load V0 through VX from RPL user flags (SCHIP)
    case 0x0085:
      for (int i = 0; i <= vx && i < 16; i++) {
        chip8->V[i] = chip8->rpl_flags[i];
      }
      break;

    default:
      printf("[WARN] Unknown 0xF000 opcode: 0x%04X\n", op);
      break;
    }
    break;

  default:
    printf("[ERROR] Unknown opcode: 0x%04X\n", op);
    break;
  }
}

void chip8_disasm_opcode(uint16_t op, char *out_buf, size_t buf_size) {
  if (!out_buf || buf_size == 0) return;

  uint8_t n1 = (op & 0xF000) >> 12;
  uint8_t x  = (op & 0x0F00) >> 8;
  uint8_t y  = (op & 0x00F0) >> 4;
  uint8_t n  =  op & 0x000F;
  uint8_t kk =  op & 0x00FF;
  uint16_t nnn = op & 0x0FFF;

  switch (n1) {
  case 0x0:
    if ((op & 0x00F0) == 0x00C0) snprintf(out_buf, buf_size, "scroll_down %d", op & 0x000F);
    else if (op == 0x00E0) snprintf(out_buf, buf_size, "cls");
    else if (op == 0x00EE) snprintf(out_buf, buf_size, "ret");
    else if (op == 0x00FB) snprintf(out_buf, buf_size, "scroll_right");
    else if (op == 0x00FC) snprintf(out_buf, buf_size, "scroll_left");
    else if (op == 0x00FD) snprintf(out_buf, buf_size, "exit");
    else if (op == 0x00FE) snprintf(out_buf, buf_size, "low");
    else if (op == 0x00FF) snprintf(out_buf, buf_size, "high");
    else snprintf(out_buf, buf_size, "sys 0x%03X", nnn);
    break;
  case 0x1: snprintf(out_buf, buf_size, "jump 0x%03X", nnn); break;
  case 0x2: snprintf(out_buf, buf_size, "call 0x%03X", nnn); break;
  case 0x3: snprintf(out_buf, buf_size, "if v%X == 0x%02X", x, kk); break;
  case 0x4: snprintf(out_buf, buf_size, "if v%X != 0x%02X", x, kk); break;
  case 0x5:
    if (n == 0) snprintf(out_buf, buf_size, "if v%X == v%X", x, y);
    else snprintf(out_buf, buf_size, "db 0x%02X, 0x%02X", (op >> 8) & 0xFF, op & 0xFF);
    break;
  case 0x6: snprintf(out_buf, buf_size, "v%X = 0x%02X", x, kk); break;
  case 0x7: snprintf(out_buf, buf_size, "v%X += 0x%02X", x, kk); break;
  case 0x8:
    switch (n) {
    case 0x0: snprintf(out_buf, buf_size, "v%X = v%X", x, y); break;
    case 0x1: snprintf(out_buf, buf_size, "v%X |= v%X", x, y); break;
    case 0x2: snprintf(out_buf, buf_size, "v%X &= v%X", x, y); break;
    case 0x3: snprintf(out_buf, buf_size, "v%X ^= v%X", x, y); break;
    case 0x4: snprintf(out_buf, buf_size, "v%X += v%X", x, y); break;
    case 0x5: snprintf(out_buf, buf_size, "v%X -= v%X", x, y); break;
    case 0x6: snprintf(out_buf, buf_size, "v%X >>= 1", x); break;
    case 0x7: snprintf(out_buf, buf_size, "v%X = v%X - v%X", x, y, x); break;
    case 0xE: snprintf(out_buf, buf_size, "v%X <<= 1", x); break;
    default:  snprintf(out_buf, buf_size, "db 0x%02X, 0x%02X", (op >> 8) & 0xFF, op & 0xFF); break;
    }
    break;
  case 0x9:
    if (n == 0) snprintf(out_buf, buf_size, "if v%X != v%X", x, y);
    else snprintf(out_buf, buf_size, "db 0x%02X, 0x%02X", (op >> 8) & 0xFF, op & 0xFF);
    break;
  case 0xA: snprintf(out_buf, buf_size, "i = 0x%03X", nnn); break;
  case 0xB: snprintf(out_buf, buf_size, "jump v0 + 0x%03X", nnn); break;
  case 0xC: snprintf(out_buf, buf_size, "v%X = rand 0x%02X", x, kk); break;
  case 0xD: snprintf(out_buf, buf_size, "draw v%X, v%X, %d", x, y, n); break;
  case 0xE:
    if (kk == 0x9E) snprintf(out_buf, buf_size, "skip_key v%X", x);
    else if (kk == 0xA1) snprintf(out_buf, buf_size, "skip_nkey v%X", x);
    else snprintf(out_buf, buf_size, "db 0x%02X, 0x%02X", (op >> 8) & 0xFF, op & 0xFF);
    break;
  case 0xF:
    switch (kk) {
    case 0x07: snprintf(out_buf, buf_size, "v%X = delay", x); break;
    case 0x0A: snprintf(out_buf, buf_size, "v%X = key", x); break;
    case 0x15: snprintf(out_buf, buf_size, "delay = v%X", x); break;
    case 0x18: snprintf(out_buf, buf_size, "sound = v%X", x); break;
    case 0x1E: snprintf(out_buf, buf_size, "i += v%X", x); break;
    case 0x29: snprintf(out_buf, buf_size, "i = font v%X", x); break;
    case 0x30: snprintf(out_buf, buf_size, "font16 v%X", x); break;
    case 0x33: snprintf(out_buf, buf_size, "bcd v%X", x); break;
    case 0x55: snprintf(out_buf, buf_size, "save v%X", x); break;
    case 0x65: snprintf(out_buf, buf_size, "load v%X", x); break;
    case 0x75: snprintf(out_buf, buf_size, "save_flags v%X", x); break;
    case 0x85: snprintf(out_buf, buf_size, "load_flags v%X", x); break;
    default:   snprintf(out_buf, buf_size, "db 0x%02X, 0x%02X", (op >> 8) & 0xFF, op & 0xFF); break;
    }
    break;
  default:
    snprintf(out_buf, buf_size, "db 0x%02X, 0x%02X", (op >> 8) & 0xFF, op & 0xFF);
    break;
  }
}

void chip8_dump_debug(const Chip8 *chip8) {
  if (!chip8) return;

  uint16_t current_op = 0;
  if (chip8->pc < 4095) {
    current_op = (chip8->memory[chip8->pc] << 8) | chip8->memory[chip8->pc + 1];
  }
  char disasm[64];
  chip8_disasm_opcode(current_op, disasm, sizeof(disasm));

  printf("\n=== [ BYTE8 DEBUGGER ] ====================================\n");
  printf("  PC: 0x%04X | Opcode: 0x%04X (%s)\n", chip8->pc, current_op, disasm);
  printf("  I:  0x%04X | SP: %d | DT: %02d | ST: %02d\n", chip8->I, chip8->sp, chip8->delay_timer, chip8->sound_timer);
  printf("  V0: %02X  V1: %02X  V2: %02X  V3: %02X  V4: %02X  V5: %02X  V6: %02X  V7: %02X\n",
         chip8->V[0], chip8->V[1], chip8->V[2], chip8->V[3],
         chip8->V[4], chip8->V[5], chip8->V[6], chip8->V[7]);
  printf("  V8: %02X  V9: %02X  VA: %02X  VB: %02X  VC: %02X  VD: %02X  VE: %02X  VF: %02X\n",
         chip8->V[8], chip8->V[9], chip8->V[10], chip8->V[11],
         chip8->V[12], chip8->V[13], chip8->V[14], chip8->V[15]);

  if (chip8->sp > 0) {
    printf("  Stack [top=%d]: ", chip8->sp);
    for (int s = 0; s < chip8->sp && s < STACK_SIZE; s++) {
      printf("0x%03X ", chip8->stack[s]);
    }
    printf("\n");
  }

  printf("  Upcoming Instructions:\n");
  for (int offset = 2; offset <= 6; offset += 2) {
    uint16_t next_pc = chip8->pc + offset;
    if (next_pc < 4095) {
      uint16_t next_op = (chip8->memory[next_pc] << 8) | chip8->memory[next_pc + 1];
      char next_disasm[64];
      chip8_disasm_opcode(next_op, next_disasm, sizeof(next_disasm));
      printf("    0x%04X: 0x%04X  %s\n", next_pc, next_op, next_disasm);
    }
  }
  printf("===========================================================\n");
  printf("Commands: [s/Enter] Step | [c] Continue | [r] Regs | [m <addr> [n]] Mem | [q] Quit\n");
}

void chip8_dump_memory(const Chip8 *chip8, uint16_t addr, size_t count) {
  if (!chip8) return;
  if (addr >= 4096) {
    printf("Error: Address 0x%04X out of range (0x0000 - 0x0FFF)\n", addr);
    return;
  }
  if (count == 0) count = 16;
  if (addr + count > 4096) count = 4096 - addr;

  printf("\n--- Memory Dump from 0x%04X (%zu bytes) ---\n", addr, count);
  for (size_t i = 0; i < count; i += 16) {
    uint16_t row_addr = addr + i;
    printf("  0x%04X: ", row_addr);
    size_t row_len = (count - i < 16) ? (count - i) : 16;
    for (size_t j = 0; j < 16; j++) {
      if (j < row_len) {
        printf("%02X ", chip8->memory[row_addr + j]);
      } else {
        printf("   ");
      }
      if (j == 7) printf(" ");
    }
    printf(" |");
    for (size_t j = 0; j < row_len; j++) {
      uint8_t c = chip8->memory[row_addr + j];
      printf("%c", (c >= 32 && c <= 126) ? c : '.');
    }
    printf("|\n");
  }
  printf("-------------------------------------------\n");
}

bool chip8_add_breakpoint(Chip8 *chip8, uint16_t addr) {
  if (!chip8) return false;
  for (int i = 0; i < chip8->breakpoint_count; i++) {
    if (chip8->breakpoints[i].is_active && chip8->breakpoints[i].address == addr) {
      return true; // already present
    }
  }
  if (chip8->breakpoint_count < MAX_BREAKPOINTS) {
    chip8->breakpoints[chip8->breakpoint_count].address = addr;
    chip8->breakpoints[chip8->breakpoint_count].is_active = true;
    chip8->breakpoint_count++;
    return true;
  }
  return false;
}

bool chip8_remove_breakpoint(Chip8 *chip8, uint16_t addr) {
  if (!chip8) return false;
  for (int i = 0; i < chip8->breakpoint_count; i++) {
    if (chip8->breakpoints[i].is_active && chip8->breakpoints[i].address == addr) {
      chip8->breakpoints[i].is_active = false;
      return true;
    }
  }
  return false;
}

void chip8_list_breakpoints(const Chip8 *chip8) {
  if (!chip8) return;
  printf("\n--- Active Breakpoints ---\n");
  int count = 0;
  for (int i = 0; i < chip8->breakpoint_count; i++) {
    if (chip8->breakpoints[i].is_active) {
      printf("  #%d: Breakpoint at 0x%04X\n", count + 1, chip8->breakpoints[i].address);
      count++;
    }
  }
  if (count == 0) printf("  (No active breakpoints)\n");

  printf("--- Active Watchpoints ---\n");
  int wcount = 0;
  for (int i = 0; i < chip8->watchpoint_count; i++) {
    if (chip8->watchpoints[i].is_active) {
      if (chip8->watchpoints[i].is_reg) {
        printf("  #%d: Watch Register V%X (current=0x%02X)\n",
               wcount + 1, chip8->watchpoints[i].reg_index, chip8->V[chip8->watchpoints[i].reg_index]);
      } else {
        printf("  #%d: Watch Memory [0x%03X] (current=0x%02X)\n",
               wcount + 1, chip8->watchpoints[i].mem_addr, chip8->memory[chip8->watchpoints[i].mem_addr]);
      }
      wcount++;
    }
  }
  if (wcount == 0) printf("  (No active watchpoints)\n");
  printf("--------------------------\n");
}

bool chip8_check_breakpoint(Chip8 *chip8) {
  if (!chip8) return false;
  for (int i = 0; i < chip8->breakpoint_count; i++) {
    if (chip8->breakpoints[i].is_active && chip8->breakpoints[i].address == chip8->pc) {
      return true;
    }
  }
  return false;
}

bool chip8_add_watchpoint_reg(Chip8 *chip8, uint8_t reg_idx) {
  if (!chip8 || reg_idx > 15) return false;
  if (chip8->watchpoint_count < MAX_WATCHPOINTS) {
    Watchpoint *wp = &chip8->watchpoints[chip8->watchpoint_count++];
    wp->is_reg = true;
    wp->reg_index = reg_idx;
    wp->last_value = chip8->V[reg_idx];
    wp->is_active = true;
    return true;
  }
  return false;
}

bool chip8_add_watchpoint_mem(Chip8 *chip8, uint16_t mem_addr) {
  if (!chip8 || mem_addr >= 4096) return false;
  if (chip8->watchpoint_count < MAX_WATCHPOINTS) {
    Watchpoint *wp = &chip8->watchpoints[chip8->watchpoint_count++];
    wp->is_reg = false;
    wp->mem_addr = mem_addr;
    wp->last_value = chip8->memory[mem_addr];
    wp->is_active = true;
    return true;
  }
  return false;
}

bool chip8_check_watchpoints(Chip8 *chip8) {
  if (!chip8) return false;
  for (int i = 0; i < chip8->watchpoint_count; i++) {
    Watchpoint *wp = &chip8->watchpoints[i];
    if (!wp->is_active) continue;

    if (wp->is_reg) {
      uint8_t curr = chip8->V[wp->reg_index];
      if (curr != wp->last_value) {
        snprintf(chip8->watchpoint_hit_desc, sizeof(chip8->watchpoint_hit_desc),
                 "Register V%X changed: 0x%02X -> 0x%02X",
                 wp->reg_index, wp->last_value, curr);
        wp->last_value = curr;
        return true;
      }
    } else {
      uint8_t curr = chip8->memory[wp->mem_addr];
      if (curr != wp->last_value) {
        snprintf(chip8->watchpoint_hit_desc, sizeof(chip8->watchpoint_hit_desc),
                 "Memory [0x%03X] changed: 0x%02X -> 0x%02X",
                 wp->mem_addr, wp->last_value, curr);
        wp->last_value = curr;
        return true;
      }
    }
  }
  return false;
}
