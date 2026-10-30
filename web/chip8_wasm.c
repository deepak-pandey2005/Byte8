#include "../src/chip8.h"

#define WASM_EXPORT __attribute__((visibility("default")))

static Chip8 chip8_instance;
static unsigned int lcg_seed = 123456789;

static int wasm_rand(void) {
    lcg_seed = (1103515245U * lcg_seed + 12345U) & 0x7fffffffU;
    return (int)(lcg_seed >> 16);
}

// Standalone memory helpers without external libc
static void *wasm_memset(void *dest, int c, unsigned long n) {
    unsigned char *p = (unsigned char *)dest;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return dest;
}

static void *wasm_memcpy(void *dest, const void *src, unsigned long n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

static const uint8_t wasm_fontset[80] = {
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

WASM_EXPORT void chip8_wasm_init(void) {
    wasm_memset(&chip8_instance, 0, sizeof(Chip8));
    chip8_instance.pc = 0x200;
    wasm_memcpy(&chip8_instance.memory[FONTSET_START_ADDRESS], wasm_fontset, sizeof(wasm_fontset));
}

WASM_EXPORT uint8_t *chip8_wasm_get_rom_buffer(void) {
    return &chip8_instance.memory[0x200];
}

WASM_EXPORT uint8_t *chip8_wasm_get_gfx_buffer(void) {
    return chip8_instance.gfx;
}

WASM_EXPORT void chip8_wasm_set_key(int key, int state) {
    if (key >= 0 && key < 16) {
        chip8_instance.keypad[key] = state ? 1 : 0;
    }
}

WASM_EXPORT int chip8_wasm_get_sound_timer(void) {
    return chip8_instance.sound_timer;
}

WASM_EXPORT int chip8_wasm_get_draw_flag(void) {
    return chip8_instance.draw_flag ? 1 : 0;
}

WASM_EXPORT void chip8_wasm_clear_draw_flag(void) {
    chip8_instance.draw_flag = false;
}

WASM_EXPORT void chip8_wasm_update_timers(void) {
    if (chip8_instance.delay_timer > 0) chip8_instance.delay_timer--;
    if (chip8_instance.sound_timer > 0) chip8_instance.sound_timer--;
}

WASM_EXPORT void chip8_wasm_cycle(void) {
    Chip8 *chip8 = &chip8_instance;
    chip8->opcode = (chip8->memory[chip8->pc] << 8) | chip8->memory[chip8->pc + 1];
    chip8->pc += 2;

    uint16_t op  = chip8->opcode;
    uint8_t  vx  = (op & 0x0F00) >> 8;
    uint8_t  vy  = (op & 0x00F0) >> 4;
    uint8_t  n   =  op & 0x000F;
    uint8_t  nn  =  op & 0x00FF;
    uint16_t nnn =  op & 0x0FFF;

    switch (op & 0xF000) {
    case 0x0000:
        switch (nn) {
        case 0x00E0:
            wasm_memset(chip8->gfx, 0, sizeof(chip8->gfx));
            chip8->draw_flag = true;
            break;
        case 0x00EE:
            if (chip8->sp > 0) {
                chip8->sp--;
                chip8->pc = chip8->stack[chip8->sp];
            }
            break;
        }
        break;

    case 0x1000:
        chip8->pc = nnn;
        break;

    case 0x2000:
        if (chip8->sp < STACK_SIZE) {
            chip8->stack[chip8->sp] = chip8->pc;
            chip8->sp++;
            chip8->pc = nnn;
        }
        break;

    case 0x3000:
        if (chip8->V[vx] == nn) chip8->pc += 2;
        break;

    case 0x4000:
        if (chip8->V[vx] != nn) chip8->pc += 2;
        break;

    case 0x5000:
        if (n == 0 && chip8->V[vx] == chip8->V[vy]) chip8->pc += 2;
        break;

    case 0x6000:
        chip8->V[vx] = nn;
        break;

    case 0x7000:
        chip8->V[vx] += nn;
        break;

    case 0x8000:
        switch (n) {
        case 0x0: chip8->V[vx] = chip8->V[vy]; break;
        case 0x1: chip8->V[vx] |= chip8->V[vy]; break;
        case 0x2: chip8->V[vx] &= chip8->V[vy]; break;
        case 0x3: chip8->V[vx] ^= chip8->V[vy]; break;
        case 0x4: {
            uint16_t sum = (uint16_t)chip8->V[vx] + (uint16_t)chip8->V[vy];
            chip8->V[0xF] = (sum > 0xFF) ? 1 : 0;
            chip8->V[vx] = (uint8_t)(sum & 0xFF);
        } break;
        case 0x5: {
            uint8_t not_borrow = (chip8->V[vx] >= chip8->V[vy]) ? 1 : 0;
            chip8->V[vx] -= chip8->V[vy];
            chip8->V[0xF] = not_borrow;
        } break;
        case 0x6: {
            uint8_t lsb = chip8->V[vx] & 0x01;
            chip8->V[vx] >>= 1;
            chip8->V[0xF] = lsb;
        } break;
        case 0x7: {
            uint8_t not_borrow = (chip8->V[vy] >= chip8->V[vx]) ? 1 : 0;
            chip8->V[vx] = chip8->V[vy] - chip8->V[vx];
            chip8->V[0xF] = not_borrow;
        } break;
        case 0xE: {
            uint8_t msb = (chip8->V[vx] & 0x80) >> 7;
            chip8->V[vx] <<= 1;
            chip8->V[0xF] = msb;
        } break;
        }
        break;

    case 0x9000:
        if (n == 0 && chip8->V[vx] != chip8->V[vy]) chip8->pc += 2;
        break;

    case 0xA000:
        chip8->I = nnn;
        break;

    case 0xB000:
        chip8->pc = nnn + chip8->V[0];
        break;

    case 0xC000:
        chip8->V[vx] = wasm_rand() & nn;
        break;

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

    case 0xE000:
        switch (nn) {
        case 0x009E:
            if (chip8->V[vx] < 16 && chip8->keypad[chip8->V[vx]]) chip8->pc += 2;
            break;
        case 0x00A1:
            if (chip8->V[vx] < 16 && !chip8->keypad[chip8->V[vx]]) chip8->pc += 2;
            break;
        }
        break;

    case 0xF000:
        switch (nn) {
        case 0x0007:
            chip8->V[vx] = chip8->delay_timer;
            break;
        case 0x000A: {
            bool key_pressed = false;
            for (int k = 0; k < 16; k++) {
                if (chip8->keypad[k]) {
                    chip8->V[vx] = (uint8_t)k;
                    key_pressed = true;
                    break;
                }
            }
            if (!key_pressed) chip8->pc -= 2;
        } break;
        case 0x0015:
            chip8->delay_timer = chip8->V[vx];
            break;
        case 0x0018:
            chip8->sound_timer = chip8->V[vx];
            break;
        case 0x001E:
            chip8->I += chip8->V[vx];
            break;
        case 0x0029:
            chip8->I = FONTSET_START_ADDRESS + ((chip8->V[vx] & 0x0F) * 5);
            break;
        case 0x0033: {
            uint8_t val = chip8->V[vx];
            chip8->memory[chip8->I]     = val / 100;
            chip8->memory[chip8->I + 1] = (val / 10) % 10;
            chip8->memory[chip8->I + 2] = val % 10;
        } break;
        case 0x0055:
            for (int i = 0; i <= vx; i++) {
                chip8->memory[chip8->I + i] = chip8->V[i];
            }
            break;
        case 0x0065:
            for (int i = 0; i <= vx; i++) {
                chip8->V[i] = chip8->memory[chip8->I + i];
            }
            break;
        }
        break;
    }
}
