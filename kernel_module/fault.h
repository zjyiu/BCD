#ifndef FAULT_H
#define FAULT_H

#include <asm/ptrace.h>

int new_futex_check_page_fault(struct pt_regs* regs);

#endif