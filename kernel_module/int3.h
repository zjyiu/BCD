#ifndef INT3_H
#define INT3_H

#include <asm/ptrace.h>

int __nf_do_int3(struct pt_regs* regs);
unsigned long virt_to_phys_user(struct task_struct* task, unsigned long vaddr);
int change_endbr_to_int3(unsigned long phys_addr);

#endif