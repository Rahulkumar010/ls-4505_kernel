#!/sbin/.magisk/busybox/sh
set -e
export PATH=/sbin/.magisk/busybox:$PATH

MOD=/data/adb/modules/ls4505_ultralean
rm -rf "$MOD"
mkdir -p "$MOD/system/app" "$MOD/system/priv-app" "$MOD/system/xbin"

cat << 'PROP_EOF' > "$MOD/module.prop"
id=ls4505_ultralean
name=LS-4505 Ultra-Lean Debloat, ZRAM & Dual-Boot Tools (v2)
version=v2.0
versionCode=2
author=Rahulkumar010
description=Systemlessly masks OEM spyware/bloatware & unneeded stock apps (100% restorable via restore-system-app), stops crashing ramdump daemons, enables 256MB ZRAM, and adds boot-linux / boot-fastboot / otg-mode CLI tools.
PROP_EOF

# System apps to mask systemlessly via Magisk .replace (original /system remains 100% untouched!)
SYS_APPS="
AGPSTestMode AdvancedReset AntHalService BasicDreams BookmarkProvider
CMFileManager CalendarGoogle CalendarLocalAccount Calibrator Chrome
CloudPrint DMAgent DeskClock DeviceInfo DownloadProviderUi
Drive Email EngineerTool EngineerToolOp ExactCalculator
Exchange2 FM2 FaceLock FotaUpdate FotaUpdateReboot
GSDeviceInfo Galaxy4 Gmail2 GoogleContactsSyncAdapter GoogleHindiIME
GoogleTTS HTMLViewer Hangouts HoloSpiralWallpaper LatinImeGoogle
LiveWallpapers LiveWallpapersPicker LunarInfoProvider Maps ModemTestMode
Music Music2 NewsWeather NoiseField OmaDownload
PartnerBookmarksProvider PhaseBeam PhotoTable Photos PlusOne
PopupPlayService PrintSpooler Protips QTIDiagServices QTITaskManager
QtiBackupAgent SalesTracker SecProtect SimContacts Stk
Translate UserDictionaryProvider Videos YouTube mmi_test
talkback
"

PRIV_APPS="
BackupRestoreConfirmation CalendarProvider CallLogBackup CellBroadcastReceiver ConfigUpdater
ConfigurationClient Contacts Gallery2 GmsCore GoogleBackupTransport
GoogleFeedback GoogleLoginService GoogleOneTimeInitializer GooglePartnerSetup GoogleServicesFramework
Ignite ManagedProvisioning Mms MusicFX Phonesky
SetupWizard SharedStorageBackup SoundRecorder TelephonyExtVodafonePack Velvet
"

masked_count=0
for app in $SYS_APPS; do
    if [ -d "/system/app/$app" ]; then
        mkdir -p "$MOD/system/app/$app"
        touch "$MOD/system/app/$app/.replace"
        masked_count=$((masked_count + 1))
    fi
done

for app in $PRIV_APPS; do
    if [ -d "/system/priv-app/$app" ]; then
        mkdir -p "$MOD/system/priv-app/$app"
        touch "$MOD/system/priv-app/$app/.replace"
        masked_count=$((masked_count + 1))
    fi
done
echo "Systemlessly masked $masked_count stock directories in $MOD"

# Boot service script: stop crashing OEM daemons, enable 256MB ZRAM, set snappy 0.5x animations
cat << 'SVC_EOF' > "$MOD/service.sh"
#!/sbin/.magisk/busybox/sh
export PATH=/sbin/.magisk/busybox:$PATH

for svc in ss_ramdump FactoryNvServer ptt_socket_app; do
    stop "$svc" 2>/dev/null || setprop "ctl.stop" "$svc" 2>/dev/null || true
done

if [ -b /dev/block/zram0 ]; then
    swapoff /dev/block/zram0 2>/dev/null || true
    echo 1 > /sys/block/zram0/reset 2>/dev/null || true
    echo $((256 * 1024 * 1024)) > /sys/block/zram0/disksize 2>/dev/null || true
    mkswap /dev/block/zram0 >/dev/null 2>&1 && swapon /dev/block/zram0 2>/dev/null || true
fi

echo 60 > /proc/sys/vm/swappiness 2>/dev/null || true

while [ "$(getprop sys.boot_completed)" != "1" ]; do
    sleep 2
done
settings put global window_animation_scale 0.5 2>/dev/null || true
settings put global transition_animation_scale 0.5 2>/dev/null || true
settings put global animator_duration_scale 0.5 2>/dev/null || true
SVC_EOF
chmod 0755 "$MOD/service.sh"

