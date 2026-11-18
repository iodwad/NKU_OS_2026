# OS Lab4 — B 组：进程创建与初始化模块

[PROMPT]

**任务**：

完成南开大学操作系统 Lab4 中 B 组负责的“进程创建与初始化”功能。

本实验基于 RISC-V RV64、Sv39 分页机制及 Lab2/Lab3 已实现的物理内存管理和 Trap 处理代码。

目标是使内核能够初始化进程管理结构，将当前启动执行现场注册为 idleproc，创建首个内核线程 initproc，并为新线程建立合法的 trapframe 和 context，使其能够通过调度器完成第一次执行。

**项目路径**：

Windows：

`D:\college_study_materials\junior_year_fall\OS\lab4\lab4\code`

WSL：

`/mnt/d/college_study_materials/junior_year_fall/OS/lab4/lab4/code`

Git 分支：`lab4`

GitHub：https://github.com/iodwad/NKU_OS_2026/tree/lab4

**目标文件**：

`kern/process/proc.c`

本次只实现该文件中 B 组负责的五个函数，代码放入：

```c
//LAB4 function2 YOUR CODE
```

**操作要求**：

1. 必须直接修改项目中的真实源码文件，而不是只输出代码片段。
2. 开始前检查 Git 分支、工作区状态及当前源码内容。
3. 阅读真实源码、相关头文件、汇编恢复逻辑和测试要求。
4. 保留原有正确实现，尤其是 Lab2/Lab3 迁移代码和 A 组已实现的 `proc_run()`。
5. 仅在 B 组负责的预留区域实现五个函数，不得重写整个 `proc.c`。
6. 不修改 A 组的 `page_insert()`、`page_remove()`、`proc_run()`。
7. 不修改 C 组的 `do_fork()`、`cpu_idle()`、`schedule()`、`switch_to()`。
8. 不修改公共结构体布局、函数签名、汇编 ABI、Makefile、评分脚本和已有测试断言。
9. 允许在 B 组代码区域加入必要的模块私有辅助函数，但不得扩大公开接口。
10. 如果发现必须修改其他组函数才能继续，应记录准确的接口依赖与冲突，不能直接替其他成员实现。
11. 不允许通过删除测试、绕过断言、伪造输出或修改评分规则来获得通过结果。
12. 本次直接完成代码实现和能够执行的验证，不要只停留在设计方案。

**输出要求**：

使用下面的 `[RELY]`、`[GUARANTEE]`、`[SPECIFICATION]` 作为编码依据，以真实仓库为最终事实来源。

完成后提供：

- 五个函数的实际实现状态。
- 修改文件及 Git diff。
- 关键字段和寄存器设计说明。
- 实际编译与测试命令、退出码和关键输出。
- B 与 C 组需要确认的接口约定。
- 尚未验证的内容及已知问题。

**本轮不执行 git push，不合并到其他分支。**

---

[RELY]

以下信息已从当前 Lab4 仓库核对。开始编码前仍须再次读取本地文件，避免仓库更新导致上下文过期。

### 1. 进程状态与结构体

来源：`kern/process/proc.h`

进程状态：

```c
enum proc_state {
    PROC_UNINIT = 0,
    PROC_SLEEPING,
    PROC_RUNNABLE,
    PROC_ZOMBIE,
};
```

`struct proc_struct` 当前包含以下字段：

```c
struct proc_struct {
    enum proc_state state;
    int pid;
    int runs;
    uintptr_t kstack;
    volatile bool need_resched;
    struct proc_struct *parent;
    struct mm_struct *mm;
    struct context context;
    struct trapframe *tf;
    uintptr_t pgdir;
    uint32_t flags;
    char name[PROC_NAME_LEN + 1];
    list_entry_t list_link;
    list_entry_t hash_link;
};
```

`struct context` 保存：

```c
struct context {
    uintptr_t ra;
    uintptr_t sp;
    uintptr_t s0;
    uintptr_t s1;
    uintptr_t s2;
    uintptr_t s3;
    uintptr_t s4;
    uintptr_t s5;
    uintptr_t s6;
    uintptr_t s7;
    uintptr_t s8;
    uintptr_t s9;
    uintptr_t s10;
    uintptr_t s11;
};
```

相关定义：

```c
#define PROC_NAME_LEN 15
#define MAX_PROCESS 4096
#define MAX_PID (MAX_PROCESS * 2)
#define KSTACKPAGE 2
#define KSTACKSIZE (KSTACKPAGE * PGSIZE)
#define CLONE_VM 0x00000100
#define CLONE_THREAD 0x00000200
```

