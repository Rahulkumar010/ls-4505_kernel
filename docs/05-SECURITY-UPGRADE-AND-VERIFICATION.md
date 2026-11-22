# 05 — Security Upgrades (`3.10.108` + Post-EOL CVEs) & Hardware Verification Report

This document summarizes the security hardening applied to the LYF Flame 8 (`LS-4505`) kernel and records the live on-device verification results from `Linux 3.10.108+ #10`.

---

## 1. Upstream Linux `v3.10.49 -> v3.10.108` LTS Upgrade (59 Releases)

All **59 upstream `linux-stable` point releases** (`v3.10.50` through `v3.10.108`, ~3,515 upstream commits across ~2,500 files) were 3-way merged onto our verified `ls4505-3.10.49` (`CAF LA.BR.1.2.7_rb1.28`) kernel tree, resolving all 56 conflict files hunk-by-hunk so no Qualcomm MSM functionality was lost.

### Notable Upstream Security Vulnerabilities Resolved via `v3.10.108`
| CVE | Subsystem | Vulnerability Mitigated |
| :--- | :--- | :--- |
| **CVE-2016-5195** | `mm/memory.c`, `mm/gup.c` | **Dirty COW** (`FOLL_COW`) local privilege escalation race condition in copy-on-write page fault handling |
| **CVE-2017-1000364** | `mm/memory.c`, `mm/mmap.c` | **Stack Clash** (`vm_start_gap`) stack guard-page bypass into adjacent VMA mappings |
| **CVE-2017-7308** | `net/packet/af_packet.c` | `PACKET_RX_RING` signed integer overflow heap out-of-bounds write |
| **CVE-2017-8890** | `net/ipv4/inet_connection_sock.c` | Double-free of `mc_list` across `accept()` in `inet_csk_clone_lock()` |
| **CVE-2016-8655** | `net/packet/af_packet.c` | Race condition between `packet_set_ring` and `packet_setsockopt` (`tp_version`) |
| **CVE-2016-7097** | `fs/posix_acl.c`, `fs/*/acl.c` | `setgid` bit preservation privilege escalation when setting POSIX ACLs (`posix_acl_update_mode`) |
| **CVE-2016-3134 / 4997** | `net/ipv4/netfilter/*_tables.c` | Local root via crafted `iptables`/`ip6tables`/`arptables` entry offsets (`xt_check_entry_offsets`) |
| **CVE-2016-5829** | `drivers/hid/usbhid/hiddev.c` | Heap buffer overflow in `hiddev_ioctl_usage` (`HIDIOCGUSAGES` / `HIDIOCSUSAGES`) |
| **CVE-2016-0728** | `security/keys/` | Keyring reference-count overflow Use-After-Free local root |
| **CVE-2016-2544 / 2546** | `sound/core/timer.c` | ALSA timer slave instance race condition & Use-After-Free |
| **CVE-2015-3339** | `fs/exec.c` | `prepare_binprm` / `chown` setuid race condition (`bprm_fill_uid`) |
| **CVE-2014-3687 / 2015-3636** | `net/ipv4/ping.c` | **PingPongRoot** `ping_unhash` rwlock / `sk_nulls_node_init` Use-After-Free local root |

---

## 2. Post-EOL High-Severity Android & Network CVE Backports

Because Linux `3.10.108` reached upstream EOL in November 2017, we additionally audited and backported high-impact post-2017 Android/Linux kernel exploits:

| CVE | Name / Vector | Backported Fix in This Repository |
| :--- | :--- | :--- |
| **CVE-2019-2215** | **BadBinder** — Local Android privilege escalation (actively exploited in the wild on 3.10/3.18/4.4 devices) | `drivers/staging/android/binder.c` (`binder_free_thread`): Added `if (waitqueue_active(&thread->wait)) { wake_up_poll(&thread->wait, POLLHUP \| POLLFREE); synchronize_rcu(); }` before `kfree(thread)` so `epoll` waitqueues safely detach via `POLLFREE` + RCU grace period instead of dereferencing freed `binder_thread` memory. |
| **CVE-2019-11477** | **SACK Panic** — Remote unauthenticated kernel panic / RCE over Wi-Fi or LTE | `net/ipv4/tcp_input.c` (`tcp_shift_skb_data`): Guarded `if (unlikely(tcp_skb_pcount(prev) + pcount > 65535)) goto fallback;` to prevent `u16` `gso_segs` overflow. |
| **CVE-2019-11478** | **SACK Slowness / Memory Exhaustion** — Remote TCP SACK fragmentation DoS | `net/ipv4/tcp_output.c` (`tcp_fragment`): Bounded `sk->sk_wmem_queued` fragmentation overhead when `sk_wmem_queued >> 1 > sk_sndbuf`. |
| **CVE-2019-11479** | **Excess Resource Consumption Low MSS** — Remote crafted TCP MSS DoS | `net/ipv4/tcp_output.c` (`__tcp_mtu_to_mss`): Enforced minimum MSS clamp of `48 + MAX_TCP_OPTION_SPACE` (`88` bytes before subtracting TCP options, guaranteeing >= 48-byte payload). |
| **CVE-2017-1000251** | **BlueBorne** — Over-the-air unauthenticated Bluetooth L2CAP RCE | `net/bluetooth/l2cap_core.c`: Added explicit `size_t endptr` stack-buffer bounds validation in `l2cap_add_conf_opt()`, `l2cap_add_opt_efs()`, `l2cap_build_conf_req()`, `l2cap_parse_conf_req()`, and `l2cap_parse_conf_rsp()`. |

