#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "mpi.h"

/* Exercise 3
    P processes, matrix A of dimensions NxM. Write a function that:
    1. Creates a grid pxq of processors.
    2. Identifies pxq rectangular sub-blocks of matrix A.
    3. Assigns to each processor the sub-block of A with corresponding coordinates.
*/

/* ****************************************************************************************************************** */

int* generate_random_numbers(int* array_ptr, int num, int* min, int* max);

void generate_random_number_grid(int* num_ptr, int* total_procs, int* min, int* max);

int* calculate_sub_matrix(int* matrix_ptr, int* sub_matrix_ptr, int* columns, int* rloc, int* cloc);

void init_my_sub_matrix_type(int* matrix_sizes, int* sub_matrix_sizes, int* coordinate, int* rows_matrix, int* columns_matrix, int* rloc, int* cloc);

void update_coordinate_sub_matrix(int* coordinate, int* columns_matrix, int* rloc, int* cloc);

void print_matrix(int* matrix, int* rows, int* columns);

void print_matrix_divided(int* matrix, int* rows, int* columns, int* rloc, int* cloc, int* col_grid);

void print_matrix_critical(int* curr_proc_id, int* sub_matrix_ptr, int* total_procs, int* rloc, int* cloc);

void int_my_comm_grid(int* vec_coordinate, int* vec_dim, int* vec_period, int* reorder, int* vec_row_col, int* rows_grid, int* cols_grid);

void create_grid(MPI_Comm* my_comm_grid, int* dim, int** vec_dim, int* id_proc_grid, int* total_procs, int** vec_coordinate, int** vec_period, int* reorder);

/* ****************************************************************************************************************** */

