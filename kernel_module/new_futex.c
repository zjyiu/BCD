#include <linux/freezer.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>
#include <linux/sched.h>

#include <linux/sched/task_stack.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <asm/barrier.h>
#include <linux/sched/topology.h>
#include <linux/kfifo.h>
#include <linux/syscalls.h>
#include <asm/processor.h>
#include <linux/atomic.h>
#include <asm/cacheflush.h>
#include <asm/uaccess.h>
#include <linux/hrtimer.h>
#include <asm/smp.h>
#include <asm/msr.h>
#include <linux/random.h>
#include <linux/numa.h>
#include <linux/topology.h>
#include <linux/cpumask.h>
#include <linux/bitmap.h>
#include <linux/plist.h>
#include <asm/cacheflush.h>
#include <linux/io.h>
#include <asm/pgtable.h>
#include <linux/pid.h>
#include <linux/page-flags.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <asm/bitops.h>

#include "new_futex.h"
#include "exe.h"

#define prefetch_up_size 2
#define prefetch_down_size 1

int velvet_wait_queue = 1;
int delegate_election = 1;

int int3_changed = 0;

int fast_path_wait_time[8] = { 1000,2000,4000,8000,16000,32000,64000,128000 };
int fast_path_wake_time[8] = { 1000,2000,4000,8000,16000,32000,64000,128000 };

// unsigned long call_addresses[] = { 0x555555567762,0x5555555697d9,0x555555567892,0x55555556715c,0x5555555671bd, 0x5555555677b2, 0x5555555696c4, 0x555555560150 }; //memcached
// unsigned long call_addresses[] = { 0x5555555572d9,0x55555555747e,0x55555555a13c,0x5555555570b1,0x55555555a416 }; //volrend
// unsigned long call_addresses[] = { 0x555555565144,0x555555566999 }; //raytrace
unsigned long call_addresses[] = { 0x5555555640cc,0x55555556418a };

extern int delegate_uaddr_cnt;
extern long nf_success_cnt;
extern unsigned long uaddrs[];
extern unsigned long uaddr_cnts[];
extern unsigned long nf_wake_rets[];
extern long uaddr_cnt;
extern atomic_t normal_wake_cnt;
extern atomic_t nf_d_lock;
extern atomic_t nf_com;
extern atomic_t nf_wake_state;
extern atomic_long_t start_work_cycle;
extern atomic_long_t count_uaddr_time;
extern atomic_t nf_is_delegating;
extern atomic_t int3_waiter_cnt;

extern spinlock_t int3_change_lock;
extern struct nf_delegate_numanode** nf_numanodes;
extern struct nf_cycle_cnt* cycle_cnts;

extern int get_futex_key(u32 __user* uaddr, bool fshared, union futex_key* key, enum futex_access rw);
extern struct futex_hash_bucket* futex_hash(union futex_key* key);
extern void futex_wake_mark(struct wake_q_head* wake_q, struct futex_q* q);
extern void futex_wake_mark_2(struct futex_q* q);
extern void wake_up_q(struct wake_q_head* head);
extern void wake_q_add_safe(struct wake_q_head* head, struct task_struct* task);
extern struct hrtimer_sleeper* futex_setup_timer(ktime_t* time, struct hrtimer_sleeper* timeout, int flags, u64 range_ns);
extern int futex_wait_setup(u32 __user* uaddr, u32 val, unsigned int flags, struct futex_q* q, struct futex_hash_bucket** hb);
extern void futex_wait_queue(struct futex_hash_bucket* hb, struct futex_q* q, struct hrtimer_sleeper* timeout);
extern int futex_unqueue(struct futex_q* q);
extern struct futex_hash_bucket* futex_q_lock(struct futex_q* q);
extern int futex_get_value_locked(u32* dest, u32 __user* from);
extern void futex_q_unlock(struct futex_hash_bucket* hb);

// unsigned long TARGET_UADDR[] = { 0x7ffff7c47820, 0x7ffff7c47888 };
// unsigned long TARGET_UADDR[] = { 0xffff555558b8a390,0x555558b8a340,0xffff555558b8a394 };
// unsigned long TARGET_UADDR[] = { 0xffff7fffffffdbb0,0xffff7fffffffdb60,0x7fffffffdbb4 };
unsigned long TARGET_UADDR[] = { 0x7fffec000b68,0xff555555ff7928 }; //upscaledb
// unsigned long TARGET_UADDR[] = { 0x555555593a20 ,0x5555555af8d0,0x555555594ce0,0x5555555a1340 }; //memcache
// unsigned long TARGET_UADDR[] = { 0x5555555b1468 }; //readrandom
// unsigned long TARGET_UADDR[] = { 0x7fffec000b68 };

int is_target_uaddr(unsigned long uaddr) {
    int length = sizeof(TARGET_UADDR) / sizeof(unsigned long);
    if (length == 0)
        return 1;
    for (int i = 0;i < length;i++)
        if (TARGET_UADDR[i] == uaddr)
            return 1;
    return 0;
}

void modify_var_with_lock(char* ptr, char value) {
    __asm__ volatile(
        "movq %0, %%rbx\n"
        "mov %1, %%al\n"
        "xchg %%al, (%%rbx)"
        :
    : "r" (ptr), "r" (value)
        : "%rax", "%rbx", "memory"
        );
}

// void modify_var_with_lock(unsigned long* ptr, unsigned long value) {
//     __asm__ volatile(
//         "movq %0, %%rbx\n"
//         "mov %1, %%rax\n"
//         "xchg %%rax, (%%rbx)"
//         :
//     : "r" (ptr), "r" (value)
//         : "%rax", "%rbx", "memory"
//         );
// }

int add_int3_in_lock(unsigned long vaddr) {
    struct page* pages[1];
    struct nf_call_inst* call_inst = get_call_inst(vaddr);
    int ret;
    ret = get_user_pages_fast(vaddr & PAGE_MASK, 1, FOLL_WRITE | FOLL_FORCE, pages);
    if (ret < 0) {
        pr_err("get user pages failed: %d\n", ret);
        return ret;
    }

    // preempt_disable();
    char* buffer = kmap(pages[0]);
    pr_info("get userspace data: %hhx %hhx %hhx %hhx %hhx", buffer[vaddr & (~PAGE_MASK)], buffer[(vaddr & (~PAGE_MASK)) + 1], \
        buffer[(vaddr & (~PAGE_MASK)) + 2], buffer[(vaddr & (~PAGE_MASK)) + 3], buffer[(vaddr & (~PAGE_MASK)) + 4]);
    pr_info("get userspace data: %lx", *((long*)(buffer + (vaddr & (~PAGE_MASK)))));
    int* offset = (int*)(buffer + (vaddr & (~PAGE_MASK)) + 1);
    call_inst->offset = *offset;

    // pr_info("get offset %d\n", call_inst->offset);

    char* ptr = buffer + (vaddr & (~PAGE_MASK));
    // modify_var_with_lock(ptr, 0xcc);
    for (int i = 0;i < 5;i++)
        modify_var_with_lock(ptr + i, 0xcc);

    // unsigned long* ptr = (unsigned long*)(buffer + (vaddr & (~PAGE_MASK)));
    // unsigned long old = *ptr;
    // unsigned long new = (old & ~0xFFFFFFFFFF) | 0x90909090cc;
    // modify_var_with_lock(ptr, new);


    // if (cmpxchg64(ptr, old, new) != old)
    //     pr_info("[add_int3_in_lock] comxchg fail");

    // buffer[vaddr & (~PAGE_MASK)] = 0x90;
    // buffer[(vaddr & (~PAGE_MASK)) + 1] = 0x90;
    // buffer[(vaddr & (~PAGE_MASK)) + 2] = 0x90;
    // buffer[(vaddr & (~PAGE_MASK)) + 3] = 0x90;
    // buffer[(vaddr & (~PAGE_MASK)) + 4] = 0xcc;

    pr_info("get userspace data: %hhx %hhx %hhx %hhx %hhx", buffer[vaddr & (~PAGE_MASK)], buffer[(vaddr & (~PAGE_MASK)) + 1], \
        buffer[(vaddr & (~PAGE_MASK)) + 2], buffer[(vaddr & (~PAGE_MASK)) + 3], buffer[(vaddr & (~PAGE_MASK)) + 4]);
    pr_info("get userspace data: %lx", *((long*)(buffer + (vaddr & (~PAGE_MASK)))));
    kunmap(pages[0]);
    put_page(pages[0]);
    stac();
    clflush_cache_range(vaddr, 64);
    // preempt_enable();
    return 0;
}

