#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu

root="${1:?rootfs directory required}"
out="${2:?output cpio required}"

rm -rf "$root"
mkdir -p "$root"/bin "$root"/dev "$root"/etc "$root"/proc "$root"/sys "$root"/tmp

busybox_bin="$(command -v busybox)"
cp "$busybox_bin" "$root/bin/busybox"

for applet in sh mount umount cat echo dmesg sleep uname; do
	ln -s busybox "$root/bin/$applet"
done

cat > "$root/init" <<'EOF'
#!/bin/sh
mount -t proc proc /proc
mount -t sysfs sysfs /sys
echo "sf2000_linux: initramfs alive"
uname -a
exec /bin/sh
EOF
chmod +x "$root/init"

(
	cd "$root"
	find . -print0 | cpio -0 -o -H newc > "../$(basename "$out")"
)
