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

# --- Remove old build output if exists ---
echo "Cleaning old student_agent_module.so if present..."
rm -f build/student_agent_module.so

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

# --- Rename the output to student_agent_module.so ---
echo "Renaming output file..."
# Find the built .so file with ABI suffix
so_file=$(find . -maxdepth 1 -type f -name "student_agent_module*.so" | head -n 1)
if [ -n "$so_file" ]; then
    mv "$so_file" student_agent_module.so
    echo "Renamed $so_file -> build/student_agent_module.so"
else
    echo "Error: No student_agent_module .so file was generated."
    exit 1
fi

# --- Return to Root Directory ---
cd ..

echo "Compilation successful! You can now run the agent with build/student_agent_module.so"
