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
  TYPE *cube = (TYPE*)(malloc)(cube_height_size*cube_row_size*cube_col_size*sizeof(TYPE));
  struct spad_t spad;
  spad.C[0] = 100;
  spad.C[1] = 200;
  spad.C[2] = 100;
  spad.C[3] = 200;

  int orig_num = sizeof(args[0].orig) / sizeof(TYPE);
  int sol_num = sizeof(args[0].sol) / sizeof(TYPE);

#ifdef GEM5_HARNESS
// Construct a mapping between Acc scratchpad and CPU memory buffer
// This is used for trace address to sim-virtual address translation
  mapArrayToAccelerator(
      MACHSUITE_STENCIL_3D, "orig", (void*)&args[0].orig, sizeof(struct bench_args_t) * NUM_ACC_TASK);
  mapArrayToAccelerator(
      MACHSUITE_STENCIL_3D, "sol", (void*)&args[0].sol, sizeof(struct bench_args_t) * NUM_ACC_TASK);
  mapArrayToAccelerator( 
      MACHSUITE_STENCIL_3D, "enable", (void*)&enable, sizeof(enable));
  mapArrayToAccelerator(
      MACHSUITE_STENCIL_3D, "avail", (void*)&avail, sizeof(avail));
#endif

  /********* Zero-out *********/
  // "it" starts from "1" instead of "0" is in order to generate getElementPtr in llvm trace
  for(int h = 0; h < cube_height_size; h++) {
		for(int r = 0; r < cube_row_size; r++) {
			for(int c = 0; c < cube_col_size; c++) {
				cube[(h * cube_row_size + r) * cube_col_size + c] = h + r + c;
			}
		}
  }
  for(int it = 1; it < NUM_ACC_TASK + 1; it++) {
    printf("Zero-out Task %d\n", it - 1);
    for(int i = 0; i < orig_num; i++) 
      args[it - 1].orig[i] = 0;
    for(int i = 0; i < sol_num; i++) 
      args[it - 1].sol[i] = 0;
  }
  /////////////////////////////

  int it = 1;
#ifdef GEM5_HARNESS
  #ifdef CACHE_FORWARDING
	regAccTaskDataForCache(MACHSUITE_STENCIL_3D, &args[it - 1], sizeof(args[0].orig));
  #endif
#endif
	printf("Initialize Task %d\n", it - 1);
  int off = ((it - 1) % 2) * col_size +
            (((it - 1) % 4) / 2) * row_size * col_size +
            ((it - 1) / 4) * height_size * row_size * col_size;
  for(int h = 0; h < height_size; h++) {
		for(int r = 0; r < row_size; r++) {
			for(int c = 0; c < col_size; c++) {
				args[it - 1].orig[(c * row_size + r) * col_size + h] = 
          cube[off + (h * cube_row_size + r) * cube_col_size + c];
			}
		}
  }
	enable[it] = 1;

#ifdef GEM5_HARNESS
  avail[0] = 0;
  avail[1] = 2;
  finish_flag = NOT_COMPLETED;
  invokeAcceleratorAndReturn(MACHSUITE_STENCIL_3D, &finish_flag);
#endif
  

  for(int it = 2; it < NUM_ACC_TASK + 1; it++) {
#ifdef GEM5_HARNESS
  #ifdef CACHE_FORWARDING
    regAccTaskDataForCache(MACHSUITE_STENCIL_3D, &args[it - 1], sizeof(args[0].orig));
    // Stop preparation if current spad is not available
    while(avail[1] == 0);
  #endif
#endif
		for(int h = 0; h < height_size; h++) {
			for(int r = 0; r < row_size; r++) {
				for(int c = 0; c < col_size; c++) {
					args[it - 1].orig[(c * row_size + r) * col_size + h] = 
						cube[off + (h * cube_row_size + r) * cube_col_size + c];
				}
			}
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
  stencil(spad.orig, spad.C, spad.sol, args, enable, avail);
#endif

  return vargs;
}

/* Input format:
%% Section 1
TYPE[2]: stencil coefficients (inner/outer)
%% Section 2
TYPE[SIZE]: input matrix
*/

/*void input_to_data(int fd, void *vdata) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;
  char *p, *s;
  // Load input string
  p = readfile(fd);

  s = find_section_start(p,1);
  STAC(parse_,TYPE,_array)(s, data->C, 2);

  s = find_section_start(p,2);
  STAC(parse_,TYPE,_array)(s, data->orig, SIZE);
}

void data_to_input(int fd, void *vdata) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;

  write_section_header(fd);
  STAC(write_,TYPE,_array)(fd, data->C, 2);

  write_section_header(fd);
  STAC(write_,TYPE,_array)(fd, data->orig, SIZE);
}*/


/* Output format:
%% Section 1
TYPE[SIZE]: solution matrix
*/

void output_to_data(int fd, void *vdata) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;
  char *p, *s;
  // Load input string
  p = readfile(fd);

  s = find_section_start(p,1);
  STAC(parse_,TYPE,_array)(s, data->sol, SIZE);
}

void data_to_output(int fd, void *vdata) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;

  write_section_header(fd);
  STAC(write_,TYPE,_array)(fd, data->sol, SIZE);
}

int check_data( void *vdata, void *vref ) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;
  struct bench_args_t *ref = (struct bench_args_t *)vref;
  int has_errors = 0;
  int i;
  TYPE diff;

  for(i=0; i<SIZE; i++) {
    diff = data->sol[i] - ref->sol[i];
    has_errors |= (diff<-EPSILON) || (EPSILON<diff);
  }

  // Return true if it's correct.
  return !has_errors;
}

