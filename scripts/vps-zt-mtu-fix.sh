#!/bin/sh
# RLCD: ZeroTier runs at MTU 2800 but the path from the VPS to the home LAN
# is ~1400 (1500 internet minus ZT/encap overhead). Large bridge responses
# (the usage JSON) get black-holed by broken PMTUD — the TCP handshake
# succeeds but the response never arrives ("Established ... 0 bytes received").
#
# Fix: lower the zt interface MTU and clamp the MSS advertised on outgoing
# SYNs so every segment fits end-to-end across the home LAN.
#
# Install as a boot-persistent service on the VPS that runs the bridge:
#   sudo install -m 0755 scripts/vps-zt-mtu-fix.sh /usr/local/sbin/rlcd-zt-fix.sh
#   sudo cp scripts/rlcd-zt-fix.service /etc/systemd/system/
#   sudo systemctl enable --now rlcd-zt-fix.service
#
# Usage (manual): sudo ./vps-zt-mtu-fix.sh [zt-interface]
set -e
# Find your ZeroTier interface name with: ip link show | grep zt
# Then run: sudo ./vps-zt-mtu-fix.sh <zt-interface>
IFACE="${1:?usage: $0 <zt-interface-name>}"
MSS=1360
for _ in $(seq 1 60); do
    ip link show "$IFACE" >/dev/null 2>&1 && break
    sleep 2
done
ip link show "$IFACE" >/dev/null 2>&1 || { echo "iface $IFACE never appeared"; exit 0; }
ip link set "$IFACE" mtu 1400
iptables -t mangle -C POSTROUTING -o "$IFACE" -p tcp --tcp-flags SYN,RST SYN -j TCPMSS --set-mss "$MSS" 2>/dev/null \
  || iptables -t mangle -A POSTROUTING -o "$IFACE" -p tcp --tcp-flags SYN,RST SYN -j TCPMSS --set-mss "$MSS"
echo "rlcd-zt-fix applied on $IFACE (mtu 1400, mss $MSS)"
