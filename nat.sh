#!/bin/bash
PROXY_PORT=8080
PROXY_GROUP="sm3tproxy"

setup_nat() {

    iptables -t nat -F OUTPUT
    iptables -t nat -A OUTPUT -o lo -j RETURN

    iptables -t nat -A OUTPUT -m owner --gid-owner "$PROXY_GROUP" -j RETURN

    iptables -t nat -A OUTPUT -p tcp --dport 80 -j REDIRECT --to-port "$PROXY_PORT"
    iptables -t nat -A OUTPUT -p tcp --dport 443 -j REDIRECT --to-port "$PROXY_PORT"

    echo "[*] Setup complete!"
    iptables -t nat -L OUTPUT -n -v --line-numbers
}

cleanup_nat() {
    iptables -t nat -F OUTPUT
    echo "[*] Cleanup complete."
}

case "$1" in
setup) setup_nat ;;
cleanup) cleanup_nat ;;
*)
    echo "Usage: $0 {setup|cleanup}"
    exit 1
    ;;
esac
