# Lab3 评分启动兼容性修复与验收

验证日期为 2026-11-05。验证工程为 D 盘当前 Lab3，输入包含已同步的团队功能代码和本地解释性注释。源码输入以 [SHA256 清单](grade-compatibility-20261105-c35ad817/source-before-sha256.json) 为准，本地独立 Git 仓库的 HEAD 不代表全部工作树内容。

## 1. 结果分类

| 验证类别 | Context / Breakpoint | Illegal / Fallback | Timer | 总结果 |
|---|---:|---:|---:|---|
| 本轮课程原版 `make grade` | 0/35 | 0/25 | 0/40 | 0/100，make 退出码 2 |
| 启动兼容性修复后的标准 `make grade` | 35/35 | 25/25 | 40/40 | 100/100，make 退出码 0 |
| 三项独立 QEMU 运行 | 对应 PASS 标记 | 对应 PASS 标记 | PASS，ticks=200 | 每项 QEMU 退出码 0 |

100/100 由原评分脚本实际运行并累加得到。独立运行只用于核查功能和退出状态。原版失败的 stdout、stderr、退出码和三份串口输出均已保留。

## 2. 原始流程与共同根因

`Makefile` 的 `grade` 目标清理本地构建结果，将代码复制到临时目录，由原 `inject.py` 注入测试并执行 `tools/grade.sh`。评分脚本分别以三个 `GRADE_LAB3_*` 宏编译，使用 8、8、30 秒超时，按测试 PASS 标记及固定输出子串判分。三组权重为 35、25、40。

镜像先链接为 ELF，再通过 `objcopy --strip-all -O binary` 生成裸镜像。原评分参数 `-device loader,file=bin/ucore.img,addr=0x80200000` 将镜像写入内存。本机 QEMU 6.2.0 与默认 OpenSBI 0.9 组合下，三份原始串口均显示 `Domain0 Next Address = 0x0`，没有进入内核的 `DTB Init` 和测试输出。

本轮固定同一个 Context 镜像做 A/B，对照镜像 SHA256 为 `50e4f921f5e57047f207b481c79393fdfb294009a8c0f52c914f30a84e2b31dc`。其余机器参数和 3 秒超时相同。loader 方式仍交接到 `0x0`，替换为 `-kernel` 后交接到 `0x80200000`，并输出 Context PASS。两个诊断进程均被有意限定到 3 秒，退出码 124 不作为评分。 [A/B 命令与结果](grade-compatibility-20261105-c35ad817/ab-comparison.json)

