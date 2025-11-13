list:
	gcc -O2 -DLIST micro_benchmark.c -o test -lpthread
	gcc -O2 -DLIST micro_benchmark.c -o nf_test -lpthread

array:
	gcc -O2 micro_benchmark.c -o test -lpthread
	gcc -O2 micro_benchmark.c -o nf_test -lpthread

latency:
	gcc -O2 -DLIST test_latency.c -o test -lpthread
	gcc -O2 -DLIST test_latency.c -o nf_test -lpthread

rw:
	gcc -O2 -DLIST rwlock.c -o test -lpthread
	gcc -O2 -DLIST rwlock.c -o nf_test -lpthread

clean:
	rm test nf_test