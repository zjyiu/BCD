obj-m += nf_km.o

# ccflags-y += -O2
nf_km-objs := km_main.o fault.o new_futex.o sys.o exe.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=/home/zz/work/new_futex/kernel_module modules 
# EXTRA_CFLAGS="-O1"

clean:
	make -C /lib/modules/$(shell uname -r)/build M=/home/zz/work/new_futex/kernel_module clean