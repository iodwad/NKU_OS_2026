# OS Lab4 组员 C · `do_fork()` 实现提示词

## [RELY]

以下上下文来自当前 `lab4` 源码。实现前以工作区中的定义为准。

[`proc.h`](../../../code/kern/process/proc.h) 声明进程数和 PID 上限，以及进程管理所用的全局变量。

```c
#define MAX_PROCESS 4096
#define MAX_PID (MAX_PROCESS * 2)

extern list_entry_t proc_list;
extern struct proc_struct *idleproc, *initproc, *current;
```

[`proc.c`](../../../code/kern/process/proc.c) 内部维护 `static int nr_process = 0;` 和 PID 哈希表。`proc_init()` 初始化链表和哈希表，设置 `current = idleproc` 和 `nr_process = 1`，随后才通过 `kernel_thread()` 创建 `initproc`。PID 为 0 的 `idleproc` 计入进程总数，普通进程链表和 PID 哈希表都从空表开始。

`struct proc_struct` 中与登记直接相关的字段如下。

| 字段声明 | 用途 |
|---|---|
| `enum proc_state state;` | 新 PCB 为 `PROC_UNINIT`，唤醒后为 `PROC_RUNNABLE`。 |
| `int pid;` | 新 PCB 初值为 `-1`，登记时取得唯一的正 PID。 |
| `struct proc_struct *parent;` | 指向创建它的线程。 |
| `list_entry_t list_link;` | 普通进程链表节点。 |
| `list_entry_t hash_link;` | PID 哈希表节点，由 `hash_proc()` 使用。 |

`proc.c` 中已有以下文件内辅助函数。

```c
static struct proc_struct *alloc_proc(void);
static int setup_kstack(struct proc_struct *proc);
static void put_kstack(struct proc_struct *proc);
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc);
static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf);
static int get_pid(void);
static void hash_proc(struct proc_struct *proc);
```

| 辅助函数 | 当前实现的约定 |
|---|---|
| `alloc_proc()` | 从内核堆分配并初始化 PCB 及两个链表节点。失败返回 `NULL`。 |
| `setup_kstack()` / `put_kstack()` | 前者分配独立内核栈，成功返回 `0`，失败返回 `-E_NO_MEM`。后者释放成功分配的栈。 |
| `copy_mm()` | 成功时使用内核页表，子线程的 `mm` 为 `NULL`。若 `current->mm != NULL`，返回 `-E_INVAL`，且未取得需回收的地址空间资源。 |
| `copy_thread()` | 在已分配的内核栈上复制 trapframe，设置子线程的 `a0` 为 `0`，建立指向 `forkret` 的首次切换上下文。`esp == 0` 时从子线程自己的栈中选择初始栈指针，否则使用传入值。函数返回类型为 `void`。 |
| `get_pid()` / `hash_proc()` | 前者在 `0 < pid < MAX_PID` 范围内选取唯一 PID，后者按 PCB 中已赋值的 PID 登记哈希节点。`find_proc(pid)` 可据此查回同一 PCB。 |

- [`kmalloc.h`](../../../code/kern/mm/kmalloc.h) 声明 `void kfree(void *objp);`，用于释放 PCB。
- [`list.h`](../../../code/libs/list.h) 的 `list_add(listelm, elm)` 把 `elm` 插在 `listelm` 之后，`proc_list` 是普通进程链表的头哨兵。
- [`sched.h`](../../../code/kern/schedule/sched.h) 声明 `void wakeup_proc(struct proc_struct *proc);`。当前实现要求目标既非 `PROC_ZOMBIE` 也非 `PROC_RUNNABLE`，并将其置为 `PROC_RUNNABLE`。
- [`sync.h`](../../../code/kern/sync/sync.h) 提供 `local_intr_save(flag)` 和 `local_intr_restore(flag)`。前者保存本 CPU 原有的中断使能状态并关闭中断，后者在原状态为开启时重新开启中断。

[`error.h`](../../../code/libs/error.h) 中相关错误编号如下，函数失败时返回其负值。

```c
#define E_INVAL             3   // Invalid parameter
#define E_NO_MEM            4   // Request failed due to memory shortage
#define E_NO_FREE_PROC      5   // Attempt to create a new process beyond
```

## [GUARANTEE]

在 `lab4/code/kern/process/proc.c` 的 C 组区域实现以下接口，签名与 `proc.h` 的声明一致。

```c
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
```

该接口从调用者提供的现场创建子线程。成功时返回可查找的子线程 PID，失败时返回负错误码。`clone_flags` 和 `stack` 沿用现有 `copy_mm()` 和 `copy_thread()` 的参数语义。

本次源码改动限于 `do_fork()` 及必要的文件内 `static` 辅助函数。沿用现有公共接口、结构体布局和 A、B 组的实现。

## [SPECIFICATION]

### `do_fork()`

**Pre-Condition**

- 内核分配器、普通进程链表和 PID 哈希表已初始化。`current` 指向有效的创建者，可以是启动时的 `idleproc`。
- `tf` 指向调用期间可读取的完整 trapframe。`stack == 0` 表示由子线程的独立内核栈提供初始栈指针。`stack` 非零时，由调用者提供符合恢复现场约定的初始栈指针。

**Post-Condition**

- 成功时，子线程拥有独立内核栈和可供首次调度恢复的现场。创建者仍继续执行，并得到子线程的正 PID。子线程的 PCB 在普通进程链表和 PID 哈希表中各登记一次，父指针指向创建者，状态为 `PROC_RUNNABLE`，进程总数恰好增加一。
- 失败时，返回负错误码并释放本次创建取得的资源。普通进程链表、PID 哈希表和进程总数保持调用前的状态。
- 返回时，本 CPU 的中断使能状态与进入函数时一致。

**Case 1 · 达到进程数上限**

现有进程总数已达到上限时，在分配资源前返回 `-E_NO_FREE_PROC`。上限按包含 `idleproc` 的进程总数判断。

**Case 2 · PCB 分配失败**

有创建名额但无法分配 PCB 时，返回 `-E_NO_MEM`，此时没有取得需要释放的资源。

**Case 3 · 内核栈分配失败**

已取得 PCB 但内核栈分配失败时，释放 PCB，返回栈分配步骤的错误码。

**Case 4 · 地址空间准备失败**

已取得 PCB 和内核栈但地址空间准备失败时，按内核栈、PCB 的顺序释放资源，并原样返回该步骤的错误码。

**Case 5 · 创建成功**

资源和首次运行现场均已准备好时，取得唯一 PID，建立父子关系，完成链表、哈希表和进程总数的登记，再将子线程设为可运行并返回其 PID。

**Requirements**

- 将可能失败的资源准备放在登记之前。PID 分配、进程登记、计数更新和最终唤醒处于同一个本地关中断临界区，恢复中断前子线程已经具备完整的可运行状态。
- 通过现有接口准备地址空间和首次运行现场。调用结束后，子线程的 trapframe 位于自己的内核栈中，调用者传入的 trapframe 内容保持原样。
- 用现有进程创建测试核对 `initproc` 的 PID、父线程、可运行状态及初始现场。对照各错误出口检查清理顺序和全局状态，区分静态检查与实测。构建或运行遇到当前分支上的其他未实现接口时，记录实际结果和受影响的验证项。