int count_uaddr_cnt(unsigned long uaddr) {
    // if (int3_changed == 0) {
    //     if (spin_trylock(&int3_change_lock)) {

    //         // for (int i = 0;i < sizeof(call_addresses) / sizeof(unsigned long);i++)
    //         //     add_int3_in_lock(call_addresses[i] - 5);

    //         // add_int3_in_lock(call_addresses[0] - 5);

    //         // add_int3_in_lock(0x55555555568b);

    //         int3_changed = 1;
    //         spin_unlock(&int3_change_lock);
    //     }
    // }

    if (atomic_long_read(&start_work_cycle) == 0) {
        atomic_long_cmpxchg(&start_work_cycle, 0, rdtsc_ordered());
        return 0;
    }
    else if (rdtsc_ordered() - atomic_long_read(&start_work_cycle) < 1000000000)
        return 0;

    struct nf_uaddr_state* u_state = get_uaddr_state(uaddr);
    // if (uaddr == 0x555555592bc0) {
    //     u_state->state = uaddr_ignore;
    //     return 0;
    // }

    if (u_state->state == uaddr_delegate)
        return 1;
    if (u_state->state == uaddr_ignore) {
        u_state->cnt++;
        if (u_state->cnt == 10000)
            u_state->end = rdtsc_ordered();
        return 0;
    }
    u_state->cnt++;
    if (u_state->state == uaddr_init) {
        u_state->state = uaddr_checking;
        u_state->begin = rdtsc_ordered();
        return 0;
    }
    if (u_state->state == uaddr_checking) {
        u_state->end = rdtsc_ordered();
        if (u_state->end - u_state->begin >= atomic_long_read(&count_uaddr_time)) {
            u_state->state = uaddr_ignore;
            return 0;
        }
        else if (u_state->cnt == 10000) {
            pr_info("uaddr %lx begin delegate", uaddr);
            atomic_set_release(&u_state->is_delegating, 0);
            atomic_set_release(&u_state->numa_info[0].is_delegating, 0);
            atomic_set_release(&u_state->numa_info[1].is_delegating, 0);
            u_state->state = uaddr_delegate;
            unsigned long old = atomic_long_read(&count_uaddr_time);
            unsigned long new = old;
            while (new / (u_state->end - u_state->begin) >= 10)
                new = new / 10;
            if (new != old)
                atomic_long_cmpxchg(&count_uaddr_time, old, new);
            return 1;
        }
    }
    return 0;
}

int check_uaddr_state(unsigned long uaddr) {
    struct nf_uaddr_state* u_state = get_uaddr_state(uaddr);
    if (u_state->state == uaddr_delegate)
        return 1;
    return 0;
}

#define __set_task_state(task, state_value)				\
	do {								\
		debug_normal_state_change((state_value));		\
		WRITE_ONCE((task)->__state, (state_value));		\
	} while (0)

#define set_task_state(task, state_value)					\
	do {								\
		debug_normal_state_change((state_value));		\
		smp_store_mb((task)->__state, (state_value));		\
	} while (0)

const struct futex_q futex_q_init = {
    /* list gets initialized in futex_queue()*/
    .key = FUTEX_KEY_INIT,
    .bitset = FUTEX_BITSET_MATCH_ANY,
    .requeue_state = ATOMIC_INIT(Q_REQUEUE_PI_NONE),
};

void __nf_futex_queue(struct futex_q* q, struct futex_hash_bucket* hb, struct task_struct* task) {
    int prio;
    prio = min(task->normal_prio, MAX_RT_PRIO);

    plist_node_init(&q->list, prio);
    plist_add(&q->list, &hb->chain);
    q->task = task;
}

void print_regs(struct pt_regs* regs) {
    pr_info("regs: %lx, %lx, %lx, %lx, %lx, %lx, %lx, %lx, %lx, %lx, %lx, %lx, %lx, %lx, %lx, %lx, %lx\n", \
        regs->ip, regs->sp, regs->bp, regs->ax, regs->bx, regs->cx, regs->dx, regs->si, regs->di, regs->r8, regs->r9, \
        regs->r10, regs->r11, regs->r12, regs->r13, regs->r14, regs->r15);
}

void add_uaddr(unsigned long u) {
    int i;
    for (i = 0;i < uaddr_cnt;i++)
        if (uaddrs[i] == u)
            break;
    if (i == 102400)
        return;
    if (i == uaddr_cnt) {
        uaddr_cnt++;
        uaddrs[i] = u;
        uaddr_cnts[i] = 0;
    }
    uaddr_cnts[i]++;
}

void record_ret(int ret) {
    if (ret >= 0 && ret <= NF_CNT_NUM)
        nf_wake_rets[ret]++;
    else if (ret > NF_CNT_NUM)
        nf_wake_rets[NF_CNT_NUM + 1]++;
}

int check_numa_node(struct futex_q* q, int node) {
    int q_node;
    q_node = task_node(q->task);
    if (q_node == node)
        return 1;
    else
        return 0;
}

void nf_call_q(struct wake_q_head* wake_q, struct exe_ret* r) {
    struct wake_q_node* node = wake_q->first;
    struct pt_regs* regs;
    r->ret = -11;
    while (node != WAKE_Q_TAIL) {
        struct task_struct* task;
        task = container_of(node, struct task_struct, wake_q);
        regs = task_pt_regs(task);
        // prefetch((void*)regs);
        // unsigned long rsp = regs->sp;
        // int i;
        // for (i = 0;i < 64;i++)
        //     prefetch((void*)(rsp + 64 * i));
        // for (i = 0;i < 64;i++)
        //     prefetch((void*)(rsp - 64 * i));
        exe_on_call(task, regs, r);
        node = node->next;
    }
}

void nf_call(struct task_struct* task, struct exe_ret* r) {
    struct pt_regs* regs;
    unsigned long rsp;
    exe_on_call(task, regs = task_pt_regs(task), r);
}

void nf_prefetch_context(struct pt_regs* regs, unsigned long rsp) {
    stac();
    int i;
    nf_prefetchw(regs);
    nf_prefetchw((char*)regs + 64);
    nf_prefetchw((char*)regs + 128);

    for (i = 0;i < prefetch_up_size;i++)
        nf_prefetchw((char*)rsp + 64 * i);
    for (i = 1;i <= prefetch_down_size;i++)
        nf_prefetchw((char*)rsp - 64 * i);
}

void nf_cldemote_context(struct pt_regs* regs, unsigned long rsp) {
    stac();
    int i;
    // nf_cldemote(regs);
    // nf_cldemote((char*)regs + 64);
    // nf_cldemote((char*)regs + 128);
    for (i = 0;i < prefetch_up_size;i++)
        nf_cldemote((char*)rsp + 64 * i);
    for (i = 1;i <= prefetch_down_size;i++)
        nf_cldemote((char*)rsp - 64 * i);
}

void not_wake_up_q(struct wake_q_head* wake_q) {
    struct wake_q_node* node = wake_q->first;

    while (node != WAKE_Q_TAIL) {
        struct task_struct* task;
        task = container_of(node, struct task_struct, wake_q);
        /* Task can safely be re-inserted now: */
        node = node->next;
        task->wake_q.next = NULL;
        put_task_struct(task);
    }
}

