# Phase 2: Ultra-Lean Android + `lk2nd` + Mainline Linux 6.12.1 LTS Dual-Boot (`Xfce4` Touch Desktop)

This document covers the complete **Phase 2 (`dual_both`)** architecture verified live on the **LYF Flame 8 (`LS-4505`, Qualcomm `MSM8909` + `PM8909`)**:

1. **Ultra-Lean Android 6.0.1 (`Linux 3.10.108+`)** on `boot` (`mmcblk0p20`) ? reversible Magisk systemless debloat removing OEM spyware/bloatware (`160 -> 97` system packages, `593 MB` free+cached RAM, `256 MB` ZRAM).
2. **Custom `lk2nd` Secondary Bootloader** on `recovery` (`mmcblk0p24`) ? restoring full unlocked Fastboot (which LYF stripped from stock `emmc_appsboot.mbn`) and `extlinux.conf` boot discovery.
3. **Mainline Linux `6.12.1` LTS (`msm8916-mainline`) + Alpine/postmarketOS `v3.21` ARMv7 `Xfce4` Lightweight Touch Desktop** ? booting in **~20 seconds**, consuming only **`134?140 MB` RAM** with full graphical X11 desktop running (`756+ MB` available RAM!), and leaving **`3.2 GB` (`81%`) free** on shared `userdata` (`mmcblk0p29`).

---

## 1. Exact Pinned Source Commits & Toolchains

| Component | Upstream Repository | Exact Commit / Version | Toolchain |
|---|---|---|---|
| **Mainline Linux `6.12.1` LTS** | `https://github.com/msm8916-mainline/linux.git` | Base `1728ab7f6075ea66a7ad1400791c826b9f01f8c3` -> Patch [`0001-ARM-dts-qcom-Add-LYF-Flame-8-LS-4505-MSM8909-mainlin.patch`](../ls4505-tools/phase2-mainline-pmos/mainline-dts/0001-ARM-dts-qcom-Add-LYF-Flame-8-LS-4505-MSM8909-mainlin.patch) | `arm-linux-gnueabihf-gcc` (GCC 13.3.0, Binutils 2.42) |
| **`lk2nd` Bootloader** | `https://github.com/msm8916-mainline/lk2nd.git` | `bef71355c7e8742d21d6eaffc6a2d0065580cf07` + [`msm8909-lyf-ls4505.dts`](../ls4505-tools/phase2-mainline-pmos/lk2nd/msm8909-lyf-ls4505.dts) | `arm-none-eabi-gcc` |
| **Userland Rootfs** | Official Alpine Linux `v3.21` `armv7` | `alpine-minirootfs-3.21.3-armv7.tar.gz` + `apk-tools-static-2.14.13-r0` (`xfce4`, `lightdm`, `xorg-server`, `networkmanager`, `openssh`, `dillo`, `matchbox-keyboard`) | Native ARMv7 (`musl` hard-float) |

---

## 2. Track A: Ultra-Lean Android (`ls4505_ultralean` Magisk Module)

### OEM Spyware, Telemetry & Broken Daemon Audit
Auditing all 166 preinstalled packages and `init` services on stock LYF Flame 8 firmware uncovered:
- **Adups FOTA Spyware**: `com.adups.fota` & `com.adups.fota.sysoper` (system-privileged silent background telemetry/installer).
- **Jio Ignite Silent Installer**: `com.jio.mobileservices` (`Ignite`).
- **Carrier/OEM Trackers**: `com.salestracker.asan` (`SalesTracker`), `com.kryten` (`Kryten` OMA DM provisioning).
- **9 Factory Engineering Test APKs**: `FactoryKit`, `EngineeringMode`, `AutoMMI`, `FTMode`, `Cit`, etc.
- **Crashing Background Daemons**: `ss_ramdump` (crashing and respawning every 5 seconds in `init`), `FactoryNvServer`, and `ptt_socket_app`.

