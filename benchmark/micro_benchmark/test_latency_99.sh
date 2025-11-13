cnum=4
ncs=500

make latency

echo "mirco-benchmark P99 latency test"

echo "BCD test"

cp ./../../kernel_module/km.sh .
cp ./../../kernel_module/nf_km.ko .

cd ../../daemon
sudo ./daemon1 &
cd ../benchmark/micro_benchmark

echo preparing
echo -n > ./mutex/latency/99/new.txt
./km.sh nf_test
./nf_test -t 10 -c $cnum -n $ncs -p 99
echo "test start"

for i in 1 2 4 6 8 12 16 20 24 28 32 40 48 56 64 80 96 112 128 192 256
do
    ./km.sh nf_test
    sleep 5
    echo "thread $i "
    ./nf_test -t $i -c $cnum -n $ncs -p 99 >> ./mutex/latency/99/new.txt
    sleep 2
done

sudo kill -9 `pidof daemon1`

echo "Baseline test"

echo -n > ./mutex/latency/99/old.txt

for i in 1 2 4 6 8 12 16 20 24 28 32 40 48 56 64 80 96 112 128 192 256
do
    echo "thread $i "
    ./test -t $i -c $cnum -n $ncs -p 99 >> ./mutex/latency/99/old.txt
    sleep 2
done

echo "TCLock SP test"

echo -n > ./mutex/latency/99/tcs.txt

for i in 1 2 4 6 8 12 16 20 24 28 32 40 48 56 64 80 96 112 128 192 256
do
    echo "thread $i "
    ./../../TCLocks/src/userspace/litl/libkomb_spinlock.sh ./test -t $i -c $cnum -n $ncs -p 99 >> ./mutex/latency/99/tcs.txt
    sleep 2
done

echo "TCLock B test"

echo -n > ./mutex/latency/99/tcb.txt

for i in 1 2 4 6 8 12 16 20 24 28 32 40 48 56 64 80 96 112 128 192 256
do
    echo "thread $i "
    ./../../TCLocks/src/userspace/litl/libkombmtx_spin_then_park.sh ./test -t $i -c $cnum -n $ncs -p 99 >> ./mutex/latency/99/tcb.txt
    sleep 2
done

python3 mutex/latency/99/draw.py

echo "see result in mutex/latency/99/result.pdf"