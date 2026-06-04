#!/bin/bash
set -e

if [ "$1" == "loader" ]; then
    echo "Starting loader bootstrap..."
    mkdir -p /app/resources /app/data
    /usr/local/bin/data-loader
    exit 0
fi

if [ "$1" == "lb" ]; then
    echo "Starting UDS load balancer..."
    /usr/local/bin/lb
    exit 0
fi

echo "Starting API..."
/usr/local/bin/fraud-api
