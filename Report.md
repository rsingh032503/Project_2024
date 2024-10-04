# CSCE 435 Group project

## 0. Group number: 
9
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
- Quick Sort:
- Merge Sort:
- Radix Sort:

### 2b. Pseudocode for each parallel algorithm
- For MPI programs, include MPI calls you will use to coordinate between processes

#### Quick Sort:
```
// local quicksort function
function quicksort(array, low, high):
    if low < high:
        // Start Caliper measurement for partition
        pivot_index = partition(array, low, high)
        // End Caliper measurement for partition

        // Start Caliper measurement for recursive quicksort
        quicksort(array, low, pivot_index - 1)
        quicksort(array, pivot_index + 1, high)
        // End Caliper measurement for recursive quicksort
        
// partition function
function partition(array, low, high):
    // pick a pivot
    // Partition around the pivot
    // Return  pivot index


// Main MPI Quicksort function
function parallelQuicksort(local_data, world_rank, world_size):
    // Start Caliper measurement for local quicksort
    quicksort(local_data, 0, local_data.size() - 1)
    // End Caliper measurement for local quicksort

    // Parallel merge phase (similar to Merge Sort)
    for step = 1 to log2(world_size):
        if this process should receive data:
            // Start Caliper measurement for MPI receive and merge
            // MPI_Recv - Receive data from partner process
            // Merge received data with local data
            // End Caliper measurement for MPI receive and merge
        else if this process should send data:
            // Start Caliper measurement for MPI send
            // MPI_Send - Send local data to partner process
            // End Caliper measurement for MPI send
            // Break out of the loop as this process is done

// Main function
function main():
    // MPI_Init - Initialize MPI
    // Get world_rank and world_size
    // Read or generate input data
    
    // Start Caliper measurement for entire Parallel Quicksort
    parallelQuicksort(local_data, world_rank, world_size)
    // End Caliper measurement for entire Parallel Quicksort
    
    // Gather results to rank 0 or write to file
    // MPI_Finalize - Finalize MPI
```

#### Bitonic Sort




#### Merge Sort



#### Radix Sort

### 2c. Evaluation plan - what and how will you measure and compare
- Input sizes, Input types
- Strong scaling (same problem size, increase number of processors/nodes)
- Weak scaling (increase problem size, increase number of processors)
