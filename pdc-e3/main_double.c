#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "mpi.h"

/* ****************************************************************************************************************** */
// Generate an array of random numbers within a given range
double *generate_random_numbers(double *punt_vec, int num, int seed);

/* ****************************************************************************************************************** */
// Calculate the local sub-matrix for P0
double *calculate_sub_matrix(const double *ptr_matrix, double *ptr_matrix_loc, const int *columns, const int *rloc, const int *cloc);

// Update the coordinates for the next sub-matrix calculation
void update_starts_sub_matrix(int *coordinate, const int *columns_matrix, int *id, const int *rloc, const int *cloc);

void calculate_matrix_matrix(double *ptr_matrix_Cloc, const double *ptr_matrix_Aloc, const double *ptr_matrix_Bloc, const int *rloc, const int *cloc);
/* ****************************************************************************************************************** */
// Print a matrix
void print_matrix(const double *Matrix, const int *rows, const int *columns);

// Print matrices in a synchronized manner to avoid overlapping output
void print_matrix_synch(const int *curr_id_proc, double *ptr_matrix_loc, const int *num_total_proc, int *rloc, int *cloc, char mat);

/* ****************************************************************************************************************** */
// Initialize MPI grid dimensions and related parameters
void init_my_comm_grid(int *vec_dim, int *vec_period, int *reorder, const int *rows_grid, const int *cols_grid);

// Create MPI Cartesian grid communicator
void create_grid(MPI_Comm *my_comm_grid, const int *dim, int **vec_dim, int *id_proc_grid, int *vec_coordinate, int **vec_period, const int *reorder);

void create_subgrid_row(const MPI_Comm *my_comm_grid, MPI_Comm *my_comm_row, int **vec_row_col, int *new_id);

void create_subgrid_col(const MPI_Comm *my_comm_grid, MPI_Comm *my_comm_col, int **vec_row_col, int *new_id);

/* ****************************************************************************************************************** */

