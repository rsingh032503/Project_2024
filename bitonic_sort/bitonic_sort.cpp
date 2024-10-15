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

void bitonic_sort(int* arr, int size, int num_procs, int rank) {
    CALI_CXX_MARK_FUNCTION;

    for (int step = 2; step <= num_procs; step *= 2) {
        for (int substep = step / 2; substep > 0; substep /= 2) {
            int partner = rank ^ substep;
            int* recv_buf = new int[size];

            CALI_MARK_BEGIN("comm");
            CALI_MARK_BEGIN("comm_large");
            MPI_Sendrecv(arr, size, MPI_INT, partner, 0,
                         recv_buf, size, MPI_INT, partner, 0,
                         MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            CALI_MARK_END("comm_large");
            CALI_MARK_END("comm");

            CALI_MARK_BEGIN("comp");
            CALI_MARK_BEGIN("comp_large");
            bool ascending = (rank & step) == 0;
            if (ascending) {
                for (int i = 0; i < size; i++) {
                    if (arr[i] > recv_buf[i]) {
                        std::swap(arr[i], recv_buf[i]);
                    }
                }
            } else {
                for (int i = 0; i < size; i++) {
                    if (arr[i] < recv_buf[i]) {
                        std::swap(arr[i], recv_buf[i]);
                    }
                }
            }
            CALI_MARK_END("comp_large");
            CALI_MARK_END("comp");

            delete[] recv_buf;
        }
    }
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

    // Bitonic Sort
    CALI_MARK_BEGIN("bitonic_sort");
    bitonic_sort(local_arr, local_size, num_procs, rank);
    CALI_MARK_END("bitonic_sort");

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