#ifndef RWLOCK_H
#define RWLOCK_H

#include <pthread.h>

typedef struct {
    volatile int cnt;
    char padding[60];
    pthread_mutex_t r_lock;
    pthread_mutex_t g_lock;
}__attribute__((aligned(64))) rwlock;

void rwlock_init(rwlock* lock) {
    lock->cnt = 0;
    pthread_mutex_init(&lock->r_lock, NULL);
    pthread_mutex_init(&lock->g_lock, NULL);
}

void rwlock_rdlock(rwlock* lock) {
    pthread_mutex_lock(&lock->r_lock);
    lock->cnt++;
    if (lock->cnt == 1)
        pthread_mutex_lock(&lock->g_lock);
    pthread_mutex_unlock(&lock->r_lock);
}

void rwlock_rdunlock(rwlock* lock) {
    pthread_mutex_lock(&lock->r_lock);
    lock->cnt--;
    if (lock->cnt == 0)
        pthread_mutex_unlock(&lock->g_lock);
    pthread_mutex_unlock(&lock->r_lock);
}

void rwlock_wrlock(rwlock* lock) {
    pthread_mutex_lock(&lock->g_lock);
}

void rwlock_wrunlock(rwlock* lock) {
    pthread_mutex_unlock(&lock->g_lock);
}

#endif