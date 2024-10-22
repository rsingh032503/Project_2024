#include <mpi.h>
#include <string>
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

using std::string;
using std::__cxx11::to_string;

bool debug = true;

// Function to perform counting sort for a specific digit
void countingSort(int*& arr, int size, int exp){
    
    // Start Caliper measurement for counting
    
    int freq_size = 1 << 8;
    int freq[freq_size]{0};
    // Count occurrences of each digit in the given place value
    for(int i = 0; i < size; i++){
        freq[(arr[i] >> (8*exp)) % freq_size]++;
    }
    
    // End Caliper measurement for counting

    
    // Calculate cumulative count
    for(int i = 1; i < freq_size; i++){
        freq[i] += freq[i-1];
    }
    
    // Build the output array
    
    int* res = new int[size];
    for(int i = size - 1; i >=0; i--){
        res[--freq[(arr[i] >> (8*exp)) % freq_size]] = arr[i];
    }
    delete arr;
    arr = res;
};

void LocalRadixSort(int*& arr, int size){
    for(int exponent = 0; exponent < 4; exponent++){
        countingSort(arr, size, exponent);
    }
};

void MPI_RadixSort(int*& arr, int& size, int global_size, int rank, int num_procs){

    if(debug){
        printf("Proc %i starting bit computations\n",rank);
    }
    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("comp_small");
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
    if(debug){
        printf("Proc %i: most significant bit: %i\n",rank ,most_sig_bit);
        printf("Proc %i: num significant bit: %i\n",rank ,num_sig_bits);
        printf("Proc %i: shift: %i\n", rank, shift);
    }


    if(debug){
        printf("Proc %i starting bitwise split\n",rank);
        printf("Processor %i data array starts at %p\n",rank ,arr);
        printf("\tand ends at %p\n",arr+size);
        fflush(stdout);
        MPI_Barrier(MPI_COMM_WORLD);

    }
    //sort the data into respective buckets based on bits
    std::vector<std::vector<int>> splits(num_procs);
    for(int i = 0; i < size; i++){
        splits.at(arr[i] >> shift).push_back(arr[i]);

    }

    delete[] arr;
    CALI_MARK_END("comp_small");
    CALI_MARK_END("comp");
    

    
    //determine how much data is being sent to each processor
    
    int send_size[num_procs]{0};
    string print = "Processor " + to_string(rank) + " send array : [";
    for(int i = 0; i < num_procs; i++){
        send_size[i] = splits[i].size(); 
        print = print + to_string(send_size[i]) + (i != num_procs-1?", ":"]\n");
    }
    if(debug){
        printf("%s",print.c_str());
    }

    CALI_MARK_BEGIN("comm");
    
    // determine how much data is being recieved from each processor
    int rec_count = 0;
    int rec_size[num_procs]{0};

    if(debug){
        printf("Proc %i starting scatter\n",rank);
    }
    CALI_MARK_BEGIN("comm_small");
    for(int i = 0; i < num_procs; i++){
        MPI_Scatter(&send_size,1,MPI_INT,&(rec_size[i]),1,MPI_INT,i,MPI_COMM_WORLD);
    }
    CALI_MARK_END("comm_small");

    string print2 = "Post scatter - Processor " + to_string(rank) + " send array : [";
    for(int i = 0; i < num_procs; i++){
        print2 = print2 + to_string(send_size[i]) + (i != num_procs-1?", ":"]\n");
    }
    if(debug){
        printf("%s",print2.c_str());
    }


    for(int i = 0; i < num_procs; i++){
        rec_count += rec_size[i];
    }
    
    int* rec = new int[rec_count];
    int rec_ind[num_procs]{0};

    for(int i = 1; i < num_procs; i++){
        rec_ind[i] = rec_ind[i-1] + rec_size[i-1];
    }

    // send the data to their respective processors
    CALI_MARK_BEGIN("comm_large");
    if(debug){
        printf("Proc %i starting send\n",rank);
    }
    for(int i = 0; i < num_procs; i++){
        //printf("Processor %i is sending %i pieces of data to processor %i\n", rank, send_size[i], i);
        if(send_size[i] == 0){
            continue;
        }
        MPI_Send(splits[i].data(),send_size[i],MPI_INT,i,0,MPI_COMM_WORLD);
    }

    // receive the data from the other processors
    if(debug){
        printf("Proc %i starting receive\n",rank);
    }

    for(int i = 0; i < num_procs; i++){
        if(rec_size[i] == 0){
            continue;
        }
        if(debug){
            printf("Processor %i recieved %i pieces of data from processor %i\n",rank,rec_size[i],i);
        }
        MPI_Status s;
        MPI_Recv(rec + rec_ind[i],rec_size[i],MPI_INT,i,0,MPI_COMM_WORLD,&s);
    }
    CALI_MARK_END("comm_large");
    CALI_MARK_END("comm");

    if(debug){
        printf("Proc %i finished communication\n",rank);
    }
    
    size = rec_count;
    arr = rec;

    if(debug){
        printf("Checking arr = rec: %u == %u\n",arr,rec);
        printf("Proc %i starting local radix sort\n",rank);
    }

    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("comp_large");
    LocalRadixSort(arr,rec_count);
    CALI_MARK_END("comp_large");
    CALI_MARK_END("comp");

    if(debug){
        bool loc_sorted = local_sorted(arr,rec_count);
        printf("Proc %i finished local radix sort\n",rank);
        printf("Proc %i is%slocally sorted\n",rank,loc_sorted?" ":" NOT ");
    }
};


int main(int argc, char* argv[]) {
    cali::ConfigManager mgr;
    mgr.start();


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

    CALI_MARK_BEGIN("main");
    CALI_MARK_BEGIN("data_init_runtime");
    int* local_arr = new int[local_size]();
    generate_data(local_arr, local_size, input_type, rank, num_procs);
    CALI_MARK_END("data_init_runtime");

    if(debug){
        printf("Proc %i stopping at pre-sort barrier\n",rank);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    if(debug){
        printf("Starting Radix Sort on Proc %i\n", rank);
    }

    MPI_RadixSort(local_arr, local_size, total_size, rank, num_procs);
    if(debug){
        printf("Proc %i stopping at post-sort barrier\n",rank);
    }
    MPI_Barrier(MPI_COMM_WORLD);

    if(debug){
        printf("Starting Global Sort Check on Proc %i\n", rank);
    }

    CALI_MARK_BEGIN("correctness_check");
    bool sort = globally_sorted(local_arr, local_size, rank, num_procs, true);
    CALI_MARK_END("correctness_check");
    CALI_MARK_END("main");

    if(debug){
        printf("Finished on Proc %i With sorted status %s\n", rank, sort? "true":"false");
    }
    delete local_arr;

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