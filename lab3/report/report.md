# 操作系统实验报告

## 实验基本信息

| 项目 | 内容 |
|---|---|
| 实验名称 | Lab3:中断与异常 |
| 小组成员 | 张宸笛号—2412565、朱泽帅—2411560、马梓涵—2413902 |
| 完成日期 | |

### 小组分工

实验如何分工：

| 成员 | 负责的练习/模块 |
|---|---|
| 张宸笛号—2412565 | |
| 朱泽帅—2411560 | |
| 马梓涵—2413902 | |

实验报告如何分工：

| 报告部分 | 分工 |
|---|---|
| | |
| | |
| | |

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

### AI 工具使用情况

| 成员 | AI 编程工具 | 底层模型 | 备注 |
|---|---|---|---|
| 张宸笛号—2412565 | | | |
| 朱泽帅—2411560 | | | |
| 马梓涵—2413902 | | | |

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

本章沿用 Lab2 报告中“功能与接口理解—SPEC—Prompt—实现核对—问题记录”的组织方式。模块二按当前源码说明处理流程，运行结果见第五章的验证范围。

### 4.1 Lab2 前置功能迁移：物理内存初始化与 Best-Fit

#### 4.1.1 迁移目标与接口关系

Lab3 依赖 Lab2 的物理内存管理基础。课程骨架在 `kern/mm/pmm.c` 和 `kern/mm/best_fit_pmm.c` 保留了 `lab2` 填写位置，而 `pmm_init()` 仍依赖 `page_init()`，物理页分配则依赖 `pmm_manager` 的回调。迁移使用的来源为团队仓库 `lab2` 分支提交 `ea3300cd028a0af8ddea475b093a2a8f9c7f10a8`，不是从已有 Lab3 工作分支复制其他同学的 Trap 实现。

课程骨架下载失败后，工程采用团队仓库的导入提交 `d0bb450d845bfd33b9e4584855d9194d93a9ab5c`。下载错误及来源见 [来源记录](../evidence/source.json)，该导入版本与课程 ZIP 的一致性未经核验。迁移时使用的 [原始 Prompt](./prompt.md) 已完整保留。

迁移范围如下：

| 目标文件 | 迁移内容 | 保留的 Lab3 原有内容 |
|---|---|---|
| [`kern/mm/pmm.c`](../code/kern/mm/pmm.c) | `page_init()`；将 `pmm_manager` 绑定到已完成的 Best-Fit | `alloc_pages()`、`free_pages()`、`nr_free_pages()` 中的中断状态保存与恢复 |
| [`kern/mm/best_fit_pmm.c`](../code/kern/mm/best_fit_pmm.c) | `free_area`、`free_list/nr_free`、五个核心函数与 `best_fit_pmm_manager` | 骨架已提供的 `basic_check()`、`best_fit_check()` |

`page_init()` 使用 DTB 提供的物理内存范围，在排除内核镜像与页描述符数组后登记可用整页。Best-Fit 则在足够大的空闲块中选取最小者，分配时按需拆分，释放时尝试合并相邻空闲块。两者分别负责可分配范围的建立和空闲块的选取，不能相互代替。

#### 4.1.2 迁移过程中的兼容性处理

Lab2 与 Lab3 的部分映射常量不同：旧 `page_init()` 使用的 `KERNEL_BEGIN_PADDR` 和 `PHYSICAL_MEMORY_END` 未在 Lab3 相应头文件中定义。本次没有整体覆盖 Lab3 的 `memlayout.h`，而是依据现有的 `KERNBASE`、`KERNTOP`、`PHYSICAL_MEMORY_OFFSET` 在 `page_init()` 内计算等价物理边界。当前映射下分别为 `0x80200000` 和 `0x88000000`。DTB 范围验证、地址溢出检查、页描述符容量检查以及页面对齐逻辑均保留。

