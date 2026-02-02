#!/bin/bash

# Get the absolute path of the directory where this script is located
INSTALL_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
SERVICE_NAME="musicbottles.service"
SERVICE_FILE="/etc/systemd/system/$SERVICE_NAME"

echo "Installing $SERVICE_NAME..."

# Create the systemd service file
# We run as root and set the WorkingDirectory so runDemo.sh can find its files
sudo bash -c "cat > $SERVICE_FILE" <<EOF
[Unit]
Description=Music Bottles Service
After=network.target

[Service]
Type=simple
WorkingDirectory=$INSTALL_DIR
ExecStart=/bin/bash $INSTALL_DIR/runDemo.sh
Restart=always
User=root

[Install]
WantedBy=multi-user.target
EOF

# Reload systemd, enable and start the service
sudo systemctl daemon-reload
sudo systemctl enable $SERVICE_NAME
sudo systemctl start $SERVICE_NAME

echo "Installation complete. $SERVICE_NAME is running."
