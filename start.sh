#!/bin/bash
set -e

if [ "$1" == "loader" ]; then
    echo "Starting loader bootstrap..."
    
    SEARCH_PATHS=("/dataset_up" "/dataset_here" "/app/resources" "/app" ".")
    FOUND=""

    for path in "${SEARCH_PATHS[@]}"; do
        if [ -f "$path/references.json.gz" ]; then
            echo "Found $path/references.json.gz, decompressing..."
            zcat "$path/references.json.gz" > /app/resources/references.json
            FOUND="yes"
            break
        elif [ -f "$path/references.json" ]; then
            echo "Found $path/references.json, copying..."
            cp "$path/references.json" /app/resources/references.json
            FOUND="yes"
            break
        fi
    done

    if [ -z "$FOUND" ]; then
        echo "ERROR: Dataset not found in mapped volumes."
        echo "Checking /dataset_up:"
        ls -la /dataset_up || true
        echo "Checking /dataset_here:"
        ls -la /dataset_here || true
        exit 1
    fi
    
    /usr/local/bin/data-loader
    exit 0
fi

/usr/local/bin/fraud-api $PORT
