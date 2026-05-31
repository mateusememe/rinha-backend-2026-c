#!/bin/bash
if [ "$1" == "loader" ]; then
    /usr/local/bin/data-loader
    exit 0
fi

/usr/local/bin/fraud-api $PORT
