#!/usr/bin/env bash

cfg_home=${ALADDIN_HOME}/integration-test/with-cpu/test_double_buffering
gem5_dir=${ALADDIN_HOME}/../..

${gem5_dir}/build/X86_MOESI_CMP_directory/gem5.opt \
  --debug-flags=HybridDatapath \
  --outdir=${cfg_home}/outputs \
  ${gem5_dir}/configs/aladdin/aladdin_se.py \
  --num-cpus=1 \
  --mem-size=4GB \
  --mem-type=DDR3_1600_x64  \
  --sys-clock=1GHz \
  --cpu-type=detailed \
  --enable_prefetchers \
  --caches \
  --accel_cfg_file=${cfg_home}/gem5.cfg \
  -c test_double_buffering \
  > stdout.gz
