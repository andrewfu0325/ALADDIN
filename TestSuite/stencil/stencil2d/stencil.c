#include "stencil.h"

#ifdef DMA_MODE
#include "gem5/dma_interface.h"
#endif


void stencil (TYPE *orig, TYPE *filter, TYPE *sol, struct bench_args_t args[NUM_ACC_TASK], int enable[NUM_ACC_TASK+1], int avail[2]) {

#ifdef DMA_INTERFACE_V4
    dmaLoad(orig, args[0].orig, 0, 0, 
            ORIG_SIZE, &enable[1], &avail[1]);
    dmaLoad(filter, args[0].filter, 0, 0, 
            FILTER_SIZE, &enable[1], NULL);
    if(NUM_ACC_TASK > 1) {
      dmaLoad(orig, args[0].orig, ORIG_SIZE, ACC_TASK_SIZE,
              ORIG_SIZE, &enable[2], &avail[1]);
      dmaLoad(filter, args[0].filter, FILTER_SIZE, ACC_TASK_SIZE, 
              FILTER_SIZE, &enable[2], NULL);
    }
#endif
    task_loop:for (int it = 0; it < NUM_ACC_TASK; it += 2) {
        stencil_label1_buf1:for (int r=0; r<row_size-2; r++) {
            stencil_label2_buf1:for (int c=0; c<col_size-2; c++) {
                TYPE temp = (TYPE)100;
                stencil_label3_buf1:for (int k1=0;k1<3;k1++){
                    stencil_label4_buf1:for (int k2=0;k2<3;k2++){
                        TYPE mul = filter[k1*3 + k2] * orig[(r+k1)*col_size + c+k2];
                        temp += mul;
                    }
                }
                sol[(r*col_size) + c] = temp;
            }
        }
#ifdef DMA_INTERFACE_V4
        dmaStore(args[0].sol, sol, it * ACC_TASK_SIZE, 0, 
                 SOL_SIZE, NULL, &avail[1]);
        if (it < NUM_ACC_TASK - 2) { 
          dmaFence();
          dmaLoad(orig, args[0].orig, 0, (it+2)*ACC_TASK_SIZE,
                  ORIG_SIZE, &enable[it+2+1], &avail[1]);
          dmaLoad(filter, args[0].filter, 0, (it+2)*ACC_TASK_SIZE, 
                  FILTER_SIZE, &enable[it+2+1], NULL);
        }
#endif
        if(NUM_ACC_TASK > 1) {
          stencil_label1_buf2:for (int r=0; r<row_size-2; r++) {
              stencil_label2_buf2:for (int c=0; c<col_size-2; c++) {
                  TYPE temp = (TYPE)100;
                  stencil_label3_buf2:for (int k1=0;k1<3;k1++){
                      stencil_label4_buf2:for (int k2=0;k2<3;k2++){
                          TYPE mul = filter[SECOND_FILTER + k1*3 + k2] * orig[SECOND_ORIG + (r+k1)*col_size + c+k2];
                          temp += mul;
                      }
                  }
                  sol[SECOND_SOL + (r*col_size) + c] = temp;
              }
          }

#ifdef DMA_INTERFACE_V4
          dmaStore(args[0].sol, sol, (it+1)*ACC_TASK_SIZE, SOL_SIZE, 
                   SOL_SIZE, NULL, &avail[1]);
          if (it < NUM_ACC_TASK - 2) { 
            dmaFence();
            dmaLoad(orig, args[0].orig, ORIG_SIZE, (it+3)*ACC_TASK_SIZE, 
                    ORIG_SIZE, &enable[it+3+1], &avail[1]);
            dmaLoad(filter, args[0].filter, FILTER_SIZE, (it+3)*ACC_TASK_SIZE, 
                    FILTER_SIZE, &enable[it+3+1], NULL);
          }
#endif
        }
    }
}
