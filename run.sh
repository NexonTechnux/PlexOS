#!/bin/bash
set -e
cd "$(dirname "$0")"
# disco grezzo per test ATA (64MB)
if [ ! -f /tmp/plexos_disk.img ]; then
  dd if=/dev/zero of=/tmp/plexos_disk.img bs=1M count=64 status=none
  echo "Disco /tmp/plexos_disk.img creato (64MB)"
fi
make plexos.iso
echo ""
echo "=== PlexOS pronta: plexos.iso ==="
echo "Avvio QEMU 720p..."
qemu-system-i386 -cdrom plexos.iso -m 256M \
 -vga std \
 -drive id=disk0,if=ide,format=raw,file=/tmp/plexos_disk.img \
 -net nic,model=e1000 -net user \
 -rtc base=localtime \
 -serial stdio \
 -display gtk,zoom-to-fit=on 2>&1 | head -n 100 || true
