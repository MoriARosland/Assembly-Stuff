#!/bin/bash

# Compile the program
gcc stetris.c -o stetris

# Check if compilation was successful
if [ $? -eq 0 ]; then
    echo "Compilation successful. Running program..."
    ./stetris
else
    echo "Compilation failed."
fi

