#!/bin/bash

echo "Splash2x benchmark"
echo "Radiosity test"

current_dir=$(pwd)

echo "BCD test"

cp ./../../kernel_module/km.sh .
cp ./../../kernel_module/nf_km.ko .

cd ../../daemon
sudo ./daemon1 &
cd ../benchmark/parsec-benchmark

echo -n > result/radiosity/new.txt

for ((i=0;i<11;i++))
do
    ./km.sh radiosity
    sleep 5
    taskset -c 0-63 ./bin/parsecmgmt -a run -p splash2x.radiosity -i simlarge -n 64 >> result/radiosity/new.txt
    sleep 2
done

sudo kill -9 `pidof daemon1`

./km.sh nf_test

sleep 5

echo "baseline test"

echo -n > result/radiosity/old.txt

for ((i=0;i<10;i++))
do
    taskset -c 0-63 ./bin/parsecmgmt -a run -p splash2x.radiosity -i simlarge -n 64 >> result/radiosity/old.txt
    sleep 2
done

echo "TCLock SP test"

echo -n > result/radiosity/tcs.txt

export test_lock="$current_dir/../../TCLocks/src/userspace/litl/libkomb_spinlock.sh"
for ((i=0;i<10;i++))
do  
    taskset -c 0-63 ./bin/parsecmgmt -a run -p splash2x.radiosity -i simlarge -n 64 >> result/radiosity/tcs.txt
    sleep 2
done
export test_lock=""

echo "TCLock B test"

echo -n > result/radiosity/tcb.txt

export test_lock="$current_dir/../../TCLocks/src/userspace/litl/libkombmtx_spin_then_park.sh"
for ((i=0;i<10;i++))
do
    taskset -c 0-63 ./bin/parsecmgmt -a run -p splash2x.radiosity -i simlarge -n 64 >> result/radiosity/tcb.txt
    sleep 2
done
export test_lock=""

echo "test result:"
echo -n "BCD:       "
python3 get_time_1.py result/radiosity/new.txt
echo -n "baseline:  "
python3 get_time_1.py result/radiosity/old.txt
echo -n "TCLock SP: "
python3 get_time_1.py result/radiosity/tcs.txt
echo -n "TCLock B:  "
python3 get_time_1.py result/radiosity/tcb.txt