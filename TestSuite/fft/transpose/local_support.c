#include "fft.h"
#include <string.h>
#include <assert.h>
#include <fcntl.h>

#ifdef GEM5_HARNESS
#include "gem5/gem5_harness.h"
#endif

#define EPSILON ((TYPE)1.0e-6)

char* run_benchmark() {
  void *vargs, *vinput;
  posix_memalign((void**)&vargs, CACHELINE_SIZE, INPUT_SIZE);
  posix_memalign((void**)&vinput, CACHELINE_SIZE, ACC_TASK_SIZE);
  assert(vargs!=NULL && "Out of memory" );
  assert(vinput!=NULL && "Out of memory" );
  struct bench_args_t *args = (struct bench_args_t *)vargs;
  struct bench_args_t *input = (struct bench_args_t *)vinput;
  struct spad_t spad;

  char *in_file;
  in_file = "input.data";
  // Load input data
  int in_fd;
  in_fd = open( in_file, O_RDONLY );
  assert( in_fd>0 && "Couldn't open input data file");
  printf("Read input\n");
  input_to_data(in_fd, input);
  printf("Read done\n");

#ifdef GEM5_HARNESS
  mapArrayToAccelerator(
      MACHSUITE_FFT_TRANSPOSE, "work_x", (void*)&args->work_x,
      INPUT_SIZE);
  mapArrayToAccelerator(
      MACHSUITE_FFT_TRANSPOSE, "work_y", (void*)&args->work_y,
      INPUT_SIZE);
  mapArrayToAccelerator(
      MACHSUITE_FFT_TRANSPOSE, "enable", (void*)&enable, sizeof(enable));
  mapArrayToAccelerator(
      MACHSUITE_FFT_TRANSPOSE, "avail", (void*)&avail, sizeof(avail));
#endif
  /********* Zero-out *********/
  // "it" starts from "1" instead of "0" is in order to generate getElementPtr in llvm trace
  for(int it = 1; it < NUM_ACC_TASK + 1; it++) {
    for(int i = 0; i < FFT_SIZE; i ++) {
      args[it - 1].work_x[i] = 0;
      args[it - 1].work_y[i] = 0;
    }
  }
#ifdef GEM5_HARNESS
  m5_work_begin(0,0);
#endif
  int it = 1;
#ifdef GEM5_HARNESS
  #ifdef CACHE_FORWARDING
  regAccTaskDataForCache(MACHSUITE_FFT_TRANSPOSE, &args[it - 1], ACC_TASK_SIZE);
  #endif
#endif
  printf("Initialize Task %d\n", it - 1);
  for(int i = 0; i < FFT_SIZE; i ++) {
    args[it - 1].work_x[i] = input->work_x[i];
    args[it - 1].work_y[i] = input->work_y[i];
  }
#ifdef GEM5_HARNESS
  m5_work_end(0,0);
#endif
  enable[it] = 1;


#ifdef GEM5_HARNESS
  avail[0] = 0;
  avail[1] = 2;
  finish_flag = NOT_COMPLETED;
  invokeAcceleratorAndReturn(MACHSUITE_FFT_TRANSPOSE, &finish_flag);
#endif
  for(int it = 2; it < NUM_ACC_TASK + 1; it++) {
  #ifdef CACHE_FORWARDING
    regAccTaskDataForCache(MACHSUITE_FFT_TRANSPOSE, &args[it - 1], ACC_TASK_SIZE);
  #endif
#ifdef GEM5_HARNESS
    // Stop preparation if current spad is not available
    while(avail[1] == 0);
#endif
    for(int i = 0; i < FFT_SIZE; i ++) {
      args[it - 1].work_x[i] = input->work_x[i];
      args[it - 1].work_y[i] = input->work_y[i];
    }
    enable[it] = 1;
  }
#ifdef GEM5_HARNESS
  while(finish_flag == NOT_COMPLETED);
  #ifdef CACHE_FORWARDING
    delAccTaskDataForCache(MACHSUITE_STENCIL_3D);
  #endif
#endif

#ifndef GEM5_HARNESS
  fft1D_512( spad.work_x, spad.work_y, args, enable, avail);
#endif

  return args;
}

/* Input format:
%% Section 1
TYPE[512]: signal (real part)
%% Section 2
TYPE[512]: signal (complex part)
*/

void input_to_data(int fd, void *vdata) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;
  char *p, *s;
  // Load input string
  p = readfile(fd);

  s = find_section_start(p,1);
  STAC(parse_,TYPE,_array)(s, data->work_x, 512);

  s = find_section_start(p,2);
  STAC(parse_,TYPE,_array)(s, data->work_y, 512);
}

void data_to_input(int fd, void *vdata) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;

  write_section_header(fd);
  STAC(write_,TYPE,_array)(fd, data->work_x, 512);

  write_section_header(fd);
  STAC(write_,TYPE,_array)(fd, data->work_y, 512);
}

/* Output format:
%% Section 1
TYPE[512]: freq (real part)
%% Section 2
TYPE[512]: freq (complex part)
*/

void output_to_data(int fd, void *vdata) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;
  char *p, *s;
  // Load input string
  p = readfile(fd);
 
  s = find_section_start(p,1);
  STAC(parse_,TYPE,_array)(s, data->work_x, 512);

  s = find_section_start(p,2);
  STAC(parse_,TYPE,_array)(s, data->work_y, 512);
}

void data_to_output(int fd, void *vdata) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;

  write_section_header(fd);
  STAC(write_,TYPE,_array)(fd, data->work_x, 512);

  write_section_header(fd);
  STAC(write_,TYPE,_array)(fd, data->work_y, 512);
}

int check_data( void *vdata, void *vref ) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;
  struct bench_args_t *ref = (struct bench_args_t *)vref;
  int has_errors = 0;
  int i;
  double real_diff, img_diff;

  for(i=0; i<512; i++) {
    real_diff = data->work_x[i] - ref->work_x[i];
    img_diff = data->work_y[i] - ref->work_y[i];
    has_errors |= (real_diff<-EPSILON) || (EPSILON<real_diff);
    //if( has_errors )
      //printf("%d (real): %f (%f)\n", i, real_diff, EPSILON);
    has_errors |= (img_diff<-EPSILON) || (EPSILON<img_diff);
    //if( has_errors )
      //printf("%d (img): %f (%f)\n", i, img_diff, EPSILON);
  }

  // Return true if it's correct.
  return !has_errors;
}
