# OS Lab1 实际使用的 AI 提示词

以下按使用时间收录 Lab1 对话中的真实用户提示词原文；未收录 AI 回复。环境准备部分摘自当日用户消息中提出的任务提示词。

## 1. 环境准备（2026-10-02）

````markdown
请在我的 WSL Ubuntu 中配置 RISC-V OS Lab1 环境。

实验目录：
/mnt/d/college_study_materials/junior_year_fall/OS/lab1

要求：
- 不修改 lab1 源码
- 不修改 Windows 文件结构
- 只安装实验所需环境

需要配置：
1. riscv64-unknown-elf 工具链
2. qemu-system-riscv64
3. riscv64 gdb
4. OpenSBI 相关环境

完成后：
- 进入 lab1 执行 make
- 检查 kernel 和 ucore.img 是否生成
- 输出所有工具版本和安装路径

不要运行 make grade。
不要编造实验结果。
````

## 2. QEMU/GDB 实验与证据采集（2026-10-03）

````text
OS Lab1 环境已经配置完成，现在开始执行真实实验。

工程目录：
/mnt/d/college_study_materials/junior_year_fall/OS/lab1

当前已确认：
- riscv64-unknown-elf-gcc 10.2.0
- riscv64-unknown-elf-gdb 12.1
- qemu-system-riscv64 6.2.0
- make 已成功
- bin/kernel 已生成
- bin/ucore.img 已生成
- qemu -bios default 实际启动时显示 OpenSBI v0.9
- 系统另外安装有 OpenSBI package 1.3

现在不要修改源码，先完成 Lab1 的真实验证和证据采集。

要求：

一、先记录环境
保存到：
evidence/environment.txt

至少包括：
uname -a
which riscv64-unknown-elf-gcc
riscv64-unknown-elf-gcc --version
which riscv64-unknown-elf-gdb
riscv64-unknown-elf-gdb --version
which qemu-system-riscv64
qemu-system-riscv64 --version

并查清：
1. qemu -bios default 实际加载哪个 OpenSBI firmware
2. 为什么系统 package 显示 1.3，但 QEMU 实际 banner 是 v0.9
3. 报告中只记录实际用于本次实验的固件路径和版本

二、重新执行一次 make，保存完整日志
不要 make grade。

保存：
evidence/build.log

记录退出码。

三、分析 ELF 和镜像

执行：
riscv64-unknown-elf-readelf -h -S -l -W bin/kernel
riscv64-unknown-elf-nm -n bin/kernel
riscv64-unknown-elf-objdump -d bin/kernel

保存到：
evidence/elf_layout.txt
evidence/kernel_objdump.txt

明确提取真实值：
- ELF Entry point
- kern_entry
- kern_init
- bootstack
- bootstacktop
- edata
- end
- bootstacktop - bootstack
- bin/kernel 文件大小
- bin/ucore.img 文件大小
- sha256

四、先测试课程原始 QEMU 启动命令

不要提前修改 Makefile。

确认 Makefile 原始 QEMU 参数。
先按原命令启动并保存真实串口输出：
evidence/qemu_original.log

判断：
- OpenSBI 是否启动
- 是否进入 kern_entry
- 是否出现 "(THU.CST) os is loading ..."
- 是否卡在 OpenSBI 而没有进入 kernel

如果原命令不能进入内核：
不要立刻修改源码或 Makefile。
先说明原因，并通过 GDB / QEMU 参数证据确认是 loader / firmware / entry 兼容问题。

五、GDB 启动验证

使用未占用的本地端口启动：

qemu-system-riscv64 ... -S -gdb tcp::<port>

不要执行 GDB load。

GDB：
file bin/kernel
set architecture riscv:rv64
set pagination off
target remote localhost:<port>

首先记录：

info registers pc sp a0 a1 a2
x/6i 0x1000
x/16bx 0x80200000

保存完整 GDB session：
evidence/gdb_boot.log

六、逐条单步 reset ROM

不要直接 si 6 后补写结果。

逐条记录：
- 执行前 PC
- 指令
- 执行后 PC
- a0
- a1
- a2
- t0（如果相关）
- 当前 privilege mode（如果 GDB 支持 priv）

确认实际 QEMU 6.2 ROM 是几条有效指令。
不要照教材或草稿预填。

七、进入 OpenSBI

设置硬件断点：
hbreak *0x80000000

