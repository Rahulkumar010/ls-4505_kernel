#!/bin/sh
set -e
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
export HOME=/root

echo "[2/3] Installing 448 ARMv7 Alpine + Xfce4 Desktop packages..."
apk add --no-network --no-progress /tmp/apk-cache/*.apk

echo "[3/3] Configuring kernel modules, Xfce4 Touch UI, Wi-Fi & SSH..."
for kver in $(ls /lib/modules); do
    depmod -a "$kver"
done

mkdir -p /etc/modules-load.d
cat << 'MOD_EOF' > /etc/modules-load.d/ls4505.conf
qcom_wcnss_pil
wcn36xx
btqcomsmd
ipv6
MOD_EOF

if ! id user >/dev/null 2>&1; then
    adduser -D -s /bin/sh -G wheel user
fi
passwd -u user 2>/dev/null || true
passwd -d user >/dev/null 2>&1 || true
for grp in wheel video input audio netdev plugdev tty; do
    addgroup "$grp" 2>/dev/null || true
    addgroup user "$grp" 2>/dev/null || true
done
mkdir -p /etc/sudoers.d
echo "%wheel ALL=(ALL:ALL) NOPASSWD: ALL" > /etc/sudoers.d/wheel
chmod 0440 /etc/sudoers.d/wheel

ssh-keygen -A
mkdir -p /root/.ssh /home/user/.ssh
echo "${SSH_AUTHORIZED_KEY:-}" > /root/.ssh/authorized_keys
echo "${SSH_AUTHORIZED_KEY:-}" > /home/user/.ssh/authorized_keys
chmod 700 /root/.ssh /home/user/.ssh
chmod 600 /root/.ssh/authorized_keys /home/user/.ssh/authorized_keys
chown -R user:wheel /home/user/.ssh
sed -i 's/^#PubkeyAuthentication.*/PubkeyAuthentication yes/' /etc/ssh/sshd_config
echo "PermitEmptyPasswords no" >> /etc/ssh/sshd_config

mkdir -p /etc/NetworkManager/conf.d
cat << 'NM_EOF' > /etc/NetworkManager/conf.d/99-ls4505.conf
[main]
plugins=ifupdown,keyfile

[ifupdown]
managed=true

[keyfile]
unmanaged-devices=interface-name:usb0

[device]
wifi.scan-rand-mac-address=no
NM_EOF

mkdir -p /etc/local.d
cat << 'LOCAL_EOF' > /etc/local.d/10-ls4505-boot.start
#!/bin/sh
echo 0 > /sys/class/graphics/fb0/blank 2>/dev/null || true
for bl in /sys/class/backlight/*/brightness; do
    [ -e "$bl" ] && echo 220 > "$bl" 2>/dev/null || true
done

if ip link show usb0 >/dev/null 2>&1; then
    ip addr add 172.16.42.1/24 dev usb0 2>/dev/null || true
    ip link set usb0 up 2>/dev/null || true
    cat << 'U_EOF' > /tmp/udhcpd.conf
start 172.16.42.2
end 172.16.42.20
interface usb0
option subnet 255.255.255.0
option lease 86400
U_EOF
    mkdir -p /var/lib/misc
    touch /var/lib/misc/udhcpd.leases
    udhcpd /tmp/udhcpd.conf 2>/dev/null || true
fi

if [ -b /dev/zram0 ]; then
    swapoff /dev/zram0 2>/dev/null || true
    echo 1 > /sys/block/zram0/reset 2>/dev/null || true
    echo $((256 * 1024 * 1024)) > /sys/block/zram0/disksize 2>/dev/null || true
    mkswap /dev/zram0 >/dev/null 2>&1 && swapon -p 100 /dev/zram0 2>/dev/null || true
fi

modprobe qcom_wcnss_pil 2>/dev/null || true
modprobe wcn36xx 2>/dev/null || true
LOCAL_EOF
chmod +x /etc/local.d/10-ls4505-boot.start

cat << 'RBA_EOF' > /usr/local/bin/reboot-to-android
#!/bin/sh
sudo reboot
RBA_EOF
chmod +x /usr/local/bin/reboot-to-android

mkdir -p /etc/X11/xorg.conf.d
cat << 'XFB_EOF' > /etc/X11/xorg.conf.d/10-modesetting.conf
Section "Device"
    Identifier "MSM8909 SimpleDRM KMS"
    Driver "modesetting"
    Option "kmsdev" "/dev/dri/card0"
    Option "SWcursor" "true"
    Option "ShadowFB" "true"
EndSection

Section "ServerFlags"
    Option "BlankTime" "0"
    Option "StandbyTime" "0"
    Option "SuspendTime" "0"
    Option "OffTime" "0"
EndSection
XFB_EOF

cat << 'XIN_EOF' > /etc/X11/xorg.conf.d/40-libinput.conf
Section "InputClass"
    Identifier "libinput touchscreen catchall"
    MatchIsTouchscreen "on"
    MatchDevicePath "/dev/input/event*"
    Driver "libinput"
EndSection

Section "InputClass"
    Identifier "libinput keyboard catchall"
    MatchIsKeyboard "on"
    MatchDevicePath "/dev/input/event*"
    Driver "libinput"
EndSection
XIN_EOF

sed -i 's/^#autologin-user=.*/autologin-user=user/' /etc/lightdm/lightdm.conf
sed -i 's/^#autologin-user-timeout=.*/autologin-user-timeout=0/' /etc/lightdm/lightdm.conf
sed -i 's/^#user-session=.*/user-session=xfce/' /etc/lightdm/lightdm.conf
if ! grep -q "^autologin-user=user" /etc/lightdm/lightdm.conf; then
    sed -i '/^\[Seat:\*\]/a autologin-user=user\nautologin-user-timeout=0\nuser-session=xfce' /etc/lightdm/lightdm.conf
