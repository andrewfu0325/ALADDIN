#!/usr/bin/env bash

bmk_home=${ALADDIN_HOME}/TestSuite/stencil/stencil2d
gem5_dir=${ALADDIN_HOME}/../..

  ${gem5_dir}/build/X86_MOESI_CMP_directory/gem5.opt \
  --debug-flags=WorkItems,HybridDatapath \
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
  --l1d_size=8kB \
  --l1i_size=32kB \
  --l1d_assoc=2 \
  --l1i_assoc=2 \
  --l1_mshrs=64 \
  --l2cache \
  --num-l2caches=1 \
  --l2_size=16MB \
  --l2_assoc=8 \
  --l2_mshrs=64 \
  --l2_hit_latency=5 \
  --num-dirs=1 \
  --dir_size=2MB \
  --dir_mshrs=32 \
  --dir_latency=4 \
  --dma_outstanding_requests=8 \
  --bus_bw=16 \
  --xbar_width=4096 \
  --accel_cfg_file=${bmk_home}/gem5.cfg \
  --work-end-exit-count=1 \
  --cmd ${bmk_home}/stencil-stencil2d-gem5-accel \
  > stdout.gz 
#  --mem-type=DDR3_1600_x64  \
#  --debug-flags=ProtocolTrace,DMA,NoncoherentXBar \
#  --debug-flags=ProtocolTrace,DMA,RubyDma,RubyNetwork \
#  --cmd ${bmk_home}/test_dma_load_store \
#  --debug-flags=HybridDatapath,DMA \
#  --debug-flags=HybridDatapath,DMA \
#  --debug-flags=ProtocolTrace,DMA \
#  --debug-flags=WorkItems,HybridDatapath \