long new_futex_wait(u32 __user* uaddr, unsigned int flags, u32 val, ktime_t* abs_time, u32 bitset) {
    struct hrtimer_sleeper timeout, * to;
    struct restart_block* restart;
    struct futex_hash_bucket* hb;
    struct futex_q q = futex_q_init;
    long ret, cpu_mask, old, new;
    u32 uval;
    int is_nf_need_add = 1, i;
    int cpu_id, numa_id, cpu_index, temp, timeout_time;
    unsigned long long begin, t, end, start, cur_ts, check_ts;
    struct nf_delegate_complete* completes;
    volatile atomic_long_t* queue;
    volatile atomic_t* state;
    struct cpumask* mask;
    struct pt_regs* regs;
    unsigned long before_rip, after_rip, wait_cost;
    struct nf_delegate_uaddrnode* uaddrnode;
    struct nf_uaddr_state* u_state;

    if (!check_uaddr_state((unsigned long)uaddr))
        return origin_futex_wait(uaddr, flags, val, abs_time, bitset);
    // pr_info("%lx", uaddr);
    if (abs_time != NULL)
        printk(KERN_INFO "wait time is not NULL\n");

    regs = task_pt_regs(current);
    before_rip = regs->ip;

    if (!bitset) return -EINVAL;
    q.bitset = bitset;

    // stac();
    // nf_cldemote((void*)uaddr);
    // nf_cldemote((char*)uaddr + 64);

    if (is_nf_need_add)
        exe_add(current->pid, regs->ip);

    stac();

    to = futex_setup_timer(abs_time, &timeout, flags, current->timer_slack_ns);

    if (to)
        printk(KERN_INFO "wait to is not null\n");

retry:
    /*
 * Prepare to wait on uaddr. On success, it holds hb->lock and q
 * is initialized.
 */
futex_wait_setup_retry:
    ret = get_futex_key(uaddr, flags & FLAGS_SHARED, &q.key, FUTEX_READ);
    if (unlikely(ret != 0))
        goto out;

futex_wait_setup_retry_private:
    hb = futex_hash(&q.key);
    // futex_hb_waiters_inc(hb);
    q.lock_ptr = &hb->lock;


    if (velvet_wait_queue == 1) {

        // numa_id = 0;
        // cpu_id = smp_processor_id();
        cpu_id = 0;
        numa_id = numa_node_id();
        mask = cpumask_of_node(numa_id);
        cpu_index = smp_processor_id();
        for_each_cpu(i, mask) {
            if (i == cpu_index)
                break;
            cpu_id++;
        }

        uaddrnode = get_uaddrnode((unsigned long)uaddr, nf_numanodes[numa_id]->uaddr_node);
        completes = uaddrnode->completes;
        queue = &uaddrnode->queue.q;
        state = &completes[cpu_id].state;
        u_state = get_uaddr_state(uaddr);

        // nf_cldemote((void*)uaddrnode);

        if (completes[cpu_id].task != current)
            completes[cpu_id].task = current;
        if (completes[cpu_id].regs != regs)
            completes[cpu_id].regs = regs;
        if (completes[cpu_id].rsp != regs->sp)
            completes[cpu_id].rsp = regs->sp;
        if (completes[cpu_id].q != &q)
            completes[cpu_id].q = &q;
        if (completes[cpu_id].val != val)
            completes[cpu_id].val = val;

        stac();
        // nf_cldemote_context(regs, regs->sp);
        nf_cldemote((void*)regs);
        nf_cldemote((char*)regs + 64);
        nf_cldemote((char*)regs + 128);
        // nf_cldemote((char*)completes + 64);

        // stac();
        // nf_cldemote((void*)(&completes[cpu_id]) + 64);

        //retrylock:
        // atomic_inc(&normal_wake_cnt);
        timeout_time = 0;

        // unsigned long delegation_begin_time = rdtsc_ordered();

        int is_timeout = 0;
        int no_delegate_cnt = 0;
        atomic_set(state, fast_path_ready);

        if (test_and_set_bit(cpu_id, (volatile unsigned long*)queue))
            pr_info("[new_futex_wait]\tfast path q start value wrong");
        nf_cldemote((void*)queue);

        start = rdtsc_ordered();
        check_ts = start;
        preempt_disable();
        while (atomic_read(state) == fast_path_ready) {
            cpu_relax();
            mb();
            cur_ts = rdtsc_ordered();
            // if (cur_ts - check_ts > 10000) {
            //     if (atomic_read_acquire(&u_state->is_delegating) == 0) {
            //         no_delegate_cnt++;
            //         if (no_delegate_cnt >= 3) {
            //             if (test_and_clear_bit(cpu_id, (volatile unsigned long*)queue)) {
            //                 atomic_set_release(state, fast_path_timeout);
            //                 preempt_enable();
            //                 is_timeout = 1;
            //             }
            //             break;
            //         }
            //     }
            //     else {
            //         no_delegate_cnt = 0;
            //     }
            //     check_ts = cur_ts;
            // }
            if (cur_ts - start >= 1024000) {
                if (test_and_clear_bit(cpu_id, (volatile unsigned long*)queue)) {
                    atomic_set_release(state, fast_path_timeout);
                    // nf_cldemote((void*)queue);
                    preempt_enable();
                    is_timeout = 1;
                    // timeout_time = timeout_time < 7 ? timeout_time + 1 : timeout_time;
                    // timeout_time++;
                }
                break;
            }
        }
        if (!is_timeout) {
            while (atomic_read_acquire(state) == fast_path_ready);

            preempt_enable();
            switch (atomic_read(state)) {
            case fast_path_success:
                // atomic_inc(&nf_com);
                smp_store_release(&q.lock_ptr, NULL);
                atomic_set(state, fast_path_uninit);
                ret = 0;
                after_rip = regs->ip;

                // unsigned long delegation_interval = rdtsc_ordered() - delegation_begin_time;
                // if (delegation_interval < 3000UL)
                //     ++cycle_cnts[cpu_index].cnt[delegation_interval / 100UL];
                // else
                //     ++cycle_cnts[cpu_index].cnt[29];
                // cycle_cnts[cpu_index].min = cycle_cnts[cpu_index].min < delegation_interval ? cycle_cnts[cpu_index].min : delegation_interval;
                // cycle_cnts[cpu_index].total += delegation_interval;
                // ++cycle_cnts[cpu_index].total_cnt;

                goto out;
            case fast_path_no_need_to_sleep:
                // atomic_inc(&nf_wake_state);
                // futex_hb_waiters_dec(hb);
                ret = -EWOULDBLOCK;
                atomic_set(state, fast_path_uninit);
                // nf_cldemote((void*)state);
                goto out;
            case fast_path_fail:
                // atomic_inc(&nf_d_lock);
                atomic_set(state, fast_path_uninit);
                // nf_cldemote((void*)state);
                goto fast_path_sleep;
            default:
                pr_info("[new_futex_wait] state return wrong value %d", atomic_read(state));
            }
        }
        // }
        mb();
        atomic_set(state, fast_path_uninit);
        // nf_cldemote((void*)state);
        spin_lock(&hb->lock);
    }
    else {
        stac();
        nf_cldemote((void*)regs);
        nf_cldemote((char*)regs + 64);
        nf_cldemote((char*)regs + 128);

        spin_lock(&hb->lock);
    }

    // atomic_inc(&nf_d_lock);
    ret = futex_get_value_locked(&uval, uaddr);
    if (ret) {
        futex_q_unlock(hb);
        ret = get_user(uval, uaddr);
        if (ret)
            goto out;
        if (!(flags & FLAGS_SHARED))
            goto futex_wait_setup_retry_private;
        goto futex_wait_setup_retry;
    }
    if (uval != val) {
        futex_q_unlock(hb);
        ret = -EWOULDBLOCK;
        goto out;
    }
    /* futex_queue and wait for wakeup, timeout, or a signal. */

    // futex_wait_queue(hb, &q, to);
    set_current_state(TASK_INTERRUPTIBLE | TASK_FREEZABLE);
    __futex_queue(&q, hb);
    spin_unlock(&hb->lock);

fast_path_sleep:
    futex_hb_waiters_inc(hb);
    if (to)
        hrtimer_sleeper_start_expires(to, HRTIMER_MODE_ABS);
    if (likely(!plist_node_empty(&q.list))) {
        if (!to || to->task)
            schedule();
    }
    __set_current_state(TASK_RUNNING);

    /* If we were woken (and unqueued), we succeeded, whatever. */
    mb();
    ret = 0;
    after_rip = regs->ip;

    if (!futex_unqueue(&q)) goto out;

    pr_info("[new_futex_wait] slow path not go out");

    ret = -ETIMEDOUT;
    if (to && !to->task) goto out;

    /*
     * We expect signal_pending(current), but we might be the
     * victim of a spurious wakeup as well.
     */
    if (!signal_pending(current)) goto retry;

    ret = -ERESTARTSYS;
    if (!abs_time) goto out;

    restart = &current->restart_block;
    restart->futex.uaddr = uaddr;
    restart->futex.val = val;
    restart->futex.time = *abs_time;
    restart->futex.bitset = bitset;
    restart->futex.flags = flags | FLAGS_HAS_TIMEOUT;

    ret = set_restart_fn(restart, futex_wait_restart);

out:
    if (to) {
        hrtimer_cancel(&to->timer);
        destroy_hrtimer_on_stack(&to->timer);
    }
    // if (ret != 0 && ret != -11)
    //     printk(KERN_INFO "wait ret %lx", (long)ret);
    if (is_nf_need_add && ret == 0 && before_rip != after_rip) {
        ret = regs->ax;
    }
    return ret;
}

