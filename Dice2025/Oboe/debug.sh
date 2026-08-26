#!/bin/sh
set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"
WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

echo "[*] Compiling exploit..."
gcc -static -O2 "$ROOT/exploit.c" -o "$WORKDIR/exploit"
gcc -static -O2 "$ROOT/evilsu.c" -o "$WORKDIR/evilsu"
gcc -static -O2 "$ROOT/trigger.c" -o "$WORKDIR/trigger"

echo "[*] Extracting initramfs..."
mkdir "$WORKDIR/initramfs"
cd "$WORKDIR/initramfs"
zcat "$ROOT/initramfs.cpio.gz" | cpio -id

echo "[*] Installing exploit..."
echo "FLAG" > "$WORKDIR/initramfs/flag"
cp "$WORKDIR/exploit" "$WORKDIR/initramfs/exploit"
chmod +x "$WORKDIR/initramfs/exploit"
cp "$WORKDIR/evilsu" "$WORKDIR/initramfs/evilsu"
chmod +x "$WORKDIR/initramfs/evilsu"
cp "$WORKDIR/trigger" "$WORKDIR/initramfs/trigger"
chmod +x "$WORKDIR/initramfs/trigger"

echo "[*] Repacking initramfs..."
cd "$WORKDIR/initramfs"
find . -print0 | cpio --null -o -H newc | gzip -9 > "$ROOT/initramfs-exploit.cpio.gz"

echo "[*] Checking archive..."
zcat "$ROOT/initramfs-exploit.cpio.gz" | cpio -it | grep exploit

echo "[*] Launching kernel..."
exec qemu-system-x86_64 \
    -kernel "$ROOT/bzImage" \
    -initrd "$ROOT/initramfs-exploit.cpio.gz" \
    -monitor none \
    -append "console=ttyS0 quiet oops=panic nokaslr" \
    -cpu qemu64,+smep,+smap \
    -m 128M \
    -nographic \
    -no-reboot \
	-s