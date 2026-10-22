# SLUB 专项测试报告

STATUS：SUCCESS。最终 Host 独立进程测试 **73/73**，其中正常/边界/压力 56 项、预期断言 17 项；失败 0、未执行 0。QEMU 中 SLUB 正常检查 **7/7**，原 Buddy 内核检查 **5/5**。两组 Host 随机序列共 **20000 步**。这些数量分别统计，不相加为“85 项独立算法测试”。

## 范围和版本证据

工作区：`D:\college_study_materials\junior_year_fall\OS\lab2\slub_work`。本次只扩展 SLUB 测试；没有发现经证实的生产 SLUB 算法 Bug，没有修改生产 `slub.c` / `slub.h`。

`evidence/slub_tests/original_prompt.txt` 保存本次原 Prompt；`before/`、`baseline_sha256.json` 保存测试前源码；`after/` 保存本次最终测试源码；`final_verification.json` 保存产物 SHA-256 和保护目录检查。上一份实现 Prompt 保留在 `evidence/slub_implementation/`。

已存在的源码仅修改 `kern/mm/slub_checks.inc`：将原 `slub_check` 原封不动改名为静态 `slub_check_basic`，末尾 include 新测试文件。`test_entry.diff` 可核对全部差异。原断言没有删除或放宽。`kern/init/init.c`、Makefile、Buddy、First-Fit、Best-Fit、page_init 和已有评分测试与前快照相同。Lab1、共享 code、buddy_work 及下载仓库的逐文件哈希均未改变。

新增：`kern/mm/slub_test_cases.inc`、`tests/slub_host_tests.c`、Host/内核/GDB 运行脚本、准备/复核/报告脚本、测试计划和本报告。源码及日志均留在本工作区；没有 Push。

## 环境、命令及结果

以下从工作区执行。完整命令、输出和退出码分别见对应日志；复跑应将 `--tag` 改为新的目录名，脚本拒绝覆盖现有证据。

```powershell
wsl -d Ubuntu-22.04 -- python3 /mnt/d/college_study_materials/junior_year_fall/OS/lab2/slub_work/tests/run_slub_host_tests.py --tag host_accepted
wsl -d Ubuntu-22.04 -- python3 /mnt/d/college_study_materials/junior_year_fall/OS/lab2/slub_work/tests/run_slub_kernel_tests.py --tag kernel
wsl -d Ubuntu-22.04 -- python3 /mnt/d/college_study_materials/junior_year_fall/OS/lab2/slub_work/tests/confirm_slub_test_resident.py
wsl -d Ubuntu-22.04 -- python3 /mnt/d/college_study_materials/junior_year_fall/OS/lab2/slub_work/tests/finalize_slub_tests.py
```

Host 编译使用 GCC：`-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -fno-pie -no-pie -std=gnu99 -fno-builtin -Wall -Werror -Wno-unused -O1 -g`，include `libs`、`kern/mm`、`kern/debug`、`kern/driver`。详见 `host_accepted/compile.log`；编译及 73 个进程均退出 0，无 ASan/UBSan 诊断。`environment_and_layout.log` 保存 uname、工具版本、Host 可执行文件 sanitizer 动态链接以及真实 RISC-V ELF section/symbol 大小。未安装或替换工具链。

内核执行 `make obj/kern/mm/slub.o`、`make -j2` 均退出 0。实际启动命令：`timeout --signal=TERM 45s qemu-system-riscv64 -machine virt -nographic -bios default -kernel bin/ucore.img`。QEMU 日志退出 124 是全部检查结束后由 timeout 正常停止驻留内核；不能单凭 124 判定成功，另以全部断言后输出、GDB 驻留 PC 和无 panic 状态验证。

## 测试为何有效

Host 编译单元直接 include 真实 `buddy_pmm.c` 与 `slub.c`，没有另一份被测试分配算法。只在 Host 适配层重定义 VA 窗口/偏移常量，并以 MAP_FIXED_NOREPLACE 在 `0x400000000000` 建立 128 MiB 等价线性映射；逻辑物理页从 `0x80200000` 起注册。默认受控资源 128 页，耗尽测试分别使用 0/1/2/4/8 页。真实 RISC-V 地址路径另由 QEMU 验证。

