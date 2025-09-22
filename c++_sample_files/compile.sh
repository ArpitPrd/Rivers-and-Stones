#!/bin/bash

# Exit immediately if a command exits with a non-zero status.
set -e

# --- Install Dependencies ---
echo "Checking for pybind11 dependency..."
if python3 -c "import pybind11" &> /dev/null; then
    echo "pybind11 is already installed."
else
    echo "pybind11 not found. Installing..."
    pip install pybind11
fi

# --- Create Build Directory ---
echo "Creating build directory..."
mkdir -p build
cd build

# --- Run CMake to Configure the Project ---
echo "Configuring project with CMake..."
cmake .. -Dpybind11_DIR=$(python3 -m pybind11 --cmakedir) \
         -DCMAKE_C_COMPILER=gcc \
         -DCMAKE_CXX_COMPILER=g++

# --- Build the Project ---
echo "Building the project with make..."
make

# --- Return to Root Directory ---
cd ..

echo "Compilation successful! You can now run the agent."
