# LYF Flame 8 (`LS-4505` / `MSM8909`) — Custom Linux `3.10.108+` LTS Kernel & WLAN Source

Complete reverse-engineered, reconstructed, security-hardened, and live-hardware-verified Linux kernel source tree for the **LYF Flame 8 (`LS-4505`)** smartphone (`Qualcomm Snapdragon 210 MSM8909` + `PM8909`, 1 GB RAM, 32-bit ARMv7, Android 6.0.1 `MMB29M`).

This repository contains **all baseline, testing, live-debugging, and production kernel versions**, the matched **Qualcomm Prima/Pronto WLAN (`wlan.ko` / `pronto_wlan.ko`)** driver source, **zero-Xubuntu live ADB testing & `ramoops` crash-extraction tools**, **EDL (`qdl`) Firehose flashing XMLs**, and **prebuilt Magisk-rooted boot images**.

---

## Phase 2: Mainline Linux 6.12.1 LTS + `lk2nd` + Ultra-Lean Android Dual-Boot

See **[`docs/06-PHASE2-ULTRALEAN-ANDROID-LK2ND-MAINLINE-LINUX.md`](docs/06-PHASE2-ULTRALEAN-ANDROID-LK2ND-MAINLINE-LINUX.md)** for full details, build scripts (`ls4505-tools/phase2-mainline-pmos/`), prebuilts (`prebuilt/phase2/`).

- **[`docs/07-FIXES-CAMERA-KEYS-OTG-FASTBOOT-JWM-MT7601U.md`](docs/07-FIXES-CAMERA-KEYS-OTG-FASTBOOT-JWM-MT7601U.md)** ? Front camera OTP red-tint fix, capacitive button mapping, hands-free `lk2nd` Fastboot, USB OTG + SMB1360 5V boost, `MT7601U` Dual Wi-Fi, ~68 MB `JWM` Touch UI, and `ls4505_ultralean` v2.

- **[`docs/08-FDROID-ANDROID6-TRUST-ANCHOR-FIX.md`](docs/08-FDROID-ANDROID6-TRUST-ANCHOR-FIX.md)** ? Root-cause & PEM-order fix for F-Droid Let's Encrypt `ISRG Root X1` (`6187b673.0`) TLS trust anchor validation on Android 6.0.1.

## Branches & Kernel Versions Included

| Branch / Tag | Kernel Version | Status & Purpose |
| :--- | :--- | :--- |
| **`main`** / **`ls4505-3.10.108`** (`tag: v3.10.108-ls4505`) | **`3.10.108+` (`#10`)** | **Production Default** — CAF `LA.BR.1.2.7_rb1.28` + 59 upstream Linux LTS releases (`v3.10.50..v3.10.108`) + post-EOL CVE backports + vendor UAF/deadlock fixes + bundled `external/prima-wlan`, `ls4505-tools`, `prebuilt`, and `docs`. **100% hardware-verified & permanently flashed.** |
| **`ls4505-3.10.49`** (`tag: v3.10.49-ls4505`) | **`3.10.49+` (`#4`)** | **Verified Stock-Equivalent Baseline** — Reconstructed LS-4505 Device Tree + ported drivers (`MSG2238`, `EPL259x`, `BMA250`, dual GPIO flash) + `-mno-android -fno-pic`. `0` symbol CRC diffs against stock `/system/lib/modules/*.ko`. |
| **`testing/ls4505-3.10.108-ramoops-debug`** | `3.10.108+` (debug) | **Live Over-the-Air Crash Telemetry Branch** — Enables `CONFIG_PANIC_ON_OOPS=y`, `0xae000000` (`0xee000000` VA) `ramoops` pstore console/oops carveout, self-recovering warm reset, and `ls4505-tools/dump_ramoops` module used to diagnose runtime vendor bugs without UART soldering. |
| **`testing/ls4505-3.10.108-rb1.52`** | `3.10.108+` (rb1.52) | **CAF `rb1.52` Experimental Merge** — Preserved testing branch merging `v3.10.108` onto Nov 2017 CAF `LA.BR.1.2.7_rb1.52` (documents the 54,807-line `KGSL`/`MDSS`/`Camera` userspace HAL ABI divergence vs Sep 2016 LS-4505 vendor blobs). |
| **`caf/LA.BR.1.2.7_rb1.28`** | `3.10.49` | **Unmodified Qualcomm CAF Baseline** — Clean upstream CodeLinaro `msm-3.10` Android 6.0.1 base commit (`SELinux policy v30`) for clean `git diff` comparisons. |
| **`archive/mainline-6.x-drm-panel-attempt`** | `6.x` (WIP) | **Archived Mainline Fork** — Preserves the earlier `msm8916-mainline` DRM panel driver (`panel-ili9806e-txd-ips.c`) experiment for future Phase 2 `lk2nd`/postmarketOS work. |

