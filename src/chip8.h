#ifndef CHIP8_H
#define CHIP8_H

#include <stdint.h>
#include <stdbool.h>

#define VIDEO_WIDTH 64
#define VIDEO_HEIGHT 32

typedef struct {

    uint8_t memory[4096];

    uint8_t V[16];

    uint16_t I;
    uint16_t pc;

    uint16_t opcode;

    uint16_t stack[16];
    uint16_t sp;

    uint8_t delay_timer;
    uint8_t sound_timer;

    uint8_t gfx[VIDEO_WIDTH * VIDEO_HEIGHT];

    uint8_t keypad[16];

    bool draw_flag;

} Chip8; chip8;

void chip8_init(Chip8 *chip8);
void chip8_load_rom(Chip8 *chip8, const char *filename);
void chip8_cycle(Chip8 *chip8);

#endif
