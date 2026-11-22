# 03 — Zero-Xubuntu Safe Testing, Live `ramoops` Debugging & Permanent Flashing

This document explains how to **test any experimental kernel on the LS-4505 with zero risk of bricking and without booting Xubuntu/EDL**, how to extract **early-boot crash stack traces over the air from physical RAM (`ramoops`) without soldering UART**, how to **permanently flash `boot` (`mmcblk0p20`) and restore stock `recovery` (`mmcblk0p24`)**, and how to use **Xubuntu EDL (`qdl`)** for emergency unbricking.

---

## 1. Zero-Xubuntu Risk-Free One-Shot Testing via `recovery` (`mmcblk0p24`)

### Why This Works Safely
1. Your primary Android `boot` partition (`/dev/block/mmcblk0p20`, 32 MB) remains **100% untouched** during testing.
2. Using Magisk's on-device `/data/adb/magisk/magiskboot`, we unpack `mmcblk0p20` into `/data/local/tmp/repack108`, swap in the new `zImage` (`kernel`) and `msm8909-lyf-ls4505.dtb` (`kernel_dtb`) while keeping the exact working Magisk-patched ramdisk + `CMDLINE`, repack `boot-ls4505-3.10.108.img`, and `dd` it **ONLY** to `/dev/block/mmcblk0p24` (`recovery`).
3. Running `adb reboot recovery` sets PMIC PON restart reason `0x01` (`PON_RESTART_REASON_RECOVERY`) for **one boot only**:
   - `aboot` loads our test kernel + normal Android Magisk ramdisk from `mmcblk0p24` (`recovery`) and boots straight into full Android!
   - If the experimental kernel ever hangs, panics (`CONFIG_PANIC_TIMEOUT=5`, `download_mode = 0` in `msm-poweroff.c`), or if you hold the Power button for ~10 seconds to hard power-cycle, PMIC PON resets to `0x00` and the phone **automatically boots right back into your untouched working `mmcblk0p20` (`boot`) Android OS**!

### Step-by-Step Safe Test Commands (Windows PowerShell or Linux Host over ADB)

```bash
# 1. Push your newly built zImage, DTB, matching pronto_wlan.ko, and test script
adb push zImage /data/local/tmp/zImage-3.10.108
adb push msm8909-lyf-ls4505.dtb /data/local/tmp/ls4505-3.10.108.dtb
adb push pronto_wlan.ko /data/local/tmp/pronto_wlan-3.10.108.ko
adb push ls4505-tools/scripts/test_boot_via_recovery.sh /data/local/tmp/

# 2. Repack against mmcblk0p20 and write ONLY to mmcblk0p24 (recovery)
adb shell "su -c '/sbin/.magisk/busybox/sh /data/local/tmp/test_boot_via_recovery.sh'"

# 3. (Optional) Pull a copy of the repacked 32 MB boot image to your PC
adb pull /data/local/tmp/repack108/boot-ls4505-3.10.108.img ./boot-ls4505-3.10.108.img

# 4. One-shot boot into the test kernel!
adb reboot recovery
```

### Kernel-Version-Aware `pronto_wlan.ko` Bind-Mount
Because `3.10.108` updates `struct sk_buff` / `cfg80211` symbol CRCs (`114/271` differ from stock `3.10.49`), `test_boot_via_recovery.sh` installs:
- `/data/adb/ls4505-3.10.108/pronto_wlan.ko` (`chmod 0644`, SELinux context `u:object_r:system_file:s0`)
- `/data/adb/post-fs-data.d/10_ls4505_modules.sh`:
  ```sh
  #!/sbin/.magisk/busybox/sh
  read KREL < /proc/sys/kernel/osrelease
  case "$KREL" in
    3.10.108*)
      /sbin/.magisk/busybox/mount -o bind /data/adb/ls4505-3.10.108/pronto_wlan.ko /system/lib/modules/pronto/pronto_wlan.ko
      ;;
  esac
  ```
- **Result**: System `/system` partition is never modified! When booted into `3.10.108*`, `/system/lib/modules/wlan.ko` -> `pronto/pronto_wlan.ko` serves the matched `3.10.108+` module; if you ever boot `3.10.49`, it automatically serves the stock `3.10.49` module!

---

## 2. Live Over-the-Air Crash Debugging (`ramoops` + `dump_ramoops.ko` — No UART Needed)

Stock LS-4505 has no exposed UART cable, `# CONFIG_DEVMEM is not set`, and `# CONFIG_PSTORE is not set` in the stock `3.10.49` kernel. When developing risky kernel upgrades (`testing/ls4505-3.10.108-ramoops-debug`), we engineered a **zero-hardware, hands-free crash capture loop**:

