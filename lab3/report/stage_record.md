# NKU OS 2026 Lab3：工程重建与 Lab2 迁移记录（初稿）

日期：2026-10-31。

## 实验环境

工程位于 `D:\college_study_materials\junior_year_fall\OS\lab3`；在 WSL Ubuntu-22.04 中通过 `/mnt/d/college_study_materials/junior_year_fall/OS/lab3/code` 构建。工具路径、系统信息及版本见 `../evidence/environment.log`。

## 代码来源

课程下载地址为 http://oslab.mobisys.top/lab2026/lab3.zip 。HTTP 连接被关闭，HTTPS 返回 TLS EOF；本地 lab2 下未发现 lab3.zip。因此采用团队仓库导入骨架提交 `d0bb450d845bfd33b9e4584855d9194d93a9ab5c` 的 `lab3/code`。未验证该导入版本与官网 ZIP 完全一致。来源归档保存在 `../reference/github_d0bb450_lab3_code.tar`，SHA256 和下载错误见 `../evidence/source.json`。

Lab2 来源为 `origin/lab2` 对应提交 `ea3300cd028a0af8ddea475b093a2a8f9c7f10a8`；已用 git ls-remote 核对远程分支。五个源文件保存在 reference 中，本地 D 盘 Lab2 与提交版本的逐文件对照见 evidence/local_lab2_vs_committed_*.diff。迁移使用提交版本，没有使用历史草稿。

## 迁移目标与实际使用的 Prompt

目标是复用已完成的 page_init 和 Best-Fit，在新建 D 盘工程中保留 Lab3 练习原状。实际任务 Prompt 原文保存在 [actual_prompt.txt](actual_prompt.txt)，未以摘要代替。

## 实际迁移情况

仅修改 code/kern/mm/pmm.c 与 code/kern/mm/best_fit_pmm.c。迁入 page_init、空闲区数据结构和宏、五个 Best-Fit 函数及 manager 定义，保留原作者学号注释。边界常量按 Lab3 现有映射等价计算；manager 绑定 Best-Fit，以延续 Lab2 分配策略。原有中断保护与课程检查函数保持不变。差异见 ../evidence/migration.patch，逐项检查见 migration-checks.json。

## 编译与验证状态

实际执行 `make -j2`，退出码为 2。错误为 trap.c 中 interrupt_handler、exception_handler 的隐式声明，位于第89和92行。随后执行 Makefile 原有对象目标 `make -j2 obj/kern/mm/pmm.o obj/kern/mm/best_fit_pmm.o`，退出码为 0。两份迁移源文件编译成功，完整内核没有构建成功。原始输出与命令分别保存在 evidence 的 make-j2.log、make-migrated-objects.log、commands.json。

Trap 三个源文件与导入骨架逐字节一致。trapentry.S 第59、68行保留模块一 YOUR CODE；trap.c 第83行保留模块二 YOUR CODE。本阶段没有实现模块一或模块二，没有运行 QEMU、GDB 或正式评分；不记录模块一实现和运行结论。

补充：首次构建和对象构建出现 D 盘时间戳略超前（0.05～0.085秒）及 clock skew 警告。随后使用原有规则 `make -B -j2 obj/kern/mm/pmm.o obj/kern/mm/best_fit_pmm.o` 强制重编译两份迁移源文件，退出码0；日志见 make-migrated-forced.log。未修改 Makefile 或调整源文件时间戳。
