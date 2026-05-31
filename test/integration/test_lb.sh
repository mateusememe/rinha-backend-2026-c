#!/bin/bash
set -e
make all
./bin/fraud-api &
API_PID=$!
sleep 1
./bin/lb localhost:9001 &
LB_PID=$!
sleep 1
RESPONSE=$(curl -s http://localhost:9999/ready)
echo "LB Response: $RESPONSE"
if [ "$RESPONSE" == "OK" ]; then
    echo "LB check: PASS"
else
    echo "LB check: FAIL"
    kill $API_PID $LB_PID
    exit 1
fi
kill $API_PID $LB_PID
