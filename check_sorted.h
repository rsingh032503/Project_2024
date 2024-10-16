#include <mpi.h>
#include <caliper/cali.h>
#include <caliper/cali-manager.h>

#ifndef CHECK_SORTED_TAG
#define CHECK_SORTED_TAG = 2

#ifndef RETURN_SORTED_TAG
#define RETURN_SORTED_TAG = 3

bool local_sorted(const int* arr, int size) const{
    for(int i = 1; i < size; i++){
        if(arr[i-1] > arr[i]){
            return false;
        }
    }
    return true;
}

bool globally_sorted(const int* arr, int size, int rank, int num_procs) const{

    //check if locally sorted
    CALI_MARK_BEGIN("Check single local sorted status");
    bool locally_sorted = local_sorted(arr,size);
    CALI_MARK_END("Check single local sorted status");

    //check if all processors are locally sorted
    CALI_MARK_BEGIN("Check global local sorted status");
    bool all_locally_sorted = false;

    MPI_Allreduce(&locally_sorted, &all_locally_sorted, 1, MPI_CXX_BOOL, MPI_LAND, MPI_WORLD);
    
    CALI_MARK_END("Check global local sorted status");

    if(!all_locally_sorted){
        CALI_MARK_END("Globally Sorted");
        return false;
    }

    // check if your values are sorted compared to processor + 1
    CALI_MARK_BEGIN("COMM - check global sorted");
    
    // send last value to processor with rank + 1
    if(rank != (num_procs - 1)){
        CALI_MARK_BEGIN("COMM - send last value");

        MPI_Send(&arr[size-1],1,MPI_INT,rank+1, CHECK_SORTED_TAG, MPI_WORLD);

        CALI_MARK_END("COMM - send last value");
    }
    
    // recieve value and compare to first value in local array
    if(rank != 0){
        CALI_MARK_BEGIN("COMM - recieve last value");

        int prev_proc_last_val;
        MPI_Status s;
        MPI_Recieve(&prev_proc_last_val, 1, MPI_INT, rank-1, CHECK_SORTED_TAG, MPI_WORLD, &s);

        CALI_MARK_END("COMM - recieve last value");

        bool sorted = prev_proc_last_val <= arr[0];

        // send sorted check from last value to firt value in local array
        CALI_MARK_BEGIN("COMM - Send sorted status");
        MPI_Send(&sorted,1,MPI_CXX_BOOL,rank-1, RETURN_SORTED_TAG, MPI_WORLD);
        CALI_MARK_END("COMM - Send sorted status");
    }

    bool globally_sorted = true;

    // recieve sorted status from processor with rank + 1
    if(rank != (num_procs - 1)){
        CALI_MARK_BEGIN("COMM - Recieve sorted status");

        MPI_Status s;
        MPI_Recieve(&globally_sorted, 1, MPI_CXX_BOOL, rank+1, RETURN_SORTED_TAG, MPI_WORLD, &s);

        CALI_MARK_END("COMM - Recieve sorted status");
    }

    bool all_globally_sorted = false;

    // check to see if all processors are sorted compared to their next processor rank
    MPI_Allreduce(&globally_sorted, &all_locally_sorted, 1, MPI_CXX_BOOL, MPI_LAND, MPI_WORLD);

    CALI_MARK_END("COMM - check global sorted");

    return all_globally_sorted;
}