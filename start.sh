#!/bin/bash
set -e

if [ "$1" == "loader" ]; then
    echo "Starting loader bootstrap..."
    mkdir -p /app/resources /app/data
    
    # data-loader agora lê diretamente do .gz via zlib streaming
    # Não precisa mais descomprimir com zcat
    /usr/local/bin/data-loader
    exit 0
fi

/usr/local/bin/fraud-api ${PORT:-9999}
