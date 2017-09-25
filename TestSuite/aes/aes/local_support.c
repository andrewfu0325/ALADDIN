#include "aes.h"
#include <assert.h>
#include <string.h>
#ifdef GEM5_HARNESS
#include "gem5/gem5_harness.h"
#endif

char *run_benchmark( ) {
  void *vargs, *vinput;
  posix_memalign((void**)&vargs, CACHELINE_SIZE, INPUT_SIZE);
  posix_memalign((void**)&vinput, CACHELINE_SIZE, INPUT_SIZE);
  assert(vargs!=NULL && "Out of memory" );

  struct bench_args_t *args = (struct bench_args_t *)vargs;
  struct bench_args_t *input = (struct bench_args_t *)vinput;
  struct spad_t spad;

  int k_num = sizeof(args[0].k) / sizeof(uint8_t);
  int buf_num = sizeof(args[0].buf) / sizeof(uint8_t);

#ifdef GEM5_HARNESS
  mapArrayToAccelerator(
      MACHSUITE_AES_AES, "k", (void*)&args[0].k, INPUT_SIZE);
  mapArrayToAccelerator(
      MACHSUITE_AES_AES, "buf", (void*)&args[0].buf, INPUT_SIZE);
  mapArrayToAccelerator(
      MACHSUITE_AES_AES, "enable", (void*)&enable, sizeof(enable));
  mapArrayToAccelerator(
      MACHSUITE_AES_AES, "avail", (void*)&avail, sizeof(avail));
#endif

  for(int it = 1; it < NUM_ACC_TASK + 1; it++) {
    printf("Zero-out Task %d\n", it - 1);
    for(int i = 0; i < k_num; i++)
      args[it - 1].k[i] = 0;
    for(int i = 0; i < buf_num; i++)
      args[it - 1].buf[i] = 0;
    for(int i = 0; i < k_num; i++)
      input[it - 1].k[i] = i;
    for(int i = 0; i < buf_num; i++)
      input[it - 1].buf[i] = i;
  }


  int it = 1;
#ifdef GEM5_HARNESS
  #ifdef CACHE_FORWARDING
  regAccTaskDataForCache(MACHSUITE_AES_AES, &args[it - 1], ACC_TASK_SIZE);
  #endif
#endif
  printf("Initialize Task %d\n", it - 1);
  memcpy(args[it-1].k, input[it-1].k, k_num);
//	for(int i = 0; i < k_num; i++)
//		args[it - 1].k[i] = input[it - 1].k[i];
  memcpy(args[it-1].buf, input[it-1].buf, buf_num);
//	for(int i = 0; i < buf_num; i++)
//		args[it - 1].buf[i] = input[it - 1].buf[i];
  enable[it] = 1;

#ifdef GEM5_HARNESS
  avail[0] = 0;
  avail[1] = 2;
  finish_flag = NOT_COMPLETED;
  invokeAcceleratorAndReturn(MACHSUITE_AES_AES, &finish_flag);
#endif



  for(int it = 2; it < NUM_ACC_TASK + 1; it++) {
#ifdef GEM5_HARNESS
  #ifdef CACHE_FORWARDING
    regAccTaskDataForCache(MACHSUITE_AES_AES, &args[it - 1], ACC_TASK_SIZE);
    // Stop preparation if current spad is not available
    while(avail[1] == 0);
  #endif
#endif
    //printf("Initialize Task %d\n", it - 1);
		for(int i = 0; i < k_num; i++)
			args[it - 1].k[i] = input[it - 1].k[i];
		for(int i = 0; i < buf_num; i++)
			args[it - 1].buf[i] = input[it - 1].buf[i];
    enable[it] = 1;
  }

#ifdef GEM5_HARNESS
  while(finish_flag == NOT_COMPLETED);
  #ifdef CACHE_FORWARDING
    delAccTaskDataForCache(MACHSUITE_STENCIL_2D);
  #endif
#endif


#ifndef GEM5_HARNESS
  aes256_encrypt_ecb( spad.ctx, spad.k, spad.buf, args, enable, avail );
#endif
  return vargs;
}

/* Input format:
%%: Section 1
uint8_t[32]: key
%%: Section 2
uint8_t[16]: input-text
*/

void input_to_data(int fd, void *vdata) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;
  char *p, *s;
  // Zero-out everything.
  memset(vdata,0,sizeof(struct bench_args_t));
  // Load input string
  p = readfile(fd);
  // Section 1: key
  s = find_section_start(p,1);
  parse_uint8_t_array(s, data->k, 32);
  // Section 2: input-text
  s = find_section_start(p,2);
  parse_uint8_t_array(s, data->buf, 16);
}

void data_to_input(int fd, void *vdata) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;
  // Section 1
  write_section_header(fd);
  write_uint8_t_array(fd, data->k, 32);
  // Section 2
  write_section_header(fd);
  write_uint8_t_array(fd, data->buf, 16);
}

/* Output format:
%% Section 1
uint8_t[16]: output-text
*/

void output_to_data(int fd, void *vdata) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;

  char *p, *s;
  // Zero-out everything.
  memset(vdata,0,sizeof(struct bench_args_t));
  // Load input string
  p = readfile(fd);
  // Section 1: output-text
  s = find_section_start(p,1);
  parse_uint8_t_array(s, data->buf, 16);
}

void data_to_output(int fd, void *vdata) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;
  // Section 1
  write_section_header(fd);
  write_uint8_t_array(fd, data->buf, 16);
}

int check_data( void *vdata, void *vref ) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;
  struct bench_args_t *ref = (struct bench_args_t *)vref;
  int has_errors = 0;

  // Exact compare encrypted output buffers
  has_errors |= memcmp(&data->buf, &ref->buf, 16*sizeof(uint8_t));

  // Return true if it's correct.
  return !has_errors;
}
