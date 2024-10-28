#include <mpi.h>
#include <vector>
#include <algorithm>
#include <iostream>
#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>
#include <cstdlib>

void generateRandomData(int *data, int size)
{
    for (int i = 0; i < size; i++)
    {
        data[i] = rand() % size;
    }
}

void generateSortedData(int *data, int size)
{
    for (int i = 0; i < size; i++)
    {
        data[i] = i;
    }
}

void generateReverseSortedData(int *data, int size)
{
    for (int i = 0; i < size; i++)
    {
        data[i] = size - i;
    }
}

void generateOnePercentPertubedData(int *data, int size)
{
    for (int i = 0; i < size; i++)
    {
        data[i] = i;
    }
    for (int i = 0; i < size / 100; i++)
    {
        int index1 = rand() % size;
        int index2 = rand() % size;
        std::swap(data[index1], data[index2]);
    }
}

void merge(int *left, int leftSize, int *right, int rightSize, int *result)
{
    int i = 0;
    int j = 0;
    int k = 0;

    // merge arrays
    while (i < leftSize && j < rightSize)
    {
        if (left[i] < right[j])
        {
            result[k++] = left[i++];
        }
        else
        {
            result[k++] = right[j++];
        }
    }

    while (i < leftSize)
    {
        result[k++] = left[i++];
    }

    while (j < rightSize)
    {
        result[k++] = right[j++];
    }
}

void mergesort(int *arr, int size)
{
    // Base case
    if (size < 2)
    {
        return;
    }

    int mid = size / 2;
    int *left = new int[mid];
    int *right = new int[size - mid];

    memcpy(left, arr, mid * sizeof(int));
    memcpy(right, arr + mid, (size - mid) * sizeof(int));

    // recurvsive sort
    mergesort(left, mid);
    mergesort(right, size - mid);

    merge(left, mid, right, size - mid, arr);

    delete[] left;
    delete[] right;
}

int main(int argc, char **argv)
{

    cali::ConfigManager mgr;
    mgr.add("runtime-report");
    mgr.start();

    CALI_MARK_BEGIN("main");

    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc != 3)
    {
        if (rank == 0)
        {
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

    int *globalArray = nullptr;

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

    // data generation
    if (rank == 0)
    {
        CALI_MARK_BEGIN("data_init_runtime");
        globalArray = new int[ARRAY_SIZE];

        if (input_type == "random")
        {
            generateRandomData(globalArray, ARRAY_SIZE);
        }
        else if (input_type == "sorted")
        {
            generateSortedData(globalArray, ARRAY_SIZE);
        }
        else if (input_type == "reverse_sorted")
        {
            generateReverseSortedData(globalArray, ARRAY_SIZE);
        }
        else if (input_type == "one_percent_pertubed")
        {
            generateOnePercentPertubedData(globalArray, ARRAY_SIZE);
        }
        else
        {
            std::cerr << "Invalid input type" << std::endl;
            MPI_Abort(comm, 1);
        }

        CALI_MARK_END("data_init_runtime");
    }

    int localSize = ARRAY_SIZE / size;
    int *localArray = new int[localSize];

    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_large");
    MPI_Scatter(globalArray, localSize, MPI_INT, localArray, localSize, MPI_INT, 0, comm);
    CALI_MARK_END("comm_large");
    CALI_MARK_END("comm");

    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("comp_large");
    mergesort(localArray, localSize);
    CALI_MARK_END("comp_large");
    CALI_MARK_END("comp");

    // parallel merging of sorted arrays
    for (int step = 1; step < size; step *= 2)
    {
        if (rank % (2 * step) == 0)
        {
            if (rank + step < size)
            {
                CALI_MARK_BEGIN("comm");
                CALI_MARK_BEGIN("comm_large");

                int recvSize = ARRAY_SIZE / (size / step);
                int *recvArray = new int[recvSize];
                MPI_Recv(recvArray, recvSize, MPI_INT, rank + step, 0, comm, MPI_STATUS_IGNORE);

                CALI_MARK_END("comm_large");
                CALI_MARK_END("comm");

                CALI_MARK_BEGIN("comp");
                CALI_MARK_BEGIN("comp_large");

                int *mergedArray = new int[localSize + recvSize];
                merge(localArray, localSize, recvArray, recvSize, mergedArray);

                delete[] localArray;
                delete[] recvArray;
                localArray = mergedArray;
                localSize += recvSize;

                CALI_MARK_END("comp_large");
                CALI_MARK_END("comp");
            }
        }
        else
        {
            CALI_MARK_BEGIN("comm");
            CALI_MARK_BEGIN("comm_large");

            int sendToRank = rank - step;
            MPI_Send(localArray, localSize, MPI_INT, sendToRank, 0, comm);

            CALI_MARK_END("comm_large");
            CALI_MARK_END("comm");

            break;
        }
    }

    // validation
    if (rank == 0)
    {
        globalArray = localArray;

        CALI_MARK_BEGIN("correctness_check");

        bool sorted = true;
        for (size_t i = 1; i < ARRAY_SIZE; ++i)
        {
            if (globalArray[i] < globalArray[i - 1])
            {
                sorted = false;
            }
        }

        std::cout << "Array size: " << ARRAY_SIZE << std::endl;
        std::cout << "Array is " << (sorted ? "sorted" : "not sorted") << std::endl;

        CALI_MARK_END("correctness_check");

        delete[] globalArray;
    }
    else
    {
        delete[] localArray;
    }

    MPI_Comm_free(&comm);

    MPI_Finalize();

    CALI_MARK_END("main");

    mgr.stop();

    return 0;
}