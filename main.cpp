#include <stdio.h>
#include <stdlib.h>
#include "mpi.h"
#include <time.h>
#include <math.h>

// Funkcja do przesunięcia macierzy o k kroków w poziomie
void shift_matrix_horizontal(int* cell, int k, MPI_Comm comm) {
    int source, dest;
    MPI_Status status;

    MPI_Cart_shift(comm, 1, -k, &source, &dest);
    MPI_Sendrecv_replace(cell, 1, MPI_INT, dest, 0, source, 0, comm, &status);
}

// Funkcja do przesunięcia macierzy o k kroków w pionie
void shift_matrix_vertical(int* cell, int k, MPI_Comm comm) {
    int source, dest;
    MPI_Status status;

    MPI_Cart_shift(comm, 0, -k, &source, &dest);
    MPI_Sendrecv_replace(cell, 1, MPI_INT, dest, 0, source, 0, comm, &status);
}

void cannon_algorithm(int* A, int* B, int* C, int size, int myid, int numprocs) {
    MPI_Comm comm;
    int dims[2] = { size, size };
    int periods[2] = { 1, 1 };
    MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, 0, &comm);

    int local_A = A[myid];
    int local_B = B[myid];
    int local_C = 0;

    int first_shift_horizontal = myid / size;
    int first_shift_vertical = myid % size;
    shift_matrix_horizontal(&local_A, first_shift_horizontal, comm);
    shift_matrix_vertical(&local_B, first_shift_vertical, comm);
    local_C += local_A * local_B;

    for (int step = 1; step < dims[0]; ++step) {
        shift_matrix_horizontal(&local_A, 1, comm);
        shift_matrix_vertical(&local_B, 1, comm);
        local_C += local_A * local_B;
    }

    if (myid == 0) {
        C[0] = local_C;
        MPI_Status status;
        for (int i = 1; i < numprocs; ++i) {
            MPI_Recv(C + i, 1, MPI_INT, i, 100 + i, MPI_COMM_WORLD, &status);
        }
    }
    else {
        MPI_Send(&local_C, 1, MPI_INT, 0, 100 + myid, MPI_COMM_WORLD);
    }

    MPI_Comm_free(&comm);
}

bool read_matrix(const char* file_name, int size, int* matrix)
{
    FILE* file = fopen(file_name, "r");
    if (file == NULL)
    {
        printf("ERROR: File with data cannot be found\n");
        return false;
    }
    for (int i = 0; i < size * size; i++)
    {
        int element;
        fscanf(file, "%d", &element);
        matrix[i] = element;
    }
    fclose(file);
    return true;
}

int main(int argc, char* argv[]) {
    //Init
    int myid, numprocs;
    int element_width = 2;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    MPI_Bcast(&numprocs, 1, MPI_INT, 0, MPI_COMM_WORLD);

    //Create matrix A and B
    int size = 2;
    if (size * size != numprocs) {
        if (myid == 0) {
            printf("This program requires one process for every matrix element!\n");
        }
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    int* matrix_a = (int*)malloc(size * size * sizeof(int));
    int* matrix_b = (int*)malloc(size * size * sizeof(int));
    int* matrix_c = (int*)calloc(size * size, sizeof(int));

    bool success_reading_data = false;
    if (myid == 0 && read_matrix("matrix_A.txt", size, matrix_a) && read_matrix("matrix_B.txt", size, matrix_b)) {
        success_reading_data = true;
        printf("Matrix A\n");
        for (int i = 0; i < size; i++) {
            for (int j = 0; j < size; j++) {
                printf("%*d ", element_width, matrix_a[i * size + j]);
            }
            printf("\n");
        }

        printf("Matrix B\n");
        for (int i = 0; i < size; i++) {
            for (int j = 0; j < size; j++) {
                printf("%*d ", element_width, matrix_b[i * size + j]);
            }
            printf("\n");
        }
        printf("\n");
    }

    MPI_Bcast(&success_reading_data, 1, MPI_C_BOOL, 0, MPI_COMM_WORLD);
    if (!success_reading_data) {
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    MPI_Bcast(matrix_a, size * size, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(matrix_b, size * size, MPI_INT, 0, MPI_COMM_WORLD);

    //Cannon
    cannon_algorithm(matrix_a, matrix_b, matrix_c, size, myid, numprocs);

    if (myid == 0) {
        printf("Result matrix:\n");
        for (int i = 0; i < size; ++i) {
            for (int j = 0; j < size; ++j) {
                printf("%*d ", element_width, matrix_c[i * size + j]);
            }
            printf("\n");
        }
    }

    //Clear memory
    free(matrix_a);
    free(matrix_b);
    free(matrix_c);
    MPI_Finalize();
    return 0;
}