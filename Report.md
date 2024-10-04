# CSCE 435 Group project

## 0. Group number: 9
## 1. Group members:
1. Rahul Singh
2. Kevin Thomas
3. Anthony Ciardelli
4. Brandon Thomas
### Team Communication:
We will be using iMessage as our primary method of communication. We will share documents and information via Google Docs.

## 2. Project topic (e.g., parallel sorting algorithms)

### 2a. Brief project description (what algorithms will you be comparing and on what architectures)

- Bitonic Sort:
- Sample Sort:
- Merge Sort:
- Radix Sort:

### 2b. Pseudocode for each parallel algorithm
- For MPI programs, include MPI calls you will use to coordinate between processes

#### Sample Sort:
```
function sampleSort(local_data, world_rank, world_size):
    // Start Caliper measurement for entire Sample Sort

    // 1. Local sorting phase
    // Start Caliper measurement for local sort
    sort(local_data)
    // End Caliper measurement for local sort

    // 2. Sample selection phase
    // Start Caliper measurement for sample selection
    samples = select_samples(local_data, world_size)
    // End Caliper measurement for sample selection

    // 3. Global splitter selection phase
    // Start Caliper measurement for global splitter selection
    splitters = gather_and_select_global_splitters(samples, world_rank, world_size)
    // End Caliper measurement for global splitter selection

    // 4. Data partitioning phase
    // Start Caliper measurement for data partitioning
    partitioned_data = partition_data(local_data, splitters)
    // End Caliper measurement for data partitioning

    // 5. All-to-all exchange phase
    // Start Caliper measurement for all-to-all exchange
    received_data = all_to_all_exchange(partitioned_data, world_rank, world_size)
    // End Caliper measurement for all-to-all exchange

    // 6. Final local sorting phase
    // Start Caliper measurement for final local sort
    sort(received_data)
    // End Caliper measurement for final local sort

    // End Caliper measurement for entire Sample Sort
    return received_data

function main():
    // Initialize MPI
    // Get world_rank and world_size
    // Read or generate input data
    
    sorted_data = sampleSort(local_data, world_rank, world_size)
    
    // Gather results to rank 0 or write to file
    // Finalize MPI
```

#### Bitonic Sort
```
// Function to compare and exchange two elements
function compareExchange(data, i, j, ascending):
    // Compare elements at indices i and j
    // Swap if they are in the wrong order based on 'ascending' flag

// Function to merge a bitonic sequence
function bitonicMerge(data, low, count, ascending):
    // If count > 1:
    //     Find the greatest power of 2 less than count
    //     Compare-exchange pairs of elements
    //     Recursively merge the two halves

// Function to generate a bitonic sequence
function bitonicSort(data, low, count, ascending):
    // If count > 1:
    //     Recursively sort first half in ascending order
    //     Recursively sort second half in descending order
    //     Merge the resulting bitonic sequence

// Main MPI Bitonic Sort function
function mpiBitonicSort(local_data, world_rank, world_size):
    total_size = local_data.size() * world_size
    local_size = local_data.size()

    // Bitonic sort stages
    for k = 2 to total_size:
        for j = k/2 to 1 (dividing by 2 each iteration):
            // Start Caliper measurement for local compare-exchange
            for each element i in local_data:
                // Calculate the index of the element to compare with
                // If the partner element is in the same process:
                //     Perform local compare-exchange
                // Else:
                //     Start Caliper measurement for MPI exchange
                //     Use MPI_Sendrecv to exchange and compare elements with partner process
                //     End Caliper measurement for MPI exchange
            // End Caliper measurement for local compare-exchange

// Main function
function main():
    // Initialize MPI
    // Get world_rank and world_size
    // Read or generate input data
    
    // Start Caliper measurement for entire MPI Bitonic Sort
    mpiBitonicSort(local_data, world_rank, world_size)
    // End Caliper measurement for entire MPI Bitonic Sort
    
    // Gather results to rank 0 or write to file
    // Finalize MPI
```




#### Merge Sort
```
// Function to merge two sorted arrays
function merge(left_array, right_array):
    // Merge the two arrays and return the result

// Function to perform local merge sort
function localMergeSort(array):
    // If array size > 1:
    //     Divide array into two halves
    //     Recursively sort both halves
    //     Merge the sorted halves

// Main MPI Merge Sort function
function mpiMergeSort(local_data, world_rank, world_size):
    // Start Caliper measurement for local sort
    
    // Perform local merge sort on this process's data
    localMergeSort(local_data)
    
    // End Caliper measurement for local sort

    // Parallel merge phase
    for step = 1 to log2(world_size):
        if this process should receive data:
            // Start Caliper measurement for MPI receive and merge
            
            // Receive data from partner process
            // Merge received data with local data
            
            // End Caliper measurement for MPI receive and merge
        else if this process should send data:
            // Start Caliper measurement for MPI send
            
            // Send local data to partner process
            
            // End Caliper measurement for MPI send
            
            // Break out of the loop as this process is done

    return local_data

// Main function
function main():
    // Initialize MPI
    
    // Get world_rank and world_size
    
    // Read or generate input data
    
    // Start Caliper measurement for entire MPI Merge Sort
    
    sorted_data = mpiMergeSort(local_data, world_rank, world_size)
    
    // End Caliper measurement for entire MPI Merge Sort
    
    // Gather results to rank 0 or write to file
    
    // Finalize MPI
```



#### Radix Sort
```
// Function to perform counting sort for a specific digit
function countingSort(array, exp):
    // Start Caliper measurement for counting
    // Count occurrences of each digit in the given place value
    // End Caliper measurement for counting

    // Calculate cumulative count
    // Build the output array
    
    // Start Caliper measurement for copying output
    // Copy the output array to the original array
    // End Caliper measurement for copying output

// Main MPI Radix Sort function
function mpiRadixSort(local_data, world_rank, world_size):
    // Find local maximum element
    
    // Start Caliper measurement for MPI reduce
    // Use MPI_Allreduce to find global maximum across all processes
    // End Caliper measurement for MPI reduce

    // Perform Radix Sort
    for exp = 1 to max_element:
        // Start Caliper measurement for local counting sort
        countingSort(local_data, exp)
        // End Caliper measurement for local counting sort

        // Start Caliper measurement for MPI all-to-all
        // Redistribute data across processes based on current digit
        // Use MPI_Alltoallv for flexible data redistribution
        // End Caliper measurement for MPI all-to-all

        // Start Caliper measurement for merging sorted chunks
        // Merge the received sorted chunks
        // End Caliper measurement for merging sorted chunks

// Main function
function main():
    // Initialize MPI
    // Get world_rank and world_size
    // Read or generate input data
    
    // Start Caliper measurement for entire MPI Radix Sort
    mpiRadixSort(local_data, world_rank, world_size)
    // End Caliper measurement for entire MPI Radix Sort
    
    // Gather results to rank 0 or write to file
    // Finalize MPI
```
### 2c. Evaluation plan - what and how will you measure and compare
- Input sizes, Input types
- 2^16, 2^18, 2^20, 2^22, 2^24, 2^26, 2^28; Sorted, Random, Reverse sorted, 1% perturbed
  
- Strong scaling (same problem size, increase number of processors/nodes)
- 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024 processors
  
- Weak scaling (increase problem size, increase number of processors)
- 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024 processors 
