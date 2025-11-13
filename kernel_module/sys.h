#ifndef SYS_H
#define SYS_H

#include <linux/cdev.h>

#define WRITE_PID 0
#define WRITE_EXE 1

void sys_init(void);
void sys_exit(void);

struct nf_dev {
    struct cdev cdev;
    // struct semaphore sem;
};

#endif