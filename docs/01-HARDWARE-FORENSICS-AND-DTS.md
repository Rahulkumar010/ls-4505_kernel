# 01 — Hardware Forensics & Device Tree Architecture (`LS-4505`)

This document records the ground-truth hardware configuration extracted from the live **LYF Flame 8 (`LS-4505`)** via ADB root forensics, stock DTB decompilation, and `emmc_appsboot.mbn` (`aboot`) ARM disassembly.

---

## 1. Platform & Bootloader (`aboot`) Identification

| Property | Value |
| :--- | :--- |
| **Device / Model** | LYF Flame 8 (`LS-4505`), ODM board `MSM8909-PM8909 1GB QRD SKUE CKT` |
| **SoC / PMIC** | Qualcomm Snapdragon 210 (`MSM8909` v2.0, `soc_id = 245`, `4x Cortex-A7` @ 1.1 GHz) + `PM8909` |
| **Architecture / RAM** | 32-bit `ARMv7` (`VMSPLIT_3G`), `1024 MB` LPDDR2/3 (`0x80000000 – 0xc0000000`) |
| **Stock OS & Kernel** | Android `6.0.1` (`MMB29M`, SELinux policy `v30`), Linux `3.10.49` (`Sep 30, 2016`), GCC `4.8` |
| **DT `qcom,msm-id`** | `<0xf5 0x0 0x102 0x0 0x109 0x0 0x113 0x0>` (`245`, `258`, `265`, `275`) |
| **DT `qcom,board-id`** | `<0x1000b 0x0>` (`hw_plat = 11` QRD, `hw_plat_ver = 0x10000` SKUE) |
| **DT `qcom,pmic-id`** | `<0x1000d 0x0 0x0 0x0>` (`PM8909`) |

### `emmc_appsboot.mbn` (`aboot` @ `0x8f600000`) Disassembly Findings
Disassembling the stock Little Kernel (`LK` / `aboot`) binary revealed critical boot mechanics:
1. **Hardcoded Load Addresses**: Regardless of the `boot.img` header fields, `aboot` overrides load addresses on `MSM8909` to:
   - `kernel_addr  = 0x80008000`
   - `tags_addr    = 0x81e00000` (DTB)
   - `ramdisk_addr = 0x82000000`
2. **Appended DTB Mandatory (`dt_size == 0`)**: Stock `boot.img` sets `dt_size = 0` in the Android boot header and appends the DTB table directly to the end of `zImage` (`zImage-dtb`, `CONFIG_ARM_APPENDED_DTB=y` + `CONFIG_BUILD_ARM_APPENDED_DTB_IMAGE=y`). Booting a bare `zImage` without an appended matching DTB causes `aboot` to fail DTB selection immediately.
3. **Unsigned Boot Images Permitted**: Even though `devinfo` (`mmcblk0p25`) reports `is_unlocked = 0`, the `keystore` partition (`mmcblk0p26`) is completely empty, so `aboot`'s signature verification falls through and boots unsigned custom `boot.img` and `recovery.img` payloads natively.
4. **Identical `boot` vs `recovery` Kernel Loading**: When `adb reboot recovery` sets `PON_RESTART_REASON_RECOVERY` (`0x01`), `aboot` loads `mmcblk0p24` (`recovery`) using the exact same normal boot path if the image inside `mmcblk0p24` contains a normal boot ramdisk — enabling **100% risk-free one-shot kernel testing** without touching `mmcblk0p20` (`boot`).

---

## 2. eMMC Partition Layout (`7824900.sdhci` / `mmcblk0`)

Key partitions on the 8 GB eMMC (`F722M`):

| Partition | Device Node | Size | Purpose |
| :--- | :--- | :--- | :--- |
| `modem` | `/dev/block/mmcblk0p1` | 64 MB | `NON-HLOS.bin` LTE/ril/WCNSS firmware |
| `sbl1` / `rpm` / `tz` | `mmcblk0p6` / `p10` / `p12` | — | Primary bootloader, RPM, TrustZone |
| `aboot` | `/dev/block/mmcblk0p16` | 1 MB | `emmc_appsboot.mbn` Little Kernel bootloader |
| **`boot`** | **`/dev/block/mmcblk0p20`** | **32 MB** | **Primary Android kernel + Magisk ramdisk (`start_sector=396384`)** |
| `system` | `/dev/block/mmcblk0p21` | ~2.0 GB | Android 6.0.1 system + `/system/lib/modules/*.ko` |
| `persist` | `/dev/block/mmcblk0p22` | 32 MB | WCNSS Wi-Fi/BT MAC & sensor calibration |
| `cache` | `/dev/block/mmcblk0p23` | 256 MB | Android cache |
| **`recovery`** | **`/dev/block/mmcblk0p24`** | **32 MB** | **Recovery partition (used for safe one-shot kernel testing)** |
| `devinfo` / `keystore` | `mmcblk0p25` / `p26` | — | Bootloader lock state / empty signing keystore |
| `misc` | `/dev/block/mmcblk0p29` / `p27` | — | Bootloader Control Block |
| `userdata` | `/dev/block/mmcblk0p29` (`p30`) | ~4.35 GB | User data + `/data/adb` Magisk state |

---

## 3. Physical RAM & CMA Carveout Map (`1024 MB`)

