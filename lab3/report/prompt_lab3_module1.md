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