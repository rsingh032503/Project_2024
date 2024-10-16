#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <random>

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
            int idx = dis(gen);
            arr[idx] = dis(gen);
        }
    }
}