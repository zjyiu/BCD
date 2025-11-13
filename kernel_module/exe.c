#include "exe.h"

#include <asm-generic/set_memory.h>
#include <asm/current.h>
#include <asm/fsgsbase.h>
#include <asm/smap.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/futex.h>
#include <linux/vmalloc.h>
#include <linux/sched/task_stack.h>
#include <asm/barrier.h>
#include <linux/sched/topology.h>
#include <linux/irqflags.h>
#include <linux/syscalls.h>
#include <asm-generic/resource.h>
#include <asm-generic/mman.h>
// #include <linux/maple_tree.h>
#include <linux/rwsem.h>
#include <linux/spinlock.h>
#include <asm/tlbflush.h>
#include <linux/kallsyms.h>
#include <linux/uaccess.h>
#include <asm/processor.h>
#include <linux/atomic.h>
#include <linux/kfifo.h>
#include <linux/cpumask.h>
#include <linux/numa.h>
#include <asm/tsc.h>
#include <linux/fs.h>

#define invalid_addr_file_path "/tmp/nf_iva"

const int num_pg = 160;
unsigned long nf_sys_call_table;

extern long nf_success_cnt;
extern long nf_total_cnt;
extern long nf_success_wake_cnt;
atomic_t normal_wake_cnt;
atomic_t nf_d_lock;
atomic_t nf_com;
atomic_t nf_wake_state;
atomic_long_t count_uaddr_time;
atomic_long_t start_work_cycle;

atomic_t nf_is_delegating;
atomic_t int3_waiter_cnt;

spinlock_t int3_change_lock;

long long total_cycle = 0, total_cnt = 0;

unsigned long btc_t = 0;
unsigned long iva[32] = { 0 };
unsigned long end_point[] = { 0x7ffff7fa4bdd,0x7ffff7fa27e7,0x7ffff7fba2f6,0x555555558e80 };

struct exe_info* exes;
struct nf_delegate_numanode** nf_numanodes;
struct nf_cycle_cnt* cycle_cnts;
struct nf_uaddr_state* uaddr_states;
struct nf_call_inst* call_insts;
struct nf_thread* nf_threads;
int delegate_uaddr_cnt;

static loff_t get_file_size(struct file* file) {
	loff_t size;
	size = vfs_llseek(file, 0, SEEK_END);
	vfs_llseek(file, 0, SEEK_SET);
	return size;
}

struct exe_info* get_exe(unsigned int pid, unsigned long ip) {
	int pos = zz_hash(pid, ip) % max_exe;
	while (exes[pos].status_code != un_used && (exes[pos].pid != pid || exes[pos].RIP != ip))
		pos = (pos + 1) % max_exe;
	return &exes[pos];
}

struct nf_delegate_uaddrnode* get_uaddrnode(unsigned long uaddr, struct nf_delegate_uaddrnode* u_list) {
	int pos = uaddr % max_delegate_uaddr;
	while (u_list[pos].uaddr != 0 && u_list[pos].uaddr != uaddr)
		pos = (pos + 1) % max_delegate_uaddr;
	if (u_list[pos].uaddr == 0)
		u_list[pos].uaddr = uaddr;
	return &u_list[pos];
}

struct nf_uaddr_state* get_uaddr_state(unsigned long uaddr) {
	int pos = uaddr % max_uaddr;
	while (uaddr_states[pos].state != uaddr_init && uaddr_states[pos].uaddr != uaddr)
		pos = (pos + 1) % max_uaddr;
	if (uaddr_states[pos].state == uaddr_init) {
		uaddr_states[pos].uaddr = uaddr;
	}
	return &uaddr_states[pos];
}

struct nf_call_inst* get_call_inst(unsigned long addr) {
	int pos = addr % max_call;
	while (call_insts[pos].address != 0 && call_insts[pos].address != addr)
		pos = (pos + 1) % max_call;
	if (call_insts[pos].address == 0)
		call_insts[pos].address = addr;
	return &call_insts[pos];
}