int __nf_do_int3(struct pt_regs* regs) {
    struct nf_uaddr_state* u_state = get_uaddr_state((unsigned long)(regs->di));
    int cpu_id = smp_processor_id();
    // atomic_inc(&normal_wake_cnt);
    if (u_state->state == uaddr_delegate) {
        preempt_disable();
        // atomic_inc(&nf_d_lock);

        // if (atomic_read(&u_state->is_delegating) == 1)
        //     atomic_inc(&nf_com);

        // unsigned long delegation_begin_time = rdtsc_ordered();

        while (atomic_read_acquire(&u_state->is_delegating) == 1) {
            cpu_relax();
        }

        // unsigned long delegation_interval = rdtsc_ordered() - delegation_begin_time;
        // if (delegation_interval < 3000UL)
        //     ++cycle_cnts[cpu_id].cnt[delegation_interval / 100UL];
        // else
        //     ++cycle_cnts[cpu_id].cnt[29];
        // cycle_cnts[cpu_id].min = cycle_cnts[cpu_id].min < delegation_interval ? cycle_cnts[cpu_id].min : delegation_interval;
        // cycle_cnts[cpu_id].total += delegation_interval;
        // ++cycle_cnts[cpu_id].total_cnt;

        preempt_enable();
    }

    struct nf_call_inst* call_inst = get_call_inst(regs->ip - 1);
    regs->sp -= 8;
    stac();
    *((unsigned long*)(regs->sp)) = regs->ip + 4;
    regs->ip += call_inst->offset + 4;

    // unsigned int wait_cycle = nf_random() % 1000;
    // unsigned long begin = rdtsc_ordered();
    // while (rdtsc_ordered() - begin < wait_cycle);

    // pr_info("int3 return ip %lx", regs->ip);
    // regs->ip -= 58802;

    return 1;
}

unsigned long delegation_end_time = 0;

