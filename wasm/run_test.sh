#!/bin/bash
# Run VOLE WASM test
# 1. Starts native sender (Alice)
# 2. Starts WebSocket proxy
# 3. Starts HTTP server for WASM receiver (Bob)
# Open http://localhost:8000/vole_portable.html in browser

set -e

cd "$(dirname "$0")"

# Kill any previous processes
pkill -f "vole_sender" 2>/dev/null || true
pkill -f "ws_proxy.js" 2>/dev/null || true
pkill -f "http.server" 2>/dev/null || true
sleep 1

echo "=== Starting VOLE WASM Test ==="
echo ""

# Start native sender
echo "1. Starting native sender (Alice) on port 12345..."
cd ../native/build
./vole_sender 12345 &
SENDER_PID=$!
cd ../../wasm
sleep 1

# Start WebSocket proxy
echo "2. Starting WebSocket proxy (8080 -> 12345)..."
node ws_proxy.js &
PROXY_PID=$!
sleep 1

# Start HTTP server
echo "3. Starting HTTP server on port 8000..."
python3 -m http.server 8000 &
HTTP_PID=$!
sleep 1

echo ""
echo "=== All services running ==="
echo ""
echo "Open in browser: http://localhost:8000/vole_receiver.html"
echo ""
echo "Press Ctrl+C to stop all services"

# Wait for Ctrl+C
trap "kill $SENDER_PID $PROXY_PID $HTTP_PID 2>/dev/null; exit 0" INT
wait
