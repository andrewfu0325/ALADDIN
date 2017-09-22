#include <stdio.h>
#include <stdlib.h>
#include "support.h"

#define CACHELINE_SIZE 64
#define NUM_ACC_TASK 8
//Define input sizes
#define height_size 32
#define col_size 32
#define row_size 32
#define f_size 9

#define cube_height_size 64
#define cube_col_size 64
#define cube_row_size 64

//Data Bounds
#define TYPE int32_t
#define MAX 1000
#define MIN 1

#define SECOND_ORIG (height_size*row_size*col_size)
#define ORIG_SIZE (height_size*row_size*col_size*sizeof(TYPE))

#define SECOND_SOL (height_size*row_size*col_size)
#define SOL_SIZE (height_size*row_size*col_size*sizeof(TYPE))

#define SECOND_C (2)
#define C_SIZE (2*sizeof(TYPE))

//Convenience Macros
#define SIZE (height_size*row_size*col_size)

////////////////////////////////////////////////////////////////////////////////
// Test harness interface code.

struct bench_args_t {
    TYPE orig[height_size*row_size*col_size];
    TYPE C[2];
    TYPE sol[height_size*row_size*col_size];
};

#define INPUT_SIZE (sizeof(struct bench_args_t)*NUM_ACC_TASK)
#define ACC_TASK_SIZE (sizeof(struct bench_args_t))

/* ACC scratchpad */
struct spad_t {
    TYPE orig[height_size*col_size*row_size * 2];
    TYPE C[2 * 2];
    TYPE sol[height_size*col_size*row_size * 2];
};
////////////////////


volatile int finish_flag;
volatile int avail[2];
volatile int enable[NUM_ACC_TASK+1];

void stencil(TYPE *orig, TYPE *filter, TYPE *sol, struct bench_args_t args[NUM_ACC_TASK], 
             volatile int enable[NUM_ACC_TASK+1], volatile int avail[2]);