Lab3 已为物理页分配接口增加 `local_intr_save/local_intr_restore`，因此不能把 Lab2 的 `pmm.c` 整文件覆盖过去，否则会丢失本实验新增的中断保护。Best-Fit 中也只迁移缺失的算法部分，避免重复定义课程检查函数。相关前后差异见 [`evidence/migration.patch`](../evidence/migration.patch)。

#### 4.1.3 迁移核对与验证边界

迁移后的 `pmm.c` 与 `best_fit_pmm.c` 均经过独立强制编译，退出码为 0；原始记录见 [`make-migrated-forced.log`](../evidence/make-migrated-forced.log)。这一结果只能说明迁移代码在当前工具链与头文件下能够编译，**不能据此宣称 Lab3 已通过物理内存管理的运行时检查**。迁移阶段的完整构建曾在 `trap.c` 处失败，见第 5.4 节。

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

该入口直接使用当前栈，没有实现从 U-mode 栈切换到独立内核栈；此处不能扩展宣称支持完整的用户态 Trap 入口或任意嵌套 Trap。

#### 4.2.5 SPEC 的编写

本题的 SPEC 首先来自已有 `SAVE_ALL`、`trap.h` 与 RISC-V ABI，而不是从预先选定的汇编语句倒推。两项功能虽然各有职责，却共同依赖同一 Trap frame，因此在同一份规格中定义了输入条件、成功后的状态及关键时序约束。

| 规格对象 | Pre-Condition | Post-Condition / Case | 关键 Requirements |
|---|---|---|---|
| `RESTORE_ALL` | `sp` 指向有效 Trap frame，`trap()` 已正常返回 | 恢复原整数现场及 `sstatus`、`sepc`；当处理函数修改 `tf->epc` 时使用更新值 | 偏移与 `SAVE_ALL` 一致，临时寄存器最终恢复，x2 最后恢复，不重复执行 `sret` |
| `__alltraps` | 已进入入口，有效内核栈可供 `SAVE_ALL` 使用 | 保存现场并把其地址传给 `trap(tf)`；正常返回后进入 `__trapret` | 首个参数使用 `a0`，维持 16 字节对齐，保护原始 `a0`、`ra`，不改变固定接口 |

在规格中将正常返回与处理函数修改 `epc` 的情况分开，是为了防止实现只恢复 Trap 发生时的旧地址；特别明确 `sp` 的恢复顺序，是为了避免在还需要读取 Trap frame 时破坏内存基地址。完整的 Pre-Condition、Post-Condition、Case 与 Requirements 见 [功能模块一原始 Prompt](./prompt.md)。

#### 4.2.6 Prompt 的编写与 AI 协作过程

本题按照课程要求使用同一份四段式提示词，完整原文保存在 [功能模块一原始 Prompt](./prompt.md)。`[PROMPT]` 限定 `trapentry.S` 中两个 `lab3 function1` 标记位置，要求直接修改文件并保留其他代码；`[RELY]` 提供真实 `LOAD/STORE/REGBYTES` 宏、`SAVE_ALL` 的特殊保存关系、`trapframe` 字段和 `trap()` 接口；`[GUARANTEE]` 只列 `RESTORE_ALL` 和 `__alltraps` 两个交付功能；`[SPECIFICATION]` 规定 CSR、通用寄存器和 `sp` 的恢复状态、传参、栈对齐及不得修改的边界。

