/*
*   Byte-oriented AES-256 implementation.
*   All lookup tables replaced with 'on the fly' calculations.
*/
#include <stdio.h>
#include <stdlib.h>
#include "support.h"

#define CACHELINE_SIZE 64
#define NUM_ACC_TASK 8

#define SECOND_K 32
#define K_SIZE 32*sizeof(uint8_t)

#define SECOND_BUF 16
#define BUF_SIZE 16*sizeof(uint8_t)

typedef struct {
  uint8_t key[32];
  uint8_t enckey[32];
  uint8_t deckey[32];
} aes256_context;


////////////////////////////////////////////////////////////////////////////////
// Test harness interface code.

struct bench_args_t {
  uint8_t k[32];
  uint8_t buf[16];
};

#define INPUT_SIZE (sizeof(struct bench_args_t)*NUM_ACC_TASK)
#define ACC_TASK_SIZE (sizeof(struct bench_args_t))

/* ACC scratchpad */
struct spad_t {
  aes256_context ctx[1];
  uint8_t k[32*2];
  uint8_t buf[16*2];
};
///////////////////

volatile int finish_flag;
volatile int avail[2];
volatile int enable[NUM_ACC_TASK+1];


void aes256_encrypt_ecb(aes256_context ctx[1], uint8_t k[32*2], uint8_t buf[16*2], 
                        struct bench_args_t args[NUM_ACC_TASK],
                        volatile int enable[NUM_ACC_TASK+1], volatile int avail[2]);
