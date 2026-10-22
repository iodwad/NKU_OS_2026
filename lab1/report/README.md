# Lab1：最小内核的启动与调试

本实验通过阅读源码和 QEMU/GDB 调试，理解 RISC-V 最小内核从启动入口运行到 C 初始化函数的过程。

主要内容包括：

- 内核镜像的编译、链接及内存布局。
- Reset ROM、OpenSBI 与内核入口 `kern_entry` 之间的启动交接。
- `entry.S` 中设置启动栈，以及跳转到 `kern_init` 的过程。
- `kern_init` 的 BSS 初始化、SBI 字符输出和最终驻留循环。
- 使用 QEMU 与 GDB 跟踪启动流程、检查寄存器和记录调试现象。

## 文件导航

- [code/](../code/)：Lab1 源代码与构建文件。
- [report.md](report.md)：实验报告，包含原理分析、调试过程、观察结果和问题记录。
- [prompt.md](prompt.md)：实验使用的 AI Prompt 记录。
- [images/](images/)：报告引用的真实调试截图。

具体实验环境、启动参数及验证结果以现有报告和截图为准。
