#!/bin/bash

SCRIPT_DIR=$(dirname "$BASH_SOURCE")

PID=$(pgrep Goggle3)

if [ -z "$PID" ]; then
    cd "$SCRIPT_DIR"
    "./build/Goggle3"
else
    kill -USR1 "$PID"
fi
