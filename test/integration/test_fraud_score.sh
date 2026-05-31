#!/bin/bash
set -e
make all

# Start data-loader in background
./bin/data-loader &
LOADER_PID=$!

# Start API in background
./bin/fraud-api 9001 &
API_PID=$!

# Wait for API and loader
sleep 2

# Check fraud score
PAYLOAD='{"id":"tx-1","transaction":{"amount":100.0,"installments":1,"requested_at":"2026-05-18T13:34:59.000Z"},"customer":{"avg_amount":50.0,"tx_count_24h":2,"known_merchants":["MERC-1"]},"merchant":{"id":"MERC-1","mcc":"5411","avg_amount":80.0},"terminal":{"is_online":true,"card_present":false,"km_from_home":10.0},"last_transaction":null}'
RESPONSE=$(curl -s -X POST http://localhost:9001/fraud-score -H "Content-Type: application/json" -d "$PAYLOAD")
echo "Fraud score response: $RESPONSE"

if [[ "$RESPONSE" == *"approved"* ]]; then
    echo "T020 PASS"
else
    echo "T020 FAIL"
    kill $API_PID $LOADER_PID
    exit 1
fi

kill $API_PID $LOADER_PID
