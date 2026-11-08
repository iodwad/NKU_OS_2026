# 操作系统实验报告

## 实验基本信息

| 项目 | 内容 |
|---|---|
| 实验名称 | Lab3:中断与异常 |
| 小组成员 | 张宸笛号—2412565、朱泽帅—2411560、马梓涵—2413902 |
| 实验实现完成日期 | 2026-11-04 |
| 实验报告最终更新日期 | 2026-11-08 |

### 小组分工

实验如何分工：

| 成员 | 负责的练习/模块 |
|---|---|
| 张宸笛号—2412565 | 功能模块二：中断与异常处理 |
| 朱泽帅—2411560 | 功能模块一：Trap 上下文恢复与 Trap 入口 |
| 马梓涵—2413902 | 早期 Trap 入口与上下文恢复初版 |

实验报告如何分工：

| 报告部分 | 负责人 |
|---|---|
| 第一至第三章 | 马梓涵—2413902 |
| 第四章 4.1 | 马梓涵—2413902 |
| 第四章 4.2 | 朱泽帅—2411560 |
| 第四章 4.3 | 张宸笛号—2412565 |
| 第五章 | 朱泽帅—2411560、张宸笛号—2412565 |
| 第六章 | 朱泽帅—2411560、张宸笛号—2412565 |

---

## 一、实验目的

本实验在 Lab2 已有的页表和物理内存管理基础上，引入中断与异常处理机制，使内核能够在正常执行被打断后保存现场，完成相应处理，再恢复状态继续运行。具体目标如下：

1. **理解 RISC-V 的 Trap 机制。** 区分中断和异常，了解 M、S、U 三种特权级及中断、异常委托的作用，掌握 `stvec`、`sepc`、`scause`、`stval` 和 `sstatus` 在 Trap 进入与返回过程中的职责。
2. **掌握上下文保存与恢复。** 理解 `struct trapframe` 的布局，以及通用寄存器和相关 CSR 的保存方式；补全与 `SAVE_ALL` 对应的 `RESTORE_ALL`，重点处理栈指针的恢复顺序，保证程序状态不被破坏。
3. **建立汇编入口与 C 处理函数的联系。** 完成 `__alltraps` 中的参数传递和函数调用，理解 RISC-V 调用约定、栈对齐及 `sret` 的作用，形成从 Trap 进入到处理完成后返回的完整流程。
4. **实现基本的中断和异常处理。** 通过时钟中断完成持续计时、统计输出和规定条件下的关机；正确识别非法指令与断点异常，并按指令长度调整返回地址，避免重复触发同一异常。
5. **理解中断保护与验证方法。** 了解修改内存管理等共享数据时保存、关闭和恢复中断状态的必要性；按照四段式结构编写提示词，通过编译、现场检查及运行测试验证实现，区分单个模块通过与完整实验通过。

---

## 二、实验环境

实验在 WSL2 Ubuntu 22.04 中完成，使用 RISC-V 交叉编译器 GCC 10.2.0、GNU ld 2.35.1、GNU Make 4.3 和 QEMU 6.2.0，QEMU 默认固件为 OpenSBI 0.9。测试使用的代码版本为 `lab3` 分支提交 `7d9948ec8e284856ee2689489cec782c09b6005b`。版本与命令见 [环境记录](../evidence/full-validation-20261105-7d9948e/environment.json) 和 [执行记录](../evidence/full-validation-20261105-7d9948e/commands.json)。

### AI 工具使用情况

| 成员 | AI 编程工具 | 底层模型 | 备注 |
|---|---|---|---|
| 张宸笛号—2412565 | Codex | gpt-6.1-sol | 已提供模块二 RELY 收集及四段式 Prompt |
| 朱泽帅—2411560 | Codex | gpt-6.1-sol | 模块一、SPEC、测试分析及报告整理 |
| 马梓涵—2413902 | Codex | gpt-6.1-sol | |

---

## 三、实验整体逻辑分析

### 3.1 本章节的逻辑主线

我们对本次实验的理解是：**让内核在程序被打断时，能够先保存当前状态，处理发生的事情，再恢复状态继续运行。** Lab2 解决了内存管理的问题，Lab3 则在此基础上加入中断和异常处理，使内核能够响应时钟等事件。

在 RISC-V 中，中断和异常统称为 Trap。中断是异步发生的，比如定时器到点，与当前执行的指令没有直接关系；异常由当前指令触发，比如非法指令和断点。在本实验中，它们都会先进入统一入口，再根据原因分别处理。

处理 Trap 时，硬件会先记录返回地址、原因和相关状态，然后跳到 `stvec` 指定的位置。但硬件不会自动把全部通用寄存器保存到栈里，所以还需要汇编代码保存这些值，在内核栈上形成一份 `struct trapframe`。我们把它理解为程序暂停时的一份现场记录，后面的 C 函数可以通过它判断发生了什么，恢复代码也可以根据它把程序状态还原。

实验中的两个模块正好对应这一流程的不同部分。模块一负责保存现场后的参数传递和处理后的现场恢复，模块二负责具体处理中断和异常。它们共用同一份现场，因此保存时放在哪个位置，处理和恢复时就必须从对应位置读取，不能各用一套布局。

相关代码的分工如下：