fi

mkdir -p /home/user/.config/xfce4/xfconf/xfce-perchannel-xml /home/user/Desktop
cat << 'XSET_EOF' > /home/user/.config/xfce4/xfconf/xfce-perchannel-xml/xsettings.xml
<?xml version="1.0" encoding="UTF-8"?>
<channel name="xsettings" version="1.0">
  <property name="Net" type="empty">
    <property name="ThemeName" type="string" value="Adwaita-dark"/>
    <property name="IconThemeName" type="string" value="Adwaita"/>
  </property>
  <property name="Gtk" type="empty">
    <property name="FontName" type="string" value="DejaVu Sans 11"/>
  </property>
  <property name="Xft" type="empty">
    <property name="DPI" type="int" value="125"/>
    <property name="Antialias" type="int" value="1"/>
  </property>
</channel>
XSET_EOF

cat << 'DESK1' > /home/user/Desktop/Terminal.desktop
[Desktop Entry]
Version=1.0
Type=Application
Name=Terminal
Exec=xfce4-terminal
Icon=utilities-terminal
Terminal=false
DESK1

cat << 'DESK2' > /home/user/Desktop/Files.desktop
[Desktop Entry]
Version=1.0
Type=Application
Name=Files
Exec=thunar /home/user
Icon=system-file-manager
Terminal=false
DESK2

cat << 'DESK3' > /home/user/Desktop/WiFi.desktop
[Desktop Entry]
Version=1.0
Type=Application
Name=Wi-Fi Setup
Exec=xfce4-terminal -e "sudo nmtui"
Icon=network-wireless
Terminal=false
DESK3

cat << 'DESK4' > /home/user/Desktop/RebootAndroid.desktop
[Desktop Entry]
Version=1.0
Type=Application
Name=Boot Android
Exec=/usr/local/bin/reboot-to-android
Icon=system-reboot
Terminal=false
DESK4

cat << 'DESK5' > /home/user/Desktop/Keyboard.desktop
[Desktop Entry]
Version=1.0
Type=Application
Name=Touch Keyboard
Exec=matchbox-keyboard
Icon=input-keyboard
Terminal=false
DESK5

cat << 'DESK6' > /home/user/Desktop/Browser.desktop
[Desktop Entry]
Version=1.0
Type=Application
Name=Web Browser
Exec=dillo
Icon=web-browser
Terminal=false
DESK6

chmod +x /home/user/Desktop/*.desktop
chown -R user:wheel /home/user

cat << 'FSTAB_EOF' > /etc/fstab
proc /proc proc defaults 0 0
sysfs /sys sysfs defaults 0 0
devpts /dev/pts devpts gid=5,mode=620 0 0
tmpfs /run tmpfs mode=0755,nosuid,nodev 0 0
tmpfs /tmp tmpfs nosuid,nodev 0 0
FSTAB_EOF

rc-update add devfs sysinit
rc-update add dmesg sysinit
rc-update add udev sysinit
rc-update add udev-trigger sysinit
rc-update add udev-settle sysinit

rc-update add modules boot
rc-update add sysctl boot
rc-update add hostname boot
rc-update add bootmisc boot
rc-update add syslog boot

rc-update add dbus default
rc-update add elogind default
rc-update add networkmanager default
rc-update add sshd default
rc-update add local default
rc-update add lightdm default

rm -rf /tmp/apk-cache
touch /etc/.pmos_installed
echo "[OK] First-boot desktop provisioning completed!"