这些定义必须在真实头文件中重新核对。

### 2. Trapframe 结构

来源：`kern/trap/trap.h`

```c
struct trapframe {
    struct pushregs gpr;
    uintptr_t status;
    uintptr_t epc;
    uintptr_t badvaddr;
    uintptr_t cause;
};
```

`struct pushregs` 中包含：

- `sp`：栈指针
- `ra`：返回地址
- `s0`、`s1`：被调用者保存寄存器
- `a0`、`a1`：参数及返回值寄存器
- 其余 RISC-V 通用寄存器

必须按照实际字段名称访问，不得使用 x86 的 `tf_esp`、`tf_eip` 等字段。

### 3. 线程入口与恢复链路

来源：`kern/process/entry.S`

```asm
.globl kernel_thread_entry
kernel_thread_entry:
    move a0, s1
    jalr s0
    jal do_exit
```

这意味着：

- 新线程的函数入口需要通过 `s0` 传递。
- 函数参数需要通过 `s1` 传递。
- `kernel_thread_entry` 将参数放入 `a0` 后调用目标函数。
- 目标函数返回后进入 `do_exit`。

来源：`kern/trap/trapentry.S`

```asm
.globl forkrets
forkrets:
    move sp, a0
    j __trapret
```

现有 `__trapret` 使用 `RESTORE_ALL` 恢复寄存器、`sstatus` 和 `sepc`，最后执行 `sret`。

`kern/process/proc.c` 中已有：

```c
static void forkret(void)
{
    forkrets(current->tf);
}
```

因此新线程第一次被切换执行时，必须满足以下调用链：

`switch_to → forkret → forkrets → __trapret → kernel_thread_entry → fn(arg)`

必须对照汇编检查每一步需要的寄存器与栈内容。

### 4. 已有全局变量及辅助接口

来源：`kern/process/proc.c`

已有全局状态：

```c
list_entry_t proc_list;

static list_entry_t hash_list[HASH_LIST_SIZE];

struct proc_struct *idleproc = NULL;
struct proc_struct *initproc = NULL;
struct proc_struct *current = NULL;

static int nr_process = 0;
```

可使用的现有接口包括：

- `kmalloc()`、`kfree()`
- `memset()`、`memcpy()`
- `list_init()`、`list_add()`、`list_empty()`
- `find_proc()`、`set_proc_name()`
- `get_pid()`、`hash_proc()`
- `setup_kstack()`、`put_kstack()`
- `bootstack`、`boot_pgdir_pa`
- `do_fork()`、`wakeup_proc()`
- `read_csr(sstatus)`
- `SSTATUS_SPP`、`SSTATUS_SPIE`、`SSTATUS_SIE`

注意：接口存在不代表本组应调用所有接口。

`get_pid()`、`hash_proc()`、`setup_kstack()` 和 `wakeup_proc()` 涉及 C 组的创建流程。B 组不能替代 C 组完成资源登记与调度。

### 5. 地址空间与页表约定

来源：`kern/mm/pmm.h`、`kern/process/proc.c`

```c
extern uintptr_t boot_pgdir_pa;
```

A 组已有的 `proc_run()` 根据 `proc->pgdir` 构造 Sv39 `satp` 值。

因此必须明确：

- `proc->pgdir` 在当前实现中应满足物理地址约定。
- 内核线程不应凭空创建用户地址空间。
- `mm == NULL` 与 `pgdir == boot_pgdir_pa` 可以同时成立。
- B 组不得擅自修改 `proc_run()` 的地址空间接口。

### 6. 测试约束

来源：`tools/test_lab4.c`

`test_lab4_process_create()` 对 B 组提出了明确要求。

`proc_init()` 完成后：

```c
current == idleproc
idleproc != NULL
idleproc->pid == 0
idleproc->state == PROC_RUNNABLE
idleproc->kstack == (uintptr_t)bootstack
idleproc->parent == NULL
idleproc->mm == NULL
idleproc->tf == NULL
idleproc->runs == 0
idleproc->flags == 0
idleproc->context.ra == 0

initproc != NULL
initproc->pid == 1
initproc->state == PROC_RUNNABLE
initproc->parent == idleproc
initproc->mm == NULL
initproc->context.ra != 0
initproc->context.sp != 0
```