| 部分 | 关键文件/接口 | 主要作用 |
|---|---|---|
| 启动与初始化 | `kern/init/entry.S`、`kern/init/init.c`、`pmm_init()` | 准备内核运行环境，接入 Lab2 的内存管理代码 |
| 设置 Trap 入口 | `idt_init()`、`stvec` | 告诉 CPU 发生交给 S 模式处理的 Trap 后跳到哪里 |
| 保存与恢复现场 | `kern/trap/trapentry.S`、`struct trapframe` | 保存寄存器和相关状态，处理结束后恢复并返回 |
| 判断和处理事件 | `trap()`、`trap_dispatch()`、两个处理函数 | 区分中断与异常，执行相应处理 |
| 设置时钟 | `kern/driver/clock.c`、`clock_set_next_event()`、SBI | 设置下一次时钟事件，支持持续计时 |
| 保护关键操作 | `kern/driver/intr.c`、`kern/sync/sync.h` | 暂时关闭本核中断，操作结束后恢复原来的中断状态 |

把这些代码连起来，可以得到下面的主要流程：

```text
初始化：
kern_init()
  ├─ 清零 BSS、解析设备树、初始化控制台
  ├─ idt_init()：设置统一 Trap 入口
  ├─ pmm_init()：初始化物理内存管理
  ├─ clock_init()：允许时钟中断，设置第一次时钟事件
  └─ intr_enable()：打开 S 模式全局中断开关

一次交给 S 模式处理的 Trap：
硬件记录返回地址、原因和状态，跳转到 __alltraps
  → SAVE_ALL：在内核栈上保存现场
  → trap(tf)
  → trap_dispatch(tf)
      ├─ interrupt_handler(tf)：处理中断
      └─ exception_handler(tf)：处理异常
  → C 函数返回，进入 __trapret
  → RESTORE_ALL：恢复现场
  → sret：返回继续执行
```

初始化部分列出了主要步骤，框架实际上在内存初始化前后都调用了 `idt_init()`。这里重要的是先把入口和处理环境准备好，再打开中断。我们还需要区分两次“返回”：`trap()` 返回只是从 C 函数回到汇编代码，之后还要恢复现场，再通过 `sret` 结束这次 Trap。`sret` 根据保存的状态决定返回到 S 模式还是 U 模式，并不总是返回用户态。

### 3.2 功能的逐步实现

1. **先接入 Lab2 的代码。** Lab3 仍然需要之前完成的物理内存初始化和 Best-Fit，所以要把对应实现补到框架标有 Lab2 的位置。迁移时不能直接用旧文件覆盖新文件，还要保留 Lab3 新增的中断保护代码，为后续处理打好基础。

2. **设置入口，弄清楚硬件已经做了什么。** `idt_init()` 把 `__alltraps` 的地址写入 `stvec`，采用 Direct 模式，让相关中断和异常先到同一个入口。发生 Trap 时，`sepc` 记录返回位置，`scause` 记录原因，`stval` 提供附加信息，`sstatus` 保存与返回有关的状态。分清这些寄存器的作用，才能理解后面哪些信息需要保存、如何恢复。

3. **完成模块一，让汇编和 C 函数接上。** `SAVE_ALL` 已经按固定顺序保存 x0～x31 和四个 CSR 的值，RV64 下现场共占 288 字节。`__alltraps` 需要按照调用约定把现场地址传给 `trap()`，并保持栈对齐。C 函数返回后，`RESTORE_ALL` 按对应位置恢复寄存器、`sstatus` 和 `sepc`，最后恢复 `sp`，再由 `sret` 返回。我们认为这里最需要注意的是 `sp`：它既是要恢复的寄存器，又是读取现场时使用的地址，必须等其他内容读取完成后再恢复。C 函数如果合法修改了现场，也要使用修改后的值返回。

4. **完成模块二，分别处理中断和异常。** `trap_dispatch()` 根据 `scause` 的最高位判断类别，再调用相应处理函数。时钟中断要设置下一次事件、增加 `ticks`，定期输出统计信息，达到规定条件后关机。非法指令和断点异常则要输出信息，并按指令长度更新 `tf->epc`。如果返回地址没有调整，CPU 可能再次执行同一条异常指令，反复进入处理函数。其他未要求实现恢复的异常可以打印现场辅助排查，但打印现场本身并不代表问题已经解决。

5. **理解时钟为什么能持续触发，以及关键操作为什么要关中断。** SBI 的定时器接口一次设置一个事件，所以每次处理中断时还要安排下一次，才能持续计时。允许时钟这一类中断和打开 S 模式全局中断开关是两件事，都需要正确设置。修改空闲链表等数据时，要先保存原来的中断状态，关闭本核中断，完成后再恢复原状态。如果之前中断本来就是关闭的，就不能在操作结束后直接打开。这里保护的是本核中断可能带来的干扰，多核共享数据还需要其他同步措施。

6. **写清提示词，再检查整条处理流程。** 提示词按 `[PROMPT]`、`[RELY]`、`[GUARANTEE]`、`[SPECIFICATION]` 四部分组织，说明改哪里、能用什么、要完成哪些接口，以及不同情况下应该有什么结果。验证时除了看代码能否编译，还要检查现场地址是否传对、寄存器和栈指针是否恢复、异常能否正确跳过，以及时钟中断能否连续发生并返回。只有保存、处理、恢复和继续执行都正确，才能说明整个流程正常；单独一个文件编译成功还不够。

