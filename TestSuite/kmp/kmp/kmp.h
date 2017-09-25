/*
Implementation based on http://www-igm.univ-mlv.fr/~lecroq/string/node8.html
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "support.h"

#define CACHELINE_SIZE 64
#define NUM_ACC_TASK 4


#define PATTERN_SIZE 4
#define STRING_SIZE (32411)
#define INPUT_SIZE (32411*NUM_ACC_TASK)

////////////////////////////////////////////////////////////////////////////////
// Test harness interface code.

struct bench_args_t {
  char pattern[PATTERN_SIZE];
  char input[STRING_SIZE];
  //char input[STRING_SIZE];
  int32_t n_matches[1];
};

struct spad_t {
  char pattern[PATTERN_SIZE*2];
  char input[STRING_SIZE*2];
  int32_t kmpNext[PATTERN_SIZE*2];
  int32_t n_matches[2];
};

volatile int finish_flag;
volatile int avail[2];
volatile int enable[NUM_ACC_TASK+1];

void kmp(char pattern[PATTERN_SIZE], char input[STRING_SIZE*2], int32_t kmpNext[PATTERN_SIZE], int32_t n_matches[2], char str[INPUT_SIZE], struct bench_args_t *args, volatile int enable[NUM_ACC_TASK+1], volatile int avail[2]);