---

## Documentation Index

1. [**01 — Hardware Forensics & Device Tree Architecture**](./docs/01-HARDWARE-FORENSICS-AND-DTS.md)
   - Exact SoC/PMIC/board IDs, eMMC GPT partition table (`mmcblk0p1..p30`), physical RAM/CMA carveouts, `aboot` (`0x8f600000`) disassembly findings, I2C bus 1/3/4/5 peripheral map, GPIO table, DSI panel, cameras, audio DAI, and why prior boot attempts failed.
2. [**02 — Building, Configuring & Updating the Kernel**](./docs/02-BUILDING-AND-UPDATING-KERNEL.md)
   - One-command and manual build guides (WSL2 / Ubuntu), AOSP GCC 4.8 setup, mandatory `-mno-android -fno-pic` flags, Ubuntu 24.04 host compiler fixes, building `external/prima-wlan` (`pronto_wlan.ko`), verifying `MODVERSIONS` symbol CRCs, and applying future kernel updates/patches.
3. [**03 — Zero-Xubuntu Safe Testing, Live `ramoops` Debugging & Permanent Flashing**](./docs/03-ZERO-XUBUNTU-TESTING-AND-FLASHING.md)
   - Risk-free one-shot testing via the `recovery` partition (`mmcblk0p24`) over ADB without touching `boot` (`mmcblk0p20`), kernel-version-aware `pronto_wlan.ko` bind-mounting, over-the-air physical RAM crash extraction (`dump_ramoops.ko`), permanent `boot` (`mmcblk0p20`) installation + stock `recovery` restoration, and Xubuntu EDL (`qdl` + Firehose) emergency flashing.
4. [**04 — Root-Cause Analysis & Bugfixes**](./docs/04-FIXES-AND-ROOT-CAUSE-ANALYSIS.md)
   - Deep technical post-mortems of all 8 critical kernel/compiler/vendor-driver bugs solved during bring-up (Android GCC `-fpic` `.data.rel*` corruption, SELinux v28 vs v30, ARM `virt_addr_valid` `pfn_valid`, ASoC bidirectional compress timer UAF, MStar `MSG2238` `_gIrqLock` recursive spinlock deadlock, and Qualcomm `SMB1360` `wakeup_source` UAF on `-EPROBE_DEFER`).
5. [**05 — Security Upgrades (`3.10.108` + Post-EOL CVEs) & Hardware Verification Report**](./docs/05-SECURITY-UPGRADE-AND-VERIFICATION.md)
   - Summary of the 59-release Linux `3.10.49 -> 3.10.108` LTS merge, post-EOL CVE backports (**BadBinder** `CVE-2019-2215`, **TCP SACK Panic** `CVE-2019-11477/78/79`, **BlueBorne** `CVE-2017-1000251`, **Dirty COW** `CVE-2016-5195`, etc.), and live hardware verification logs across every subsystem.

---

## Repository Layout (`main`)

