#!/bin/sh
# /opt/install-litehaus.sh
# run this ON THE DROPLET after unzipping the region pack into /opt/litehaus
# example: ssh wofl@IP "sudo sh /opt/install-litehaus.sh"

set -eu

echo "==> Litehaus installer starting"

# sanity
if [ ! -d /opt/litehaus ]; then
  echo "ERROR: /opt/litehaus missing"
  exit 1
fi

# make runtime dirs
sudo mkdir -p /opt/litehaus/litehaus-listener
sudo mkdir -p /var/log/litehaus
sudo chown -R wofl:wofl /opt/litehaus /var/log/litehaus

# ensure node modules
cd /opt/litehaus
echo "==> npm install"
sudo -u wofl npm install

# systemd units
echo "==> installing systemd units"
sudo cp /opt/litehaus/ultimate-litehaus.service /etc/systemd/system/ultimate-litehaus.service
sudo cp /opt/litehaus/litehaus-listener.service /etc/systemd/system/litehaus-listener.service

sudo systemctl daemon-reload

# enable and start both
sudo systemctl enable --now ultimate-litehaus.service
sudo systemctl enable --now litehaus-listener.service

# PM2 setup (optional but desirable for your reboot persistence layer)
echo "==> configuring pm2"
# install pm2 globally if missing
if ! command -v pm2 >/dev/null 2>&1; then
  sudo npm install -g pm2
fi

sudo -u wofl pm2 start /opt/litehaus/ecosystem.config.js
sudo -u wofl pm2 save
sudo pm2 startup systemd -u wofl --hp /home/wofl >/dev/null 2>&1 || true

echo "==> Litehaus install complete"
