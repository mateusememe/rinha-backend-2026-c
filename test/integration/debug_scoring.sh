#!/bin/bash
echo "Testing low amount (legit pattern):"
PAYLOAD_LEGIT='{"transaction":{"amount":0.0},"customer":{"avg_amount":0.0},"merchant":{},"terminal":{"km_from_home":0.0}}'
curl -s -X POST http://localhost:9999/fraud-score -H "Content-Type: application/json" -d "$PAYLOAD_LEGIT"
echo -e "\nTesting high amount (fraud pattern):"
PAYLOAD_FRAUD='{"transaction":{"amount":10000.0},"customer":{"avg_amount":0.0},"merchant":{},"terminal":{"km_from_home":0.0}}'
curl -s -X POST http://localhost:9999/fraud-score -H "Content-Type: application/json" -d "$PAYLOAD_FRAUD"
echo ""
