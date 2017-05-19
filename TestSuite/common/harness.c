#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <assert.h>

#define WRITE_OUTPUT 
//#define CHECK_OUTPUT 

#include "support.h"
#define CACHELINE_SIZE 64

int main(int argc, char **argv)
{
  #ifdef CHECK_OUTPUT
  char *check_file;
  check_file = "check.data";
  #endif

  //data = malloc(INPUT_SIZE);
  //posix_memalign((void**)&data, CACHELINE_SIZE, INPUT_SIZE * NUM_ACC_TASK);
  //assert( data!=NULL && "Out of memory" );
  
  // Unpack and call
  char *data = run_benchmark();

  #ifdef WRITE_OUTPUT
  int out_fd;
  out_fd = open("output.data", O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
  assert( out_fd>0 && "Couldn't open output data file" );
  data_to_output(out_fd, data);
  close(out_fd);
  #endif

  // Load check data
  #ifdef CHECK_OUTPUT
  int check_fd;
  char *ref;
  ref = malloc(INPUT_SIZE);
  assert( ref!=NULL && "Out of memory" );
  check_fd = open( check_file, O_RDONLY );
  assert( check_fd>0 && "Couldn't open check data file");
  output_to_data(check_fd, ref);
  #endif

  // Validate benchmark results
  #ifdef CHECK_OUTPUT
  if( !check_data(data, ref) ) {
    fprintf(stderr, "Benchmark results are incorrect\n");
    return -1;
  }
  #endif

  printf("Success.\n");
  return 0;
}
