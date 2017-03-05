#!/usr/bin/env bash

bmk_home=${ALADDIN_HOME}/integration-test/with-cpu/test_dma_load_store
gem5_dir=${ALADDIN_HOME}/../..

  ${gem5_dir}/build/X86_MOESI_CMP_directory/gem5.opt \
  --outdir=${bmk_home}/outputs \
  ${gem5_dir}/configs/aladdin/aladdin_se.py \
  --num-cpus=1 \
  --enable_prefetchers \
  --num-mems=2 \
  --mem-size=1GB \
  --mem-type=LPDDR3_1600_x32  \
  --cpu-clock=2GHz \
  --sys-clock=1GHz \
  --cpu-type=detailed \
  --ruby \
  --topology=Cluster \
  --caches \
  --sweep_l1d_size=64kB \
  --l1d_size=1kB \
  --l1i_size=32kB \
  --l1d_assoc=2 \
  --l1i_assoc=2 \
  --l1_mshrs=32 \
  --l1d_banks=2 \
  --l1i_banks=2 \
  --l2cache \
  --num-l2caches=1 \
  --l2_size=512kB \
  --l2_assoc=8 \
  --l2_mshrs=32 \
  --l2_banks=8 \
  --l2_hit_latency=5 \
  --num-dirs=1 \
  --dir_size=2MB \
  --dir_banks=8 \
  --dir_mshrs=32 \
  --dir_latency=4 \
  --dma_outstanding_requests=32 \
  --bus_bw=4096 \
  --xbar_width=4096 \
  --accel_cfg_file=${bmk_home}/gem5.cfg \
  --cmd ${bmk_home}/test_dma_load_store \
  > stdout.gz
#  --mem-type=DDR3_1600_x64  \
#  --debug-flags=ProtocolTrace,DMA,NoncoherentXBar \
#  --debug-flags=ProtocolTrace,DMA,RubyDma,RubyNetwork \
