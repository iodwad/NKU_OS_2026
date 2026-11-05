# Lab3 实验原始 Prompt

以下收录团队仓库保存的两份实际使用过的 Prompt，保留原文内容。模块二的原始 Prompt 在当前提交中无可核实的记录。

## 一、Lab2 前置功能迁移 Prompt

````text
# Role

你是一名熟悉 RISC-V RV64、ucore、操作系统实验、Linux/WSL、Git 和 C/Assembly 的高级系统工程师。

现在重新建立我的 NKU OS 2026 Lab3 工程。

这次不要继续使用之前建立在 /home/andy 下的 Lab3 工作区。

我要直接在 Windows D 盘的课程目录中创建一个全新的 Lab3 文件夹，并在官方 Lab3 原始代码基础上，迁移已经完成的 Lab2 代码。

**本阶段需要实际创建工程、完成 Lab2 代码迁移并进行编译验证；但暂时不实现 Lab3 功能模块一和模块二。**

# 1. Target Directory

Windows 目标目录：

D:\college_study_materials\junior_year_fall\OS\lab3

WSL 对应路径：

/mnt/d/college_study_materials/junior_year_fall/OS/lab3

期望结构：

```text
OS/
├── lab1/
├── lab2/
├── lab3/
│   ├── code/
│   │   ├── kern/
│   │   ├── libs/
│   │   ├── tools/
│   │   └── Makefile
│   ├── report/
│   ├── evidence/
│   └── reference/
└── report_build/
```

注意：

1. 绝对不能修改原来的 lab1、lab2、report_build。
2. 先检查目标 lab3 是否已存在。
3. 如果已存在且包含文件，不得删除、覆盖或自动清理；使用新的非冲突目录并报告。
4. 后续的全部实验代码都放在这个新的 D 盘 Lab3 工程中。
5. 优先通过 WSL Ubuntu 完成构建，不要把 Windows PowerShell 和 Linux 路径混用。
6. 所有文件修改必须限制在新建的 Lab3 工程内。

# 2. Sources of Truth

课程官方网站：

http://oslab.mobisys.top/lab2026/

官方代码：

http://oslab.mobisys.top/lab2026/lab3.zip

团队 GitHub：

https://github.com/iodwad/NKU_OS_2026

已完成的 Lab2：

https://github.com/iodwad/NKU_OS_2026/tree/lab2

Lab2 参考提交：

ea3300cd028a0af8ddea475b093a2a8f9c7f10a8

课程 Lab3 骨架在 GitHub 上的导入提交：

d0bb450d845bfd33b9e4584855d9194d93a9ab5c

注意：团队 lab3 分支后续有人实现了 RESTORE_ALL 和 __alltraps。这次我希望自己重新完成，因此不能直接将队友已经完成的整个 lab3 分支复制为新工程。

# 3. Task A：创建全新 Lab3 工程

首先检查：

- D 盘 OS 目录
- 原 Lab2 目录
- 新 Lab3 目标路径
- WSL 和 RISC-V 工具链是否可用

创建目标目录。

优先从课程官网下载 lab3.zip，验证 ZIP 可以完整解压，检查内部目录结构和 Makefile。

保存未经修改的官方 ZIP 到 reference/。

从压缩包中定位真正的实验代码根目录，把内容放入 lab3/code/，不要出现 lab3/code/lab3/code 这种重复嵌套。

如果官网下载失败：

1. 检查是否已有本地官方 lab3.zip。
2. 如果仍不可用，可以从 GitHub 的 d0bb450 原始骨架提交取得 lab3/code 作为替代起点。
3. 明确说明此时使用的是 GitHub 导入版本，不能冒称已经验证与官方 ZIP 完全一致。

禁止直接采用团队当前 lab3 分支中已实现的 RESTORE_ALL 和 __alltraps。

创建 report/ 与 evidence/。

检查最终目录是否正确。

# 4. Task B：读取已经完成的 Lab2

重点读取 GitHub origin/lab2 中：

```text
lab2/code/kern/mm/pmm.c
lab2/code/kern/mm/best_fit_pmm.c
lab2/code/kern/mm/best_fit_pmm.h
lab2/code/kern/mm/memlayout.h
lab2/code/kern/mm/pmm.h
```

如果本地 D:\college_study_materials\junior_year_fall\OS\lab2 中有已完成代码，也要对照。

