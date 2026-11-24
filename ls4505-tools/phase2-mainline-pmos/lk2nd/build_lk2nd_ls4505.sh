#!/usr/bin/env bash
set -euo pipefail

# Build custom lk2nd secondary bootloader for LYF Flame 8 (LS-4505) on recovery (mmcblk0p24)
# Verified lk2nd upstream commit: bef71355c7e8742d21d6eaffc6a2d0065580cf07
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LK2ND_DIR="${LK2ND_DIR:-$HOME/ls4505-work/lk2nd}"
LK2ND_SHA="bef71355c7e8742d21d6eaffc6a2d0065580cf07"

if [ ! -d "$LK2ND_DIR/.git" ]; then
    git clone https://github.com/msm8916-mainline/lk2nd.git "$LK2ND_DIR"
    git -C "$LK2ND_DIR" checkout "$LK2ND_SHA"
    git -C "$LK2ND_DIR" submodule update --init --recursive
fi

cp -f "$SCRIPT_DIR/msm8909-lyf-ls4505.dts" "$LK2ND_DIR/lk2nd/device/dts/msm8909/msm8909-lyf-ls4505.dts"
if ! grep -q "msm8909-lyf-ls4505.dtb" "$LK2ND_DIR/lk2nd/device/dts/msm8909/rules.mk"; then
    sed -i 's/msm8909- nokia/msm8909-lyf-ls4505.dtb \\\n\t$(LOCAL_DIR)\/msm8909-nokia/' "$LK2ND_DIR/lk2nd/device/dts/msm8909/rules.mk" 2>/dev/null || \
    sed -i '/ADTBS +=/a \\t$(LOCAL_DIR)/msm8909-lyf-ls4505.dtb \\' "$LK2ND_DIR/lk2nd/device/dts/msm8909/rules.mk"
fi

# Note: LK2ND_BOOT_MEM_SIZE=0x03000000 keeps ramdisk/DTB below 0x83000000 (cont_splash_mem framebuffer)
make -C "$LK2ND_DIR" -j"$(nproc)" TOOLCHAIN_PREFIX=arm-none-eabi- \
    LK2ND_ADTBS="msm8909-lyf-ls4505.dtb" \
    LK2ND_QCDTBS="" \
    LK2ND_PARTITION_BASE="recovery" \
    LK2ND_BOOT_MEM_SIZE=0x03000000 \
    lk2nd-msm8909

ls -lh "$LK2ND_DIR/build-lk2nd-msm8909/lk2nd.img"
