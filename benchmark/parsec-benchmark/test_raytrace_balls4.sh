#!/bin/bash

echo "Splash2x benchmark"
echo "raytrace balls4 test"

thread_number=64
current_dir=$(pwd)

echo "BCD test"

cp ./../../kernel_module/km.sh .
cp ./../../kernel_module/nf_km.ko .

cd ../../daemon
sudo ./daemon1 &
cd ../benchmark/parsec-benchmark

echo -n > result/raytrace/balls4_new.txt

for ((i=0;i<11;i++))
do
    ./km.sh raytrace
    sleep 5
    taskset -c 0-63 ./bin/parsecmgmt -a run -p splash2x.raytrace -i simlarge -n $thread_number >> result/raytrace/balls4_new.txt
    sleep 2
done

sudo kill -9 `pidof daemon1`

./km.sh nf_test

sleep 5

echo "baseline test"

echo -n > result/raytrace/balls4_old.txt

for ((i=0;i<10;i++))
do
    taskset -c 0-63 ./bin/parsecmgmt -a run -p splash2x.raytrace -i simlarge -n $thread_number >> result/raytrace/balls4_old.txt
    sleep 2
done

echo "TCLock SP test"

echo -n > result/raytrace/balls4_tcs.txt

export test_lock="$current_dir/../../TCLocks/src/userspace/litl/libkomb_spinlock.sh"
for ((i=0;i<10;i++))
do  
    taskset -c 0-63 ./bin/parsecmgmt -a run -p splash2x.raytrace -i simlarge -n $thread_number >> result/raytrace/balls4_tcs.txt
    sleep 2
done
export test_lock=""

echo "TCLock B test"

echo -n > result/raytrace/balls4_tcb.txt

export test_lock="$current_dir/../../TCLocks/src/userspace/litl/libkombmtx_spin_then_park.sh"
for ((i=0;i<10;i++))
do
    taskset -c 0-63 ./bin/parsecmgmt -a run -p splash2x.raytrace -i simlarge -n $thread_number >> result/raytrace/balls4_tcb.txt
    sleep 2
done
export test_lock=""

echo "test result:"
echo -n "BCD:       "
python3 get_time_1.py result/raytrace/balls4_new.txt
echo -n "baseline:  "
python3 get_time_1.py result/raytrace/balls4_old.txt
echo -n "TCLock SP: "
python3 get_time_1.py result/raytrace/balls4_tcs.txt
echo -n "TCLock B:  "
python3 get_time_1.py result/raytrace/balls4_tcb.txt