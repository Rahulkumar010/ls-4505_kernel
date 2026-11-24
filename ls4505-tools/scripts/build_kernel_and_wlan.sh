#!/usr/bin/env bash
set -euo pipefail

# Build custom Linux 3.10.108+ LTS kernel, appended DTB, and Prima pronto_wlan.ko for LYF Flame 8 (LS-4505)
# Exact Toolchain Commit: d9735fc81434f2af2c44d86ca57740c673c8d9bc (AOSP arm-linux-androideabi-4.8 lollipop-mr1-release)
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TC_DIR="${TC_DIR:-$HOME/ls4505-work/arm-linux-androideabi-4.8}"
TC_URL="https://android.googlesource.com/platform/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.8"
TC_SHA="d9735fc81434f2af2c44d86ca57740c673c8d9bc"
OUT_DIR="${OUT_DIR:-$HOME/ls4505-work/out-3.10.108}"

if [ ! -x "$TC_DIR/bin/arm-linux-androideabi-gcc" ]; then
    echo "Fetching AOSP arm-linux-androideabi-4.8 @ $TC_SHA into $TC_DIR ..."
    mkdir -p "$TC_DIR"
    git -C "$TC_DIR" init
    git -C "$TC_DIR" remote add origin "$TC_URL" 2>/dev/null || git -C "$TC_DIR" remote set-url origin "$TC_URL"
    git -C "$TC_DIR" fetch --depth 1 origin "$TC_SHA"
    git -C "$TC_DIR" checkout FETCH_HEAD
fi

export PATH="$TC_DIR/bin:$PATH"
export ARCH=arm
export CROSS_COMPILE=arm-linux-androideabi-

mkdir -p "$OUT_DIR"
cd "$REPO_ROOT"

echo "[1/3] Configuring ls4505_defconfig..."
make O="$OUT_DIR" ls4505_defconfig

echo "[2/3] Compiling zImage-dtb, dtbs, and in-tree modules..."
make O="$OUT_DIR" -j"$(nproc)" zImage-dtb modules dtbs

echo "[3/3] Compiling matching Prima WLAN module (pronto_wlan.ko)..."
make -C "$REPO_ROOT" O="$OUT_DIR" M="$REPO_ROOT/external/prima-wlan" \
    WLAN_ROOT="$REPO_ROOT/external/prima-wlan" MODNAME=wlan CONFIG_PRONTO_WLAN=m \
    KCPPFLAGS="-DWLAN_OPEN_SOURCE" -j"$(nproc)" modules

arm-linux-androideabi-strip --strip-debug \
    -o "$OUT_DIR/pronto_wlan.ko" "$REPO_ROOT/external/prima-wlan/wlan.ko"

echo "=== Build Complete ==="
ls -lh "$OUT_DIR/arch/arm/boot/zImage" \
       "$OUT_DIR/arch/arm/boot/zImage-dtb" \
       "$OUT_DIR/arch/arm/boot/dts/qcom/msm8909-lyf-ls4505.dtb" \
       "$OUT_DIR/pronto_wlan.ko"
