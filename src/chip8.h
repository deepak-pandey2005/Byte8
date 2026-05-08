#indef CHIP8_H
#define CHIP8_H

struct Chip{
    unsigned short opcode;
    unsigned char memory[4096];
    unsigned char v[16]; // the chip 8 has 15 genral purpose register named from v0 to vE. 
                         // the 16 register is used for carry flag

    unsigned short I;
    unsigned short pc;
    /*
    0x000-0x1FF - Chip 8 interpreter (contains font set in emu)
    0x050-0x0A0 - Used for the built in 4x5 pixel font set (0-F)
    0x200-0xFFF - Program ROM and work RAM
    */
    unsigned char gfx[64 * 32]; // full name graphics
                                // total pixels = 2048

    unsigned char delay_timer;
    unsigned char sound_timer;

    unsigned short stack[16]; // 16 levels of stack 
    unsigned short sp; // stack pointer to point to stack

    unsigned char keypad[16]; // because the chip8 can only go from 0x0 to 0xf so we get only 16 keypad

} Chip8;