以 GitHub 上已经验证并提交的 Lab2 实现为主要迁移参考，避免误用本地历史草稿。

重点是：

- page_init()
- best_fit_init()
- best_fit_init_memmap()
- best_fit_alloc_pages()
- best_fit_free_pages()
- best_fit_nr_free_pages()
- best_fit_pmm_manager
- 相关 free_area 数据结构和宏

先对照 Lab3 的结构、接口、头文件与已有函数，确认需要迁移的最小代码范围。

不要直接将整个 Lab2 源文件覆盖到 Lab3。

# 5. Task C：实际迁移 Lab2 代码

现在可以修改新建的 Lab3 工程中的 Lab2 占位位置。

## C1. pmm.c

目标：

lab3/code/kern/mm/pmm.c

将 Lab2 中已完成的 page_init() 迁移到 Lab3 对应的 `lab2` 注释位置。

要求：

- 保留原有 lab2 注释。
- 保留 DTB 物理内存探测逻辑。
- 保留物理地址范围和溢出检查。
- 保留页描述符初始化。
- 保留空闲内存区初始化。
- 检查 Lab3 的地址常量是否与 Lab2 一致。
- 对 Lab3 缺失的常量做最小、可解释的等价适配。
- 保留 Lab3 已有的 local_intr_save/local_intr_restore 中断保护。
- 不覆盖 Lab3 原有的 alloc_pages、free_pages、nr_free_pages 封装。

如果迁移的 Best-Fit 完整且需要延续 Lab2 分配策略，将 pmm_manager 绑定为 best_fit_pmm_manager。

但必须解释选择这一 manager 的原因，并保留已有检查机制。

## C2. best_fit_pmm.c

目标：

lab3/code/kern/mm/best_fit_pmm.c

迁移 Lab2 已完成的 Best-Fit 算法与管理器定义。

要求：

- 补齐 free_area。
- 补齐 free_list/nr_free 宏。
- 迁移五个 Best-Fit 核心函数。
- 补齐 best_fit_pmm_manager。
- 保留 Lab3 官方已有的 basic_check() 和 best_fit_check()。
- 不重复定义函数。
- 不改变算法的最小合适空闲块选择原则。
- 确认内存分配、拆分、释放、相邻块合并均与原 Lab2 实现一致。

## C3. 迁移约束

不要迁移：

- Buddy System 扩展
- SLUB 扩展
- Lab2 报告和无关测试
- 与 Lab3 无关的调试补丁

所有迁移都要能够追溯到 Lab2 的源代码或明确的 Lab3 兼容性调整。

# 6. Task D：保持 Lab3 练习为原始待实现状态

我之后要自己完整完成 Lab3 的功能模块一。

因此本阶段：

- 不实现 RESTORE_ALL。
- 不实现 __alltraps。
- 不实现 interrupt_handler。
- 不实现 exception_handler。
- 不复制队友后续已完成的 Trap 代码。
- 不添加临时空函数欺骗编译器。
- 不修改 SAVE_ALL。
- 不修改 struct trapframe。
- 不修改 trap() 函数签名。

尤其检查：

```text
lab3/code/kern/trap/trapentry.S
lab3/code/kern/trap/trap.c
lab3/code/kern/trap/trap.h
```

确保这些文件仍处于官方课程骨架状态。

保留 `lab3` 练习标记和 `YOUR CODE`，留待下一阶段我实现后再将本人负责的 `YOUR CODE` 替换为学号。

注意：Lab2 已完成部分的原作者及学号注释应保留，不要更改为我的学号。

# 7. Task E：真实编译验证

在 WSL 中进入：

```bash
cd /mnt/d/college_study_materials/junior_year_fall/OS/lab3/code
```

首先检查：

```bash
which riscv64-unknown-elf-gcc
which qemu-system-riscv64
which make
```

然后执行：

```bash
make -j2
```

如果构建失败，请记录原始错误信息。

重点判断：

A. Lab2 迁移代码是否存在编译错误。

B. 是否存在接口、数据结构或宏定义不兼容。

C. 是否因为 Lab3 的 RESTORE_ALL、__alltraps 尚未实现而失败。

D. 是否因为模块二 interrupt_handler、exception_handler 未实现而失败。

