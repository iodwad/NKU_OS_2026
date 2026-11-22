#include <unistd.h>
#include <error.h>
#include <proc.h>
#include <syscall.h>
#include <trap.h>
#include <stdio.h>
#include <vmm.h>

static int
sys_exit(uint64_t arg[]) {
    int error_code = (int)arg[0];
    return do_exit(error_code);
}

static int
sys_fork(uint64_t arg[]) {
    struct trapframe *tf = current->tf;
    uintptr_t stack = tf->gpr.sp;
    return do_fork(0, stack, tf);
}

static int
sys_wait(uint64_t arg[]) {
    int pid = (int)arg[0];
    int *store = (int *)arg[1];
    return do_wait(pid, store);
}

static int
sys_exec(uint64_t arg[]) {
    const char *name = (const char *)arg[0];
    size_t len = (size_t)arg[1];
    unsigned char *binary = (unsigned char *)arg[2];
    size_t size = (size_t)arg[3];
    struct mm_struct *mm = current->mm;

    if (mm == NULL && trap_in_kernel(current->tf)) {
        return do_execve(name, len, binary, size);
    }
    if (mm == NULL || !user_mem_check(mm, (uintptr_t)name, len, 0) ||
        !user_mem_check(mm, (uintptr_t)binary, size, 0)) {
        return -E_INVAL;
    }
    // do_execve releases the old address space before reading the ELF image.
    return -E_UNIMP;
}

static int
sys_yield(uint64_t arg[]) {
    return do_yield();
}

static int
sys_kill(uint64_t arg[]) {
    int pid = (int)arg[0];
    return do_kill(pid);
}

static int
sys_getpid(uint64_t arg[]) {
    return current->pid;
}

static int
sys_putc(uint64_t arg[]) {
    int c = (int)arg[0];
    cputchar(c);
    return 0;
}

static int
sys_pgdir(uint64_t arg[]) {
    return -E_UNIMP;
}

static int (*syscalls[])(uint64_t arg[]) = {
    [SYS_exit]              sys_exit,
    [SYS_fork]              sys_fork,
    [SYS_wait]              sys_wait,
    [SYS_exec]              sys_exec,
    [SYS_yield]             sys_yield,
    [SYS_kill]              sys_kill,
    [SYS_getpid]            sys_getpid,
    [SYS_putc]              sys_putc,
    [SYS_pgdir]             sys_pgdir,
};

#define NUM_SYSCALLS        ((sizeof(syscalls)) / (sizeof(syscalls[0])))

void
syscall(void) {
    struct trapframe *tf = current->tf;
    uintptr_t num = tf->gpr.a0;
    if (num >= NUM_SYSCALLS) {
        tf->gpr.a0 = -E_INVAL;
        return;
    }
    if (syscalls[num] == NULL) {
        tf->gpr.a0 = -E_UNIMP;
        return;
    }

    uint64_t arg[] = {tf->gpr.a1, tf->gpr.a2, tf->gpr.a3, tf->gpr.a4, tf->gpr.a5};
    tf->gpr.a0 = syscalls[num](arg);
}

