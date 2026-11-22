# Lab5：用户程序与进程生命周期

## 代码来源与状态

- `code/`：从 [nkgongxl/ucoreonrv](https://github.com/nkgongxl/ucoreonrv) 的 `code_practice/lab5`（`8fa1b7b`）导入的 **RISC-V uCore 练习框架**，包含 `kern/`、`libs/`、`tools/`、`user/` 和 Makefile。
- 此框架尚未核实与 2026 年课程发放的压缩包逐字节相同；它不是已完成的 Lab5 实现。
- Git 分支：`lab5`，从本仓库远程 `lab4` 分支开始。原 Lab4 及其未提交的本地修改不受影响。

## 后续工作

1. 将本机已完成的 Lab2 物理内存管理、Lab3 Trap、Lab4 进程创建与调度代码逐项迁移到 Lab5 框架，保留 Lab5 新增字段和接口；需要特别处理支持用户态返回的 `RESTORE_ALL`。
2. 完成系统调用、用户程序加载、用户进程复制及退出等四个基础功能模块。
3. 编译并实际运行用户态测试，收集 `make grade`、QEMU 和 GDB 的真实输出。
4. 扩展 Challenge：COW、页故障处理、引用计数、并发/竞态测试和 Dirty COW 原理分析。

**当前仅完成独立工作区、练习框架导入及 GitHub 分支搭建；上面的功能实现与测试尚未完成。**
