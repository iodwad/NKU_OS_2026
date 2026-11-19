# OS Lab4 组员 C · `schedule()` 实现提示词

## [RELY]

[`proc.h`](../../../code/kern/process/proc.h) 提供进程链表、当前线程、idle 以及真正执行切换的接口。

```c
extern list_entry_t proc_list;
extern struct proc_struct *idleproc, *initproc, *current;
void proc_run(struct proc_struct *proc);

#define le2proc(le, member) \
    to_struct((le), struct proc_struct, member)
```

| `struct proc_struct` 的字段声明 | 调度用途 |
|---|---|
| `enum proc_state state;` | 普通线程只有处于 `PROC_RUNNABLE` 时才可被选中，正在运行的线程也可以处于此状态。 |
| `int runs;` | 记录线程被选中运行的次数。 |
| `volatile bool need_resched;` | 当前线程的重调度请求。 |
| `list_entry_t list_link;` | 普通进程链表节点，通过 `le2proc(le, list_link)` 找到其 PCB。 |

[`list.h`](../../../code/libs/list.h) 中的 `list_next(list_entry_t *listelm)` 返回后继节点。`proc_list` 是双向循环链表的头哨兵。普通线程登记在这个链表中，`idleproc` 单独作为回退目标。

[`sync.h`](../../../code/kern/sync/sync.h) 提供 `local_intr_save(flag)` 和 `local_intr_restore(flag)`。前者保存本 CPU 原有的中断使能状态并关闭中断，后者在原状态为开启时重新开启中断。[`proc_run()`](../../../code/kern/process/proc.c) 负责更新 `current`、切换页表并调用底层上下文切换函数。

## [GUARANTEE]

在 `lab4/code/kern/schedule/sched.c` 的 C 组区域实现以下接口，保持 [`sched.h`](../../../code/kern/schedule/sched.h) 中的声明。

```c
void schedule(void);
```

该函数消耗一次重调度请求，按轮转规则选出目标，必要时交给 `proc_run()` 切换。现有链表结构、`wakeup_proc()` 和 `proc_run()` 的接口照常使用。

## [SPECIFICATION]

### `schedule()`

**Pre-Condition**

进程链表和 `idleproc` 已初始化。`current` 指向 `idleproc` 或已登记的普通线程。调用者可以在中断开启或关闭的状态下进入调度器。

**Post-Condition**

当前线程发起的重调度请求已清除。调度目标的 `runs` 增加一次。目标不同于调用时的当前线程时，完成一次上下文切换，原调用现场待该线程再次被调度时继续执行。此次调用返回时，恢复该调用现场进入调度器前的中断使能状态。

**Case 1 · 从 idle 发起调度**

从普通进程链表的首个节点开始，正向寻找首个可运行线程。

**Case 2 · 从普通线程发起调度**

从当前普通线程之后开始，按链表顺序寻找首个可运行线程。到链表末端后绕回，最多遍历一圈。若当前线程是唯一可运行的普通线程，本次选中它并继续执行。

**Case 3 · 没有可运行的普通线程**

链表为空或其中所有线程均不可运行时，选择 `idleproc`。当前已经是 `idleproc` 时继续运行它。

**Requirements**

- 清除请求、遍历链表、统计选择及发起切换均在本地关中断的保护下完成。线程恢复执行后，按原调用记录恢复中断状态。
- 跳过链表头哨兵，只从 `PROC_RUNNABLE` 的普通线程中选择候选。目标与当前线程相同时只记录一次选择。
- 用仓库现有调度测试核对两个测试线程都获得运行机会。检查单一可运行线程、跳过阻塞线程、回退 idle 和中断状态恢复，分别记录静态检查与实测覆盖。