独立对象账本记录返回地址、原请求、编号、模式和活动状态；以外部请求和规定大小类/96 字节头部计算预期容量、有效范围、块占用及实际 Buddy 取整，不从待验证的 used/owner 字段生成全部期望。PMM 包装调用真实 Buddy，独立记录原页请求及实际 2 的幂容量，释放必须匹配原请求。每次操作交叉核对 Page 状态、Buddy 空闲链、owner、Slab 头、位图、自由槽链、partial 链、对象数和实际物理页守恒。Python `stress_replay.json` 还独立重放种子、每次请求/释放、数据签名及页占用。

一般功能测试全量读回活动对象。压力测试每步检查所有活动对象首/中/尾各最多 32 字节，每 128 次操作全量核验，释放前全量读回，清理时再验证。已释放用户数据不作为正确性依据：快照只比较活动对象有效字节、有效 Slab 头、自由槽的 2 字节链指针（分配器元数据）、仍由调用者持有的裸 Buddy 数据，以及管理数组/描述符/链表；不整块读取已归还 Slab 或已释放用户载荷。只读检查不修改被检查的私有状态。

所有正常用例结束核对资源恢复及对象/Slab 清零。非法用例分别在新进程执行；panic 包装器要求匹配具体生产文件、断言表达式，并在退出前比较快照。任意信号崩溃、错误断言、正常返回、sanitizer 错误或超时均不能算通过。通过的负例进程以 0 表示“预期诊断已精确验证”，不表示生产函数正常接受了非法操作；没有声称进程终止后还能观察其状态。

## Host 逐项结果

每行对应一个实际独立进程。原始输出位于 `evidence/slub_tests/host_accepted/<编号>.log`；机器清单为 `case_catalog.json`。

