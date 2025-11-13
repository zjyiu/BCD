#include <linux/futex.h>
#include <syscall.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>

#define barrier() asm volatile("": : :"memory")

static inline unsigned xchg_32(void* ptr, unsigned x)
{
    __asm__ __volatile__("xchgl %0,%1"
        :"=r" ((unsigned)x)
        : "m" (*(volatile unsigned*)ptr), "0" (x)
        : "memory");

    return x;
}

// Futex wait
static int futex_wait(volatile int* addr, int val) {
    return syscall(SYS_futex, addr, 0x80, val, NULL, NULL, 0);
}

// Futex wake
static int futex_wake(volatile int* addr, int val) {
    return syscall(SYS_futex, addr, 0x81, val, NULL, NULL, 0);
}

// Simple futex-based lock
typedef struct {
    unsigned int locked;
} futex_lock_t;

void futex_lock(futex_lock_t* lock) {
    while (1) {
        // if (lock->locked == 1)
        barrier();
        if (!xchg_32(&lock->locked, 1))
            return;
        barrier();
        // printf("lock: %d\n", lock->locked);
        futex_wait(&lock->locked, 1);
    }
}

void futex_unlock(futex_lock_t* lock) {
    barrier();
    lock->locked = 0;
    barrier();
    futex_wake(&lock->locked, 1);
}
