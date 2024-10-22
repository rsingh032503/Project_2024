#include <mpi.h>
#include <vector>
#include <algorithm>
#include <iostream>
#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>
#include <cstdlib>

std::vector<int> generateRandomData(int size) {
    std::vector<int> data(size);
    for (int i = 0; i < size; i++) {
        data[i] = rand() % size;
    }
    return data;
}

std::vector<int> generateSortedData(int size) {
    std::vector<int> data(size);
    for (int i = 0; i < size; i++) {
        data[i] = i;
    }
    return data;
}

std::vector<int> generateReverseSortedData(int size) {
    std::vector<int> data(size);
    for (int i = 0; i < size; i++) {
        data[i] = size - i;
    }
    return data;
}

std::vector<int> generateOnePercentPertubedData(int size) {
    std::vector<int> data(size);
    for (int i = 0; i < size; i++) {
        data[i] = i;
    }
    for (int i = 0; i < size / 100; i++) {
        int index1 = rand() % size;
        int index2 = rand() % size;
        std::swap(data[index1], data[index2]);
    }
    return data;
}

std::vector<int> merge(std::vector<int>& left, std::vector<int>& right){
    std::vector<int> result;
    int i = 0;
    int j = 0;

    // sort the two arrays
    while (i < left.size() && j < right.size()){
        if (left[i] < right[j]){
            result.push_back(left[i]);
            i++;
        } else {
            result.push_back(right[j]);
            j++;
        }
    }

    // append the rest of the remaining values in the arrays
    while (i < left.size()){
        result.push_back(left[i]);
        i++;
    }

    while (j < right.size()){
        result.push_back(right[j]);
        j++;
    }

    return result;
}

std::vector<int> mergesort(std::vector<int>& arr){
    // base case
    if (arr.size() == 1){
        return arr;
    }

    // split the array into two halves
    std::vector<int> left(arr.begin(), arr.begin() + arr.size() / 2);
    std::vector<int> right(arr.begin() + arr.size() / 2, arr.end());

    // recursively sort the two halves
    left = mergesort(left);
    right = mergesort(right);

    // merge the two sorted halves
    return merge(left, right);
}

int main(int argc, char** argv) {

    // CALIPER START

    cali::ConfigManager mgr;
    mgr.add("runtime-report");
    mgr.start();

    CALI_MARK_BEGIN("main");

    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Validate inputs

    if (argc != 3) {
        if (rank == 0) {
            std::cerr << "Usage: " << argv[0] << " <array_size_power> <input_type>" << std::endl;
            std::cerr << "Example: " << argv[0] << " 16 random" << std::endl;
            std::cerr << "Input types: random, sorted, reverse_sorted, one_percent_pertubed" << std::endl;
        }
        MPI_Finalize();
        return 1;
    }

    MPI_Comm comm;
    MPI_Comm_dup(MPI_COMM_WORLD, &comm);

    const int ARRAY_SIZE = 1 << atoi(argv[1]);
    std::string input_type = argv[2];
    std::vector<int> globalArray;

    // METADATA

    // std::string algorithm = "mergesort";
    // std::string programming_model = "mpi";
    // std::string data_type = "int";
    // int size_of_data_type = sizeof(int);
    // int input_size = ARRAY_SIZE;
    // std::string input_type = "Random";
    // int num_procs = size;
    // std::string scalability = "strong";
    // int group_number = 9;
    // std::string implementation_source = "online";

    adiak::init(NULL);
    adiak::launchdate();
    adiak::libraries();
    adiak::cmdline();
    adiak::clustername();
    adiak::value("algorithm", "mergesort");
    adiak::value("programming_model", "mpi");
    adiak::value("data_type", "int");
    adiak::value("size_of_data_type", sizeof(int));
    adiak::value("input_size", ARRAY_SIZE);
    adiak::value("input_type", input_type);
    adiak::value("num_procs", size);
    adiak::value("scalability", "strong");
    adiak::value("group_num", 9);
    adiak::value("implementation_source", "online");

    // END METADATA

    // generate random array with size ARRAY_SIZE
    if (rank == 0) {
        CALI_MARK_BEGIN("data_init_runtime");
        globalArray.resize(ARRAY_SIZE);

        // for (int i = 0; i < ARRAY_SIZE; i++) {
        //     globalArray[i] = rand() % ARRAY_SIZE;
        // }
        
        if (input_type == "random") {
            globalArray = generateRandomData(ARRAY_SIZE);
        } else if (input_type == "sorted") {
            globalArray = generateSortedData(ARRAY_SIZE);
        } else if (input_type == "reverse_sorted") {
            globalArray = generateReverseSortedData(ARRAY_SIZE);
        } else if (input_type == "one_percent_pertubed") {
            globalArray = generateOnePercentPertubedData(ARRAY_SIZE);
        }
        else {
            std::cerr << "Invalid input type" << std::endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        CALI_MARK_END("data_init_runtime");
    }

    int localSize = ARRAY_SIZE / size;
    std::vector<int> localArray(localSize);

    // scatter the array
    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_large");
    MPI_Scatter(globalArray.data(), localSize, MPI_INT, localArray.data(), localSize, MPI_INT, 0, comm);
    CALI_MARK_END("comm_large");
    CALI_MARK_END("comm");

    // sort the local array
    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("comp_large");
    localArray = mergesort(localArray);
    CALI_MARK_END("comp_large");
    CALI_MARK_END("comp");

    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_large");
    MPI_Barrier(comm);
    CALI_MARK_END("comm_large");
    CALI_MARK_END("comm");

    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_large");
    MPI_Gather(localArray.data(), localSize, MPI_INT, globalArray.data(), localSize, MPI_INT, 0, comm);
    CALI_MARK_END("comm_large");
    CALI_MARK_END("comm");
    
    // merge the sorted arrays from each process
    if (rank == 0) {
        CALI_MARK_BEGIN("comp");
        CALI_MARK_BEGIN("comp_large");
        for (int i = 1; i < size; i++) {
            std::vector<int> left(globalArray.begin(), globalArray.begin() + i * localSize);
            std::vector<int> right(globalArray.begin() + i * localSize, globalArray.begin() + (i + 1) * localSize);
            std::vector<int> merged = merge(left, right);
            std::copy(merged.begin(), merged.end(), globalArray.begin());
        }
        CALI_MARK_END("comp_large");
        CALI_MARK_END("comp");

        // verify sorting
        CALI_MARK_BEGIN("correctness_check");

        bool sorted = true;
        for (size_t i = 1; i < globalArray.size(); ++i) {
            if (globalArray[i] < globalArray[i - 1]) {
                sorted = false;
                break;
            }
        }

        std::cout << "Array size: " << globalArray.size() << std::endl;
        std::cout << "Array is " << (sorted ? "sorted" : "not sorted") << std::endl;

        CALI_MARK_END("correctness_check");
    }

    MPI_Comm_free(&comm);

    MPI_Finalize();

    CALI_MARK_END("main");

    mgr.stop();

    return 0;
}