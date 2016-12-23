#!/usr/bin/env bash

bmk_home=${ALADDIN_HOME}/integration-test/with-cpu/test_dma_load_store
gem5_dir=${ALADDIN_HOME}/../..

  ${gem5_dir}/build/X86_MOESI_CMP_directory/gem5.opt \
  --debug-flags=DMA \
  --outdir=${bmk_home}/outputs \
  ${gem5_dir}/configs/aladdin/aladdin_se.py \
  --num-cpus=1 \
  --enable_prefetchers \
  --mem-size=8GB \
  --mem-type=LPDDR3_1600_x32  \
  --sys-clock=1GHz \
  --cpu-type=detailed \
  --caches \
  --l1d_size=64kB \
  --l1i_size=32kB \
  --l2cache \
  --l2_size=2MB \
  --accel_cfg_file=${bmk_home}/gem5.cfg \
  -c ${bmk_home}/test_dma_load_store \
