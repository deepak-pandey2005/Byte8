#include "chip8.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void chip8_init(Chip8 *chip8) {
  memset(chip8, 0, sizeof(Chip8)); // this sets the entire memory to 0
  /* we can use memset to reset the memory to 0 or

     for(int i = 0; i < 2048; ++i)
                     gfx[i] = 0;

     we can use like this for all other variables in the struct which we are
     using as a emulator information*/

  chip8->pc = 0x200; // 0x200-0xFFF - Program ROM and work RAM
}

void chip8_load_rom(Chip8 *chip8, const char *filename) {
  printf("LOADING : %s \n", filename);
  FILE *file = fopen(filename, "rb"); // opens a file

  if (!file) {
    printf("FAILED TO OPEN ROM \n");
    exit(1);
  }
  fseek(file, 0, SEEK_END); // move cursor to end of file
  long size = ftell(file);  // get the file size
  rewind(file);             // now it returns to the beggining

  printf("ROM SIZE: %ld bytes\n", size);

  // CHIP-8 memory limit check (4096 total - 512 reserved)
  if (size > (4096 - 512)) {
    printf("ERROR: ROM too large\n");
    fclose(file);
    exit(1);
  }

  // Read directly into CHIP-8 memory at 0x200
  size_t result = fread(&chip8->memory[0x200], 1, size, file);

  if (result != (size_t)size) {
    printf("READING ERROR\n");
    fclose(file);
    exit(1);
  }

  fclose(file);

  printf("ROM loaded successfully\n");
}

void chip8_cycle(Chip8 *chip8) {

  /************************************************************
   * 1. FETCH STAGE
   * ----------------------------------------------------------
   * Each CHIP-8 instruction is 2 bytes (16 bits).
   * We combine two consecutive memory bytes into one opcode.
   ************************************************************/
  chip8->opcode =
      (chip8->memory[chip8->pc] << 8) | chip8->memory[chip8->pc + 1];

  // Move program counter to next instruction
  chip8->pc += 2;

  /************************************************************
   * 2. DECODE + EXECUTE
   * ----------------------------------------------------------
   * We decode using the highest nibble (first 4 bits)
   ************************************************************/
  switch (chip8->opcode & 0xF000) {

  /************************************************************
   * 0x0000 family
   ************************************************************/
  case 0x0000: {

    switch (chip8->opcode & 0x00FF) {

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
        printf("Stack underflow!\n");
      }
      break;

    default:
      printf("[WARN] Unknown 0x0000 opcode: 0x%X\n", chip8->opcode);
      break;
    }
  } break;

  /************************************************************
   * 1NNN → Jump to address NNN
   ************************************************************/
  case 0x1000:
    chip8->pc = chip8->opcode & 0x0FFF;
    break;

  /************************************************************
   * 2NNN → Call subroutine at NNN
   ************************************************************/
  case 0x2000:
    if (chip8->sp < STACK_SIZE) {
      chip8->stack[chip8->sp] = chip8->pc;
      chip8->sp++;
      chip8->pc = chip8->opcode & 0x0FFF;
    } else {
      printf("Stack overflow!\n");
    }
    break;

  /************************************************************
   * 3XNN → Skip next instruction if VX == NN
   ************************************************************/
  case 0x3000: {
    uint8_t vx = (chip8->opcode & 0x0F00) >> 8;
    uint8_t nn = chip8->opcode & 0x00FF;

    if (chip8->V[vx] == nn)
      chip8->pc += 2;
  } break;

  /************************************************************
   * 4XNN → Skip next instruction if VX != NN
   ************************************************************/
  case 0x4000: {
    uint8_t vx = (chip8->opcode & 0x0F00) >> 8;
    uint8_t nn = chip8->opcode & 0x00FF;

    if (chip8->V[vx] != nn)
      chip8->pc += 2;
  } break;

  /************************************************************
   * 6XNN → Set VX = NN
   ************************************************************/
  case 0x6000: {
    uint8_t vx = (chip8->opcode & 0x0F00) >> 8;
    chip8->V[vx] = chip8->opcode & 0x00FF;
  } break;

  /************************************************************
   * 7XNN → VX += NN (no carry flag)
   ************************************************************/
  case 0x7000: {
    uint8_t vx = (chip8->opcode & 0x0F00) >> 8;
    chip8->V[vx] += chip8->opcode & 0x00FF;
  } break;

  /************************************************************
   * 8XY* → Arithmetic + logic operations
   ************************************************************/
  case 0x8000: {

    uint8_t vx = (chip8->opcode & 0x0F00) >> 8;
    uint8_t vy = (chip8->opcode & 0x00F0) >> 4;

    switch (chip8->opcode & 0x000F) {

    // 8XY0 → VX = VY
    case 0x0000:
      chip8->V[vx] = chip8->V[vy];
      break;

    // 8XY4 → VX += VY, set VF = carry
    case 0x0004:
      chip8->V[0xF] = (chip8->V[vy] > (0xFF - chip8->V[vx])) ? 1 : 0;

      chip8->V[vx] += chip8->V[vy];
      break;

    // 8XY1 → VX |= VY
    case 0x0001:
      chip8->V[vx] |= chip8->V[vy];
      break;

    // 8XY2 → VX &= VY
    case 0x0002:
      chip8->V[vx] &= chip8->V[vy];
      break;

    // 8XY3 → VX ^= VY
    case 0x0003:
      chip8->V[vx] ^= chip8->V[vy];
      break;

    default:
      printf("[WARN] Unknown 0x8000 opcode: 0x%X\n", chip8->opcode);
      break;
    }
  } break;

  /************************************************************
   * ANNN → Set I register
   ************************************************************/
  case 0xA000:
    chip8->I = chip8->opcode & 0x0FFF;
    break;

  /************************************************************
   * DXYN → Draw sprite
   ************************************************************/
  case 0xD000: {

    uint8_t x = chip8->V[(chip8->opcode & 0x0F00) >> 8];
    uint8_t y = chip8->V[(chip8->opcode & 0x00F0) >> 4];
    uint8_t height = chip8->opcode & 0x000F;

    chip8->V[0xF] = 0;

    for (int row = 0; row < height; row++) {

      uint8_t sprite = chip8->memory[chip8->I + row];

      for (int col = 0; col < 8; col++) {

        if (sprite & (0x80 >> col)) {

          int px = (x + col) % VIDEO_WIDTH;
          int py = (y + row) % VIDEO_HEIGHT;

          int index = px + (py * VIDEO_WIDTH);

          // Collision detection
          if (chip8->gfx[index] == 1)
            chip8->V[0xF] = 1;

          chip8->gfx[index] ^= 1;
        }
      }
    }

    chip8->draw_flag = true;

  } break;

  /************************************************************
   * FX33 → BCD conversion
   ************************************************************/
  case 0xF000: {

    switch (chip8->opcode & 0x00FF) {

    case 0x0033: {
      uint8_t vx = (chip8->opcode & 0x0F00) >> 8;
      uint8_t value = chip8->V[vx];

      chip8->memory[chip8->I] = value / 100;
      chip8->memory[chip8->I + 1] = (value / 10) % 10;
      chip8->memory[chip8->I + 2] = value % 10;
    } break;

    default:
      printf("[WARN] Unknown 0xF000 opcode: 0x%X\n", chip8->opcode);
      break;
    }

  } break;

  /************************************************************
   * Unknown opcode fallback
   ************************************************************/
  default:
    printf("[ERROR] Unknown opcode: 0x%X\n", chip8->opcode);
    break;
  }
}
