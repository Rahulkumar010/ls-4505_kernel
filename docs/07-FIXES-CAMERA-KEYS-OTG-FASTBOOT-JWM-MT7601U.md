# Post-Deployment Fixes & Optimizations: Camera OTP, Capacitive Keys, USB OTG, `lk2nd` Fastboot, Ultra-Light `JWM`, and `MT7601U` Dual Wi-Fi

This document records all root-cause analyses, fixes, and verifications across **Ultra-Lean Android `3.10.108+` (`#12`)**, **`lk2nd`**, and **Mainline Linux `6.12.1` LTS (`#5`)**.

---

## 1. Front Camera Red Tint Fix (`drivers/media/platform/msm/camera_v2/sensor/msm_sensor.c`)

- **Symptom**: Rear camera (`OV8856`) displayed accurate colors, while front-facing camera (`S5K5E2`) rendered with a strong red cast and logged `E mm-camera-CORE: awb_bayer_set_led_gains AWB decision invalid`.
- **Root Cause**: Disassembling `msm_sensor_match_id()` (`0xc064c9c0..0xc064d430`) from the stock `3.10.49` `zImage` revealed that LYF shipped multiple `S5K5E2` front camera module vendors sharing the same sensor chip ID (`0x5e20` at CCI slave `0x20`):
  - **Season (`s5k5e2_front_55a0_8909`)**: OTP Page 2 `0x0a04` (`eepromRid`) == `0x10`
  - **Viewfly (`s5k5e2_front_55a0_8909_viewfly`)**: `eepromRid` == `0x00` (different `0x0101` mirror/flip Bayer CFA alignment)
  Without reading OTP `0x0a04` inside `msm_sensor_match_id()`, `s5k5e2_front_55a0_8909` falsely matched first on Viewfly hardware, shifting the Bayer CFA phase and producing a red image.
- **Fix & Verification**: Restored the exact OTP `eepromRid` checks in `msm_sensor_match_id()` for both `S5K5E2` (`0x10`) and `OV8856` (`0x06`/`0x05`/`0x16`). Boot `dmesg` confirmed `<eeprom> eepromRid:0x0 -> s5k5e2_front_55a0_8909_viewfly probe succeeded`, loading `libchromatix_s5k5e2_front_55a0_8909_viewfly_preview.so` with 100% accurate colors.

---

## 2. Capacitive Bottom Button Remapping (`Left=Back, Middle=Home, Right=Recents`)

- **Root Cause**: `MSG2238` reports hardware touch-key codes `4` (Left), `1` (Middle), and `2` (Right), which `mstar_drv_self_fw_control.c` maps to `g_TpVirtualKey[0]`, `[2]`, and `[1]`. Default `{TOUCH_KEY_MENU, TOUCH_KEY_HOME, TOUCH_KEY_BACK}` produced `Left=Recents, Middle=Back, Right=Home`.
- **Fix**:
  - **Android `3.10.108+`**: Updated `g_TpVirtualKey[]` in `mstar_drv_platform_porting_layer.c` to `{TOUCH_KEY_BACK, TOUCH_KEY_MENU, TOUCH_KEY_HOME}` -> **Left=`BACK` (`158`), Middle=`HOME` (`172`), Right=`APP_SWITCH` (`139`)**.
  - **Mainline `6.12.1`**: Updated `linux,keycodes` in `qcom-msm8909-lyf-ls4505.dts` (`ffs(y_low)-1` indexing `1`->`[0]`, `2`->`[1]`, `4`->`[2]`) to `<KEY_HOMEPAGE KEY_APPSELECT KEY_BACK>`.

---

## 3. Hands-Free Unlocked `lk2nd` Fastboot (`boot-fastboot` / `reboot-to-fastboot`)

- **Root Cause**: Stock LYF `emmc_appsboot.mbn` compiled out all `fastboot` commands except `getvar:` and `download:`.
- **Fix**: Added [`0001-lk2nd-misc-oneshot-fastboot-trigger.patch`](../ls4505-tools/phase2-mainline-pmos/lk2nd/0001-lk2nd-misc-oneshot-fastboot-trigger.patch) so `lk2nd` checks the first 16 bytes of `misc` (`mmcblk0p13`) for `"lk2nd-fastboot"`, immediately zeroes those 16 bytes (one-shot safety), and enters **unlocked `lk2nd` Fastboot** (`product: lk2nd-msm8909`, `flash`, `boot`, `continue`, `erase`, `reboot` all enabled).
- **Usage**:
  - From Android: `su -c boot-fastboot`
  - From Mainline Linux: `reboot-to-fastboot` (or tap **Apps -> Boot lk2nd Fastboot** in JWM)

