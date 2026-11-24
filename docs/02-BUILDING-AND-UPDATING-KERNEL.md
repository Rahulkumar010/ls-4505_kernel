# 02 — Building, Configuring & Updating the Kernel

This guide covers building the LYF Flame 8 (`LS-4505`) kernel and matching Prima Wi-Fi module (`pronto_wlan.ko`) from scratch, customizing kernel options, and applying future updates.

> **CRITICAL FILESYSTEM REQUIREMENT**: Always clone and build this repository on a **case-sensitive Linux filesystem** (native Ubuntu/Debian ext4, or `/home/<user>/...` inside WSL2 — **never** on `/mnt/c/` or `/mnt/d/` Windows NTFS, because Linux netfilter headers such as `xt_MARK.h` vs `xt_mark.h` collide on case-insensitive NTFS).

---

## 1. Exact Provenance, Toolchains, External Tools & Commit SHAs

Every upstream source repository, cross-compiler, and flashing/packing utility used to build and flash this kernel is pinned below by exact URL and commit SHA. **Note:** Only the **AOSP GCC 4.8 cross-compiler** needs to be downloaded separately to compile the kernel & Wi-Fi module — the complete CAF kernel, `linux-stable` `v3.10.108` merge, Prima WLAN source (`external/prima-wlan/`), and helper scripts (`ls4505-tools/`) are already self-contained inside this repository.

| Component | Required Download? | Repository URL | Branch / Tag | Exact Commit SHA & Version |
| :--- | :--- | :--- | :--- | :--- |
| **Cross-Compiler (`arm-linux-androideabi-4.8`)** | **Yes** (auto-cloned by `build_kernel_and_wlan.sh`) | `https://android.googlesource.com/platform/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.8` | `lollipop-mr1-release` | **`d9735fc81434f2af2c44d86ca57740c673c8d9bc`** (`2013-08-22`, `arm-linux-androideabi-gcc (GCC) 4.8`) |
| **Qualcomm CAF Kernel Base (`msm-3.10`)** | No *(included in repo)* | `https://git.codelinaro.org/clo/la/kernel/msm-3.10.git` | `LA.BR.1.2.7_rb1.28` *(prod)* / `LA.BR.1.2.7_rb1.52` *(test)* | **`e7ff3099f9143956ea8bfd19a9c3ae0069b05406`** *(rb1.28)* / `2099af7a4a610dd5bccbbdfadc24ff70080a9a36` *(rb1.52)* |
| **Upstream Linux Stable LTS (`linux-stable`)** | No *(merged in repo)* | `https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git` | `v3.10.49` .. `v3.10.108` | Base **`d02dae430d1f41f2c1dc8592f68569f90b86f6d9`** -> Target **`e7a59c7f266809d17dcde20fd2055e23e7eb6895`** |
| **Qualcomm Prima/Pronto WLAN (`wlan.ko`)** | No *(bundled at `external/prima-wlan/`)* | `https://git.codelinaro.org/clo/la/platform/vendor/qcom-opensource/wlan/prima.git` *(mirror: `https://github.com/mifl/android_vendor_qcom-opensource_wlan_prima.git`)* | `LA.BR.1.2.7_rb1.28` | **`101740b1ba2b10313233cec674410b4b7733a0b4`** (`2017-11-14`, `AU_LINUX_ANDROID_LA.BR.1.2.7_RB1.06.00.01.208.069`, driver `v3.0.10.085`) |
| **`qdl` (Qualcomm EDL Firehose Flasher)** | Only for EDL flashing | `https://github.com/linux-msm/qdl.git` | `master` | **`67926299fb959b33625661d333c1c6bf8e0c8c0d`** (`2026-05-16`) + stock `prog_emmc_firehose_8909_ddr.mbn` |
| **`magiskboot` & `busybox` (On-Device Packer)** | Already on Magisk-rooted device | `https://github.com/topjohnwu/Magisk/releases/tag/v20.4` | `v20.4` | `/data/adb/magisk/magiskboot` & `/sbin/.magisk/busybox` (`BusyBox v1.32.1-Magisk`) |
| **`abootimg` (Optional Offline Host Packer)** | Optional (if packing `boot.img` offline without ADB) | `https://github.com/ggrandou/abootimg.git` | `master` | **`7e127fee6a3981f6b0a50ce9910267cd501e09d4`** (`2012-09-12`, or `sudo apt install abootimg`) |

---

## 2. Host Prerequisites (Ubuntu 22.04 / 24.04 LTS or WSL2 `x86_64`)

