#ifndef CHIP8_H
#define CHIP8_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define VIDEO_WIDTH 64
#define VIDEO_HEIGHT 32
#define SCHIP_VIDEO_WIDTH 128
#define SCHIP_VIDEO_HEIGHT 64
#define STACK_SIZE 16
#define FONTSET_START_ADDRESS 0x50
#define SCHIP_FONTSET_START_ADDRESS 0xA0

#define MAX_BREAKPOINTS 32
#define MAX_WATCHPOINTS 32

typedef struct {
  uint16_t address;
  bool is_active;
} Breakpoint;

typedef struct {
  bool is_reg;           // true if watching V[reg_index], false if memory[mem_addr]
  uint8_t reg_index;
  uint16_t mem_addr;
  uint8_t last_value;
  bool is_active;
} Watchpoint;

typedef struct {
  uint8_t memory[4096];
  uint8_t V[16];
  uint16_t I;
  uint16_t pc;
  uint16_t opcode;
  uint16_t stack[STACK_SIZE];
  uint16_t sp;
  uint8_t delay_timer;
  uint8_t sound_timer;
  uint8_t gfx[SCHIP_VIDEO_WIDTH * SCHIP_VIDEO_HEIGHT];
  uint8_t keypad[16];
  bool draw_flag;

  // SUPER-CHIP (SCHIP-8) state
  bool hires_mode;
  int video_width;
  int video_height;
  uint8_t rpl_flags[16];
  bool exit_flag;

  // Debugging & Instrumentation
  uint64_t cycle_count;
  Breakpoint breakpoints[MAX_BREAKPOINTS];
  int breakpoint_count;
  Watchpoint watchpoints[MAX_WATCHPOINTS];
  int watchpoint_count;
  bool breakpoint_hit;
  bool watchpoint_hit;
  char watchpoint_hit_desc[128];
} Chip8;

void chip8_init(Chip8 *chip8);
bool chip8_load_rom(Chip8 *chip8, const char *filename);
void chip8_cycle(Chip8 *chip8);
void chip8_update_timers(Chip8 *chip8);
bool chip8_save_state(const Chip8 *chip8, const char *filename);
bool chip8_load_state(Chip8 *chip8, const char *filename);
void chip8_disasm_opcode(uint16_t opcode, char *out_buf, size_t buf_size);
void chip8_dump_debug(const Chip8 *chip8);
void chip8_dump_memory(const Chip8 *chip8, uint16_t addr, size_t count);

bool chip8_add_breakpoint(Chip8 *chip8, uint16_t addr);
bool chip8_remove_breakpoint(Chip8 *chip8, uint16_t addr);
void chip8_list_breakpoints(const Chip8 *chip8);
bool chip8_check_breakpoint(Chip8 *chip8);

bool chip8_add_watchpoint_reg(Chip8 *chip8, uint8_t reg_idx);
bool chip8_add_watchpoint_mem(Chip8 *chip8, uint16_t mem_addr);
bool chip8_check_watchpoints(Chip8 *chip8);

#endif