int new_futex_wake(u32 __user* uaddr, unsigned int flags, int nr_wake, u32 bitset) {
    struct futex_hash_bucket* hb;
    struct futex_q* this, * next, * nf_wake_targets;
    union futex_key key = FUTEX_KEY_INIT;
    int i, j, temp, w_index, t_index, ret = 0, pos[64], wake_pos[64], \
        numa_id, numa_nodes, cpu_id, cpu_index, timeout_time, nf_wake_total;
    int need_wake_nr = nr_wake;
    struct exe_ret e_ret;
    unsigned int wake_id, ran;
    long old, new, cnt, wake_cnt = 0, waker_cnt = 0, need_e_queue = 1;
    struct nf_delegate_complete* completes;
    volatile atomic_long_t* queue, * wake_queue;
    volatile atomic_t* state;
    struct task_struct* wake_task;
    struct nf_delegate_uaddrnode* uaddrnode;
    struct pt_regs* target_regs;
    struct cpumask* mask;
    struct nf_uaddr_state* u_state;
    u32 uval;
    unsigned long begin, end, start, t;
    unsigned long delegation_interval = 0;

    if (!check_uaddr_state((unsigned long)uaddr))
        return origin_futex_wake(uaddr, flags, nr_wake, bitset);

    numa_id = numa_node_id();
    numa_nodes = 2;
    cpu_id = 0;
    cpu_index = smp_processor_id();
    mask = cpumask_of_node(numa_id);
    for_each_cpu(i, mask) {
        if (i == cpu_index)
            break;
        cpu_id++;
    }

    if (!bitset) {
        printk(KERN_INFO "nf wake bitset is NULL\n");
        ret = -EINVAL;
        goto out;
    }

    ret = get_futex_key(uaddr, flags & FLAGS_SHARED, &key, FUTEX_READ);
    if (unlikely(ret != 0)) {
        printk(KERN_INFO "nf wake can not get futex key\n");
        goto out;
    }
    hb = futex_hash(&key);

    // if (!futex_hb_waiters_pending(hb))
    //     return ret;

    uaddrnode = get_uaddrnode((unsigned long)uaddr, nf_numanodes[numa_id]->uaddr_node);
    completes = uaddrnode->completes;
    queue = &uaddrnode->queue.q;
    state = &completes[cpu_id].state;
    u_state = get_uaddr_state(uaddr);

    if (velvet_wait_queue == 1) {

        for (i = 0;i < 32;i++) {
            prefetch((char*)(completes + i) + 256);
        }

    }

    if (delegate_election == 1) {
        // i = 0;
        // while (atomic_read_acquire(&u_state->wake_numa) != numa_id) {
        //     cpu_relax();
        //     if (i++ > 1000000000) {
        //         break;
        //     }
        // }

        wake_queue = &uaddrnode->wake_queue.q;
        // nf_cldemote((void*)uaddrnode);
        timeout_time = 0;
        // while (atomic_read_acquire(&u_state->is_delegating) == 1 || !spin_trylock(&hb->lock)) {
        while (atomic_cmpxchg(&u_state->numa_info[numa_id].is_delegating, 0, 1) == 1) {
            int is_timeout = 0;
            atomic_set(state, fast_wake_ready);
            if (test_and_set_bit(cpu_id, (volatile unsigned long*)wake_queue))
                pr_info("[new_futex_wake]\tfast path q start value wrong");
            // nf_cldemote((void*)wake_queue);

            start = rdtsc_ordered();
            preempt_disable();
            while (atomic_read(state) == fast_wake_ready) {
                cpu_relax();
                mb();
                if (rdtsc_ordered() - start >= fast_path_wake_time[timeout_time]) {
                    if (test_and_clear_bit(cpu_id, (volatile unsigned long*)wake_queue)) {
                        atomic_set_release(state, fast_wake_timeout);
                        // nf_cldemote((void*)wake_queue);
                        preempt_enable();
                        is_timeout = 1;
                        timeout_time = timeout_time < 6 ? timeout_time + 1 : timeout_time;
                    }
                    break;
                }
            }
            if (!is_timeout) {
                while (atomic_read_acquire(state) == fast_wake_ready);
                preempt_enable();
                if (atomic_read(state) == fast_wake_success) {
                    atomic_set(state, fast_path_uninit);
                    // nf_cldemote((void*)state);
                    return 0;
                }
                else if (atomic_read(state) == fast_to_slow_wake) {
                    atomic_set(state, fast_path_uninit);
                    spin_lock(&hb->lock);
                    goto delegation_phase;
                }
                else
                    pr_info("[new_futex_wake] state return wrong value %d", atomic_read(state));
            }
        }
        atomic_set(state, fast_path_uninit);

        // wake_cnt = 0;
        // old = atomic_long_xchg(wake_queue, 0UL);
        // // nf_cldemote((void*)wake_queue);
        // if (old != 0) {
        //     for (i = 0;i < 64;i++)
        //         if ((old & (1UL << i))) {
        //             wake_pos[wake_cnt++] = i;
        //             // atomic_set_release(&completes[i].state, fast_wake_success);
        //             // nf_cldemote((void*)(&completes[i].state));
        //         }
        //     for (i = 0;i < wake_cnt;i++) {
        //         atomic_set_release(&completes[wake_pos[i]].state, fast_wake_success);
        //     }
        // }
        // need_wake_nr += wake_cnt - 1;
        // waker_cnt += wake_cnt;

        while (atomic_cmpxchg(&u_state->is_delegating, 0, 1) == 1) {
            cpu_relax();
            // if (need_e_queue == 1 && waker_cnt < NF_E_QUEUE_NUM) {
            //     wake_cnt = 0;
            //     old = atomic_long_xchg(wake_queue, 0UL);
            //     if (old != 0) {
            //         for (i = 0;i < 64;i++)
            //             if ((old & (1UL << i))) {
            //                 wake_pos[wake_cnt++] = i;
            //             }
            //         for (i = 0;i < wake_cnt;i++) {
            //             atomic_set_release(&completes[wake_pos[i]].state, fast_wake_success);
            //         }
            //     }
            //     else
            //         need_e_queue = 0;
            //     need_wake_nr += wake_cnt - 1;
            //     waker_cnt += wake_cnt;
            // }
        }
        // atomic_set_release(&u_state->is_delegating, 1);
        // spin_unlock(&hb->lock);
        // atomic_set_release(&u_state->wake_numa, (numa_id + 1) % numa_nodes);
        // nf_cldemote((void*)state);

    }
    else {
        // nf_cldemote((void*)uaddrnode);
        spin_lock(&hb->lock);
        // record_ret(futex_hb_waiters_pending(hb));
    }

delegation_phase:
    // unsigned long delegation_begin_time = rdtsc_ordered();

    // if (delegation_end_time > 0) {
    //     delegation_interval = rdtsc_ordered() - delegation_end_time;
    //     if (delegation_interval < 3000UL)
    //         ++cycle_cnts[cpu_id].cnt[delegation_interval / 100UL];
    //     else
    //         ++cycle_cnts[cpu_id].cnt[29];
    //     cycle_cnts[cpu_id].min = cycle_cnts[cpu_id].min < delegation_interval ? cycle_cnts[cpu_id].min : delegation_interval;
    //     cycle_cnts[cpu_id].total += delegation_interval;
    //     ++cycle_cnts[cpu_id].total_cnt;
    // }



    // stac();
    // nf_prefetchw((void*)uaddr);
    // nf_prefetchw((char*)uaddr + 64);


    nf_wake_total = 0;
    int nf_wake_cnt = 0;
    int nf_need_end = 0;
    int int3_hook = 0;
    int need_vw_queue = 1;
    cnt = 0;
    w_index = 0;
    int p_index = 0;
    int empty_cnt = 0;
    int one_dequeue_cnt = 0;
    int temp_pos[64] = { 0 };

    e_ret.number = 0;

    while (1) {
        e_ret.count = 0;
        ret = 0;

        if (delegate_election == 1 && need_e_queue == 1 && waker_cnt < NF_E_QUEUE_NUM) {
            wake_cnt = 0;
            old = atomic_long_xchg(wake_queue, 0UL);
            if (old != 0) {
                for (i = 0;i < 64;i++)
                    if ((old & (1UL << i))) {
                        wake_pos[wake_cnt++] = i;
                    }
                for (i = 0;i < wake_cnt;i++) {
                    atomic_set_release(&completes[wake_pos[i]].state, fast_wake_success);
                }
            }
            else
                need_e_queue = 0;
            need_wake_nr += wake_cnt - 1;
            waker_cnt += wake_cnt;
        }


        if (velvet_wait_queue == 1) {

            if (!nf_need_end && need_vw_queue == 1 && cnt < NF_FAST_PATH_WAKE_NUM && w_index == cnt) {
            vw_queue_retry:
                one_dequeue_cnt = 0;
                old = atomic_long_xchg(queue, 0UL);
                if (old != 0) {
                    while (old) {
                        temp_pos[one_dequeue_cnt++] = __builtin_ctzl(old);
                        old &= old - 1;
                    }

                    // for (i = 0;i < 64;i++) {
                    //     if ((old & (1UL << i))) {
                    //         temp_pos[one_dequeue_cnt++] = i;
                    //     }
                    // }

                    // for (j = one_dequeue_cnt - 1;j > 0;j--) {
                    //     ran = nf_random() % (j + 1);
                    //     temp = temp_pos[j];
                    //     temp_pos[j] = temp_pos[ran];
                    //     temp_pos[ran] = temp;
                    // }
                    // if (one_dequeue_cnt == 1)
                    //     need_vw_queue = 0;

                    ran = nf_random() % 2;
                    if (ran == 0)
                        for (j = 0;j < one_dequeue_cnt;j++)
                            pos[cnt++] = temp_pos[j];
                    else
                        for (j = one_dequeue_cnt - 1;j >= 0;j--)
                            pos[cnt++] = temp_pos[j];
                }
                else {
                    // empty_cnt++;
                    // if (empty_cnt < 20)
                    //     goto vw_queue_retry;
                    // else
                    need_vw_queue = 0;
                }

            }
        }

        for (;p_index < cnt && p_index < w_index + 2;p_index++)
            nf_prefetch_context(completes[pos[p_index]].regs, completes[pos[p_index]].rsp);

        if (!nf_need_end) {
            if (w_index < cnt) {

                e_ret.ret = -14;
                nf_wake_cnt++;

                // if (nf_wake_cnt == 16) {
                //     atomic_set_release(&u_state->is_delegating, 0);
                // }

                // ran = nf_random();
                // t_index = w_index + ran % (cnt - w_index);
                // temp = pos[t_index];
                // pos[t_index] = pos[w_index];
                // pos[w_index] = temp;

                // nf_prefetch_context(completes[pos[w_index]].regs, completes[pos[w_index]].rsp);



                exe_on_call(completes[pos[w_index]].task, completes[pos[w_index]].regs, &e_ret);



                if (e_ret.ret < -1)
                    nf_need_end = 1;
                else if (e_ret.ret == 1) {
                    nf_wake_total++;
                    need_wake_nr++;
                }
                else if (e_ret.ret == 0) {
                    need_wake_nr--;
                }

                // unsigned long delegation_begin_time = rdtsc_ordered();
                // futex_hb_waiters_dec(hb);
                // delegation_interval += rdtsc_ordered() - delegation_begin_time;

                atomic_set_release(&completes[pos[w_index]].state, fast_path_success);


                w_index++;



            }
            else {
                // atomic_inc(&normal_wake_cnt);
                DEFINE_WAKE_Q(wake_q);
                if (delegate_election == 1)
                    spin_lock(&hb->lock);
                e_ret.ret = -15;
                ret = 0;

                plist_for_each_entry_safe(this, next, &hb->chain, list) {
                    if (futex_match(&this->key, &key)) {
                        if (this->pi_state || this->rt_waiter) {
                            ret = -EINVAL;
                            printk(KERN_INFO "wake error\n");
                            goto out;
                        }
                        if (!(this->bitset & bitset))
                            continue;
                        if (!check_numa_node(this, numa_id))
                            continue;
                        nf_wake_targets = this;
                        target_regs = task_pt_regs(nf_wake_targets->task);
                        nf_prefetch_context(target_regs, target_regs->sp);
                        if (++ret >= 1)
                            break;
                    }
                }

                // if (ret == 0) {
                //     plist_for_each_entry_safe(this, next, &hb->chain, list) {
                //         if (futex_match(&this->key, &key)) {
                //             if (this->pi_state || this->rt_waiter) {
                //                 ret = -EINVAL;
                //                 printk(KERN_INFO "wake error\n");
                //                 goto out;
                //             }
                //             if (!(this->bitset & bitset))
                //                 continue;
                //             nf_wake_targets = this;
                //             target_regs = task_pt_regs(nf_wake_targets->task);
                //             nf_prefetch_context(target_regs, target_regs->sp);
                //             if (++ret >= 1)
                //                 break;
                //         }
                //     }
                // }
                if (ret == 1) {
                    if (nf_wake_cnt < cnt + NF_SLOW_PATH_WAKE_NUM) {
                        nf_wake_cnt++;
                        exe_on_call(nf_wake_targets->task, task_pt_regs(nf_wake_targets->task), &e_ret);
                    }
                    futex_wake_mark(&wake_q, nf_wake_targets);
                }

                if (e_ret.ret < 0)
                    nf_need_end = 1;
                else if (e_ret.ret == 1) {
                    nf_wake_total++;
                    need_wake_nr++;
                }
                else if (e_ret.ret == 0)
                    need_wake_nr--;
                if (delegate_election == 1)
                    spin_unlock(&hb->lock);
                wake_up_q(&wake_q);
            }
        }
        else {
            // for (i = 0;i < w_index;i++)
            //     atomic_set_release(&completes[pos[i]].state, fast_path_success);

            // atomic_inc(&normal_wake_cnt);

            for (;w_index < cnt;w_index++) {
                ret = futex_get_value_locked(&uval, uaddr);
                if (ret)
                    pr_info("[new_futex_wake] futex_get_value_locked error");
                if (uval != completes[pos[w_index]].val) {
                    atomic_set_release(&completes[pos[w_index]].state, fast_path_no_need_to_sleep);
                }
                else {
                    if (need_wake_nr > 0) {
                        atomic_set_release(&completes[pos[w_index]].state, fast_path_no_need_to_sleep);
                        need_wake_nr--;
                    }
                    else {
                        if (delegate_election == 1)
                            spin_lock(&hb->lock);
                        set_task_state(completes[pos[w_index]].task, TASK_INTERRUPTIBLE | TASK_FREEZABLE);
                        __nf_futex_queue(completes[pos[w_index]].q, hb, completes[pos[w_index]].task);
                        atomic_set_release(&completes[pos[w_index]].state, fast_path_fail);
                        if (delegate_election == 1)
                            spin_unlock(&hb->lock);
                    }
                }
            }



            break;
        }
    }
    // record_ret(cnt);

    atomic_set_release(&u_state->is_delegating, 0);
    atomic_set_release(&u_state->numa_info[numa_id].is_delegating, 0);

    if (delegate_election == 1)
        spin_lock(&hb->lock);
    DEFINE_WAKE_Q(wake_q);
    ret = 0;
    plist_for_each_entry_safe(this, next, &hb->chain, list) {
        if (futex_match(&this->key, &key)) {
            if (this->pi_state || this->rt_waiter) {
                ret = -EINVAL;
                printk(KERN_INFO "wake error\n");
                goto out;
            }
            if (!(this->bitset & bitset))
                continue;

            futex_wake_mark(&wake_q, this);
            // nf_wake_cnt++;
            if (++ret >= need_wake_nr)
                break;
        }
    }
    // record_ret(ret);
    wake_up_q(&wake_q);
    spin_unlock(&hb->lock);


    // for (i = 0;i < 64;i++) {
    //     wake_task = completes[i].task;
    //     if (wake_task != NULL && wake_task->exit_state != EXIT_DEAD && wake_task->exit_state != EXIT_ZOMBIE)
    //         nf_cldemote_context(completes[i].regs, completes[i].rsp);
    // }
    // for (i = 63;i >= 0;i--) {
    //     nf_cldemote((char*)(&completes[i]) + 64);
    // }
    // record_ret(atomic_read(&int3_waiter_cnt));
    // delegation_interval = delegation_end_time - delegation_begin_time;



    // unsigned long delegation_begin_time = rdtsc_ordered();



    // delegation_end_time = rdtsc_ordered();

    // delegation_interval = rdtsc_ordered() - delegation_begin_time;
    // if (delegation_interval < 3000UL)
    //     ++cycle_cnts[cpu_id].cnt[delegation_interval / 100UL];
    // else
    //     ++cycle_cnts[cpu_id].cnt[29];
    // cycle_cnts[cpu_id].min = cycle_cnts[cpu_id].min < delegation_interval ? cycle_cnts[cpu_id].min : delegation_interval;
    // cycle_cnts[cpu_id].total += delegation_interval;
    // ++cycle_cnts[cpu_id].total_cnt;

    ret = nf_wake_cnt;
    // record_ret(nf_wake_cnt);

out:
    // printk(KERN_INFO "wake ret %d", ret);
    // atomic_set(&nf_d_lock, 0);
    return ret;
}

