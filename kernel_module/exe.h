#ifndef EXE_H
#define EXE_H

#include <asm/ptrace.h>
#include <linux/spinlock.h>
#include <linux/kfifo.h>
#include <linux/sched/task.h>

#define un_used -2
#define un_init -1
#define normal 0
#define call_miss 1
#define ij_miss 2
#define error_inst 3
#define inst_limit 4

#define uaddr_init -1
#define uaddr_checking 0
#define uaddr_delegate 1
#define uaddr_ignore -2

static const int max_exe = 511;
static const int max_thread = 256;

static unsigned int nf_seed = 12345;
static const unsigned int nf_a = 1664525;
static const unsigned int nf_c = 1013904223;
static const unsigned int nf_m = 0xFFFFFFFF;

#define max_delegate_uaddr 31
#define max_ran 511
#define max_uaddr 131071
#define max_call 31

struct exe_info {
	int status_code;
	int pid;
	int tgid;
	unsigned long RIP;
	void* code_block;
	unsigned long jump_from;
	unsigned long tgt_addr;
	unsigned long fsbase;
	int success_cnt;
	int fail_cnt;
}__attribute__((aligned(64)));

struct exe_ret {
	int ret;
	int count;
	int number;
	int wake_num[16];
	unsigned long uaddr[16];
};

struct nf_thread {
	int pid;
	unsigned long fsbase;
}__attribute__((aligned(64)));

struct numa_delegate_info {
	atomic_t is_delegating;
}__attribute__((aligned(64)));

struct nf_uaddr_state {
	unsigned long uaddr;
	int cnt;
	unsigned long begin;
	unsigned long end;
	int state;
	char padding_1[96];
	atomic_t is_delegating;
	char padding_2[124];
	struct numa_delegate_info numa_info[2];
}__attribute__((aligned(64)));

struct nf_call_inst {
	unsigned long address;
	int offset;
	char padding[52];
}__attribute__((aligned(64)));

struct nf_delegate_queue {
	atomic_long_t q;
	char padding_2[248];
}__attribute__((aligned(64)));

struct nf_delegate_complete {
	atomic_t state;
	char padding_2[252];
	struct task_struct* task;
	struct pt_regs* regs;
	unsigned long rsp;
	struct futex_q* q;
	u32 val;
	char padding_3[28];
}__attribute__((aligned(64)));

struct nf_delegate_uaddrnode {
	unsigned long uaddr;
	char padding1[248];
	struct nf_delegate_queue queue;
	struct nf_delegate_queue wake_queue;
	struct nf_delegate_complete completes[64];
}__attribute__((aligned(64)));

struct nf_delegate_numanode {
	struct nf_delegate_uaddrnode uaddr_node[max_delegate_uaddr];
}__attribute__((aligned(64)));

struct nf_cycle_cnt {
	int cnt[30];
	unsigned long min;
	unsigned long total;
	unsigned long total_cnt;
	char padding[48];
}__attribute__((aligned(64)));

struct nf_random {
	int ran[max_ran];
	int cur_index;
}__attribute__((aligned(64)));

void exe_add(int pid, unsigned long RIP);
void exe_init(void);
void exe_exit(void);
void exe_on_call(struct task_struct* task, struct pt_regs* regs, struct exe_ret* r);

struct exe_info* get_exe(unsigned int pid, unsigned long ip);
struct nf_delegate_uaddrnode* get_uaddrnode(unsigned long uaddr, struct nf_delegate_uaddrnode* u_list);
struct nf_uaddr_state* get_uaddr_state(unsigned long uaddr);
struct nf_call_inst* get_call_inst(unsigned long addr);

static inline unsigned int zz_hash(unsigned int pid, unsigned long ip) {
	return (pid + (ip & 0xFFFF)) * 1000003;
}

static inline unsigned int nf_random(void) {
	nf_seed = (nf_a * nf_seed + nf_c) % nf_m;
	return nf_seed;
}

static inline void nf_prefetchw(volatile void* v) {
	barrier();
	__asm__ __volatile__(
		"prefetchw (%0)"
		:
	: "r" (v)
		:
		);
	barrier();
	// *((volatile long*)v) = *((volatile long*)v);
}

static inline void nf_clwb(void* v) {
	__asm__ __volatile__(
		"clwb (%0)"
		:
	: "r" (v)
		: "memory"
		);
}

static inline void nf_cldemote(void* v) {
	__asm__ __volatile__(
		"cldemote (%0)"
		:
	: "r" (v)
		: "memory"
		);
}

#endif