# CLI Tool 1: restore-system-app (selective or full restoration of any Magisk-masked stock app!)
cat << 'RESTORE_EOF' > "$MOD/system/xbin/restore-system-app"
#!/sbin/.magisk/busybox/sh
MOD=/data/adb/modules/ls4505_ultralean
if [ $# -eq 0 ] || [ "$1" = "--help" ]; then
    echo "Usage:"
    echo "  restore-system-app --list         List all currently masked system apps"
    echo "  restore-system-app <AppName>      Restore a specific app (e.g. Chrome, Music, Contacts, Mms, Gallery2)"
    echo "  restore-system-app --all          Disable entire ls4505_ultralean debloat module"
    exit 0
fi

if [ "$1" = "--list" ]; then
    echo "=== Masked /system/app ==="
    ls -1 "$MOD/system/app" 2>/dev/null || true
    echo "=== Masked /system/priv-app ==="
    ls -1 "$MOD/system/priv-app" 2>/dev/null || true
    exit 0
fi

if [ "$1" = "--all" ]; then
    touch "$MOD/disable"
    echo "Disabled ls4505_ultralean module. Reboot to restore all original stock apps."
    exit 0
fi

restored=0
for app in "$@"; do
    for sub in app priv-app; do
        if [ -d "$MOD/system/$sub/$app" ]; then
            rm -rf "$MOD/system/$sub/$app"
            echo "[OK] Unmasked /system/$sub/$app ? reboot to restore it!"
            restored=1
        fi
    done
done
[ "$restored" -eq 1 ] || echo "App '$1' was not found in masked list (run 'restore-system-app --list')."
RESTORE_EOF
chmod 0755 "$MOD/system/xbin/restore-system-app"

# CLI Tool 2: mask-system-app (re-mask a stock app systemlessly)
cat << 'MASK_EOF' > "$MOD/system/xbin/mask-system-app"
#!/sbin/.magisk/busybox/sh
MOD=/data/adb/modules/ls4505_ultralean
if [ $# -eq 0 ]; then
    echo "Usage: mask-system-app <AppName>"
    exit 1
fi
for app in "$@"; do
    if [ -d "/system/app/$app" ]; then
        mkdir -p "$MOD/system/app/$app" && touch "$MOD/system/app/$app/.replace"
        echo "[OK] Masked /system/app/$app ? reboot to apply."
    elif [ -d "/system/priv-app/$app" ]; then
        mkdir -p "$MOD/system/priv-app/$app" && touch "$MOD/system/priv-app/$app/.replace"
        echo "[OK] Masked /system/priv-app/$app ? reboot to apply."
    else
        echo "Not found in /system/app or /system/priv-app: $app"
    fi
done
MASK_EOF
chmod 0755 "$MOD/system/xbin/mask-system-app"

# CLI Tool 3: boot-linux (reboot straight into lk2nd -> Mainline Linux 6.12.1)
cat << 'BL_EOF' > "$MOD/system/xbin/boot-linux"
#!/system/bin/sh
sync
reboot recovery
BL_EOF
chmod 0755 "$MOD/system/xbin/boot-linux"

# CLI Tool 4: boot-fastboot (hands-free one-shot boot into unlocked lk2nd Fastboot!)
cat << 'BF_EOF' > "$MOD/system/xbin/boot-fastboot"
#!/sbin/.magisk/busybox/sh
printf "lk2nd-fastboot\0\0" | /sbin/.magisk/busybox/dd of=/dev/block/mmcblk0p22 bs=16 count=1 conv=sync 2>/dev/null
sync
reboot recovery
BF_EOF
chmod 0755 "$MOD/system/xbin/boot-fastboot"

# CLI Tool 5: otg-mode (inspect or manually switch USB OTG Host + SMB1360 5V boost)
cat << 'OTG_EOF' > "$MOD/system/xbin/otg-mode"
#!/sbin/.magisk/busybox/sh
MODE_FILE=/sys/kernel/debug/msm_otg/mode
STATE_FILE=/sys/kernel/debug/msm_otg/otg_state
case "${1:-status}" in
    host|on)
        echo host > "$MODE_FILE"
        echo "Switched USB OTG to HOST mode (5V SMB1360 boost ON)."
        ;;
    peripheral|device|off)
        echo peripheral > "$MODE_FILE"
        echo "Switched USB OTG to PERIPHERAL (ADB/charge) mode."
        ;;
    *)
        echo "Current OTG mode : $(cat "$MODE_FILE" 2>/dev/null || echo N/A)"
        echo "Current OTG state: $(cat "$STATE_FILE" 2>/dev/null || echo N/A)"
        ls /sys/bus/usb/devices/ 2>/dev/null || true
        ;;
esac
OTG_EOF
chmod 0755 "$MOD/system/xbin/otg-mode"

chmod -R 0755 "$MOD/system"
echo "=== ls4505_ultralean v2 installed cleanly! ==="
