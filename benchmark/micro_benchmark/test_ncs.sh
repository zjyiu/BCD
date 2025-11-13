cnum=4
tnum=64

make list

echo "mirco-benchmark NCS test"

echo "BCD test"

cp ./../../kernel_module/km.sh .
cp ./../../kernel_module/nf_km.ko .

cd ../../daemon
sudo ./daemon1 &
cd ../benchmark/micro_benchmark

echo preparing
echo -n > ./mutex/ncs/new.txt
./km.sh nf_test
./nf_test -t 10 -c $cnum -n 500
echo "test start"

for i in 10 30 100 300 1000 3000 10000 30000 100000 300000 1000000
do
    ./km.sh nf_test
    sleep 5
    echo "ncs $i "
    ./nf_test -t $tnum -c $cnum -n $i >> ./mutex/ncs/new.txt
    sleep 2
done

sudo kill -9 `pidof daemon1`

echo "Baseline test"

echo -n > ./mutex/ncs/old.txt

for i in 10 30 100 300 1000 3000 10000 30000 100000 300000 1000000
do
    echo "ncs $i "
    ./test -t $tnum -c $cnum -n $i >> ./mutex/ncs/old.txt
    sleep 2
done

echo "TCLock SP test"

echo -n > ./mutex/ncs/tcs.txt

for i in 10 30 100 300 1000 3000 10000 30000 100000 300000 1000000
do
    echo "ncs $i "
    ./../../TCLocks/src/userspace/litl/libkomb_spinlock.sh ./test -t $tnum -c $cnum -n $i >> ./mutex/ncs/tcs.txt
    sleep 2
done

echo "TCLock B test"

echo -n > ./mutex/ncs/tcb.txt

for i in 10 30 100 300 1000 3000 10000 30000 100000 300000 1000000
do
    echo "ncs $i "
    ./../../TCLocks/src/userspace/litl/libkombmtx_spin_then_park.sh ./test -t $tnum -c $cnum -n $i >> ./mutex/ncs/tcb.txt
    sleep 2
done

python3 mutex/ncs/draw.py

echo "see result in mutex/ncs/result.pdf"