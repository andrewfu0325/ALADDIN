#include "fft.h"

#ifdef DMA_MODE
#include "gem5/dma_interface.h"
#endif

void fft(double real[FFT_SIZE * 2], double img[FFT_SIZE * 2], double real_twid[FFT_SIZE/2 * 2], double img_twid[FFT_SIZE/2 * 2], struct bench_args_t args[NUM_ACC_TASK], volatile int enable[NUM_ACC_TASK+1], volatile int avail[2]){

#ifdef DMA_INTERFACE_V4
    dmaLoad(real, args[0].real, 0, 0, REAL_SIZE, &enable[1], &avail[1]);
    dmaLoad(img, args[0].img, 0, 0, IMG_SIZE, &enable[1], NULL);
//    dmaLoad(real_twid, args[0].real_twid, 0, 0, TWID_SIZE, &enable[1], NULL);
//    dmaLoad(img_twid, args[0].img_twid, 0, 0, TWID_SIZE, &enable[1], NULL);
    if(NUM_ACC_TASK > 1) {
			dmaLoad(real, args[0].real, REAL_SIZE, ACC_TASK_SIZE, REAL_SIZE, &enable[2], &avail[1]);
			dmaLoad(img, args[0].img, IMG_SIZE, ACC_TASK_SIZE, IMG_SIZE, &enable[2], NULL);
//			dmaLoad(real_twid, args[0].real_twid, TWID_SIZE, ACC_TASK_SIZE, TWID_SIZE, &enable[2], NULL);
//			dmaLoad(img_twid, args[0].img_twid, TWID_SIZE, ACC_TASK_SIZE, TWID_SIZE, &enable[2], NULL);
    }
#endif

    for(int it = 0; it < NUM_ACC_TASK; it+=2) {
      int even, odd, span, log, rootindex;
      log = 0;
      double temp;
      outer_buf1:for(span=FFT_SIZE>>1; span; span>>=1, log++){
          inner_buf1:for(odd=span; odd<FFT_SIZE; odd++){
              odd |= span;
              even = odd ^ span;

              temp = real[even] + real[odd];
              real[odd] = real[even] - real[odd];
              real[even] = temp;

              temp = img[even] + img[odd];
              img[odd] = img[even] - img[odd];
              img[even] = temp;

              rootindex = (even<<log) & (FFT_SIZE - 1);
              if(rootindex){
                  temp = real_twid[rootindex] * real[odd] -
                      img_twid[rootindex]  * img[odd];
                  img[odd] = real_twid[rootindex]*img[odd] +
                      img_twid[rootindex]*real[odd];
                  real[odd] = temp;
              }
          }
      }
#ifdef DMA_INTERFACE_V4
      dmaStore(args[0].real, real, it * ACC_TASK_SIZE, 0, REAL_SIZE, NULL, &avail[1]);
      dmaStore(args[0].img, img, it * ACC_TASK_SIZE, 0, IMG_SIZE, NULL, NULL);
      if(it < NUM_ACC_TASK - 2) {
        dmaFence();
        dmaLoad(real, args[0].real, 0, (it+2)*ACC_TASK_SIZE, REAL_SIZE, &enable[it+2+1], &avail[1]);
        dmaLoad(img, args[0].img, 0, (it+2)*ACC_TASK_SIZE, IMG_SIZE, &enable[it+2+1], NULL);
//        dmaLoad(real_twid, args[0].real_twid, 0, (it+2)*ACC_TASK_SIZE, TWID_SIZE, &enable[it+2+1], NULL);
//        dmaLoad(img_twid, args[0].img_twid, 0, (it+2)*ACC_TASK_SIZE, TWID_SIZE, &enable[it+2+1], NULL);
      }
#endif
      if(NUM_ACC_TASK > 1) {
        int even, odd, span, log, rootindex;
        log = 0;
        double temp;
        outer_buf2:for(span=FFT_SIZE>>1; span; span>>=1, log++){
            inner_buf2:for(odd=span; odd<FFT_SIZE; odd++){
                odd |= span;
                even = odd ^ span;

                temp = real[SECOND_REAL + even] + real[SECOND_REAL + odd];
                real[SECOND_REAL + odd] = real[SECOND_REAL + even] - real[SECOND_REAL + odd];
                real[SECOND_REAL + even] = temp;

                temp = img[SECOND_IMG + even] + img[SECOND_IMG + odd];
                img[SECOND_IMG + odd] = img[SECOND_IMG + even] - img[SECOND_IMG + odd];
                img[SECOND_IMG + even] = temp;
  
                 rootindex = (even<<log) & (FFT_SIZE - 1);
               if(rootindex){
                    temp = real_twid[rootindex] * real[SECOND_REAL + odd] -
                        img_twid[rootindex]  * img[SECOND_IMG + odd];
                    img[SECOND_IMG + odd] = real_twid[rootindex]*img[SECOND_IMG + odd] +
                        img_twid[rootindex]*real[SECOND_REAL + odd];
                    real[SECOND_REAL + odd] = temp;
                }
            }
        }
#ifdef DMA_INTERFACE_V4
        dmaStore(args[0].real, real, (it+1) * ACC_TASK_SIZE, REAL_SIZE, REAL_SIZE, NULL, &avail[1]);
        dmaStore(args[0].img, img, (it+1) * ACC_TASK_SIZE, IMG_SIZE, IMG_SIZE, NULL, NULL);
        if(it < NUM_ACC_TASK - 2) {
          dmaFence();
          dmaLoad(real, args[0].real, REAL_SIZE, (it+3)*ACC_TASK_SIZE, REAL_SIZE, &enable[it+3+1], &avail[1]);
          dmaLoad(img, args[0].img, IMG_SIZE, (it+3)*ACC_TASK_SIZE, REAL_SIZE, &enable[it+3+1], NULL);
//          dmaLoad(real_twid, args[0].real_twid, TWID_SIZE, (it+3)*ACC_TASK_SIZE, TWID_SIZE, &enable[it+2+1], NULL);
//          dmaLoad(img_twid, args[0].img_twid, TWID_SIZE, (it+3)*ACC_TASK_SIZE, TWID_SIZE, &enable[it+2+1], NULL);
        }
#endif
      }
    }
}
