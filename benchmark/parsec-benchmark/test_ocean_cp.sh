#!/bin/bash

echo "Splash2x benchmark"
echo "Ocean_cp test"

current_dir=$(pwd)

echo "BCD test"

cp ./../../kernel_module/km.sh .
cp ./../../kernel_module/nf_km.ko .

echo -n > result/ocean_cp/new.txt

for ((i=0;i<10;i++))
do
    ./km.sh ocean_cp
    sleep 5
    taskset -c 0-63 ./bin/parsecmgmt -a run -p splash2x.ocean_cp -i native -n 64 >> result/ocean_cp/new.txt
    sleep 2
done

./km.sh nf_test

sleep 5

echo "baseline test"

echo -n > result/ocean_cp/old.txt

for ((i=0;i<10;i++))
do
    taskset -c 0-63 ./bin/parsecmgmt -a run -p splash2x.ocean_cp -i native -n 64 >> result/ocean_cp/old.txt
    sleep 2
done

echo "TCLock SP test"

echo -n > result/ocean_cp/tcs.txt

export test_lock="$current_dir/../../TCLocks/src/userspace/litl/libkomb_spinlock.sh"
for ((i=0;i<10;i++))
do  
    taskset -c 0-63 ./bin/parsecmgmt -a run -p splash2x.ocean_cp -i native -n 64 >> result/ocean_cp/tcs.txt
    sleep 2
done
export test_lock=""

echo "TCLock B test"

echo -n > result/ocean_cp/tcb.txt

export test_lock="$current_dir/../../TCLocks/src/userspace/litl/libkombmtx_spin_then_park.sh"
for ((i=0;i<10;i++))
do
    taskset -c 0-63 ./bin/parsecmgmt -a run -p splash2x.ocean_cp -i native -n 64 >> result/ocean_cp/tcb.txt
    sleep 2
done
export test_lock=""

echo "test result:"
echo -n "BCD:       "
python3 get_time_3.py result/ocean_cp/new.txt
echo -n "baseline:  "
python3 get_time_3.py result/ocean_cp/old.txt
echo -n "TCLock SP: "
python3 get_time_3.py result/ocean_cp/tcs.txt
echo -n "TCLock B:  "
python3 get_time_3.py result/ocean_cp/tcb.txt