long origin_futex_wait(u32 __user* uaddr, unsigned int flags, u32 val, ktime_t* abs_time, u32 bitset) {
    struct hrtimer_sleeper timeout, * to;
    struct restart_block* restart;
    struct futex_hash_bucket* hb;
    struct futex_q q = futex_q_init;
    long ret;
    u32 uval;

    // add_uaddr((unsigned long)uaddr);

    if (!bitset)
        return -EINVAL;
    q.bitset = bitset;

    to = futex_setup_timer(abs_time, &timeout, flags,
        current->timer_slack_ns);
retry:
    /*
     * Prepare to wait on uaddr. On success, it holds hb->lock and q
     * is initialized.
     */
    ret = futex_wait_setup(uaddr, val, flags, &q, &hb);
    if (ret)
        goto out;

    /* futex_queue and wait for wakeup, timeout, or a signal. */
    futex_wait_queue(hb, &q, to);

    /* If we were woken (and unqueued), we succeeded, whatever. */
    ret = 0;
    if (!futex_unqueue(&q))
        goto out;
    ret = -ETIMEDOUT;
    if (to && !to->task)
        goto out;

    /*
     * We expect signal_pending(current), but we might be the
     * victim of a spurious wakeup as well.
     */
    if (!signal_pending(current))
        goto retry;

    ret = -ERESTARTSYS;
    if (!abs_time)
        goto out;

    restart = &current->restart_block;
    restart->futex.uaddr = uaddr;
    restart->futex.val = val;
    restart->futex.time = *abs_time;
    restart->futex.bitset = bitset;
    restart->futex.flags = flags | FLAGS_HAS_TIMEOUT;

    ret = set_restart_fn(restart, futex_wait_restart);

out:
    if (to) {
        hrtimer_cancel(&to->timer);
        destroy_hrtimer_on_stack(&to->timer);
    }
    return ret;
}

int origin_futex_wake(u32 __user* uaddr, unsigned int flags, int nr_wake, u32 bitset) {
    struct futex_hash_bucket* hb;
    struct futex_q* this, * next;
    union futex_key key = FUTEX_KEY_INIT;
    int ret, nf_wake_cnt;
    DEFINE_WAKE_Q(wake_q);

    if (nr_wake != INT_MAX)
        count_uaddr_cnt((unsigned long)uaddr);
    // add_uaddr((unsigned long)uaddr);

    if (!bitset)
        return -EINVAL;

    ret = get_futex_key(uaddr, flags & FLAGS_SHARED, &key, FUTEX_READ);
    if (unlikely(ret != 0))
        return ret;

    hb = futex_hash(&key);

    /* Make sure we really have tasks to wakeup */
    // if (!futex_hb_waiters_pending(hb))
    //     return ret;

    spin_lock(&hb->lock);

    plist_for_each_entry_safe(this, next, &hb->chain, list) {
        if (futex_match(&this->key, &key)) {
            if (this->pi_state || this->rt_waiter) {
                ret = -EINVAL;
                break;
            }

            /* Check if one of the bits is set in both bitsets */
            if (!(this->bitset & bitset))
                continue;

            futex_wake_mark(&wake_q, this);
            if (++ret >= nr_wake)
                break;
        }
    }

    spin_unlock(&hb->lock);
    wake_up_q(&wake_q);

    return ret;
}

