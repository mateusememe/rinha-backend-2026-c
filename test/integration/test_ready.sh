#!/bin/bash
set -e
make clean && make all

# Start data-loader in background
./bin/data-loader &
LOADER_PID=$!

# Start API in background
./bin/fraud-api 9001 &
API_PID=$!

# Wait for API and loader
sleep 2

# Check readiness
RESPONSE=$(curl -s http://localhost:9001/ready)
echo "Readiness response: $RESPONSE"

if [ "$RESPONSE" == "OK" ]; then
    echo "T011 PASS"
else
    echo "T011 FAIL"
    kill $API_PID $LOADER_PID
    exit 1
fi

kill $API_PID $LOADER_PID
