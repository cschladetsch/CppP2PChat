#!/bin/bash

# Build script for P2P Chat System

set -e

echo "Building P2P Chat System..."

# Create build directory
mkdir -p build
cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake ..

# Build
echo "Building..."
make -j$(nproc)

# Run tests if built
if [ -f "test_crypto" ]; then
    echo "Running tests..."
    ctest --verbose
fi

echo "Build complete! Executable: build/p2pchat"
echo "Run with: ./build/p2pchat"