int new_futex_wake1(u32 __user* uaddr, unsigned int flags, int nr_wake, u32 bitset) {
    struct futex_hash_bucket* hb;
    struct futex_q* this, * next, * nf_wake_targets;
    union futex_key key = FUTEX_KEY_INIT;
    int i, j, temp, w_index, t_index, ret = 0, pos[64], wake_pos[64], \
        numa_id, numa_nodes, cpu_id, cpu_index, timeout_time, nf_wake_total;
    int need_wake_nr = nr_wake;
    struct exe_ret e_ret;
    unsigned int wake_id, ran;
    long old, new, cnt, wake_cnt = 0;
    struct nf_delegate_complete* completes;
    volatile atomic_long_t* queue, * wake_queue;
    volatile atomic_t* state;
    struct task_struct* wake_task;
    struct nf_delegate_uaddrnode* uaddrnode;
    struct pt_regs* target_regs;
    struct cpumask* mask;
    struct nf_uaddr_state* u_state;
    u32 uval;
    unsigned long begin, end, start, t;
    unsigned long delegation_interval = 0;

    if (!check_uaddr_state((unsigned long)uaddr))
        return origin_futex_wake(uaddr, flags, nr_wake, bitset);

    numa_id = numa_node_id();
    numa_nodes = 2;
    cpu_id = 0;
    cpu_index = smp_processor_id();
    mask = cpumask_of_node(numa_id);
    for_each_cpu(i, mask) {
        if (i == cpu_index)
            break;
        cpu_id++;
    }

    if (!bitset) {
        printk(KERN_INFO "nf wake bitset is NULL\n");
        ret = -EINVAL;
        goto out;
    }

    ret = get_futex_key(uaddr, flags & FLAGS_SHARED, &key, FUTEX_READ);
    if (unlikely(ret != 0)) {
        printk(KERN_INFO "nf wake can not get futex key\n");
        goto out;
    }
    hb = futex_hash(&key);

    // if (!futex_hb_waiters_pending(hb))
    //     return ret;

    uaddrnode = get_uaddrnode((unsigned long)uaddr, nf_numanodes[numa_id]->uaddr_node);
    completes = uaddrnode->completes;
    queue = &uaddrnode->queue.q;
    state = &completes[cpu_id].state;
    u_state = get_uaddr_state(uaddr);

    if (velvet_wait_queue == 1) {

        for (i = 0;i < 32;i++) {
            prefetch((char*)(completes + i) + 256);
        }

    }

    if (delegate_election == 1) {
        // i = 0;
        // while (atomic_read_acquire(&u_state->wake_numa) != numa_id) {
        //     cpu_relax();
        //     if (i++ > 1000000000) {
        //         break;
        //     }
        // }

        wake_queue = &uaddrnode->wake_queue.q;
        // nf_cldemote((void*)uaddrnode);
        timeout_time = 0;
        // while (atomic_read_acquire(&u_state->is_delegating) == 1 || !spin_trylock(&hb->lock)) {
        while (atomic_cmpxchg(&u_state->is_delegating, 0, 1) == 1) {
            int is_timeout = 0;
            atomic_set(state, fast_wake_ready);
            if (test_and_set_bit(cpu_id, (volatile unsigned long*)wake_queue))
                pr_info("[new_futex_wake]\tfast path q start value wrong");
            // nf_cldemote((void*)wake_queue);

            start = rdtsc_ordered();
            preempt_disable();
            while (atomic_read(state) == fast_wake_ready) {
                cpu_relax();
                mb();
                if (rdtsc_ordered() - start >= fast_path_wake_time[timeout_time]) {
                    if (test_and_clear_bit(cpu_id, (volatile unsigned long*)wake_queue)) {
                        atomic_set_release(state, fast_wake_timeout);
                        // nf_cldemote((void*)wake_queue);
                        preempt_enable();
                        is_timeout = 1;
                        timeout_time = timeout_time < 6 ? timeout_time + 1 : timeout_time;
                    }
                    break;
                }
            }
            if (!is_timeout) {
                while (atomic_read_acquire(state) == fast_wake_ready);
                preempt_enable();
                if (atomic_read(state) == fast_wake_success) {
                    atomic_set(state, fast_path_uninit);
                    // nf_cldemote((void*)state);
                    return 0;
                }
                else if (atomic_read(state) == fast_to_slow_wake) {
                    atomic_set(state, fast_path_uninit);
                    spin_lock(&hb->lock);
                    goto delegation_phase;
                }
                else
                    pr_info("[new_futex_wake] state return wrong value %d", atomic_read(state));
            }
        }
        atomic_set(state, fast_path_uninit);
        // atomic_set_release(&u_state->is_delegating, 1);
        // spin_unlock(&hb->lock);
        // atomic_set_release(&u_state->wake_numa, (numa_id + 1) % numa_nodes);
        // nf_cldemote((void*)state);

        // wake_cnt = 0;
        // old = atomic_long_xchg(wake_queue, 0UL);
        // // nf_cldemote((void*)wake_queue);
        // if (old != 0) {
        //     for (i = 0;i < 64;i++)
        //         if ((old & (1UL << i))) {
        //             wake_pos[wake_cnt++] = i;
        //             // atomic_set_release(&completes[i].state, fast_wake_success);
        //             // nf_cldemote((void*)(&completes[i].state));
        //         }
        //     for (i = 0;i < wake_cnt;i++) {
        //         atomic_set_release(&completes[wake_pos[i]].state, fast_wake_success);
        //     }
        // }
        // need_wake_nr += wake_cnt - 1;
    }
    else {
        // nf_cldemote((void*)uaddrnode);
        spin_lock(&hb->lock);
        // record_ret(futex_hb_waiters_pending(hb));
    }

delegation_phase:
    // unsigned long delegation_begin_time = rdtsc_ordered();

    // if (delegation_end_time > 0) {
    //     delegation_interval = rdtsc_ordered() - delegation_end_time;
    //     if (delegation_interval < 3000UL)
    //         ++cycle_cnts[cpu_id].cnt[delegation_interval / 100UL];
    //     else
    //         ++cycle_cnts[cpu_id].cnt[29];
    //     cycle_cnts[cpu_id].min = cycle_cnts[cpu_id].min < delegation_interval ? cycle_cnts[cpu_id].min : delegation_interval;
    //     cycle_cnts[cpu_id].total += delegation_interval;
    //     ++cycle_cnts[cpu_id].total_cnt;
    // }



    // stac();
    // nf_prefetchw((void*)uaddr);
    // nf_prefetchw((char*)uaddr + 64);


    nf_wake_total = 0;
    int nf_wake_cnt = 0;
    int nf_need_end = 0;
    int int3_hook = 0;
    int need_vw_queue = 1;
    cnt = 0;
    w_index = 0;
    int p_index = 0;
    int waker_cnt = 0;
    int need_e_queue = 1;

    e_ret.number = 0;

    while (1) {
        DEFINE_WAKE_Q(wake_q);
        e_ret.count = 0;
        ret = 0;

        if (delegate_election == 1 && need_e_queue == 1 && waker_cnt < NF_E_QUEUE_NUM) {
            wake_cnt = 0;
            old = atomic_long_xchg(wake_queue, 0UL);
            if (old != 0) {
                for (i = 0;i < 64;i++)
                    if ((old & (1UL << i))) {
                        wake_pos[wake_cnt++] = i;
                    }
                for (i = 0;i < wake_cnt;i++) {
                    atomic_set_release(&completes[wake_pos[i]].state, fast_wake_success);
                }
            }
            else
                need_e_queue = 0;
            need_wake_nr += wake_cnt - 1;
            waker_cnt += wake_cnt;
        }


        if (velvet_wait_queue == 1) {

            if (!nf_need_end && need_vw_queue == 1 && cnt < NF_FAST_PATH_WAKE_NUM && w_index == cnt) {
                int one_dequeue_cnt = 0;
                int temp_pos[64] = { 0 };
                old = atomic_long_xchg(queue, 0UL);
                if (old != 0) {
                    while (old) {
                        temp_pos[one_dequeue_cnt++] = __builtin_ctzl(old);
                        old &= old - 1;
                    }

                    // for (i = 0;i < 64;i++) {
                    //     if ((old & (1UL << i))) {
                    //         temp_pos[one_dequeue_cnt++] = i;
                    //     }
                    // }

                    // for (j = one_dequeue_cnt - 1;j > 0;j--) {
                    //     ran = nf_random() % (j + 1);
                    //     temp = temp_pos[j];
                    //     temp_pos[j] = temp_pos[ran];
                    //     temp_pos[ran] = temp;
                    // }
                    if (one_dequeue_cnt == 1)
                        need_vw_queue = 0;

                    ran = nf_random() % 2;
                    if (ran == 0)
                        for (j = 0;j < one_dequeue_cnt;j++)
                            pos[cnt++] = temp_pos[j];
                    else
                        for (j = one_dequeue_cnt - 1;j >= 0;j--)
                            pos[cnt++] = temp_pos[j];
                }
                else
                    need_vw_queue = 0;

            }
        }

        for (;p_index < cnt && p_index < w_index + 2;p_index++)
            nf_prefetch_context(completes[pos[p_index]].regs, completes[pos[p_index]].rsp);

        if (!nf_need_end) {
            if (w_index < cnt) {

                e_ret.ret = -14;
                nf_wake_cnt++;

                // if (nf_wake_cnt == 16) {
                //     atomic_set_release(&u_state->is_delegating, 0);
                // }

                // ran = nf_random();
                // t_index = w_index + ran % (cnt - w_index);
                // temp = pos[t_index];
                // pos[t_index] = pos[w_index];
                // pos[w_index] = temp;

                // nf_prefetch_context(completes[pos[w_index]].regs, completes[pos[w_index]].rsp);



                exe_on_call(completes[pos[w_index]].task, completes[pos[w_index]].regs, &e_ret);



                if (e_ret.ret < -1)
                    nf_need_end = 1;
                else if (e_ret.ret == 1) {
                    nf_wake_total++;
                    need_wake_nr++;
                }
                else if (e_ret.ret == 0) {
                    need_wake_nr--;
                }

                // unsigned long delegation_begin_time = rdtsc_ordered();
                // futex_hb_waiters_dec(hb);
                // delegation_interval += rdtsc_ordered() - delegation_begin_time;

                atomic_set_release(&completes[pos[w_index]].state, fast_path_success);


                w_index++;



            }
            else {
                // atomic_inc(&normal_wake_cnt);

                spin_lock(&hb->lock);
                e_ret.ret = -15;
                ret = 0;

                plist_for_each_entry_safe(this, next, &hb->chain, list) {
                    if (futex_match(&this->key, &key)) {
                        if (this->pi_state || this->rt_waiter) {
                            ret = -EINVAL;
                            printk(KERN_INFO "wake error\n");
                            goto out;
                        }
                        if (!(this->bitset & bitset))
                            continue;
                        if (!check_numa_node(this, numa_id))
                            continue;
                        nf_wake_targets = this;
                        target_regs = task_pt_regs(nf_wake_targets->task);
                        nf_prefetch_context(target_regs, target_regs->sp);
                        if (++ret >= 1)
                            break;
                    }
                }

                // if (ret == 0) {
                //     plist_for_each_entry_safe(this, next, &hb->chain, list) {
                //         if (futex_match(&this->key, &key)) {
                //             if (this->pi_state || this->rt_waiter) {
                //                 ret = -EINVAL;
                //                 printk(KERN_INFO "wake error\n");
                //                 goto out;
                //             }
                //             if (!(this->bitset & bitset))
                //                 continue;
                //             nf_wake_targets = this;
                //             target_regs = task_pt_regs(nf_wake_targets->task);
                //             nf_prefetch_context(target_regs, target_regs->sp);
                //             if (++ret >= 1)
                //                 break;
                //         }
                //     }
                // }
                if (ret == 1) {
                    if (nf_wake_cnt < cnt + NF_SLOW_PATH_WAKE_NUM) {
                        nf_wake_cnt++;
                        exe_on_call(nf_wake_targets->task, task_pt_regs(nf_wake_targets->task), &e_ret);
                    }
                    futex_wake_mark(&wake_q, nf_wake_targets);
                }

                if (e_ret.ret < -1)
                    nf_need_end = 1;
                else if (e_ret.ret == 1) {
                    nf_wake_total++;
                    need_wake_nr++;
                }
                else if (e_ret.ret == 0)
                    need_wake_nr--;
                spin_unlock(&hb->lock);
                wake_up_q(&wake_q);
            }
        }
        else {
            // for (i = 0;i < w_index;i++)
            //     atomic_set_release(&completes[pos[i]].state, fast_path_success);

            // atomic_inc(&normal_wake_cnt);

            for (;w_index < cnt;w_index++) {
                ret = futex_get_value_locked(&uval, uaddr);
                if (ret)
                    pr_info("[new_futex_wake] futex_get_value_locked error");
                if (uval != completes[pos[w_index]].val) {
                    atomic_set_release(&completes[pos[w_index]].state, fast_path_no_need_to_sleep);
                }
                else {
                    if (need_wake_nr > 0) {
                        atomic_set_release(&completes[pos[w_index]].state, fast_path_no_need_to_sleep);
                        need_wake_nr--;
                    }
                    else {
                        spin_lock(&hb->lock);
                        set_task_state(completes[pos[w_index]].task, TASK_INTERRUPTIBLE | TASK_FREEZABLE);
                        __nf_futex_queue(completes[pos[w_index]].q, hb, completes[pos[w_index]].task);
                        atomic_set_release(&completes[pos[w_index]].state, fast_path_fail);
                        spin_unlock(&hb->lock);
                    }
                }
            }



            break;
        }
    }
    // record_ret(cnt);


    spin_lock(&hb->lock);
    DEFINE_WAKE_Q(wake_q);
    ret = 0;
    plist_for_each_entry_safe(this, next, &hb->chain, list) {
        if (futex_match(&this->key, &key)) {
            if (this->pi_state || this->rt_waiter) {
                ret = -EINVAL;
                printk(KERN_INFO "wake error\n");
                goto out;
            }
            if (!(this->bitset & bitset))
                continue;

            futex_wake_mark(&wake_q, this);
            // nf_wake_total++;
            if (++ret >= need_wake_nr)
                break;
        }
    }
    // record_ret(ret);
    wake_up_q(&wake_q);



    // for (i = 0;i < 64;i++) {
    //     wake_task = completes[i].task;
    //     if (wake_task != NULL && wake_task->exit_state != EXIT_DEAD && wake_task->exit_state != EXIT_ZOMBIE)
    //         nf_cldemote_context(completes[i].regs, completes[i].rsp);
    // }
    // for (i = 63;i >= 0;i--) {
    //     nf_cldemote((char*)(&completes[i]) + 64);
    // }
    // record_ret(atomic_read(&int3_waiter_cnt));
    // delegation_interval = delegation_end_time - delegation_begin_time;



    // unsigned long delegation_begin_time = rdtsc_ordered();



    // delegation_end_time = rdtsc_ordered();

    // delegation_interval = rdtsc_ordered() - delegation_begin_time;
    // if (delegation_interval < 3000UL)
    //     ++cycle_cnts[cpu_id].cnt[delegation_interval / 100UL];
    // else
    //     ++cycle_cnts[cpu_id].cnt[29];
    // cycle_cnts[cpu_id].min = cycle_cnts[cpu_id].min < delegation_interval ? cycle_cnts[cpu_id].min : delegation_interval;
    // cycle_cnts[cpu_id].total += delegation_interval;
    // ++cycle_cnts[cpu_id].total_cnt;

    atomic_set_release(&u_state->is_delegating, 0);
    spin_unlock(&hb->lock);
    ret = nf_wake_cnt;
    // record_ret(nf_wake_cnt);

out:
    // printk(KERN_INFO "wake ret %d", ret);
    // atomic_set(&nf_d_lock, 0);
    return ret;
}