```text
0x80000000 – 0x879fffff : System RAM (Lowmem bank 0)
  0x80008000 – 0x80fb507c : Kernel .text / .rodata
  0x81110000 – 0x81375cb8 : Kernel .data / .bss
  0x81e00000              : Flattened Device Tree (ATAGS/DTB)
  0x82000000              : Initramfs / Magisk ramdisk (~1.05 MB)
  0x83000000 – 0x83bfffff : cont_splash_mem (12 MiB continuous splash framebuffer)
0x87a00000 – 0x8d9fffff : Carved-out Non-System-RAM Hole (Modem / PIL / Pronto)
  0x87a00000 – 0x87ffffff : external_image_mem (6 MiB)
  0x88000000 – 0x8d4fffff : modem_adsp_mem (85 MiB PIL region)
  0x8d500000 – 0x8d9fffff : peripheral_mem / WCNSS Pronto (5 MiB)
0x8da00000 – 0xbfffffff : System RAM (Lowmem bank 1 + 266 MB HighMem)
  0x8f800000 – 0x8fffffff : venus_qseecom_mem (8 MiB CMA)
  0xae000000 – 0xae0fffff : (Optional debug branch) ramoops pstore (VA 0xee000000)
  0xaf000000 – 0xaf7fffff : default CMA region (8 MiB)
  0xbfc00000 – 0xbfffffff : audio_mem (4 MiB CMA)
```

---

## 4. Verified Peripheral & I2C / GPIO Hardware Map

Auditing `/sys/bus/i2c/devices/*/name`, `/sys/kernel/debug/gpio`, and `stock_dtbdump_MSM8909_PM8909_1GB_QRD_SKUE_CKT.dts` revealed why generic QRD or Alcatel MSM8909 device trees failed on the LS-4505:

| Subsystem | Bus / Controller | Address / GPIOs | Driver & DT Compatible | Notes vs Generic QRD |
| :--- | :--- | :--- | :--- | :--- |
| **Display Panel** | `mdss_dsi0` | Reset `GPIO 25`, Backlight PMIC MPP / `lcd-backlight` | `qcom,mdss_dsi_ili9806e_txd_fwvga_ips_video` (`480x854` IPS video mode) | Exact `_ips_video` suffix required to match `aboot` cmdline `mdss_mdp3.panel=...` |
| **Touchscreen** | `I2C5` (`78b9000.i2c`) | **`0x26`**, RST `GPIO 12` (`923`), IRQ `GPIO 13` (`924`) | **MStar `MSG2238`** (`mstar,msg2xxx`), virtualkeys `MENU`/`HOME`/`BACK` (`y=900`) | Generic QRD used Focaltech `ft5x06@38`; LS-4505 uses MStar `msg2xxx@26` (`chip_type=0x7a`) |
| **Charger + Fuel Gauge** | `I2C4` (`78b8000.i2c`) | **`0x14`**, STAT IRQ `GPIO 65` | **Summit `SMB1360`** (`qcom,smb1360-chg-fg`) + `batterydata-lyf-ck2000.dtsi` | Uses external I2C4 SMB1360 (revision `0x03`) + OTG vreg alongside PM8909 |
| **Accelerometer** | `I2C1` (`78b5000.i2c`) | **`0x18`**, INT `GPIO 96` | **Bosch `BMA250`** (`bosch,bma2x2`, chip ID **`0x03`**) | Stock CAF `bma2x2.c` lacked `{0x03, BMA250E_TYPE, "BMA250"}` in `sensor_type_map[]` |
| **Light & Proximity** | `I2C1` (`78b5000.i2c`) | **`0x49`**, IRQ `GPIO 94` | **ELAN `EPL259x`** (`epl,epl259x`) | Absent from stock CAF tree; ported with `"epl259x,irq-gpio"` DT binding |
| **Rear Camera (8 MP)** | `CCI / I2C3` (`78b7000.i2c`) | `ov8856_1608ste_chicony` + Actuator `@18` + EEPROMs `@20,@21,@22` | `qcom,camera` (`msm_sensor` + `msm_actuator` + `msm_eeprom`) | Paired with rear GPIO LED flash |
| **Front Camera (5 MP)** | `CCI / I2C3` (`78b7000.i2c`) | `s5k5e2_front_55a0_8909` | `qcom,camera` | Paired with dedicated front selfie flash |
| **Dual LED Flash** | Platform GPIO | Rear `GPIO 3` (`flash_en`), Torch/Front `GPIO 9` (`flash_now` / `enable_multi`) | `qcom,camera-led-flash` (`drivers/leds/leds-msm-gpio-flash.c`) | Extended driver to parse 3-element `qcom,op-seq` (`flash_en`, `flash_now`, `enable_multi`) |
| **Audio Codec & DAI** | SPMI + QDSP6 APR | `msm8x16_wcd_codec` + `sound.70` | `qcom,msm8909-audio-codec` (`msm8909-skue-snd-card`) | Requires `<&bt_a2dp_rx>` (`msm-dai-q6-dev.12290`) and CAF bidirectional compress DAI |
| **Wi-Fi / BT / FM** | `a21b000.qcom,pronto` | WCN3620 (`WCN v2.0`, 19.2 MHz XO) | Out-of-tree Prima `wlan.ko` (`pronto_wlan.ko` `v3.0.10.085`) + SMD BT/FM | Loaded as `/system/lib/modules/wlan.ko` -> `pronto/pronto_wlan.ko` |
| **ION Heaps** | `qcom,msm-ion` | Heaps `25` (`SYSTEM`), **`21` (`SYSTEM_CONTIG`)**, `27` (`QSEECOM`), `28` (`AUDIO`), `29` (`PIL_2`) | `drivers/staging/android/ion/msm/` | Heap `21` (`SYSTEM_CONTIG`) explicitly required in `msm8909-lyf-ls4505.dts` |