| 编号 | 测试功能 | 输入条件 | 预期行为 | 实际结果 | 状态 |
|---|---|---|---|---|---|
| size_1 | 大小边界/写读 | 1 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_1 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_8 | 大小边界/写读 | 8 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_8 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_15 | 大小边界/写读 | 15 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_15 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_16 | 大小边界/写读 | 16 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_16 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_17 | 大小边界/写读 | 17 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_17 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_24 | 大小边界/写读 | 24 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_24 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_31 | 大小边界/写读 | 31 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_31 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_32 | 大小边界/写读 | 32 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_32 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_33 | 大小边界/写读 | 33 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_33 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_60 | 大小边界/写读 | 60 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_60 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_63 | 大小边界/写读 | 63 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_63 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_64 | 大小边界/写读 | 64 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_64 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_65 | 大小边界/写读 | 65 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_65 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_100 | 大小边界/写读 | 100 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_100 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_127 | 大小边界/写读 | 127 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_127 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_128 | 大小边界/写读 | 128 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_128 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_129 | 大小边界/写读 | 129 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_129 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_255 | 大小边界/写读 | 255 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_255 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_256 | 大小边界/写读 | 256 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_256 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_257 | 大小边界/写读 | 257 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_257 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_511 | 大小边界/写读 | 511 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_511 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_512 | 大小边界/写读 | 512 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_512 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_513 | 大小边界/写读 | 513 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_513 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_1023 | 大小边界/写读 | 1023 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_1023 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_1024 | 大小边界/写读 | 1024 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_1024 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_1025 | 大小边界/写读 | 1025 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_1025 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_2047 | 大小边界/写读 | 2047 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_2047 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_2048 | 大小边界/写读 | 2048 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_2048 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_2049 | 大小边界/写读 | 2049 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_2049 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_3000 | 大小边界/写读 | 3000 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_3000 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_4095 | 大小边界/写读 | 4095 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_4095 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_4096 | 大小边界/写读 | 4096 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_4096 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_4097 | 大小边界/写读 | 4097 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_4097 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_5000 | 大小边界/写读 | 5000 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_5000 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_8192 | 大小边界/写读 | 8192 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_8192 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_8193 | 大小边界/写读 | 8193 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_8193 operations=2 restored=128 objects=0 slabs=0 | PASS |
| size_8200 | 大小边界/写读 | 8200 字节；新隔离实例 | 至少 16 字节对齐、完整请求范围写读正确、无重叠、释放后恢复 | PASS size_8200 operations=2 restored=128 objects=0 slabs=0 | PASS |
| zero | 零请求与空释放 | 空实例及活动对象存在时 alloc(0)、free(NULL) | 返回 NULL / 安全返回；快照不变 | PASS zero operations=3 restored=128 objects=0 slabs=0 | PASS |
| same_isolation | 同类隔离 | 20 个 100 字节对象，释放中间项并重写其余项 | 请求范围不重叠；其余活动数据正确 | PASS same_isolation operations=40 restored=128 objects=0 slabs=0 | PASS |
| mixed_isolation | 跨类隔离 | 24/100/256/500/1000/5000/8200 同时存活 | 释放中间对象不影响其余对象 | PASS mixed_isolation operations=14 restored=128 objects=0 slabs=0 | PASS |
| reuse | 槽位复用 | 31 个 100 字节对象，释放 16 个偶数槽后重填 | 仍只占一页；数据和所有权正确 | PASS reuse operations=94 restored=128 objects=0 slabs=0 | PASS |
| expand | 扩容及页再利用 | 仅 2 页，32 个 100 字节对象；回收首 Slab | 第 32 项使用第二页；Buddy 可重新取得首 Slab 页 | PASS expand operations=66 restored=2 objects=0 slabs=0 | PASS |
| classes | 全部大小类 | 16..2048 八类分别申请容量+1 个 | 每类扩展到两页；全部释放后恢复 | PASS classes operations=1004 restored=128 objects=0 slabs=0 | PASS |
| reclaim | 最后对象回收 | 两个 24 字节对象，依次释放，再申请 | 部分释放不还页；最后释放清 owner、还页且可再用 | PASS reclaim operations=6 restored=128 objects=0 slabs=0 | PASS |
| large_pair | 原始页数配对 | 8200 字节与 24 字节同时存在 | 8200 原请求 3 页、独占 4 页、释放返回全部 4 页 | PASS large_pair operations=4 restored=128 objects=0 slabs=0 | PASS |
| huge | 容量与整数边界 | 保留活动对象，申请 SIZE_MAX、2^63、容量上界附近 | 四次失败均返回 NULL；状态和活动数据不变 | PASS huge operations=6 restored=128 objects=0 slabs=0 | PASS |
| no_pages | 零可用页 | 零页实例申请 1/5000 字节 | 返回 NULL，无部分初始化或泄漏 | PASS no_pages operations=2 restored=0 objects=0 slabs=0 | PASS |
| oom_small | 小对象耗尽 | 2 页；1000 字节对应 1024 类，容量 6 个 | 第 7 个失败；释放后可再申请并恢复 | PASS oom_small operations=15 restored=2 objects=0 slabs=0 | PASS |
| oom_16 | 最小类耗尽 | 2 页；500 个 16 字节对象 | 第 501 个失败；释放后可再申请并恢复 | PASS oom_16 operations=1003 restored=2 objects=0 slabs=0 | PASS |
| free_slots_oom | 零剩余页仍可复用 | 1 页，1000 字节对象，含部分释放 | 使用已有第三槽；满时失败；部分释放后复用 | PASS free_slots_oom operations=9 restored=1 objects=0 slabs=0 | PASS |
| oom_large | 大对象耗尽 | 4 页；8200 占满后申请 5000 | 失败不损坏前对象；释放后 5000 成功 | PASS oom_large operations=5 restored=4 objects=0 slabs=0 | PASS |
| fragmented | 连续页不足 | 8 个单页，释放偶数物理页号，形成 4 个孤立空闲页 | 5000 失败、2048 成功；整理释放后 8200 成功 | PASS fragmented operations=5 restored=8 objects=0 slabs=0 | PASS |
| raw_coexist | 两层共存 | Buddy 原请求 3 页写入模式，同时申请 SLUB 对象 | 原 Buddy 数据不变；按原 3 页释放；完整恢复 | PASS raw_coexist operations=4 restored=128 objects=0 slabs=0 | PASS |
| basic | 原检查及新增内核套件 | 在 Host 等价映射中调用实际 slub_check() | 原检查及七组正常检查实际运行并恢复 | PASS basic operations=0 restored=128 objects=0 slabs=0 | PASS |
| stress_a | 压力 A | 种子 0x20261021，10000 步，64 活动槽 | 每步核验，周期全量读回，清理后恢复 | PASS stress_a operations=10036 restored=128 objects=0 slabs=0 | PASS |
| stress_b | 压力 B | 种子 0x13579BDF，10000 步，64 活动槽 | 每步核验，周期全量读回，清理后恢复 | PASS stress_b operations=10030 restored=128 objects=0 slabs=0 | PASS |
| N01 | 隔离非法操作 | 同一 Slab 中 100 字节对象释放两次，另一对象仍存活 | 仅接受 slub.c:220 的 `slab->inuse > 0 && slub_slot_used(slab, slot)`；终止前管理状态和活动数据不变 | PASS N01 exact expected assertion; pre-termination state unchanged | PASS |
| N02 | 隔离非法操作 | 2048 字节对象触发 Slab 回收后再次释放旧指针 | 仅接受 slub.c:205 的 `slab != NULL`；终止前管理状态和活动数据不变 | PASS N02 exact expected assertion; pre-termination state unchanged | PASS |
| N03 | 隔离非法操作 | 100 字节对象首地址 +1 | 仅接受 slub.c:218 的 `displacement % slab->object_bytes == 0`；终止前管理状态和活动数据不变 | PASS N03 exact expected assertion; pre-termination state unchanged | PASS |
| N04 | 隔离非法操作 | 8200 字节对象首地址 +1 | 仅接受 slub.c:209 的 `va == payload && slab->inuse == 1 && slab->used[0] == 1`；终止前管理状态和活动数据不变 | PASS N04 exact expected assertion; pre-termination state unchanged | PASS |
| N05 | 隔离非法操作 | 有效 Slab 的页首元数据地址 | 仅接受 slub.c:215 的 `va >= payload`；终止前管理状态和活动数据不变 | PASS N05 exact expected assertion; pre-termination state unchanged | PASS |
| N06 | 隔离非法操作 | 有效栈变量地址 | 仅接受 slub.c:200 的 `va >= KERNBASE && va < KERNTOP && va >= va_pa_offset`；终止前管理状态和活动数据不变 | PASS N06 exact expected assertion; pre-termination state unchanged | PASS |
| N07 | 隔离非法操作 | 仍由调用者持有的裸 Buddy 页数据地址 | 仅接受 slub.c:205 的 `slab != NULL`；终止前管理状态和活动数据不变 | PASS N07 exact expected assertion; pre-termination state unchanged | PASS |
| N08 | 隔离非法操作 | 100 字节对象首地址 +100 | 仅接受 slub.c:218 的 `displacement % slab->object_bytes == 0`；终止前管理状态和活动数据不变 | PASS N08 exact expected assertion; pre-termination state unchanged | PASS |
| N09 | 隔离非法操作 | 100 字节对象下一尚未分配槽位地址 +128 | 仅接受 slub.c:220 的 `slab->inuse > 0 && slub_slot_used(slab, slot)`；终止前管理状态和活动数据不变 | PASS N09 exact expected assertion; pre-termination state unchanged | PASS |
| N10 | 隔离非法操作 | UINTPTR_MAX | 仅接受 slub.c:200 的 `va >= KERNBASE && va < KERNTOP && va >= va_pa_offset`；终止前管理状态和活动数据不变 | PASS N10 exact expected assertion; pre-termination state unchanged | PASS |
| N11 | 隔离非法操作 | 映射范围内、逻辑 npage 之外的页地址 | 仅接受 slub.c:203 的 `ppn >= nbase && ppn < npage && ppn - nbase < SLUB_PAGE_LIMIT`；终止前管理状态和活动数据不变 | PASS N11 exact expected assertion; pre-termination state unchanged | PASS |
| N12 | 隔离非法操作 | 8200 字节大对象释放并回收后再次释放旧指针 | 仅接受 slub.c:205 的 `slab != NULL`；终止前管理状态和活动数据不变 | PASS N12 exact expected assertion; pre-termination state unchanged | PASS |
| N13 | 隔离非法操作 | Page 描述符数组地址 pages，另有活动对象 | 仅接受 slub.c:200 的 `va >= KERNBASE && va < KERNTOP && va >= va_pa_offset`；终止前管理状态和活动数据不变 | PASS N13 exact expected assertion; pre-termination state unchanged | PASS |
| N14 | 隔离非法操作 | 有效 Slab 页末尾第 4095 字节 | 仅接受 slub.c:217 的 `displacement < (size_t)slab->slots * slab->object_bytes`；终止前管理状态和活动数据不变 | PASS N14 exact expected assertion; pre-termination state unchanged | PASS |
| N15 | 隔离非法操作 | 8200 字节对象首地址 +4096 | 仅接受 slub.c:209 的 `va == payload && slab->inuse == 1 && slab->used[0] == 1`；终止前管理状态和活动数据不变 | PASS N15 exact expected assertion; pre-termination state unchanged | PASS |
| N16 | 隔离非法操作 | 未初始化 SLUB 时申请 24 字节 | 仅接受 slub.c:151 的 `slub_ready`；终止前管理状态和活动数据不变 | PASS N16 exact expected assertion; pre-termination state unchanged | PASS |
| N17 | 隔离非法操作 | 存在活动对象时重复 slub_init() | 仅接受 slub.c:140 的 `!slub_ready`；终止前管理状态和活动数据不变 | PASS N17 exact expected assertion; pre-termination state unchanged | PASS |

