# OS Lab4 组员 C · `cpu_idle()` 实现提示词

## [RELY]

[`proc.h`](../../../code/kern/process/proc.h) 中与空闲循环相关的定义如下。

```c
extern struct proc_struct *idleproc, *initproc, *current;
```

`struct proc_struct` 的重调度标记声明为 `volatile bool need_resched;`。[`sched.h`](../../../code/kern/schedule/sched.h) 提供 `void schedule(void);`。

[`proc_init()`](../../../code/kern/process/proc.c) 将 `current` 指向 `idleproc`，并设置 `idleproc->need_resched = 1`。[`kern_init()`](../../../code/kern/init/init.c) 完成进程和时钟初始化、开启中断后调用 `cpu_idle()`。

## [GUARANTEE]

在 `lab4/code/kern/process/proc.c` 的 C 组区域实现 `cpu_idle()`，保持 [`proc.h`](../../../code/kern/process/proc.h) 中的公开声明。

```c
void cpu_idle(void) __attribute__((noreturn));
```

该函数是启动线程进入空闲状态后的永久入口，由 `schedule()` 决定下一运行线程。

## [SPECIFICATION]

### `cpu_idle()`

**Pre-Condition**

进程管理和调度所用的全局状态已初始化，当前执行现场属于 `idleproc`。

**Post-Condition**

空闲线程持续观察当前线程的重调度标记。标记置位时发起调度，空闲线程重新获得执行权后继续检查。该入口始终留在循环中，重调度标记由调度器处理。