---

## 3. Live On-Device Verification Matrix (`Linux 3.10.108+ #10`)

Verified on physical LYF Flame 8 (`LS-4505`, serial `4f409015`) after permanent installation to `/dev/block/mmcblk0p20` (`boot`) and cold `adb reboot`:

| Subsystem | Verification Check | Result |
| :--- | :--- | :--- |
| **Kernel Version** | `uname -a` / `cat /proc/version` | **PASS** — `Linux localhost 3.10.108+ #10 SMP PREEMPT armv7l` (`gcc version 4.8 (GCC)`) |
| **Boot & Root** | `getprop sys.boot_completed` & `su -c id` | **PASS** — `1`, `uid=0(root) context=u:r:magisk:s0`, SELinux Enforcing |
| **Kernel Config** | `ls -lh /proc/config.gz` | **PASS** — `23.3 KB` live config accessible |
| **Loadable Modules** | `lsmod` | **PASS** — `wlan` (`4,201,024` bytes, `271/271` CRCs) + `core_ctl` (`19,456` bytes) |
| **Wi-Fi (WCN3620)** | `ip addr show wlan0` & AP association | **PASS** — `wlan0` `UP,LOWER_UP`, live IPv4 (`172.30.137.149/24`) + global IPv6 |
| **Modem / Baseband** | `getprop gsm.version.baseband` | **PASS** — `MPSS.JO.1.2.c3.4-00009-8909_GEN_PACK-1` |
| **Display & Backlight** | `mdss_fb` / `lcd-backlight` | **PASS** — `480x854` `ili9806e_txd_fwvga_ips_video` + smooth backlight control |
| **Touchscreen & Keys** | `input0: msg2xxx` (`5-0026`) | **PASS** — Multi-touch + virtualkeys (`MENU`/`HOME`/`BACK`) active |
| **Suspend / Resume** | `input keyevent 26` x4 + idle screen timeout | **PASS** — Clean suspend/resume, zero IRQ spinlock deadlocks |
| **Accelerometer** | `1-0018` (`bma2x2-accel`, `input1`) | **PASS** — Bosch `BMA250` (`0x03`) bound & reporting |
| **Light & Proximity** | `1-0049` (`EPL259x`, `input3`/`input4`) | **PASS** — ALS + PS calibrated (`low_thd=1301, high_thd=1521`) |
| **Battery & Charger** | `4-0014` (`smb1360-chg-fg`) | **PASS** — `status=Charging cap=99 vnow=4098000 usb=4860350` |
| **Wakeup Sources** | `wc -l /sys/kernel/debug/wakeup_sources` | **PASS** — `195` wakeup sources read to EOF with zero `dmesg` warnings |
| **Cameras & Flash** | `video1`, `video2`, `flashlight`, `torch-light0` | **PASS** — Rear `ov8856` + actuator + EEPROMs, Front `s5k5e2`, dual LED flash |
| **Audio (ALSA/QDSP6)** | `cat /proc/asound/cards` | **PASS** — `0 [msm8909skuesndc]: msm8909-skue-snd-card` + Headset/Button Jack |

---

## 4. Captured Verification Output Excerpt (`#10` Cold Boot)

```text
$ adb shell 'uname -a; getprop sys.boot_completed; getprop wlan.driver.status; su -c "id; lsmod; ip -4 addr show wlan0"'
Linux localhost 3.10.108+ #10 SMP PREEMPT Sun Nov 22 18:11:11 UTC 2026 armv7l
1
ok
uid=0(root) gid=0(root) groups=0(root) context=u:r:magisk:s0
Module                  Size  Used by
core_ctl               19456  0
wlan                 4201024  0
22: wlan0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc mq state UP qlen 1000
    inet 172.30.137.149/24 brd 172.30.137.255 scope global wlan0
       valid_lft forever preferred_lft forever
```

---

## 5. Next-Stage Roadmap (Stripped Android / `lk2nd` + postmarketOS)

With all hardware addresses, GPIOs, regulators, panel init commands, and an EDL + `mmcblk0p24` recovery workflow verified:
1. **Debloated / Hardened Android**: Disable unused carrier/OEM daemons on `/system` (via systemless Magisk overlay) to reclaim ~200+ MB of the 1 GB RAM while keeping full VoLTE/Camera/Wi-Fi support on `3.10.108+`.
2. **`lk2nd` + Mainline Linux / postmarketOS**: Use the verified hardware map in [`01-HARDWARE-FORENSICS-AND-DTS.md`](./01-HARDWARE-FORENSICS-AND-DTS.md) (`ili9806e_txd_fwvga_ips_video`, `msg2238@26`, `smb1360@14`, `bma250@18`, `epl259x@49`, and `archive/mainline-6.x-drm-panel-attempt`) to test `lk2nd` + mainline kernel builds safely via `mmcblk0p24` (`recovery`) first without disturbing the working `3.10.108+` Android installation on `mmcblk0p20`!
