#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/uaccess.h>
#include <asm/pgtable.h>
#include <linux/io.h>

#include "int3.h"
#include "exe.h"

extern atomic_t nf_is_delegating;
extern atomic_t int3_waiter_cnt;

int __nf_do_int3(struct pt_regs* regs) {
    atomic_inc(&int3_waiter_cnt);
    while (atomic_read(&nf_is_delegating) == 1);
    atomic_dec(&int3_waiter_cnt);
    return 1;
}


unsigned long virt_to_phys_user(struct task_struct* task, unsigned long vaddr) {
    struct mm_struct* mm;
    pgd_t* pgd;
    p4d_t* p4d;
    pud_t* pud;
    pmd_t* pmd;
    pte_t* pte;
    unsigned long phys_addr = 0;

    mm = task->mm;
    if (!mm) return 0;

    pgd = pgd_offset(mm, vaddr);
    if (pgd_none(*pgd) || pgd_bad(*pgd)) return 0;

    p4d = p4d_offset(pgd, vaddr);
    if (p4d_none(*p4d) || p4d_bad(*p4d)) return 0;

    pud = pud_offset(p4d, vaddr);
    if (pud_none(*pud) || pud_bad(*pud)) return 0;

    pmd = pmd_offset(pud, vaddr);
    if (pmd_none(*pmd) || pmd_bad(*pmd)) return 0;

    pte = pte_offset_map(pmd, vaddr);
    if (!pte_present(*pte)) return 0;

    phys_addr = (pte_pfn(*pte) << PAGE_SHIFT) | (vaddr & ~PAGE_MASK);
    pte_unmap(pte);

    return phys_addr;
}

int change_endbr_to_int3(unsigned long phys_addr) {
    void __iomem* virt_addr;
    virt_addr = ioremap(phys_addr, sizeof(uint8_t));
    if (!virt_addr) {
        printk(KERN_ERR "Failed to map physical address\n");
        return -ENOMEM;
    }
    iowrite8(0xcc, virt_addr);
    iounmap(virt_addr);
    return 0;
}