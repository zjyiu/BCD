cnum=4
ncs=500
t=64

make rw

echo "mirco-benchmark read-write lock test"

cp ./../../kernel_module/km.sh .
cp ./../../kernel_module/nf_km.ko .

echo "BCD test"

cd ../../daemon
sudo ./daemon1 &
cd ../benchmark/micro_benchmark

echo preparing
echo -n > ./rwlock/new.txt
./km.sh nf_test
./nf_test -t $t -c $cnum -n $ncs -w 100
echo "test start"

for i in 1 5 10 20 30 40 50 60 70 80 90 100
do
    ./km.sh nf_test
    sleep 5
    echo "write ratio $i% "
    ./nf_test -t $t -c $cnum -n $ncs -w $i >> ./rwlock/new.txt
    sleep 2
done

sudo kill -9 `pidof daemon1`

echo "Baseline test"

echo -n > ./rwlock/old.txt

for i in 1 5 10 20 30 40 50 60 70 80 90 100
do
    echo "write ratio $i% "
    ./test -t $t -c $cnum -n $ncs -w $i >> ./rwlock/old.txt
    sleep 2
done

python3 rwlock/draw.py

echo "see result in rwlock/result.pdf"