struct nf_thread* get_nf_thread(int pid) {
	int pos = pid % max_thread;
	while (nf_threads[pos].pid != 0 && nf_threads[pos].pid != pid)
		pos = (pos + 1) % max_thread;
	if (nf_threads[pos].pid == 0)
		nf_threads[pos].pid = pid;
	return &nf_threads[pos];
}

void exe_init(void) {
	int i, j, k, ret, cpu, addr_num;
	struct file* fi;
	loff_t file_size;
	loff_t pos = 0;
	unsigned long* addr_buffer;

	pr_info("struct nf_delegate_uaddrnode %d", sizeof(struct nf_delegate_uaddrnode));

	// fi = filp_open(invalid_addr_file_path, O_RDONLY, 0);
	// if (IS_ERR(fi)) {
	// 	pr_info("Open file %s error\n", invalid_addr_file_path);
	// 	return;
	// }
	// file_size = get_file_size(fi);
	// if (file_size % sizeof(unsigned long) != 0) {
	// 	pr_info("Invalid file size: %lld\n", file_size);
	// 	filp_close(fi, NULL);
	// 	return;
	// }
	// kernel_read(fi, iva, file_size, &pos);
	// filp_close(fi, NULL);
	// for (i = 0;i < 32;i++)
	// 	if (iva[i] != 0)
	// 		pr_info("read iva : %lx", iva[i]);

	exes = (struct exe_info*)vmalloc(sizeof(struct exe_info) * max_exe);
	for (i = 0; i < max_exe; i++) {
		exes[i].status_code = un_used;
		exes[i].code_block = NULL;
		exes[i].fsbase = 0;
	}

	nf_numanodes = (struct nf_delegate_numanode**)vmalloc(sizeof(struct nf_delegate_numanode*) * num_online_nodes());
	for (i = 0;i < num_online_nodes();i++) {
		nf_numanodes[i] = (struct nf_delegate_numanode*)vmalloc_node(sizeof(struct nf_delegate_numanode), i);
		for (j = 0;j < max_delegate_uaddr;j++) {
			struct nf_delegate_uaddrnode* u_node = &nf_numanodes[i]->uaddr_node[j];
			u_node->uaddr = 0;
			atomic_long_set(&u_node->queue.q, 0);
			atomic_long_set(&u_node->wake_queue.q, 0);
			for (k = 0;k < 64;k++) {
				atomic_set(&u_node->completes[k].state, 0);
				u_node->completes[k].task = NULL;
				u_node->completes[k].regs = NULL;
				u_node->completes[k].rsp = 0;
			}
		}
	}

	uaddr_states = (struct nf_uaddr_state*)vmalloc(sizeof(struct nf_uaddr_state) * max_uaddr);
	for (i = 0;i < max_uaddr;i++) {
		uaddr_states[i].begin = 0;
		uaddr_states[i].cnt = 0;
		uaddr_states[i].end = 0;
		uaddr_states[i].state = uaddr_init;
		uaddr_states[i].uaddr = 0;
		atomic_set(&uaddr_states[i].is_delegating, 0);
	}

	// nf_threads = (struct nf_thread*)vmalloc(sizeof(struct nf_thread) * max_thread);
	// for (i = 0;i < max_thread;i++) {
	// 	nf_threads[i].pid = 0;
	// 	nf_threads[i].fsbase = 0;
	// }

	call_insts = (struct nf_call_inst*)vmalloc(sizeof(struct nf_call_inst) * max_call);
	for (i = 0;i < max_call;i++) {
		call_insts[i].address = 0;
		call_insts[i].offset = 0;
	}

	cycle_cnts = (struct nf_cycle_cnt*)vmalloc(sizeof(struct nf_cycle_cnt) * 64);
	for (i = 0;i < 64;i++) {
		cycle_cnts[i].min = INT_MAX;
		cycle_cnts[i].total = 0;
		cycle_cnts[i].total_cnt = 0;
		for (j = 0;j < 30;++j)
			cycle_cnts[i].cnt[j] = 0;
	}
	delegate_uaddr_cnt = 0;

	atomic_set(&normal_wake_cnt, 0);
	atomic_set(&nf_d_lock, 0);
	atomic_set(&nf_com, 0);
	atomic_set(&nf_wake_state, 0);
	atomic_long_set(&start_work_cycle, 0);
	atomic_long_set(&count_uaddr_time, 2500000000);
	atomic_set(&nf_is_delegating, 0);
	atomic_set(&int3_waiter_cnt, 0);
	spin_lock_init(&int3_change_lock);
}

