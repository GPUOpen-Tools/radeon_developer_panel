# Copyright (c) 2021-2025 Advanced Micro Devices, Inc. All rights reserved.

printf "Removing shared memory .. "

rm -rf /tmp/qipc_sharedmemory* > /dev/null 2>&1
rm -rf /tmp/qipc_systemsem* > /dev/null 2>&1

# Remove the single instance semaphore containing the RadeonDeveloperServiceCLI GUID in the filename.
rm -rf /dev/shm/sem.D0939873-BA4B-4C4E-9729-D82DED85BC41 > /dev/null 2>&1
echo "done"
