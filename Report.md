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




#### Merge Sort



#### Radix Sort

### 2c. Evaluation plan - what and how will you measure and compare
- Input sizes, Input types
- Strong scaling (same problem size, increase number of processors/nodes)
- Weak scaling (increase problem size, increase number of processors)
