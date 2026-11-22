# 02 — Building, Configuring & Updating the Kernel

This guide covers building the LYF Flame 8 (`LS-4505`) kernel and matching Prima Wi-Fi module (`pronto_wlan.ko`) from scratch, customizing kernel options, and applying future updates.

> **CRITICAL FILESYSTEM REQUIREMENT**: Always clone and build this repository on a **case-sensitive Linux filesystem** (native Ubuntu/Debian ext4, or `/home/<user>/...` inside WSL2 — **never** on `/mnt/c/` or `/mnt/d/` Windows NTFS, because Linux netfilter headers such as `xt_MARK.h` vs `xt_mark.h` collide on case-insensitive NTFS).

---

## 1. Host Prerequisites (Ubuntu 22.04 / 24.04 or WSL2)

```bash
sudo apt update
sudo apt install -y build-essential bc bison flex libssl-dev libncurses5-dev \
    libelf-dev device-tree-compiler git python-is-python3 rsync cpio kmod
```

### Toolchain: Official AOSP GCC 4.8 (`arm-linux-androideabi-4.8`)
Linux `3.10.x` on 32-bit ARMv7 Qualcomm CAF is designed for **AOSP GCC 4.8** (`lollipop-mr1-release`), which also produces **100% matching `CONFIG_MODVERSIONS` symbol CRCs** for binary-only stock modules:

```bash
mkdir -p ~/ls4505-work
git clone --depth 1 -b lollipop-mr1-release \
    https://android.googlesource.com/platform/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.8 \
    ~/ls4505-work/arm-linux-androideabi-4.8
```

> **Why `-mno-android -fno-pic` is already set in `Makefile`**:
> Android toolchains (`arm-linux-androideabi-gcc`) enable `-fpic` (`__PIC__=1`) by default. Without `-mno-android -fno-pic` in `KBUILD_CFLAGS` and `KBUILD_AFLAGS`, GCC places ~812 KB of initialized kernel pointer tables into orphan `.data.rel*` sections *after* `_edata`/`__bss_start`, causing `__mmap_switched` (`arch/arm/kernel/head-common.S`) to zero them out at `t = 0.000000s`! Both `ls4505-3.10.49` and `ls4505-3.10.108` (`main`) already enforce `-mno-android -fno-pic` in the root `Makefile`.

---

## 2. One-Command Automated Build (`zImage-dtb` + `pronto_wlan.ko`)

From a fresh clone on ext4:

```bash
git clone https://github.com/Rahulkumar010/ls-4505_kernel.git ~/ls4505-work/ls-4505_kernel
cd ~/ls4505-work/ls-4505_kernel
./ls4505-tools/scripts/build_kernel_and_wlan.sh
```

Outputs are generated in `~/ls4505-work/out-3.10.108/`:
- `arch/arm/boot/zImage` — Compressed kernel (`~7.0 MB`)
- `arch/arm/boot/dts/qcom/msm8909-lyf-ls4505.dtb` — Compiled LS-4505 Device Tree Blob (`~160 KB`)
- `arch/arm/boot/zImage-dtb` — Combined kernel + appended DTB (`~7.1 MB`)
- `pronto_wlan.ko` — Debug-stripped Prima WLAN kernel module (`~3.8 MB`, internal `MODNAME=wlan`)

---

## 3. Manual Step-by-Step Build Commands