必要时使用 Makefile 原有规则分别验证迁移源文件能否成功编译，不得修改官方 Makefile 来绕过问题。

严格区分：

- Lab2 迁移编译成功
- Lab3 整体编译成功
- Lab3 QEMU 运行成功

三者不能混为一谈。

本阶段不要求 make grade 达到满分，因为 Lab3 两个功能模块尚未完成。

# 8. Task F：保存本阶段真实证据

在新 Lab3 的 evidence/ 中保存：

- 官方代码来源与 SHA256
- Lab2 来源 commit SHA
- Lab2 迁移前后代码差异
- 实际编译命令
- 编译日志
- 编译退出码
- 发现的兼容性问题
- 修复过程与最终状态

只记录真实发生的操作，不得编造错误、修复过程或成功截图。

在 report/ 中创建一个简洁的 Markdown 实验记录初稿。

本阶段只记录：

- 实验环境
- 代码来源
- Lab2 迁移目标
- 实际使用的 Prompt
- 实际迁移情况
- 编译与验证状态

不要提前编造 Lab3 功能模块一的实现分析和测试结论。

# 9. Specification

必须遵守：

1. 所有新增实验工作只发生在 D:\college_study_materials\junior_year_fall\OS\lab3。
2. 不修改原 lab1、lab2。
3. 不修改此前 /home/andy 下的 Git 仓库或 Lab3 worktree。
4. 不删除或重置现有文件。
5. 不覆盖团队已提交的成果。
6. 不执行 git push。
7. 本阶段不需要创建 commit。
8. 保留课程原有 lab2、lab3 注释。
9. 完整迁移 Lab2 必需功能，但不提前实现 Lab3 功能模块。
10. 不修改官方评分脚本。
11. 不为了消除错误而删除中断或异常处理调用。
12. 不将尚未执行的测试描述为通过。

# 10. Deliverables

完成后输出中文报告，至少包含：

一、最终 D 盘 Lab3 目录结构及绝对路径。

二、官方 Lab3 代码来源与版本。

三、Lab2 迁移清单及每个修改文件的说明。

四、Lab2 与 Lab3 之间发现的接口差异及处理方案。

五、实际编译结果与未解决错误。

六、Lab3 功能模块一当前仍需要填写的位置。

七、下一阶段实现 RESTORE_ALL、__alltraps 的建议顺序。

完成创建、迁移和编译验证之后停止。

不要继续自动实现 Lab3 功能模块一或模块二。
````

## 二、功能模块一：Trap 上下文恢复与 Trap 入口

````text
[PROMPT]

**任务**：修改 `code/kern/trap/trapentry.S`，实现 RISC-V RV64 内核的 Trap 上下文恢复与 Trap 入口功能，包括 `RESTORE_ALL` 和 `__alltraps`。

本模块需要在已有 `SAVE_ALL` 的基础上，实现从 Trap 发生、上下文保存、进入 C 语言 `trap()` 处理函数，到恢复上下文并通过 `sret` 返回原执行流的完整过程。

**工程位置**：

Windows：
`D:\college_study_materials\junior_year_fall\OS\lab3`

WSL：
`/mnt/d/college_study_materials/junior_year_fall/OS/lab3`

**操作要求**：

1. 必须在项目中实际修改 `code/kern/trap/trapentry.S`，而不是仅展示代码。
2. 修改前先读取 `SAVE_ALL`、`trap.h` 和 `riscv.h`，确认真实寄存器布局、CSR 偏移和汇编宏定义。
3. 仅在两个 `#lab3 function1 YOUR CODE` 标记位置补充实现。
4. 保留已有的 `SAVE_ALL`、`__trapret` 和 `sret`。
5. 将本次负责位置的 `YOUR CODE` 替换为学号 `2411560`，保留 `lab3` 注释。
6. 不修改功能模块二、Lab2 已迁移代码、官方测试或其他成员的学号标记。

**输出要求**：

根据下面的 `[RELY]`、`[GUARANTEE]` 和 `[SPECIFICATION]` 完成实际代码修改。以项目真实源码为准，不引入无关功能。

实现后进行静态检查、汇编编译与反汇编核对。记录实际验证结果，不得将尚未完成的 QEMU 测试描述为通过。

本次只修改实验代码，不执行 Git commit 或 push。

[RELY]

**1. 可依赖的汇编宏**