1. **In the test kernel** (`testing/ls4505-3.10.108-ramoops-debug`):
   - `arm_memblock_init()` (`arch/arm/mm/init.c`) reserves 1 MiB at physical `0xae000000` (`memblock_remove(0xae000000, 0x100000)`).
   - `fs/pstore/ram.c` (`CONFIG_PSTORE=y`, `CONFIG_PSTORE_CONSOLE=y`, `CONFIG_PSTORE_RAM=y`) defaults to `mem_address = 0xae000000`, `mem_size = 0x100000`, `record_size = 0x40000`, `ramoops_console_size = 0x40000`.
   - `drivers/power/reset/msm-poweroff.c` sets `download_mode = 0` (preventing Qualcomm `9006` black-screen Sahara hang), forces `PON_POWER_OFF_WARM_RESET` (preserving DRAM across reboot!), and writes `PON_RESTART_REASON_UNKNOWN` (`0x00`) + `0x77665501` on `in_panic` so a panic on `mmcblk0p24` **auto-reboots straight into working `mmcblk0p20` in 5 seconds**!
2. **Reading the crash log after auto-reboot** (`ls4505-tools/dump_ramoops/dump_ramoops.c`):
   - On 32-bit ARM MSM8909 (`PHYS_OFFSET = 0x80000000`, `PAGE_OFFSET = 0xc0000000`, 760 MB lowmem), physical `0xae000000` is **already linearly mapped at virtual address `0xee000000`**!
   - `dump_ramoops.ko` imports only `module_layout` and `printk`, reads `(const char *)0xee000000UL`, prints every line of the captured `Oops#1` + `console [pstore-1]` ring buffer to `dmesg`, and returns `-EAGAIN` so it unloads itself immediately!

---

## 3. Verifying All Hardware Subsystems Before Permanent Flash

Once booted into your test kernel on `mmcblk0p24`, run the two verification scripts over ADB:

```bash
adb push ls4505-tools/scripts/hw_audit.sh /data/local/tmp/
adb push ls4505-tools/scripts/test_wifi_and_suspend.sh /data/local/tmp/
adb shell "su -c '/sbin/.magisk/busybox/sh /data/local/tmp/hw_audit.sh'"
adb shell "su -c '/sbin/.magisk/busybox/sh /data/local/tmp/test_wifi_and_suspend.sh'"
```

Confirm:
- `uname -r` reports `3.10.108+`
- `boot_completed=1`, `baseband=MPSS.JO...`, `wlan_status=ok`
- `lsmod` shows both `wlan` and `core_ctl`
- All 10 I2C devices (`bma2x2`, `EPL259x`, `smb1360-chg-fg`, `msg2xxx`, cameras, actuator, EEPROMs) are bound to drivers
- ALSA sound card `0 [msm8909skuesndc]: msm8909-skue-snd-card` is registered
- `/sys/kernel/debug/wakeup_sources` reads cleanly (`~195` lines, zero `dmesg` warnings)
- Two full screen-off/screen-on suspend/resume cycles succeed

---

## 4. Complete Permanent Installation (`mmcblk0p20` Overwrite + Stock `recovery` Restore)

Once verified 100% on `mmcblk0p24`, permanently overwrite `boot` (`mmcblk0p20`) and restore stock `recovery` (`mmcblk0p24`):

### Method A — Live Over ADB Root (Fastest, No EDL Needed)

```bash
# Push stock recovery backup (recovery-orginal.img) and permanent installer script
adb push recovery-orginal.img /data/local/tmp/recovery-orginal.img
adb push prebuilt/boot-ls4505-3.10.108.img /data/local/tmp/boot-ls4505-3.10.108.img

adb shell "su -c '
  dd if=/data/local/tmp/boot-ls4505-3.10.108.img of=/dev/block/mmcblk0p20 bs=1048576 &&
  dd if=/data/local/tmp/recovery-orginal.img of=/dev/block/mmcblk0p24 bs=1048576 &&
  sync &&
  /sbin/.magisk/busybox/md5sum /data/local/tmp/boot-ls4505-3.10.108.img /dev/block/mmcblk0p20 &&
  rm -f /data/local/tmp/boot-ls4505-3.10.108.img /data/local/tmp/recovery-orginal.img
'"
adb reboot
```
*(Or run `ls4505-tools/scripts/flash_permanent_boot.sh` immediately after `test_boot_via_recovery.sh`.)*

### Method B — Direct Hardware Overwrite via Qualcomm EDL (`qdl` on Xubuntu/Linux)

Put the LS-4505 into Qualcomm Emergency Download Mode (`9008` — `adb reboot edl`, or hold Vol+ & Vol- while plugging in USB) and run from your `Firmware/` folder containing `prog_emmc_firehose_8909_ddr.mbn`, `prebuilt/boot-ls4505-3.10.108.img`, and `ls4505-tools/edl/*.xml`:

```bash
# Flash ONLY the upgraded 3.10.108+ boot partition (preserves system, persist & userdata):
sudo ../qdl/build/qdl prog_emmc_firehose_8909_ddr.mbn rawprogram_boot_3.10.108.xml

# Emergency Revert 1 — Restore original Magisk-rooted stock 3.10.49 boot partition only:
sudo ../qdl/build/qdl prog_emmc_firehose_8909_ddr.mbn rawprogram_restore_magisk.xml

# Emergency Revert 2 — Complete factory firmware overwrite (full unbrick):
sudo ../qdl/build/qdl prog_emmc_firehose_8909_ddr.mbn rawprogram_unsparse.xml patch0.xml
```
