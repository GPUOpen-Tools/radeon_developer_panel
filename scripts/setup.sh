#!/bin/bash
# Copyright (c) 2021-2025 Advanced Micro Devices, Inc. All rights reserved.

# RMV support setup

# Create specific kernel tracing instance if it does not already exists
if [ ! -d "/sys/kernel/tracing/instances/amd_rmv" ]; then
  sudo mkdir "/sys/kernel/tracing/instances/amd_rmv"
fi

# Change read/write permissions of tracing directories
sudo chmod 755 /sys/kernel/tracing/
sudo chmod 755 /sys/kernel/tracing/instances/
sudo chmod -R 755 /sys/kernel/tracing/instances/amd_rmv

# Change ownership of amd_rmv tracing directory and all subdirectories to current user
current_user=$(who | awk '{print $1}' | head -1)
sudo chown $current_user -R /sys/kernel/tracing/instances/amd_rmv

echo "done"
