#include <stdio.h>
#include <stdlib.h>
#include "support.h"

#define FFT_SIZE 512
#define twoPI 6.28318530717959
#define NUM_ACC_TASK 1
#define CACHELINE_SIZE 64

////////////////////////////////////////////////////////////////////////////////
// Test harness interface code.

struct bench_args_t {
        double real[FFT_SIZE];
        double img[FFT_SIZE];
        double real_twid[FFT_SIZE/2];
        double img_twid[FFT_SIZE/2];
};

#define INPUT_SIZE (NUM_ACC_TASK * sizeof(struct bench_args_t))
#define ACC_TASK_SIZE (sizeof(struct bench_args_t))

/* ACC scratchpad */
struct spad_t {
        double real[FFT_SIZE * 2];
        double img[FFT_SIZE * 2];
        double real_twid[FFT_SIZE/2 * 2];
        double img_twid[FFT_SIZE/2 * 2];
};
///////////////////

#define REAL_SIZE (FFT_SIZE*sizeof(double))
#define IMG_SIZE (FFT_SIZE*sizeof(double))
#define TWID_SIZE (FFT_SIZE*sizeof(double)/2)
#define SECOND_REAL FFT_SIZE
#define SECOND_IMG FFT_SIZE

volatile int finish_flag;
volatile int avail[2];
volatile int enable[NUM_ACC_TASK+1];

void fft(double real[FFT_SIZE * 2], double img[FFT_SIZE * 2], double real_twid[FFT_SIZE/2 * 2], double img_twid[FFT_SIZE/2 * 2], struct bench_args_t args[NUM_ACC_TASK], volatile int enable[NUM_ACC_TASK+1], volatile int avail[2]);
