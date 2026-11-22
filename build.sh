#!/bin/sh
# The Q2's binary and the App Store's zip: dist/sixtrack.zip holds .sixtrack/q2app and .sixtrack/q2-sixtrack.
# Needs clang and ld.lld (no MIPS sysroot: the program makes its own syscalls), as Buddy's build.
set -e
cd "$(dirname "$0")"
rm -rf dist && mkdir -p dist/.sixtrack
clang --target=mipsel-linux-gnu -march=mips32r2 -mabi=32 -mfp64 -mnan=2008 -mabs=2008 \
    -mno-abicalls -fno-pic -G0 -ffreestanding -fno-builtin -fno-stack-protector \
    -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-math-errno -O2 -Wall -Wextra -Werror \
    -c sixtrack.c -o dist/sixtrack.o
ld.lld -m elf32ltsmip -static -e __start --gc-sections -s -z noexecstack dist/sixtrack.o -o dist/.sixtrack/q2-sixtrack
cp q2app dist/.sixtrack/
(cd dist && zip -qr sixtrack.zip .sixtrack)
echo dist/sixtrack.zip