void exe_exit(void) {
	int i, j, cpu, range[30] = { 0 };
	loff_t pos = 0;
	struct file* fi;
	unsigned long sum = 0, sum_cnt = 0;

	// fi = filp_open(invalid_addr_file_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
	// if (IS_ERR(fi)) {
	// 	pr_err("Failed to open file: %ld\n", PTR_ERR(fi));
	// 	return;
	// }
	// // iva[0] = 0xdeadbeaf;
	// for (i = 0;i < 32;i++)
	// 	if (iva[i] != 0)
	// 		kernel_write(fi, &iva[i], sizeof(unsigned long), &pos);
	// filp_close(fi, NULL);

	pr_info("normal_wake_cnt: %d nf_d_lock: %d nf_com: %d nf_wake_state: %d", \
		atomic_read(&normal_wake_cnt), atomic_read(&nf_d_lock), atomic_read(&nf_com), atomic_read(&nf_wake_state));

	for (i = 0;i < max_exe;i++) {
		// if (exes[i].code_block != NULL)
		// 	pr_info("code address: %lx", (unsigned long)(exes[i].code_block));
		vfree(exes[i].code_block);
	}

	vfree(exes);

	for (i = 0;i < 64;i++) {
		sum += cycle_cnts[i].total;
		sum_cnt += cycle_cnts[i].total_cnt;
		for (j = 0;j < 30;j++)
			range[j] += cycle_cnts[i].cnt[j];
		if (cycle_cnts[i].min < INT_MAX)
			pr_info("min cycle: %lld", cycle_cnts[i].min);
	}
	if (sum_cnt > 0)
		pr_info("average cycle: %ld", sum / sum_cnt);
	for (i = 0;i < 30;i++)
		pr_info("[%2d00 --- %2d00]: %d", i, i + 1, range[i]);

	vfree(call_insts);

	vfree(cycle_cnts);
	for (i = 0;i < num_online_nodes();i++)
		vfree(nf_numanodes[i]);
	vfree(nf_numanodes);

	for (i = 0;i < max_uaddr;i++) {
		if ((uaddr_states[i].state == uaddr_delegate || uaddr_states[i].state == uaddr_ignore) && uaddr_states[i].cnt > 100)
			pr_info("uaddr: %lx cycle: %ld state: %d cnt: %d", uaddr_states[i].uaddr, uaddr_states[i].end - uaddr_states[i].begin, uaddr_states[i].state, uaddr_states[i].cnt);
	}
	vfree(uaddr_states);

	// vfree(nf_threads);

	return;
}

extern void print_regs(char* s, int pid, struct pt_regs* regs);

int valid_address(unsigned long addr) {
	int length = sizeof(iva) / sizeof(iva[0]);
	int i;
	for (i = 0; i < length; i++) {
		if (addr == iva[i]) {
			return 0;
		}
	}
	length = sizeof(end_point) / sizeof(end_point[0]);
	for (i = 0;i < length;i++)
		if (addr == end_point[i])
			return 0;
	return 1;
}