---

## 四、实验内容与实现

### 4.1 Lab2 前置功能迁移：物理内存初始化与 Best-Fit

#### 4.1.1 迁移内容

Lab3 需要在物理内存管理的基础上实现中断和异常处理，而提供的代码框架中还保留着 Lab2 的待填写部分。因此，我们先把 Lab2 已完成的 `page_init()` 和 Best-Fit 算法迁移到 Lab3 中，让内核能够正常初始化和分配物理页。

这部分主要涉及两个文件：

| 文件 | 主要修改 |
|---|---|
| `kern/mm/pmm.c` | 补全 `page_init()`，并将 `pmm_manager` 指向 `best_fit_pmm_manager` |
| `kern/mm/best_fit_pmm.c` | 补全空闲链表管理、空闲页统计及 Best-Fit 的初始化、分配、释放等函数 |

`page_init()` 负责确定哪些物理页可以使用。它根据 DTB 中的内存信息，排除内核和页描述符数组占用的空间，再把剩余的可用整页交给物理内存管理器。Best-Fit 则负责从这些空闲页中选择合适的内存块：分配时找到满足需求的最小空闲块，必要时拆分；释放时把页面放回空闲链表，并合并相邻的空闲块。

#### 4.1.2 迁移时的调整

迁移时主要处理了两处差异。

第一处是内存地址常量。Lab2 的 `page_init()` 使用了 `KERNEL_BEGIN_PADDR` 和 `PHYSICAL_MEMORY_END`，但 Lab3 中没有这两个定义。我们利用 Lab3 已有的 `KERNBASE`、`KERNTOP` 和 `PHYSICAL_MEMORY_OFFSET` 计算对应的物理地址边界，当前分别为 `0x80200000` 和 `0x88000000`，同时保留原来的 DTB 范围检查、溢出检查、页描述符容量检查和页面对齐处理。

第二处是中断保护。Lab3 的 `alloc_pages()`、`free_pages()` 和 `nr_free_pages()` 已加入 `local_intr_save`、`local_intr_restore`，用于在访问内存管理数据时保存、关闭和恢复中断状态。因此，我们只迁移 `pmm.c` 中缺少的部分，保留这些接口中的中断保护代码。`best_fit_pmm.c` 也采用同样的做法，保留框架已有的 `basic_check()` 和 `best_fit_check()`，避免重复定义。

#### 4.1.3 迁移检查

完成迁移后，我们分别强制编译了 `pmm.c` 和 `best_fit_pmm.c`，两个文件都编译通过，说明迁移后的代码能够与 Lab3 的头文件和接口配合使用。具体修改见 [`migration.patch`](../evidence/migration.patch)，编译记录见 [`make-migrated-forced.log`](../evidence/make-migrated-forced.log)。

迁移阶段的完整构建仍在 `trap.c` 处报错，当时还不能进行完整的运行测试。后续补全 Trap 相关代码后的测试结果放在第五章说明。

### 4.2 功能模块一：Trap 上下文恢复与 Trap 入口

**负责人：2411560—朱泽帅**

#### 4.2.1 功能与接口理解

本模块在 [`kern/trap/trapentry.S`](../code/kern/trap/trapentry.S) 中补全 `RESTORE_ALL` 与 `__alltraps`。课程已经提供 `SAVE_ALL`、`__trapret` 和 `sret`，并规定不能修改 `struct trapframe` 或 `void trap(struct trapframe *tf)` 的签名。

我将本模块理解为汇编与 C 之间的状态交接：CPU 进入 S-mode Trap 后，`SAVE_ALL` 在当前可用内核栈上保存现场；`__alltraps` 以指针形式将现场传给 `trap()`；处理函数正常返回后，`RESTORE_ALL` 从同一个现场恢复整数寄存器及返回所需的 CSR；最后由 `sret` 完成 Trap 返回。`trap()` 的普通 `ret` 仅返回汇编调用位置，不能代替 `sret`。

#### 4.2.2 Trap frame 布局与 SAVE_ALL 的对应关系

RV64 下 `REGBYTES=8`，`SAVE_ALL` 为现场分配 `36×8=288` 字节。`struct pushregs` 对应 x0～x31 的 32 个槽位，之后是 `status`、`epc`、`badvaddr` 和 `cause` 四个字段。核心偏移为：

| 内容 | 相对当前 `sp` 的偏移 | 作用 |
|---|---:|---|
| `gpr.ra`（x1） | 8 | 保留被中断代码原来的普通函数返回地址 |
| `gpr.sp`（x2） | 16 | 保留 Trap 前的栈指针 |
| `gpr.a0`（x10） | 80 | 保留原始第一个参数寄存器 |
| `status`（`sstatus`） | 256 | 为 `sret` 提供相关特权与中断状态 |
| `epc`（`sepc`） | 264 | 为 `sret` 提供返回地址 |
| `badvaddr`（`stval`） | 272 | 保存部分异常的附加信息 |
| `cause`（`scause`） | 280 | 供 C 处理函数判断 Trap 原因 |

`SAVE_ALL` 不能像保存其他通用寄存器那样直接保存 x2，因为划出 288 字节后 `sp` 已经改变。它先将原始 `sp` 写入 `sscratch`，再经由已保存原值的 `s0` 转存到第 2 个槽位。因此 `RESTORE_ALL` 必须按实际布局读取原始 x2，不能把当前 Trap frame 的地址当成原程序的 `sp`。

