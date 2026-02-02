#!/bin/bash

SERVICE_NAME="musicbottles.service"
SERVICE_FILE="/etc/systemd/system/$SERVICE_NAME"

echo "Uninstalling $SERVICE_NAME..."

if [ -f "$SERVICE_FILE" ]; then
    echo "Stopping and disabling service..."
    sudo systemctl stop $SERVICE_NAME
    sudo systemctl disable $SERVICE_NAME
    echo "Removing service file..."
    sudo rm "$SERVICE_FILE"
    sudo systemctl daemon-reload
    echo "Service removed."
else
    echo "Service $SERVICE_NAME not found, skipping."
fi

echo "Uninstallation complete."
