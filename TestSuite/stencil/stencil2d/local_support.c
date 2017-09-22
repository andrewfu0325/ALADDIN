#include "stencil.h"
#include <assert.h>
#include <fcntl.h>
#include <string.h>

#ifdef GEM5_HARNESS
#include "gem5/gem5_harness.h"
#include "gem5/aladdin_sys_constants.h"
#endif


#define EPSILON (1.0e-6)

void input_to_data(int fd, void *vdata);
void output_to_data(int fd, void *vdata);
void data_to_output(int fd, void *vdata);
int check_data( void *vdata, void *vref );

char *run_benchmark() {
  // Parse command line.
/*  char *in_file;
  in_file = "input.data";
  // Load input data
  int in_fd;
  in_fd = open( in_file, O_RDONLY );
  assert( in_fd>0 && "Couldn't open input data file");*/

  void *vargs;
  posix_memalign((void**)&vargs, CACHELINE_SIZE, INPUT_SIZE);
  assert(vargs!=NULL && "Out of memory" );

  struct bench_args_t *args = (struct bench_args_t *)vargs;
  TYPE *image = (TYPE*)(malloc)(image_row_size*image_col_size*sizeof(TYPE));
  struct spad_t spad;

  int orig_num = sizeof(args[0].orig) / sizeof(TYPE);
  int sol_num = sizeof(args[0].sol) / sizeof(TYPE);
  int filter_num = sizeof(args[0].filter) / sizeof(TYPE);

#ifdef GEM5_HARNESS
// Construct a mapping between Acc scratchpad and CPU memory buffer
// This is used for trace address to sim-virtual address translation
  mapArrayToAccelerator(
      MACHSUITE_STENCIL_2D, "avail", (void*)avail, sizeof(avail));
  mapArrayToAccelerator( 
      MACHSUITE_STENCIL_2D, "enable", (void*)enable, sizeof(enable));
  mapArrayToAccelerator(
      MACHSUITE_STENCIL_2D, "orig", (void*)args[0].orig, sizeof(struct bench_args_t) * NUM_ACC_TASK);
  mapArrayToAccelerator(
      MACHSUITE_STENCIL_2D, "filter", (void*)args[0].filter, sizeof(struct bench_args_t) * NUM_ACC_TASK);
  mapArrayToAccelerator(
      MACHSUITE_STENCIL_2D, "sol", (void*)args[0].sol, sizeof(struct bench_args_t) * NUM_ACC_TASK);
#endif

  /********* Zero-out *********/
  // "it" starts from "1" instead of "0" is in order to generate getElementPtr in llvm trace
  for(int j = 0; j < image_row_size; j++) {
    for(int i = 0; i < image_col_size; i++) {
      image[j * image_col_size + i] = i + j;
    }
  }
  for(int it = 1; it < NUM_ACC_TASK + 1; it++) {
    printf("Zero-out Task %d\n", it - 1);
    for(int i = 0; i < orig_num; i++) 
      args[it - 1].orig[i] = 0;
    for(int i = 0; i < sol_num; i++) 
      args[it - 1].sol[i] = 0;
    for(int i = 0; i < filter_num; i++) 
      args[it - 1].filter[i] = i;
  }
  /////////////////////////////

  int it = 1;
#ifdef GEM5_HARNESS
  #ifdef CACHE_FORWARDING
	regAccTaskDataForCache(MACHSUITE_STENCIL_2D, &args[it - 1], sizeof(args[0].orig) + sizeof(args[0].filter));
  #endif
#endif
	printf("Initialize Task %d\n", it - 1);
	int off = ((it - 1) % 2) * col_size + ((it - 1) / 2) * row_size *  col_size;
	for(int j = 0; j < row_size; j++) {
		for(int i = 0; i < col_size; i++) {
			args[it - 1].orig[j * col_size + i] = image[j * image_col_size + i + off];
		}
	}
	enable[it] = 1;
#ifdef GEM5_HARNESS
  avail[0] = 0;
  avail[1] = 2;
  finish_flag = NOT_COMPLETED;
  invokeAcceleratorAndReturn(MACHSUITE_STENCIL_2D, &finish_flag);
#endif
  

  for(int it = 2; it < NUM_ACC_TASK + 1; it++) {
#ifdef GEM5_HARNESS
  #ifdef CACHE_FORWARDING
    regAccTaskDataForCache(MACHSUITE_STENCIL_2D, &args[it - 1], sizeof(args[0].orig) + sizeof(args[0].filter));
    // Stop preparation if current spad is not available
    while(avail[1] == 0);
  #endif
#endif
    int off = ((it - 1) % 2) * col_size + ((it - 1) / 2) * row_size *  col_size;
		for(int j = 0; j < row_size; j++) {
			for(int i = 0; i < col_size; i++) {
				args[it - 1].orig[j * col_size + i] = image[j * image_col_size + i + off];
			}
		}
    enable[it] = 1;
  }

#ifdef GEM5_HARNESS
  while(finish_flag == NOT_COMPLETED);
  #ifdef CACHE_FORWARDING
    delAccTaskDataForCache(MACHSUITE_STENCIL_2D);
  #endif
#endif

#ifndef GEM5_HARNESS
  stencil(spad.orig, spad.filter, spad.sol, args, enable, avail);
#endif

  return vargs;
}


/* Input format:
%% Section 1
TYPE[row_size*col_size]: input matrix
%% Section 2
TYPE[f_size]: filter coefficients
#ifdef GEM5_HARNESS
  m5_work_begin(0,0);
#endif
#ifdef GEM5_HARNESS
  m5_work_end(0,0);
#endif
*/

void input_to_data(int fd, void *vdata) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;
  char *p, *s;
  // Zero-out everything.
  memset(vdata,0,sizeof(struct bench_args_t));
  // Load input string
  p = readfile(fd);

  s = find_section_start(p,1);
  STAC(parse_,TYPE,_array)(s, data->orig, row_size*col_size);

  s = find_section_start(p,2);
  STAC(parse_,TYPE,_array)(s, data->filter, f_size);
}

void data_to_input(int fd, void *vdata) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;

  write_section_header(fd);
  STAC(write_,TYPE,_array)(fd, data->orig, row_size*col_size);

  write_section_header(fd);
  STAC(write_,TYPE,_array)(fd, data->filter, f_size);
}

/* Output format:
%% Section 1
TYPE[row_size*col_size]: solution matrix
*/

void output_to_data(int fd, void *vdata) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;
  char *p, *s;
  // Zero-out everything.
  memset(vdata,0,sizeof(struct bench_args_t));
  // Load input string
  p = readfile(fd);

  s = find_section_start(p,1);
  STAC(parse_,TYPE,_array)(s, data->sol, row_size*col_size);
}

void data_to_output(int fd, void *vdata) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;

  write_section_header(fd);
  STAC(write_,TYPE,_array)(fd, data->sol, row_size*col_size);
}

int check_data( void *vdata, void *vref ) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;
  struct bench_args_t *ref = (struct bench_args_t *)vref;
  int has_errors = 0;
  int error = 0;
  int row, col;
  TYPE diff = 0;

  for(row=0; row<row_size - 2; row++) {
    for(col=0; col<col_size - 2; col++) {
      diff = data->sol[row*col_size + col] - ref->sol[row*col_size + col];
      error = (diff<-EPSILON) || (EPSILON<diff);
      has_errors |= error;
    }
  }

  // Return true if it's correct.
  return !has_errors;
}