#### 4.2.3 RESTORE_ALL：上下文恢复的实现

恢复过程先从第 32、33 号槽位取出当前 Trap frame 中的 `status` 和 `epc`，写回 `sstatus`、`sepc`；再按保存偏移恢复 x1、x3～x31；最后才恢复 x2。其核心逻辑可由以下片段概括（完整实现见源码）：

```asm
#lab3 function1 2411560
LOAD s0, 32*REGBYTES(sp)
LOAD s1, 33*REGBYTES(sp)
csrw sstatus, s0
csrw sepc, s1
# ...按原槽位恢复其他通用寄存器...
LOAD x2, 2*REGBYTES(sp)
```

此处 `s0`、`s1` 是恢复 CSR 时的临时寄存器；它们的原始值此前也保存在 Trap frame，随后通过对应的通用寄存器加载恢复。`sp` 必须最后恢复，因为前面的加载都需要用当前 `sp` 定位 Trap frame。`x0` 恒为零，不需要恢复；`scause` 和 `stval` 用于处理与诊断，不是本次 `sret` 的返回输入，因此没有在此处写回。恢复 `sepc` 时读取的是 Trap frame 中**当前**的 `epc`，以便兼容 C 处理函数对返回地址的合法调整。

#### 4.2.4 __alltraps：汇编入口与 C 函数调用

`__alltraps` 在现有 `SAVE_ALL` 之后，将当前 `sp` 指向的 Trap frame 地址放入 `a0`，随后调用 C 语言的 `trap()`：

```asm
__alltraps:
    SAVE_ALL
    #lab3 function1 2411560
    move a0, sp
    call trap
```

RISC-V 调用约定使用 `a0` 传递第一个指针参数，因此传入的是 `struct trapframe *`，并非复制整个 288 字节结构体。原始 `a0` 和 `ra` 已在 `SAVE_ALL` 中保存，允许调用代码暂时使用这些寄存器。若进入 Trap 前的内核栈按 16 字节对齐，减去 288 字节仍保持对齐。`trap()` 正常返回后，控制流顺序进入 `__trapret`，由恢复宏和 `sret` 完成后续工作。

当前入口直接使用已有的内核栈，尚未实现用户栈到内核栈的切换，也没有完整处理嵌套 Trap。

#### 4.2.5 SPEC 的编写

编写 SPEC 前，我们先对照 `SAVE_ALL` 和 `trap.h` 确认栈帧布局，再结合 RISC-V 调用约定，明确恢复寄存器、传递参数和保持栈对齐的要求。两项功能共用同一个 Trap frame，因此放在同一份规格中说明。

| 规格对象 | Pre-Condition | Post-Condition / Case | 关键 Requirements |
|---|---|---|---|
| `RESTORE_ALL` | `sp` 指向有效 Trap frame，`trap()` 已正常返回 | 恢复原整数现场及 `sstatus`、`sepc`；当处理函数修改 `tf->epc` 时使用更新值 | 偏移与 `SAVE_ALL` 一致，临时寄存器最终恢复，x2 最后恢复，不重复执行 `sret` |
| `__alltraps` | 已进入入口，有效内核栈可供 `SAVE_ALL` 使用 | 保存现场并把其地址传给 `trap(tf)`；正常返回后进入 `__trapret` | 首个参数使用 `a0`，维持 16 字节对齐，保护原始 `a0`、`ra`，不改变固定接口 |

在规格中将正常返回与处理函数修改 `epc` 的情况分开，是为了防止实现只恢复 Trap 发生时的旧地址；特别明确 `sp` 的恢复顺序，是为了避免在还需要读取 Trap frame 时破坏内存基地址。完整的 Pre-Condition、Post-Condition、Case 与 Requirements 见 [功能模块一原始 Prompt](./prompt.md)。

#### 4.2.6 Prompt 的编写与 AI 协作过程

本题按照课程要求使用同一份四段式提示词，完整原文保存在 [功能模块一原始 Prompt](./prompt.md)。`[PROMPT]` 限定 `trapentry.S` 中两个 `lab3 function1` 标记位置，要求直接修改文件并保留其他代码；`[RELY]` 提供真实 `LOAD/STORE/REGBYTES` 宏、`SAVE_ALL` 的特殊保存关系、`trapframe` 字段和 `trap()` 接口；`[GUARANTEE]` 只列 `RESTORE_ALL` 和 `__alltraps` 两个交付功能；`[SPECIFICATION]` 规定 CSR、通用寄存器和 `sp` 的恢复状态、传参、栈对齐及不得修改的边界。

两处汇编填空共用同一个栈帧布局，所以我们用一份 Prompt 一起说明。生成代码后，先检查修改范围和寄存器偏移，再通过汇编编译和反汇编确认实现。过程中遇到的问题见 4.2.7，具体检查结果见 5.3。

#### 4.2.7 实现与检查中遇到的问题

模块一的主要问题出现在检查脚本和构建过程中：

