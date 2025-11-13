#!/bin/bash

echo "Splash2x benchmark"
echo "Volrend test"

current_dir=$(pwd)

echo "BCD test"

cp ./../../kernel_module/km.sh .
cp ./../../kernel_module/nf_km.ko .

cd ../../daemon
sudo ./daemon1 &
cd ../benchmark/parsec-benchmark

echo -n > result/volrend/new.txt

for ((i=0;i<11;i++))
do
    ./km.sh volrend
    sleep 5
    taskset -c 0-63 ./bin/parsecmgmt -a run -p splash2x.volrend -i native -n 64 >> result/volrend/new.txt
    sleep 2
done

sudo kill -9 `pidof daemon1`

./km.sh nf_test

sleep 20

echo "baseline test"

echo -n > result/volrend/old.txt

for ((i=0;i<10;i++))
do
    taskset -c 0-63 ./bin/parsecmgmt -a run -p splash2x.volrend -i native -n 64 >> result/volrend/old.txt
    sleep 2
done

sleep 10

echo "TCLock SP test"

echo -n > result/volrend/tcs.txt

export test_lock="$current_dir/../../TCLocks/src/userspace/litl/libkomb_spinlock.sh"
for ((i=0;i<10;i++))
do  
    taskset -c 0-63 ./bin/parsecmgmt -a run -p splash2x.volrend -i native -n 64 >> result/volrend/tcs.txt
    sleep 2
done
export test_lock=""

sleep 10

echo "TCLock B test"

echo -n > result/volrend/tcb.txt

export test_lock="$current_dir/../../TCLocks/src/userspace/litl/libkombmtx_spin_then_park.sh"
for ((i=0;i<10;i++))
do
    taskset -c 0-63 ./bin/parsecmgmt -a run -p splash2x.volrend -i native -n 64 >> result/volrend/tcb.txt
    sleep 2
done
export test_lock=""

echo "test result:"
echo -n "BCD:       "
python3 get_time_2.py result/volrend/new.txt
echo -n "baseline:  "
python3 get_time_2.py result/volrend/old.txt
echo -n "TCLock SP: "
python3 get_time_2.py result/volrend/tcs.txt
echo -n "TCLock B:  "
python3 get_time_2.py result/volrend/tcb.txt