QEMU 6.2 的 `virt_machine_init()` 在没有 `machine->kernel_filename` 时，将 `kernel_entry` 设为 0。`boot.c` 再将其写入 OpenSBI 的 `fw_dynamic_info.next_addr`。Generic loader 放置镜像没有提供这个内核交接信息。源码与本轮同镜像 A/B 相互对应，确认共同故障发生在固件交接阶段。增加超时或放宽输出匹配不能修正入口地址。 [QEMU 6.2 virt.c](https://github.com/qemu/qemu/blob/v6.2.0/hw/riscv/virt.c#L819-L841) · [QEMU 6.2 boot.c](https://github.com/qemu/qemu/blob/v6.2.0/hw/riscv/boot.c#L219-L235)

## 3. 最小修复

本次只把 `code/tools/grade.sh` 的一行启动参数替换为 `-kernel bin/ucore.img`。回换该参数后，脚本与原版备份逐字节一致。Makefile、测试源、注入器、断言、权重、超时、匹配和分数计算保持原样，Trap 功能与本地中文注释的文件哈希也保持原值。

[原脚本](grade-compatibility-20261105-c35ad817/original-grade.sh) · [原 Makefile](grade-compatibility-20261105-c35ad817/original-Makefile) · [修复 diff](grade-compatibility-20261105-c35ad817/launch-fix.diff) · [脚本哈希](grade-compatibility-20261105-c35ad817/launch-fix-sha256.json)

## 4. 完整标准验收

在本地 `code/` 按以下顺序实际执行。

```sh
make clean
make -B -j2
make grade
```

| 命令 | 实际退出码 | 原始证据 |
|---|---:|---|
| `make clean` | 0 | [stdout](grade-compatibility-20261105-c35ad817/acceptance-clean.stdout.log)、[stderr](grade-compatibility-20261105-c35ad817/acceptance-clean.stderr.log) |
| `make -B -j2` | 0 | [stdout](grade-compatibility-20261105-c35ad817/acceptance-build.stdout.log)、[stderr](grade-compatibility-20261105-c35ad817/acceptance-build.stderr.log) |
| 修复后 `make grade` | 0 | [stdout](grade-compatibility-20261105-c35ad817/compat-grade.stdout.log)、[stderr](grade-compatibility-20261105-c35ad817/compat-grade.stderr.log) |
| 修复前 `make grade` | 2 | [stdout](grade-compatibility-20261105-c35ad817/original-grade.stdout.log)、[stderr](grade-compatibility-20261105-c35ad817/original-grade.stderr.log) |

`MAKEFLAGS` 仅将 `GRADE_TMP` 定位到本任务 D 盘临时目录，未改变评分内容。入口仍为标准 `make grade`。 [命令、工作目录与退出码](grade-compatibility-20261105-c35ad817/commands.json) · [工具与固件版本、哈希](grade-compatibility-20261105-c35ad817/environment.json)

评分 stdout 实际包含以下结果。

```text
trap entry, register restore, and breakpoint EPC: PASS (35/35)
illegal instruction EPC advance and return: PASS (25/25)
re-armed supervisor timer and repeated trap return: PASS (40/40)
Total Score: 100/100
```

WSL/DrvFS 仍偶发不足一秒的时间戳超前警告，已保留 stderr。本轮强制构建成功，评分中每个用例也先清理再独立构建。没有调整源文件时间戳。

## 5. 独立 QEMU 与覆盖范围

三项独立运行使用标准评分实际生成的三个镜像，各设 15 秒超时。每项均出现自己的 PASS 标记、输出十次 `100 ticks`，随后按现有计时关机逻辑退出，QEMU 退出码均为 0。没有达到超时。

- [Context 与断点完整输出](grade-compatibility-20261105-c35ad817/independent-context.stdout.log)
- [非法指令与回退完整输出](grade-compatibility-20261105-c35ad817/independent-illegal.stdout.log)
- [持续时钟完整输出](grade-compatibility-20261105-c35ad817/independent-timer.stdout.log)

原评分脚本忽略单次 QEMU/timeout 的退出码，使用输出标记判分，这一逻辑保持原样。前两组标准运行可能在成功标记之后达到 8 秒时限。独立运行验证了它们能继续计时并正常关机。

Context 比较 26 个整数寄存器。测试镜像中的断点为 `0x9002`，即 2 字节 C.EBREAK，非法指令为 `0x0000007b`，长度 4 字节。原用例均能返回并输出 PASS。编码、触发地址及预期下一地址见 [指令长度核对](grade-compatibility-20261105-c35ad817/instruction-length-check.json)。预期下一地址来自镜像编码计算，没有冒充对返回 CSR 的逐条观测。

时钟测试读取真实 `ticks=200` 后继续计时，最终正常关机。回退测试仍使用人工 Trap frame。所有寄存器状态、不可读 EPC、长指令、用户栈切换、多核和复杂设备应答超出现有用例覆盖。

## 6. 提交检查与剩余范围

本地评分兼容性修复及完整验收已通过，100/100 应带修复配置说明。Makefile 的其他 `qemu/debug/test` 启动目标仍沿用原配置，本次仅修复 `make grade`。

本轮先在独立本地工作区完成验收，原工作区的暂存及未暂存状态未被覆盖。兼容性修复及证据另行同步至团队 `lab3` 分支；本报告的 100/100 结论仅对应文内说明的 QEMU/OpenSBI 环境与评分配置。