1. **初始代码还未补全。** 对空白框架检查时，通过了 9/16 项；完成两处汇编填空后，原版本检查达到 16/16。
2. **脚本没有正确保存退出码。** 首次执行 shell 包装脚本出现 `exit: : numeric argument required`。排查发现是退出码变量传递有误，随后改用 Python 的 `subprocess.returncode` 记录子进程退出状态。
3. **检查脚本只识别一种调用写法。** 脚本原来匹配 `jal trap`，发布代码使用 `call trap`，导致一项检查未通过。重新汇编成功，查看反汇编和 `R_RISCV_CALL trap` 重定位后，确认调用关系正常。
4. **完整构建依赖模块二。** 当时 `trap.c` 中尚未实现 `interrupt_handler()` 和 `exception_handler()`，编译因隐式声明报错。补全模块二后，完整内核构建通过。

相关记录见 [`static-before.log`](../evidence/module1/static-before.log)、[`static-after.log`](../evidence/module1/static-after.log)、[`publication-checks.json`](https://github.com/iodwad/NKU_OS_2026/blob/9f292ecd0f239d3d44fe05f60156671f964f750f/lab3/evidence/module1/publication-checks.json) 和 [`kernel-build.log`](../evidence/module1/kernel-build.log)。

### 4.3 功能模块二：中断与异常处理

**负责人：2412565—张宸笛号**

#### 4.3.1 功能与接口理解

`trap(tf)` 接收汇编入口保存的现场，`trap_dispatch()` 将 `cause` 解释为有符号整数，按 RV64 的第 63 位将中断和异常交给不同函数。[`trap.c`](../code/kern/trap/trap.c) 中的 `interrupt_handler(tf)` 负责定时器和中断诊断，`exception_handler(tf)` 负责实验要求的异常。`trap_instruction_length(epc)` 是内部辅助函数。模块二改写 `tf->epc` 后，模块一从同一帧恢复 `sepc`，`sret` 才会使用新的地址。

#### 4.3.2 中断处理机制

`interrupt_handler()` 清除 `cause` 的第 63 位，再以原因编号分类。S 模式时钟中断的编号为 `IRQ_S_TIMER=5`。处理函数先调用 `clock_set_next_event()`，通过 SBI 将下一事件设置为 `rdtime + timebase`，再递增 `ticks`。`TICK_NUM` 为 100，每达到一个正整数倍就调用已有的 `print_ticks()`。

`SHUTDOWN_AFTER=10` 来自正式 Prompt 的暂定约定，源码也保留了暂定注释。它统计的是 `print_ticks()` 的调用次数，常规配置下对应第 1000 个 tick，随后调用 `sbi_shutdown()`。官方持续时钟用例要求到达 200 ticks，并未把 10 次统计规定为评分条件。原有 `DEBUG_GRADE` 条件分支仍由框架控制。

其他已知编号按软件、时钟、外部中断等名称输出，其中 S 模式软件中断输出 `Supervisor software interrupt`。未知编号输出原因值与 Trap frame。两类分支不重新设置时钟、不累加 `ticks`。打印名称仅提供诊断，设备应答、清除待决位与中断控制器处理需要相应驱动。

#### 4.3.3 异常处理机制

非法指令和断点分别打印 `Illegal instruction at`、`Breakpoint at` 与触发地址。辅助函数要求 `epc` 按半字对齐，且首半字落在 `[KERNBASE, etext)`。读取首半字后，在本实验的 16/32 位编码范围内，低两位为 `11` 时按 4 字节处理，否则按 2 字节处理。完整指令仍须在内核文本范围内才会推进 `tf->epc`。这一区别使普通 `EBREAK`、压缩 `C.EBREAK` 和测试中的 4 字节非法指令不必使用同一固定偏移。

地址不满足检查时，代码打印 Trap frame 并保留原 EPC。其他异常直接打印现场并返回。若同一条指令或取指条件保持不变，`sret` 后可能再次触发相同异常。这里的范围检查依赖当前内核文本映射可读，没有通用的用户地址或页表探测恢复机制。

#### 4.3.4 SPEC 的编写

两项规格的共同前置条件是 `tf` 指向有效 Trap frame。中断还依赖已初始化的时钟与有效的 `cause`。异常依赖保存的 `epc` 和异常编号。后置条件落在可核对的状态变化上，包括是否安排下次时钟、是否计数、是否更新 EPC，以及哪些字段保持原值。

| 接口与条件 | Case 1 | Case 2 | Case 3 | Requirements 与实现对应 |
|---|---|---|---|---|
| `interrupt_handler()` | S 模式时钟重新安排事件并计数，周期输出，按真实统计次数检查关机 | 其他已知类型按编号打印，不计时 | 未知编号打印原因和现场，保留 EPC 与计数 | 清除最高位后比较编号，也允许课程测试直接传入 15。计时路径复用框架接口，200 ticks 由测试读取真实计数 |
| `exception_handler()` | 非法指令输出地址，按编码和边界推进 EPC | 断点同样按 2 或 4 字节推进 EPC | 其余异常仅输出 Trap frame，保留现场 | 使用现有异常常量和固定输出子串。读指令前检查地址，只有可判定长度时才更新 `tf->epc` |

SPEC 中分别说明计数、输出、关机和返回地址的变化，方便逐项检查。对于只打印现场的分支，还要注意返回后可能再次触发同一个异常。

#### 4.3.5 Prompt 的编写与 AI 协作过程

模块二的提示词分两步编写。先整理代码中的宏、Trap frame、已有接口和测试输出要求，这一步只读取项目，不修改代码；再按四段式结构说明修改位置、接口约束和不同中断、异常的处理行为，并要求完成后编译和测试。两阶段提示词见 [原始 Prompt](./prompt.md)。

#### 4.3.6 实现过程与实际问题

模块二补全了 `interrupt_handler()` 和 `exception_handler()`，增加 `trap_instruction_length()` 判断指令长度，并设置统计输出和关机阈值。两个处理函数接入已有的 `trap_dispatch()`，无需修改汇编传参和返回接口。完成后，完整内核编译通过。

首次运行课程评分时，三组测试都未通过。查看启动日志后发现 OpenSBI 的内核交接地址为 `0x0`，测试没有进入内核。改用 `-kernel` 后，三组用例能够正常运行；随后将相同调整应用到评分脚本，最终得到 100/100。排查过程见第五章。

阅读代码时还注意到 `trap()` 上方沿用了 `iret` 的旧注释，本实验实际使用的返回指令是 `sret`。

#### 4.3.7 测试范围与实现不足

测试包含实际触发的断点、非法指令和时钟中断，也包含直接构造 `tf` 调用处理函数的回退分支。具体结果和覆盖范围放在第五章说明。

目前代码依赖有效的内核栈、可读的内核文本和 SBI 定时器。对于超出文本范围的 EPC、未知异常和设备中断，主要通过打印现场辅助排查，尚未实现通用的恢复处理。

---

## 五、测试与验证

测试分为编译检查、模块一检查和完整内核运行测试。首次执行原配置的 `make grade` 得到 0/100，排查后发现是 QEMU 与 OpenSBI 的启动交接问题。将评分脚本的启动参数改为 `-kernel` 后，三组课程用例全部通过，总分为 100/100。下面分别说明检查过程和问题的解决方法。

### 5.1 Lab2 迁移源文件编译

迁移后，先对 `kern/mm/pmm.c` 和 `kern/mm/best_fit_pmm.c` 强制重新编译，两个文件的退出码均为 0，确认地址常量和接口调整没有引入编译错误。编译记录见 [`make-migrated-forced.log`](../evidence/make-migrated-forced.log) 及对应的 `.exit` 文件，完整内核的运行测试在后续进行。

### 5.2 模块一静态检查与 Trap frame 布局验证

模块一重点检查保存与恢复的槽位是否一致、`sstatus` 和 `sepc` 的偏移是否正确、`sp` 是否最后恢复，以及入口是否正确传递参数。原版本静态检查通过 16/16 项，目标文件检查通过 8/8 项。

另外，使用交叉编译器编译 `layout_check.c`，检查 32 个通用寄存器槽位和 4 个 CSR 字段的偏移，以及结构体大小和对齐，编译通过。记录见 [`static-after.log`](../evidence/module1/static-after.log)、[`object-check.log`](../evidence/module1/object-check.log) 和 [`layout-compile.log`](../evidence/module1/layout-compile.log)。

### 5.3 模块一汇编编译、反汇编与发布核对

使用工程 Makefile 中的参数单独编译 `trapentry.S`，退出码为 0。再通过 `objdump` 和 `readelf` 检查生成的目标文件，确认代码按对应槽位恢复寄存器，最后恢复 `sp`，并将 Trap frame 地址放入 `a0` 传给 `trap()`。

发布版本使用 `call trap`，而最初的检查脚本只匹配 `jal trap`，导致脚本显示 15/16。重新汇编通过，反汇编中也能看到 `R_RISCV_CALL trap` 重定位，补充检查确认了调用关系。检查记录见 [`publication-checks.json`](https://github.com/iodwad/NKU_OS_2026/blob/9f292ecd0f239d3d44fe05f60156671f964f750f/lab3/evidence/module1/publication-checks.json)、[`publication-assemble.log`](https://github.com/iodwad/NKU_OS_2026/blob/9f292ecd0f239d3d44fe05f60156671f964f750f/lab3/evidence/module1/publication-assemble.log) 和 [`publication-objdump.log`](https://github.com/iodwad/NKU_OS_2026/blob/9f292ecd0f239d3d44fe05f60156671f964f750f/lab3/evidence/module1/publication-objdump.log)。

### 5.4 完整内核构建

模块二尚未补全时，完整构建在 `kern/trap/trap.c` 处报错，`make` 退出码为 2，主要信息如下：

```text
trap.c:89: implicit declaration of function 'interrupt_handler'
trap.c:92: implicit declaration of function 'exception_handler'
cc1: all warnings being treated as errors
```

原因是当时还没有实现两个处理函数。补全后，在独立副本中执行 `make -j2`，退出码为 0，成功链接 `bin/kernel` 并生成 `bin/ucore.img`。前后记录见 [`kernel-build.log`](../evidence/module1/kernel-build.log)、[`kernel-build.exit`](../evidence/module1/kernel-build.exit)、[构建标准输出](../evidence/full-validation-20261105-7d9948e/make.stdout.log)、[标准错误](../evidence/full-validation-20261105-7d9948e/make.stderr.log) 和 [退出码](../evidence/full-validation-20261105-7d9948e/make.exit)。

同步代码后，本地再次执行 `make -B -j2; echo "MAKE_EXIT=$?"`，强制重新编译。图1中可以看到链接、镜像生成和 `MAKE_EXIT=0`。

![图1 Lab3 完整内核编译和镜像生成](../evidence/figures/fig1_build.png)

*图1：完整内核编译通过，生成 `ucore.img`，退出码为 0。*

### 5.5 课程评分与启动问题排查

#### 5.5.1 原配置 `make grade` 的结果

首次按原配置执行 `make grade`，脚本分别编译三组测试镜像，使用 `-device loader,file=bin/ucore.img,addr=0x80200000` 启动 QEMU。三组测试都没有输出通过标记，最终得到 **`Total Score: 0/100`**，顶层 `make` 退出码为 2。记录见 [评分标准输出](../evidence/full-validation-20261105-7d9948e/grade.stdout.log)、[标准错误](../evidence/full-validation-20261105-7d9948e/grade.stderr.log) 和 [退出码](../evidence/full-validation-20261105-7d9948e/grade.exit)。

![图2 原配置 make grade 结果 0/100](../evidence/figures/fig2_official_grade.png)

*图2：首次评分的终端输出末段，总分为 0/100。首项测试结果见完整日志。*

随后查看启动输出，发现这三次运行都没有进入内核测试。我们因此先排查启动参数，再检查各组用例的运行情况。

#### 5.5.2 使用 `-kernel` 分别运行三组用例

保留课程测试源和对应的 `GRADE_LAB3_*` 宏，重新编译三组镜像，改用以下命令启动：

```sh
qemu-system-riscv64 -machine virt -nographic -bios default \
    -kernel bin/ucore.img -serial stdio -monitor none -no-reboot
```

这一步是在评分脚本外分别运行用例。三组均出现通过标记，与前面的原配置评分结果分开记录：

| 用例 | 原配置 `make grade` | `-kernel` 单独运行结果 | 运行日志 |
|---|---|---|---|
| Trap 上下文、断点及返回 | 0/35 | `[Test 1] ... PASS (35/35)` | [Context 日志](../evidence/full-validation-20261105-7d9948e/supplementary-context-run.stdout.log) |
| 非法指令与回退处理 | 0/25 | `[Test 2] ... PASS (25/25)` | [非法指令日志](../evidence/full-validation-20261105-7d9948e/supplementary-illegal-run.stdout.log) |
| S 模式持续时钟中断 | 0/40 | `[Test 3] ... PASS (40/40, ticks=200)` | [时钟日志](../evidence/full-validation-20261105-7d9948e/supplementary-timer-run.stdout.log) |

**Trap 上下文与断点。** 内核先输出 `check_alloc_page() succeeded!`，随后触发 `Breakpoint at 0xffffffffc0201a18`，处理后成功返回，并通过用例中的寄存器比较。

![图3 Trap 上下文与断点返回测试](../evidence/figures/fig3_context_breakpoint.png)

*图3：断点触发后成功返回，输出 `PASS (35/35)`。*

**非法指令与回退处理。** 用例触发 `Illegal instruction at 0xffffffffc02019c8`，处理后继续执行，并输出 `PASS (25/25)`。测试还构造 Trap frame，直接调用处理函数检查部分回退分支。

![图4 非法指令和回退处理测试](../evidence/figures/fig4_illegal_instruction.png)

*图4：非法指令处理后成功返回，输出 `PASS (25/25)`。*

**持续时钟。** 用例周期性输出 `100 ticks`，计数达到 200 时输出 `PASS (40/40, ticks=200)`，随后继续计时。累计十次统计输出后，调用 SBI 关机，QEMU 退出码为 0。这里的 200 ticks 是课程测试检查点，十次输出后关机是当前实现采用的设置。

![图5 S 模式持续时钟中断与正常关机](../evidence/figures/fig5_timer.png)

*图5：时钟中断持续触发，通过 200 ticks 检查，最后正常关机。*

前两组在输出通过标记后仍继续运行，由 `timeout 5s` 终止，因此截图末尾出现 `Terminated`；时钟组则正常关机。各次命令、退出码和关键输出见 [单独运行记录](../evidence/full-validation-20261105-7d9948e/supplementary-results.json)。

#### 5.5.3 OpenSBI 启动地址对比

对同一个 `bin/ucore.img` 分别使用两种启动方式，得到以下结果：

| 启动参数 | OpenSBI 输出的交接地址 | 运行情况 |
|---|---|---|
| `-device loader,file=bin/ucore.img,addr=0x80200000` | `Domain0 Next Address = 0x0` | 没有进入内核测试 |
| `-kernel bin/ucore.img` | `Domain0 Next Address = 0x80200000` | 进入内核并输出测试结果 |

在本次 QEMU 6.2/OpenSBI 0.9 环境中，原参数把镜像加载到了内存，但没有提供正确的内核交接地址。排查记录见 [启动串口](../evidence/full-validation-20261105-7d9948e/qemu-timer.serial.log)、[QEMU 追踪](../evidence/full-validation-20261105-7d9948e/diagnostic-qemu-trace.log) 和 [GDB 记录](../evidence/full-validation-20261105-7d9948e/diagnostic-gdb.stdout.log)。

![图6 OpenSBI 两种启动方式的交接地址对比](../evidence/figures/fig6_boot_comparison.png)

*图6：上方为 `-device loader`，交接地址为 `0x0`；下方为 `-kernel`，交接地址为 `0x80200000`。两次诊断运行均在三秒后由超时命令终止。*

#### 5.5.4 修改启动参数后重新评分

2026-11-05，我们在本地再次复现原配置的 0/100，并结合上述启动对比和 QEMU 6.2 源码确认原因。随后仅将评分脚本中的启动参数改为 `-kernel bin/ucore.img`，保留原测试用例、断言、35/25/40 权重和判分逻辑。

重新执行 `make clean`、`make -B -j2` 和 `make grade`，退出码均为 **0**，三组分别得到 **35/35、25/25、40/40，总分为 100/100**。这是修改启动参数后的评分结果。

之后又分别运行三份评分镜像，均输出对应 PASS 标记并以退出码 0 结束。Context 用例触发的是 2 字节 `C.EBREAK`，非法指令用例使用 4 字节编码，两者均能处理后返回。时钟用例通过 200 ticks 检查后继续计时，并在十次统计输出后正常关机。

[启动兼容性分析](../evidence/GRADE_COMPATIBILITY_REPORT.md) · [启动参数修改](../evidence/grade-compatibility-20261105-c35ad817/launch-fix.diff) · [修改后的评分输出](../evidence/grade-compatibility-20261105-c35ad817/compat-grade.stdout.log)

### 5.6 测试结果与不足

完成启动参数调整后，课程三组测试全部通过。测试验证了断点返回、非法指令跳过和持续时钟中断，也检查了部分现场恢复情况。

Context 用例显式比较 `t0–t6`、`s0–s11` 和 `a1–a7`，共 26 个整数寄存器，没有逐一比对 `ra`、`sp`、`gp`、`tp` 和 `a0`。测试汇编还直接改写了 `s` 寄存器，没有另存调用者原值；本次未观察到相关失败，后续完善测试时可以补上对调用约定的处理。

非法指令用例使用 4 字节编码，部分回退分支通过人工构造 Trap frame 测试，未经过完整的硬件入口和 `sret`。目前也没有全面测试各种断点编码、不可读 EPC、缺页恢复、设备中断应答、多核以及浮点和向量状态。

---

## 六、实验总结与收获

### 6.1 实验知识与 OS 原理的联系

| 实验机制 | 对应 OS 原理 | 本实验中的体现 |
|---|---|---|
| 中断与异常 | 异步事件与同步异常 | 定时器与当前指令无关，断点和非法指令由当前指令触发。两者使用统一 Trap 入口 |
| RISC-V 特权级 | 硬件保护与特权切换 | OpenSBI 运行于 M 模式，实验内核在 S 模式处理委托给它的 Trap |
| CSR 与 Trap frame | 硬件状态与软件保存的上下文 | `sepc`、`sstatus`、`scause` 和附加异常信息由硬件提供，汇编将它们与整数寄存器保存到栈上 |
| 保存与恢复 | 异步执行点的透明性 | 入口保存现场，返回前最后恢复 `sp`，让此前的加载始终使用同一个 Trap frame 基址 |
| `ret` 与 `sret` | 普通调用返回与特权返回 | C 函数先通过 `ret` 回到汇编。恢复 CSR 后，`sret` 使用 `sepc` 和状态位返回 |
| 汇编调用 C | RISC-V ABI | `a0` 传入 Trap frame 地址。288 字节帧保持已有的 16 字节栈对齐 |
| 定时器与 SBI | 固件接口与定时事件 | S 模式通过 SBI 设置下一次事件，连续中断来自每次重设而非计数输出 |
| 中断保护 | 临界区与本核并发 | 物理页分配保存原 `SIE` 状态，再临时屏蔽中断，完成后恢复。跨核共享状态还需同步机制 |
| 异常返回地址 | 异常的重试与跳过 | 实验选择跳过可判定长度的断点和非法指令，其他异常可能需要修复条件后重试 |

### 6.2 OS 原理中重要但本实验未覆盖的内容

本实验处理完 Trap 后继续原来的程序，还没有涉及进程或线程调度。后续实现调度时，需要保存和切换不同任务的状态。用户态系统调用还需要用户栈与内核栈切换、参数检查等支持，这些也不在本次实现范围内。

另外，本核关中断不能解决多核共享数据的同步问题，多核环境还需要锁等机制。嵌套中断需要进一步考虑栈和重入问题，缺页异常需要检查并修复映射，设备中断也需要完成设备应答和中断控制器操作。这些都需要在当前 Trap 框架上继续补充。

### 6.3 AI 协作与实验总结

这次实验让我们把 Trap 的保存、处理和恢复过程串了起来。模块一主要需要注意栈帧偏移、参数传递和 `sp` 的恢复顺序；模块二则需要正确判断中断与异常，并根据指令长度调整异常返回地址。迁移 Lab2 代码时，也要保留 Lab3 新增的中断保护，不能直接覆盖整个旧文件。

调试时，最初评分为 0 分。对比两种 QEMU 启动方式后，我们发现问题出在 OpenSBI 的内核交接地址。将评分脚本的启动参数改为 `-kernel` 后，保留原测试用例和判分逻辑，三组测试最终全部通过，总分为 100/100。这个过程也提醒我们，测试失败时要结合启动输出和内核日志定位原因。

AI 主要帮助整理接口、生成代码和分析问题。我们先按四段式结构说明已有条件和实现要求，生成代码后再对照栈帧布局检查，并通过编译、反汇编和运行测试确认结果。尤其是寄存器恢复顺序和异常返回地址，仍需要自己结合执行流程理解和检查。
