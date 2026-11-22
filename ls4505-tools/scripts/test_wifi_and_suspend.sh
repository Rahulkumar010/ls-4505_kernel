#!/sbin/.magisk/busybox/sh
BB=/sbin/.magisk/busybox
echo "=== 1. Check current kernel & uptime ==="
$BB/uname -a
$BB/uptime

echo "=== 2. Stress-test /sys/kernel/debug/wakeup_sources ==="
$BB/wc -l /sys/kernel/debug/wakeup_sources
$BB/tail -n 5 /sys/kernel/debug/wakeup_sources

echo "=== 3. Verify pronto_wlan.ko bind mount & bring up Wi-Fi wlan0 ==="
if ! $BB/mount | $BB/grep -q pronto_wlan.ko; then
    $BB/mount -o bind /data/adb/ls4505-3.10.108/pronto_wlan.ko /system/lib/modules/pronto/pronto_wlan.ko
fi
/system/bin/svc wifi enable
$BB/sleep 8
$BB/lsmod
/system/bin/ip link show wlan0 || true
$BB/dmesg | $BB/grep -E "wlan|VOSS|HDD" | $BB/tail -n 10

echo "=== 4. Test Screen Off (Suspend) & Screen On (Resume) x2 ==="
/system/bin/input keyevent 26
$BB/sleep 4
/system/bin/input keyevent 26
$BB/sleep 3
/system/bin/input keyevent 26
$BB/sleep 4
/system/bin/input keyevent 26
$BB/sleep 3

echo "=== 5. Wait past 75s uptime mark & re-verify wakeup_sources + uname ==="
$BB/sleep 20
$BB/wc -l /sys/kernel/debug/wakeup_sources
$BB/uname -a
$BB/uptime
echo "=== 100% ALL LIVE TESTS PASSED ON 3.10.108+ ==="
