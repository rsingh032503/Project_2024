#include <mpi.h>
#include <caliper/cali.h>
#include <caliper/cali-manager.h>

#define CHECK_SORTED_TAG 2
#define RETURN_SORTED_TAG 3

bool local_sorted(const int* arr, int size){
    for(int i = 1; i < size; i++){
        if(arr[i-1] > arr[i]){
            //printf("val at index %i: %i",i-1,arr[i-1]);
            //printf("val at index %i: %i",i,arr[i]);
            return false;
        }
    }
    return true;
};

bool globally_sorted(const int* arr, int size, int rank, int num_procs, bool debug = false){
    if (debug){
        printf("Processor %i entered the global sort function\n",rank);
    }
    //check if locally sorted
    bool locally_sorted = local_sorted(arr,size);

    //check if all processors are locally sorted
    bool all_locally_sorted = false;

    MPI_Allreduce(&locally_sorted, &all_locally_sorted, 1, MPI_CXX_BOOL, MPI_LAND, MPI_COMM_WORLD);

    if(debug){
        printf("Processor %i is%slocally sorted\n",rank,(locally_sorted)?" ":" NOT ");
        if(rank == 0){
            printf("All processors are%slocally sorted\n",(all_locally_sorted)?" ":" NOT ");
        }
    }

    if(!all_locally_sorted){
        return false;
    }

    // check if your values are sorted compared to processor + 1
    
    // send last value to processor with rank + 1
    if(rank != (num_procs - 1)){
        MPI_Send(&arr[size-1],1,MPI_INT,rank+1, CHECK_SORTED_TAG, MPI_COMM_WORLD);
    }
    
    // recieve value and compare to first value in local array
    if(rank != 0){
        int prev_proc_last_val;
        MPI_Status s;
        MPI_Recv(&prev_proc_last_val, 1, MPI_INT, rank-1, CHECK_SORTED_TAG, MPI_COMM_WORLD, &s);

        bool sorted = prev_proc_last_val <= arr[0];
        if(debug){
            printf("Processor %i is%ssorted in relation to processor %i\n",rank,(sorted)?" ":" NOT " ,rank-1);
        }

        // send sorted check from last value to firt value in local array
        MPI_Send(&sorted,1,MPI_CXX_BOOL,rank-1, RETURN_SORTED_TAG, MPI_COMM_WORLD);
    }

    bool globally_sorted = true;

    // recieve sorted status from processor with rank + 1
    if(rank != (num_procs - 1)){
        MPI_Status s;
        MPI_Recv(&globally_sorted, 1, MPI_CXX_BOOL, rank+1, RETURN_SORTED_TAG, MPI_COMM_WORLD, &s);
    }
    if(debug){
        printf("Processor %i is%sglobally sorted\n",rank,(globally_sorted)?" ":" NOT ");
    }

    int g_sort = globally_sorted * 1;
    int a_g_sort = 0;

    // check to see if all processors are sorted compared to their next processor rank
    MPI_Allreduce(&g_sort, &a_g_sort, 1, MPI_INT, MPI_LAND, MPI_COMM_WORLD);

    if(debug){
        printf("Processor %i a_g_sort: %i\n",rank,a_g_sort);
    }

    return a_g_sort;
};