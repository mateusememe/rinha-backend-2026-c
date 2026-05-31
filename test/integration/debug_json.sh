#!/bin/bash
PAYLOAD='{"id":"tx-1","amount":100.0,"installments":1,"customer":{"avg_amount":50.0,"tx_count_24h":2,"known_merchants":["MERC-1"]},"merchant":{"id":"MERC-1","mcc":"5411","avg_amount":80.0},"terminal":{"is_online":true,"card_present":false,"km_from_home":10.0},"last_transaction":null}'
curl -v -X POST http://localhost:9999/fraud-score -H "Content-Type: application/json" -d "$PAYLOAD"
