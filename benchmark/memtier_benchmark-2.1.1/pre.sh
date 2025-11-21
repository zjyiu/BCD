#!/bin/bash

proc_number=$(nproc)
client_thread=8

taskset -c 0-$(($1-1)) ../memcached-1.6.32/memcached -d -t $1
./km.sh "mc-worker"
sleep 5
taskset -c $(($proc_number-$client_thread))-$(($proc_number-1)) ./memtier_benchmark -P memcache_text -p 11211 -n 100000 -t $client_thread --ratio=1:0 --pipeline=3
sleep 2
sudo kill -9 `pidof memcached`
sleep 5