## QEMU 实际正常检查

函数位于 `kern/mm/slub_test_cases.inc`，`slub_check()` 在 PMM 和 SLUB 初始化后运行，不重置已使用的全局分配器。每条 PASS 都在对应真实操作/断言之后输出，使用当前启动时测得的 F0。

| 编号 | 测试功能 | 输入条件 | 预期行为 | 实际结果 | 状态 |
|---|---|---|---|---|---|
| T01 | 保留原基础检查 | 原 slub_check_basic | 写读、隔离、复用及 5000/8200 回收 | 原全部断言完成 | PASS |
| T02 | 37 种大小并存 | 全部指定小/大对象 | 请求范围写读、合法对齐、无重叠、释放守恒 | 全部断言完成 | PASS |
| T03 | 部分槽复用 | 31 个 100 字节对象；释放/重填偶数槽 | 不新增页，活动数据不变 | 全部断言完成 | PASS |
| T04 | 八类扩容及独立回收 | 每类容量+1 个对象 | 两个 Slab，回收一个不影响另一个，owner 清理 | 全部断言完成 | PASS |
| T05 | 大小对象共存 | 8200/5000/24 | 分别独占 4/2/1 页；原请求 3 完整归还 4 | 全部断言完成 | PASS |
| T06 | 固定种子混合 | 0x20261021，1024 步，64 槽 | 每步状态/数据检查，周期全量，最终恢复 | 全部断言完成 | PASS |
| T07 | Buddy 与 SLUB 共存 | 裸 Buddy 原请求 3 页+128 步混合操作 | 裸页模式不变，按原请求释放 | 全部断言完成 | PASS |

