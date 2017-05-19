#include "stencil.h"

#ifdef DMA_MODE
#include "gem5/dma_interface.h"
#endif

void stencil (TYPE orig[row_size * col_size], TYPE sol[row_size * col_size], TYPE filter[f_size]){

#ifdef DMA_MODE
    dmaLoad(&orig[0], 0, PAGE_SIZE * 8);
    dmaLoad(&filter[0], 0, f_size * sizeof(TYPE));
#endif

    stencil_label1:for (int r=0; r<row_size-2; r++) {
        stencil_label2:for (int c=0; c<col_size-2; c++) {
            TYPE temp = (TYPE)0;
            stencil_label3:for (int k1=0;k1<3;k1++){
                stencil_label4:for (int k2=0;k2<3;k2++){
                    TYPE mul = filter[k1*3 + k2] * orig[(r+k1)*col_size + c+k2];
                    temp += mul;
                }
            }
            sol[(r*col_size) + c] = temp;
        }
    }

#ifdef DMA_MODE
    dmaStore(&sol[0], 0, PAGE_SIZE * 8);
#endif
}
