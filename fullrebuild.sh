#!/bin/bash

# Navigate to the source directory
cd ~/os161/src || exit

# Rebuild the entire source with 24 parallel jobs
bmake fullrebuild -j 24

# Install the build
bmake install

# Navigate to the kernel compile directory for DUMBVM
cd ~/os161/src/kern/compile/DUMBVM || exit

# Build dependencies and the kernel with 24 parallel jobs, then install
bmake depend -j 24
bmake -j 24
bmake install

# Navigate to the root directory
cd ~/os161/src/root || exit

