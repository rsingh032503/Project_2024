#include <mpi.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <random>
#include <caliper/cali.h>
#include <caliper/cali-manager.h>


void generate_data(int* arr, int size, const char* type, int mpi_rank, int mpi_size) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, mpi_size*size);

    if (strcmp(type, "sorted") == 0) {
        for (int i = 0; i < size; i++) {
            arr[i] = i + mpi_rank*size;
        }
    } else if (strcmp(type, "reverse_sorted") == 0) {
        for (int i = 0; i < size; i++) {
            arr[i] = (mpi_size-mpi_rank)*size - i - 1;
        }
    } else if (strcmp(type, "random") == 0) {
        for (int i = 0; i < size; i++) {
            arr[i] = dis(gen);
        }
    } else if (strcmp(type, "perturbed") == 0) {
        for (int i = 0; i < size; i++) {
            arr[i] = i + mpi_rank*size;
        }
        int perturb_count = size / 100;
        for (int i = 0; i < perturb_count; i++) {
            int idx = dis(gen) % size;
            arr[idx] = dis(gen);
        }
    }
};

void bitonic_merge(int* arr, int size, int dir) {
    for (int step = size / 2; step > 0; step /= 2) {
        for (int i = 0; i < size; i++) {
            int partner = i ^ step;
            if (partner > i && ((arr[i] > arr[partner]) == dir)) {
                std::swap(arr[i], arr[partner]);
            }
        }
    }
}

void bitonic_sort_local(int* arr, int size, int dir) {
    if (size <= 1) return;

    // Sort the first half in ascending order and the second half in descending order
    int mid = size / 2;
    bitonic_sort_local(arr, mid, 1);    // Ascending
    bitonic_sort_local(arr + mid, size - mid, 0);  // Descending

    // Merge the whole array in the given direction
    bitonic_merge(arr, size, dir);
}

void bitonic_merge_global(int* local_arr, int size, int partner, int dir, MPI_Comm comm) {
    int* recv_arr = new int[size];

    // Exchange data with the partner process
    MPI_Sendrecv(local_arr, size, MPI_INT, partner, 0,
                 recv_arr, size, MPI_INT, partner, 0,
                 comm, MPI_STATUS_IGNORE);

    // Create a temporary array to hold the merged data
    int* temp = new int[size * 2];

    // Merge the local and received arrays into temp
    std::merge(local_arr, local_arr + size, recv_arr, recv_arr + size, temp);

    // If ascending (dir == 1), keep the first half (smaller elements),
    // if descending (dir == 0), keep the second half (larger elements)
    if (dir == 1) {
        // Ascending: Keep the first half of the merged array
        std::copy(temp, temp + size, local_arr);
    } else {
        // Descending: Keep the second half of the merged array
        std::copy(temp + size, temp + 2 * size, local_arr);
    }

    // Clean up
    delete[] recv_arr;
    delete[] temp;
}

void bitonic_sort(int* arr, int size, int num_procs, int rank) {
    CALI_CXX_MARK_FUNCTION;

    // Step 1: Perform local bitonic sort on each process
    bitonic_sort_local(arr, size, 1);  // Local sort in ascending order

    // Step 2: Global sorting using MPI
    for (int phase = 1; phase < num_procs; phase <<= 1) {
        for (int step = phase; step > 0; step >>= 1) {
            int partner = rank ^ step;
            int dir = ((rank & phase) == 0) ? 1 : 0;  // Ascending if rank is lower, descending if higher

            // Perform the bitonic merge with the partner process
            bitonic_merge_global(arr, size, partner, dir, MPI_COMM_WORLD);
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
    generate_data(local_arr, local_size, input_type, rank, num_procs);
    CALI_MARK_END("data_init_runtime");
    
    // initial array before sorting
    int* initial_full_arr = nullptr;
    if (rank == 0) {
        initial_full_arr = new int[total_size];
    }
    MPI_Gather(local_arr, local_size, MPI_INT, initial_full_arr, local_size, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("Initial array before sorting:\n");
        for (int i = 0; i < total_size; i++) {
            printf("%d ", initial_full_arr[i]);
            if ((i + 1) % 20 == 0 || i == total_size - 1) printf("\n");
        }
        delete[] initial_full_arr;
    }

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

    // Gather all sorted data to root process
    CALI_MARK_BEGIN("gather_data");
    int* gathered_arr = nullptr;
    if (rank == 0) {
        gathered_arr = new int[total_size];
    }
    MPI_Gather(local_arr, local_size, MPI_INT, gathered_arr, local_size, MPI_INT, 0, MPI_COMM_WORLD);
    CALI_MARK_END("gather_data");

    // Verify and print the gathered array (only on root process)
    if (rank == 0) {
        CALI_MARK_BEGIN("verify_and_print_gathered_data");
        bool full_array_sorted = check_sorted(gathered_arr, total_size);
        printf("Full gathered array is %s\n", full_array_sorted ? "correctly sorted" : "not correctly sorted");

        // Print the entire array
        printf("Full sorted array:\n");
        const int max_print_size = 1000000; // Adjust this value to set a reasonable limit
        if (total_size > max_print_size) {
            printf("Warning: Array size (%d) is very large. Printing only the first %d elements.\n", total_size, max_print_size);
            printf("To print the entire array, adjust the max_print_size variable in the code.\n");
        }
        for (int i = 0; i < std::min(total_size, max_print_size); i++) {
            printf("%d ", gathered_arr[i]);
            if ((i + 1) % 20 == 0) printf("\n"); // Line break every 20 elements for readability
        }
        printf("\n");

        CALI_MARK_END("verify_and_print_gathered_data");

        delete[] gathered_arr;
    }

    delete[] local_arr;
    MPI_Finalize();

    CALI_MARK_END("main");
    return 0;
}