#!/sbin/.magisk/busybox/sh
export PATH="/sbin/.magisk/busybox:$PATH"
echo "=== KERNEL & UPTIME ==="
uname -a
cat /proc/version
uptime
echo "boot_completed=$(getprop sys.boot_completed)"
echo "baseband=$(getprop gsm.version.baseband)"
echo "wlan_status=$(getprop wlan.driver.status)"

echo "=== LOADED MODULES ==="
lsmod

echo "=== WIFI INTERFACE ==="
ip addr show wlan0 2>/dev/null || echo "wlan0 not up yet"

echo "=== BOUND I2C DRIVERS ==="
for d in /sys/bus/i2c/devices/*; do
  if [ -L "$d/driver" ]; then
    echo "$(basename $d): name=$(cat $d/name 2>/dev/null) -> driver=$(basename $(readlink $d/driver))"
  fi
done

echo "=== INPUT DEVICES ==="
for i in /sys/class/input/input*; do
  echo "$(basename $i): $(cat $i/name 2>/dev/null)"
done

echo "=== SENSORS CLASS ==="
ls -1 /sys/class/sensors/ 2>/dev/null

echo "=== POWER SUPPLY ==="
for p in /sys/class/power_supply/*; do
  echo "$(basename $p): type=$(cat $p/type 2>/dev/null) status=$(cat $p/status 2>/dev/null) cap=$(cat $p/capacity 2>/dev/null) vnow=$(cat $p/voltage_now 2>/dev/null)"
done

echo "=== DISPLAY & LEDS ==="
cat /sys/class/graphics/fb0/msm_fb_type /sys/class/graphics/fb0/modes 2>/dev/null
ls -1 /sys/class/leds/

echo "=== CAMERAS & AUDIO ==="
for v in /sys/class/video4linux/video*; do
  echo "$(basename $v): $(cat $v/name 2>/dev/null)"
done
cat /proc/asound/cards
ls -lh /proc/config.gz

echo "=== DMESG ERRORS/PROBES ==="
dmesg | grep -iE "ili9806|mdp3|msg2|epl|bma2|smb1360|wlan|pronto|core_ctl|s5k5e2|ov8856|flashlight" | tail -n 50
