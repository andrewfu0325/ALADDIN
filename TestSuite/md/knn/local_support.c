#include "md.h"
#include <assert.h>
#include <fcntl.h>
#include <string.h>

#ifdef GEM5_HARNESS
#include "gem5/gem5_harness.h"
#endif

#define domainEdge 20.0
#define EPSILON ((TYPE)1.0e-6)

static inline TYPE dist_sq(TYPE x1, TYPE y1, TYPE z1, TYPE x2, TYPE y2, TYPE z2) {
  TYPE dx, dy, dz;
  dx=x2-x1;
  dy=y2-y1;
  dz=z2-z1;
  return dx*dx + dy*dy + dz*dz;
}

typedef struct {
  int index;
  TYPE dist_sq;
} neighbor_t;

int neighbor_compar(const void *v_lhs, const void *v_rhs) {
  neighbor_t lhs = *((neighbor_t *)v_lhs);
  neighbor_t rhs = *((neighbor_t *)v_rhs);
  return lhs.dist_sq==rhs.dist_sq ? 0 : ( lhs.dist_sq<rhs.dist_sq ? -1 : 1 );
}

char *run_benchmark( ) {
  void *vargs, *vinput;
  posix_memalign((void**)&vargs, CACHELINE_SIZE, INPUT_SIZE);
  posix_memalign((void**)&vinput, CACHELINE_SIZE, INPUT_SIZE);
  assert(vargs!=NULL && "Out of memory" );
  assert(vinput!=NULL && "Out of memory" );
  struct bench_args_t *args = (struct bench_args_t *)vargs;
  struct bench_args_t *input = (struct bench_args_t *)vinput;

  char *in_file;
  in_file = "input.data";
  // Load input data
  int in_fd;
  in_fd = open( in_file, O_RDONLY );
  assert( in_fd>0 && "Couldn't open input data file");
  printf("Read input\n");
  input_to_data(in_fd, args);
  printf("Read done\n");

#ifdef GEM5_HARNESS
  mapArrayToAccelerator(
      MACHSUITE_MD_KNN, "force_x", (void*)&args->force_x,
      sizeof(args->force_x));
  mapArrayToAccelerator(
      MACHSUITE_MD_KNN, "force_y", (void*)&args->force_y,
      sizeof(args->force_y));
  mapArrayToAccelerator(
      MACHSUITE_MD_KNN, "force_z", (void*)&args->force_z,
      sizeof(input->force_z));
  mapArrayToAccelerator(
      MACHSUITE_MD_KNN, "position_x", (void*)&input->position_x,
      sizeof(input->position_x));
  mapArrayToAccelerator(
      MACHSUITE_MD_KNN, "position_y", (void*)&input->position_y,
      sizeof(input->position_y));
  mapArrayToAccelerator(
      MACHSUITE_MD_KNN, "position_z", (void*)&input->position_z,
      sizeof(input->position_z));
  mapArrayToAccelerator(
      MACHSUITE_MD_KNN, "NL", (void*)&input->NL, sizeof(input->NL));
#endif

  for(int i = 0; i < nAtoms; i++) {
    input->position_x[i] = 0;
    input->position_y[i] = 0;
    input->position_z[i] = 0;
    for(int j = 0; j < maxNeighbors; j++) {
      input->NL[i * maxNeighbors + j] = 0;
    }
  }
  for(int i = 0; i < nAtoms; i++) {
    args->force_x[i] = 0;
    args->force_y[i] = 0;
    args->force_z[i] = 0;
  }


#ifdef GEM5_HARNESS
  #ifdef CACHE_FORWARDING
  regAccTaskDataForCache(MACHSUITE_MD_KNN, input, sizeof(input->position_x)*3 + sizeof(input->NL));
  #endif
#endif

#ifdef GEM5_HARNESS
  m5_work_begin(0,0);
#endif
  memcpy(input->position_x, args->position_x, sizeof(args->position_x));
  memcpy(input->position_y, args->position_y, sizeof(args->position_y));
  memcpy(input->position_z, args->position_z, sizeof(args->position_z));
  memcpy(input->NL, args->NL, sizeof(args->NL));
#ifdef GEM5_HARNESS
  m5_work_end(0,0);
#endif

#ifdef GEM5_HARNESS
  invokeAcceleratorAndBlock(MACHSUITE_MD_KNN);
#endif


/*  neighbor_t neighbor_list[nAtoms];
  const TYPE infinity = (domainEdge*domainEdge*3.)*1000;//(max length)^2 * 1000
  printf("Generate NL\n");
  for(int i = 0; i < nAtoms; i++) {
    for(int j=0; j < nAtoms; j++ ) {
      neighbor_list[j].index = j;
      if( i==j )
        neighbor_list[j].dist_sq = infinity;
      else
        neighbor_list[j].dist_sq = dist_sq(args->position_x[i], args->position_y[i], args->position_z[i], args->position_x[j], args->position_y[j], args->position_z[j]);
    }
    qsort(neighbor_list, nAtoms, sizeof(neighbor_t), neighbor_compar);
    //printf("%d:", i);
    for(int j=0; j<maxNeighbors; j++ ) {
      input->NL[i*maxNeighbors +j] = neighbor_list[j].index;
      //printf(" {%d,%lf}", neighbor_list[j].index, neighbor_list[j].dist_sq);
    }
  }
  printf("Generate DONE\n");*/

#ifndef GEM5_HARNESS
  md_kernel( args->force_x, args->force_y, args->force_z,
             input->position_x, input->position_y, input->position_z,
             input->NL );
#endif
  return vargs;
}

