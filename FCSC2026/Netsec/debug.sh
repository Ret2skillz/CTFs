#!/bin/bash

# Launch both VMs locally for exploit debugging
#
# Challenge VM (192.168.2.2) + Pivot VM (192.168.2.1)
# connected via UNIX socket
#
# SSH into pivot VM via localhost:2222,
# then run /tmp/exploit

cd "$(dirname "$0")"

cleanup() {
    [ -n "$pid1" ] && kill "$pid1" 2>/dev/null
    [ -n "$pid2" ] && kill "$pid2" 2>/dev/null
    rm -f /tmp/qemu_debug_sock
}

trap cleanup EXIT INT TERM

rm -f /tmp/qemu_debug_sock

ROOTFS_PIVOT="rootfs_pivot.ext2"

#
# Compile exploit
#

echo "[*] Compiling exploit..."

gcc -static exploit.c -o exploit

if [ $? -ne 0 ]; then
    echo "[!] Failed to compile exploit"
    exit 1
fi

chmod 755 exploit

echo "[+] exploit compiled successfully"

#
# Check pivot rootfs
#

if [ ! -f "$ROOTFS_PIVOT" ]; then
    echo "[!] $ROOTFS_PIVOT not found"
    exit 1
fi

#
# Inject exploit into /exploit
# /tmp is a tmpfs in the pivot VM, so putting it
# directly in /tmp inside the ext2 image won't work.
#

echo "[*] Removing old /exploit..."

debugfs -w -R "unlink /exploit" "$ROOTFS_PIVOT" 2>/dev/null || true

echo "[*] Injecting exploit -> /exploit..."

debugfs -w -R "write exploit /exploit" "$ROOTFS_PIVOT"

if [ $? -ne 0 ]; then
    echo "[!] Failed to inject exploit"
    exit 1
fi

echo "[*] Setting executable permissions..."

debugfs -w -R "set_inode_field /exploit mode 0100755" "$ROOTFS_PIVOT"

if [ $? -ne 0 ]; then
    echo "[!] Failed to set exploit permissions"
    exit 1
fi

echo "[*] Verifying exploit in rootfs..."

if ! debugfs -R "stat /exploit" "$ROOTFS_PIVOT" 2>&1 | grep -q "Inode:"; then
    echo "[!] /exploit was not found in rootfs"
    exit 1
fi

echo "[+] exploit successfully injected into rootfs"

#
# Start challenge VM
#

echo "[*] Starting challenge VM (192.168.2.2, port 1337)..."

qemu-system-x86_64 \
    -gdb tcp::4321 \
    -cpu qemu64,+smep,+smap \
    -m 256M \
    -kernel bzImage \
    -nographic \
    -monitor none \
    -serial null \
    -append 'rootwait root=/dev/vda console=ttyS0 kptr_restrict=0 nokaslr' \
    -drive file=rootfs_challenge.ext2,if=virtio,format=raw,readonly=on \
    -device e1000,netdev=net0 \
    -netdev user,id=net0 \
    -device e1000,netdev=net1 \
    -netdev stream,id=net1,server=on,addr.type=unix,addr.path=/tmp/qemu_debug_sock \
    &

pid1=$!

sleep 2

#
# Start pivot VM
#

echo "[*] Starting pivot VM (192.168.2.1, SSH on localhost:2222)..."

qemu-system-x86_64 \
    -cpu qemu64,+smep,+smap \
    -m 256M \
    -kernel bzImage \
    -serial stdio \
    -nographic \
    -monitor none \
    -append 'rootwait root=/dev/vda console=ttyS0 kptr_restrict=0 nokaslr' \
    -drive file=rootfs_pivot.ext2,if=virtio,format=raw \
    -device e1000,netdev=net0 \
    -netdev user,id=net0,net=192.168.1.0/24,hostfwd=tcp::2222-192.168.1.1:22 \
    -device e1000,netdev=net1 \
    -netdev stream,id=net1,addr.type=unix,addr.path=/tmp/qemu_debug_sock \
    &

pid2=$!

#
# Wait for SSH
#

echo "[*] Waiting for pivot VM to boot..."

for i in $(seq 1 30); do
    if ssh \
        -o ConnectTimeout=1 \
        -o ConnectionAttempts=1 \
        -o StrictHostKeyChecking=no \
        -o UserKnownHostsFile=/dev/null \
        -o BatchMode=yes \
        -p 2222 \
        ctf@127.0.0.1 \
        true 2>/dev/null; then
        break
    fi

    sleep 1
done

#
# Verify SSH actually came up
#

if ! ssh \
    -o ConnectTimeout=2 \
    -o StrictHostKeyChecking=no \
    -o UserKnownHostsFile=/dev/null \
    -o BatchMode=yes \
    -p 2222 \
    ctf@127.0.0.1 \
    true 2>/dev/null; then

    echo "[!] Pivot SSH did not become available"
    exit 1
fi

echo "[+] Pivot VM is ready"

#
# Copy exploit from  into the boot-mounted /tmp
#

echo "[*] Copying exploit to /tmp..."

ssh \
    -o StrictHostKeyChecking=no \
    -o UserKnownHostsFile=/dev/null \
    -p 2222 \
    ctf@127.0.0.1 \
    'cp /exploit /tmp/exploit && chmod 755 /tmp/exploit'

if [ $? -ne 0 ]; then
    echo "[!] Failed to copy exploit into /tmp"
    exit 1
fi

#
# Verify
#

echo "[*] Verifying /tmp/exploit..."

ssh \
    -o StrictHostKeyChecking=no \
    -o UserKnownHostsFile=/dev/null \
    -p 2222 \
    ctf@127.0.0.1 \
    'ls -alh /tmp/exploit'

if [ $? -ne 0 ]; then
    echo "[!] /tmp/exploit is not available"
    exit 1
fi

echo ""
echo "=========================================="
echo "  VMs are running!"
echo ""
echo "  SSH into pivot:"
echo "    ssh -p 2222 ctf@127.0.0.1"
echo ""
echo "  Exploit:"
echo "    /tmp/exploit"
echo ""
echo "  Challenge:"
echo "    192.168.2.2:1337"
echo ""
echo "=========================================="
echo ""

wait