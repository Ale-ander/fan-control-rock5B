#!/bin/bash

if [ "$EUID" -ne 0 ]; then 
  echo "Need root privileges. Please run with sudo."
  exit
fi

APP_NAME="fan-control-rock5b"
SOURCE_FILE="fan-control.c"
CONFIG_FILE="fan-control.json"
BIN_PATH="/usr/local/bin/$APP_NAME"
SERVICE_FILE="/etc/systemd/system/$APP_NAME.service"
CONF_DEST="/etc/$CONFIG_FILE"

echo "--- Installing $APP_NAME ---"

echo "[1/5] Installing dependencies (libcjson-dev)..."
apt-get update && apt-get install -y libcjson-dev gcc

echo "[2/5] Compiling..."
gcc -O3 "$SOURCE_FILE" -lcjson -o "$APP_NAME"
if [ $? -ne 0 ]; then
    echo "Error during compilation!"
    exit 1
fi
mv "$APP_NAME" "$BIN_PATH"
chmod +x "$BIN_PATH"

if [ ! -f "$CONF_DEST" ]; then
    echo "[3/5] Copying configuration file to $CONF_DEST..."
    cp "$CONFIG_FILE" "$CONF_DEST"
else
    echo "[3/5] Configuration file already exists, skipping copy."
fi

echo "[4/5] Creating systemd service..."
cat <<EOF > "$SERVICE_FILE"
[Unit]
Description=Rock5B 1-Wire Fan Control Service
After=network.target

[Service]
Type=simple
ExecStart=$BIN_PATH
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

# 5. Avvio Servizio
echo "[5/5] Starting service..."
systemctl daemon-reload
systemctl enable "$APP_NAME.service"
systemctl restart "$APP_NAME.service"

echo "--- Installation completed! ---"
echo "You can check the status with: systemctl status $APP_NAME"
echo "You can view the logs with: journalctl -u $APP_NAME -f"