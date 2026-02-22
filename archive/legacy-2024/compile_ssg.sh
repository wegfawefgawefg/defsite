#!/bin/bash

# Navigate to the Cargo project directory
cd ./ssg

# Build the project in release mode
cargo build --release

# Check if compilation was successful
if [ $? -eq 0 ]; then
    echo "Compilation successful! Woo!"
    # Copy the binary to the parent directory
    cp ./target/release/generate ../
    echo "Binary copied to parent directory."
else
    echo "Compilation failed. Please check your code and try again."
fi

# Return to the original directory
cd ..