记录：
- PC
- sp
- a0/a1/a2
- satp
- priv（若可读）
- OpenSBI banner
- firmware 实际版本

八、进入内核

通过真实符号地址设置：
hbreak kern_entry

如果原始 QEMU 启动命令不能到 kern_entry，
先保留失败证据，再寻找最小兼容启动方式。
不要跳过 reset/OpenSBI 流程。

到 kern_entry 后记录：
- pc
- sp
- ra
- a0/a1
- satp
- priv

九、验证 entry.S

反汇编：
disassemble /r kern_entry

逐条执行 la sp, bootstacktop 实际展开指令。

验证：
- sp == bootstacktop
- bootstacktop - bootstack == 8192
- sp % 16 == 0

再记录 tail kern_init 前的 ra。

逐条执行到 kern_init 第一条机器指令。

验证：
- PC == kern_init
- tail 自身没有建立返回地址
- 不要拿进入 kern_init 后其它函数调用改变 ra 的结果来判断 tail

十、验证 kern_init

确认：
- BSS memset 区间
- cprintf 输出
- SBI ecall
- 输出字符串：
  "(THU.CST) os is loading ..."

保存：
evidence/qemu_serial.log

定位 while(1) 对应机器指令并多次单步，
证明打印后确实进入循环。

十一、截图

需要真实截图，不生成假截图。

至少保留：
1. QEMU OpenSBI 启动
2. reset PC = 0x1000
3. ROM 单步
4. OpenSBI 入口
5. kern_entry
6. la 执行前后 sp
7. tail 到 kern_init
8. 最终内核输出

放：
images/

截图必须能看清命令、PC、寄存器或输出，
不要只截局部文字。

十二、限制

- 不运行 make grade
- 不删除已有成果
- 不修改源码，除非真实运行确认存在兼容性问题
- 如果必须改启动命令，先保存原始失败证据
- 不编造 GDB 数值
- 不把预期行为写成已验证
- 不上传课程平台
- 不提交 GitHub

完成后给我：
1. 所有真实地址
2. 原始 QEMU 命令是否成功
3. 实际 OpenSBI 路径和版本
4. reset → OpenSBI → kern_entry 的完整 PC 链
5. la/tail 的实际反汇编
6. kernel 实际输出
7. 所有 evidence 文件路径
8. 所有截图路径
9. 是否修改任何文件及原因
````

## 3. 实验报告结构整理（2026-10-08）

````text
请重新调整 OS Lab1 实验报告第四章的组织结构。

工作目录：\
`D:\college_study_materials\junior_year_fall\OS`

修改：

- `Lab1_实验报告.md`
- 重新生成 `Lab1_实验报告.pdf`

参考：

- `实验报告模板.md`
- `实验报告模板.pdf`
- Lab1 官方实验要求
- `lab1/evidence/` 下的真实实验日志
- `lab1/images/` 下的真实截图

**本轮只重新组织第四章，不修改实验源码，不重新运行实验，不改变已有实测数据。**

### 一、当前结构的问题

当前第四章将功能模块、练习题、调试过程和最终提示词混在一起，没有遵循老师模板的顺序。

老师模板中，“功能模块”与“练习”是两类不同的内容。

必须先介绍功能模块，再分别解答练习题。

### 二、严格调整为以下结构

## 四、实验内容与实现

### 4.1 功能模块一：内核内存布局与镜像构建

**负责人：朱泽帅（全员参与）**

#### 模块功能描述

**涉及的核心文件和函数：**

- `Makefile`
- `tools/kernel.ld`
- `kern/init/entry.S`
- `kern_entry`

本实验没有修改上述课程源码，应明确写出“使用课程提供的原始代码，没有修改内核实现”。

#### 功能说明

介绍交叉编译、链接脚本、内存布局、入口地址、ELF 与 BIN 镜像生成，以及 QEMU 如何加载内核。

保留已经实测的关键符号地址，不需要堆砌完整构建日志。

#### 最终提示词

查找实际保存过的 AI 提示词，摘录该模块相关内容。

如果没有独立的模块提示词，就明确说明使用了统一的 Lab1 调试提示词，不要编造提示词迭代记录。

#### 实现迭代过程

使用真实发生的问题：

第一次运行课程原始 QEMU 参数时，OpenSBI 的下一阶段地址为 `0x0`，未能进入内核。

分析后，在实际启动命令中使用 `-kernel bin/ucore.img`，使下一阶段地址变为 `0x80200000`，成功进入内核。

