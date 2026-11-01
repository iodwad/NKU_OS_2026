# 兼容性处理与构建状态

- Lab3 缺少 Lab2 的 PHYSICAL_MEMORY_END、KERNEL_BEGIN_PADDR。仅在 page_init 中用现有映射常量计算等价边界：KERNTOP - PHYSICAL_MEMORY_OFFSET = 0x88000000；KERNBASE - PHYSICAL_MEMORY_OFFSET = 0x80200000。
- 迁移前先检查接口，未制造或运行包含缺失常量的中间版本；上述调整在首次构建前完成。
- 保留 Lab3 原子位操作、中断保护和两个课程检查函数；绑定 Best-Fit 以延续已完成 Lab2 的分配策略。
- make -j2 失败于 trap.c 的 interrupt_handler、exception_handler 隐式声明，属于模块二缺项；未修补该文件。
- 迁移的两个内存管理对象通过 Makefile 原有规则编译成功。整体未链接，不能据此判断入口占位代码运行正确。
- RESTORE_ALL 和 __alltraps 的空白练习本身未必造成编译错误；即使汇编成功也不表示能够保存、分派、恢复并返回。
- 未运行 QEMU、GDB、make grade，PMM 运行检查未验证。

补充：首次构建和对象构建出现 D 盘时间戳略超前（0.05～0.085秒）及 clock skew 警告。随后使用原有规则 `make -B -j2 obj/kern/mm/pmm.o obj/kern/mm/best_fit_pmm.o` 强制重编译两份迁移源文件，退出码0；日志见 make-migrated-forced.log。未修改 Makefile 或调整源文件时间戳。
