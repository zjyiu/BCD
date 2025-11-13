#!/bin/bash

echo "LevelDB Readrandom test"

./db_bench --benchmarks=fillseq --db=./test_db --threads=1 --num=4000000 > pre.txt

echo "BCD test"

cp ./../../../kernel_module/km.sh .
cp ./../../../kernel_module/nf_km.ko .

cd ../../../daemon
sudo ./daemon1 &
cd ../benchmark/leveldb-1.23/build

echo -n > data.txt

./km.sh db_bench
sleep 5
taskset -c 0-31 ./db_bench --benchmarks=readrandom --use_existing_db=1 --db=./test_db --num=4000000 --threads=32 >> pre.txt
sleep 2
for i in 4 8 12 16 20 24 28 32 36 40 44 48 52 56 60 64
do
    ./km.sh db_bench
    sleep 5
    taskset -c 0-$((i-1)) ./db_bench --benchmarks=readrandom --use_existing_db=1 --db=./test_db --num=4000000 --threads=$i >> data.txt
    sleep 2
done

python3 get_ops.py data.txt > new.txt

sudo kill -9 `pidof daemon1`

./km.sh nf_test

sleep 5

echo "baseline test"

echo -n > data.txt

for i in 4 8 12 16 20 24 28 32 36 40 44 48 52 56 60 64
do
    taskset -c 0-$((i-1)) ./db_bench --benchmarks=readrandom --use_existing_db=1 --db=./test_db --num=4000000 --threads=$i >> data.txt
    sleep 2
done

python3 get_ops.py data.txt > old.txt

echo "TCLock SP test"

echo -n > data.txt

for i in 4 8 12 16 20 24 28 32 36 40 44 48 52 56 60 64
do
    ./../../../TCLocks/src/userspace/litl/libkomb_spinlock.sh taskset -c 0-$((i-1)) ./db_bench --benchmarks=readrandom --use_existing_db=1 --db=./test_db --num=4000000 --threads=$i >> data.txt
    sleep 2
done

python3 get_ops.py data.txt > tcs.txt

echo "TCLock B test"

echo -n > data.txt

for i in 4 8 12 16 20 24 28 32 36 40 44 48 52 56 60 64
do
    ./../../../TCLocks/src/userspace/litl/libkombmtx_spin_then_park.sh taskset -c 0-$((i-1)) ./db_bench --benchmarks=readrandom --use_existing_db=1 --db=./test_db --num=4000000 --threads=$i >> data.txt
    sleep 2
done

python3 get_ops.py data.txt > tcb.txt

python3 draw.py

echo "see result in result.pdf"