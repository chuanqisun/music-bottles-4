#!/usr/bin/env bash

# Exit immediately on failure, trace unbound variables
set -euo pipefail

# Check for root/sudo privileges (required by arp-scan)
if [[ $EUID -ne 0 ]]; then
    echo "[-] This script requires root privileges. Please run with sudo:"
    echo "    sudo $0"
    exit 1
fi

# Check if arp-scan is installed
if ! command -v arp-scan &> /dev/null; then
    echo "[-] arp-scan is not installed."
    echo "    Install it with: sudo apt install arp-scan (or equivalent for your distro)"
    exit 1
fi

# 1. Automatically detect the primary active wireless interface
WIFI_IFACE=$(ip -o link show | awk -F': ' '$2 ~ /^wl/ {print $2; exit}')

# Fallback: prompt user if no wireless interface starting with 'wl' was found
if [[ -z "${WIFI_IFACE:-}" ]]; then
    echo "[!] Could not automatically detect a Wi-Fi interface."
    read -rp "Please enter your network interface name manually (e.g., wlp194s0, wlan0): " WIFI_IFACE
fi

echo "[*] Using network interface: ${WIFI_IFACE}"
echo "[*] Scanning for Raspberry Pi devices on local network..."

# Known Raspberry Pi Foundation / Trading OUI prefixes
RPI_PREFIXES="b8:27:eb|dc:a6:32|e4:5f:01|28:cd:c1|d8:3a:dd|2c:cf:67|88:a2:9e|98:fe:54"

# 2. Run arp-scan and capture matches
# -N suppresses vendor lookup (avoids the permission denied warning)
# 2>/dev/null hides stderr noise
MATCHES=$(arp-scan -N --interface="${WIFI_IFACE}" --localnet 2>/dev/null | grep -Ei "${RPI_PREFIXES}" || true)

# 3. Output results
if [[ -z "${MATCHES}" ]]; then
    echo "[-] No Raspberry Pi devices found on the network."
    echo "    - Check that the Pi is powered on and connected to the same Wi-Fi SSID."
    echo "    - Ensure AP/Client isolation is disabled on your router."
    exit 1
fi

echo ""
echo "[+] Raspberry Pi found!"
printf "%-18s %-20s\n" "IP Address" "MAC Address"
echo "-------------------------------------"

echo "${MATCHES}" | while read -r ip mac rest; do
    printf "%-18s %-20s\n" "${ip}" "${mac}"
done

echo ""
echo "Quick connect: ssh <user>@$(echo "${MATCHES}" | awk 'NR==1{print $1}')"