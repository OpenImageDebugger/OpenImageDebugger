#!/bin/bash

# Build script for OpenImageDebugger with protobuf, conan, ninja, and ccache

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Building OpenImageDebugger with protobuf, conan, ninja, and ccache${NC}"

# Check if required tools are installed
check_tool() {
    if ! command -v $1 &> /dev/null; then
        echo -e "${RED}Error: $1 is not installed${NC}"
        exit 1
    fi
}

echo -e "${YELLOW}Checking required tools...${NC}"
check_tool conan
check_tool ninja
check_tool ccache

# Install conan dependencies
echo -e "${YELLOW}Installing conan dependencies...${NC}"
conan install . --output-folder=build --build=missing --profile:build=default --profile:host=default

# Configure with cmake using the conan preset
echo -e "${YELLOW}Configuring CMake...${NC}"
cmake --preset default --fresh

# Build the project
echo -e "${YELLOW}Building project...${NC}"
cmake --build --preset default --parallel $(nproc)

echo -e "${GREEN}Build completed successfully!${NC}"

# Show ccache statistics
if command -v ccache &> /dev/null; then
    echo -e "${YELLOW}CCCache statistics:${NC}"
    ccache -s
fi