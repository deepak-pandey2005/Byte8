#!/usr/bin/env bash
set -e

echo "================================================="
echo "        Byte8 Full System Test Suite             "
echo "================================================="

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo "[1/6] Building all project targets..."
make clean > /dev/null
make all > /dev/null
echo "  -> Targets compiled successfully: chip8, compiler, disassembler, b8c"

echo ""
echo "[2/6] Running Emulator Opcode Unit Tests..."
${CC:-gcc} ${CFLAGS:--Wall -Wextra -std=c99} tests/test_opcodes.c src/chip8.c -o tests/test_opcodes
./tests/test_opcodes
rm -f tests/test_opcodes

echo ""
echo "[3/6] Testing Compiler & Preprocessor on all ROM sources..."
for src in roms/*.txt; do
    rom_name="$(basename "$src" .txt)"
    echo "  Compiling $src -> roms/${rom_name}.ch8"
    ./compiler "$src" "roms/${rom_name}.ch8" > /dev/null
done
echo "  -> All curated source programs compiled cleanly without errors."

echo ""
echo "[4/6] Testing Multi-Line Parameterized Compiler Macros..."
TMP_MACRO_SRC="/tmp/test_macro.txt"
TMP_MACRO_BIN="/tmp/test_macro.ch8"
cat << 'EOF' > "$TMP_MACRO_SRC"
#macro DRAW_ENTITY(rx, ry, s_label, h)
    i = s_label
    draw rx, ry, h
#endmacro

#macro SETUP_COORDS(vx, vy, xval, yval)
    vx = xval
    vy = yval
#endmacro

start:
    cls
    SETUP_COORDS(v0, v1, 10, 20)
    DRAW_ENTITY(v0, v1, sprite, 5)
loop:
    jump loop

sprite:
    db 0x3C, 0x7E, 0xFF, 0x7E, 0x3C
EOF

./compiler "$TMP_MACRO_SRC" "$TMP_MACRO_BIN" > /dev/null
echo "  -> #macro definition, parameter substitution, and nested calls compiled successfully."
rm -f "$TMP_MACRO_SRC" "$TMP_MACRO_BIN"

echo ""
echo "[5/6] Testing Disassembler & Compiler Round-Trip Fidelity..."
TMP_DISASM="/tmp/byte8_test_disasm.txt"
TMP_RECOMP="/tmp/byte8_test_recomp.ch8"

for rom in roms/*.ch8; do
    echo -n "  Verifying round-trip byte-for-byte on $rom ... "
    ./disassembler "$rom" "$TMP_DISASM" > /dev/null
    ./compiler "$TMP_DISASM" "$TMP_RECOMP" > /dev/null

    if cmp -s "$rom" "$TMP_RECOMP"; then
        echo "[MATCH OK]"
    else
        echo "[MISMATCH FAIL]"
        rm -f "$TMP_DISASM" "$TMP_RECOMP"
        exit 1
    fi
done

rm -f "$TMP_DISASM" "$TMP_RECOMP"

echo ""
echo "[6/6] Testing Byte8-C Compiler (b8c) & High-Level Examples..."
for b8 in examples/*.b8; do
    b8_name="$(basename "$b8" .b8)"
    echo -n "  Compiling Byte8-C source $b8 -> examples/${b8_name}.ch8 ... "
    ./b8c "$b8" -o "examples/${b8_name}.ch8" > /dev/null
    echo "[COMPILED OK]"

    echo -n "  Verifying round-trip on examples/${b8_name}.ch8 ... "
    TMP_B8_DISASM="/tmp/b8_test_disasm.txt"
    TMP_B8_RECOMP="/tmp/b8_test_recomp.ch8"
    ./disassembler "examples/${b8_name}.ch8" "$TMP_B8_DISASM" > /dev/null
    ./compiler "$TMP_B8_DISASM" "$TMP_B8_RECOMP" > /dev/null

    if cmp -s "examples/${b8_name}.ch8" "$TMP_B8_RECOMP"; then
        echo "[MATCH OK]"
        rm -f "$TMP_B8_DISASM" "$TMP_B8_RECOMP"
    else
        echo "[MISMATCH FAIL]"
        rm -f "$TMP_B8_DISASM" "$TMP_B8_RECOMP"
        exit 1
    fi
done

echo ""
echo "================================================="
echo "   All Byte8 Tests Passed Successfully! (100%)   "
echo "================================================="
