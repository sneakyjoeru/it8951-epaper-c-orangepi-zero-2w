#!/bin/bash
# setup.sh — OS setup for IT8951 e-paper display on Orange Pi Zero 2W
# This script is called by: ./it8951 --setup
# It configures the boot overlay and installs required packages.

set -e

echo "=== IT8951 E-Paper Display OS Setup ==="
echo ""

# 1. Install packages
echo "1. Installing packages..."
sudo apt update
sudo apt install -y \
    libgpiod-dev \
    libfreetype-dev \
    gcc \
    make \
    python3-pil \
    python3-libgpiod \
    python3-spidev \
    python3-numpy
echo "   Packages installed."
echo ""

# 2. Configure boot overlay
echo "2. Configuring boot overlay..."
ENV_FILE="/boot/orangepiEnv.txt"
NEEDED_OVERLAY="spi1-cs1-spidev"

if [ -f "$ENV_FILE" ]; then
    CURRENT_OVERLAY=$(grep "^overlays=" "$ENV_FILE" 2>/dev/null | cut -d= -f2)
    if [ "$CURRENT_OVERLAY" = "$NEEDED_OVERLAY" ]; then
        echo "   Overlay already correct ($NEEDED_OVERLAY)"
    else
        sudo sed -i "s/^overlays=.*/overlays=$NEEDED_OVERLAY/" "$ENV_FILE"
        echo "   Changed overlay from '$CURRENT_OVERLAY' to '$NEEDED_OVERLAY'"
        echo "   REBOOT REQUIRED!"
    fi
else
    echo "   $ENV_FILE not found!"
    echo "   Please manually add: overlays=$NEEDED_OVERLAY"
fi
echo ""

# 3. Check current status
echo "3. Checking current status..."
if [ -e /dev/spidev1.1 ]; then
    echo "   /dev/spidev1.1 exists ✓"
else
    echo "   /dev/spidev1.1 NOT found — REBOOT required!"
    echo "   Run: sudo reboot"
fi

# Check GPIO 229 is free
GPIO_229=$(sudo cat /sys/kernel/debug/gpio 2>/dev/null | grep "gpio-229")
if [ -z "$GPIO_229" ]; then
    echo "   GPIO 229 (CS) is free ✓"
else
    echo "   GPIO 229 is kernel-claimed: $GPIO_229"
    echo "   May need reboot to free it."
fi
echo ""

echo "=== Setup complete! ==="
echo ""
if [ ! -e /dev/spidev1.1 ]; then
    echo "⚠️  Reboot required. Run: sudo reboot"
    echo "   After reboot, run: sudo ./it8951 --info"
else
    echo "✓  Ready! Run: sudo ./it8951 --info"
fi