/* Input format:
%% Section 1
TYPE[nAtoms]: x positions
%% Section 2
TYPE[nAtoms]: y positions
%% Section 3
TYPE[nAtoms]: z positions
%% Section 4
int32_t[nAtoms*maxNeighbors]: neighbor list
*/

void input_to_data(int fd, void *vdata) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;
  char *p, *s;
  // Zero-out everything.
  memset(vdata,0,sizeof(struct bench_args_t));
  // Load input string
  p = readfile(fd);

  s = find_section_start(p,1);
  STAC(parse_,TYPE,_array)(s, data->position_x, nAtoms);

  s = find_section_start(p,2);
  STAC(parse_,TYPE,_array)(s, data->position_y, nAtoms);

  s = find_section_start(p,3);
  STAC(parse_,TYPE,_array)(s, data->position_z, nAtoms);

  s = find_section_start(p,4);
  parse_int32_t_array(s, data->NL, nAtoms*maxNeighbors);

}

void data_to_input(int fd, void *vdata) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;

  write_section_header(fd);
  STAC(write_,TYPE,_array)(fd, data->position_x, nAtoms);

  write_section_header(fd);
  STAC(write_,TYPE,_array)(fd, data->position_y, nAtoms);

  write_section_header(fd);
  STAC(write_,TYPE,_array)(fd, data->position_z, nAtoms);

  write_section_header(fd);
  write_int32_t_array(fd, data->NL, nAtoms*maxNeighbors);

}

/* Output format:
%% Section 1
TYPE[nAtoms]: new x force
%% Section 2
TYPE[nAtoms]: new y force
%% Section 3
TYPE[nAtoms]: new z force
*/

void output_to_data(int fd, void *vdata) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;
  char *p, *s;
  // Zero-out everything.
  memset(vdata,0,sizeof(struct bench_args_t));
  // Load input string
  p = readfile(fd);

  s = find_section_start(p,1);
  STAC(parse_,TYPE,_array)(s, data->force_x, nAtoms);

  s = find_section_start(p,2);
  STAC(parse_,TYPE,_array)(s, data->force_y, nAtoms);

  s = find_section_start(p,3);
  STAC(parse_,TYPE,_array)(s, data->force_z, nAtoms);

}

void data_to_output(int fd, void *vdata) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;

  write_section_header(fd);
  STAC(write_,TYPE,_array)(fd, data->force_x, nAtoms);

  write_section_header(fd);
  STAC(write_,TYPE,_array)(fd, data->force_y, nAtoms);

  write_section_header(fd);
  STAC(write_,TYPE,_array)(fd, data->force_z, nAtoms);
}

int check_data( void *vdata, void *vref ) {
  struct bench_args_t *data = (struct bench_args_t *)vdata;
  struct bench_args_t *ref = (struct bench_args_t *)vref;
  int has_errors = 0;
  int i;
  TYPE diff_x, diff_y, diff_z;

  for( i=0; i<nAtoms; i++ ) {
    diff_x = data->force_x[i] - ref->force_x[i];
    diff_y = data->force_y[i] - ref->force_y[i];
    diff_z = data->force_z[i] - ref->force_z[i];
    has_errors |= (diff_x<-EPSILON) || (EPSILON<diff_x);
    has_errors |= (diff_y<-EPSILON) || (EPSILON<diff_y);
    has_errors |= (diff_z<-EPSILON) || (EPSILON<diff_z);
  }

  // Return true if it's correct.
  return !has_errors;
}
