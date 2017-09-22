#include "stencil.h"

#ifdef DMA_MODE
#include "gem5/dma_interface.h"
#endif


void stencil (TYPE *orig, TYPE *C, TYPE *sol, struct bench_args_t args[NUM_ACC_TASK], volatile int enable[NUM_ACC_TASK+1], volatile int avail[2]) {

#ifdef DMA_INTERFACE_V4
    dmaLoad(orig, args[0].orig, 0, 0, 
            ORIG_SIZE, &enable[1], &avail[1]);
    if(NUM_ACC_TASK > 1) {
      dmaLoad(orig, args[0].orig, ORIG_SIZE, ACC_TASK_SIZE,
              ORIG_SIZE, &enable[2], &avail[1]);
    }
#endif
    task_loop:for (int it = 0; it < NUM_ACC_TASK; it += 2) {
        loop_height_buf1:for (int h=1; h<height_size-1; h++) {
          loop_row_buf1:for (int r=1; r<row_size-1; r++) {
            loop_col_buf1:for (int c=1; c<col_size-1; c++) {
                TYPE sum0, sum1, mul0, mul1;
                sum0 = orig[(c)+col_size*(r+h*row_size)];
                sum1 = orig[(c)+col_size*(r+(h+1)*row_size)] +
                       orig[(c)+col_size*(r+(h-1)*row_size)] +
                       orig[(c)+col_size*((r+1)+h*row_size)] +
                       orig[(c)+col_size*((r-1)+h*row_size)] +
                       orig[(c+1)+col_size*((r)+h*row_size)] +
                       orig[(c-1)+col_size*((r)+h*row_size)];
                mul0 = sum0 * C[0];
                mul1 = sum1 * C[1];
                sol[c+col_size*(r+h*row_size)] = mul0 + mul1;
            }
          }
        }
#ifdef DMA_INTERFACE_V4
        dmaStore(args[0].sol, sol, it * ACC_TASK_SIZE, 0, 
                 SOL_SIZE, NULL, &avail[1]);
        if (it < NUM_ACC_TASK - 2) { 
          dmaFence();
          dmaLoad(orig, args[0].orig, 0, (it+2)*ACC_TASK_SIZE,
                  ORIG_SIZE, &enable[it+2+1], &avail[1]);
        }
#endif
        if(NUM_ACC_TASK > 1) {
          loop_height_buf2:for (int h=1; h<height_size-1; h++) {
            loop_row_buf2:for (int r=1; r<row_size-1; r++) {
              loop_col_buf2:for (int c=1; c<col_size-1; c++) {
                  TYPE sum0, sum1, mul0, mul1;
                  sum0 = orig[SECOND_ORIG+(c)+col_size*(r+h*row_size)];
                  sum1 = orig[SECOND_ORIG+(c)+col_size*(r+(h+1)*row_size)] +
                         orig[SECOND_ORIG+(c)+col_size*(r+(h-1)*row_size)] +
                         orig[SECOND_ORIG+(c)+col_size*((r+1)+h*row_size)] +
                         orig[SECOND_ORIG+(c)+col_size*((r-1)+h*row_size)] +
                         orig[SECOND_ORIG+(c+1)+col_size*((r)+h*row_size)] +
                         orig[SECOND_ORIG+(c-1)+col_size*((r)+h*row_size)];
                  mul0 = sum0 * C[SECOND_C+0];
                  mul1 = sum1 * C[SECOND_C+1];
                  sol[SECOND_SOL+c+col_size*(r+h*row_size)] = mul0 + mul1;
              }
            }
          }


#ifdef DMA_INTERFACE_V4
          dmaStore(args[0].sol, sol, (it+1)*ACC_TASK_SIZE, SOL_SIZE, 
                   SOL_SIZE, NULL, &avail[1]);
          if (it < NUM_ACC_TASK - 2) { 
            dmaFence();
            dmaLoad(orig, args[0].orig, ORIG_SIZE, (it+3)*ACC_TASK_SIZE, 
                    ORIG_SIZE, &enable[it+3+1], &avail[1]);
          }
#endif
        }
    }
}
