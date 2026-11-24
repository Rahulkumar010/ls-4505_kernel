#!/sbin/.magisk/busybox/sh
set -e
export PATH=/sbin/.magisk/busybox:$PATH

MOD=/data/adb/modules/ls4505_ultralean
rm -rf "$MOD"
mkdir -p "$MOD/system/app" "$MOD/system/priv-app"

cat << 'EOF' > "$MOD/module.prop"
id=ls4505_ultralean
name=LS-4505 Ultra-Lean & Anti-Telemetry Suite
version=v1.0
versionCode=1
author=Rahulkumar010
description=Systemlessly strips Adups FOTA spyware, Jio Ignite, SalesTracker, factory test daemons, and GMS bloat while keeping Chrome, CMFileManager, Camera, FM, Phone/SMS, and tuning 1GB RAM + ZRAM.
EOF

# 1. Switch default keyboard to offline AOSP LatinIME BEFORE hiding LatinImeGoogle
settings put secure enabled_input_methods com.android.inputmethod.latin/.LatinIME
settings put secure default_input_method com.android.inputmethod.latin/.LatinIME

# 2. Snappy 0.5x UI animations
settings put global window_animation_scale 0.5
settings put global transition_animation_scale 0.5
settings put global animator_duration_scale 0.5

# 3. System /system/app directories to systemlessly replace (.replace)
SYS_APPS="
AGPSTestMode
AntHalService
BasicDreams
CalendarGoogle
Calibrator
CloudPrint
DMAgent
DeviceInfo
Drive
Email
EngineerTool
EngineerToolOp
Exchange2
FaceLock
FotaUpdate
FotaUpdateReboot
GSDeviceInfo
Galaxy4
Gmail2
GoogleContactsSyncAdapter
GoogleHindiIME
GoogleTTS
Hangouts
HoloSpiralWallpaper
LatinImeGoogle
LiveWallpapers
LunarInfoProvider
Maps
ModemTestMode
Music2
NewsWeather
NoiseField
OmaDownload
PartnerBookmarksProvider
PhaseBeam
PhotoTable
Photos
PlusOne
PopupPlayService
Protips
QTIDiagServices
QtiBackupAgent
SalesTracker
SecProtect
Translate
Videos
YouTube
mmi_test
talkback
"

for app in $SYS_APPS; do
    if [ -d "/system/app/$app" ]; then
        mkdir -p "$MOD/system/app/$app"
        touch "$MOD/system/app/$app/.replace"
    fi
done

# 4. System /system/priv-app directories to systemlessly replace (.replace)
#    IMPORTANT: Keep GooglePackageInstaller (runtime permissions + APK installer)!
PRIV_APPS="
ConfigUpdater
ConfigurationClient
GmsCore
GoogleBackupTransport
GoogleFeedback
GoogleLoginService
GoogleOneTimeInitializer
GooglePartnerSetup
GoogleServicesFramework
Ignite
Phonesky
SetupWizard
TelephonyExtVodafonePack
Velvet
"

for app in $PRIV_APPS; do
    if [ -d "/system/priv-app/$app" ]; then
        mkdir -p "$MOD/system/priv-app/$app"
        touch "$MOD/system/priv-app/$app/.replace"
    fi
done

# 5. Stop crashing/factory daemons and tune ZRAM + LMK in service.sh
cat << 'EOF' > "$MOD/service.sh"
#!/sbin/.magisk/busybox/sh
# Stop crashing/factory/diagnostic daemons immediately and after boot_completed
for svc in ss_ramdump FactoryNvServer ptt_socket_app RIDL qti-testscripts; do
    setprop ctl.stop "$svc"
done

while [ "$(getprop sys.boot_completed)" != "1" ]; do
    sleep 2
done

for svc in ss_ramdump FactoryNvServer ptt_socket_app RIDL qti-testscripts; do
    setprop ctl.stop "$svc"
done

# Enable 256MB ZRAM swap if not already active & tune VM for 1GB RAM
if [ -b /dev/block/zram0 ]; then
    CUR_ZRAM=$(cat /sys/block/zram0/disksize 2>/dev/null || echo 0)
    if [ "$CUR_ZRAM" = "0" ]; then
        echo 268435456 > /sys/block/zram0/disksize 2>/dev/null || true
        /sbin/.magisk/busybox/mkswap /dev/block/zram0 >/dev/null 2>&1 || true
        /sbin/.magisk/busybox/swapon /dev/block/zram0 >/dev/null 2>&1 || true
    fi
fi
echo 60 > /proc/sys/vm/swappiness 2>/dev/null || true
echo 0 > /proc/sys/vm/page-cluster 2>/dev/null || true
EOF
chmod 0755 "$MOD/service.sh"
chmod -R 0755 "$MOD/system"
find "$MOD/system" -name ".replace" -exec chmod 0644 {} +

echo "Installed Magisk module ls4505_ultralean:"
echo "  Replaced /system/app dirs: $(ls -1 "$MOD/system/app" | wc -l)"
echo "  Replaced /system/priv-app dirs: $(ls -1 "$MOD/system/priv-app" | wc -l)"
sync
