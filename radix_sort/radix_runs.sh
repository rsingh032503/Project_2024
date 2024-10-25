#!/bin/bash

processors=$1

array_sizes=(16 18 20 22 24 26 28)

input_type=("reverse_sorted")

# Loop through each combination
for size in "${array_sizes[@]}"; do
    for type in "${input_type[@]}"; do
        echo "Submitting job with array size 2^$size and $processors processors and input type $type"
        sbatch "radix_sort.grace_job_${processors}" $size $processors $type
        
        sleep 1
    done
done