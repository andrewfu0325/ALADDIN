#include "assert.h"
#include "page.h"
#include "stdio.h"
#include "math.h"
#include "aladdin_sys_connection.h"
#include "aladdin_sys_constants.h"
#define TYPE uint32_t
#include <pthread.h>

#ifdef DMA_MODE
#include "gem5/dma_interface.h"
#endif

int test_stores(TYPE* store_vals, TYPE* store_loc, int num_vals) {
  int num_failures = 0;
  for (int i = 0; i < num_vals; i++) {
    if (store_loc[i] != store_vals[i]) {
      fprintf(stdout, "FAILED: store_loc[%d] = %d, should be %d\n",
                               i, store_loc[i], store_vals[i]);
      num_failures++;
    }
  }
  return num_failures;
}


// Read values from store_vals and copy them into store_loc.
void store_kernel(TYPE* store_vals, TYPE* store_loc, int num_vals) {
#ifdef DMA_MODE
  dmaLoad(&store_vals[0], 0, num_vals * sizeof(TYPE));
  dmaLoad(&store_loc[0], 0, num_vals * sizeof(TYPE));
#endif

  loop: for (int i = 0; i < num_vals; i++)
    store_loc[i] = store_vals[i];

#ifdef DMA_MODE
  dmaStore(&store_loc[0], 0, num_vals * sizeof(TYPE));
#endif

}

int main(int argc, char **argv) {

  int num_vals = atoi(argv[1]) * 1024 / sizeof(TYPE);

  TYPE* store_vals =  (TYPE *) malloc (sizeof(TYPE) * num_vals);
  TYPE* store_loc =  (TYPE *) malloc (sizeof(TYPE) * num_vals);


  printf("Start Preparation for %d elements\n", num_vals);
#ifdef GEM5_HARNESS
  m5_work_begin(0,0);
#endif
  for (int i = 0; i < num_vals; i++) {
    store_vals[i] = i;
    store_loc[i] = -1;
  }
#ifdef GEM5_HARNESS
  m5_work_end(0,0);
#endif
  printf("End Preparation\n");

#ifdef GEM5_HARNESS
  mapArrayToAccelerator(
      INTEGRATION_TEST, "store_vals", &(store_vals[0]), num_vals * sizeof(TYPE));
  mapArrayToAccelerator(
      INTEGRATION_TEST, "store_loc", &(store_loc[0]), num_vals * sizeof(TYPE));
  fprintf(stdout, "Invoking accelerator!\n");
  invokeAcceleratorAndBlock(INTEGRATION_TEST);
  fprintf(stdout, "Accelerator finished!\n");
#else
  store_kernel(store_vals, store_loc, num_vals);
#endif

  int num_failures = test_stores(store_vals, store_loc, num_vals);
  if (num_failures != 0) {
    fprintf(stdout, "Test failed with %d errors.", num_failures);
    return -1;
  }
  fprintf(stdout, "Test passed!\n");
  return 0;
}
