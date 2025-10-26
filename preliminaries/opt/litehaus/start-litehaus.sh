#!/bin/sh
# /opt/litehaus/start-lighthouse.sh
# tiny helper to run the dashboard binary if/when you wire it in

. /opt/litehaus/.env

echo "Starting dashboard for $SITE_NAME ($LITEHAUS_REGION)"
echo "Binary: $LIGHTHOUSE_EXECUTABLE"
echo "Web:    http://0.0.0.0:${PORT}"
echo "WS:     ws://0.0.0.0:${WS_PORT}"

exec "$LIGHTHOUSE_EXECUTABLE"
