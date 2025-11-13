#!/bin/bash
if [ $1 = "nf" ]
    then

    # cd ../../daemon
    # sudo ./daemon1 &
    # cd ../benchmark/memtier_benchmark-2.1.1

    taskset -c 0-$(($2-1)) ../memcached-1.6.32/memcached -d -t $2

    sleep 3

    # ./pre.sh > pre.txt

    for i in 1
    do 
        
        ./km.sh "mc-worker"
        sleep 5
        taskset -c 56-63 ./memtier_benchmark -P memcache_text -p 11211 -n 100000 -t 8 --ratio=1:0 --pipeline=3
        sleep 2
    done
    sudo kill -9 `pidof memcached`

    # sudo kill -9 `pidof daemon1`
    
elif [ $1 = "of" ]
    then
    taskset -c 0-$(($2-1)) ../memcached-1.6.32/memcached -d -t $2
    for i in 1
    do
        sleep 5
        taskset -c 56-63 ./memtier_benchmark -P memcache_text -p 11211 -n 100000 -t 8 --ratio=1:0 --pipeline=3
    done
    sudo kill -9 `pidof memcached`
fi