定义位置：`code/libs/riscv.h`

在 RV64 环境下：

```c
#define STORE sd
#define LOAD ld
#define LOG_REGBYTES 3
#define REGBYTES (1 << LOG_REGBYTES)
```

因此每个寄存器槽位为 8 字节。

**2. 已有 SAVE_ALL**

定义位置：`code/kern/trap/trapentry.S`

关键保存关系：

```asm
csrw sscratch, sp
addi sp, sp, -36 * REGBYTES
```

随后按照寄存器编号，将 x0、x1、x3～x31 保存到对应的 `n*REGBYTES(sp)` 槽位。

原始 `sp` 经过 `sscratch` 暂存，再通过 `s0` 写入：

```asm
csrrw s0, sscratch, x0
STORE s0, 2*REGBYTES(sp)
```

相关 CSR 的保存关系为：

```asm
csrr s1, sstatus
csrr s2, sepc
csrr s3, sbadaddr
csrr s4, scause

STORE s1, 32*REGBYTES(sp)
STORE s2, 33*REGBYTES(sp)
STORE s3, 34*REGBYTES(sp)
STORE s4, 35*REGBYTES(sp)
```

以上为关键代码摘录。必须读取工程中完整的 `SAVE_ALL`，不能仅凭摘录推断其他寄存器的保存方式。

**3. Trap frame 结构**

定义位置：`code/kern/trap/trap.h`

```c
struct trapframe {
    struct pushregs gpr;
    uintptr_t status;
    uintptr_t epc;
    uintptr_t badvaddr;
    uintptr_t cause;
};
```

`struct pushregs` 按 x0～x31 的寄存器编号顺序组织字段，具体字段定义必须读取实际 `trap.h`。

在当前 RV64 布局下，Trap frame 共占用 36 个 8 字节槽位，即 288 字节。

**4. 可依赖的 C 语言接口**

```c
void trap(struct trapframe *tf);
```

定义于 `code/kern/trap/trap.c`。

**5. 已有 Trap 返回入口**

```asm
.globl __trapret
__trapret:
    RESTORE_ALL
    sret
```

`__trapret` 已负责执行恢复宏和特权级返回指令，不需要重新实现。

[GUARANTEE]

必须实现以下两个汇编接口：

**1. RESTORE_ALL**

```asm
.macro RESTORE_ALL
    #lab3 function1 2411560
    // 在这里实现寄存器与返回状态恢复
.endm
```

该宏负责从当前 Trap frame 恢复被打断程序的通用寄存器和返回所需 CSR，为后续 `sret` 提供正确状态。

上面的 `//` 说明文字仅表示实现位置，不是要求直接复制的汇编注释。

**2. __alltraps**

```asm
.globl __alltraps
.align(2)
__alltraps:
    SAVE_ALL

    #lab3 function1 2411560
    # 在这里实现 Trap frame 传参与 C 函数调用
```

该入口负责将已保存的 Trap frame 按照 RISC-V ABI 传递给 `trap(struct trapframe *tf)`，并在 C 函数正常返回后进入已有 `__trapret`。

不得修改现有公开接口，不需要新增辅助函数。

[SPECIFICATION]

## 1. RESTORE_ALL

**Pre-Condition**:

- CPU 正在执行内核的 Trap 返回路径。
- `SAVE_ALL` 已建立有效 Trap frame。
- 当前 `sp` 指向 Trap frame 的起始地址。
- Trap frame 的布局与 `struct trapframe` 一致。
- `trap()` 已正常返回，且没有破坏当前 Trap frame 的位置和有效性。
- Trap 处理函数可能已经合法修改 Trap frame 中的返回地址或状态。

**Post-Condition**:

- 恢复被打断程序的整数通用寄存器状态。
- 恢复 `sstatus` 和 `sepc`，使其反映 Trap frame 中当前保存的返回状态。
- 恢复原始 `sp`，使栈指针不再指向本次 Trap frame。
- 不改变 Trap frame 中未要求修改的内容。
- 恢复完成后，已有 `sret` 能依据恢复后的 CSR 执行 Trap 返回。

**Case 1：正常上下文恢复**

当 Trap frame 保存着被打断程序的有效状态时，按 `SAVE_ALL` 的真实内存布局恢复 x1～x31 及返回需要的 CSR。

x0 恒为零，不需要恢复。

