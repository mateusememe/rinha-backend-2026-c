#!/bin/bash
set -e

# Mostrar info de ambiente para debug
echo "--- Environment Info ---"
echo "User: $(whoami)"
echo "PWD: $(pwd)"
ls -la /app || true

if [ "$1" == "loader" ]; then
    echo "Starting loader bootstrap..."
    
    # Garantir que a pasta de destino existe
    mkdir -p /app/resources
    
    SEARCH_PATHS=("/dataset_up/resources" "/dataset_here/resources" "/dataset_up" "/dataset_here" "/app/resources" ".")
    FOUND=""

    for path in "${SEARCH_PATHS[@]}"; do
        if [ -f "$path/references.json.gz" ]; then
            echo "SUCCESS: Found $path/references.json.gz, decompressing..."
            zcat "$path/references.json.gz" > /app/resources/references.json
            FOUND="yes"
            break
        elif [ -f "$path/references.json" ]; then
            echo "SUCCESS: Found $path/references.json, copying..."
            cp "$path/references.json" /app/resources/references.json
            FOUND="yes"
            break
        fi
    done

    if [ -z "$FOUND" ]; then
        echo "FATAL ERROR: Dataset NOT FOUND!"
        echo "Check /dataset_up/resources:"
        ls -la /dataset_up/resources || true
        echo "Check /dataset_here:"
        ls -la /dataset_here || true
        exit 1
    fi
    
    echo "Starting data-loader binary..."
    /usr/local/bin/data-loader
    exit 0
fi

echo "Starting fraud-api on port ${PORT:-9999}..."
exec /usr/local/bin/fraud-api ${PORT:-9999}
