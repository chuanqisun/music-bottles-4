#!/bin/bash

# Compile scaleTool if it doesn't exist or is older than source
if [ ! -f scaleTool ] || [ scaleTool.c -nt scaleTool ]; then
    echo "Compiling scaleTool..."
    gcc -o scaleTool scaleTool.c hx711.c gb_common.c
    if [ $? -ne 0 ]; then
        echo "Compilation failed."
        exit 1
    fi
fi

# Run the tool with sudo
echo "Running scaleTool..."
sudo ./scaleTool
