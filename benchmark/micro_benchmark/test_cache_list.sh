ncs=500
tnum=64

make list

echo "mirco-benchmark cache line count in CS test (array)"

echo "BCD test"

cp ./../../kernel_module/km.sh .
cp ./../../kernel_module/nf_km.ko .

cd ../../daemon
sudo ./daemon1 &
cd ../benchmark/micro_benchmark

echo preparing
echo -n > ./mutex/cache/list/new.txt
./km.sh nf_test
./nf_test -t $tnum -c 1 -n $ncs
echo "test start"

for i in 1 2 4 8 16 32 64 128
do
    ./km.sh nf_test
    sleep 5
    echo "cache $i "
    ./nf_test -t $tnum -c $i -n $ncs >> ./mutex/cache/list/new.txt
    sleep 2
done

sudo kill -9 `pidof daemon1`

echo "Baseline test"

echo -n > ./mutex/cache/list/old.txt

for i in 1 2 4 8 16 32 64 128
do
    echo "cache $i "
    ./test -t $tnum -c $i -n $ncs >> ./mutex/cache/list/old.txt
    sleep 2
done

echo "TCLock SP test"

echo -n > ./mutex/cache/list/tcs.txt

for i in 1 2 4 8 16 32 64 128
do
    echo "cache $i "
    ./../../TCLocks/src/userspace/litl/libkomb_spinlock.sh ./test -t $tnum -c $i -n $ncs >> ./mutex/cache/list/tcs.txt
    sleep 2
done

echo "TCLock B test"

echo -n > ./mutex/cache/list/tcb.txt

for i in 1 2 4 8 16 32 64 128
do
    echo "cache $i "
    ./../../TCLocks/src/userspace/litl/libkombmtx_spin_then_park.sh ./test -t $tnum -c $i -n $ncs >> ./mutex/cache/list/tcb.txt
    sleep 2
done

python3 mutex/cache/list/draw.py

echo "see result in mutex/cache/list/result.pdf"