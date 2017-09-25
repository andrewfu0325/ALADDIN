make gem5

#baseline
#sh ruby.sh baseline.cfg base.out 0
#perfect
sh ruby.sh perfect.cfg perfect.out 0
#host page walking
#sh ruby.sh hostPageWalk.cfg hpw.out 0
#cache forwarding
#make gem5 GEM5_SPECIFIC_CFLAGS="-DCACHE_FORWARDING"
#sh ruby.sh cacheForwarding.cfg cf.out 0
