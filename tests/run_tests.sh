#!/usr/bin/env bash
set -e

echo "================================================="
echo "        Byte8 Full System Test Suite             "
echo "================================================="

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo "[1/4] Building all project targets..."
make clean > /dev/null
make all > /dev/null
echo "  -> Targets compiled successfully: chip8, compiler, disassembler, web/chip8.wasm"

echo ""
echo "[2/4] Running Emulator Opcode Unit Tests..."
gcc -Wall -Wextra -std=c99 tests/test_opcodes.c src/chip8.c -o tests/test_opcodes
./tests/test_opcodes
rm -f tests/test_opcodes

echo ""
echo "[3/4] Testing Compiler & Preprocessor on all ROM sources..."
for src in roms/*.txt; do
    rom_name="$(basename "$src" .txt)"
    echo "  Compiling $src -> roms/${rom_name}.ch8"
    ./compiler "$src" "roms/${rom_name}.ch8" > /dev/null
done
echo "  -> All curated source programs compiled cleanly without errors."

echo ""
echo "[4/4] Testing Disassembler & Compiler Round-Trip Fidelity..."
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
echo "================================================="
echo "   All Byte8 Tests Passed Successfully! (100%)   "
echo "================================================="
