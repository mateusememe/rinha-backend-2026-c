#!/bin/bash
set -e

if [ "$1" == "loader" ]; then
    echo "Starting loader bootstrap..."
    mkdir -p /app/resources
    
    # Ordem de busca que funcionou no Smoke Test
    if [ -f "/dataset_up/resources/references.json.gz" ]; then
        echo "Found references at /dataset_up/resources/references.json.gz"
        zcat "/dataset_up/resources/references.json.gz" > /app/resources/references.json
    elif [ -f "/dataset_here/resources/references.json.gz" ]; then
        echo "Found references at /dataset_here/resources/references.json.gz"
        zcat "/dataset_here/resources/references.json.gz" > /app/resources/references.json
    elif [ -f "resources/references.json.gz" ]; then
        echo "Found references at resources/references.json.gz"
        zcat "resources/references.json.gz" > /app/resources/references.json
    elif [ -f "/app/resources/references.json.gz" ]; then
        echo "Found references at /app/resources/references.json.gz"
        zcat "/app/resources/references.json.gz" > /app/resources/references.json
    else
        echo "ERROR: Could not find references.json.gz anywhere!"
        exit 1
    fi
    
    /usr/local/bin/data-loader
    exit 0
fi

/usr/local/bin/fraud-api ${PORT:-9999}
