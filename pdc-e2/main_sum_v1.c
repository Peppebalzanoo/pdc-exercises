#include <stdio.h>
#include <omp.h>
#include <stdlib.h>
#include <time.h>

// Function to generate random numbers
int* generate_random_numbers(int *vector, const int* num);

/* ****************************************************************************************************************** */

int main() {

    int const NUM_THREADS = 8;
    int const NUM = 100;

    int total_sum = 0;

    int* vector = (int*) malloc(NUM * sizeof(int));
    vector = generate_random_numbers(vector, &NUM);

    // OpenMP parallel region with reduction for sum and shared vector
    #pragma omp parallel default(none) reduction(+:total_sum) shared(vector) num_threads(NUM_THREADS)
    {
        // Private variables for each thread
        int partial_sum = 0; // Partial sum for this thread
        int count = 0;       // Counter for this thread

        // OpenMP loop for parallel summation
        #pragma omp for
            for(int i = 0; i < 100; i++){
                partial_sum += *(vector + i);
                count += 1;
            }

        // Print partial results for each thread
        printf("[THREAD %d]: Partial Sum: %d; Addition Count: %d\n", omp_get_thread_num(), partial_sum, count);

        // Accumulate the partial sum into the total sum
        total_sum += partial_sum;
    }

    // Print the total sum computed by all threads
    printf("[MAIN THREAD]: Total Sum: %d\n", total_sum);

    // Free the allocated memory for the vector
    free(vector);

    return 0;
}

/* ****************************************************************************************************************** */

// Function to generate random numbers and populate the vector
int* generate_random_numbers(int *vector, const int* num){
    int const max = 1000;
    int const min = 1;

    // Seed the random number generator with the current time
    srand(time(NULL));

    int i = 0;
    for(; i < (*num); i++) {
        // Generate random numbers and populate the vector
        *(vector + i) = (rand() % (max - min + 1)) + min;
    }
    return vector;
}
