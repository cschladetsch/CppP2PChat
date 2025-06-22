#!/bin/bash

# Build script for P2P Chat System

set -e

echo "Building P2P Chat System..."

# Create build directory
mkdir -p build
cd build

# Configure with CMake only if needed
if [ ! -f "CMakeCache.txt" ]; then
    echo "Configuring with CMake..."
    cmake ..
fi

# Build
echo "Building..."
make -j$(nproc)

# Run tests if built
if [ -f "Bin/TestCrypto" ]; then
    echo "Running tests..."
    ctest --verbose
fi

echo "Build complete! Executable: build/Bin/p2pchat"
echo "Run with: ./build/Bin/p2pchat"