#!/bin/sh

# Required to kill the QEMU instances when the TCP connections ends
cleanup() {
    [ -n "$pid1" ] && kill "$pid1" 2>/dev/null
    [ -n "$pid2" ] && kill "$pid2" 2>/dev/null
}
trap cleanup EXIT INT TERM

# Randomize a TCP port
min_port=1024
max_port=65355
while :; do
    r=$(od -An -N2 -tu2 /dev/urandom | tr -d ' ')
    port=$((min_port + r % (max_port - min_port + 1)))
    (echo >/dev/tcp/127.0.0.1/$port) >/dev/null 2>&1 && continue
    break
done

# Create a UNIX socket for inter-VM communications
sock_path=$(mktemp /fcsc/qemu_sock_XXXX)
rm -f "$sock_path"

echo "New connection! port=$port, UNIX socket=$sock_path" >&2

# Start the victim VM
(/usr/bin/qemu-system-x86_64                                                \
  -cpu qemu64,+smep,+smap                                                   \
  -kernel bzImage                                                           \
  -serial stdio                                                             \
  -nographic                                                                \
  -monitor none                                                             \
  -append 'rootwait root=/dev/vda console=ttyS0'                            \
  -drive file=rootfs_challenge.ext2,if=virtio,format=raw,readonly=on        \
  -device e1000,netdev=net0                                                 \
  -netdev user,id=net0                                                      \
  -device e1000,netdev=net1                                                 \
  -netdev stream,id=net1,server=on,addr.type=unix,addr.path=$sock_path      \
         >> /dev/null                                                       \
) &
pid1=$!

sleep 1

# Start the pivoting VM (exploit will be run from here)
(/usr/bin/qemu-system-x86_64                                                \
  -cpu qemu64,+smep,+smap                                                   \
  -kernel bzImage                                                           \
  -serial stdio                                                             \
  -nographic                                                                \
  -monitor none                                                             \
  -append 'rootwait root=/dev/vda console=ttyS0'                            \
  -drive file=rootfs_pivot.ext2,if=virtio,format=raw,readonly=on            \
  -device e1000,netdev=net0                                                 \
  -netdev user,id=net0,net=192.168.1.0/24,hostfwd=tcp::$port-192.168.1.1:22 \
  -device e1000,netdev=net1                                                 \
  -netdev stream,id=net1,addr.type=unix,addr.path=$sock_path                \
            >> /dev/null                                                    \
) &
pid2=$!

sleep 10;

# Redirect incoming TCP traffic to pivoting VM
socat STDIO TCP:127.0.0.1:$port