原 Buddy K01 原基础检查、K02 请求 1/2/3/4/5/8、K03 不足及溢出失败不变、K04 种子 0x20261021 共 1024 步、K05 活动 5 页请求跨 128 步保持有效，均在此次真实 QEMU 中运行并通过；原实现及测试未修改。

## 压力记录及资源恢复

| 序列 | 种子 | 随机步骤 | 含清理及最终重申请的操作数 | 成功分配 | 释放 | 随机分配失败 |
|---|---|---|---|---|---|---|
| stress_a | 0x20261021 | 10000 | 10036 | 5018 | 5018 | 0 |
| stress_b | 0x13579BDF | 10000 | 10030 | 5015 | 5015 | 0 |

两组压力本次没有遇到分配失败，不将其宣传为随机耗尽覆盖；真正耗尽和碎片化由单独受控用例验证。额外清理/复查操作不计入 20000 随机步。两组最终均恢复 128 页，活动对象和 Slab 为 0。

QEMU 最终恢复本次 F0=**31723** 页。GDB 在第一次有效 slub_free 之前观察到实际对象：VA `0xffffffffc0416080`、PA `0x80416080`；Slab VA `0xffffffffc0416000`、PA `0x80416000`；Page 描述符 VA `0xffffffffc02df370`，索引 1046，槽宽 32。实际读取 24 字节 `0x55`。这四类地址没有混用。

