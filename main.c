#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

#define N 4 // Rozmiar macierzy kwadratowych

void matrix_multiply(int *a, int *b, int *c, int size) {
    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            for (int k = 0; k < size; ++k) {
                c[i*size + j] += a[i*size + k] * b[k*size + j];
            }
        }
    }
}

void cannon_algorithm(int *a, int *b, int *c, int size, int my_rank, int p, MPI_Comm *cart_comm) {
    int *temp_a = (int *)malloc(size * size * sizeof(int));
    int *temp_b = (int *)malloc(size * size * sizeof(int));
    int *temp_c = (int *)calloc(size * size, sizeof(int));

    MPI_Status status;
    int shift;
    int coords[2], my_coords[2];
    int source, dest;
    int left, right, up, down;

    // Inicjalizacja koordynatów siatki
    coords[0] = my_rank / p;
    coords[1] = my_rank % p;

    for (int step = 0; step < p; ++step) {
        // Przesunięcie macierzy a
        shift = (coords[0] + step) % p;

        MPI_Cart_shift(*cart_comm, 1, -shift, &left, &right);
        MPI_Sendrecv_replace(a, size * size, MPI_INT, left, 0, right, 0, MPI_COMM_WORLD, &status);

        // Przesunięcie macierzy b
        shift = (coords[1] + step) % p;
        MPI_Cart_shift(*cart_comm, 0, -shift, &up, &down);
        MPI_Sendrecv_replace(b, size * size, MPI_INT, up, 0, down, 0, MPI_COMM_WORLD, &status);

        // Mnożenie macierzy lokalnych
        matrix_multiply(a, b, temp_c, size);

        // Przesunięcie macierzy a w kierunku poziomym
        MPI_Cart_shift(*cart_comm, 1, -1, &left, &right);
        MPI_Sendrecv_replace(a, size * size, MPI_INT, left, 0, right, 0, MPI_COMM_WORLD, &status);

        // Przesunięcie macierzy b w kierunku pionowym
        MPI_Cart_shift(*cart_comm, 0, -1, &up, &down);
        MPI_Sendrecv_replace(b, size * size, MPI_INT, up, 0, down, 0, MPI_COMM_WORLD, &status);
    }

    // Sumowanie wyników do macierzy wynikowej
    MPI_Reduce(temp_c, c, size * size, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    free(temp_a);
    free(temp_b);
    free(temp_c);
}

int main(int argc, char *argv[]) {
    int *matrix_a, *matrix_b, *matrix_c;

    int my_rank, size;
    int dims[2] = {2, 2};
    int periods[2] = {0, 0};

    MPI_Comm cart_comm;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    if (size != 4) {
        printf("This program requires exactly 4 processes.\n");
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, 0, &cart_comm);

    if (my_rank == 0) {
        printf("test");
        // Inicjalizacja macierzy
        matrix_a = (int *)malloc(size * size * sizeof(int));
        matrix_b = (int *)malloc(size * size * sizeof(int));
        matrix_c = (int *)calloc(size * size, sizeof(int));

        // Wypełnienie macierzy losowymi wartościami (możesz to zmodyfikować)
        for (int i = 0; i < size * size; ++i) {
            matrix_a[i] = rand() % 10;
            matrix_b[i] = rand() % 10;
        }
    }

    // Rozgłaszanie rozmiaru macierzy do innych procesów
    MPI_Bcast(&size, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Rozgłaszanie macierzy a i b do wszystkich procesów
    MPI_Bcast(matrix_a, size * size, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(matrix_b, size * size, MPI_INT, 0, MPI_COMM_WORLD);

    // Mnożenie macierzy algorytmem Cannona
    cannon_algorithm(matrix_a, matrix_b, matrix_c, size, my_rank, size, &cart_comm);

    if (my_rank == 0) {
        // Wyświetlenie wyniku
        printf("Macierz wynikowa:\n");
        for (int i = 0; i < size; ++i) {
            for (int j = 0; j < size; ++j) {
                printf("%d ", matrix_c[i*size + j]);
            }
            printf("\n");
        }
        free(matrix_a);
        free(matrix_b);
        free(matrix_c);
    }


    MPI_Comm_free(&cart_comm);


    MPI_Finalize();
    return 0;
}