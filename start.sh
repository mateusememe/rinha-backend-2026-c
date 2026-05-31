#!/bin/bash
set -e

if [ "$1" == "loader" ]; then
    echo "Starting loader bootstrap..."
    
    # Se o arquivo .gz estiver na pasta resources, descompacta
    if [ -f "/app/resources/references.json.gz" ]; then
        echo "Decompressing references.json.gz..."
        zcat /app/resources/references.json.gz > /app/resources/references.json
    fi

    if [ ! -f "/app/resources/references.json" ]; then
        echo "ERROR: references.json not found in /app/resources"
        ls -la /app/resources
        exit 1
    fi
    
    /usr/local/bin/data-loader
    exit 0
fi

/usr/local/bin/fraud-api $PORT
