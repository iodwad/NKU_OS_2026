# 发布工作区重新验证（2026-10-22）

本次在 `publish_work/lab2/code` 中实际运行，基线 `origin/lab2` 为 `b2298c6f0be7b2b5001b3d29d7b039cb7f72c19b`。不是复制研发工作区的历史 PASS。代码目录移动后，原始 39 个工程文件全部保留，`page_init()`、Best-Fit、First-Fit、公共 PMM 接口、官方评分逻辑和 Makefile 的 Git 内容保持上游版本；仅 pmm 管理器选择与 init 接入是必要逻辑增量。Lab1 及四个本地保护目录逐文件哈希核对通过。

| 验证 | 本次实际结果 | 记录 |
|---|---|---|
| Buddy Host | 43/43，失败 0，含 19 个预期异常 | [汇总](validation/buddy_host_summary.json)、[逐项](validation/buddy_host_results.json) |
| SLUB Host | 73/73，失败 0，含 17 个精确断言用例 | [汇总](validation/slub_host_summary.json)、[逐项](validation/slub_host_results.json) |
| Host Sanitizer | 两套均使用 ASan+UBSan，无诊断 | [Buddy 编译](validation/buddy_host_compile.log)、[SLUB 编译](validation/slub_host_compile.log) |
| 随机压力 | Buddy 两组各 10000 步；SLUB 两组各 10000 步 | 每组包含在各自 Host 用例中，不另增通过项数 |
| RISC-V 对象/完整内核 | 编译链接退出 0 | [对象](validation/slub_object_build.log)、[完整构建](validation/kernel_build.log) |
| QEMU 正常自检 | Buddy 5/5，SLUB 7/7，原基础检查保留 | [实际启动与输出](validation/qemu.log) |
| 物理页守恒 | 本次开始/结束均为 31723 页 | QEMU 各组输出与 GDB 一致 |
| GDB 驻留 | 无 panic；活动对象与 Slab 为 0；owner 全零、partial 链空 | [终端](validation/gdb_terminal.log)、[最终状态](validation/gdb_resident.json) |
| 实际对象数据 | 对象 VA/PA、描述符、Slab 各有区别；24 字节 0x55 读回 | [GDB 样本](validation/gdb_live_object.json) |
| 评分路径迁移 | Shell 语法、官方注入、临时副本完整构建退出 0 | [检查范围与限制](validation/grader_path_check.json) |

全部可复现命令见 [README](README.md)。真实运行命令为：

```sh
cd lab2/code
make -j2
mkdir -p evidence/buddy_tests evidence/slub_tests
python3 tests/run_buddy_tests.py --tag publish_20261022 --sanitize
python3 tests/run_slub_host_tests.py --tag publish_20261022
python3 tests/run_slub_kernel_tests.py --tag publish_kernel_20261022
python3 tests/confirm_slub_test_resident.py
```

运行脚本在真实启动后由 timeout 结束驻留 QEMU，退出 124；七项 SLUB 和五项 Buddy 完成、GDB 无 panic 和最终空对象状态共同作为判据，不把 timeout 本身当通过。ASan/UBSan 只覆盖 Host，未用于 RISC-V 内核。

官方评分脚本最初因 Windows CRLF 检出导致 shell 语法失败，失败记录保留在 [原始失败](validation/grader_windows_crlf_failure.log)。恢复上游 LF 后通过；新增 `code/.gitattributes` 固定 Shell 文件 LF，评分脚本的 Git blob 与原版一致。官方注入器在临时独立目录中完成注入并编译；没有修改发布源码的测试状态。

未宣称官方 `make grade` 得分：原评分要求 Best-Fit，当前按任务选择 Buddy；原 grader 和 make qemu 还保留旧 loader。此次不使用旧 loader 启动，也不改评分条件制造 PASS。Best-Fit 切换方法、原标记待队友处理项和局限见 README。

本轮仅加入源码学号注释和必要接入，Buddy/SLUB 算法及测试逻辑与最终研发源码一致。四份原始 Prompt 的独立归档逐字节核对，汇总文件包含同一完整原文；报告公式与代码块保持原样，发表整理差异仅涉及入口路径、临时提示及从真实记录补入的拼写诊断。

完整源码指纹与机器结果见 [results.json](validation/results.json)。未上传 bin/obj、完整压力大日志、core、旧源码副本或认证信息。本地完整新运行日志在 `publish_work/lab2/code/evidence/`，受 Git ignore 保护。