int main(int argc, char **argv) {
    int rows_matrix, columns_matrix, rloc, cloc;
    double *ptr_matrix_A, *ptr_matrix_Aloc, *ptr_matrix_A2loc, *ptr_matrix_B, *ptr_matrix_Bloc, *ptr_matrix_C, *ptr_matrix_Cloc;

    int rows_grid, cols_grid;
    int curr_id_proc, new_id, proc_id_row, proc_id_col, num_total_proc;

    int dim = 2, *vec_coordinate, *vec_dim, *vec_period, reorder, *vec_row_col;
    MPI_Comm my_comm_grid, my_comm_row, my_comm_col;

    int *matrix_sizes, *sub_matrix_sizes, *starts;
    MPI_Datatype my_sub_matrix_type;

    double t0, t1, time_diff, time_tot;

    /* *********************************************************** */
    MPI_Init(&argc, &argv);

    MPI_Comm_rank(MPI_COMM_WORLD, &curr_id_proc);
    MPI_Comm_size(MPI_COMM_WORLD, &num_total_proc);

    if (curr_id_proc == 0) {
        rows_matrix = strtol(*(argv + 1), NULL, 10);
        // CONSIDERING ONLY SQUARE MATRICES
        columns_matrix = rows_matrix;
        printf("[#] The matrices dimensions are: %d x %d\n", rows_matrix, columns_matrix);
        fflush(stdout);

        rows_grid = cols_grid = (int) sqrt(num_total_proc);
        if ((floor(sqrt((num_total_proc))) != sqrt(num_total_proc))) {
            printf("\n[!!!] Error in grid dimensions!\n");
            fflush(stdout);
            MPI_Abort(MPI_COMM_WORLD, 1);
            return 0;
        }
        printf("[#] The generated grid dimensions are: %d x %d\n", rows_grid, cols_grid);
        fflush(stdout);

        rloc = cloc = rows_matrix / rows_grid;
        if (floor(rloc) != rloc) {
            printf("\n[!!!] Error in local sub-matrix dimensions!\n");
            fflush(stdout);
            MPI_Abort(MPI_COMM_WORLD, 2);
            return 0;
        }
        printf("[#] The generated local dimensions are: %d x %d\n", rloc, cloc);
        fflush(stdout);
    }

    // ALL PROCESSORS RECEIVE ROWS_MATRIX, COLUMNS_MATRIX, ROWS_GRID, COLS_GRID, RLOC AND CLOC FROM P0
    MPI_Bcast(&rows_matrix, 1, MPI_INT, 0, MPI_COMM_WORLD);
    columns_matrix = rows_matrix;

    MPI_Bcast(&rows_grid, 1, MPI_INT, 0, MPI_COMM_WORLD);
    cols_grid = rows_grid;

    MPI_Bcast(&rloc, 1, MPI_INT, 0, MPI_COMM_WORLD);
    cloc = rloc;

    if (curr_id_proc == 0) {

        int MIN = 0;
        int MAX = RAND_MAX;
        srand(time(NULL));
        int seed1 = (rand() % (MIN - MAX + 1)) + MIN;
        int seed2 = (rand() % (MIN - MAX + 1)) + MIN;
        printf("[#] Seed1 for A matrix: %d\n", seed1);
        printf("[#] seed2 for B matrix: %d\n", seed2);
        fflush(stdout);

        // P0 ALLOCATES SPACE FOR THE DATA MATRIX A
        ptr_matrix_A = (double *) calloc(rows_matrix * columns_matrix, sizeof(double));
        ptr_matrix_A = generate_random_numbers(ptr_matrix_A, (rows_matrix * columns_matrix), seed1);
        printf("[#] The generated A matrix %d x %d is: \n", rows_matrix, columns_matrix);
        print_matrix(ptr_matrix_A, &rows_matrix, &columns_matrix);

        // P0 ALLOCATES SPACE FOR THE DATA MATRIX B
        ptr_matrix_B = (double *) calloc(rows_matrix * columns_matrix, sizeof(double));
        ptr_matrix_B = generate_random_numbers(ptr_matrix_B, (rows_matrix * columns_matrix), seed2);
        printf("[#] The generated B matrix %d x %d is: \n", rows_matrix, columns_matrix);
        print_matrix(ptr_matrix_B, &rows_matrix, &columns_matrix);
    }

    // ALL PROCESSORS ALLOCATE MEMORY FOR THEIR DATA A_LOC, A2_LOC, B_LOC, C AND C_LOC SUB-MATRIXES
    ptr_matrix_Aloc = (double *) calloc(rloc * cloc, sizeof(double)); //! P0 COULD USE A_PTR_MATRIX
    ptr_matrix_Bloc = (double *) calloc(rloc * cloc, sizeof(double)); //! P0 COULD USE B_PTR_MATRIX
    ptr_matrix_Cloc = (double *) calloc(rloc * cloc, sizeof(double)); //! P0 COULD USE C_PTR_MATRIX
    ptr_matrix_C = (double *) calloc(rows_matrix * columns_matrix, (sizeof(double)));
    //WHEN RECEIVING ROW-BLOCKS OF A_LOC, PROCESSORS SHOULD NOT REPLACE THEIR A_LOC BLOCK WITH THE RECEIVED ONE
    ptr_matrix_A2loc = (double *) calloc(rloc * cloc, sizeof(double));



    // ------------- START CODE FOR EXTRACTING A_LOC, B_LOC SUB-MATRICES FOR P0 AND ALL OTHER PROCESSORS -----------
    if (curr_id_proc == 0) {

        matrix_sizes = (int *) calloc(2, sizeof(int));
        *matrix_sizes = rows_matrix;
        *(matrix_sizes + 1) = columns_matrix;

        sub_matrix_sizes = (int *) calloc(2, sizeof(int));
        *sub_matrix_sizes = rloc;
        *(sub_matrix_sizes + 1) = cloc;

        starts = (int *) calloc(2, sizeof(int));
        *starts = 0;
        *(starts + 1) = cloc;

        // P0 CALCULATES ITS A_LOC SUB-MATRIX
        ptr_matrix_Aloc = calculate_sub_matrix(ptr_matrix_A, ptr_matrix_Aloc, &columns_matrix, &rloc, &cloc);
        // P0 CALCULATES ITS B_LOC SUB-MATRIX
        ptr_matrix_Bloc = calculate_sub_matrix(ptr_matrix_B, ptr_matrix_Bloc, &columns_matrix, &rloc, &cloc);

        // P0 CALCULATES ALL A_LOC SUB-MATRICES FOR ALL OTHER PROCESSORS
        for (int id = 1; id < num_total_proc; id++) {
            MPI_Type_create_subarray(2, matrix_sizes, sub_matrix_sizes, starts, MPI_ORDER_C, MPI_DOUBLE, &my_sub_matrix_type);
            MPI_Type_commit(&my_sub_matrix_type);

            // P0 SENDS THE CALCULATED SUB-MATRICES TO ALL OTHER PROCESSORS {P1,...,PN}
            int tag = 100 + id;
            MPI_Send(ptr_matrix_A, 1, my_sub_matrix_type, id, tag, MPI_COMM_WORLD);

            // UPDATE COORDINATES FOR THE CALCULATION OF THE NEXT SUB-MATRIX
            update_starts_sub_matrix(starts, &columns_matrix, &id, &rloc, &cloc);
        }

        // RESET STARTS OF SUB-MATRICES
        *starts = 0; // ROW
        *(starts + 1) = cloc; // COLUMN

        // P0 CALCULATES ALL B_LOC SUB-MATRICES FOR ALL OTHER PROCESSORS
        for (int id = 1; id < num_total_proc; id++) {

            MPI_Type_create_subarray(2, matrix_sizes, sub_matrix_sizes, starts, MPI_ORDER_C, MPI_DOUBLE, &my_sub_matrix_type);
            MPI_Type_commit(&my_sub_matrix_type);

            // P0 SENDS THE CALCULATED SUB-MATRICES TO ALL OTHER PROCESSORS {P1,...,PN}
            int tag = 200 + id;
            MPI_Send(ptr_matrix_B, 1, my_sub_matrix_type, id, tag, MPI_COMM_WORLD);

            // UPDATE COORDINATES FOR THE CALCULATION OF THE NEXT SUB-MATRIX
            update_starts_sub_matrix(starts, &columns_matrix, &id, &rloc, &cloc);
        }

        if (num_total_proc > 1) {
            // DEALLOCATION
            MPI_Type_free(&my_sub_matrix_type);
        }

    } else {
        // ALL PROCESSORS EXCEPT P0 RECEIVE THE A_LOC and B_LOC SUB-MATRIX CALCULATED BY P0
        int tag_a = 100 + curr_id_proc;
        MPI_Recv(ptr_matrix_Aloc, (rloc * cloc), MPI_DOUBLE, 0, tag_a, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        int tag_b = 200 + curr_id_proc;
        MPI_Recv(ptr_matrix_Bloc, (rloc * cloc), MPI_DOUBLE, 0, tag_b, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    // ------------ END CODE FOR EXTRACTING A_LOC, B_LOC SUB-MATRICES FOR P0 AND ALL OTHER PROCESSORS -----------

    // ALL PROCESSORS DEFINE THE VECTOR CONTAINING THEIR COORDINATES
    vec_coordinate = (int *) calloc(dim, sizeof(int));
    // ALL PROCESSORS DEFINE THE VECTOR CONTAINING THE VALUES OF DIMENSIONS (ROWS, COLUMNS)
    vec_dim = (int *) calloc(dim, sizeof(int));
    // ALL PROCESSORS DEFINE THE VECTOR CONTAINING THE PERIODICITY OF DIMENSIONS
    vec_period = (int *) calloc(dim, sizeof(int));
    // ALL PROCESSORS DEFINE THE VECTOR THAT WILL ALLOW THE PARTITIONING OF THE GRID BY ROWS AND COLUMNS
    vec_row_col = (int *) calloc(dim, sizeof(int));

    // ALL PROCESSORS INITIALIZE THE NECESSARY STRUCTURES FOR THE CREATION OF THE GRID
    init_my_comm_grid(vec_dim, vec_period, &reorder, &rows_grid, &cols_grid);

    // ALL PROCESSORS CREATE THE GRID
    create_grid(&my_comm_grid, &dim, &vec_dim, &new_id, vec_coordinate, &vec_period, &reorder);

    // ALL PROCESSORS CREATE ROW-COMMUNICATOR AND COL-COMMUNICATOR
    create_subgrid_row(&my_comm_grid, &my_comm_row, &vec_row_col, &proc_id_row);
    create_subgrid_col(&my_comm_grid, &my_comm_col, &vec_row_col, &proc_id_col);


    MPI_Barrier(MPI_COMM_WORLD);
    t0 = MPI_Wtime();
    // ----------------------------------------------------- BROADCAST MULTIPLY ROLLING STRATEGY ----------------------------------------------------
    // PROCESSORS WORKING AT PASS (IMO-PASSO) ARE THOSE WITH (ROW COORDINATE) == (COLUMN COORDINATE - (IMO-PASSO))
    for (int passo = 0; passo < rows_grid; passo++) {

        int intuition = vec_coordinate[1] - passo;

        if (intuition < 0) {
            intuition = (((vec_coordinate[1] - passo) + rows_grid) % rows_grid);
        }

        // ------------------------------------------------- BROADCAST START -------------------------------------------------
        //PROCESSORS ON THE DIAGONAL (MAIN, UPPER 1 OR UPPER 2) MUST SEND A_LOC
        if (vec_coordinate[0] == intuition) {

            for (int i = 0; i < rows_grid; i++) {
                // SEND TO PROCESSORS WITH ID_ROW != CURRENT ONE
                if (proc_id_row != i) {
                    int tag = 80 * i;
                    MPI_Send(ptr_matrix_Aloc, (rloc * cloc), MPI_DOUBLE, i, tag, my_comm_row);
                }
            }

        } else {
            // PROCESSORS NOT ON THE DIAGONAL (MAIN, UPPER 1 OR UPPER 2) MUST RECEIVE
            int id_source = (vec_coordinate[0] + passo) % rows_grid;
            int tag = 80 * proc_id_row;
            MPI_Recv(ptr_matrix_A2loc, (rloc * cloc), MPI_DOUBLE, id_source, tag, my_comm_row, MPI_STATUS_IGNORE);
        }
        // ------------------------------------------------- BROADCAST END -------------------------------------------------

        // ------------------------------------------------- MULTIPLY START -------------------------------------------------

        //PROCESSORS ON THE DIAGONAL (MAIN, UPPER 1 OR UPPER 2) MUST CALCULATE A_LOC * B_LOC
        if (vec_coordinate[0] == intuition) {
            calculate_matrix_matrix(ptr_matrix_Cloc, ptr_matrix_Aloc, ptr_matrix_Bloc, &rloc, &cloc);
        }
            //  ALL OTHERS MUST CALCULATE A2_LOC * B_LOC (A2_LOC IS THE BLOCK OF A RECEIVED FROM THOSE ON THE DIAGONAL)
        else {
            calculate_matrix_matrix(ptr_matrix_Cloc, ptr_matrix_A2loc, ptr_matrix_Bloc, &rloc, &cloc);
        }
        // ------------------------------------------------- MULTIPLY END -------------------------------------------------

        // ------------------------------------------------- ROLLING START -------------------------------------------------

        // NO ROLLING NEEDED ON THE LAST PASS
        if (passo != rows_grid - 1) {
            //SEND ONE ROW AT A TIME TO AVOID DEADLOCK WITH LARGE MATRICES
            for (int r = 0; r < rloc; r++) {
                for (int id_row = 0; id_row < cols_grid; id_row++) {
                    //IF THE CURRENT PROCESSOR HAS ID_COL == THE INDEX OF THE LOOP (1,..., COLS_GRID)
                    if (proc_id_col == id_row) {
                        //SEND TO THE ONE WITH THE PREVIOUS COLUMN INDEX
                        int id_dest = ((id_row - 1) + cols_grid) % cols_grid;
                        int tag_send = 100 * proc_id_col;

                        //SEND ONE ROW AT A TIME TO AVOID DEADLOCK WITH LARGE MATRICES
                        MPI_Send((ptr_matrix_Bloc + (r * cloc)), cloc, MPI_DOUBLE, id_dest, tag_send, my_comm_col);


                        //AND RECEIVE FROM THE ONE WITH THE NEXT COLUMN INDEX
                        int id_source = (((id_row + 1) + cols_grid) % cols_grid);
                        int tag_recv = 100 * id_source;

                        MPI_Recv((ptr_matrix_Bloc + (r * cloc)), cloc, MPI_DOUBLE, id_source, tag_recv, my_comm_col, MPI_STATUS_IGNORE);
                    }
                }
            }
            // ------------------------------------------------- ROLLING END -------------------------------------------------
        }
        // ----------------------------------------------------- END BROADCAST MULTIPLY ROLLING STRATEGY ----------------------------------------------------
    }
    MPI_Barrier(MPI_COMM_WORLD);
    t1 = MPI_Wtime();
    time_diff = t1 - t0;
    MPI_Reduce(&time_diff, &time_tot, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    // ----------------------------------------------------- START RESULT  ----------------------------------------------------

    print_matrix_synch(&curr_id_proc, ptr_matrix_Cloc, &num_total_proc, &rloc, &cloc, 'C');


    double *block_columns = (double *) calloc(rows_matrix * cloc, sizeof(double));
    //EACH PROCESSOR CONCATENATES ITS MATRIX C_LOC TO THAT OF THE OTHERS USING COMM_COL
    MPI_Allgather(ptr_matrix_Cloc, rloc * cloc, MPI_DOUBLE, block_columns, rloc * cloc, MPI_DOUBLE, my_comm_col);


    MPI_Datatype gather_block;
    //EACH PROCESSOR DEFINES THE GATHER_BLOCKS TYPE, I.E., A VECTOR CONTAINING count BLOCKS OF LENGTH cloc AND DISTANT EACH columns_matrix
    MPI_Type_vector(rows_matrix, cloc, columns_matrix, MPI_DOUBLE, &gather_block); //rows_matrix or columns_matrix
    MPI_Type_commit(&gather_block);

    MPI_Datatype resized_gather_block;
    //EACH PROCESSOR DEFINES THE RESIZED_GATHER_BLOCK TYPE TAKEN FROM GATHER_BLOCK
    MPI_Type_create_resized(gather_block, 0, (long) (cloc * sizeof(double)), &resized_gather_block); //Define the size for the display of allgatherv
    MPI_Type_commit(&resized_gather_block);


    int *send_counts = (int *) calloc(rows_grid, sizeof(int));
    int *displacements = (int *) calloc(rows_grid, sizeof(int));

    displacements[0] = 0; //THE FIRST ROW PROCESSOR WILL PLACE ITS BLOCK_COLUMNS STARTING FROM INDEX 0
    send_counts[0] = 1;   //EXACTLY 1 BLOCK_COLUMNS

    for (int id_row = 1; id_row < rows_grid; id_row++) {
        send_counts[id_row] = 1; //EACH ROW PROCESSOR WILL SEND EXACTLY 1 BLOCK_COLUMNS
        // EACH PROCESSOR WITH ROW ID_ROW WILL SAY TO PLACE THE RECEIVED BLOCK_COLUMNS FROM THE POSITION
        // OF THE PREVIOUS ONE + THE SIZE OF THE PREVIOUS BLOCK
        displacements[id_row] = displacements[id_row - 1] + send_counts[id_row - 1];
    }
    //EACH PROCESSOR WILL SEND ITS OWN BLOCK_COLUMNS AND WILL RECEIVE IN PTR_MATRIX_C THOSE OF THE OTHER PROCESSORS CORRECTLY
    MPI_Allgatherv(block_columns, rows_matrix * cloc, MPI_DOUBLE, ptr_matrix_C, send_counts, displacements, resized_gather_block, my_comm_row);
    //IT WILL BE THE ALLGATHERV FUNCTION TO MANAGE THE INDICES OF THE MATRIX PTR_MATRIX_C AND WILL TAKE CARE TO INSERT THE COLUMNS BLOCKS
    //AND DISTRIBUTE THEM CORRECTLY AMONG ALL THE PROCESSORS


    // ALL PROCESS DEALLOCATION
    MPI_Type_free(&gather_block);
    MPI_Type_free(&resized_gather_block);

    // ----------------------------------------------------- END RESULT  ----------------------------------------------------

    // if(curr_id_proc == 0){
    //     printf("\n[#] C of P%d: \n", curr_id_proc);
    //     fflush(stdout);
    //     print_matrix(ptr_matrix_C, &rows_matrix, &columns_matrix);
    // }

    print_matrix_synch(&curr_id_proc, ptr_matrix_C, &num_total_proc, &rows_matrix, &columns_matrix, 'C');

    if (curr_id_proc == 0) {
        printf("\nTotal time : %f\n", time_tot);
        fflush(stdout);
    }

    MPI_Finalize();

    if (curr_id_proc == 0) {
        free(ptr_matrix_A);
        free(ptr_matrix_B);
        free(matrix_sizes);
        free(sub_matrix_sizes);
        free(starts);
    }
    free(ptr_matrix_Aloc);
    free(ptr_matrix_A2loc);
    free(ptr_matrix_Bloc);
    free(ptr_matrix_C);
    free(ptr_matrix_Cloc);
    free(vec_coordinate);
    free(vec_dim);
    free(vec_period);
    free(vec_row_col);
    free(block_columns);
    free(send_counts);
    free(displacements);

    return 0;
}

/* ****************************************************************************************************************** */

void print_matrix(const double *Matrix, const int *rows, const int *columns) {
    for (int i = 0; i < *rows; i++) {
        fflush(stdout);
        for (int j = 0; j < *columns; j++) {
            printf("\t%f", *(Matrix + (i * (*columns) + j)));
        }
        printf("\n");
        fflush(stdout);
    }
}

void print_matrix_synch(const int *curr_id_proc, double *ptr_matrix_loc, const int *num_total_proc, int *rloc, int *cloc, char mat) {
    int rank = 0;
    while (rank < *num_total_proc) {
        if (*curr_id_proc == rank) {
            printf("\n[#] %c Matrix of P%d: \n", mat, *curr_id_proc);
            print_matrix(ptr_matrix_loc, rloc, cloc);
            fflush(stdout);
        }
        rank++;
        MPI_Barrier(MPI_COMM_WORLD);
    }
}

/* ****************************************************************************************************************** */
double *generate_random_numbers(double *punt_vec, const int num, int seed) {
    double MIN = 1.0;
    double MAX = 10.0;

    srand(seed);

    for (int i = 0; i < (num); i++) {
        *(punt_vec + i) = (rand() / (RAND_MAX / (MAX - MIN)));
    }
    return punt_vec;
}


/* ****************************************************************************************************************** */
double *calculate_sub_matrix(const double *ptr_matrix, double *ptr_matrix_loc, const int *columns, const int *rloc, const int *cloc) {
    for (int i = 0; i < *rloc; i++) {
        for (int j = 0; j < *cloc; j++) {
            *(ptr_matrix_loc + (i * (*cloc) + j)) = *(ptr_matrix + (i * (*columns) + j));
        }
    }
    return ptr_matrix_loc;
}

void update_starts_sub_matrix(int *coordinate, const int *columns_matrix, int *id, const int *rloc, const int *cloc) {
    // IF THE NEXT SUB-MATRIX EXCEEDS THE COLUMN,
    // THEN THE NEXT SUB-MATRIX STARTS FROM (CURRENT ROW + RLOC, 0)
    if (*(coordinate + 1) + *cloc >= *columns_matrix) {
        *(coordinate) = *(coordinate) + *rloc; // Row
        *(coordinate + 1) = 0; // Column
    } else {
        // IF THE NEXT SUB-MATRIX DOES NOT EXCEED THE COLUMN, IT STARTS FROM (CURRENT ROW, CURRENT COLUMN + CLOC)
        *(coordinate + 1) = *(coordinate + 1) + *cloc; // Column
    }
}

/* ****************************************************************************************************************** */
void init_my_comm_grid(int *vec_dim, int *vec_period, int *reorder, const int *rows_grid, const int *cols_grid) {

    *(vec_dim) = *rows_grid;
    *(vec_dim + 1) = *cols_grid;

    // TO AVOID REORDERING OF PROCESSORS
    *reorder = 0;

    // TO AVOID PERIODICITY, OTHERWISE IT WOULD BE A TORUS
    *(vec_period) = *(vec_period + 1) = 0;
}

void create_grid(MPI_Comm *my_comm_grid, const int *dim, int **vec_dim, int *id_proc_grid, int *vec_coordinate, int **vec_period, const int *reorder) {
    // EACH PROCESS DEFINES THE BIDIMENSIONAL GRID
    MPI_Cart_create(MPI_COMM_WORLD, *dim, *vec_dim, *vec_period, *reorder, my_comm_grid);
    // ASSIGN AN IDENTIFIER TO EACH PROCESSOR IN THE NEW COMMUNICATOR
    MPI_Comm_rank(*my_comm_grid, id_proc_grid);
    // EACH PROCESS CALCULATES ITS OWN VEC_COORDINATE IN THE VEC_COORDINATE VECTOR
    MPI_Cart_coords(*my_comm_grid, *id_proc_grid, *dim, vec_coordinate);
}

void create_subgrid_row(const MPI_Comm *my_comm_grid, MPI_Comm *my_comm_row, int **vec_row_col, int *new_id) {
    // NEW COMMUNICATOR CONTAINING A SUB-GROUP OF PROCESSORS IN A SUB-GRID WITH DIM - 1, WHICH IS 1
    // EACH PROCESSOR IN EACH SUB-GROUP WILL COMMUNICATE ONLY WITH THOSE WITHIN THEIR GROUP (ROW)

    *(*vec_row_col) = 0; // FALSE: DELETE CONNECTIONS BETWEEN DIFFERENT ROWS
    *((*vec_row_col) + 1) = 1;

    MPI_Cart_sub(*my_comm_grid, *vec_row_col, my_comm_row);

    // ASSIGN A NEW IDENTIFIER TO EACH PROCESSOR IN THE NEW COMMUNICATOR
    MPI_Comm_rank(*my_comm_row, new_id);
}

void create_subgrid_col(const MPI_Comm *my_comm_grid, MPI_Comm *my_comm_col, int **vec_row_col, int *new_id) {
    // NEW COMMUNICATOR CONTAINING A SUB-GROUP OF PROCESSORS IN A SUB-GRID WITH DIM - 1, WHICH IS 1
    // EACH PROCESSOR IN EACH SUB-GROUP WILL COMMUNICATE ONLY WITH THOSE WITHIN THEIR GROUP (COLUMN)

    *(*vec_row_col) = 1;
    *((*vec_row_col) + 1) = 0; // FALSE: DELETE CONNECTIONS BETWEEN DIFFERENT COLUMNS

    MPI_Cart_sub(*my_comm_grid, *vec_row_col, my_comm_col);

    // ASSIGN A NEW IDENTIFIER TO EACH PROCESSOR IN THE NEW COMMUNICATOR
    MPI_Comm_rank(*my_comm_col, new_id);
}

void calculate_matrix_matrix(double *ptr_matrix_Cloc, const double *ptr_matrix_Aloc, const double *ptr_matrix_Bloc, const int *rloc, const int *cloc) {
    int ca, rb, cb, cc;
    int ra = 0;
    int rc = 0;

    while (rc < (*rloc)) {
        ca = 0;
        cc = 0;
        rb = 0;
        cb = 0;
        while (cc < (*cloc)) {
            *(ptr_matrix_Cloc + (rc * (*cloc) + cc)) +=
                    (*(ptr_matrix_Aloc + (ra * (*cloc) + ca))) * (*(ptr_matrix_Bloc + (rb * (*cloc) + cb)));

            ca += 1;
            rb += 1;

            // CHECK IF FINISHED CALCULATING THE ELEMENT OF MATRIX C[RC][CC]
            if ((ca >= (*cloc)) && (rb >= (*rloc))) {
                cc += 1;
            }
            // CHECK IF EXCEEDED THE COLUMNS OF MATRIX A AND CONTINUE THE CALCULATION
            if ((ca >= (*cloc)) && (cc < (*cloc))) {
                ca = 0;
            }

            // CHECK IF EXCEEDED THE ROWS OF MATRIX B AND CONTINUE THE CALCULATION
            if ((rb >= (*rloc)) && (rc < (*rloc))) {
                rb = 0;
                cb += 1;
            }
        }
        ra += 1;
        rc += 1;
    }
}