```text
├── arch/arm/boot/dts/qcom/
│   ├── msm8909-lyf-ls4505.dts                      # Complete LS-4505 board Device Tree
│   ├── dsi-panel-ili9806e-txd-fwvga-ips-video.dtsi # TXD ILI9806E 480x854 IPS MIPI-DSI panel
│   ├── msm8909-camera-sensor-ls4505.dtsi           # OV8856 rear + S5K5E2 front camera & actuator/EEPROM
│   └── batterydata-lyf-ck2000.dtsi                 # LYF CK2000 2000mAh Li-ion battery profile
├── arch/arm/configs/
│   └── ls4505_defconfig                            # Production LS-4505 kernel configuration
├── drivers/
│   ├── input/touchscreen/msg2238/                  # Ported & spinlock-fixed MStar MSG2238 driver
│   ├── input/misc/epl259x.c                        # Ported ELAN EPL259x ALS/PS sensor driver
│   ├── input/misc/bma2x2.c                         # Patched Bosch BMA250 (0x03) accelerometer driver
│   ├── leds/leds-msm-gpio-flash.c                  # Dual GPIO (GPIO 3 / GPIO 9) front & rear flash
│   └── power/smb1360-charger-fg.c                  # SMB1360 charger/FG (fixed -EPROBE_DEFER UAF)
├── external/prima-wlan/                            # Qualcomm CAF Prima/Pronto WLAN v3.0.10.085 source
├── ls4505-tools/
│   ├── scripts/
│   │   ├── build_kernel_and_wlan.sh                # Automated one-command kernel + wlan.ko builder
│   │   ├── test_boot_via_recovery.sh               # Zero-Xubuntu safe test flasher (mmcblk0p24)
│   │   ├── flash_permanent_boot.sh                 # Final permanent installer (mmcblk0p20 + restore recovery)
│   │   ├── hw_audit.sh                             # On-device full subsystem audit script
│   │   └── test_wifi_and_suspend.sh                # Wi-Fi + wakeup_sources + suspend/resume verifier
│   ├── dump_ramoops/                               # Live 0xee000000 pstore/ramoops reader module
│   ├── edl/                                        # qdl Firehose boot-only flash & restore XMLs
│   └── reference/                                  # Extracted stock defconfig & stock SKUE dtbdump DTS
├── prebuilt/
│   ├── boot-ls4505-3.10.108.img                    # Ready-to-flash 3.10.108+ Magisk-rooted boot.img
│   ├── boot-ls4505-3.10.49.img                     # Ready-to-flash 3.10.49+ Magisk-rooted boot.img
│   ├── zImage-dtb-3.10.108                         # Compiled 3.10.108+ kernel + appended LS-4505 DTB
│   ├── msm8909-lyf-ls4505-3.10.108.dtb             # Compiled standalone LS-4505 Device Tree Blob
│   └── modules/
│       ├── pronto_wlan-3.10.108.ko                 # Stripped 3.10.108+ WLAN module (271/271 CRCs)
│       ├── pronto_wlan-stock-3.10.49.ko            # Stock 3.10.49 WLAN module backup
│       └── core_ctl-stock.ko                       # Stock Qualcomm core_ctl module backup
└── docs/                                           # Detailed engineering, testing & security guides
```

---

## Quick Start

### 1. Build from Source (Linux / WSL2 ext4)
Only **AOSP GCC 4.8** (`https://android.googlesource.com/platform/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.8` @ commit `d9735fc81434f2af2c44d86ca57740c673c8d9bc`, auto-fetched by `build_kernel_and_wlan.sh`) and standard Ubuntu build packages (`build-essential bc bison flex libssl-dev libelf-dev device-tree-compiler python-is-python3 rsync cpio kmod`) are needed — the full kernel + Prima WLAN (`external/prima-wlan`) source are bundled in this repo:
```bash
git clone https://github.com/Rahulkumar010/ls-4505_kernel.git ~/ls4505-work/ls-4505_kernel
cd ~/ls4505-work/ls-4505_kernel
./ls4505-tools/scripts/build_kernel_and_wlan.sh
```
*(See [`docs/02-BUILDING-AND-UPDATING-KERNEL.md`](./docs/02-BUILDING-AND-UPDATING-KERNEL.md) for the full table of upstream URLs, exact commit SHAs, host versions, and offline `boot.img` packing.)*

### 2. Safe One-Shot Live Test over ADB (No Xubuntu / Untouched `boot`)
```bash
adb push ~/ls4505-work/out-3.10.108/arch/arm/boot/zImage /data/local/tmp/zImage-3.10.108
adb push ~/ls4505-work/out-3.10.108/arch/arm/boot/dts/qcom/msm8909-lyf-ls4505.dtb /data/local/tmp/ls4505-3.10.108.dtb
adb push ~/ls4505-work/out-3.10.108/pronto_wlan.ko /data/local/tmp/pronto_wlan-3.10.108.ko
adb push ls4505-tools/scripts/test_boot_via_recovery.sh /data/local/tmp/
adb shell "su -c '/sbin/.magisk/busybox/sh /data/local/tmp/test_boot_via_recovery.sh'"
adb reboot recovery
```
*(Boots the new kernel **once** from `mmcblk0p24`. Any normal reboot returns to untouched `mmcblk0p20`.)*

### 3. Permanent Installation to `boot` (`mmcblk0p20`) + Restore Stock `recovery`
```bash
# Over ADB root (after verifying the one-shot recovery boot):
adb push recovery-orginal.img /data/local/tmp/recovery-orginal.img
adb push ls4505-tools/scripts/flash_permanent_boot.sh /data/local/tmp/
adb shell "su -c '/sbin/.magisk/busybox/sh /data/local/tmp/flash_permanent_boot.sh'"
adb reboot

# Or directly via Qualcomm EDL (qdl) on Linux/Xubuntu:
sudo ./qdl prog_emmc_firehose_8909_ddr.mbn rawprogram_boot_3.10.108.xml
```
