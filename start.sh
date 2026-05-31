#!/bin/bash
set -e

if [ "$1" == "loader" ]; then
    echo "Starting loader bootstrap..."
    
    # Caminhos absolutos do ambiente da Rinha
    SEARCH_PATHS=("/dataset_up/resources" "/dataset_here/resources" "/dataset_up" "/dataset_here" "/app/resources" "/app" ".")
    FOUND=""

    for path in "${SEARCH_PATHS[@]}"; do
        if [ -f "$path/references.json.gz" ]; then
            echo "Found $path/references.json.gz, decompressing..."
            mkdir -p /app/resources
            zcat "$path/references.json.gz" > /app/resources/references.json
            FOUND="yes"
            break
        elif [ -f "$path/references.json" ]; then
            echo "Found $path/references.json, copying..."
            mkdir -p /app/resources
            cp "$path/references.json" /app/resources/references.json
            FOUND="yes"
            break
        fi
    done

    if [ -z "$FOUND" ]; then
        echo "ERROR: Dataset not found in mapped volumes."
        echo "Checking /dataset_up/resources:"
        ls -la /dataset_up/resources || true
        echo "Checking /dataset_here/resources:"
        ls -la /dataset_here/resources || true
        exit 1
    fi
    
    /usr/local/bin/data-loader
    exit 0
fi

/usr/local/bin/fraud-api $PORT