void exe_add(int pid, unsigned long RIP) {
	void* mem;
	struct exe_info* p = get_exe(pid, RIP);

	if (p->pid == pid && p->RIP == RIP)  return;

	// if (!target_address(RIP)) return;

	if (p->status_code != un_used)
		printk(KERN_INFO "exe add crash error %d %d %lx --- %d %lx\t%d---%d\n", p->status_code, p->pid, p->RIP, pid, RIP, zz_hash(p->pid, p->RIP) % max_exe, zz_hash(pid, RIP) % max_exe);

	p->pid = pid;
	p->RIP = RIP;
	p->tgid = current->tgid;

	mem = vmalloc(4096 * num_pg);
	set_memory_x((unsigned long)mem, num_pg);

	p->code_block = mem;
	p->status_code = un_init;
	p->success_cnt = 0;
	p->fail_cnt = 0;

	if (p->fsbase == 0)
		p->fsbase = x86_fsbase_read_cpu();
	// struct nf_thread* t = get_nf_thread(pid);
	// t->fsbase = x86_fsbase_read_cpu();


	printk(KERN_INFO "exe added %d %lx %d\n", pid, RIP, current->tgid);
}

/*args is used to pass arguments to code block
  For input, the first arg is fs, and second being gs.
  For output, the first is return_code, or the address of indirect jump.
*/
void exe_on_call(struct task_struct* task, struct pt_regs* regs, struct exe_ret* r) {
	unsigned long long begin, end;

	// int cpu_id = smp_processor_id();
	// begin = rdtsc_ordered();

	// if (task->exit_state == EXIT_DEAD || task->exit_state == EXIT_ZOMBIE) {
	// 	pr_info("[exe_on_call] task is dead");
	// 	r->ret = -0xdead;
	// 	return;
	// }
	volatile int pid = task->pid;
	int tid = task->tgid;

	unsigned long args[2];
	void (*virt_ptr)(struct pt_regs* regs, unsigned long* args);
	struct exe_info* exe;
	// struct nf_thread* th;
	unsigned long cur_uaddr;
	r->count = 0;

	unsigned long origin_uaddr = regs->di;

	while (1) {  // chaining
		// if (regs->ip == 0x7ffff7fb4330)
		// 	pid = task->tgid;

		unsigned long rip = regs->ip;


		exe = get_exe(pid, rip);
		// spin_lock(&exe->lock);

		if (exe->status_code != normal || exe->RIP != regs->ip || exe->pid != pid) {
			if (exe->status_code == normal)
				printk(KERN_INFO "run error exe %lx %lx %lx %d %d!\n", exe->status_code, exe->RIP, regs->ip, exe->pid, pid);
			r->ret = -1;
			// spin_unlock(&exe->lock);
			return;
		}

		// th = get_nf_thread(pid);

		// nf_total_cnt++;

		virt_ptr = exe->code_block;

		if (exe->fsbase == 0)
			pr_info("[exe on call]\tfsbase is zero !!!");
		args[0] = exe->fsbase;
		// if (th->fsbase == 0)
		// 	pr_info("[exe on call]\tfsbase is zero !!!");
		// args[0] = th->fsbase;
		args[1] = 0;

		regs->cx = regs->ip;
		regs->r11 = regs->flags;
		regs->orig_ax = regs->ax;
		regs->ax = 0;

		// barrier();
		// rdmsrl_safe(0xC4, &begin);
		// barrier();

		stac();


		// pid = *((volatile int*)(0x5555556922a0));

		virt_ptr(regs, args);
		// barrier();
		// rdmsrl_safe(0xC4, &end);
		// barrier();

		if (args[0] == normal) {  // syscall
			if (regs->ax == 202) {  // legal syscall
				int nf_op = regs->si & 0x7f;
				if (!((regs->si) & FUTEX_PRIVATE_FLAG))
					printk(KERN_INFO "nf not shared flag\n");
				if (nf_op == 1) { // futex wake

					cur_uaddr = regs->di;
					r->uaddr[r->count] = cur_uaddr;
					r->wake_num[r->count] = regs->dx;
					r->count++;
					// exe->success_cnt++;
					// nf_success_cnt++;
					// nf_success_wake_cnt++;
					// exe->success_cnt++;
					// printk(KERN_INFO "wake number: %lx uaddr: %lx ip:%lx\n", regs->dx, origin_uaddr, regs->ip);
					if (cur_uaddr == origin_uaddr) { //wake same uaddr
						regs->ip += 2;
						regs->cx = regs->ip;
						regs->r11 = regs->flags;
						regs->orig_ax = regs->ax;
						//regs->ax = -38;
						regs->ax = 1;
						r->ret = 1;

						// end = rdtsc_ordered();
						// unsigned long cost = end - begin;
						// if (cost < 3000UL)
						// 	++cycle_cnts[cpu_id].cnt[cost / 100UL];
						// else
						// 	++cycle_cnts[cpu_id].cnt[29];
						// cycle_cnts[cpu_id].min = cycle_cnts[cpu_id].min < cost ? cycle_cnts[cpu_id].min : cost;
						// cycle_cnts[cpu_id].total += cost;
						// ++cycle_cnts[cpu_id].total_cnt;

					}
					else { //wake another uaddr
						// regs->ip += 2;
						// regs->cx = regs->ip;
						// regs->r11 = regs->flags;
						// regs->orig_ax = regs->ax;
						// regs->ax = 1;
						// exe_add(pid, regs->ip);
						// printk(KERN_INFO "not same wake.ip: %lx wake number: %lx uaddr: %lx ip:%lx rsi: %lx\n", exe->RIP, regs->dx, origin_uaddr, regs->ip, regs->si);
						// continue;
						r->ret = 2;
					}
				}
				else if (nf_op == 0 || nf_op == 9) { //futex wait
					cur_uaddr = regs->di;
					// nf_success_cnt++;
					if (cur_uaddr == origin_uaddr) {
						// regs->ip += 2;
						// regs->cx = regs->ip;
						// regs->r11 = regs->flags;
						// regs->orig_ax = regs->ax;
						// regs->ax = 0;
						r->ret = 0;


					}
					else {
						// printk(KERN_INFO "not same uaddr wait !");
						r->ret = 2;
					}
				}
				else {
					printk(KERN_INFO "illegal nf_op %d\n", nf_op);
					// exe->fail_cnt++;
					r->ret = -4;
				}
			}
			else {
				printk(KERN_INFO "illegal syscall %ld %lx\n", regs->ax, regs->ip);
				// exe->fail_cnt++;
				r->ret = -4;
			}
		}
		else if (args[0] == call_miss /*&& exe->success_cnt >= 0*/) {  // missing call
			exe->tgt_addr = regs->ip;
			// exe->fail_cnt++;
			if (valid_address(exe->tgt_addr)) {
				// exe->miss_cnt++;
				// if (exe->miss_cnt < 10)
				exe->status_code = call_miss;
				// else {
				// 	for (int i = 0;i < 10;i++) {
				// 		if (iva[i] == exe->tgt_addr)
				// 			break;
				// 		if (iva[i] == 0) {
				// 			iva[i] = exe->tgt_addr;
				// 			break;
				// 		}
				// 	}
				// }
			}
			// exe->fail_cnt++;
			r->ret = -1;
		}
		else if (args[0] == inst_limit) {
			pr_info("inst limit\n");
			exe->status_code = normal;
			r->ret = 3;
		}
		else if (1/*exe->success_cnt >= 0*/) {  // ij
			// exe->fail_cnt++;
			exe->tgt_addr = regs->ip;
			exe->jump_from = args[0];
			if (valid_address(exe->tgt_addr) && valid_address(exe->jump_from))
				exe->status_code = ij_miss;
			// exe->fail_cnt++;
			r->ret = -1;
		}
		break;
	}
	// spin_unlock(&exe->lock);
	// r->number++;
	clac();
	mb();

}