long test_futex_wait(u32 __user* uaddr, unsigned int flags, u32 val, ktime_t* abs_time, u32 bitset) {
    struct hrtimer_sleeper timeout, * to;
    struct restart_block* restart;
    struct futex_hash_bucket* hb;
    struct futex_q q = futex_q_init;
    long ret;
    u32 uval;

    // add_uaddr((unsigned long)uaddr);

    if (!bitset)
        return -EINVAL;
    q.bitset = bitset;

    to = futex_setup_timer(abs_time, &timeout, flags,
        current->timer_slack_ns);
    int cpu_id = smp_processor_id();
    unsigned long wait_begin = rdtsc_ordered();
retry:
    /*
     * Prepare to wait on uaddr. On success, it holds hb->lock and q
     * is initialized.
     */
     // ret = futex_wait_setup(uaddr, val, flags, &q, &hb);
     // if (ret)
     //     goto out;

     // /* futex_queue and wait for wakeup, timeout, or a signal. */
     // futex_wait_queue(hb, &q, to);



futex_wait_setup_retry:
    ret = get_futex_key(uaddr, flags & FLAGS_SHARED, &q.key, FUTEX_READ);
    if (unlikely(ret != 0))
        goto out;

futex_wait_setup_retry_private:
    hb = futex_hash(&q.key);
    futex_hb_waiters_inc(hb);
    q.lock_ptr = &hb->lock;

    spin_lock(&hb->lock);



    ret = futex_get_value_locked(&uval, uaddr);
    if (ret) {
        futex_q_unlock(hb);
        ret = get_user(uval, uaddr);
        if (ret)
            goto out;
        if (!(flags & FLAGS_SHARED))
            goto futex_wait_setup_retry_private;
        goto futex_wait_setup_retry;
    }
    if (uval != val) {
        futex_q_unlock(hb);
        ret = -EWOULDBLOCK;
        goto out;
    }
    set_current_state(TASK_INTERRUPTIBLE | TASK_FREEZABLE);
    __futex_queue(&q, hb);
    spin_unlock(&hb->lock);



    if (to)
        hrtimer_sleeper_start_expires(to, HRTIMER_MODE_ABS);
    if (likely(!plist_node_empty(&q.list))) {
        if (!to || to->task)
            schedule();
    }
    __set_current_state(TASK_RUNNING);

    unsigned long wait_cost = rdtsc_ordered() - wait_begin;
    if (wait_cost < 3000UL)
        ++cycle_cnts[cpu_id].cnt[wait_cost / 100UL];
    else
        ++cycle_cnts[cpu_id].cnt[29];
    cycle_cnts[cpu_id].min = cycle_cnts[cpu_id].min < wait_cost ? cycle_cnts[cpu_id].min : wait_cost;
    cycle_cnts[cpu_id].total += wait_cost;
    ++cycle_cnts[cpu_id].total_cnt;

    /* If we were woken (and unqueued), we succeeded, whatever. */
    ret = 0;
    if (!futex_unqueue(&q))
        goto out;
    ret = -ETIMEDOUT;
    if (to && !to->task)
        goto out;

    /*
     * We expect signal_pending(current), but we might be the
     * victim of a spurious wakeup as well.
     */
    if (!signal_pending(current))
        goto retry;

    ret = -ERESTARTSYS;
    if (!abs_time)
        goto out;

    restart = &current->restart_block;
    restart->futex.uaddr = uaddr;
    restart->futex.val = val;
    restart->futex.time = *abs_time;
    restart->futex.bitset = bitset;
    restart->futex.flags = flags | FLAGS_HAS_TIMEOUT;

    ret = set_restart_fn(restart, futex_wait_restart);

out:
    if (to) {
        hrtimer_cancel(&to->timer);
        destroy_hrtimer_on_stack(&to->timer);
    }
    return ret;
}

int test_futex_wake(u32 __user* uaddr, unsigned int flags, int nr_wake, u32 bitset) {
    struct futex_hash_bucket* hb;
    struct futex_q* this, * next;
    union futex_key key = FUTEX_KEY_INIT;
    int ret, nf_wake_cnt;
    DEFINE_WAKE_Q(wake_q);

    if (nr_wake != INT_MAX)
        count_uaddr_cnt((unsigned long)uaddr);

    if (!bitset)
        return -EINVAL;

    ret = get_futex_key(uaddr, flags & FLAGS_SHARED, &key, FUTEX_READ);
    if (unlikely(ret != 0))
        return ret;

    hb = futex_hash(&key);

    /* Make sure we really have tasks to wakeup */
    // if (!futex_hb_waiters_pending(hb))
    //     return ret;

    spin_lock(&hb->lock);

    plist_for_each_entry_safe(this, next, &hb->chain, list) {
        if (futex_match(&this->key, &key)) {
            if (this->pi_state || this->rt_waiter) {
                ret = -EINVAL;
                break;
            }

            /* Check if one of the bits is set in both bitsets */
            if (!(this->bitset & bitset))
                continue;

            futex_wake_mark(&wake_q, this);
            if (++ret >= nr_wake)
                break;
        }
    }

    spin_unlock(&hb->lock);
    wake_up_q(&wake_q);

    return ret;
}