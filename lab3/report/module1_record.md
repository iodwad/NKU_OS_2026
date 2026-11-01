# Lab3 功能模块一：Trap 入口与上下文恢复记录

日期：2026-11-01；学号：2411560。

## 修改范围与实现

本次实际使用的完整任务 Prompt 原文保存在 [prompt_lab3_module1.md](prompt_lab3_module1.md)，与用户附件的 SHA256 均为 `86bfea717ef299e3dcefb935b28d1c56f77697134f20538f090e929e502ab7bc`。

修改前已读取完整的 `code/kern/trap/trapentry.S`、`code/kern/trap/trap.h`、`code/libs/riscv.h`，并核对 `trap.c` 接口及 Makefile。源码中，RV64 的 STORE/LOAD 分别展开为 sd/ld，每槽 8 字节；pushregs 按 x0～x31 排列，后接 status、epc、badvaddr、cause，Trap frame 共 288 字节。

代码仅补充 `code/kern/trap/trapentry.S` 中两个原 `#lab3 function1 YOUR CODE` 位置，并将这两处标记改为 `#lab3 function1 2411560`（修改后位于第60、105行）。开始任务时文件已有三处解释 SAVE_ALL、RESTORE_ALL、sret 的行尾注释，本次原样保留。功能模块二、Lab2 迁移代码、官方测试和其他成员标记均未修改；实现阶段没有执行 Git commit 或 push，后续发布阶段另行记录。

`RESTORE_ALL` 从第32、33槽读取 Trap frame 当前的 status、epc，用 s0、s1 暂存并写回 sstatus、sepc。随后按各自槽位恢复 x1、x3～x31，包含先前用于暂存 CSR 的 s0、s1；最后从第2槽恢复 x2。恢复 sp 后没有继续访问 Trap frame，也没有再调整 sp。宏不写回 scause、sbadaddr，不写 Trap frame，不执行 sret。

`__alltraps` 在已有 SAVE_ALL 之后执行 `move a0, sp`、`call trap`，把当前 Trap frame 地址作为第一个参数。调用使用的 ra 已经由 SAVE_ALL 保存，trap 正常返回后顺序进入原有 __trapret。入口栈满足16字节对齐的前提下，减去288字节仍保持对齐。没有增加用户栈切换或调度逻辑。

本次生成的完整实现如下，完整源文件快照另存为 `evidence/module1/trapentry.after.S`。

```asm
    .macro RESTORE_ALL

    #lab3 function1 2411560
    LOAD s0, 32*REGBYTES(sp)
    LOAD s1, 33*REGBYTES(sp)
    csrw sstatus, s0
    csrw sepc, s1

    LOAD x1, 1*REGBYTES(sp)
    LOAD x3, 3*REGBYTES(sp)
    LOAD x4, 4*REGBYTES(sp)
    LOAD x5, 5*REGBYTES(sp)
    LOAD x6, 6*REGBYTES(sp)
    LOAD x7, 7*REGBYTES(sp)
    LOAD x8, 8*REGBYTES(sp)
    LOAD x9, 9*REGBYTES(sp)
    LOAD x10, 10*REGBYTES(sp)
    LOAD x11, 11*REGBYTES(sp)
    LOAD x12, 12*REGBYTES(sp)
    LOAD x13, 13*REGBYTES(sp)
    LOAD x14, 14*REGBYTES(sp)
    LOAD x15, 15*REGBYTES(sp)
    LOAD x16, 16*REGBYTES(sp)
    LOAD x17, 17*REGBYTES(sp)
    LOAD x18, 18*REGBYTES(sp)
    LOAD x19, 19*REGBYTES(sp)
    LOAD x20, 20*REGBYTES(sp)
    LOAD x21, 21*REGBYTES(sp)
    LOAD x22, 22*REGBYTES(sp)
    LOAD x23, 23*REGBYTES(sp)
    LOAD x24, 24*REGBYTES(sp)
    LOAD x25, 25*REGBYTES(sp)
    LOAD x26, 26*REGBYTES(sp)
    LOAD x27, 27*REGBYTES(sp)
    LOAD x28, 28*REGBYTES(sp)
    LOAD x29, 29*REGBYTES(sp)
    LOAD x30, 30*REGBYTES(sp)
    LOAD x31, 31*REGBYTES(sp)
    LOAD x2, 2*REGBYTES(sp)

    .endm

    # __alltraps 中 SAVE_ALL 后新增：
    #lab3 function1 2411560
    move a0, sp
    call trap
```

## 验证方式与结果

下表记录前一实现阶段的验证；当前发布版本使用 `call trap`，发布检查见本节末尾。

使用 WSL Ubuntu-22.04 的 `/usr/bin/riscv64-unknown-elf-gcc` 10.2.0 和配套 binutils。工具链默认目标为 rv64imafdc、lp64d。构建沿用工程 Makefile 规则，仅通过命令行把 OBJDIR/BINDIR 指向 `evidence/module1/build/`，未改 Makefile，也未覆盖原有 obj/bin。该目录保留本次目标文件及完整内核构建失败时生成的对象，用于审查。