---

## 4. USB OTG + `SMB1360` 5V Boost & External `Wayona` (`MT7601U`, `148f:7601`) Dual Wi-Fi

- **Root Cause**:
  1. In `3.10.108` (`phy-msm-usb.c`), `OTG_PMIC_CONTROL` wrote `OTG_COMP_DISABLE` to `ULPI_PWR_CLK_MNG_REG` (shutting off the USB PHY's `ID` comparator even though `PM8909` lacks a PMIC `ID` pin!) and hid `/sys/kernel/debug/msm_otg/mode`.
  2. In Mainline `6.12.1`, `&usb` was `dr_mode = "peripheral"` without `usb-role-switch;` or `&blsp_i2c4` (`SMB1360` `@ 0x14` 5V OTG boost).
- **Fixes**:
  - **Android `3.10.108+`**: Kept the SNPS Femto PHY `ID` comparator + internal `IDPU` pull-up active when `mode == USB_OTG && phy_irq && !pmic_id_irq`, and exposed `/sys/kernel/debug/msm_otg/mode` + `/system/xbin/otg-mode` (`otg-mode host` / `otg-mode peripheral`).
  - **Mainline `6.12.1` (Best OS for Simultaneous Dual Wi-Fi!)**:
    - Enabled `dr_mode = "otg"; usb-role-switch;` (`/sys/class/usb_role/ci_hdrc.0-role-switch/role`), enabled `&blsp_i2c4` (`i2c-1` -> `SMB1360` `@ 0x14`), built `CONFIG_MT7601U=m` (`mt7601u.ko`), and installed `/lib/firmware/mt7601u.bin` + `networkmanager-cli` (`nmcli`) + `hostapd` + `usbutils`.
    - Added `/usr/local/bin/otg-mode` (and **`OTG`** button right on the JWM top bar!):
      - `sudo otg-mode host`: switches ChipIdea to `host`, enables **SMB1360 5V 1.5A VBUS boost** (`reg 0x42 |= 0x01`), and loads `mt7601u` so your **Wayona `148f:7601` USB Wi-Fi (`wlan1`)** runs simultaneously alongside built-in **`WCN3620` (`wlan0`)**!
      - `sudo otg-mode gadget`: disables 5V boost and restores USB RNDIS (`172.16.42.1`) SSH to PC.

---

## 5. Ultra-Light `JWM` Graphical Touch UI (`~66?71 MB` Total OS + GUI RAM!)

- Replaced `LightDM` + `Xfce4` + `elogind` (`~140+ MB` RAM) with **JWM (`jwm-2.4.6`)** launched directly via OpenRC `jwm-desktop`:
  - **Total system + Xorg + JWM + NetworkManager + SSHD RAM**: **`66?71 MB / 896 MB`** (**`825?829 MB` available ? `92.5%` RAM free!**).
  - Dual touch toolbars: Top bar (`Apps`, `Kbd`, `Term`, `OTG`, active window list, clock) + Bottom bar (`Wi-Fi`, `Web`, `Files`, `Android`).

---

## 6. Android `ls4505_ultralean` v2 (`69` System Packages) & Selective Restore

- Masked **91 stock directories** systemlessly via Magisk `.replace` (including `Videos`, `Mms`, `Music`, `CMFileManager`, `DownloadProviderUi`, `Contacts`, `SimContacts`, `Chrome`, `Gallery2`, `SoundRecorder`, all `LiveWallpapers*`, `PrintSpooler`, `Stk`, etc., while preserving `WebViewGoogle`, `DocumentsUI` SAF picker, `DownloadProvider`, `ContactsProvider`, and `SnapdragonCamera`).
- Installed lightweight open-source replacements from F-Droid: **TermOne Plus** (`com.termoneplus`), **Material Files** (`me.zhanghai.android.files` with root `/system` & `/data` support), and **SmartCookieWeb** (`com.cookiegames.smartcookie`).
- **Restore any masked stock app at any time**:
  ```sh
  su -c "restore-system-app --list"
  su -c "restore-system-app Chrome"        # restores Chrome on next reboot
  su -c "restore-system-app Contacts Mms"  # restores Contacts & Messaging
  su -c "restore-system-app --all"         # disables entire debloat module
  ```
