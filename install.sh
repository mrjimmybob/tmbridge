#!/bin/sh

set -e

if [ "$(id -u)" -ne 0 ]; then
    echo "Please run as root."
    exit 1
fi

echo "Installing tmbridge..."

# Create service user (ignore if it already exists)
id -u tmbridge >/dev/null 2>&1 || \
    useradd --system --home /nonexistent --shell /usr/sbin/nologin tmbridge

# Install binary
install -d /opt/tmbridge
install -m 755 tmbridge /opt/tmbridge/tmbridge

# Install configuration (don't overwrite an existing one)
install -d /etc/tmbridge

if [ ! -f /etc/tmbridge/tmbridge.conf ]; then
    install -m 644 tmbridge.conf /etc/tmbridge/tmbridge.conf
fi

# Install systemd service
install -m 644 tmbridge.service /etc/systemd/system/tmbridge.service

systemctl daemon-reload
systemctl enable tmbridge
systemctl restart tmbridge

echo
echo "tmbridge installed successfully."
echo
systemctl --no-pager --full status tmbridge
