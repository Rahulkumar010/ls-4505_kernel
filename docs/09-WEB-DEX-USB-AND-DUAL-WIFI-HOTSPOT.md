# 09 ? Zero-Install Browser "Web-DeX" (Universal USB + Dual-Wi-Fi Hotspot & OTG)

Turn the **LYF Flame 8 (`LS-4505`, Mainline Linux `6.12.1` LTS)** into an OS-independent, zero-install desktop computer using **any laptop/desktop's built-in screen, keyboard, and trackpad** ? with **zero client software, zero drivers, and ~85?90 MB total RAM usage** (`806+ MB` free out of `896 MB`).

---

## 1. Architecture Overview

| Component | Implementation | Footprint / Role |
|---|---|---|
| **Virtual Desktop (`:1`)** | `TigerVNC Xvnc 1.13.1` (`-AcceptSetDesktopSize=1 -AlwaysShared=1 -FrameRate=60 -interface 127.0.0.1 -rfbport 5901`) + `vncconfig -nowin` | Native 1:1 `RandR` dynamic resolution matching browser viewport + bidirectional clipboard sync |
| **Desktop Window Manager** | `JWM` (`/etc/jwm/jwmrc-dex`) independent of `:0` (`480x854` touchscreen UI) | Samsung DeX-style bottom taskbar, quick-launch bar, 2 virtual desktops, `Alt+Tab` / `Alt+F4` / `Ctrl+Alt+T` |
| **HTTP + WebSocket Server (`:80`)** | Custom **`dex-webd`** (`dex-webd.c`, static ARMv7 `musl` C binary) + HTML5 `noVNC 1.5.0` (`/usr/share/novnc`) | **`46 KB` binary / `< 0.5 MB` RSS** (replaces `150 MB` Python `websockify` + `numpy` stack; `TCP_NODELAY` + `writev` framing) |
| **Mode 1 ? Universal USB DeX** | USB ConfigFS **`CDC-NCM`** (`0x1d6b:0x0106`, `WINNCM` + USB-IF `Class_02&SubClass_0d`) + `dnsmasq` (`--dhcp-option=3`) | **`426 Mbps`** plug-and-play on **Windows 10/11, macOS, Linux, ChromeOS, iPadOS** without hijacking host internet |
| **Mode 2A ? Wireless Hotspot + OTG Dual-Wi-Fi** | Internal `WCN3620` (`wlan0`) WPA2 AP **`LS4505-DeX`** (`172.16.43.1/24`) + `SMB1360` **5V 1.5A OTG Boost** powering external **Wayona `MT7601U` (`wlan1`)** + `iptables-legacy` NAT `MASQUERADE` | Wireless Web-DeX at `http://172.16.43.1` / `http://dex.lan` while simultaneously using external USB Wi-Fi (`wlan1`) for upstream internet or scanning |

---

## 2. Quick-Start Usage

### Mode 1: Plug-and-Play USB Cable (Default on Boot)
1. Boot **Mainline Linux `6.12.1`** and plug LS-4505 into **any Windows 10/11, macOS, Linux, ChromeOS, or iPadOS** machine via Micro-USB.
2. Wait ~2 seconds for the OS's built-in `CDC-NCM` driver (`UsbNcm` / `AppleUSBNCM` / `cdc_ncm`) to auto-assign `172.16.42.x` (your laptop keeps its own Wi-Fi internet untouched thanks to DHCP `--dhcp-option=3`).
3. Open **any web browser** (Chrome, Edge, Safari, Firefox) and go to:
   - **`http://172.16.42.1`** *(or `http://dex.lan`)*
4. Click **Full Screen (`F11`)** ? the Linux desktop automatically resizes (`RemoteResize`) to your exact laptop screen resolution!

