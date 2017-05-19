#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <assert.h>

#define WRITE_OUTPUT false
#define CHECK_OUTPUT false
#define CACHELINE_SIZE 64

#include "support.h"

int main(int argc, char **argv)
{
  // Parse command line.
  char *in_file;
  #if CHECK_OUTPUT
  char *check_file;
  #endif
  assert( argc<4 && "Usage: ./benchmark <input_file> <check_file>" );
  in_file = "input.data";
  #if CHECK_OUTPUT
  check_file = "check.data";
  #endif
  if( argc>1 )
    in_file = argv[1];
  #if CHECK_OUTPUT
  if( argc>2 )
    check_file = argv[2];
  #endif

  // Load input data
  int in_fd;
  char *data;

  data = malloc(INPUT_SIZE);
  assert( data!=NULL && "Out of memory" );
  in_fd = open( in_file, O_RDONLY );
  assert( in_fd>0 && "Couldn't open input data file");
  #ifdef GEM5_HARNESS
  m5_work_begin(0,0);
  #endif
  input_to_data(in_fd, data);
  #ifdef GEM5_HARNESS
  m5_work_end(0,0);
  #endif
  
  // Unpack and call
  run_benchmark( data );

  #if WRITE_OUTPUT
  int out_fd;
  out_fd = open("output.data", O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
  assert( out_fd>0 && "Couldn't open output data file" );
  data_to_output(out_fd, data);
  close(out_fd);
  #endif

  // Load check data
  #if CHECK_OUTPUT
  int check_fd;
  char *ref;
  ref = malloc(INPUT_SIZE);
  assert( ref!=NULL && "Out of memory" );
  check_fd = open( check_file, O_RDONLY );
  assert( check_fd>0 && "Couldn't open check data file");
  output_to_data(check_fd, ref);
  #endif

  // Validate benchmark results
  #if CHECK_OUTPUT
  if( !check_data(data, ref) ) {
    fprintf(stderr, "Benchmark results are incorrect\n");
    return -1;
  }
  #endif

  printf("Success.\n");
  return 0;
}