int main(int argc, char** argv) {
    int curr_proc_id, total_procs;
    int dim = 2, rows_grid, cols_grid, id_proc_grid;
    int *vec_coordinate, *vec_dim, *vec_period, reorder, *vec_row_col;
    int rows_matrix, columns_matrix, rloc, cloc, *ptr_matrix_loc;
    MPI_Comm my_comm_grid;
    MPI_Status mpi_status;

    int* matrix_sizes, *sub_matrix_sizes, *coordinate;
    MPI_Datatype my_sub_matrix_type;

    int *ptr_matrix;
    /* *********************************************************** */
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &curr_proc_id);
    MPI_Comm_size(MPI_COMM_WORLD, &total_procs);

    // INSTRUCTIONS EXECUTED ONLY BY P0
    if (curr_proc_id == 0) {
        printf("@ ENTER THE NUMBER OF ROWS OF THE DATA MATRIX M: "); fflush(stdout);
        scanf("%d", &rows_matrix);

        //! FOR NOW, CONSIDER ONLY SQUARE MATRICES
        columns_matrix = rows_matrix;
    }

    // ALL PROCESSORS RECEIVE ROWS_MATRIX AND COLUMNS_MATRIX FROM P0
    // PROCESSOR P0 SENDS ROWS_MATRIX AND COLUMNS_MATRIX TO ALL OTHER PROCESSORS
    MPI_Bcast(&rows_matrix, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&columns_matrix, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // INSTRUCTIONS EXECUTED ONLY BY P0
    if (curr_proc_id == 0) {
        // ALLOCATE SPACE FOR THE DATA MATRIX
        ptr_matrix = (int *) malloc((rows_matrix * columns_matrix) * (sizeof(int)));

        ptr_matrix = generate_random_numbers(ptr_matrix, (rows_matrix * columns_matrix), NULL, NULL);
        printf("[P%d] The GENERATED MATRIX %d x %d is: \n", curr_proc_id, rows_matrix, columns_matrix);
        print_matrix(ptr_matrix, &rows_matrix, &columns_matrix);

        // GENERATE DIMENSIONS FOR THE PROCESSOR GRID
        generate_random_number_grid(&rows_grid, &total_procs, NULL, &total_procs);
        cols_grid = total_procs / rows_grid; // GET THE NUMBER OF COLUMNS
        printf("[P%d] The generated grid dimensions are: %d x %d\n", curr_proc_id, rows_grid, cols_grid);

        // CALCULATE DIMENSIONS OF SUB-MATRICES
        rloc = rows_matrix / rows_grid;
        cloc = columns_matrix / cols_grid;
        printf("[P%d] Each processor will have a data sub-matrix m of dimensions %d x %d\n", curr_proc_id, rloc, cloc);

        print_matrix_divided(ptr_matrix, &rows_matrix, &columns_matrix, &rloc, &cloc, &rows_grid);
    }

    // ALL PROCESSORS RECEIVE ROW_GRID AND COLS_GRID FROM P0
    // PROCESSOR P0 SENDS ROWS_GRID AND COLS_GRID TO ALL OTHER PROCESSORS
    MPI_Bcast(&rows_grid, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&cols_grid, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // ALL PROCESSORS RECEIVE RLOC AND CLOC FROM P0
    // PROCESSOR P0 SENDS RLOC AND CLOC TO ALL OTHER PROCESSORS
    MPI_Bcast(&rloc, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&cloc, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // ALL PROCESSORS ALLOCATE MEMORY FOR THEIR DATA SUB-MATRIX
    ptr_matrix_loc = (int*) malloc((rloc * cloc) * (sizeof(int))); //! P0 COULD USE PTR_MATRIX

    // INSTRUCTIONS EXECUTED ONLY BY P0
    if(curr_proc_id == 0){
        matrix_sizes = (int*) malloc(2 * sizeof (int));
        sub_matrix_sizes = (int*) malloc(2 * sizeof (int));
        coordinate  = (int*) malloc(2 * sizeof (int));

        // INITIALIZE NECESSARY STRUCTURES FOR CALCULATING SUB-MATRICES
        init_my_sub_matrix_type(matrix_sizes, sub_matrix_sizes, coordinate, &rows_matrix, &columns_matrix, &rloc, &cloc);

        // P0 CALCULATES ITS SUB-MATRIX
        ptr_matrix_loc = calculate_sub_matrix(ptr_matrix, ptr_matrix_loc, &columns_matrix, &rloc, &cloc);

        // P0 CALCULATES ALL SUB-MATRICES OF ALL OTHER PROCESSORS
        for(int id = 1; id < total_procs; id++){
            MPI_Type_create_subarray(2, matrix_sizes, sub_matrix_sizes, coordinate, MPI_ORDER_C, MPI_INT, &my_sub_matrix_type);
            MPI_Type_commit(&my_sub_matrix_type);

            // P0 SENDS THE CALCULATED SUB-MATRICES TO ALL OTHER PROCESSORS {P1,...,PN}
            MPI_Send(ptr_matrix, 1, my_sub_matrix_type, id, 100 + id, MPI_COMM_WORLD);

            // UPDATE COORDINATES FOR CALCULATING THE NEXT SUB-MATRIX
            update_coordinate_sub_matrix(coordinate, &columns_matrix, &rloc, &cloc);
        }


    }
    else{
        // ALL PROCESSORS OTHER THAN P0 RECEIVE THE SUB-MATRIX CALCULATED BY P0
        MPI_Recv(ptr_matrix_loc, rloc * cloc, MPI_INT, 0, 100 + curr_proc_id, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    print_matrix_critical(&curr_proc_id, ptr_matrix_loc, &total_procs, &rloc, &cloc);

    // ALL PROCESSORS DEFINE THE VECTOR CONTAINING THEIR COORDINATES
    vec_coordinate = (int*) malloc(dim * sizeof(int));

    // ALL PROCESSORS DEFINE THE VECTOR CONTAINING THE VALUES OF DIMENSIONS (ROWS, COLUMNS)
    vec_dim = (int*)malloc(dim * sizeof(int));

    // ALL PROCESSORS DEFINE THE VECTOR CONTAINING THE PERIODICITY OF THE DIMENSIONS
    vec_period = (int*)malloc(dim * sizeof(int));

    // ALL PROCESSORS DEFINE THE VECTOR THAT WILL ALLOW GRID PARTITIONING BY ROWS AND COLUMNS
    vec_row_col = (int*)malloc(dim * sizeof(int));

    // ALL PROCESSORS INITIALIZE NECESSARY STRUCTURES FOR GRID CREATION
    int_my_comm_grid(vec_coordinate, vec_dim, vec_period, &reorder, vec_row_col, &rows_grid, &cols_grid);

    create_grid(&my_comm_grid, &dim,  &vec_dim, &id_proc_grid, &total_procs, &vec_coordinate, &vec_period, &reorder);

    MPI_Finalize();

    // INSTRUCTIONS EXECUTED ONLY BY P0
    if(curr_proc_id == 0){
        // DEALLOCATIONS ONLY FOR P0
        MPI_Type_free(&my_sub_matrix_type);
        free(matrix_sizes);
        free(sub_matrix_sizes);
        free(coordinate);
    }
    // DEALLOCATIONS FOR ALL PROCESSORS
    free(ptr_matrix);
    free(ptr_matrix_loc);
    free(vec_coordinate);
    free(vec_dim);
    free(vec_period);
    free(vec_row_col);

    return 0;
}

/* ****************************************************************************************************************** */
void print_matrix(int* matrix, int* rows, int* columns){
    for(int i = 0; i < *rows; i++) {
        fflush(stdout);
        for (int j = 0; j < *columns; j++) {
            printf("\t%d", *(matrix + (i * (*columns) + j)));
        }
        printf("\n");
    }
    fflush(stdout);
}

void print_matrix_divided(int* matrix, int* rows, int* columns, int* rloc, int* cloc, int* col_grid){
    int countr = 0;
    int countc = 0;

    printf("\n");
    for(int k = 0; k < *columns+*col_grid+1; k++){
        printf("---\t");
    }
    printf("\n");
    for(int i = 0; i < *rows; i++) {
        for (int j = 0; j < *columns; j++) {
            if(j == 0){
                printf("|");
            }
            if(countc == *cloc-1){
                printf("\t%d\t|", *(matrix + (i * (*columns) + j)));
                countc = 0;
            }
            else{
                printf("\t%d", *(matrix + (i * (*columns) + j)));
                countc += 1;
            }
        }
        printf("\n");

        if (countr == *rloc-1){
            for(int k = 0; k < *columns+*col_grid+1; k++){
                printf("---\t");
            }
            printf("\n");
            countr = 0;
        }
        else{
            countr += 1;
        }
        fflush(stdout);
    }
}

void print_matrix_critical(int* curr_proc_id, int* ptr_matrix_loc, int* total_procs, int* rloc, int* cloc){
    int rank = 0;
    while (rank < *total_procs) {
        if (*curr_proc_id == rank) {
            printf ("\nMatrix of P%d: \n", *curr_proc_id);
            print_matrix(ptr_matrix_loc, rloc, cloc);
        }
        rank ++;
        MPI_Barrier(MPI_COMM_WORLD);
    }
}

void init_my_sub_matrix_type(int* matrix_sizes, int* sub_matrix_sizes, int* coordinate, int* rows_matrix, int* columns_matrix, int* rloc, int* cloc){
    *matrix_sizes = *rows_matrix;
    *(matrix_sizes + 1) = *columns_matrix;

    *sub_matrix_sizes = *rloc;
    *(sub_matrix_sizes + 1) = *cloc;

    // START CALCULATION OF SUB-MATRICES FROM (0, CLOC)
    *coordinate = 0;
    *(coordinate + 1) = *cloc;
}

void int_my_comm_grid(int* vec_coordinate, int* vec_dim, int* vec_period, int* reorder, int* vec_row_col, int* rows_grid, int* cols_grid){
    *(vec_dim) = *rows_grid;
    *(vec_dim + 1) = *cols_grid;

    // TO AVOID REORDERING OF PROCESSORS
    *reorder = 0;

    // TO AVOID PERIODICITY, OTHERWISE IT WOULD BE A TORUS
    *(vec_period) = *(vec_period + 1) = 0;
}

/* ****************************************************************************************************************** */
int* generate_random_numbers(int *array_ptr, const int num, int* min, int* max){
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
        *(array_ptr + i) = (rand() % (MAX - MIN + 1)) + MIN;
    }
    return array_ptr;
}

void generate_random_number_grid(int* num_ptr, int* total_procs, int* min, int* max){
    int MIN = 2; // AVOID GENERATING ROW OR COLUMN GRIDS
    int MAX = 1000;

    if(min != NULL){
        MIN = *min;
    }
    if(max != NULL){
        MAX = *max;
    }

    srand(time(NULL));
    do {
        *(num_ptr) = (rand() % (MIN - MAX + 1)) + MIN;
    }while(*total_procs % *num_ptr != 0);

}
/* ****************************************************************************************************************** */
int* calculate_sub_matrix(int* matrix_ptr, int* sub_matrix_ptr, int* columns, int* rloc, int* cloc){
    for(int i = 0; i < *rloc; i++){
        for(int j = 0; j < *cloc; j++){
            *(sub_matrix_ptr + (i * (*cloc) + j)) = *(matrix_ptr + (i * (*columns) + j));
        }
    }
    return sub_matrix_ptr;
}

void update_coordinate_sub_matrix(int* coordinate, int* columns_matrix, int* rloc, int* cloc){
    // IF THE NEXT SUB-MATRIX WILL EXCEED THE COLUMN,
    // THEN THE NEXT SUB-MATRIX WILL START FROM (CURRENT_ROW + RLOC, 0)
    if(*(coordinate + 1) + *cloc >= *columns_matrix ){
        *(coordinate) = *(coordinate) + *rloc;
        *(coordinate + 1) = 0;
    }
    else{
        // IF THE NEXT SUB-MATRIX DOES NOT EXCEED THE COLUMN, IT STARTS FROM (CURRENT_ROW, CURRENT_COLUMN + CLOC)
        *(coordinate + 1) = *(coordinate + 1) + *cloc;
    }
}

void create_grid(MPI_Comm* my_comm_grid, int* dim, int** vec_dim, int* id_proc_grid, int* total_procs, int** vec_coordinate, int** vec_period, int* reorder){
    // EACH PROCESS DEFINES THE TWO-DIMENSIONAL GRID
    MPI_Cart_create(MPI_COMM_WORLD, *dim, *vec_dim, *vec_period, *reorder, my_comm_grid);

    // ASSIGN EACH PROCESSOR an IDENTIFIER in the NEW COMMUNICATOR
    MPI_Comm_rank(*my_comm_grid, id_proc_grid);

    // EACH PROCESS CALCULATES ITS COORDINATES IN THE VECTOR vec_coordinate
    MPI_Cart_coords(*my_comm_grid, *id_proc_grid, *dim, *vec_coordinate);

}
/* ****************************************************************************************************************** */