此外，调度测试会调用 `kernel_thread()` 创建两个测试线程，并检查线程函数能否执行及参数是否正确传递。

真实评分脚本还会检查 initproc 的输出：

```text
this initproc, pid = 1, name = "init"
To U: "Hello world!!".
To U: "en.., Bye, Bye. :)"
```

不要修改测试内容来迎合实现。

---

[GUARANTEE]

本次必须实现以下五个函数，均位于 `kern/process/proc.c`。

```c
static struct proc_struct *alloc_proc(void);

void proc_init(void);

int kernel_thread(
    int (*fn)(void *),
    void *arg,
    uint32_t clone_flags
);

static void copy_thread(
    struct proc_struct *proc,
    uintptr_t esp,
    struct trapframe *tf
);

static int copy_mm(
    uint32_t clone_flags,
    struct proc_struct *proc
);
```

**接口保证**：

1. 函数签名、返回类型和可见性必须保持一致。
2. 五个函数必须具备完整实现，不允许保留空函数或临时假成功返回。
3. `alloc_proc()` 分配失败时返回 `NULL`。
4. `proc_init()` 建立有效的 `idleproc`、`initproc` 和 `current`。
5. `kernel_thread()` 成功返回正 PID，失败返回实际错误码。
6. `copy_thread()` 建立新线程第一次运行所需的执行现场。
7. `copy_mm()` 正确处理当前 Lab4 的内核线程地址空间语义。
8. 不得在这五个函数中重复实现 C 组的 `do_fork()`、`schedule()` 或上下文切换逻辑。

---

[SPECIFICATION]

## 1. alloc_proc()

**Pre-Condition**：

- 内核的动态内存分配机制已经初始化。
- 调用者需要创建一个新的进程控制块。
- 新 PCB 尚未分配 PID，也未加入全局进程管理结构。

**Post-Condition**：

- 成功时返回一个已完成安全初始化的 `struct proc_struct *`。
- 返回对象的所有当前代码路径可能读取的字段均具有确定的初始值。
- PCB 尚未进入 `PROC_RUNNABLE` 状态。
- PCB 未登记到全局进程链表或 PID 哈希表。
- 失败时返回 `NULL`，不留下不完整对象。

**Case 1：分配成功**

为新 PCB 分配内存，设置初始状态，初始化身份信息、运行统计、调度标志、内核栈地址、父进程指针、地址空间指针、trapframe 指针、context、名称和链表节点。

初始状态应为 `PROC_UNINIT`。

PID 应具有明确的“尚未分配”语义，不能提前占用正式 PID。

`context`、`tf`、`mm` 和其他指针字段不得包含未初始化的随机值。

**Case 2：分配失败**

如果无法获得 PCB 内存，返回 `NULL`。

不得解引用空指针，也不得修改任何全局进程管理状态。

**Requirements**：

- 初始化必须覆盖所有实际字段。
- 不得依赖 `kmalloc()` 返回的内存天然为零。
- 新 PCB 的链表节点应处于安全且可供后续登记的状态。
- 不进行内核栈分配、PID 分配或进程唤醒。

## 2. proc_init()

**Pre-Condition**：

- `pmm_init()` 和 `vmm_init()` 已完成。
- 当前内核运行在有效的内核地址空间。
- `bootstack` 和 `boot_pgdir_pa` 可用。
- 进程管理的全局链表和哈希表尚未初始化。
- 当前系统尚未开始正式的线程调度。

**Post-Condition**：

完成进程管理初始化，建立 idleproc 和 initproc。

必须满足：

- `idleproc` 表示当前内核启动执行现场。
- `idleproc->pid == 0`。
- `idleproc->state == PROC_RUNNABLE`。
- `idleproc->kstack == (uintptr_t)bootstack`。
- `idleproc->mm == NULL`。
- `current == idleproc`。
- `initproc` 是新创建的内核线程。
- `initproc->pid == 1`。
- `initproc->parent == idleproc`。
- `initproc->state == PROC_RUNNABLE`。
- `initproc->mm == NULL`。
- `initproc` 具有有效的首次运行 context。

**Case 1：初始化进程管理结构**

初始化 `proc_list` 和 PID 哈希表。

确保所有全局状态与实际已创建的进程数量一致。

**Case 2：建立 idleproc**

通过 `alloc_proc()` 获取 PCB。

idleproc 不是通过 `do_fork()` 创建的普通子线程，而是当前正在执行的启动线程。

因此需要赋予特殊 PID、已有的启动栈、合法的地址空间与调度状态，并将 `current` 指向它。

