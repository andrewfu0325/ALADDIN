/*
Implementations based on:
V. Volkov and B. Kazian. Fitting fft onto the g80 architecture. 2008.
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "support.h"

#define CACHELINE_SIZE 64
#define NUM_ACC_TASK 1
#define FFT_SIZE 512
#define WORK_SIZE (512*sizeof(double))
#define SECOND_WORK 512
#define TYPE double

typedef struct complex_t {
        TYPE x;
        TYPE y;
} complex;

#define PI 3.1415926535
#ifndef M_SQRT1_2
#define M_SQRT1_2      0.70710678118654752440f
#endif

////////////////////////////////////////////////////////////////////////////////
// Test harness interface code.

struct bench_args_t {
        TYPE work_x[FFT_SIZE];
        TYPE work_y[FFT_SIZE];
};

#define INPUT_SIZE (sizeof(struct bench_args_t)*NUM_ACC_TASK)
#define ACC_TASK_SIZE (sizeof(struct bench_args_t))

/* ACC scratchpad */
struct spad_t {
        TYPE work_x[FFT_SIZE * 2];
        TYPE work_y[FFT_SIZE * 2];
};
///////////////////

volatile int finish_flag;
volatile int avail[2];
volatile int enable[NUM_ACC_TASK+1];

void fft1D_512(TYPE work_x[FFT_SIZE*2], TYPE work_y[FFT_SIZE*2], struct bench_args_t *args, volatile int enable[NUM_ACC_TASK+1], volatile avail[2]);
