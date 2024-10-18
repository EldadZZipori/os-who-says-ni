#!/bin/bash

# Navigate to the source directory
cd ~/os161/src/kern/compile/DUMBVM || exit

# Build dependencies and the kernel
bmake depend -j 24
bmake -j 24
bmake install

# Navigate to the root directory
cd ~/os161/src/root || exit

pwd