不得为 idleproc 错误地创建一份普通 fork 的内核栈。

**Case 3：创建 initproc**

使用 `kernel_thread()` 创建以已有 `init_main()` 为入口的内核线程。

使用实际的 `find_proc()` 找到新创建的线程对象。

设置名称为 `"init"`。

确保 initproc 的身份、父子关系和初始 context 满足测试要求。

`init_main()` 传入的字符串必须与当前评分预期一致。

**Case 4：初始化失败**

如果 PCB 分配或第一个内核线程创建失败，使用仓库已有的 `panic()` / `assert()` 约定处理。

不能在关键状态缺失时继续运行或伪装初始化成功。

**Requirements**：

- 当前执行线程必须在创建 initproc 前具有合法身份。
- 全局进程数量、链表和 PID 状态必须保持一致。
- 不直接调用调度器或执行上下文切换。
- 保留当前已有的启动流程。

## 3. kernel_thread()

**Pre-Condition**：

- 进程管理结构已经完成初始化。
- `current` 指向合法的内核线程。
- `fn` 为有效的内核函数入口。
- `arg` 为需要传递给新线程的参数。
- `do_fork()` 接口按照 `proc.h` 的声明提供。

**Post-Condition**：

- 成功时返回新内核线程的正 PID。
- 失败时返回对应负错误码。
- 新线程第一次执行时能够调用 `fn(arg)`。
- 内核线程的首次恢复状态与 RISC-V S 模式语义一致。

**Case 1：创建线程初始现场**

构造临时 trapframe，确保其初始内容确定。

根据真实的 `kernel_thread_entry` 汇编约定，将：

- 目标函数入口对应到 `s0`。
- 函数参数对应到 `s1`。
- 首次恢复入口对应到 `kernel_thread_entry`。

设置正确的特权级状态和中断恢复状态。

**Case 2：提交线程创建请求**

通过 `do_fork()` 创建线程，将准备好的 trapframe 和正确的 clone 标志交给 C 组接口。

对于成功和失败结果，不得自行伪造 PID 或覆盖 C 组返回的错误。

**Case 3：创建失败**

不得遗留已发布但无法执行的线程。

B 组负责保持其构造的临时现场有效；实际 PCB、内核栈和全局登记资源的失败回滚由 C 组负责。

**Requirements**：

- 核对 `SSTATUS_SPP`、`SSTATUS_SPIE`、`SSTATUS_SIE` 的含义。
- 首次执行最终应在正确的特权级运行。
- 不能将函数入口错误地写入无关寄存器。
- 不能忽略 `clone_flags` 的语义。
- 不修改 `kernel_thread_entry` 汇编。

## 4. copy_thread()

**Pre-Condition**：

- `proc` 指向已分配且初始化的 PCB。
- C 组已为该线程提供合法的内核栈。
- `tf` 指向有效的待复制 trapframe。
- `esp` 表示 `do_fork()` 提供的初始栈参数，其语义必须与 C 组一致。

**Post-Condition**：

- 子线程拥有自己独立保存的 trapframe。
- `proc->tf` 指向其内核栈中的合法位置。
- trapframe 的内容与线程入口、参数、返回值和特权级恢复要求一致。
- `proc->context.ra` 指向项目定义的首次恢复入口。
- `proc->context.sp` 指向能够安全进入该恢复入口的内核栈位置。
- 新线程可以通过 `switch_to()` 首次切入，然后经 `forkret`、`forkrets`、`__trapret` 恢复执行。

**Case 1：构造独立 trapframe**

在新线程的内核栈上安排 trapframe。

复制传入的现场，并设置必要的子线程返回值语义。

不得把 `proc->tf` 指向调用者的临时栈帧。

**Case 2：处理首次运行的栈指针**

确定内核栈的实际范围、trapframe 位置以及首次恢复后的 `sp`。

特别检查 `esp == 0` 的内核线程创建路径。

即使 `do_fork()` 传入的栈参数为零，也不能让目标线程在首次进入 C 函数时使用无效的零栈指针。

应依据真实的 RISC-V ABI、栈布局和汇编恢复逻辑确定合法初始值，并验证栈对齐。

**Case 3：初始化 context**

配置新线程首次运行需要的 `ra`、`sp` 等字段。

`context` 的职责是供 `switch_to()` 保存和恢复调度上下文。

`trapframe` 的职责是供 `__trapret` 恢复完整执行现场。

