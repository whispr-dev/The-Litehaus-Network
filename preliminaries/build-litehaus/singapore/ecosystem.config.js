/*
 * PM2 ecosystem config.
 * We assume:
 *   - /opt/litehaus/.env will be present after deploy
 *   - install-litehaus.sh will set up /var/log/litehaus for logging
 *
 * We read basic runtime vars from process.env (which PM2 inherits).
 */

const fs = require('fs');
const path = require('path');

// load .env manually into process.env before exporting config
(function loadEnv() {
  try {
    const envPath = path.join(__dirname, '.env');
    if (fs.existsSync(envPath)) {
      const data = fs.readFileSync(envPath, 'utf8');
      for (const line of data.split('\n')) {
        if (!line.trim() || line.trim().startsWith('#')) continue;
        const idx = line.indexOf('=');
        if (idx === -1) continue;
        const k = line.slice(0, idx).trim();
        const v = line.slice(idx + 1).trim();
        if (!process.env[k]) process.env[k] = v;
      }
    }
  } catch (e) {
    // swallow - we don't want to crash on first boot
  }
})();

const siteName = process.env.SITE_NAME || "Litehaus";

module.exports = {
  apps: [{
    name: siteName,
    script: "./server.js",
    watch: false,
    env: {
      NODE_ENV: process.env.NODE_ENV || "production",
      PORT: process.env.PORT || 3000,
      WS_PORT: process.env.WS_PORT || 3001,
      BEACON_IP: process.env.BEACON_IP || "127.0.0.1",
      BEACON_PORT: process.env.BEACON_PORT || 6789,
      LISTENER_PORT: process.env.LISTENER_PORT || 6789,
      UPDATE_INTERVAL: process.env.UPDATE_INTERVAL || 4000,
      LOG_LEVEL: process.env.LOG_LEVEL || "info",
      SITE_NAME: siteName
    },
    out_file: `/var/log/litehaus/${siteName}.log`,
    error_file: `/var/log/litehaus/${siteName}-error.log`,
    merge_logs: true
  }]
};
