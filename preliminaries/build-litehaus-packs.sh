#!/bin/sh
# build-litehaus-packs.sh
# run on your local machine (WSL etc.)
# Generates per-region zips for Sydney + Singapore.

set -eu

OUTDIR="./build-litehaus/new"
SYD_DIR="$OUTDIR/sydney"
SGP_DIR="$OUTDIR/singapore"

echo "==> preparing output dirs"
rm -rf "$OUTDIR"
mkdir -p "$SYD_DIR" "$SGP_DIR"

# Common files we will embed for both regions:
write_common_files() {
  DEST="$1"

  # server.js (beacon)
  cat > "$DEST/server.js" <<'EOF'
REPLACED_BEACON_JS
EOF

  # listener server.js
  mkdir -p "$DEST/litehaus-listener"
  cat > "$DEST/litehaus-listener/server.js" <<'EOF'
REPLACED_LISTENER_JS
EOF

  # ecosystem.config.js
  cat > "$DEST/ecosystem.config.js" <<'EOF'
REPLACED_PM2_CONFIG
EOF

  # start-lighthouse.sh
  cat > "$DEST/start-lighthouse.sh" <<'EOF'
REPLACED_START_LIGHTHOUSE
EOF
  chmod +x "$DEST/start-lighthouse.sh"

  # package.json
  cat > "$DEST/package.json" <<'EOF'
REPLACED_PACKAGE_JSON
EOF

  # systemd units
  cat > "$DEST/ultimate-litehaus.service" <<'EOF'
REPLACED_BEACON_SERVICE
EOF

  cat > "$DEST/litehaus-listener.service" <<'EOF'
REPLACED_LISTENER_SERVICE
EOF

  # installer
  cat > "$DEST/install-litehaus.sh" <<'EOF'
REPLACED_INSTALLER
EOF
  chmod +x "$DEST/install-litehaus.sh"

  # placeholder dashboard binary so path exists
  touch "$DEST/litehaus"
  chmod +x "$DEST/litehaus"
}

############################################
# Sydney content
############################################
write_common_files "$SYD_DIR"

cat > "$SYD_DIR/.env" <<'EOF'
SITE_NAME="Litehaus Sydney"

LITEHAUS_REGION="SYD-AU"
LITEHAUS_NAME="litehaus-sydney"

BEACON_IP="134.199.170.197"

PORT=3000
WS_PORT=3001

LITEHAUS_PORT=9876
LISTENER_PORT=9876

LIGHTHOUSE_EXECUTABLE="./litehaus"

UPDATE_INTERVAL=250
LOG_LEVEL="info"

BEACON_INTERVAL_SEC=0.25

TARGET_URL=""
EOF

############################################
# Singapore content
############################################
write_common_files "$SGP_DIR"

cat > "$SGP_DIR/.env" <<'EOF'
SITE_NAME="Litehaus Singapore"

LITEHAUS_REGION="SGP-SG"
LITEHAUS_NAME="litehaus-singapore"

BEACON_IP="68.183.227.135"

PORT=3000
WS_PORT=3001

LITEHAUS_PORT=6789
LISTENER_PORT=6789

LIGHTHOUSE_EXECUTABLE="./litehaus"

UPDATE_INTERVAL=4000
LOG_LEVEL="info"

BEACON_INTERVAL_SEC=2

TARGET_URL=""
EOF

############################################
# Make the zips
############################################
echo "==> zipping"
(cd "$SYD_DIR" && zip -r ../litehaus-sydney.zip . >/dev/null)
(cd "$SGP_DIR" && zip -r ../litehaus-singapore.zip . >/dev/null)

echo "==> done"
echo "Sydney pack:     $OUTDIR/litehaus-sydney.zip"
echo "Singapore pack:  $OUTDIR/litehaus-singapore.zip"
