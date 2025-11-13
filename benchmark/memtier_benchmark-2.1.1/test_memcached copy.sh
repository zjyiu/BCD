#!/bin/bash

echo "memcached set test"

echo "BCD test"

./pre.sh > pre.txt

sleep 5

echo -n > result_new.txt

for i in 4 8 12 16 20 24 28 32 36 40 44 48 52 56
do
    ./test_one.sh nf $i > new.txt
    sleep 5
    echo "thread $i"
    python3 get_ops.py new.txt >> result_new.txt
done

echo "baseline test"

echo -n > result_old.txt

./../../kernel_module/km.sh "nf_test"
sleep 5
for i in 4 8 12 16 20 24 28 32 36 40 44 48 52 56
do
    ./test_one.sh of $i > old.txt
    echo "thread $i"
    python3 get_ops.py old.txt >> result_old.txt
done

python3 draw.py

echo "see result in result.pdf"