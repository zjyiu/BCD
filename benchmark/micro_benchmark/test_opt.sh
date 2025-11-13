cnum=4
ncs=500

make list

echo " effect of optimization test"

cp ./../../kernel_module/km.sh .
cp ./../../kernel_module/nf_km.ko .

echo "Baseline test"

echo -n > ./mutex/opt/old.txt

for i in 1 2 4 6 8 12 16 20 24 28 32 40 48 56 64 80 96 112 128 192 256
do
    echo "thread $i "
    ./test -t $i -c $cnum -n $ncs >> ./mutex/opt/old.txt
    sleep 2
done

cd ../../daemon
sudo ./daemon1 &
cd ../benchmark/micro_benchmark

echo " + delegation test"

echo preparing
echo -n > ./mutex/opt/ub.txt
./km.sh nf_test
./nf_test -t 10 -c $cnum -n $ncs

for i in 1 2 4 6 8 12 16 20 24 28 32 40 48 56 64 80 96 112 128 192 256
do
    ./km.sh nf_test UB
    sleep 5
    echo "thread $i "
    ./nf_test -t $i -c $cnum -n $ncs >> ./mutex/opt/ub.txt
    sleep 2
done

echo " + velvet wait queue test"

echo -n > ./mutex/opt/vw.txt

for i in 1 2 4 6 8 12 16 20 24 28 32 40 48 56 64 80 96 112 128 192 256
do
    ./km.sh nf_test VW
    sleep 5
    echo "thread $i "
    ./nf_test -t $i -c $cnum -n $ncs >> ./mutex/opt/vw.txt
    sleep 2
done

echo "+ delegate election test"

echo -n > ./mutex/opt/new.txt

for i in 1 2 4 6 8 12 16 20 24 28 32 40 48 56 64 80 96 112 128 192 256
do
    ./km.sh nf_test
    sleep 5
    echo "thread $i "
    ./nf_test -t $i -c $cnum -n $ncs >> ./mutex/opt/new.txt
    sleep 2
done

sudo kill -9 `pidof daemon1`

python3 mutex/opt/draw.py

echo "see result in mutex/opt/result.pdf"