### Systemless Implementation
[`ls4505-tools/phase2-mainline-pmos/ultralean-android/install_ultralean.sh`](../ls4505-tools/phase2-mainline-pmos/ultralean-android/install_ultralean.sh) installs `/data/adb/modules/ls4505_ultralean`:
- Systemlessly masks **49 `/system/app`** and **14 `/system/priv-app`** directories (including GMS `GmsCore`/`Velvet` while **preserving `GooglePackageInstaller`, `Chrome`, `WebViewGoogle`, `CMFileManager`, `SnapdragonCamera`, `LatinIME`, `FM2`, `Gallery2`, `Music`, Phone/SMS, and Settings**).
- Stops `ss_ramdump`, `FactoryNvServer`, and `ptt_socket_app` cleanly on boot.
- Configures a **256 MB ZRAM** swap device and `0.5x` WindowManager animation scales.
- **Result**: System packages reduced **`160 -> 97`**, reclaimable free+cached RAM increased to **`593 MB / 922 MB` (`64.3%`)**, and 100% reversible by deleting `/data/adb/modules/ls4505_ultralean`.

---

## 3. Track B: Custom `lk2nd` & Engineering Root-Cause Fixes

Reverse-engineering stock `Firmware/emmc_appsboot.mbn` revealed two critical OEM quirks on the LS-4505:
1. **Stripped Fastboot Table in Stock `aboot`**: LYF compiled stock `aboot` with only `getvar:` and `download:` registered (`flash:`, `boot`, and `reboot` all return `FAILED (remote: 'unknown command')`). Flashing `lk2nd` to `recovery` (`mmcblk0p24`) provides a complete, unrestricted `lk2nd` Fastboot server (entered by holding **Volume Down** right as `lk2nd` starts) without touching `aboot` or `boot`.
2. **Mandatory 32-bit Appended DTB (`skeleton32.dtsi`)**: Prebuilt `lk2nd-msm8909.img` uses QCDT and lacked LS-4505's `<0x1000b 0x0>` (`PM8909` SKUE subtype 0) board tuple. Custom [`msm8909-lyf-ls4505.dts`](../ls4505-tools/phase2-mainline-pmos/lk2nd/msm8909-lyf-ls4505.dts) builds with `LK2ND_ADTBS="msm8909-lyf-ls4505.dtb" LK2ND_QCDTBS=""`.
3. **`LK2ND_BOOT_MEM_SIZE=0x03000000` Splash Framebuffer Collision Fix**: Default `lk2nd` `LK2ND_BOOT_MEM_SIZE` (`0x03200000` -> `0x83200000`) placed `initramfs` at `0x83126000`, colliding directly with LS-4505's `cont_splash_mem` (`0x83000000..0x83200000`, marked `no-map`). Setting `LK2ND_BOOT_MEM_SIZE=0x03000000` places `initramfs` and DTB cleanly below `0x83000000` in standard lowmem.
4. **Pure `ext2` (`rev 1`, `128-byte` inodes) `/cache` Boot Partition**: `lk2nd`'s `ext2_mount()` enforces `s_feature_ro_compat & ~(SPARSE_SUPER | LARGE_FILE) == 0` (`0x0003`) and `^extents`. Formatting `mmcblk0p23` (`cache`) with `mke2fs -t ext2 -r 1 -I 128 -O filetype,sparse_super,large_file,^resize_inode,^ext_attr,^dir_index` allows **both** `lk2nd` (to read `/extlinux/extlinux.conf`) and Android `3.10.108+` (`CONFIG_EXT4_USE_FOR_EXT23=y`, mounting `/cache` rw without adding `extents`) to share `mmcblk0p23` across unlimited reboots.
5. **Gated UART `earlycon` Bus-Abort Avoidance**: On production LS-4505 hardware, `aboot` leaves `GCC_BLSP1_UART1_APPS_CLK` gated; enabling `earlycon` / `stdout-path = "serial0"` causes a synchronous external abort at `0.000000s` before `ramoops` probes. Using `console=tty0` + `ramoops@ae000000` avoids touching unclocked UART registers while capturing full kernel logs in DRAM.

---

## 4. Mainline Linux `6.12.1` LTS Verified Hardware Matrix

Defined in [`arch/arm/boot/dts/qcom/qcom-msm8909-lyf-ls4505.dts`](../ls4505-tools/phase2-mainline-pmos/mainline-dts/qcom-msm8909-lyf-ls4505.dts) :

