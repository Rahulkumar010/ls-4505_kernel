#!/sbin/.magisk/busybox/sh
set -e
export PATH=/sbin/.magisk/busybox:$PATH
echo "=== 1. Writing verified boot-ls4505-3.10.108.img to permanent boot (mmcblk0p20) ==="
dd if=/data/local/tmp/repack108/boot-ls4505-3.10.108.img of=/dev/block/mmcblk0p20 bs=1048576
sync

echo "=== 2. Restoring stock recovery-orginal.img to recovery (mmcblk0p24) ==="
dd if=/data/local/tmp/recovery-orginal.img of=/dev/block/mmcblk0p24 bs=1048576
sync

echo "=== 3. Verify MD5 checksums on eMMC partitions ==="
md5sum /data/local/tmp/repack108/boot-ls4505-3.10.108.img /dev/block/mmcblk0p20
md5sum /data/local/tmp/recovery-orginal.img /dev/block/mmcblk0p24

echo "=== 4. Clean up /data/local/tmp scratch files ==="
rm -rf /data/local/tmp/repack* /data/local/tmp/*.img /data/local/tmp/*.dtb /data/local/tmp/zImage* /data/local/tmp/*.ko /data/local/tmp/*.sh /data/local/tmp/ramoops*
sync
echo "=== PERMANENT INSTALL COMPLETE ==="