Verified on **Ubuntu 24.04 LTS (WSL2 ext4)** with Host `gcc 13.3.0`, `GNU Make 4.3`, `Python 3.12.3`, and `DTC 1.7.0`:

```bash
sudo apt update
sudo apt install -y build-essential bc bison flex libssl-dev libncurses5-dev \
    libelf-dev device-tree-compiler git python-is-python3 rsync cpio kmod abootimg android-tools-adb
```

### Cloning the Exact Pinpoint Cross-Compiler (`d9735fc81434f2af2c44d86ca57740c673c8d9bc`)
Linux `3.10.x` on 32-bit ARMv7 Qualcomm CAF requires **AOSP GCC 4.8** (`x86_64` Linux prebuilt), which produces **100% matching `CONFIG_MODVERSIONS` symbol CRCs** (`0` diffs) for binary-only stock modules (`core_ctl.ko` / `pronto_wlan.ko`):

```bash
mkdir -p ~/ls4505-work/arm-linux-androideabi-4.8
cd ~/ls4505-work/arm-linux-androideabi-4.8
git init
git remote add origin https://android.googlesource.com/platform/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.8
git fetch --depth 1 origin d9735fc81434f2af2c44d86ca57740c673c8d9bc
git checkout FETCH_HEAD

# Verify exact compiler version & commit:
git rev-parse HEAD
# -> d9735fc81434f2af2c44d86ca57740c673c8d9bc
./bin/arm-linux-androideabi-gcc --version
# -> arm-linux-androideabi-gcc (GCC) 4.8
```

> **Why `-mno-android -fno-pic` is already set in `Makefile`**:
> Android toolchains (`arm-linux-androideabi-gcc`) enable `-fpic` (`__PIC__=1`) by default. Without `-mno-android -fno-pic` in `KBUILD_CFLAGS` and `KBUILD_AFLAGS`, GCC places ~812 KB of initialized kernel pointer tables into orphan `.data.rel*` sections *after* `_edata`/`__bss_start`, causing `__mmap_switched` (`arch/arm/kernel/head-common.S`) to zero them out at `t = 0.000000s`! Both `ls4505-3.10.49` and `ls4505-3.10.108` (`main`) already enforce `-mno-android -fno-pic` in the root `Makefile`.

---

## 3. One-Command Automated Build (`zImage-dtb` + `pronto_wlan.ko`)

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

## 4. Manual Step-by-Step Build Commands

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

## 5. Packing a Flashable 32 MB `boot.img` Offline on Host (Without ADB)

Normally [`test_boot_via_recovery.sh`](../ls4505-tools/scripts/test_boot_via_recovery.sh) repacks `boot.img` directly on the phone via `/data/adb/magisk/magiskboot`. If you ever want to build a ready-to-EDL-flash 32 MB `boot.img` **completely offline on Linux** using `abootimg` (extracting the Magisk ramdisk from `prebuilt/boot-ls4505-3.10.108.img`):

```bash
mkdir -p ~/ls4505-work/repack-host && cd ~/ls4505-work/repack-host

# 1. Extract the working Magisk ramdisk (initrd.img) and bootimg.cfg from prebuilt boot.img
abootimg -x ~/ls4505-work/ls-4505_kernel/prebuilt/boot-ls4505-3.10.108.img

# 2. Pack new zImage-dtb (which MUST have msm8909-lyf-ls4505.dtb appended!) + initrd.img
#    Exact LS-4505 boot header parameters:
#      pagesize = 0x800 (2048), kerneladdr = 0x80008000, ramdiskaddr = 0x82000000, tagsaddr = 0x81e00000
#      cmdline  = console=ttyHSL0,115200,n8 androidboot.console=ttyHSL0 androidboot.hardware=qcom msm_rtb.filter=0x237 ehci-hcd.park=3 androidboot.bootdevice=7824900.sdhci lpm_levels.sleep_disabled=1 earlyprintk
sed -i '/^bootsize/d' bootimg.cfg
abootimg --create new-boot-raw.img -f bootimg.cfg \
    -k "$OUT/arch/arm/boot/zImage-dtb" \
    -r initrd.img

# 3. Pad to exact 32 MB (33,554,432 bytes = 65,536 sectors) partition size for EDL / dd
dd if=/dev/zero of=boot-ls4505-custom.img bs=1M count=32
dd if=new-boot-raw.img of=boot-ls4505-custom.img conv=notrunc
```

---

## 6. Building Other Versions in This Repository

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

## 7. How to Patch / Update the Kernel Further

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
