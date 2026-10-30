#include "chip8.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

void chip8_init(Chip8 *chip8) {
  memset(chip8, 0, sizeof(Chip8));
  chip8->pc = 0x200; // CHIP-8 programs start at 0x200

  // Load fontset into memory at 0x050
  memcpy(&chip8->memory[FONTSET_START_ADDRESS], chip8_fontset, sizeof(chip8_fontset));
}

void chip8_load_rom(Chip8 *chip8, const char *filename) {
  printf("LOADING : %s \n", filename);
  FILE *file = fopen(filename, "rb");

  if (!file) {
    fprintf(stderr, "FAILED TO OPEN ROM: %s\n", filename);
    exit(1);
  }

  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  rewind(file);

  printf("ROM SIZE: %ld bytes\n", size);

  if (size > (4096 - 0x200)) {
    fprintf(stderr, "ERROR: ROM too large\n");
    fclose(file);
    exit(1);
  }

  size_t result = fread(&chip8->memory[0x200], 1, size, file);
  if (result != (size_t)size) {
    fprintf(stderr, "READING ERROR\n");
    fclose(file);
    exit(1);
  }

  fclose(file);
  printf("ROM loaded successfully\n");
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

    default:
      printf("[WARN] Unknown 0x0000 opcode: 0x%04X\n", op);
      break;
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
    uint8_t height = n;

    chip8->V[0xF] = 0;

    for (int row = 0; row < height; row++) {
      uint8_t sprite = chip8->memory[chip8->I + row];

      for (int col = 0; col < 8; col++) {
        if (sprite & (0x80 >> col)) {
          int px = (x + col) % VIDEO_WIDTH;
          int py = (y + row) % VIDEO_HEIGHT;
          int index = px + (py * VIDEO_WIDTH);

          if (chip8->gfx[index] == 1) {
            chip8->V[0xF] = 1;
          }
          chip8->gfx[index] ^= 1;
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
