#include <stdio.h>
#include <malloc.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include "mpi.h"

// Function to read input numbers from the command line
int* read_input_numbers(int* punt_vec, const int* num, char** punt_argv);

// Function to generate random numbers
int* generate_random_numbers(int* punt_vec, const int* num);

// Function to check if a number is a power of two
bool is_power_of_two(const int* num_proc);

// Function to calculate the partial sum of an array
int calculate_partial_sum(const int* punt_vec, const int* num);

// Function to calculate powers of two
int* calculate_pow(int* punt_pow, const int* num);

// Function to print a vector
void print_vector(const int* vec, const int* num);

// Function to sum the elements of a vector
void sum_vector(const int* vec, const int* num);

// Strategy 1 function
void strategy1(const int* curr_id_proc, const int* num_proc, int* partial_sum, MPI_Status* mpi_status, const double* t_start, double* t_end, double* t_diff, double* t_tot);

// Strategy 2 function
void strategy2(const int* curr_id_proc, const double* log_proc, int* partial_sum, const int* punt_pow, MPI_Status* mpi_status, const double* t_start, double* t_end, double* t_diff, double* t_tot);

// Strategy 3 function
void strategy3(const int* curr_id_proc, const double* log_proc, int* partial_sum, const int* punt_pow, MPI_Status* mpi_status, const double* t_start, double* t_end, double* t_diff, double *t_tot);

/* ****************************************************************************************************************** */

