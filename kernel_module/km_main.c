#include <linux/init.h>
#include <linux/kallsyms.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/sched/task_stack.h>
#include <linux/string.h>
#include <linux/perf_event.h>
// #include <asm/ptrace.h>

#include "exe.h"
#include "sys.h"
#include "new_futex.h"
#include "fault.h"

#define MAX_NAME_CNT 20
#define MAX_NAME_LENGTH 20

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Junyao Zhang");

#define rate_int(a,b) b>0?((a*100)/b):0
#define rate_dec(a,b) b>0?(((a*10000)/b)%100):0

extern int velvet_wait_queue;
extern int delegate_election;
module_param(velvet_wait_queue, int, S_IRUGO);
module_param(delegate_election, int, S_IRUGO);

char* target_process = "nf_test";
module_param(target_process, charp, S_IRUGO);
char* target_names[MAX_NAME_CNT] = { 0 };

// extern void (*nf_debug)(char*);
extern int (*is_nf_test)(void);

extern int (*nf_futex_wake)(u32 __user*, unsigned int, int, u32);
extern long (*nf_futex_wait)(u32 __user*, unsigned int, u32, ktime_t*, u32);
// extern int (*nf_check_page_fault)(struct pt_regs*);
// extern int (*new_futex_syscall_wake)(u32 __user*, unsigned int, int, u32);
extern int (*nf_do_int3)(struct pt_regs*);

extern unsigned long nf_sys_call_table;

long nf_success_cnt;
long nf_success_wake_cnt;
long nf_total_cnt;
unsigned long uaddrs[102400];
unsigned long uaddr_cnts[102400];
unsigned long nf_wake_rets[512];
long uaddr_cnt;

struct perf_event* event;

static void __debug(char* s) {
	// printk(KERN_INFO "%d---%s\n", current->pid, s);
}

int __is_nf_test(void) {
	int i;
	if (strcmp(current->comm, target_names[0]) == 0)
		return 2;
	for (i = 0;i < MAX_NAME_CNT;i++) {
		if (target_names[i] == NULL)
			break;
		if (strcmp(current->comm, target_names[i]) == 0)
			return 1;
	}

	return 0;
}

void __test(int* v) {
	rmb();
}

static int __init nf_km_init(void) {
	int i;
	char* token;
	exe_init();
	sys_init();
	i = 0;
	token = strsep(&target_process, " ");
	while (token != NULL) {
		if (i < MAX_NAME_CNT) {
			target_names[i] = (char*)kmalloc(MAX_NAME_LENGTH, GFP_KERNEL);
			strncpy(target_names[i], token, MAX_NAME_LENGTH - 1);
			target_names[i][MAX_NAME_LENGTH - 1] = '\0';
			i++;
		}
		token = strsep(&target_process, " ");
	}

	for (i = 0;i < MAX_NAME_CNT;i++) {
		if (target_names[i] == NULL)
			break;
		pr_info("%s", target_names[i]);
	}

	uaddr_cnt = 0;
	nf_success_cnt = 0;
	nf_success_wake_cnt = 0;
	nf_total_cnt = 0;
	for (i = 0;i < NF_CNT_NUM + 2;i++)
		nf_wake_rets[i] = 0;
	// nf_debug = __debug;
	is_nf_test = __is_nf_test;
	nf_futex_wait = new_futex_wait;
	nf_futex_wake = new_futex_wake;
	// nf_check_page_fault = NULL;
	nf_do_int3 = NULL;
	// new_futex_syscall_wake = NULL;
	nf_sys_call_table = kallsyms_lookup_name("sys_call_table");
	printk(KERN_INFO "nf kernel module init\n");
	return 0;
}

static void __exit nf_km_exit(void) {
	int i, target_cnt = 0, time_cnt = 0;
	nf_futex_wait = NULL;
	nf_futex_wake = NULL;
	// nf_debug = NULL;
	// nf_check_page_fault = NULL;
	is_nf_test = NULL;
	// new_futex_syscall_wake = NULL;
	nf_do_int3 = NULL;
	printk(KERN_INFO "nf_success_cnt:%ld nf_total_cnt:%ld nf_success_wake_cnt:%ld success_rate:%ld.%02ld%% success_wake_rate:%ld.%02ld%%\n", \
		nf_success_cnt, nf_total_cnt, nf_success_wake_cnt, \
		rate_int(nf_success_cnt, nf_total_cnt), rate_dec(nf_success_cnt, nf_total_cnt), \
		rate_int(nf_success_wake_cnt, nf_success_cnt), rate_dec(nf_success_wake_cnt, nf_success_cnt));
	for (i = 0;i < NF_CNT_NUM + 2;i++) {
		target_cnt += i * nf_wake_rets[i];
		time_cnt += nf_wake_rets[i];
		printk(KERN_INFO "ret: %d cnt: %lu\n", i, nf_wake_rets[i]);
	}
	if (time_cnt > 0)
		pr_info("average ret: %d", target_cnt / time_cnt);
	pr_info("uaddr total: %d", uaddr_cnt);
	for (i = 0;i < uaddr_cnt;i++)
		// if (uaddr_cnts[i] > 100)
		pr_info("%lx --- %d", uaddrs[i], uaddr_cnts[i]);

	for (i = 0;i < MAX_NAME_CNT;i++)
		if (target_names[i] != NULL)
			kfree(target_names[i]);

	exe_exit();
	sys_exit();
	printk(KERN_INFO "nf kernel module exit\n");
}

module_init(nf_km_init);
module_exit(nf_km_exit);
