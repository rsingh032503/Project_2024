#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <vector>
#include <cmath>
#include <adiak.hpp>
#include <caliper/cali.h>


void generate_data(int* data, int size,std::string input_type) {
    if(input_type == "Random"){
      for (int i = 0; i < size; i++) {
          data[i] = rand() % size;  // Random numbers between 0 and size-1
      }
    }else if(input_type == "Reverse"){
      for (int i = 0; i < size; i++) {
          data[i] = size - i;
      }
    }else if(input_type == "Sorted"){
      for (int i = 0; i < size; i++) {
          data[i] = i;
      }
    }else if(input_type == "Perturbed"){
      for (int i = 0; i < size; i++) {
          data[i] = i;
      }
      for (int i = 0; i < size / 100; i++) {
          int index1 = rand() % size;
          int index2 = rand() % size;
          std::swap(data[index1], data[index2]);
      }
    }else{
      std::cerr << "Invalid input type" << std::endl;
      MPI_Abort(MPI_COMM_WORLD, 1);
    }
}


bool is_sorted(int* data, int size) {
    for (int i = 1; i < size; i++) {
        if (data[i] < data[i-1]) {
            return false;
        }
    }
    return true;
}

int main(int argc, char** argv) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    int power = atoi(argv[1]);
    int array_size = 1 << power;  // 2^power
    std::string input_type = argv[2];
    adiak::init(NULL);
    adiak::launchdate();    // launch date of the job
    adiak::libraries();     // Libraries used
    adiak::cmdline();       // Command line used to launch the job
    adiak::clustername();   // Name of the cluster
    adiak::value("algorithm", "Sample"); // The name of the algorithm you are using (e.g., "merge", "bitonic")
    adiak::value("programming_model", "MPI"); // e.g. "mpi"
    adiak::value("data_type", "I"); // The datatype of input elements (e.g., double, int, float)
    adiak::value("size_of_data_type", sizeof(int)); // sizeof(datatype) of input elements in bytes (e.g., 1, 2, 4)
    adiak::value("input_size", array_size); // The number of elements in input dataset (1000)
    adiak::value("input_type", input_type); // For sorting, this would be choices: ("Sorted", "ReverseSorted", "Random", "1_perc_perturbed")
    adiak::value("num_procs", argv[0]); // The number of processors (MPI ranks)
    adiak::value("scalability", "strong"); // The scalability of your algorithm. choices: ("strong", "weak")
    adiak::value("group_num", 9); // The number of your group (integer, e.g., 1, 10)
    adiak::value("implementation_source", "online"); // Where you got the source code of your algorithm. choices: ("online", "ai", "handwritten").
    CALI_MARK_BEGIN("main");
    
    if (argc != 3) {
      if (rank == 0) {
          printf("Usage: %s <num_processes> <array_size_power> <input_type>\n", argv[0]);
          MPI_Finalize();
          return 1;
      }
    }
    
    // Create duplicate communicator
    MPI_Comm comm_dup;
    MPI_Comm_dup(MPI_COMM_WORLD, &comm_dup);
    
    // Calculate local array size
    int local_size = array_size / size;
    int* local_data = new int[local_size];
    
    // Generate data on rank 0 and distribute
    int* global_data = nullptr;
    if (rank == 0) {
        CALI_MARK_BEGIN("data_init_runtime");
        global_data = new int[array_size];
        generate_data(global_data, array_size,input_type);
        CALI_MARK_END("data_init_runtime");
    }
    
    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_large");
    MPI_Scatter(global_data, local_size, MPI_INT, 
                local_data, local_size, MPI_INT, 
                0, comm_dup);
    CALI_MARK_END("comm_large");
    CALI_MARK_END("comm");
    
    // Local sort
    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("comp_large");
    std::sort(local_data, local_data + local_size);
    CALI_MARK_END("comp_large");
    CALI_MARK_END("comp");
    
    // Choose regular samples
    int samples_per_proc = size - 1;
    int sample_stride = local_size / samples_per_proc;
    std::vector<int> local_samples(samples_per_proc);
    
    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("comp_small");
    for (int i = 0; i < samples_per_proc; i++) {
        local_samples[i] = local_data[i * sample_stride];
    }
    CALI_MARK_END("comp_small");
    CALI_MARK_END("comp");
    
    // Gather samples to rank 0
    std::vector<int> all_samples;
    if (rank == 0) {
        all_samples.resize(size * samples_per_proc);
    }
    
    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_small");
    MPI_Gather(local_samples.data(), samples_per_proc, MPI_INT,
               all_samples.data(), samples_per_proc, MPI_INT,
               0, comm_dup);
    CALI_MARK_END("comm_small");
    CALI_MARK_END("comm");
    
    // Choose and broadcast splitters
    std::vector<int> splitters(size - 1);
    if (rank == 0) {
        CALI_MARK_BEGIN("comp");
        CALI_MARK_BEGIN("comp_small");
        std::sort(all_samples.begin(), all_samples.end());
        for (int i = 0; i < size - 1; i++) {
            splitters[i] = all_samples[(i + 1) * size * samples_per_proc / size];
        }
        CALI_MARK_END("comp_small");
        CALI_MARK_END("comp");
    }
    
    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_small");
    MPI_Bcast(splitters.data(), size - 1, MPI_INT, 0, comm_dup);
    CALI_MARK_END("comm_small");
    CALI_MARK_END("comm");
    
    // Count elements going to each processor
    std::vector<int> send_counts(size, 0);
    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("comp_large");
    for (int i = 0; i < local_size; i++) {
        int dest = 0;
        while (dest < size - 1 && local_data[i] >= splitters[dest]) {
            dest++;
        }
        send_counts[dest]++;
    }
    CALI_MARK_END("comp_large");
    CALI_MARK_END("comp");
    
    // All-to-all communication
    std::vector<int> recv_counts(size);
    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_small");
    MPI_Alltoall(send_counts.data(), 1, MPI_INT,
                 recv_counts.data(), 1, MPI_INT,
                 comm_dup);
    CALI_MARK_END("comm_small");
    CALI_MARK_END("comm");
    
    // Calculate displacements
    std::vector<int> send_displs(size), recv_displs(size);
    int send_total = 0, recv_total = 0;
    for (int i = 0; i < size; i++) {
        send_displs[i] = send_total;
        recv_displs[i] = recv_total;
        send_total += send_counts[i];
        recv_total += recv_counts[i];
    }
    
    // Prepare send buffer
    std::vector<int> send_buffer(local_size);
    std::vector<int> send_pos = send_displs;
    
    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("comp_large");
    for (int i = 0; i < local_size; i++) {
        int dest = 0;
        while (dest < size - 1 && local_data[i] >= splitters[dest]) {
            dest++;
        }
        send_buffer[send_pos[dest]++] = local_data[i];
    }
    CALI_MARK_END("comp_large");
    CALI_MARK_END("comp");
    
    // All-to-all exchange
    std::vector<int> recv_buffer(recv_total);
    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_large");
    MPI_Alltoallv(send_buffer.data(), send_counts.data(), send_displs.data(), MPI_INT,
                  recv_buffer.data(), recv_counts.data(), recv_displs.data(), MPI_INT,
                  comm_dup);
    CALI_MARK_END("comm_large");
    CALI_MARK_END("comm");
    
    // Final local sort
    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("comp_large");
    std::sort(recv_buffer.begin(), recv_buffer.end());
    CALI_MARK_END("comp_large");
    CALI_MARK_END("comp");
    
    // Gather all sorted data to rank 0 for verification
    int* final_data = nullptr;
    std::vector<int> final_counts(size);
    std::vector<int> final_displs(size);
    
    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_small");
    MPI_Gather(&recv_total, 1, MPI_INT,
               final_counts.data(), 1, MPI_INT,
               0, comm_dup);
    CALI_MARK_END("comm_small");
    CALI_MARK_END("comm");
    
    if (rank == 0) {
        int total = 0;
        for (int i = 0; i < size; i++) {
            final_displs[i] = total;
            total += final_counts[i];
        }
        final_data = new int[array_size];
    }
    
    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_large");
    MPI_Gatherv(recv_buffer.data(), recv_total, MPI_INT,
                final_data, final_counts.data(), final_displs.data(), MPI_INT,
                0, comm_dup);
    CALI_MARK_END("comm_large");
    CALI_MARK_END("comm");
    
    if (rank == 0) {
        CALI_MARK_BEGIN("correctness_check");
        bool sorted = is_sorted(final_data, array_size);
        printf("Array %s sorted correctly\n", sorted ? "is" : "is not");
        CALI_MARK_END("correctness_check");
        delete[] final_data;
        delete[] global_data;
    }
    

    
    delete[] local_data;
    MPI_Comm_free(&comm_dup);
    MPI_Finalize();
    CALI_MARK_END("main");
    

    return 0;

}