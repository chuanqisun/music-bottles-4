# Finding the Raspberry Pi (`musicbottle`) on the Local Wi-Fi Network

> [!NOTE] Current device mac address is `dc:a6:32:01:10:c7`

This guide documents how to locate the Raspberry Pi on a local Wi-Fi network from a Linux machine, even if its hostname does not resolve or its vendor database fails to load.

---

### Prerequisites

Ensure you have `arp-scan` installed:

```bash
# Ubuntu / Debian
sudo apt install arp-scan

# Fedora / RHEL
sudo dnf install arp-scan

# Arch Linux
sudo pacman -S arp-scan
```

---

### Method 1: Using the Automated Script (Recommended)

An automated script `find_pi.sh` is provided to detect your wireless interface and scan for known Raspberry Pi hardware addresses automatically.

1. **Make the script executable:**

   ```bash
   chmod +x find_pi.sh
   ```

2. **Run with `sudo`:**

   ```bash
   sudo ./find_pi.sh
   ```

3. **Example Output:**

   ```text
   [*] Using network interface: wlp194s0
   [*] Scanning for Raspberry Pi devices on local network...

   [+] Raspberry Pi found!
   IP Address         MAC Address
   -------------------------------------
   192.168.1.145      dc:a6:32:1a:2b:3c

   Quick connect: ssh <user>@192.168.1.145
   ```

---

### Method 2: Manual Scan

If you prefer to run the commands directly:

#### 1. Identify your local Wi-Fi interface

```bash
ip -br a
```

Find your active wireless interface starting with `wl...` (e.g., `wlp194s0` or `wlan0`).

#### 2. Run the scan

Run `arp-scan` with the `-N` flag (to bypass vendor lookup permission warnings) and filter for Raspberry Pi OUI prefixes:

```bash
sudo arp-scan -N --interface=<YOUR_WIFI_IFACE> --localnet | grep -Ei "(b8:27:eb|dc:a6:32|e4:5f:01|28:cd:c1|d8:3a:dd|2c:cf:67|88:a2:9e|98:fe:54)"
```

> **Example:**
>
> ```bash
> sudo arp-scan -N --interface=wlp194s0 --localnet | grep -Ei "(b8:27:eb|dc:a6:32|e4:5f:01|28:cd:c1|d8:3a:dd|2c:cf:67|88:a2:9e|98:fe:54)"
> ```

#### 3. Interpret the output

```text
192.168.1.145    dc:a6:32:1a:2b:3c
```

- **Column 1:** Assigned IP address.
- **Column 2:** MAC address. Pi 4 devices commonly start with **`dc:a6:32`** or **`e4:5f:01`**.

---

### Connect to the Device

Once the IP is obtained:

1. **Test response:**

   ```bash
   ping -c 3 <IP_ADDRESS>
   ```

2. **Connect via SSH:**
   ```bash
   ssh <username>@<IP_ADDRESS>
   ```

---

### Troubleshooting

- **No output returned / Device not found:**
  - Verify that your Linux machine and the Raspberry Pi are connected to the exact same Wi-Fi network/SSID.
  - Check that **Client/AP Isolation** is disabled on the router (common on guest networks, which blocks peer-to-peer discovery).
  - Ensure the Raspberry Pi is fully powered on (wait 1–2 minutes after boot).
- **Interface detection issues in `find_pi.sh`:**
  - If you use a non-standard interface name (e.g., tethering or Ethernet), the script will prompt you to enter the interface name manually.
