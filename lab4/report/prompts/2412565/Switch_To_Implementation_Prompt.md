# OS Lab4 组员 C · `switch_to()` 实现提示词

## [RELY]

[`proc.h`](../../../code/kern/process/proc.h) 中的切换现场布局如下。

```c
struct context
{
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

当前内核的目标是 RV64，每个 `uintptr_t` 字段占 8 字节。在 `proc_run()` 中传入两个 `struct context *`，汇编入口分别从 `a0` 和 `a1` 接收它们。

| 寄存器 | 字节偏移 | 寄存器 | 字节偏移 |
|---|---:|---|---:|
| `ra` | 0 | `s5` | 56 |
| `sp` | 8 | `s6` | 64 |
| `s0` | 16 | `s7` | 72 |
| `s1` | 24 | `s8` | 80 |
| `s2` | 32 | `s9` | 88 |
| `s3` | 40 | `s10` | 96 |
| `s4` | 48 | `s11` | 104 |

[`proc.c`](../../../code/kern/process/proc.c) 中的 `copy_thread()` 将新线程的 `context.ra` 设为 `forkret`，将 `context.sp` 设为新线程自身栈上的 trapframe 地址。`proc_run()` 已在调用前设置 `current` 和页表。[`trapentry.S`](../../../code/kern/trap/trapentry.S) 提供 `forkrets` 和 `__trapret`，用于新线程第一次恢复。

## [GUARANTEE]

在 `lab4/code/kern/process/switch.S` 的 C 组区域导出 `switch_to` 汇编符号，使其符合 `proc.c` 的声明及现有 RV64 调用约定。

```c
void switch_to(struct context *from, struct context *to);
```

该接口保存旧线程的调用现场并恢复目标线程的调用现场，沿用现有公共结构体布局。

## [SPECIFICATION]

### `switch_to()`

**Pre-Condition**

`from` 指向旧线程可写的上下文存储，`to` 指向目标线程已准备好的上下文。调用者已完成 `current` 和页表切换，并在需要的临界区内调用。

**Post-Condition**

旧线程的返回地址、栈指针和全部 `s0`–`s11` 寄存器完整保存到 `from`。CPU 装入 `to` 中对应的寄存器值，并从恢复的返回地址继续执行。

**Case 1 · 恢复已有线程**

目标线程曾主动切出时，沿保存的返回地址恢复它上一次调用 `switch_to()` 之后的执行。

**Case 2 · 首次运行新线程**

目标线程首次运行时，沿初始化现场进入 `forkret`，使用已经准备好的内核栈，再进入已有的 trapframe 恢复链路。

**Requirements**

- 按 `struct context` 的实际字段偏移进行 64 位保存和恢复。旧现场保存完毕后才开始恢复新现场。
- 进程选择、页表和中断开关状态由调用链的上层负责。
- 对照编译后的上下文偏移与汇编指令核对全部寄存器，再用现有启动和调度测试验证新线程的首次进入与已有线程的恢复。