```bash
export PATH="$HOME/ls4505-work/arm-linux-androideabi-4.8/bin:$PATH"
export ARCH=arm
export CROSS_COMPILE=arm-linux-androideabi-
OUT="$HOME/ls4505-work/out-3.10.108"

cd ~/ls4505-work/ls-4505_kernel
mkdir -p "$OUT"

# 1. Load LS-4505 configuration
make O="$OUT" ls4505_defconfig

# 2. (Optional) Customize kernel features interactively
make O="$OUT" menuconfig
# Save changes back to arch/arm/configs/ls4505_defconfig if desired:
# cp "$OUT/.config" arch/arm/configs/ls4505_defconfig

# 3. Build kernel, appended DTB, standalone DTBs, and in-tree modules
make O="$OUT" -j"$(nproc)" zImage-dtb modules dtbs

# 4. Verify zero .data.rel* orphan sections exist in vmlinux
arm-linux-androideabi-readelf -S "$OUT/vmlinux" | grep '\.data\.rel' || echo "PASS: Zero .data.rel sections!"

# 5. Build out-of-tree Prima WLAN module against the built kernel
make -C "$PWD" O="$OUT" M="$PWD/external/prima-wlan" \
    WLAN_ROOT="$PWD/external/prima-wlan" \
    MODNAME=wlan CONFIG_PRONTO_WLAN=m \
    KCPPFLAGS="-DWLAN_OPEN_SOURCE" -j"$(nproc)" modules

# 6. Strip debug symbols (shrinks 31 MB -> 3.8 MB to fit /data or /system)
arm-linux-androideabi-strip --strip-debug \
    -o "$OUT/pronto_wlan.ko" external/prima-wlan/wlan.ko
```

---

## 4. Building Other Versions in This Repository

To build the verified **`3.10.49`** stock-equivalent baseline instead of `3.10.108`:

```bash
git checkout ls4505-3.10.49
OUT="$HOME/ls4505-work/out-3.10.49"
mkdir -p "$OUT"
make O="$OUT" ls4505_defconfig
make O="$OUT" -j"$(nproc)" zImage-dtb modules dtbs
```
*(On `ls4505-3.10.49`, all `271/271` symbol CRCs of stock `/system/lib/modules/pronto/pronto_wlan.ko` and `44/44` CRCs of `core_ctl.ko` match natively, so rebuilding `pronto_wlan.ko` is optional.)*

---

## 5. How to Patch / Update the Kernel Further

1. **Adding security patches or backports**:
   - Cherry-pick or apply patches on a topic branch off `main` (`git checkout -b my-patch main`).
   - Remember Qualcomm's `scripts/gcc-wrapper.py` treats certain compiler warnings (such as unused variables) as fatal errors (`error, forbidden warning`).
   - Do **not** remove `-mno-android -fno-pic` from `Makefile`, do **not** add `pfn_valid()` to `virt_addr_valid()` in `arch/arm/include/asm/memory.h` (breaks `sg_set_buf` on carved-out MSM8909 CMA memory), and keep CAF's bidirectional `soc_new_compress()` in `sound/soc/soc-compress.c`.
2. **Whenever exported kernel symbols (`Module.symvers`) change**:
   - Always re-run step 5 & 6 above to rebuild `pronto_wlan.ko` against `$OUT` so `wlan.ko`'s `__versions` symbol CRCs match your updated `vmlinux` (`271/271`), and push the new `pronto_wlan.ko` alongside your test boot image!
   - Stock `/system/lib/modules/core_ctl.ko` (proprietary, no source) continues loading automatically thanks to our `strcmp(mod->name, "core_ctl") == 0` exemption in `check_version()` (`kernel/module.c`), provided `sizeof(struct module)` (`364` bytes) in `include/linux/module.h` is not altered.
3. **Verifying `pronto_wlan.ko` Symbol CRCs Before Flashing**:
   ```bash
   python3 - "$OUT/Module.symvers" "$OUT/pronto_wlan.ko" << 'PY'
   import sys, subprocess
   symvers = {l.split()[1]: int(l.split()[0], 16) for l in open(sys.argv[1]) if len(l.split()) >= 2}
   out = subprocess.check_output(['arm-linux-androideabi-objdump', '-s', '-j', '__versions', sys.argv[2]]).decode()
   raw = b''.join(bytes.fromhex(''.join(l.strip().split('  ')[0].split()[1:5]))
                  for l in out.splitlines() if l.strip() and l.strip()[0] in '0123456789abcdef' and ' ' in l)
   tot = len(raw) // 64
   mis = [raw[i*64+4:(i+1)*64].split(b'\0',1)[0].decode()
          for i in range(tot) if int.from_bytes(raw[i*64:i*64+4], 'little') != symvers.get(raw[i*64+4:(i+1)*64].split(b'\0',1)[0].decode(), -1)]
   print(f"Matched {tot - len(mis)}/{tot} symbol CRCs (mismatches: {mis})")
   PY
   ```
