#!/usr/bin/env bash
set -euo pipefail

# Offline host boot.img builder for LS-4505 using abootimg (no live ADB phone needed)
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT_DIR="${OUT_DIR:-$HOME/ls4505-work/out-3.10.108}"
ZIMAGE_DTB="${1:-$OUT_DIR/arch/arm/boot/zImage-dtb}"
OUT_IMG="${2:-$OUT_DIR/boot-ls4505-3.10.108-custom.img}"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

command -v abootimg >/dev/null 2>&1 || { echo "Install abootimg first: sudo apt install -y abootimg"; exit 1; }
[ -f "$ZIMAGE_DTB" ] || { echo "Missing zImage-dtb: $ZIMAGE_DTB"; exit 1; }

cd "$TMP_DIR"
abootimg -x "$REPO_ROOT/prebuilt/boot-ls4505-3.10.108.img" >/dev/null
sed -i '/^bootsize/d' bootimg.cfg
abootimg --create raw-boot.img -f bootimg.cfg -k "$ZIMAGE_DTB" -r initrd.img >/dev/null

dd if=/dev/zero of="$OUT_IMG" bs=1048576 count=32 status=none
dd if=raw-boot.img of="$OUT_IMG" conv=notrunc status=none
echo "Created 32MB flashable boot image: $OUT_IMG"
ls -lh "$OUT_IMG"
md5sum "$OUT_IMG"
