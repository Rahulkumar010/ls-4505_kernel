#!/bin/sh
# Cross-compiles zero-dependency static ARMv7 dex-webd (~46 KB) against Alpine v3.21 musl-dev
set -e
SYS="${MUSL_SYSROOT:-/home/thati/ls4505-work/musl-sysroot}"
LIBGCC=$(arm-linux-gnueabihf-gcc -print-libgcc-file-name)
arm-linux-gnueabihf-gcc -O2 -Wall -Wextra \
    -nostdinc -isystem "$SYS/usr/include" -isystem "$(arm-linux-gnueabihf-gcc -print-file-name=include)" \
    -nostdlib -static "$SYS/usr/lib/crt1.o" "$SYS/usr/lib/crti.o" \
    dex-webd.c \
    -L"$SYS/usr/lib" -Wl,--start-group -lc "$LIBGCC" -Wl,--end-group "$SYS/usr/lib/crtn.o" \
    -o dex-webd
arm-linux-gnueabihf-strip dex-webd
ls -lh dex-webd