选择以**一个模块、一份 Prompt**完成两处汇编填空，是因为参数传递与恢复共享一个现场布局，单独生成其中一个功能可能遗漏对另一个功能的依赖。AI 生成后没有直接将“编译成功”视为闭环正确，而是保留源文件前后差异、静态核对、汇编对象和反汇编输出，核对过程见 [4.2.7](#427-ai-生成及核验过程中实际遇到的问题)，汇编与发布证据见 [5.3](#53-模块一汇编编译反汇编与发布核对)。

#### 4.2.7 AI 生成及核验过程中实际遇到的问题

本次没有证据表明生成的 `RESTORE_ALL` 存在已确认的语义错误。实际记录的问题需要按性质区分：

1. **原始骨架未填写。** 首次对空白骨架执行静态检查为 9/16，通过项不涉及尚缺失的两项功能。这说明任务尚未实现，不应描述成 AI 生成后的 Bug。填入模块一代码后，原版本静态检查达到 16/16。
2. **验证命令的退出码保存错误。** 首次 shell 包装脚本出现 `exit: : numeric argument required`，属于退出码变量传递问题。随后改用 Python 的 `subprocess.returncode` 保存实际子进程退出状态，不需要修改内核汇编。
3. **发布版本的测试脚本与指令写法不匹配。** 最初检查脚本匹配 `jal trap`，后续发布代码采用 `call trap`，使历史脚本出现 15/16。重新汇编退出码为 0，反汇编中确认 `R_RISCV_CALL trap` 重定位；对当前调用形式的独立检查通过。该不匹配来自验证脚本对指令写法的过度限定，不能简单认定为 Trap 入口功能错误。
4. **模块一实现阶段的完整内核构建失败。** 当时的 `trap.c` 尚未定义 `interrupt_handler` 和 `exception_handler`，隐式声明在 `-Werror` 下导致 `make` 退出码为 2。该日志对应模块二处理函数提交前的版本。

原始日志分别见 [`static-before.log`](../evidence/module1/static-before.log)、[`static-after.log`](../evidence/module1/static-after.log)、[`publication-checks.json`](https://github.com/iodwad/NKU_OS_2026/blob/9f292ecd0f239d3d44fe05f60156671f964f750f/lab3/evidence/module1/publication-checks.json) 和 [`kernel-build.log`](../evidence/module1/kernel-build.log)。

### 4.3 功能模块二：中断与异常处理

团队提交 `0de568894a7787cca404ca1b524b23febc4e79b5` 在 [`kern/trap/trap.c`](../code/kern/trap/trap.c) 中实现了 `interrupt_handler()` 和 `exception_handler()`。`trap_dispatch()` 根据 `scause` 的最高位分派给两个处理函数。

Supervisor Timer 中断先调用 `clock_set_next_event()`，再递增 `ticks`。每累计 100 ticks 输出一次统计，当前代码累计输出 10 次后调用 `sbi_shutdown()`。断点和非法指令处理会打印异常地址，检查 `epc` 的对齐和内核代码范围，再读取指令首半字，按 2 字节或 4 字节更新 `tf->epc`。无法安全判定长度时打印 Trap frame，保留原返回地址。

以上行为来自源码核对。模块二的原始 Prompt、调试过程和运行结果待补充。

---

## 五、测试与验证

本章分别记录迁移和模块一阶段的构建证据，以及模块二提交后的验证范围。**报告所依据的记录未包含模块二提交后的 QEMU/GDB 或 Lab3 `make grade` 结果。**

### 5.1 Lab2 迁移源文件编译

迁移后的 `kern/mm/pmm.c` 与 `kern/mm/best_fit_pmm.c` 已通过强制重新编译，退出码为 0。该检查用于确认 Lab2 功能迁移没有因头文件或地址常量差异造成即时编译错误。证据为 [`make-migrated-forced.log`](../evidence/make-migrated-forced.log) 及对应的 `.exit` 文件。它不是迁移后的内存分配运行测试，也不继承 Lab2 既往 `make grade` 分数作为 Lab3 的新结果。

### 5.2 模块一静态检查与 Trap frame 布局验证

模块一实现阶段以课程骨架为基准，验证了保存与恢复槽位、`sstatus/sepc` 位置、x2 最后恢复、参数传递与修改范围。原版本静态检查 16/16；目标文件检查 8/8。使用 RISC-V 交叉编译器编译专门的 `layout_check.c`，通过 32 个 GPR、4 个 CSR 字段偏移和结构体大小/对齐的编译期检查，退出码为 0。相关记录见 [`static-after.log`](../evidence/module1/static-after.log)、[`object-check.log`](../evidence/module1/object-check.log)、[`layout-compile.log`](../evidence/module1/layout-compile.log)。这些属于静态及编译期证据，不能替代真实 Trap 执行后的寄存器值比较。

### 5.3 模块一汇编编译、反汇编与发布核对

使用工程 Makefile 的编译参数单独构建 `trapentry.S`，实际汇编退出码为 0。`objdump/readelf` 证据显示：保存的 CSR 与 GPR 按预期槽位恢复，最后恢复 `sp`；入口将当前 Trap frame 地址置入 `a0`。发布版本采用 `call trap`，重新汇编退出码仍为 0；反汇编检查到 `R_RISCV_CALL trap` 重定位，说明对象文件保留了对 C 语言 `trap` 符号的正常调用关系。

发布后 [`publication-checks.json`](https://github.com/iodwad/NKU_OS_2026/blob/9f292ecd0f239d3d44fe05f60156671f964f750f/lab3/evidence/module1/publication-checks.json) 记录历史脚本 15/16，原因是原脚本固定匹配 `jal trap`，而当前实现为 `call trap`；独立调用检查通过。此处应把“核验脚本的字面匹配失败”和“汇编功能检查失败”分开描述。相关证据为 [`publication-assemble.log`](https://github.com/iodwad/NKU_OS_2026/blob/9f292ecd0f239d3d44fe05f60156671f964f750f/lab3/evidence/module1/publication-assemble.log)、[`publication-objdump.log`](https://github.com/iodwad/NKU_OS_2026/blob/9f292ecd0f239d3d44fe05f60156671f964f750f/lab3/evidence/module1/publication-objdump.log)。

### 5.4 完整内核构建及失败归因

模块二提交前，实际执行完整内核构建时，`make` 退出码为 2，编译停在 `kern/trap/trap.c`，未进入链接。主要错误为：

```text
trap.c:89: implicit declaration of function 'interrupt_handler'
trap.c:92: implicit declaration of function 'exception_handler'
cc1: all warnings being treated as errors
```

该日志记录了两个处理函数尚未实现时的失败。当前源码已包含两者，报告尚无对应版本的完整内核构建记录。历史输出及退出状态见 [`kernel-build.log`](../evidence/module1/kernel-build.log) 和 [`kernel-build.exit`](../evidence/module1/kernel-build.exit)。

### 5.5 课程评分项目与后续运行验证范围

当前 [`tools/grade.sh`](../code/tools/grade.sh) 定义了三组评分项目：

| 评分项目 | 分值 | 需要验证的行为 | 当前状态 |
|---|---:|---|---|
| Trap 入口、寄存器恢复与断点返回 | 35 | `ebreak` 处理后正确恢复现场并继续执行 | 尚未运行 |
| 非法指令及异常返回 | 25 | 处理非法指令、正确推进 `epc`，并覆盖回退分支 | 尚未运行 |
| 持续 Supervisor Timer | 40 | 重设时钟事件、连续处理中断并累计至 200 ticks | 尚未运行 |

这些项目同时依赖模块二的处理逻辑。后续应在核对当前版本的完整内核构建后执行 `make grade`，保留原始终端输出；必要时用 QEMU/GDB 检查 `trap(tf)` 入口参数、`__trapret` 前后的寄存器、`sepc` 和 `sret` 返回位置。在实际执行前，本报告不填写分数、不提供虚构截图，也不把静态检查的 16/16 当作正式评分结果。

### 5.6 阶段性结论与未覆盖范围

Lab2 迁移的源文件已通过单独编译，模块一的 `RESTORE_ALL` 和 `__alltraps` 已完成静态布局与汇编对象核对。模块二已有处理函数，目前有源码可核对。当前版本的完整内核构建、实际 Trap 的上下文恢复、断点与非法指令返回，以及持续时钟中断的测试结果仍待补充。
