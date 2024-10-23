#!/bin/bash

# Array sizes (powers of 2)
array_sizes=(16 18 20 22 24 26 28)

# Number of processors
processors=(2 4 8 16 32 64 128)

# Loop through each combination
for size in "${array_sizes[@]}"; do
    for proc in "${processors[@]}"; do
        echo "Submitting job with array size 2^$size and $proc processors"
        sbatch "mpi.grace_job_${proc}" "$size" "$proc" "Random"
        
        sleep 5
    done
done