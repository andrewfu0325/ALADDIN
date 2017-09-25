/*
Implementation based on http://www-igm.univ-mlv.fr/~lecroq/string/node8.html
*/

#include "kmp.h"

#ifdef DMA_MODE
#include "gem5/dma_interface.h"
#endif

void CPF(char pattern[PATTERN_SIZE], int32_t kmpNext[PATTERN_SIZE]) {
    int32_t k, q;
    k = 0;
    kmpNext[0] = 0;

    c1 : for(q = 1; q < PATTERN_SIZE; q++){
        c2 : while(k > 0 && pattern[k] != pattern[q]){
            k = kmpNext[q];
        }
        if(pattern[k] == pattern[q]){
            k++;
        }
        kmpNext[q] = k;
    }
}


void kmp(char pattern[PATTERN_SIZE], char input[STRING_SIZE], int32_t kmpNext[PATTERN_SIZE], int32_t n_matches[2], char *str, struct bench_args_t *args, volatile int enable[NUM_ACC_TASK+1], volatile int avail[2]) {
    int32_t i, q;

    n_matches[0] = 0;
    n_matches[1] = 0;

#ifdef DMA_INTERFACE_V4
    dmaLoad(pattern, args->pattern, 0, 0,
             4, &enable[1], NULL);
    dmaLoad(input, str, 0, 0,
            STRING_SIZE, &enable[1], NULL);
    if(NUM_ACC_TASK > 1) {
      dmaLoad(input, str, STRING_SIZE, STRING_SIZE,
              STRING_SIZE, &enable[2], NULL);
    }
#endif

    CPF(pattern, kmpNext);

    for(int it = 0; it < NUM_ACC_TASK; it+=2) {
      q = 0;
      k1_buf1 : for(i = 0; i < STRING_SIZE; i++){
          k2_buf1 : while (q > 0 && pattern[q] != input[i]){
              q = kmpNext[q];
          }
          if (pattern[q] == input[i]){
              q++;
          }
          if (q >= PATTERN_SIZE){
              n_matches[1]++;
              q = kmpNext[q - 1];
          }
      }
#ifdef DMA_INTERFACE_V4
      if(it < NUM_ACC_TASK - 2) {
        dmaFence();
        dmaLoad(input, str, 0, (it+2) * STRING_SIZE,
                STRING_SIZE, &enable[it+2+1], NULL);
      }
#endif
      if(NUM_ACC_TASK > 1) {
        q = 0;
        k1_buf2 : for(i = 0; i < STRING_SIZE; i++){
            k2_buf2 : while (q > 0 && pattern[q] != input[STRING_SIZE + i]){
                q = kmpNext[q];
            }
            if (pattern[q] == input[STRING_SIZE + i]){
                q++;
            }
            if (q >= PATTERN_SIZE){
                n_matches[1]++;
                q = kmpNext[q - 1];
            }
        }
#ifdef DMA_INTERFACE_V4
        if(it < NUM_ACC_TASK - 2) {
          dmaFence();
          dmaLoad(input, str, STRING_SIZE, (it+3) * STRING_SIZE,
                  STRING_SIZE, &enable[it+3+1], NULL);
        }
#endif
      }
    }
#ifdef DMA_INTERFACE_V4
    dmaStore(args->n_matches, n_matches, 0, 4,
             4, NULL, NULL);
#endif

}