必须说明这是启动参数调整，不是修改内核源码。

### 4.2 功能模块二：最小内核初始化与 SBI 输出

**负责人：马梓涵（全员参与）**

#### 模块功能描述

**涉及的核心函数：**

- `kern_init()`
- `memset()`
- `cprintf()`
- `vcprintf()`
- `vprintfmt()`
- `cons_putc()`
- `sbi_console_putchar()`
- `sbi_call()`

#### 功能说明

说明 `kern_init()` 如何完成 BSS 初始化、打印启动信息及进入无限循环。

说明 SBI 字符输出的函数调用过程，以及 S-mode 内核如何通过 `ecall` 请求 M-mode OpenSBI 服务。

这里以原始代码分析及实际调试结果为主。

#### 最终提示词

仅填写真实使用过的、与该模块有关的 AI 提示词内容。

没有独立提示词时如实说明，不得虚构。

#### 实现迭代过程

该模块没有新增代码，也没有修改现有功能。

可以写已经实际发生的 SBI 调试过程，例如通过 `ecall` 和固件异常入口断点验证 SBI 输出。

不要编造“第一次生成代码失败、第二次修改通过”之类的记录。

### 4.3 练习一：理解内核启动中的程序入口操作

**负责人：朱泽帅（全员参与）**

按照老师原题明确分为：

1. `la sp, bootstacktop` 完成什么操作？
2. `la sp, bootstacktop` 的目的是什么？
3. `tail kern_init` 完成什么操作？
4. `tail kern_init` 的目的是什么？

保留当前报告中已有的真实反汇编、栈地址、PC、sp 和 ra 数据。

每个问题单独回答，不与前面的模块介绍混淆。

### 4.4 练习二：使用 GDB 验证启动流程

**负责人：张宸笛号（全员参与）**

严格按照以下结构：

#### 4.4.1 调试过程

描述 QEMU、GDB 的实际启动方法，如何连接、查看指令、单步和设置断点。

#### 4.4.2 观察结果

展示从 `0x1000` 进入 `0x80000000`，最终到达 `0x80200000` 的真实观察。

保留对应截图与关键寄存器变化。

#### 4.4.3 问题一：RISC-V 硬件加电后最初执行的几条指令位于什么地址？

直接回答问题，列出实测六条 Reset ROM 指令的地址。

#### 4.4.4 问题二：这些指令主要完成哪些功能？

直接解释每条指令的主要功能，最后总结启动参数准备以及向 OpenSBI 的跳转。

### 三、Challenge 的处理

本次提供的 Lab1 课程要求中没有单独布置 Challenge，因此不要编造一个 Challenge。

模板里的 Challenge 是可选结构，没有实际题目时不需要增加空章节。

### 四、关于模板的核心要求

对于每一个功能模块，都要检查是否具有：

- 负责人
- 模块功能描述
- 涉及的核心函数或文件
- 功能说明
- 最终提示词或真实的使用情况说明
- 实现迭代过程或实际调试过程

对于每一道练习，都要检查是否具有：

- 练习原题
- 负责人
- 明确的问题答案
- 必要的分析和真实实验数据
- 对应截图

不得把“最终提示词”和“实现迭代过程”孤立地放在两个练习后面。

### 五、其他限制

1. 不修改第一、二、三、五、六章的内容，除非第四章调整导致图片引用需要同步修正。
2. 所有图片继续使用真实截图。
3. 保留可点击的“见图 X”内部交叉引用，调整图片顺序后同步维护编号和目标。
4. 小组成员姓名严格为：朱泽帅、张宸笛号、马梓涵。
5. 不编造学号。
6. 不编造未发生的代码修改、提示词迭代或测试结果。
7. 不修改 Makefile、entry.S、init.c 等任何实验源码。
8. 不运行 make grade。
9. 不提交 Git 或课程平台。

### 六、重新编译与检查

修改 Markdown 后重新生成 PDF。

逐页检查第四章，确保：

- 功能模块一和功能模块二都按照模板组织。
- 两道练习位于两个功能模块之后。
- 模块与练习没有混淆。
- 表格、代码块和截图完整。
- 正文图片引用可点击并跳转到正确位置。
- 没有大面积空白或乱码。
- 没有删除已有的关键实测结论。

最终只汇报第四章重组情况、修改文件路径、PDF 页数、截图数量和模板符合性检查结果。
````