int main(int argc, char** argv) {
    int curr_id_proc, num_proc, num_input_elem, num_loc, num_rest, count_elem, index;
    double log_proc, t_start, t_end, t_diff, t_tot;
    int *vec = NULL, *vec_loc = NULL, *pow = NULL;
    int communication_tag;
    MPI_Status mpi_status;

    if (!((argc == 3 && (strtol(argv[2], NULL, 10) > 20)) || (argc > 3 && (strtol(argv[2], NULL, 10) <= 20) && (strtol(argv[2], NULL, 10) == (argc - 3))) || (argc == 3 && (strtol(argv[2], NULL, 10) == 0)))) {
        printf("Invalid number of arguments");
        return 1;
    }

    int strategy = strtol(*(argv + 1), NULL, 10);

    MPI_Init(&argc, &argv);

    /* Start MPI Program */
    MPI_Comm_rank(MPI_COMM_WORLD, &curr_id_proc);
    MPI_Comm_size(MPI_COMM_WORLD, &num_proc);

    // Each process calculates the array of powers of two
    pow = (int*)malloc((num_proc) * sizeof(int));
    calculate_pow(pow, &num_proc);

    // Process P0 reads (num_input_elem, vec: vector_of_elements) and sends information to all processes
    if(curr_id_proc == 0){
        num_input_elem = strtol(*(argv + 2), NULL, 10); // In the third position of argv[], we find the number of elements

        // Allocate memory for the input elements
        vec = (int*)malloc((num_input_elem) * sizeof(int));

        if (num_input_elem <= 20){
            vec = read_input_numbers(vec, &num_input_elem, argv);
        }
        else{
            vec = generate_random_numbers(vec, &num_input_elem);
        }
        sum_vector(vec, &num_input_elem);
    }
    MPI_Bcast(&num_input_elem, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // All processes calculate num_loc and num_rest
    num_loc = (num_input_elem / num_proc);  // Number of elements for the current process
    num_rest = (num_input_elem % num_proc); // Number of elements to assign to {P0, ..., (P_num_rest - 1)} processes

    log_proc = log2(num_proc);

    if (curr_id_proc < num_rest){ // Check if the current process is in {P0,...,(P_num_rest - 1)}
        num_loc += 1;
    }

    // P0 sends elements for each {P1, ..., P_num_proc} processes
    if(curr_id_proc == 0){

        vec_loc = vec; // P0 can use vec directly for vec_loc
        count_elem = num_loc;
        index = 0;

        int id = 1;
        for(; id < num_proc; id++){
            index += count_elem;
            communication_tag = 10 * id;

            // Check if  we have reached the P_num_rest process
            // Processes {P_num_rest,...,P_num_proc} receive count - 1 element
            if(id == num_rest){
                count_elem -= 1;
            }
            MPI_Send((vec_loc + index), count_elem, MPI_INT, id, communication_tag, MPI_COMM_WORLD);
        }
    }
    else{
        // For each process != P0, {P1,...,P_num_proc}
        vec_loc = (int*)malloc((num_loc) * sizeof(int)); 

        communication_tag = 10 * curr_id_proc;
        MPI_Recv(vec_loc, num_loc, MPI_INT, 0, communication_tag, MPI_COMM_WORLD, &mpi_status);
    }

    // For each process {P0,...,P_num_proc}
    MPI_Barrier(MPI_COMM_WORLD);
    t_start = MPI_Wtime();

    int curr_partial_sum = calculate_partial_sum(vec_loc, &num_loc); // Allocate vec_loc

    if(!is_power_of_two(&num_proc)){
        // Strategy 2/3 not applicable! Apply strategy 1!
        strategy1(&curr_id_proc, &num_proc, &curr_partial_sum, &mpi_status, &t_start, &t_end, &t_diff, &t_tot);
    }
    else{
        switch (strategy) {
            case 1:
                strategy1(&curr_id_proc, &num_proc, &curr_partial_sum, &mpi_status, &t_start, &t_end, &t_diff, &t_tot);
                break;
            case 2:
                strategy2(&curr_id_proc, &log_proc, &curr_partial_sum, pow, &mpi_status, &t_start, &t_end, &t_diff, &t_tot);
                break;
            case 3:
                strategy3(&curr_id_proc, &log_proc, &curr_partial_sum, pow, &mpi_status, &t_start, &t_end, &t_diff, &t_tot);
                break;
            default:
                printf("Invalid strategy!\n");
                break;
        }
    }

    free(pow);

    if(curr_id_proc == 0) {
        free(vec);
    }
    else{
        free(vec_loc);
    }
    /* End MPI Program */
    MPI_Finalize();

    return 0;
}

/* ****************************************************************************************************************** */
// Function to read input numbers from the command line
int* read_input_numbers(int* const punt_vec, const int* num, char** const punt_argv){
    int i = 0;
    for(; i < (*num); i++) {
        *(punt_vec + i) = strtol(*(punt_argv + i + 3), NULL, 10); // In the 3rd position of argv[], we find the elements' number
    }
    return punt_vec;
}

// Function to generate random numbers
int* generate_random_numbers(int *punt_vec, const int* num){
    int const max = 1000;
    int const min = 1;

    srand(time(NULL));

    int i = 0;
    for(; i < (*num); i++) {
        *(punt_vec + i) = (rand() % (max - min + 1)) + min;
    }
    return punt_vec;
}

/* ****************************************************************************************************************** */
// Function to check if a number is a power of two
bool is_power_of_two(const int* num_proc){
    //All power of two numbers have only one bit set
    // If num is a power of 2, then the bitwise & of num and num-1 will be zero
    return ((*num_proc != 0) && ((*num_proc & (*num_proc - 1)) == 0));
}

/* ****************************************************************************************************************** */
// Function to print a vector
void print_vector(const int* vec, const int* num){
    printf("[");
    int i = 0;
    for(; i < *num; i++){
        if(i == (*num)-1){
            printf("%d]\n", *(vec+i));
            fflush(stdout);
        }
        else{
            printf("%d,", *(vec+i));
            fflush(stdout);
        }
    }
}

// Function to sum the elements of a vector
void sum_vector(const int* vec, const int* num){
    int sum = 0;
    int i = 0;
    for(; i < *num; i++){
        sum += *(vec + i);
    }
    printf("@Vector SUM: %d\n", sum);
    fflush(stdout);
}

/* ****************************************************************************************************************** */
// Function to calculate the partial sum of an array
int calculate_partial_sum(const int* punt_vec, const int* num){
    int sum = 0;
    int i = 0;
    for(; i < *num; i++){
        sum += *(punt_vec + i);
    }
    return sum;
}

// Function to calculate powers of two
int* calculate_pow(int* punt_pow, const int* num){
    *(punt_pow) = 1;
    int pow = 2;
    int i = 1;
    for(; i < *num; i++){
        *(punt_pow + i) = pow;
        pow *= 2;
    }
    return punt_pow;
}
/* ****************************************************************************************************************** */
void strategy1(const int* curr_id_proc, const int* num_proc, int* partial_sum, MPI_Status* mpi_status, const double* t_start, double* t_end, double* t_diff, double *t_tot){
    int tag_receive;
    int tot_sum = 0;

    if((*curr_id_proc) == 0){
        tot_sum = *partial_sum; //Start from partial_sum of P0
        int curr_sum = 0;

        // P0 receives the partial sum for each P1,...,P_num_proc}
        int id = 1;
        for(; id < (*num_proc); id++){
            tag_receive = (100 * id);
            MPI_Recv(&curr_sum, 1, MPI_INT, id, tag_receive, MPI_COMM_WORLD, mpi_status);
            tot_sum += curr_sum;
        }
    }
    else{
        //All {P1,...,P_num_proc} processes send partial_sum to P0 process
        tag_receive = (100 * (*curr_id_proc));
        MPI_Send(partial_sum, 1, MPI_INT, 0, tag_receive, MPI_COMM_WORLD);
    }
    *t_end = MPI_Wtime();
    *t_diff = *t_end - *t_start;
    //printf("P%d time spent: %.5f sec\n", *curr_id_proc, *t_diff);

    MPI_Reduce(t_diff, t_tot, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if(*curr_id_proc == 0){
        // Process P0 prints the total sum and total time
        printf("#P0 Strategy1 SUM: %d\n", tot_sum);
        printf("#P0 Total time spent: %.7f sec\n", *t_tot);
        fflush(stdout);
    }
}

/* ****************************************************************************************************************** */
void strategy2(const int* curr_id_proc, const double* log_proc, int* partial_sum, const int* punt_pow, MPI_Status* mpi_status, const double* t_start, double* t_end, double* t_diff, double *t_tot){
    int tag_send, tag_receive;
    int rec_sum = 0;

    int i = 0;
    for(; i < (*log_proc); i++){
        // Check if the current process participates in the communication
        if((*curr_id_proc % *(punt_pow+i)) == 0){
            if((*curr_id_proc % *(punt_pow+i+1)) == 0){ //Current process is a receiver from curr_id_proc + 2^i
                tag_receive = 200 * i;
                MPI_Recv(&rec_sum, 1, MPI_INT, (*curr_id_proc + *(punt_pow+i)), tag_receive, MPI_COMM_WORLD, mpi_status);

                *partial_sum += rec_sum;

            }
            else{ //Current process is a sender to curr_id_proc - 2^i
                tag_send = 200 * i;
                MPI_Send(partial_sum, 1, MPI_INT, (*curr_id_proc - *(punt_pow+i)), tag_send , MPI_COMM_WORLD);
            }
        }
    }
    *t_end = MPI_Wtime();
    *t_diff = *t_end - *t_start;
    //printf("P%d time spent: %.5f sec\n", *curr_id_proc, *t_diff);

    MPI_Reduce(t_diff, t_tot, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if(*curr_id_proc == 0){
        // Process P0 prints the total sum and total time
        printf("#P0 Strategy2 SUM: %d\n", *partial_sum);
        printf("#P0 Total time spent: %.7f sec\n", *t_tot);
        fflush(stdout);
    }
}

/* ****************************************************************************************************************** */

void strategy3(const int* curr_id_proc, const double* log_proc, int* partial_sum, const int* punt_pow, MPI_Status* mpi_status, const double* t_start, double* t_end, double* t_diff, double *t_tot){
    int tag_rec, tag_send;
    int rec_sum = 0;

    // All processes participate in the communication
    int i = 0;
    for(; i < (*log_proc); i++){
        if( (*curr_id_proc % *(punt_pow + i + 1) ) < *(punt_pow + i) ){ //Current process is a sender/receiver from curr_id_proc + 2^i
            tag_send = 400 * i;
            MPI_Send(partial_sum, 1, MPI_INT, (*curr_id_proc + *(punt_pow + i)), tag_send , MPI_COMM_WORLD);

            tag_rec = 300 * i;
            MPI_Recv(&rec_sum, 1, MPI_INT, (*curr_id_proc + *(punt_pow + i)), tag_rec, MPI_COMM_WORLD, mpi_status);

            *partial_sum += rec_sum;

        }
        else{ //Current process is a sender/receiver to curr_id_proc - 2^i
            tag_send = 300 * i;
            MPI_Send(partial_sum, 1, MPI_INT, (*curr_id_proc - *(punt_pow + i)), tag_send , MPI_COMM_WORLD);

            tag_rec = 400 * i;
            MPI_Recv(&rec_sum, 1, MPI_INT, (*curr_id_proc - *(punt_pow + i)), tag_rec, MPI_COMM_WORLD, mpi_status);

            *partial_sum += rec_sum;
        }
    }
    *t_end = MPI_Wtime();
    *t_diff = *t_end - *t_start;
    //printf("P%d time spent: %.5f sec\n", *curr_id_proc, *t_diff);

    MPI_Reduce(t_diff, t_tot, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    // All processes print the total sum
    printf("#P%d Strategy3 SUM: %d\n", *curr_id_proc, *partial_sum);
    if(*curr_id_proc == 0){
        // Process P0 prints the total time
        printf("#P0 Total time spent: %.7f sec\n", *t_tot);
        fflush(stdout);
    }
}

