#include "fft.h"
#include <string.h>
#include <assert.h>
#include <fcntl.h>


#ifdef GEM5_HARNESS
#include "gem5/gem5_harness.h"
#endif

#define EPSILON ((double)1.0e-6)

char *run_benchmark( ) {
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
      MACHSUITE_FFT_STRIDED, "real", (void*)args[0].real, INPUT_SIZE);
  mapArrayToAccelerator(
      MACHSUITE_FFT_STRIDED, "img", (void*)args[0].img, INPUT_SIZE);
  mapArrayToAccelerator(
      MACHSUITE_FFT_STRIDED, "real_twid", (void*)args[0].real_twid,
                                          INPUT_SIZE);
  mapArrayToAccelerator(
      MACHSUITE_FFT_STRIDED, "img_twid", (void*)args[0].img_twid,
                                          INPUT_SIZE);
  mapArrayToAccelerator(
      MACHSUITE_FFT_STRIDED, "enable", (void*)&enable, sizeof(enable));
  mapArrayToAccelerator(
      MACHSUITE_FFT_STRIDED, "avail", (void*)&avail, sizeof(avail));
#endif
  /********* Zero-out *********/
  // "it" starts from "1" instead of "0" is in order to generate getElementPtr in llvm trace
  for(int it = 1; it < NUM_ACC_TASK + 1; it++) {
    for(int i = 0; i < FFT_SIZE; i ++) {
      args[it - 1].real[i] = 0;
      args[it - 1].img[i] = 0;
    }
    for(int i = 0; i < FFT_SIZE/2; i ++) {
      spad.real_twid[i] = input->real_twid[i];
      spad.img_twid[i] = input->img_twid[i];
    }
  }


#ifdef GEM5_HARNESS
  m5_work_begin(0,0);
#endif
  int it = 1;
#ifdef GEM5_HARNESS
  #ifdef CACHE_FORWARDING
  regAccTaskDataForCache(MACHSUITE_FFT_STRIDED, &args[it - 1], ACC_TASK_SIZE);
  printf("Registration\n");
  #endif
#endif
  for(int i = 0; i < FFT_SIZE; i ++) {
    args[it - 1].real[i] = input->real[i];
    args[it - 1].img[i] = input->img[i];
  }
  enable[it] = 1;
#ifdef GEM5_HARNESS
  m5_work_end(0,0);
#endif

#ifdef GEM5_HARNESS
  avail[0] = 0;
  avail[1] = 2;
  finish_flag = NOT_COMPLETED;
  invokeAcceleratorAndReturn(MACHSUITE_FFT_STRIDED, &finish_flag);
#endif

  for(int it = 2; it < NUM_ACC_TASK + 1; it++) {
  #ifdef CACHE_FORWARDING
    regAccTaskDataForCache(MACHSUITE_FFT_STRIDED, &args[it - 1], ACC_TASK_SIZE);
  #endif
#ifdef GEM5_HARNESS
    // Stop preparation if current spad is not available
    while(avail[1] == 0);
#endif
		for(int i = 0; i < FFT_SIZE; i ++) {
			args[it - 1].real[i] = input->real[i];
			args[it - 1].img[i] = input->img[i];
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
  fft(spad.real, spad.img, spad.real_twid, spad.img_twid, args, enable ,avail );
#endif
  return vargs;
}

/* Input format:
%% Section 1
double: signal (real part)
%% Section 2
double: signal (complex part)
%% Section 3
double: twiddle factor (real part)
%% Section 4
double: twiddle factor (complex part)
*/

void input_to_data(int fd, void *vdata) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;
  char *p, *s;
  // Load input string
  p = readfile(fd);

  s = find_section_start(p,1);
  parse_double_array(s, data->real, FFT_SIZE);

  s = find_section_start(p,2);
  parse_double_array(s, data->img, FFT_SIZE);

  s = find_section_start(p,3);
  parse_double_array(s, data->real_twid, FFT_SIZE/2);

  s = find_section_start(p,4);
  parse_double_array(s, data->img_twid, FFT_SIZE/2);
}

void data_to_input(int fd, void *vdata) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;

  write_section_header(fd);
  write_double_array(fd, data->real, FFT_SIZE);

  write_section_header(fd);
  write_double_array(fd, data->img, FFT_SIZE);

  write_section_header(fd);
  write_double_array(fd, data->real_twid, FFT_SIZE/2);

  write_section_header(fd);
  write_double_array(fd, data->img_twid, FFT_SIZE/2);
}

/* Output format:
%% Section 1
double: freq (real part)
%% Section 2
double: freq (complex part)
*/

void output_to_data(int fd, void *vdata) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;
  char *p, *s;
  // Zero-out everything.
  memset(vdata,0,sizeof(struct bench_args_t));
  // Load input string
  p = readfile(fd);

  s = find_section_start(p,1);
  parse_double_array(s, data->real, FFT_SIZE);

  s = find_section_start(p,2);
  parse_double_array(s, data->img, FFT_SIZE);
}

void data_to_output(int fd, void *vdata) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;

  write_section_header(fd);
  write_double_array(fd, data->real, FFT_SIZE);

  write_section_header(fd);
  write_double_array(fd, data->img, FFT_SIZE);
}

int check_data( void *vdata, void *vref ) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;
  struct bench_args_t *ref = (struct bench_args_t *)vref;
  int has_errors = 0;
  int i;
  double real_diff, img_diff;

  for(i=0; i<FFT_SIZE; i++) {
    real_diff = data->real[i] - ref->real[i];
    img_diff = data->img[i] - ref->img[i];
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
