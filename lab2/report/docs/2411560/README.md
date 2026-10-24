# 2411560 Buddy 与 SLUB 交付导航

负责人：2411560—朱泽帅。代码位于仓库根目录的 `lab2/code/`。报告只包含已完成的 4.4 Buddy 与 4.5 SLUB 章节，其余小组章节待合并。

- [章节报告](../../report.md)
- [四份完整原始 Prompt 汇总](../../prompt.md)
- [保留原始字节的独立 Prompt](../../prompts/2411560/)
- [Prompt 来源和 SHA-256](prompt_provenance.json)
- [本次发布工作区的新验证](PUBLISH_VALIDATION.md)
- [本次验证机器结果](validation/results.json)
- [Buddy 接口规约](BUDDY_INTERFACE_SPEC.md)
- [Buddy 实现记录](BUDDY_IMPLEMENTATION.md)
- [Buddy 专项测试](BUDDY_TEST_REPORT.md)
- [Buddy 最终审计](BUDDY_FINAL_AUDIT.md)
- [SLUB 实现记录](SLUB_IMPLEMENTATION.md)
- [SLUB 专项测试](SLUB_TEST_REPORT.md)

六份研发说明保持原文，描述各自当时的源码、行号、目录及结果；其中原 `evidence/` 路径和 Windows 路径是本地历史归档位置，不代表仓库包含全部旧日志或二进制。Buddy 的 31801/31792 页、SLUB 首轮 31726 页和专项测试 31723 页对应不同镜像，不能将这些历史数值当成固定测试预期。当前验收以本次发布验证记录为准。

选录的实际历史问题证据：

- [SLUB 首次编译的变量名错误](historical/slub_initial_compile.log)
- [保留断言的单行拼写修正](historical/slub_compile_fix.diff)
- [1000 字节对象容量计算错误的首次失败](historical/slub_capacity_first_failure.log)
- [测试驱动修正差异](historical/slub_harness_corrections.diff)

## 从新克隆复现

Ubuntu 22.04 / WSL，使用既有 RISC-V GNU 工具链、QEMU、GDB 和 Host GCC。以下在仓库根目录执行；无需第三方 Python 包。

```sh
cd lab2/code
make -j2
mkdir -p evidence/buddy_tests evidence/slub_tests
python3 tests/run_buddy_tests.py --tag local_buddy --sanitize
python3 tests/run_slub_host_tests.py --tag local_slub
python3 tests/run_slub_kernel_tests.py --tag local_kernel
python3 tests/confirm_slub_test_resident.py
```

`--tag` 每次使用新名称；GDB 脚本首次创建 `evidence/slub_tests/gdb/`，再次执行需先自行保留旧记录并选择独立副本，不覆盖已有证据。源码测试路径按脚本自身所在目录计算，移动到 `code/` 后仍然生效。所有编译产物及完整运行证据由 `code/.gitignore` 排除。

实际 QEMU 启动方式：

```sh
qemu-system-riscv64 -machine virt -nographic -bios default -kernel bin/ucore.img
```

内核检查通过后停在原驻留循环；运行脚本使用 timeout 结束 QEMU，退出 124 本身不能算 PASS，脚本另核对七组 SLUB 和五组 Buddy 标记，GDB 核对无 panic、最终空对象与资源恢复。

## 恢复 Best-Fit 的方式与官方评分边界

当前 `kern/mm/pmm.c` 的 `init_pmm_manager()` 选择 `&buddy_pmm_manager`。恢复 Best-Fit 时，在单独验证副本中改回 `&best_fit_pmm_manager`，保留原 Best-Fit 头文件及全部接口。SLUB 的正常分配接口复用 PMM，但新增专项 `slub_check()` 包含 Buddy 取整容量断言，不能将它作为 Best-Fit 自检；进行官方 Best-Fit 单独验收时，应在该验证副本不调用这套 Buddy 专属 SLUB 检查。不要把验证副本的切换覆盖回本次 Buddy+SLUB 成果。

官方 `tools/test_lab2.c` 的 `test_best_fit()` 明确检查管理器指针为 Best-Fit；当前 Buddy 配置不满足该评分前提。本次没有将其报告为官方 Best-Fit 评分通过。原 `make grade`/`make qemu` 的旧 loader 启动逻辑均未修改；本次不使用已知不兼容的 loader 方式来证明内核工作。官方评分脚本、注入器、测试源码和 Makefile 的 Git 内容保持上游版本，只迁移路径；Shell 文件强制 LF 防止 Windows CRLF 检出影响执行。

## 小组待合并事项

- 将队友的初始化、First-Fit、Best-Fit 说明及原始 Prompt 合并进报告；本次没有发现已有 Lab2 总报告或队友 Prompt，未覆盖其内容。
- Best-Fit 源码第 6 行的 `//lab2 function2-2 YOUR CODE` 由对应队友填写学号。
- 上游 `page_init()` 已有实现，但官方原来的 `lab2 function1 YOUR CODE` 注释已缺失，由对应负责人核对恢复；本次不代签学号。
- 本地未找到 Lab2 实验截图，`report/images/.gitkeep` 只保留目录；未借用 Lab1 截图，也未生成或上传虚假截图。
