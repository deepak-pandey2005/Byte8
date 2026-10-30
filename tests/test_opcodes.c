#include "../src/chip8.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

static int total_tests = 0;
static int passed_tests = 0;

#define TEST_ASSERT(cond, msg) do { \
    total_tests++; \
    if (cond) { \
        passed_tests++; \
    } else { \
        fprintf(stderr, "  [FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); \
    } \
} while (0)

// Helper: load 2-byte opcode into memory at PC 0x200 and cycle once
static void run_one_opcode(Chip8 *c, uint16_t op) {
    c->memory[c->pc] = (op >> 8) & 0xFF;
    c->memory[c->pc + 1] = op & 0xFF;
    chip8_cycle(c);
}

static void test_cls_and_ret() {
    Chip8 c;
    chip8_init(&c);

    // Test CLS (0x00E0)
    c.gfx[0] = 1; c.gfx[63] = 1;
    run_one_opcode(&c, 0x00E0);
    TEST_ASSERT(c.gfx[0] == 0 && c.gfx[63] == 0, "CLS clears screen");
    TEST_ASSERT(c.draw_flag == true, "CLS sets draw_flag");

    // Test RET (0x00EE)
    c.sp = 1;
    c.stack[0] = 0x350;
    run_one_opcode(&c, 0x00EE);
    TEST_ASSERT(c.pc == 0x350, "RET restores PC from stack");
    TEST_ASSERT(c.sp == 0, "RET decrements SP");
}

static void test_jumps_and_calls() {
    Chip8 c;
    chip8_init(&c);

    // Test JUMP 1NNN
    run_one_opcode(&c, 0x1456);
    TEST_ASSERT(c.pc == 0x456, "1NNN sets PC to NNN");

    // Test CALL 2NNN
    uint16_t next_pc = c.pc + 2;
    run_one_opcode(&c, 0x2789);
    TEST_ASSERT(c.pc == 0x789, "2NNN jumps to subroutine");
    TEST_ASSERT(c.sp == 1, "2NNN increments SP");
    TEST_ASSERT(c.stack[0] == next_pc, "2NNN pushes next PC onto stack");

    // Test JUMP V0 + NNN (BNNN)
    c.pc = 0x200;
    c.V[0] = 0x12;
    run_one_opcode(&c, 0xB300);
    TEST_ASSERT(c.pc == 0x312, "BNNN jumps to NNN + V0");
}

static void test_skips() {
    Chip8 c;
    chip8_init(&c);

    // 3XNN: Skip if VX == NN
    c.V[1] = 0x42;
    c.pc = 0x200;
    run_one_opcode(&c, 0x3142);
    TEST_ASSERT(c.pc == 0x204, "3XNN skips instruction when equal");

    c.pc = 0x200;
    run_one_opcode(&c, 0x3199);
    TEST_ASSERT(c.pc == 0x202, "3XNN does not skip instruction when not equal");

    // 4XNN: Skip if VX != NN
    c.pc = 0x200;
    run_one_opcode(&c, 0x4199);
    TEST_ASSERT(c.pc == 0x204, "4XNN skips when not equal");

    // 5XY0: Skip if VX == VY
    c.V[2] = 0x42;
    c.pc = 0x200;
    run_one_opcode(&c, 0x5120);
    TEST_ASSERT(c.pc == 0x204, "5XY0 skips when VX == VY");

    // 9XY0: Skip if VX != VY
    c.V[3] = 0x10;
    c.pc = 0x200;
    run_one_opcode(&c, 0x9130);
    TEST_ASSERT(c.pc == 0x204, "9XY0 skips when VX != VY");
}

static void test_alu_operations() {
    Chip8 c;
    chip8_init(&c);

    // 6XNN: Set VX = NN
    run_one_opcode(&c, 0x60AB);
    TEST_ASSERT(c.V[0] == 0xAB, "6XNN sets VX");

    // 7XNN: VX += NN (no carry)
    run_one_opcode(&c, 0x7005);
    TEST_ASSERT(c.V[0] == 0xB0, "7XNN adds immediate");

    // 8XY0: VX = VY
    c.V[1] = 0x55;
    run_one_opcode(&c, 0x8210);
    TEST_ASSERT(c.V[2] == 0x55, "8XY0 assigns VY to VX");

    // 8XY1: VX |= VY
    c.V[0] = 0xF0; c.V[1] = 0x0F;
    run_one_opcode(&c, 0x8011);
    TEST_ASSERT(c.V[0] == 0xFF, "8XY1 bitwise OR");

    // 8XY2: VX &= VY
    c.V[0] = 0b10101010; c.V[1] = 0b11001100;
    run_one_opcode(&c, 0x8012);
    TEST_ASSERT(c.V[0] == 0b10001000, "8XY2 bitwise AND");

    // 8XY3: VX ^= VY
    c.V[0] = 0xFF; c.V[1] = 0x0F;
    run_one_opcode(&c, 0x8013);
    TEST_ASSERT(c.V[0] == 0xF0, "8XY3 bitwise XOR");

    // 8XY4: VX += VY with carry
    c.V[0] = 200; c.V[1] = 100;
    run_one_opcode(&c, 0x8014);
    TEST_ASSERT(c.V[0] == 44, "8XY4 addition wraps modulo 256");
    TEST_ASSERT(c.V[0xF] == 1, "8XY4 carry flag set on overflow");

    c.V[0] = 10; c.V[1] = 20;
    run_one_opcode(&c, 0x8014);
    TEST_ASSERT(c.V[0] == 30, "8XY4 normal addition");
    TEST_ASSERT(c.V[0xF] == 0, "8XY4 carry flag cleared when no overflow");

    // 8XY5: VX -= VY with NOT borrow
    c.V[0] = 50; c.V[1] = 20;
    run_one_opcode(&c, 0x8015);
    TEST_ASSERT(c.V[0] == 30, "8XY5 normal subtraction");
    TEST_ASSERT(c.V[0xF] == 1, "8XY5 VF=1 when no borrow");

    c.V[0] = 10; c.V[1] = 20;
    run_one_opcode(&c, 0x8015);
    TEST_ASSERT(c.V[0] == 246, "8XY5 underflow wraps modulo 256");
    TEST_ASSERT(c.V[0xF] == 0, "8XY5 VF=0 on borrow");

    // 8XY6: VX >>= 1
    c.V[0] = 0b00000101;
    run_one_opcode(&c, 0x8006);
    TEST_ASSERT(c.V[0] == 0b00000010, "8XY6 shifts right");
    TEST_ASSERT(c.V[0xF] == 1, "8XY6 VF receives dropped LSB");

    // 8XY7: VX = VY - VX
    c.V[0] = 10; c.V[1] = 35;
    run_one_opcode(&c, 0x8017);
    TEST_ASSERT(c.V[0] == 25, "8XY7 VY - VX subtraction");
    TEST_ASSERT(c.V[0xF] == 1, "8XY7 VF=1 when no borrow");

    // 8XYE: VX <<= 1
    c.V[0] = 0b10000001;
    run_one_opcode(&c, 0x800E);
    TEST_ASSERT(c.V[0] == 0b00000010, "8XYE shifts left");
    TEST_ASSERT(c.V[0xF] == 1, "8XYE VF receives MSB");
}

static void test_index_and_memory() {
    Chip8 c;
    chip8_init(&c);

    // ANNN: Set I
    run_one_opcode(&c, 0xA500);
    TEST_ASSERT(c.I == 0x500, "ANNN sets index register I");

    // FX1E: I += VX
    c.V[1] = 0x20;
    run_one_opcode(&c, 0xF11E);
    TEST_ASSERT(c.I == 0x520, "FX1E increments I by VX");

    // FX29: I = font VX
    c.V[2] = 0x3;
    run_one_opcode(&c, 0xF229);
    TEST_ASSERT(c.I == FONTSET_START_ADDRESS + (3 * 5), "FX29 points I to fontset for digit 3");

    // FX33: BCD conversion of 254
    c.I = 0x600;
    c.V[3] = 254;
    run_one_opcode(&c, 0xF333);
    TEST_ASSERT(c.memory[0x600] == 2, "FX33 hundreds digit");
    TEST_ASSERT(c.memory[0x601] == 5, "FX33 tens digit");
    TEST_ASSERT(c.memory[0x602] == 4, "FX33 units digit");

    // FX55: Dump registers V0..V3
    c.I = 0x700;
    c.V[0] = 11; c.V[1] = 22; c.V[2] = 33; c.V[3] = 44;
    run_one_opcode(&c, 0xF355);
    TEST_ASSERT(c.memory[0x700] == 11 && c.memory[0x703] == 44, "FX55 dumps V0-V3 to memory at I");

    // FX65: Load registers V0..V3
    c.V[0] = 0; c.V[1] = 0; c.V[2] = 0; c.V[3] = 0;
    run_one_opcode(&c, 0xF365);
    TEST_ASSERT(c.V[0] == 11 && c.V[3] == 44, "FX65 loads V0-V3 from memory at I");
}

static void test_graphics_and_collision() {
    Chip8 c;
    chip8_init(&c);

    // Place 2-byte sprite at I
    c.I = 0x300;
    c.memory[0x300] = 0b11110000;
    c.memory[0x301] = 0b11110000;
    c.V[0] = 10;
    c.V[1] = 5;

    // First draw: no collision
    run_one_opcode(&c, 0xD012);
    TEST_ASSERT(c.V[0xF] == 0, "DXYN VF=0 when no collision on first draw");
    TEST_ASSERT(c.gfx[5 * VIDEO_WIDTH + 10] == 1, "DXYN pixel correctly flipped to 1");

    // Second draw at same position: XOR collision detected!
    c.pc -= 2;
    run_one_opcode(&c, 0xD012);
    TEST_ASSERT(c.V[0xF] == 1, "DXYN VF=1 collision detected on XOR erase");
    TEST_ASSERT(c.gfx[5 * VIDEO_WIDTH + 10] == 0, "DXYN pixel XOR erased back to 0");
}

int main(void) {
    printf("=========================================\n");
    printf("Running Byte8 Emulator Opcode Unit Tests\n");
    printf("=========================================\n");

    test_cls_and_ret();
    test_jumps_and_calls();
    test_skips();
    test_alu_operations();
    test_index_and_memory();
    test_graphics_and_collision();

    printf("Results: %d / %d tests passed\n", passed_tests, total_tests);
    if (passed_tests == total_tests) {
        printf("All Emulator Opcode Tests PASSED!\n");
        printf("=========================================\n");
        return 0;
    } else {
        printf("Some tests FAILED!\n");
        printf("=========================================\n");
        return 1;
    }
}