不能混淆二者，也不能假设 `switch_to()` 会恢复所有通用寄存器。

**Requirements**：

- 必须保证 trapframe 完整落在新线程内核栈范围内。
- 遵守 RISC-V 调用约定要求的栈对齐。
- 不覆盖内核栈以外的内存。
- 不写入无效的 `ra` 或 `sp`。
- 必须和 C 组 `switch_to()` 使用的 `struct context` 布局一致。

## 5. copy_mm()

**Pre-Condition**：

- `proc` 指向已分配的 PCB。
- 父线程地址空间状态有效。
- 当前 Lab4 主要支持内核线程创建。
- 当前内核地址空间的根页表物理地址可用。

**Post-Condition**：

- 新线程具有与当前实验要求一致的地址空间状态。
- 内核线程不虚构用户态 `mm_struct`。
- `proc->pgdir` 满足当前 `proc_run()` 的页表切换约定。
- 成功时返回 `0`。
- 失败时返回实际定义的负错误码，而非假装成功。

**Case 1：父线程没有独立用户地址空间**

如果 `current->mm == NULL`，新线程应保持当前实验规定的内核地址空间语义。

不额外分配用户页表，也不创建不必要的 `mm_struct`。

**Case 2：存在 clone 标志**

按照项目实际支持的 clone 语义处理。

不能仅因为 `CLONE_VM` 已定义，就假设完整的用户地址空间共享和引用计数机制已经实现。

**Case 3：父线程具有非空 mm**

检查当前项目是否已经具备合法的地址空间共享或复制及资源释放机制。

如果这些能力尚未实现，应明确当前实验的支持边界，避免静默产生不一致的地址空间状态。

**Requirements**：

- 不提前实现后续实验的用户进程地址空间复制。
- 不增加没有对应释放路径的引用计数。
- 不改变 A 组的 `proc_run()`。
- 与 C 组 `do_fork()` 的失败清理逻辑兼容。

## 6. 模块级一致性与验收要求

实现上述五个函数后，必须执行以下检查。

**代码范围检查**：

- 使用 `git diff --check` 检查补丁。
- 确认所有源代码变更都位于 B 组许可范围内。
- 确认 A/C 组函数没有被修改。
- 确认现有 Lab2/Lab3 代码未被覆盖。

**构建验证**：

优先在 WSL Ubuntu 中使用现有 RISC-V 工具链。

尝试执行真实 Makefile 的完整构建命令，并记录退出码与构建输出。

由于 C 组的 `do_fork()`、`schedule()`、`switch_to()` 等可能尚未实现，完整链接失败时，必须区分：

1. B 组代码自身的语法或类型错误。
2. C 组函数缺失造成的链接错误。
3. 已有 A 组代码或环境问题。

如完整构建受其他组未完成代码阻断，应进一步使用实际 Makefile 的编译参数验证 B 组 `proc.c` 的编译，不得直接宣称整个 Lab4 已通过。

**行为检查**：

至少核对：

- PCB 所有字段安全初始化。
- idleproc 与 current 的身份一致。
- initproc PID、父进程及状态正确。
- 线程函数入口与参数传递正确。
- trapframe 位于独立内核栈内。
- 首次运行的 `sp` 非零且满足栈要求。
- context 的 `ra/sp` 与恢复链路一致。
- `copy_mm()` 满足内核线程地址空间约定。

**评分验证**：

当前仓库提供了 `make grade` 和 `tools/test_lab4.c`。

应先检查评分脚本的实际行为，再决定是否运行。

只有在相关 A/C 组代码齐备时，才能将完整评分结果用作系统级验收。

不得将“B 组单文件编译通过”等同于“完整 Lab4 通过”。

**最终交付**：

在当前本地仓库完成真实代码修改，并输出：

1. B 组五个函数的实现摘要。
2. 修改文件及 Git diff 检查结果。
3. PCB 初始化字段清单。
4. trapframe/context/内核栈的实际布局与首次执行路径。
5. 所有执行过的验证命令、退出码和关键输出。
6. 与 A/C 组的接口交接清单。
7. 已通过、未通过、因其他组缺失而无法验证的测试项。

不要将未经实际运行的测试写成 PASS。

不要修改评分机制，不要自动提交或推送 GitHub。

**完成标准**：B 组五个函数在真实源码中完整实现，修改范围符合分工，能够独立编译验证的部分已实际验证，且与 A/C 组的接口依赖清晰可交接。
