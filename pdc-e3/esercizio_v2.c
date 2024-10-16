#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "mpi.h"

/* ****************************************************************************************************************** */
// Generate a random number within a given range
void generate_random_number(int *number, int* min, int* max);

// Generate an array of random numbers within a given range
int* generate_random_numbers(int* punt_vec, int num, int* min, int* max);

// Generate random dimensions for a grid of processes
void generate_random_number_grid(int* punt_num, int* num_tot_proc, int* min, int* max);

/* ****************************************************************************************************************** */
// Calculate the local sub-matrix for P0
int* calculate_sub_matrix(int* ptr_matrix, int* ptr_matrix_loc, int* columns, int* rloc, int* cloc);

// Update the coordinates for the next sub-matrix calculation
void update_starts_sub_matrix(int* coordinate, int* columns_matrix, int* id, int* matrix_dim);

/* ****************************************************************************************************************** */
// Print a matrix
void print_matrix(int* Matrix, int* rows, int* columns);

// Print matrices in a synchronized manner to avoid overlapping output
void print_matrix_synch(int* curr_id_proc, int* ptr_matrix_loc, int* num_total_proc, int* rloc, int* cloc);

/* ****************************************************************************************************************** */
// Initialize MPI grid dimensions and related parameters
void init_my_comm_grid(int* vec_coordinate, int* vec_dim, int* vec_period, int* reorder, int* vec_row_col, int* rows_grid, int* cols_grid);

// Create MPI Cartesian grid communicator
void create_grid(MPI_Comm* my_comm_grid, int* dim, int** vec_dim, int* id_proc_grid, int* num_total_proc, int** vec_coordinate, int** vec_period, int* reorder);

/* ****************************************************************************************************************** */