| Subsystem | Mainline Driver / DT Binding | Live Status on `6.12.1` |
|---|---|---|
| **SoC / SMP / Power Domains** | `qcom,msm8909-smp`, `qcom,rpm-pm8909-regulators`, `qcom,msm8909-rpmpd` | **Working** (4x Cortex-A7 SMP, all `s2`/`l1..l18` regulators) |
| **Display (`480x854` 60 Hz)** | `simpledrm` (`/dev/dri/card0`) + `lk2nd.pass-simplefb=xrgb8888` + Xorg `modesetting` | **Working** (32-bit `XRGB8888` @ 60.00 Hz) |
| **PWM Backlight** | `pwm-backlight` + `qcom,pm8916-pwm` (`LEDS_QCOM_LPG=y`) on `pm8909_mpps` `mpp2` (`DTEST1`) | **Working** (`/sys/class/backlight/backlight`, `0..255` + Xfce4 brightness slider) |
| **Touchscreen + Capacitive Keys** | `mstar,msg2138` (`TOUCHSCREEN_MSG2638=y`) on `BLSP_I2C5 @ 0x26`, IRQ GPIO 13, RST GPIO 12 | **Working** (`MStar TouchScreen` `2048x2048` full-screen calibrated + Menu/Home/Back) |
| **3-Axis Accelerometer** | `bosch,bma250` (`BMA180=y`) on `BLSP_I2C1 @ 0x18`, IRQ GPIO 96 | **Working** (`/sys/bus/iio/devices/iio:device0`) |
| **Hardware Buttons & Haptics** | `gpio-keys` (`Vol+` GPIO 90), `pm8941-resin` (`Vol-`), `pm8941-pwrkey` (`Power`), `pm8xxx-vibrator` | **Working** (`event0..event3`) |
| **USB Networking + SSH** | `ci_hdrc_msm` + `PHY_QCOM_USB_HS=y` + `pm8909_usbin` extcon + ConfigFS RNDIS | **Working** (`172.16.42.1/24` plug-and-play on Windows/Linux, `sshd` on port 22) |
| **Wi-Fi (`WCN3620` 2.4 GHz)** | `qcom_wcnss_pil` (`wcnss.mdt`) + `wcn36xx` (`WCN v2.0` + factory `WCNSS_qcom_wlan_nv.bin`) | **Working** (`wlan0` AP scanning & WPA2 via `NetworkManager` / `nmtui`) |
| **Bluetooth (`WCN3620`)** | `btqcomsmd` | **Working** (`hci0`) |
| **Storage & Shared Media** | `sdhci_msm` HS200 eMMC (`mmcblk0`) + microSD (`mmc1`, CD GPIO 38) | **Working** (`/data/pmos_root` native ext4 bind-root + `/mnt/android_media` shared storage) |
| **Front & Rear Flash LEDs** | `gpio-leds` (`GPIO 3` front flash, `GPIO 9` rear torch) | **Working** (`white:flash-front`, `white:flash-rear`) |
| **Crash/Console Persistence** | `ramoops@ae000000` (`CONFIG_PSTORE_RAM=y`, `CONFIG_PSTORE_CONSOLE=y`) | **Working** (1 MB @ `0xae000000`) |

---

## 5. Everyday Dual-Boot & SSH Usage

### Switching Operating Systems (Untethered or via USB)
- **From Android -> Mainline Linux `6.12.1` (`Xfce4` Desktop)**:
  - Over ADB: `adb reboot recovery` (or `adb shell su -c boot-linux`)
  - Untethered on phone: run `su -c boot-linux` in a terminal app, **or** power on holding **Volume Up + Power**.
- **From Mainline Linux `6.12.1` -> Ultra-Lean Android `3.10.108+`**:
  - Tap the **`Boot Android`** icon on the Xfce4 touchscreen desktop (single-tap enabled!), or run `sudo reboot` / `reboot-to-android`.
- **Reboot while staying in Mainline Linux `6.12.1`**:
  - Tap the **`Reboot Linux`** icon on the Xfce4 desktop, or run `reboot-to-linux` (invokes `/usr/local/bin/reboot-mode recovery`, setting PM8909 PON `0x01`).

### Connecting via USB Network (`172.16.42.1`) from Windows / Linux
1. Plug the LS-4505 into USB while booted into Mainline Linux `6.12.1`; it enumerates automatically as **`Remote NDIS Compatible Device`**.
2. If Windows has not yet assigned the `172.16.42.x` address on the RNDIS adapter:
   ```powershell
   $idx = (Get-NetAdapter | Where-Object InterfaceDescription -match "Remote NDIS").ifIndex
   New-NetIPAddress -InterfaceIndex $idx -IPAddress 172.16.42.2 -PrefixLength 24
   ```
3. Connect over SSH:
   ```powershell
   ssh -i ls4505_ssh_key root@172.16.42.1
   # or desktop user:
   ssh -i ls4505_ssh_key user@172.16.42.1
   ```
