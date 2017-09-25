#include "kmp.h"
#include <assert.h>
#include <fcntl.h>
#include <string.h>

#ifdef GEM5_HARNESS
#include "gem5/gem5_harness.h"
#endif

char *run_benchmark( ) {
  void *vargs;
  posix_memalign((void**)&vargs, CACHELINE_SIZE, INPUT_SIZE);
  assert(vargs!=NULL && "Out of memory" );

  char *str = (char*)malloc(INPUT_SIZE);
  struct bench_args_t *args = (struct bench_args_t *)vargs;
  struct spad_t spad;

  char *in_file;
  in_file = "input.data";
  // Load input data
  int in_fd;
  in_fd = open( in_file, O_RDONLY );
  assert( in_fd>0 && "Couldn't open input data file");
  input_to_data(in_fd, args);

#ifdef GEM5_HARNESS
  mapArrayToAccelerator(
      MACHSUITE_KMP_KMP, "pattern", (void*)args->pattern, sizeof(args->pattern));
  mapArrayToAccelerator(
      MACHSUITE_KMP_KMP, "input", (void*)str, INPUT_SIZE);
  mapArrayToAccelerator(
      MACHSUITE_KMP_KMP, "n_matches", (void*)args->n_matches, sizeof(args->n_matches));
  mapArrayToAccelerator(
      MACHSUITE_KMP_KMP, "enable", (void*)enable, sizeof(enable));
  mapArrayToAccelerator(
      MACHSUITE_KMP_KMP, "avail", (void*)avail, sizeof(avail));
#endif

  /********* Zero-out *********/
  // "it" starts from "1" instead of "0" is in order to generate getElementPtr in llvm trace
  for(int it = 1; it < NUM_ACC_TASK + 1; it++) {
    printf("Zero-out Task %d\n", it - 1);
		for(int i = 0; i < STRING_SIZE; i++) {
      str[(it-1)*STRING_SIZE + i] = 0;
    }
  }
  /////////////////////////////


  int it = 1;
#ifdef GEM5_HARNESS
  m5_work_begin(0,0);
#endif
#ifdef CACHE_FORWARDING
regAccTaskDataForCache(MACHSUITE_KMP_KMP, &str[(it - 1)*STRING_SIZE], STRING_SIZE);
#endif
  memcpy(&str[(it-1)*STRING_SIZE], args->input, STRING_SIZE);
  /*for(int i = 0; i < STRING_SIZE; i++) {
    str[(it-1)*STRING_SIZE + i] = args->input[i];
  }*/
  enable[it] = 1;
#ifdef GEM5_HARNESS
  m5_work_end(0,0);
#endif


	for(int it = 2; it < NUM_ACC_TASK + 1; it++) {
#ifdef CACHE_FORWARDING
    regAccTaskDataForCache(MACHSUITE_KMP_KMP, &str[(it - 1)*STRING_SIZE], STRING_SIZE);
#endif
    memcpy(&str[(it-1)*STRING_SIZE], args->input, STRING_SIZE);
		/*for(int i = 0; i < STRING_SIZE; i++) {
			str[(it-1)*STRING_SIZE + i] = args->input[i];
		}*/
    enable[it] = 1;
  }

#ifdef GEM5_HARNESS
  avail[0] = 0;
  avail[1] = 2;
  finish_flag = NOT_COMPLETED;
  invokeAcceleratorAndReturn(MACHSUITE_KMP_KMP, &finish_flag);
#endif

#ifdef GEM5_HARNESS
  while(finish_flag == NOT_COMPLETED);
  #ifdef CACHE_FORWARDING
    delAccTaskDataForCache(MACHSUITE_STENCIL_2D);
  #endif
#endif


#ifndef GEM5_HARNESS
//  kmp( args->pattern, args->input, spad.kmpNext, spad.n_matches, str, args);
  kmp( spad.pattern, spad.input, spad.kmpNext, spad.n_matches, str, args, enable, avail);
#endif
  return vargs;
}

/* Input format:
%% Section 1
char[PATTERN_SIZE]: pattern
%% Section 2
char[STRING_SIZE]: text
*/

void input_to_data(int fd, void *vdata) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;
  char *p, *s;
  // Zero-out everything.
  memset(vdata,0,sizeof(struct bench_args_t));
  // Load input string
  p = readfile(fd);

  s = find_section_start(p,1);
  parse_string(s, data->pattern, PATTERN_SIZE);

  s = find_section_start(p,2);
  parse_string(s, data->input, STRING_SIZE);
}

void data_to_input(int fd, void *vdata) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;

  write_section_header(fd);
  write_string(fd, data->pattern, PATTERN_SIZE);

  write_section_header(fd);
  write_string(fd, data->input, STRING_SIZE);
}

/* Output format:
%% Section 1
int[1]: number of matches
*/

void output_to_data(int fd, void *vdata) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;
  char *p, *s;
  // Zero-out everything.
  memset(vdata,0,sizeof(struct bench_args_t));
  // Load input string
  p = readfile(fd);

  s = find_section_start(p,1);
  parse_int32_t_array(s, data->n_matches, 1);
}

void data_to_output(int fd, void *vdata) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;

  write_section_header(fd); // No section header
  write_int32_t_array(fd, data->n_matches, 1);
}

int check_data( void *vdata, void *vref ) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;
  struct bench_args_t *ref = (struct bench_args_t *)vref;
  int has_errors = 0;

  has_errors |= (data->n_matches[0] != ref->n_matches[0]);

  // Return true if it's correct.
  return !has_errors;
}
