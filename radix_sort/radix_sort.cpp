#include <mpi.h>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <random>
#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>

#include "../generate_data.h"
#include "../check_sorted.h"

// Function to perform counting sort for a specific digit
void countingSort(int*& arr, int size, int exp){
    CALI_MARK_BEGIN("Counting Sort");
    // Start Caliper measurement for counting
    CALI_MARK_BEGIN("Compute Frequency");
    int freq_size = 1 << 8;
    int freq[freq_size];
    // Count occurrences of each digit in the given place value
    for(int i = 0; i < size; i++){
        freq[(arr[i] >> (8*exp)) % freq_size]++;
    }
    CALI_MARK_END("Compute Frequency");
    // End Caliper measurement for counting

    CALI_MARK_BEGIN("Compute Cumulative Index");
    // Calculate cumulative count
    for(int i = 1; i < freq_size; i++){
        freq[i] += freq[i-1];
    }
    CALI_MARK_END("Compute Cumulative Index");
    
    
    // Build the output array
    CALI_MARK_BEGIN("Create Output Array");
    int* res = new int[size];
    for(int i = size - 1; i >=0; i--){
        res[--freq[(arr[i] >> (8*exp)) % freq_size]] = arr[i];
    }
    CALI_MARK_END("Create Output Array");

    CALI_MARK_BEGIN("Copy Output to Original Array");
    delete arr;
    arr = res;
    CALI_MARK_END("Copy Output to Original Array");
    // End Caliper measurement for copying output
    CALI_MARK_END("Counting Sort");
};

void LocalRadixSort(int*& arr, int size){
    CALI_MARK_BEGIN("Local Radix Sort");
    for(int exponent = 0; exponent < 4; exponent++){
        countingSort(arr, size, exponent);
    }
    CALI_MARK_END("Local Radix Sort");
};

void MPI_RadixSort(int*& arr, int size, int global_size, int rank, int num_procs){

    CALI_MARK_BEGIN("Bit Computations");
    //find what bit the maximum number is at
    int most_sig_bit = 1;
    while(global_size >> most_sig_bit > 1){
        most_sig_bit++;
    }

    //find number of bits needed to split data among processors
    int num_sig_bits = 1;
    while(num_procs >> num_sig_bits > 1){
        num_sig_bits++;
    }

    int shift = most_sig_bit - num_sig_bits;
    CALI_MARK_END("Bit Computations");


    //sort the data into respective buckets based on bits
    CALI_MARK_BEGIN("Bitwise split");
    std::vector<std::vector<int>> splits(num_procs);
    for(int i = 0; i < size; i++){
        splits[arr[i] >> shift].push_back(arr[i]);
    }

    delete[] arr;
    CALI_MARK_END("Bitwise split");

    
    CALI_MARK_BEGIN("Communication");
    //determine how much data is being sent to each processor
    CALI_MARK_BEGIN("COMM - Send");
    int send_size[num_procs]{0};
    for(int i = 0; i < num_procs; i++){
        send_size[i] = splits[i].size();
    }

    // send the data to their respective processors
    for(int i = 0; i < num_procs; i++){
        if(send_size[i] == 0){
            continue;
        }
        MPI_Send(splits[i].data(),send_size[i],MPI_INT,i,0,MPI_COMM_WORLD);
    }

    CALI_MARK_END("COMM - Send");

    // determine how much data is being recieved from each processor
    CALI_MARK_BEGIN("COMM - Rec");
    int rec_count = 0;
    int rec_size[num_procs]{0};

    CALI_MARK_BEGIN("Scatter - recieve sizes");
    MPI_Scatter(&send_size,1,MPI_INT,&rec_size,num_procs,MPI_INT,rank,MPI_COMM_WORLD);
    CALI_MARK_END("Scatter - recieve sizes");
    for(int i = 0; i < num_procs; i++){
        rec_count += rec_size[i];
    }

    int* rec = new int[rec_count];
    int rec_ind[num_procs]{0};

    for(int i = 1; i < num_procs; i++){
        rec_ind[i] = rec_ind[i-1] + rec_size[i-1];
    }

    // receive the data from the other processors
    for(int i = 0; i < num_procs; i++){
        if(rec_size[i] == 0){
            continue;
        }
        MPI_Status s;
        MPI_Recv(rec + rec_ind[i],rec_size[i],MPI_INT,i,0,MPI_COMM_WORLD,&s);
    }
    CALI_MARK_END("COMM - Rec");

    LocalRadixSort(rec,rec_count);
};


int main(int argc, char* argv[]) {
    bool debug = true;
    cali::ConfigManager mgr;
    mgr.start();

    CALI_MARK_BEGIN("main");

    int rank, num_procs;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
    

    if (argc != 3) {
        if (rank == 0) {
            printf("Usage: <num_processes> <exponent> <input_type>\nRecieved ");
            for(int i = 0; i < argc; i++){
                printf("%s, ",argv[i]);
            }
            printf("\n");
        }
        MPI_Finalize();
        return 1;
    }

    int exponent = atoi(argv[1]);
    const char* input_type = argv[2];
    int total_size = 1 << exponent;
    int local_size = total_size / num_procs;

    if(rank == 0){
        printf("Starting Radix Sort with %i Processors with input size 2^%i and input type %s\n",
                num_procs, exponent, input_type);
        printf("Total input size: %i\nInput/Processor: %i\n", total_size, local_size);
    }

    if(debug){
        printf("Starting Data Generation on Proc %i\n",rank);
    }
    CALI_MARK_BEGIN("data_init_runtime");
    int* local_arr = new int[local_size];
    generate_data(local_arr, local_size, input_type, rank, num_procs);
    CALI_MARK_END("data_init_runtime");

    if(debug){
        printf("Starting Radix Sort on Proc %i\n", rank);
    }

    CALI_MARK_BEGIN("MPI Radix Sort");
    MPI_RadixSort(local_arr, local_size, total_size, rank, num_procs);
    CALI_MARK_END("MPI Radix Sort");

    if(debug){
        printf("Starting Global Sort Check on Proc %i\n", rank);
    }

    CALI_MARK_BEGIN("Check Global Sort");
    globally_sorted(local_arr, local_size, rank, num_procs);
    CALI_MARK_BEGIN("Check Global Sort");

    if(debug){
        printf("Finished on Proc %i\n", rank);
    }

    CALI_MARK_END("main");
    MPI_Finalize();

    adiak::init(NULL);
    adiak::launchdate();    // launch date of the job
    adiak::libraries();     // Libraries used
    adiak::cmdline();       // Command line used to launch the job
    adiak::clustername();   // Name of the cluster
    adiak::value("algorithm", "Radix"); // The name of the algorithm you are using (e.g., "merge", "bitonic")
    adiak::value("programming_model", "MPI"); // e.g. "mpi"
    adiak::value("data_type", "I"); // The datatype of input elements (e.g., double, int, float)
    adiak::value("size_of_data_type", sizeof(int)); // sizeof(datatype) of input elements in bytes (e.g., 1, 2, 4)
    adiak::value("input_size", total_size); // The number of elements in input dataset (1000)
    adiak::value("input_type", input_type); // For sorting, this would be choices: ("Sorted", "ReverseSorted", "Random", "1_perc_perturbed")
    adiak::value("num_procs", num_procs); // The number of processors (MPI ranks)
    adiak::value("scalability", "strong"); // The scalability of your algorithm. choices: ("strong", "weak")
    adiak::value("group_num", 9); // The number of your group (integer, e.g., 1, 10)
    adiak::value("implementation_source", "online"); // Where you got the source code of your algorithm. choices: ("online", "ai", "handwritten").

    
}