#!/usr/bin/env bash
set -euo pipefail

# Build Mainline Linux 6.12.1 LTS (msm8916-mainline) for LYF Flame 8 (LS-4505)
# Base commit: 1728ab7f6075ea66a7ad1400791c826b9f01f8c3 (branch mainline/ls4505-6.12.1 in this repo)
MAINLINE_DIR="${MAINLINE_DIR:-$HOME/ls4505-work/linux-mainline}"
OUT_DIR="${OUT_DIR:-$HOME/ls4505-work/out-mainline}"
MOD_DIR="${MOD_DIR:-$HOME/ls4505-work/mainline-mods}"

export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-

mkdir -p "$OUT_DIR" "$MOD_DIR"
make -C "$MAINLINE_DIR" O="$OUT_DIR" ls4505_mainline_defconfig
make -C "$MAINLINE_DIR" O="$OUT_DIR" -j"$(nproc)" zImage modules qcom/qcom-msm8909-lyf-ls4505.dtb
make -C "$MAINLINE_DIR" O="$OUT_DIR" INSTALL_MOD_PATH="$MOD_DIR" INSTALL_MOD_STRIP=1 modules_install

ls -lh "$OUT_DIR/arch/arm/boot/zImage" "$OUT_DIR/arch/arm/boot/dts/qcom/qcom-msm8909-lyf-ls4505.dtb"