*(For legacy Windows 7/8 PCs without `usbncm.inf`, run `dex-mode usb-rndis` or select **DeX Legacy USB RNDIS** from the phone's `Apps` menu.)*

---

### Mode 2A: Wireless `LS4505-DeX` Hotspot + External Wayona `MT7601U` OTG Dual-Wi-Fi
Because Micro-USB carries the external **Wayona `MT7601U` (`148f:7601`)** USB Wi-Fi adapter via OTG, Mode 2A moves the laptop link onto LS-4505's internal Wi-Fi (`wlan0`) while powering `wlan1` on the OTG port:

1. Unplug the laptop USB cable and tap **`DeX AP`** on the top bar of the phone's touchscreen (or run `dex-mode hotspot`).
2. Plug your **OTG adapter + Wayona `MT7601U`** into LS-4505's Micro-USB port ? `SMB1360`'s 5V 1.5A boost powers the adapter immediately as **`wlan1`**.
3. On your laptop, connect to Wi-Fi:
   - **SSID**: `LS4505-DeX`
   - **Password**: `ls4505dex`
4. Open **`http://172.16.43.1`** *(or **`http://dex.lan`**)* in your laptop browser!
5. Inside Web-DeX, click **`Dual-WiFi`** (`nmtui`) on the bottom taskbar to connect **`wlan1` (Wayona `MT7601U`)** to any upstream router (automatically NAT-shared to your laptop via `iptables-legacy` `MASQUERADE`), or click **Scan External Wayona MT7601U (`wlan1`)** in the `LS-4505 DeX` menu!
6. To switch back to USB mode anytime, tap **`DeX USB`** on the phone's bottom touch bar (or run `dex-mode usb`).

---

## 3. Desktop Keyboard Shortcuts & CLI Reference

### Keyboard Shortcuts (inside Browser Web-DeX)
- **`Ctrl + Alt + T`**: Launch new Terminal (`xterm`)
- **`Alt + Tab`**: Cycle windows
- **`Alt + F4`**: Close active window
- **`Alt + F10`**: Maximize / restore active window
- **`Ctrl + Alt + Left / Right`**: Switch between Desktop 1 & Desktop 2

### CLI Commands (`/usr/local/bin/dex-mode`)
```sh
dex-mode status       # Show active mode, URLs, Xvnc/dex-webd status, USB role, wlan0 & wlan1
dex-mode usb          # Mode 1: Universal CDC-NCM USB Gadget (http://172.16.42.1)
dex-mode usb-rndis    # Mode 1 (Legacy): RNDIS USB Gadget (http://172.16.42.1)
dex-mode hotspot      # Mode 2A: LS4505-DeX Wi-Fi AP (http://172.16.43.1) + 5V OTG Boost for wlan1
dex-mode wlan1-scan   # Scan Wi-Fi APs specifically using external Wayona MT7601U (wlan1)
```

---

## 4. Key Engineering Fixes Discovered During Implementation

1. **Zero-Python `dex-webd` (`46 KB` vs `150 MB`)**:
   - Alpine's `websockify` pulls `python3`, `py3-numpy`, `openblas`, and `libgfortran` (`~150 MB`). Writing `dex-webd.c` in pure C (RFC-6455 binary WebSocket state machine + SHA-1/Base64 handshake + ES6 module `text/javascript` static server + `TCP_NODELAY` / `writev` single-segment framing) cut disk overhead by 99.9% and kept total system RAM with **both `:0` and `:1` desktops running** at **`85?90 MB`**.
2. **Linux ConfigFS `ncm.usb0` Dynamic Netdev Registration Order**:
   - In `drivers/usb/gadget/function/f_ncm.c`, `gether_register_netdev()` only registers `usb0` inside `ncm_bind()` when `$UDC` is written to `/sys/kernel/config/usb_gadget/g1/UDC` (reading `functions/ncm.usb0/ifname` before UDC bind returns `"Unnamed net_device"`). Reading `ifname` *after* `echo "$UDC" > "$G/UDC"` plus marking `usb0`/`usb1` unmanaged in `/etc/NetworkManager/conf.d/99-unmanaged-usb.conf` brings up `426 Mbps` `UsbNcm` reliably in `< 2s`.
3. **Non-Intrusive USB DHCP (`--dhcp-option=3`)**:
   - Sending an empty DHCP Option 3 (Router) on `usb0` ensures Windows/macOS/Linux hosts route `172.16.42.0/24` over USB while keeping their existing Wi-Fi/Ethernet default internet gateway intact.
4. **Alpine `iptables-legacy` vs `xtables-nft-multi` + Automatic NTP Sync**:
   - Installed `iptables-legacy` (`CONFIG_IP_NF_NAT=m` + `CONFIG_NETFILTER_XT_TARGET_MASQUERADE=m`) for `172.16.43.0/24 -> wlan1` NAT forwarding, and added `/etc/local.d/30-ntp-sync.start` to step the read-only PM8909 1970 RTC to UTC/IST automatically as soon as an upstream default route appears.

---

## 5. JavaScript-Enabled Lightweight Web Browser (`BadWolf` + `libbadwolf_js_on.so`)

- **Why Dillo lacked JS**: `Dillo` is a static HTML/CSS viewer (`~8 MB` RAM) with no JavaScript engine.
- **Solution**: Installed **`BadWolf 1.3.0`** backed by the already-present **`webkit2gtk-4.1` (`2.46.3` WebKit + JavaScriptCore)** engine (`0 MiB` extra library bloat, **`~56 MB` RAM** when browsing JS sites ? leaving **`~749 MB` RAM available**!).
- **Default-ON `[JS]` Toggle (`libbadwolf_js_on.so` + `/usr/local/bin/dex-browser`)**:
  - Upstream BadWolf defaults its per-tab `[JS]` toolbar button to `OFF`.
  - `libbadwolf_js_on.c` (`5 KB` `LD_PRELOAD` helper) activates the `[JS]` toggle automatically upon tab creation (while keeping the `[JS]` toolbar button clickable anytime you want to disable JS on a tab), and `dex-browser` sets `WEBKIT_DISABLE_DMABUF_RENDERER=1` + `WEBKIT_DISABLE_COMPOSITING_MODE=1` for tear-free software rendering on `Xvnc` & `SimpleDRM`.
  - Bound to the **`Browser`** taskbar button in Web-DeX (`jwmrc-dex`), with `Dillo` retained in the **`LS-4505 DeX`** menu as *Lite Static Browser (Dillo)*.

---

## 6. Web-DeX Password Protection & Session Security (`dex-passwd` + `dex-webd`)

- **Full-Length Salted-SHA1 Gate (`/etc/dex-webd.auth`, `0600`)**:
  - Protects both the HTML5 UI (`/`, `/vnc.html`) and the `/websockify` RFC-6455 WebSocket endpoint (`401 Unauthorized` without a valid session token).
  - Supports arbitrary password lengths and characters (no 8-character classic VNC truncation) with a 1-second brute-force delay on failed `POST /login` attempts.
  - Issues an `HttpOnly; SameSite=Strict` session cookie (`dex_session`) derived from a 256-bit boot secret + `/run/dex-webd.epoch`, preserving seamless `noVNC` auto-reconnects while allowing instant server-side session revocation via `/logout` or `dex-passwd --lock`.
- **Dual-OS Bridge Upgrade (`0x1d6b:0x0107`, `br0`)**:
  - Upgraded Mode 1 USB gadget to expose `c.1` (`rndis.usb0` + `MSFT100` `os_desc` for Windows 7/8/10/11 ? immune to `UsbNcm.sys` idle RX stalls) and `c.2` (`ecm.usb0` `CDC-ECM` for macOS, Linux, ChromeOS, and iPadOS) bridged together under `br0` (`172.16.42.1/24`), plus disabled `wcn36xx` BMPS power-save (`iw dev wlan0 set power_save off`).
- **Managing Web-DeX Password**:
  ```sh
  dex-passwd                 # Change Web-DeX password interactively
  dex-passwd <new_password>  # Set Web-DeX password non-interactively
  dex-passwd --lock          # Immediately lock all open Web-DeX browser tabs
  dex-passwd --disable       # Disable password gate
  ```
