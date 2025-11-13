#define _GNU_SOURCE
#include <linux/futex.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>	
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <x86intrin.h>
#include <sched.h>
#include <poll.h>

#define CMAX 32
#define TMAX 256
#define PRE_TIME 5
#define TEST_TIME 8
#define max_cost_cnt 1024000

unsigned int TNUM = 32;
unsigned int CNUM = 1;
unsigned int NCS = 1000;
unsigned int core_per_socket = 32;
double p = 0.99;

int costs[max_cost_cnt] = { 0 };
int test_begin;

struct share_cnt {
    volatile int c;
    struct share_cnt* next;
    int padding[13];
}__attribute__((aligned(64)));

pthread_mutex_t mutex;
struct share_cnt* s_cnt;
int times = 0;
long long start[TMAX], end[TMAX];
volatile int cnt = 0;
int cnts[32 * CMAX] = { 0 };
// int* cnts;

static inline uint64_t perf_counter(void) {
    __asm__ __volatile__("" : : : "memory");
    uint64_t r = __rdtsc();
    __asm__ __volatile__("" : : : "memory");
    return r;
}

static void usage(void) {
    printf("\t-t threads\tNumber of threads to run\n");
    // printf("\t-l loops\tNumber of loops to run\n");
    printf("\t-c cache lines\tNumber of cache lines in a critical section\n");
    printf("\t-n ncs\tNumber of cycles in non-critical section\n");
    printf("\t-h help\n");
    exit(1);
}

int compare(const void* a, const void* b) {
    int diff = (*(int*)a - *(int*)b);
    return (diff > 0) - (diff < 0);
}

int set_cpu(int i) {
    int cpu;
    if (TNUM >= core_per_socket)
        cpu = i;
    else
        cpu = (i == 0 || i < TNUM / 2) ? i : i - TNUM / 2 + core_per_socket / 2;
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu, &mask);
    if (-1 == pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask)) {
        fprintf(stderr, "pthread_setaffinity_np erro\n");
        return -1;
    }
    return 0;
}

void* child_thread(void* args) {
    long long t, c_begin, c_end;
    int id = *(int*)args;
    set_cpu(id);
    int cost_cnt = 0;
    // start[id] = perf_counter();
    // volatile long a[4];
    // for (long i = 0; i < 10; i = i) {
    while (1) {
        pthread_testcancel();
        if (test_begin == 1 && cost_cnt < max_cost_cnt / TNUM)
            c_begin = perf_counter();
        pthread_mutex_lock(&mutex);

#ifdef LIST
        struct share_cnt* temp;
        temp = s_cnt;
        while (temp != NULL) {
            temp->c++;
            temp = temp->next;
        }
#else
        for (int k = 0;k < CNUM;k++)
            cnts[k * 32]++;
#endif

        pthread_mutex_unlock(&mutex);
        t = perf_counter();

        if (test_begin == 1 && cost_cnt < max_cost_cnt / TNUM)
            costs[id * (max_cost_cnt / TNUM) + cost_cnt++] = t - c_begin;

        while (perf_counter() - t < NCS);
    }
    // end[id] = perf_counter();
    // printf("cnt:%d\n", s_cnt->c);
}

void printer(void) {
    pthread_t thread[TNUM];
    int args[TNUM];
    struct share_cnt* pre = NULL, * temp;
    long long start_min = INT64_MAX, end_max = 0;
    int cnt_begin, cnt_end;

    s_cnt = NULL;
    for (int i = 0;i < CNUM;i++) {
        temp = (struct share_cnt*)malloc(sizeof(struct share_cnt));
        temp->c = 0;
        temp->next = NULL;
        if (s_cnt == NULL)
            s_cnt = temp;
        if (pre != NULL)
            pre->next = temp;
        pre = temp;
    }
    // cnts = (int*)malloc(sizeof(int) * CNUM * 32);
    test_begin = 0;

    for (int i = 0; i < TNUM; i++) {
        times++;
        args[i] = i;
        int re = pthread_create(&thread[i], NULL, child_thread, (void*)(args + i));
        if (re != 0)
            printf("Error creating thread %d: %d\n", i, re);
    }

    sleep(PRE_TIME);
    test_begin = 1;
    sleep(TEST_TIME);

    for (int i = 0; i < TNUM; i++) {
        pthread_cancel(thread[i]);
    }

    // for (int i = 0;i < max_cost_cnt - 1;i++)
    //     for (int j = i + 1;j < max_cost_cnt;j++)
    //         if (costs[i] > costs[j]) {
    //             int temp = costs[i];
    //             costs[i] = costs[j];
    //             costs[j] = temp;
    //         }
    qsort(costs, max_cost_cnt, sizeof(int), compare);

    // printf("latency: %d\n", costs[(int)(max_cost_cnt * p)]);
    printf("%d\n", costs[(int)(max_cost_cnt * p)]);
}

int main(int argc, char* argv[]) {
    while (1) {
        signed char c = getopt(argc, argv, "t:c:n:p:h");
        if (c < 0)
            break;
        switch (c) {
        case 't':
            TNUM = atoi(optarg);
            if (TNUM > TMAX) {
                printf("tasks cannot exceed %d\n", TMAX);
                exit(1);
            }
            break;
        case 'c':
            CNUM = atoi(optarg);
            if (CNUM > CMAX) {
                printf("cache line cannot exceed %d\n", CMAX);
                exit(1);
            }
            break;
        case 'n':
            NCS = atoi(optarg);
            break;
        case 'p':
            p = (double)atoi(optarg) / 100.0;
            break;
        default:
            usage();
        }
    }

    pthread_mutex_init(&mutex, NULL);
    printer();
    pthread_mutex_destroy(&mutex);

    // printf("times: %d cacheline: %d ncs %d\n", TNUM, CNUM, NCS);
    return 0;
}