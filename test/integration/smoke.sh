#!/bin/bash
set -e
make all
./bin/fraud-api &
API_PID=$!
sleep 1
PAYLOAD='{"id":"tx-1","amount":100.0,"installments":1,"avg_amount":50.0,"tx_count_24h":2,"mcc":"5411","km_from_home":10.0}'
curl -s -X POST http://localhost:9001/fraud-score -H "Content-Type: application/json" -d "$PAYLOAD"
kill $API_PID