检查结束后 GDB 停在 `init.c` 原驻留循环 PC `0xffffffffc0200118`：panic=false、live_objects=0、slab_count=0、owner 全零、八条 partial 链均空、251 个测试记录指针清零、31723 空闲页。见 `gdb/inspect.gdb`、`terminal.log`、`serial.log`、`resident.json`。

相比上一轮 31726 页，本次少 3 页并非测试结束泄漏：真实链接 end 从 `0xffffffffc02d1204` 增至 `0xffffffffc02d498c`，页对齐后 Page 描述符起始由 `0xffffffffc02d2000` 移到 `0xffffffffc02d5000`。新增正常测试代码和静态测试记录数组（6024 字节）扩大镜像；实际启动 F0 已为 31723，结束恢复相同数量。历史证据保留于 `evidence/slub_implementation/`，本次 ELF 符号见 `environment_and_layout.log`；没有把历史页数写死在测试中。

## 失败证据与修正分类

没有证实 SLUB 算法 Bug，生产实现未修复或重写。以下均为测试建设时发现的问题，保留原源码和失败日志：

1. `host_initial/compile.log`：两个 for 语句触发 `-Werror=misleading-indentation`。补充大括号后编译；未降低编译告警或断言要求。
2. `host_first_run/oom_small.log`：首轮 72/73，测试错误地按 1000 字节计算每页 4 个槽，实际请求进入 1024 类，每页 floor(4000/1024)=3。最小复现为 2 页资源连续申请七次 1000 字节，第七次正确返回 NULL。修正测试预期容量为 `2*((PGSIZE-TEST_HEADER)/expected_width(n))`；保留耗尽用例并增加恢复检查，`host_regression` 完整通过。
3. 早期 N12/N13 使用私有元数据故障注入，不符合本次只读辅助检查边界。保留旧源和日志，最终改为通过公共接口构造的大对象旧指针/Page 描述符非法释放，不能把旧注入当作额外通过项。最终完整回归见 `host_accepted/`。
4. 审查快照时去掉整片物理映射的比较，改为明确列出的活动数据及管理元数据片段，避免以读取已释放用户载荷判断正确性。此后重新运行全部 73 项，而非沿用之前结果。最终差异见 `harness_corrections.diff`。

## 适用范围与尚未覆盖边界

本 Prompt 计划类别全部实际执行，未执行项目 0；有限测试并非形式化证明。ASan/UBSan 只用于 Host 构建，不用于 QEMU 内核。Host 将未登记/保留的映射页 poison、只 unpoison 登记页；ASan 能发现这类越界和普通 C 访问问题，但不会自动为自定义 SLUB 每个槽生成对象红区或在对象释放时追踪生命周期。槽内越界、复用覆盖和资源泄漏另依赖对象账本、完整/抽样数据及内部不变量检查；不能声称 sanitizer 单独证明了全部内存安全。

当前单 hart、串行环境，没有验证多核并发/中断重入；不扩展当前接口的线程安全保证。压力未枚举全部操作序列，抽样步骤之间的暂态载荷错误仍可能遗漏。地址被合法重新分配给新对象后的 ABA 旧指针，与合法新指针数值相同，当前接口无 generation 信息，不能保证识别。没有通过故障注入模拟任意元数据损坏。

扩展 slub_check 是启动阶段自检，要求 SLUB 已初始化且没有预先活动对象，适用于现有 init 调用点，不是任意在线运行时检测接口。零资源/重复初始化/预期 panic 都只在独立 Host 实例运行；正常 QEMU 不耗尽全机物理页、不故意 panic。

综合实际运行与静态核对，本轮专项测试交付完成。结论限定为以上实现、构建产物与覆盖范围；没有未完成的本轮测试项目。
