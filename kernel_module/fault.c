#include <linux/vmalloc.h>

#include "fault.h"

int new_futex_check_page_fault(struct pt_regs* regs) {
    int ret = 0;
    if (VMALLOC_START <= regs->ip && regs->ip < VMALLOC_END)
        ret = 1;
    return ret;
}