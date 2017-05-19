#include "stencil.h"
#include <string.h>

#ifdef GEM5_HARNESS
#include "gem5/gem5_harness.h"
#endif

int INPUT_SIZE = sizeof(struct bench_args_t);

#define EPSILON (1.0e-6)

void run_benchmark( void *vargs ) {
  struct bench_args_t *args = (struct bench_args_t *)vargs;
#ifdef GEM5_HARNESS
  mapArrayToAccelerator(
      MACHSUITE_STENCIL_2D, "orig", (void*)&args->orig, sizeof(args->orig));
  mapArrayToAccelerator(
      MACHSUITE_STENCIL_2D, "sol", (void*)&args->sol, sizeof(args->sol));
  mapArrayToAccelerator(
      MACHSUITE_STENCIL_2D, "filter", (void*)&args->filter,
      sizeof(args->filter));
  invokeAcceleratorAndBlock(MACHSUITE_STENCIL_2D);
#else
  stencil( args->orig, args->sol, args->filter );
#endif
}

/* Input format:
%% Section 1
TYPE[row_size*col_size]: input matrix
%% Section 2
TYPE[f_size]: filter coefficients
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