用于临时恢复 CSR 的通用寄存器最终也必须恢复原值，不能把临时数据遗留给被打断程序。

**Case 2：Trap 处理函数修改了返回地址**

当 C 语言处理函数更新 `tf->epc` 后，恢复过程必须使用 Trap frame 中更新后的 `epc`，使返回地址由处理函数决定。

不得使用硬编码地址，也不得无条件使用 Trap 发生时未经处理的原始地址。

**Requirements**:

- 所有恢复偏移必须与已有 `SAVE_ALL` 完全一致。
- 原始 x2(sp) 保存在第2号槽位，必须特殊处理。
- 在完成其他所有依赖当前 `sp` 的内存访问后，才能恢复原始 `sp`。
- 不得在恢复 `sp` 后继续使用它访问 Trap frame。
- 不需要为了 `sret` 写回 `scause` 和 `sbadaddr`。
- 不得额外破坏寄存器、重复调整栈指针或在宏内部执行 `sret`。
- 不得修改 `SAVE_ALL` 或 `struct trapframe` 的布局。

## 2. __alltraps

**Pre-Condition**:

- CPU 已根据 `stvec` 进入 `__alltraps`。
- 当前执行环境具备有效且满足调用约定的内核栈。
- 已有 `SAVE_ALL` 可以将 Trap 发生时的整数寄存器与 CSR 状态保存到 Trap frame。
- `trap(struct trapframe *tf)` 是可供汇编调用的 C 语言接口。

**Post-Condition**:

- 已执行 `SAVE_ALL`，形成完整有效的 Trap frame。
- `trap()` 接收到正确的 `struct trapframe *` 参数。
- C 语言处理函数能够读取 Trap frame 中的寄存器、异常原因和返回地址。
- 在 `trap()` 正常返回时，执行流能够进入已有 `__trapret`。
- Trap frame 仍可供 `RESTORE_ALL` 正确读取和恢复。

**Case 1：正常进入与处理**

发生 Trap 后，入口保存被打断程序的状态，将当前 Trap frame 的起始地址作为 `trap()` 的第一个参数，并按照 RISC-V ABI 调用该函数。

传递的是 Trap frame 的地址，而不是整个结构体的副本。

**Case 2：C 语言处理函数正常返回**

当 `trap()` 完成处理并返回时，控制流应继续进入 `__trapret`，随后执行 `RESTORE_ALL` 和 `sret`。

期间不得丢失原程序的返回地址、栈指针或其他已保存的通用寄存器状态。

**Requirements**:

- 遵循 RISC-V ABI，第一个指针参数通过 `a0` 传递。
- C 函数调用时必须保持16字节栈对齐。
- 调用 C 函数不能覆盖尚未保存的原程序寄存器状态。
- 必须保证 `trap()` 正常返回后，`sp` 仍指向原 Trap frame。
- 不得修改已有 `SAVE_ALL`、`__trapret` 和 `sret`。
- 本实验当前只要求在已有有效内核栈的场景下完成 Trap 处理，不擅自加入用户态独立内核栈切换或额外调度功能。

## 3. 正确性验证要求

实现完成后，执行以下检查：

**静态检查**

- 检查所有寄存器的保存与恢复关系。
- 检查 CSR 的保存偏移与恢复偏移。
- 确认 x2(sp) 最后恢复。
- 确认 Trap frame 大小、内存布局及调用栈对齐。
- 确认 `trap()` 的参数与 `struct trapframe *` 一致。
- 确认仅修改两个指定实现位置。

**编译检查**

- 使用工程现有 RISC-V 交叉编译工具链编译 `trapentry.S`。
- 检查生成的目标文件与反汇编。
- 保存实际编译日志及退出状态到 `evidence/module1/`。
- 如果完整内核因功能模块二尚未实现而无法链接，必须单独说明，不允许通过添加假处理函数或修改评分脚本绕过。

**实验记录**

- 将本次实际使用的完整 Prompt 保存至 `report/prompt_lab3_module1.md`。
- 记录具体修改位置、生成代码、真实编译结果、实际出现的问题与修复过程。
- 不得编造 Bug、运行结果或测试通过记录。
- 不执行 commit 或 push。

完成后输出中文总结，说明两个接口的实现、遵守的关键约束、实际验证结果与尚未完成的运行测试，然后停止。
````