| 检查 | 实际结果 | 证据（相对 evidence/module1） |
|---|---|---|
| 修改前空实现检查 | 9/16，通过项不含缺失功能；退出码1 | static-before.log、static-before.exit、trapentry.before.S |
| 修改后静态与符号模型检查 | 16/16，退出码0 | static-after.log、static-after.exit、verify_static.py |
| 单独汇编 trapentry.S | 退出码0 | assemble.log、assemble.exit、build/obj/kern/trap/trapentry.o |
| 反汇编与重定位读取 | objdump、readelf 均退出码0 | objdump.log、readelf.log 及对应 .exit |
| 目标文件核对 | 8/8，退出码0 | object-check.log、object-check.exit、verify_object.py |
| C 结构布局与函数类型检查 | 退出码0 | layout_check.c、layout-compile.log、layout-compile.exit |
| 完整内核构建 | 退出码2，停在 trap.c 编译阶段，未进入链接 | kernel-build.log、kernel-build.exit |

静态检查逐项核对保存/恢复槽位、CSR 写回、sp 最后恢复、传参、对齐与修改边界，并对两组不同 epc 做源指令符号模拟；这不是 CPU 执行测试。结构检查通过 C 编译期断言覆盖32个通用寄存器字段、4个 CSR 字段的实际偏移、结构大小及对齐，并检查 trap 的函数指针类型。

实现阶段（入口仍为 jal trap）的历史反汇编显示 sstatus、sepc 分别读取偏移256、264；通用寄存器按对应偏移恢复，末尾为 `c.ldsp x2,16(x2)` 和 `sret`。入口的传参为 `c.mv x10,x2`，其后 jal 使用 x1，并带 `R_RISCV_JAL trap` 重定位；调用的下一条指令位于 __trapret。对象尚未链接，反汇编中的调用地址不是最终 trap 地址。压缩指令来自现有工具链默认启用的 C 扩展。

`commands.json` 保存各步骤实际命令、工作目录和子进程退出码。`code-before-sha256.json` 与最终审计用于核对其他源码没有变化；`module1.patch` 以任务开始时文件为基线，排除了此前已有的三处注释修改。

## 实际问题与处理

1. 修改前 RESTORE_ALL 和入口调用位置为空，初次静态检查因此有7项失败；补全两个指定位置后，16项检查全部通过。记录中的 static-before 是对任务开始快照的可复现检查。
2. 首次通过 shell 包装保存退出状态时，状态变量传递失败，出现 `exit: : numeric argument required`。改用 Python subprocess.returncode 直接保存子进程状态，再检查任务开始快照；该检查真实退出码为1。此问题属于验证命令包装，没有因此改动内核代码。
3. 完整内核构建在 `trap.c:89`、`trap.c:92` 报告 interrupt_handler、exception_handler 隐式声明，且工程启用了 Werror。模块二实现位置仍为 `//lab3 function2 YOUR CODE`，本次按授权范围保留，没有添加占位处理函数或修改测试、评分脚本，也没有降低警告级别。

本次未运行 QEMU、GDB 或正式评分；Trap 实际触发、C 处理、sret 返回及处理函数修改 epc 后的动态行为仍待运行验证。模块一的编译和静态结果不能代替完整内核运行结果。

## 文件与空间收尾

最终源码哈希审计确认：只有 trapentry.S 发生变化，code 下没有新增源码；`git diff --check` 退出码0。原始输入、前后源码快照、差异、命令、日志、退出状态和对象文件均保存在 `evidence/module1/`；索引见 final-audit.json、artifact-sha256.json。

没有创建临时克隆、临时 checkout 或临时解压目录，没有删除目录，删除回收量为0。构建产物是本次要求保留的验证证据。C盘在台账开始记录和收尾测量时分别有30.148 GiB、30.067 GiB可用空间，实际变化为减少82.797 MiB；此变化不归因于本次证据大小，也不作为删除回收量。

## GitHub 发布阶段补充（2026-11-01）

发布同步当前本地代码的 `call trap`，不回退为历史快照中的 `jal trap`。重新汇编退出码0；反汇编确认 a0 传入帧地址、`R_RISCV_CALL trap` 重定位及 auipc/jalr 调用，返回地址顺序进入 __trapret，恢复指令序列与历史验证一致。旧静态脚本固定匹配 jal，当前运行15/16，唯一不匹配为调用拼写；对当前 call 形式的独立检查通过。其余原始日志和快照按历史证据保留，不改写为当前结果。发布证据见 `evidence/module1/publication-checks.json` 与 `publication-assemble.log`、`publication-objdump.log`。

仅删除团队 lab3 分支根目录的冗余 lab2 目录，保留 lab3 内迁移的 page_init、Best-Fit 及功能模块二；独立 lab2、lab1 分支不作修改。构建对象及缓存不加入提交。本阶段仍未运行 QEMU。
