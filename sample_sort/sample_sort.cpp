#include <mpi.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <random>
#include <caliper/cali.h>
#include <caliper/cali-manager.h>

void generate_data(int* arr, int size, const char* type) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, size - 1);

    if (strcmp(type, "sorted") == 0) {
        for (int i = 0; i < size; i++) {
            arr[i] = i;
        }
    } else if (strcmp(type, "reverse_sorted") == 0) {
        for (int i = 0; i < size; i++) {
            arr[i] = size - i - 1;
        }
    } else if (strcmp(type, "random") == 0) {
        for (int i = 0; i < size; i++) {
            arr[i] = dis(gen);
        }
    } else if (strcmp(type, "perturbed") == 0) {
        for (int i = 0; i < size; i++) {
            arr[i] = i;
        }
        int perturb_count = size / 100;
        for (int i = 0; i < perturb_count; i++) {
            int idx = dis(gen);
            arr[idx] = dis(gen);
        }
    }
}

void sample_sort(int* arr, int size, int num_procs, int rank) {
    CALI_CXX_MARK_FUNCTION;

    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_small");
    // Gather samples
    int sample_size = num_procs - 1;
    int* samples = new int[sample_size * num_procs];
    for (int i = 0; i < sample_size; i++) {
        samples[i] = arr[i * (size / sample_size)];
    }
    MPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, samples, sample_size, MPI_INT, MPI_COMM_WORLD);
    CALI_MARK_END("comm_small");
    CALI_MARK_END("comm");

    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("comp_small");
    // Sort samples and select pivots
    std::sort(samples, samples + sample_size * num_procs);
    int* pivots = new int[num_procs - 1];
    for (int i = 0; i < num_procs - 1; i++) {
        pivots[i] = samples[(i + 1) * sample_size];
    }
    CALI_MARK_END("comp_small");
    CALI_MARK_END("comp");

    // Count elements for each bucket
    int* send_counts = new int[num_procs]();
    for (int i = 0; i < size; i++) {
        int bucket = std::upper_bound(pivots, pivots + num_procs - 1, arr[i]) - pivots;
        send_counts[bucket]++;
    }

    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_small");
    // Alltoall to get receive counts
    int* recv_counts = new int[num_procs];
    MPI_Alltoall(send_counts, 1, MPI_INT, recv_counts, 1, MPI_INT, MPI_COMM_WORLD);
    CALI_MARK_END("comm_small");
    CALI_MARK_END("comm");

    // Calculate displacements
    int* send_displs = new int[num_procs];
    int* recv_displs = new int[num_procs];
    send_displs[0] = recv_displs[0] = 0;
    for (int i = 1; i < num_procs; i++) {
        send_displs[i] = send_displs[i-1] + send_counts[i-1];
        recv_displs[i] = recv_displs[i-1] + recv_counts[i-1];
    }

    // Prepare send buffer
    int* send_buf = new int[size];
    int* send_indices = new int[num_procs]();
    for (int i = 0; i < size; i++) {
        int bucket = std::upper_bound(pivots, pivots + num_procs - 1, arr[i]) - pivots;
        send_buf[send_displs[bucket] + send_indices[bucket]++] = arr[i];
    }

    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_large");
    // Alltoallv to redistribute data
    int recv_total = recv_displs[num_procs-1] + recv_counts[num_procs-1];
    int* recv_buf = new int[recv_total];
    MPI_Alltoallv(send_buf, send_counts, send_displs, MPI_INT,
                  recv_buf, recv_counts, recv_displs, MPI_INT, MPI_COMM_WORLD);
    CALI_MARK_END("comm_large");
    CALI_MARK_END("comm");

    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("comp_large");
    // Sort received data
    std::sort(recv_buf, recv_buf + recv_total);
    CALI_MARK_END("comp_large");
    CALI_MARK_END("comp");

    // Copy sorted data back to original array
    std::copy(recv_buf, recv_buf + size, arr);

    // Clean up
    delete[] samples;
    delete[] pivots;
    delete[] send_counts;
    delete[] recv_counts;
    delete[] send_displs;
    delete[] recv_displs;
    delete[] send_buf;
    delete[] send_indices;
    delete[] recv_buf;
}

bool check_sorted(int* arr, int size) {
    for (int i = 1; i < size; i++) {
        if (arr[i] < arr[i-1]) {
            return false;
        }
    }
    return true;
}

int main(int argc, char** argv) {
    CALI_MARK_BEGIN("main");

    int rank, num_procs;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    if (argc != 3) {
        if (rank == 0) {
            printf("Usage: %s <exponent> <input_type>\n", argv[0]);
        }
        MPI_Finalize();
        return 1;
    }

    int exponent = atoi(argv[1]);
    const char* input_type = argv[2];
    int total_size = 1 << exponent;
    int local_size = total_size / num_procs;

    int* local_arr = new int[local_size];

    CALI_MARK_BEGIN("data_init_runtime");
    generate_data(local_arr, local_size, input_type);
    CALI_MARK_END("data_init_runtime");

    // Sample Sort
    CALI_MARK_BEGIN("sample_sort");
    sample_sort(local_arr, local_size, num_procs, rank);
    CALI_MARK_END("sample_sort");

    // Check correctness
    CALI_MARK_BEGIN("correctness_check");
    bool local_sorted = check_sorted(local_arr, local_size);
    bool globally_sorted;
    MPI_Allreduce(&local_sorted, &globally_sorted, 1, MPI_C_BOOL, MPI_LAND, MPI_COMM_WORLD);
    if (rank == 0) {
        printf("Sorting %s\n", globally_sorted ? "successful" : "failed");
    }
    CALI_MARK_END("correctness_check");

    delete[] local_arr;
    MPI_Finalize();

    CALI_MARK_END("main");
    return 0;
}