int main(int argc, char** argv) {
    int curr_id_proc, num_total_proc;
    int dim = 2, rows_grid, cols_grid, rrest, crest, id_proc_grid;
    int *vec_coordinate, *vec_dim, *vec_period, reorder, *vec_row_col;
    int rows_matrix, columns_matrix, base_rloc, base_cloc, rloc, cloc, *ptr_matrix, *ptr_matrix_loc;
    int *matrix_sizes, *sub_matrix_sizes, *starts, *matrix_dim;
    MPI_Comm my_comm_grid;
    MPI_Status mpi_status;
    MPI_Datatype my_sub_matrix_type;

    /* *********************************************************** */
    MPI_Init(&argc, &argv);

    MPI_Comm_rank(MPI_COMM_WORLD, &curr_id_proc);
    MPI_Comm_size(MPI_COMM_WORLD, &num_total_proc);

    // INSTRUCTIONS EXECUTED ONLY BY P0
    if (curr_id_proc == 0) {
        do {
            printf("@ Enter the number of rows (>1) for the data matrix M: ");
            fflush(stdout);
            scanf("%d", &rows_matrix);
        } while (rows_matrix < 2);

        //!*************************************************************************************************************
        //! GENERATE MATRICES WITH AT LEAST 2 COLUMNS
        int min_c = 2; // MINIMUM NUMBER OF COLUMNS
        int max_c = 10; // MAXIMUM NUMBER OF COLUMNS
        generate_random_number(&columns_matrix, &min_c, &max_c);
        //!*************************************************************************************************************
    }

    // ALL PROCESSORS RECEIVE ROWS_MATRIX AND COLUMNS_MATRIX FROM P0
    // PROCESSOR P0 SENDS ROWS_MATRIX AND COLUMNS_MATRIX TO ALL OTHER PROCESSORS
    MPI_Bcast(&rows_matrix, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&columns_matrix, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // INSTRUCTIONS EXECUTED ONLY BY P0
    if (curr_id_proc == 0) {
        // P0 ALLOCATES SPACE FOR THE DATA MATRIX
        ptr_matrix = (int *) malloc((rows_matrix * columns_matrix) * (sizeof(int)));
        ptr_matrix = generate_random_numbers(ptr_matrix, (rows_matrix * columns_matrix), NULL, NULL);
        printf("[#] The generated matrix M %d x %d is: \n", rows_matrix, columns_matrix);
        print_matrix(ptr_matrix, &rows_matrix, &columns_matrix);

        // P0 GENERATES DIMENSIONS FOR THE PROCESSOR GRID
        generate_random_number_grid(&rows_grid, &num_total_proc, NULL, &num_total_proc);
        cols_grid = num_total_proc / rows_grid; // OBTAIN THE NUMBER OF COLUMNS
        printf("[#] The generated grid dimensions are: %d x %d\n", rows_grid, cols_grid);

        // P0 CALCULATES THE BASE DIMENSIONS OF THE SUB-MATRICES
        base_rloc = rows_matrix / rows_grid;
        base_cloc = columns_matrix / cols_grid;
        printf("[#] The base dimensions are base_rloc: %d, base_cloc: %d\n", base_rloc, base_cloc);

        // P0 CALCULATES THE REMAINING ROWS AND COLUMNS
        rrest = rows_matrix % rows_grid;
        crest = columns_matrix % cols_grid;
        printf("[#] rrest: %d, crest: %d\n", rrest, crest);

        matrix_dim = (int *) malloc(num_total_proc * 2 * sizeof(int));

        // P0 CALCULATES RLOC AND CLOC FOR PROCESSORS {P1,...,PN-1}
        for (int id = 0; id < num_total_proc; id++) {
            int curr_rloc = base_rloc;
            int curr_cloc = base_cloc;

            // IF THERE IS ONE MORE ROW, ALL PROCESSORS WITH ID < ROW_GRID RECEIVE ONE MORE ROW
            if (rrest != 0 && (id < rows_grid)) {
                curr_rloc += 1;
            }
            // IF THERE IS ONE MORE COLUMN, ALL PROCESSORS IN THE "COLUMN" RECEIVE ONE MORE COLUMN
            if (crest != 0 && (id % cols_grid) == 0) {
                curr_cloc += 1;
            }

            if (id == 0) {
                rloc = curr_rloc;
                cloc = curr_cloc;
            } else {
                MPI_Send(&curr_rloc, 1, MPI_INT, id, id * 100, MPI_COMM_WORLD);
                MPI_Send(&curr_cloc, 1, MPI_INT, id, id * 100, MPI_COMM_WORLD);
            }

            // P0 SAVES THE RLOC AND CLOC OF PROCESSORS {P0,...,PN-1}
            *(matrix_dim + (id * 2)) = curr_rloc;
            *(matrix_dim + (id * 2 + 1)) = curr_cloc;
        }
        //!*************************************************************************************************************
        //! TEMPORARY PRINT FUNCTION
        for (int id = 0; id < num_total_proc; id++) {
            printf("[P%d] will have a data sub-matrix m of dimensions %d x %d\n", id, *(matrix_dim + (id * 2)),
                   *(matrix_dim + (id * 2 + 1)));
            //printf("[P%d] will have a data sub-matrix m of dimensions %d x %d\n", id, matrix_dim[id][0], matrix_dim[id][1]);
            fflush(stdout);
        }
        //!*************************************************************************************************************
    }

    // ALL PROCESSORS RECEIVE ROW_GRID AND COLS_GRID FROM P0
    // PROCESSOR P0 SENDS ROWS_GRID AND COLS_GRID TO ALL OTHER PROCESSORS
    MPI_Bcast(&rows_grid, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&cols_grid, 1, MPI_INT, 0, MPI_COMM_WORLD); //! CAN BE DERIVED

    if (curr_id_proc != 0) {
        MPI_Recv(&rloc, 1, MPI_INT, 0, curr_id_proc * 100, MPI_COMM_WORLD, &mpi_status);
        MPI_Recv(&cloc, 1, MPI_INT, 0, curr_id_proc * 100, MPI_COMM_WORLD, &mpi_status);
    }

    // ALL PROCESSORS ALLOCATE MEMORY FOR THEIR DATA SUB-MATRIX
    ptr_matrix_loc = (int *) malloc(rloc * cloc * (sizeof(int))); //! P0 COULD USE PTR_MATRIX

    if (curr_id_proc == 0) {
        matrix_sizes = (int *) malloc(2 * sizeof(int));
        sub_matrix_sizes = (int *) malloc(2 * sizeof(int));
        starts = (int *) malloc(2 * sizeof(int));

        *matrix_sizes = rows_matrix;
        *(matrix_sizes + 1) = columns_matrix;

        // BEGIN CALCULATION OF SUB-MATRICES FROM (0, P0_CLOC)
        *starts = 0; // ROW
        *(starts + 1) = cloc; // COLUMN

        // P0 CALCULATES ITS SUB-MATRIX
        ptr_matrix_loc = calculate_sub_matrix(ptr_matrix, ptr_matrix_loc, &columns_matrix, &rloc, &cloc);

        // P0 CALCULATES ALL SUB-MATRICES FOR ALL OTHER PROCESSORS
        for (int id = 1; id < num_total_proc; id++) {

            // DIMENSIONS OF THE SUB-MATRIX TO BE CALCULATED FOR PROCESSOR ID
            *sub_matrix_sizes = *(matrix_dim + (id * 2));
            *(sub_matrix_sizes + 1) = *(matrix_dim + (id * 2 + 1));

            MPI_Type_create_subarray(2, matrix_sizes, sub_matrix_sizes, starts, MPI_ORDER_C, MPI_INT,
                                     &my_sub_matrix_type);
            MPI_Type_commit(&my_sub_matrix_type);

            // P0 SENDS THE CALCULATED SUB-MATRICES TO ALL OTHER PROCESSORS {P1,...,PN}
            MPI_Send(ptr_matrix, 1, my_sub_matrix_type, id, 1000 + id, MPI_COMM_WORLD);

            // UPDATE COORDINATES FOR THE CALCULATION OF THE NEXT SUB-MATRIX
            update_starts_sub_matrix(starts, &columns_matrix, &id, matrix_dim);
        }
        // DEALLOCATION
        MPI_Type_free(&my_sub_matrix_type);
    } else {
        // ALL PROCESSORS EXCEPT P0 RECEIVE THE SUB-MATRIX CALCULATED BY P0
        MPI_Recv(ptr_matrix_loc, (rloc * cloc), MPI_INT, 0, 1000 + curr_id_proc, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    // ALL PROCESSORS CALL THE PRINT FUNCTION
    print_matrix_synch(&curr_id_proc, ptr_matrix_loc, &num_total_proc, &rloc, &cloc);

    // ALL PROCESSORS DEFINE THE VECTOR CONTAINING THEIR COORDINATES
    vec_coordinate = (int *) malloc(dim * sizeof(int));

    // ALL PROCESSORS DEFINE THE VECTOR CONTAINING THE VALUES OF DIMENSIONS (ROWS, COLUMNS)
    vec_dim = (int *) malloc(dim * sizeof(int));

    // ALL PROCESSORS DEFINE THE VECTOR CONTAINING THE PERIODICITY OF DIMENSIONS
    vec_period = (int *) malloc(dim * sizeof(int));

    // ALL PROCESSORS DEFINE THE VECTOR THAT WILL ALLOW THE PARTITIONING OF THE GRID BY ROWS AND COLUMNS
    vec_row_col = (int *) malloc(dim * sizeof(int));

    // ALL PROCESSORS INITIALIZE THE NECESSARY STRUCTURES FOR THE CREATION OF THE GRID
    init_my_comm_grid(vec_coordinate, vec_dim, vec_period, &reorder, vec_row_col, &rows_grid, &cols_grid);

    create_grid(&my_comm_grid, &dim, &vec_dim, &id_proc_grid, &num_total_proc, &vec_coordinate, &vec_period, &reorder);

    MPI_Finalize();

    // DEALLOCATIONS ONLY FOR P0
    if (curr_id_proc == 0) {
        free(ptr_matrix);
        free(matrix_dim);
        free(matrix_sizes);
        free(sub_matrix_sizes);
        free(starts);
    }
    // DEALLOCATIONS FOR ALL PROCESSORS
    free(ptr_matrix_loc);
    free(vec_coordinate);
    free(vec_dim);
    free(vec_period);
    free(vec_row_col);

    return 0;
}
/* ****************************************************************************************************************** */
void print_matrix(int* Matrix, int* rows, int* columns){
    for(int i = 0; i < *rows; i++) {
        fflush(stdout);
        for (int j = 0; j < *columns; j++) {
            printf("\t%d", *(Matrix + (i * (*columns) + j)));
        }
        printf("\n");
    }
    fflush(stdout);
}

void print_matrix_synch(int* curr_id_proc, int* ptr_matrix_loc, int* num_total_proc, int* rloc, int* cloc){
    int rank = 0;
    while (rank < *num_total_proc) {
        if (*curr_id_proc == rank) {
            printf ("\nMatrix of P%d: \n", *curr_id_proc);
            print_matrix(ptr_matrix_loc, rloc, cloc);
            fflush(stdout);
        }
        rank ++;
        MPI_Barrier(MPI_COMM_WORLD);
    }
}

/* ****************************************************************************************************************** */
int* generate_random_numbers(int *punt_vec, const int num, int* min, int* max){
    int MIN = 1;
    int MAX = 1000;

    if(min != NULL){
        MIN = *min;
    }
    if(max != NULL){
        MAX = *max;
    }

    srand(time(NULL));

    int i = 0;
    for(; i < (num); i++) {
        *(punt_vec + i) = (rand() % (MAX - MIN + 1)) + MIN;
    }
    return punt_vec;
}


void generate_random_number(int *number, int* min, int* max){
    int MIN = 1;
    int MAX = 1000;

    if(min != NULL){
        MIN = *min;
    }
    if(max != NULL){
        MAX = *max;
    }

    srand(time(NULL));

    *number = (rand() % (MAX - MIN + 1)) + MIN;
}

void generate_random_number_grid(int* punt_num, int* num_tot_proc, int* min, int* max){
    int MIN = 2;  // Avoid generating row or column grids
    int MAX = 1000;

    if(min != NULL){
        MIN = *min;
    }
    if(max != NULL){
        MAX = *max;
    }

    srand(time(NULL));
    do {
        *(punt_num) = (rand() % (MIN - MAX + 1)) + MIN;
    } while(*num_tot_proc % *punt_num != 0);
}

/* ****************************************************************************************************************** */
int* calculate_sub_matrix(int* ptr_matrix, int* ptr_matrix_loc, int* columns, int* rloc, int* cloc){
    for(int i = 0; i < *rloc; i++){
        for(int j = 0; j < *cloc; j++){
            *(ptr_matrix_loc+(i*(*cloc)+j)) = *(ptr_matrix+(i * (*columns) + j));
        }
    }
    return ptr_matrix_loc;
}

void update_starts_sub_matrix(int* coordinate, int* columns_matrix, int* id, int* matrix_dim){
    // If the next sub-matrix exceeds the column,
    // then the next sub-matrix starts from (current row + rloc, 0)
    if(*(coordinate+1) + (*(matrix_dim + (*id * 2 + 1))) >= *columns_matrix ){
        *(coordinate) =  *(coordinate) + (*(matrix_dim + (*id * 2))); // Row
        *(coordinate + 1) = 0; // Column
    }
    else{
        // If the next sub-matrix does not exceed the column, it starts from (current row, current column + cloc)
        *(coordinate + 1) = *(coordinate + 1) + (*(matrix_dim + (*id * 2 + 1))); // Column
    }
}

/* ****************************************************************************************************************** */

void init_my_comm_grid(int* vec_coordinate, int* vec_dim, int* vec_period, int* reorder, int* vec_row_col, int* rows_grid, int* cols_grid){

    *(vec_dim) = *rows_grid;
    *(vec_dim + 1) = *cols_grid;

    // To avoid reordering of processors
    *reorder = 0;

    // To avoid periodicity, otherwise it would be a torus
    *(vec_period) = *(vec_period + 1) = 0;
}

void create_grid(MPI_Comm* my_comm_grid, int* dim, int** vec_dim, int* id_proc_grid, int* num_total_proc, int** vec_coordinate, int** vec_period, int* reorder){
    // Each process defines the bidimensional grid
    MPI_Cart_create(MPI_COMM_WORLD, *dim, *vec_dim, *vec_period, *reorder, my_comm_grid);

    // Assign an identifier to each processor in the new communicator
    MPI_Comm_rank(*my_comm_grid, id_proc_grid);

    // Each process calculates its own vec_coordinate in the vec_coordinate vector
    MPI_Cart_coords(*my_comm_grid, *id_proc_grid, *dim, *vec_coordinate);
}

/* ****************************************************************************************************************** */
