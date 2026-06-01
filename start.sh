#!/bin/bash
set -e

if [ "$1" == "loader" ]; then
    echo "Starting loader bootstrap..."
    mkdir -p /app/resources
    
    # Ordem de busca que funcionou no Smoke Test
    if [ -f "/dataset_up/resources/references.json.gz" ]; then
        zcat "/dataset_up/resources/references.json.gz" > /app/resources/references.json
    elif [ -f "/dataset_here/resources/references.json.gz" ]; then
        zcat "/dataset_here/resources/references.json.gz" > /app/resources/references.json
    elif [ -f "resources/references.json.gz" ]; then
        zcat "resources/references.json.gz" > /app/resources/references.json
    fi
    
    /usr/local/bin/data-loader
    exit 0
fi

/usr/local/bin/fraud-api ${PORT:-9999}
