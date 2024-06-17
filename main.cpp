#include <upcxx/upcxx.hpp>
#include <iostream>
#include <fstream>
#include <vector>

using namespace std;
using namespace upcxx;

// Funkcja do przesunięcia pozycji o k kroków w poziomie na prawo
void shift_matrix_horizontal(int &local_cell, global_ptr<int> matrix, int k, int size, int &position)
{
    position = (position + k) % size + (position / size) * size; // położenie na osi x + przesunięcie na osi y
    local_cell = rget(matrix + position).wait();
}

// Funkcja do przesunięcia pozycji o k kroków w pionie w dół
void shift_matrix_vertical(int &local_cell, global_ptr<int> matrix, int k, int size, int &position)
{
    position = ((position / size + k) % size) * size + position % size; // położenie na osi y + przesunięcie na osi x
    local_cell = rget(matrix + position).wait();
}

void cannon_algorithm(global_ptr<int> A, global_ptr<int> B, global_ptr<int> C, int size, int myid, int numprocs)
{
    int local_A = 0;
    int local_B = 0;
    int local_C = 0;
    int position_A = myid;
    int position_B = myid;

    int first_shift_horizontal = myid / size;
    int first_shift_vertical = myid % size;
    shift_matrix_horizontal(local_A, A, first_shift_horizontal, size, position_A);
    shift_matrix_vertical(local_B, B, first_shift_vertical, size, position_B);
    local_C += local_A * local_B;

    for (int step = 1; step < size; ++step)
    {
        shift_matrix_horizontal(local_A, A, 1, size, position_A);
        shift_matrix_vertical(local_B, B, 1, size, position_B);
        local_C += local_A * local_B;
    }

    upcxx::rput(local_C, C + myid).wait();
    upcxx::barrier();
}

bool read_matrix(const char *file_name, int size, global_ptr<int> matrix)
{
    ifstream file(file_name);
    if (!file.is_open())
    {
        cout << "ERROR: File with data cannot be found\n";
        return false;
    }
    int value;
    for (int i = 0; i < size; i++)
    {
        for (int j = 0; j < size; j++)
        {
            if (file >> value)
            {
                rput(value, matrix + i * size + j).wait();
            }
            else
            {
                cerr << "Error reading file at position " << i << "," << j << endl;
                return -1;
            }
        }
    }
    file.close();
    return true;
}

int main(int argc, char *argv[])
{
    upcxx::init();

    int myid = upcxx::rank_me();
    int numprocs = upcxx::rank_n();
    broadcast(numprocs, 0).wait();

    if (argc != 4)
    {
        if (myid == 0)
        {
            cout << "Please enter three arguments <string1> <string2> <integer>";
        }
        upcxx::finalize();
        return EXIT_FAILURE;
    }

    int size = std::atoi(argv[3]);
    if (size * size != numprocs)
    {
        if (myid == 0)
        {
            cout << "This program requires one process for every matrix element!\n";
        }
        upcxx::finalize();
        return EXIT_FAILURE;
    }

    global_ptr<int> matrix_a = nullptr, matrix_b = nullptr, matrix_c = nullptr;
    if (myid == 0)
    {
        matrix_c = new_array<int>(size * size);
        for (int i = 0; i < numprocs; i++)
        {
            rput(0, matrix_c + i).wait();
        }
    }
    matrix_c = broadcast(matrix_c, 0).wait();

    int success_reading_data = 0;
    if (myid == 0)
    {
        matrix_a = new_array<int>(size * size);
        matrix_b = new_array<int>(size * size);

        if (read_matrix(argv[1], size, matrix_a) && read_matrix(argv[2], size, matrix_b))
        {
            success_reading_data = 1;
            cout << "Matrix A\n";
            for (int i = 0; i < size; i++)
            {
                for (int j = 0; j < size; j++)
                {
                    int value = rget(matrix_a + i * size + j).wait();
                    cout << setw(2) << value << " ";
                }
                cout << endl;
            }

            cout << "\nMatrix B\n";
            for (int i = 0; i < size; i++)
            {
                for (int j = 0; j < size; j++)
                {
                    int value = rget(matrix_b + i * size + j).wait();
                    cout << setw(2) << value << " ";
                }
                cout << endl;
            }
            cout << endl;
        }
    }

    broadcast(&success_reading_data, 1, 0).wait();
    upcxx::barrier();

    if (success_reading_data != 1)
    {
        upcxx::finalize();
        return EXIT_FAILURE;
    }

    matrix_a = broadcast(matrix_a, 0).wait();
    matrix_b = broadcast(matrix_b, 0).wait();

    cannon_algorithm(matrix_a, matrix_b, matrix_c, size, myid, numprocs);

    if (myid == 0)
    {
        cout << "Result matrix:\n";
        for (int i = 0; i < size; i++)
        {
            for (int j = 0; j < size; j++)
            {
                int value = rget(matrix_c + i * size + j).wait();
                cout << setw(2) << value << " ";
            }
            cout << endl;
        }

        upcxx::delete_array(matrix_a);
        upcxx::delete_array(matrix_b);
        upcxx::delete_array(matrix_c);
    }

    upcxx::finalize();
    return 0;
}