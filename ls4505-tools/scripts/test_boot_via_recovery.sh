#!/sbin/.magisk/busybox/sh
set -e
export PATH=/sbin/.magisk/busybox:$PATH
MB=/data/adb/magisk/magiskboot

echo '=== 1. Check untouched boot (mmcblk0p20) MD5 ==='
md5sum /dev/block/mmcblk0p20

echo '=== 2. Stage kernel-version-aware pronto_wlan.ko bind-mount ==='
mkdir -p /data/adb/ls4505-3.10.108 /data/adb/post-fs-data.d
cp /data/local/tmp/pronto_wlan-3.10.108.ko /data/adb/ls4505-3.10.108/pronto_wlan.ko
chmod 0644 /data/adb/ls4505-3.10.108/pronto_wlan.ko
chcon u:object_r:system_file:s0 /data/adb/ls4505-3.10.108/pronto_wlan.ko

cat << 'EOF' > /data/adb/post-fs-data.d/10_ls4505_modules.sh
#!/sbin/.magisk/busybox/sh
read KREL < /proc/sys/kernel/osrelease
case "$KREL" in
  3.10.108*)
    /sbin/.magisk/busybox/mount -o bind /data/adb/ls4505-3.10.108/pronto_wlan.ko /system/lib/modules/pronto/pronto_wlan.ko
    ;;
esac
EOF
chmod 0755 /data/adb/post-fs-data.d/10_ls4505_modules.sh

echo '=== 3. Repack boot-ls4505-3.10.108.img via magiskboot ==='
rm -rf /data/local/tmp/repack108
mkdir -p /data/local/tmp/repack108
cd /data/local/tmp/repack108
dd if=/dev/block/mmcblk0p20 of=orig-boot.img bs=1048576
$MB unpack orig-boot.img
cp /data/local/tmp/zImage-3.10.108 kernel
cp /data/local/tmp/ls4505-3.10.108.dtb kernel_dtb
$MB repack orig-boot.img new-boot.img
dd if=/dev/zero of=boot-ls4505-3.10.108.img bs=1048576 count=32
dd if=new-boot.img of=boot-ls4505-3.10.108.img conv=notrunc
chmod 0644 boot-ls4505-3.10.108.img

echo '=== 4. Write test image ONLY to mmcblk0p24 (recovery) ==='
dd if=boot-ls4505-3.10.108.img of=/dev/block/mmcblk0p24 bs=1048576
sync
echo '=== Verify mmcblk0p20 untouched & mmcblk0p24 matches ==='
md5sum /dev/block/mmcblk0p20 boot-ls4505-3.10.108.img /dev/block/mmcblk0p24
