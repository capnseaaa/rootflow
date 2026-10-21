#!/bin/sh
# Build rootflow.gba. Art PNGs in art/ are converted on every build.
set -e
cd "$(dirname "$0")"
python3 tools/make_art.py
python3 tools/build_data.py
python3 tools/make_music.py
python3 tools/make_sfx.py
CFLAGS="-mcpu=arm7tdmi -mthumb -mthumb-interwork -O2 -ffreestanding -fno-builtin -Wall"
mkdir -p build
arm-none-eabi-gcc -mcpu=arm7tdmi -c src/crt0.s -o build/crt0.o
arm-none-eabi-gcc $CFLAGS -c src/main.c -o build/main.o
arm-none-eabi-gcc -mcpu=arm7tdmi -mthumb-interwork -nostdlib -T src/gba.ld build/crt0.o build/main.o -lgcc -o build/rootflow.elf
arm-none-eabi-objcopy -O binary build/rootflow.elf rootflow.gba
python3 tools/fixheader.py rootflow.gba
