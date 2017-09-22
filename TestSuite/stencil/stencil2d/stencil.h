#include <stdio.h>
#include <stdlib.h>
#include "support.h"

#define CACHELINE_SIZE 64
#define NUM_ACC_TASK 4
//Define input sizes
#define col_size 64
#define row_size 128
#define f_size 9

#define image_col_size 128
#define image_row_size 256

//Data Bounds
#define TYPE int32_t
#define MAX 1000
#define MIN 1

#define SECOND_ORIG row_size*col_size
#define ORIG_SIZE row_size*col_size*sizeof(TYPE)

#define SECOND_SOL row_size*col_size
#define SOL_SIZE row_size*col_size*sizeof(TYPE)

#define SECOND_FILTER f_size
#define FILTER_SIZE f_size*sizeof(TYPE)


////////////////////////////////////////////////////////////////////////////////
// Test harness interface code.

struct bench_args_t {
    TYPE orig[row_size*col_size];
    TYPE filter[f_size];
    TYPE sol[row_size*col_size];
};

#define INPUT_SIZE (sizeof(struct bench_args_t)*NUM_ACC_TASK)
#define ACC_TASK_SIZE (sizeof(struct bench_args_t))

/* ACC scratchpad */
struct spad_t {
    TYPE orig[col_size*row_size * 2];
    TYPE filter[f_size * 2];
    TYPE sol[col_size*row_size * 2];
};
////////////////////


volatile int finish_flag;
volatile int enable[NUM_ACC_TASK+1];
volatile int avail[2];

void stencil(TYPE *orig, TYPE *filter, TYPE *sol, struct bench_args_t args[NUM_ACC_TASK], 
            volatile int enable[NUM_ACC_TASK+1], volatile int avail[2]);
