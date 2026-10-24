# Buddy System 专项测试结果

STATUS：SUCCESS。最终去重用例共 48 项：Host 43 项、真实 RISC-V QEMU 5 组，48 项通过、0 项失败、0 项未执行。构建、GDB 检查及重复回归不重复计入用例数。

## 范围与测试对象

工作区：`D:\college_study_materials\junior_year_fall\OS\lab2\buddy_work`。Host 文件直接 include 当前 `kern/mm/buddy_pmm.c`，没有复制替代算法。负面用例各用独立进程，panic 适配器只在分配器自身触发断言且调用前快照逐字节相同时报告通过并退出；不在真实内核捕获或跳过断言。

本轮未发现分配/释放算法 Bug，未修改算法。buddy_pmm.c 仅将原 buddy_check 改名 buddy_check_basic，并 include 新测试文件。原基础检查的断言和执行顺序保留；manager.check 指向新增包装入口，依次执行基础与专项检查。buddy_pmm.h、pmm.c、First-Fit、Best-Fit、page_init、Makefile 和官方测试均未改变。保护目录及精确差异见 `evidence/buddy_tests/integrity_final.json` 和 `buddy_pmm.diff`。

## 实际结果

| 测试编号 | 测试功能 | 测试输入 | 预期结果 | 实际结果 | 状态 |
|---|---|---|---|---|---|
| H01 | 2 的幂初始化 | 对齐起点登记 16 页 | 完整分配 16 页并回收，逐页覆盖与总数均为 16 | PASS H01 operations=2 registered=16 restored=16 | PASS |
| H02 | 非 2 的幂初始化 | 登记 13 页，逐页耗尽 | 恰好成功 13 次单页申请，第 14 次失败；恢复 13 页 | PASS H02 operations=27 registered=13 restored=13 | PASS |
| H03 | 未大块对齐初始化 | 起点 pages+3，登记 29 页 | 恰好 29 页可分配；所有原生块按真实 PPN 对齐 | PASS H03 operations=59 registered=29 restored=29 | PASS |
| H04 | 不连续登记与保留洞 | 区间 [0,8)、[16,24) | 16 页申请失败，两个 8 页申请成功，不跨洞合并 | PASS H04 operations=5 registered=16 restored=16 | PASS |
| H05 | 逆序相邻登记 | 先 [8,16)，再 [0,8) | 可重新申请完整 16 页，证明跨登记调用的合法伙伴合并 | PASS H05 operations=2 registered=16 restored=16 | PASS |
| H06 | 基本请求与互斥 | 连续申请 1、2、3、4、5、8 页 | 实际容量 1、2、4、4、8、8，全部对齐且互斥，逆序释放恢复 64 页 | PASS H06 operations=12 registered=64 restored=64 | PASS |
| H07 | 多级拆分与剩余可用性 | 16 页区间申请 1、2、4，再申请 9 个单页 | 16 页全部可用，无重叠；耗尽失败，释放后可申请 16 页 | PASS H07 operations=27 registered=16 restored=16 | PASS |
| H08 | 伙伴仍被占用 | 16 页分成两个 8 页，只释放一个 | 另一块仍活动，16 页申请失败，不错误合并 | PASS H08 operations=5 registered=16 restored=16 | PASS |
| H09 | 合法同阶伙伴合并 | 先后释放两个 8 页伙伴 | 可重新申请完整 16 页；不仅检查总数 | PASS H09 operations=7 registered=16 restored=16 | PASS |
| H10 | 顺序多级合并 | 16 个单页按分配顺序释放 | 可重新申请完整 16 页 | PASS H10 operations=34 registered=16 restored=16 | PASS |
| H11 | 乱序多级合并 | 16 个单页按 (i*5)%16 顺序释放 | 可重新申请完整 16 页 | PASS H11 operations=34 registered=16 restored=16 | PASS |
| H12 | 相邻非伙伴 | 登记 [4,8)、[8,12) | 总空闲 8 页但申请 8 页失败；两个 4 页仍可用 | PASS H12 operations=5 registered=8 restored=8 | PASS |
| H13 | 申请 3 页的隐藏容量 | 16 页中申请 3 页，再逐页耗尽余量 | 独占 4 页且尾页不会重分配，配对释放后可申请 16 页 | PASS H13 operations=29 registered=16 restored=16 | PASS |
| H14 | 申请 5 页的隐藏容量 | 16 页中申请 5 页，再逐页耗尽余量 | 独占 8 页且尾页不会重分配，配对释放后可申请 16 页 | PASS H14 operations=21 registered=16 restored=16 | PASS |
| H15 | 混合取整与乱序回收 | 申请 1、3、5、2、4，释放顺序 2、0、4、1、3 | 实际容量守恒，恢复后可申请完整 32 页 | PASS H15 operations=12 registered=32 restored=32 | PASS |
| H16 | 超出当前空闲资源 | 16 页申请 17 页 | NULL，完整状态逐字节不变，后续 16 页仍可申请 | PASS H16 operations=3 registered=16 restored=16 | PASS |
| H17 | 碎片导致的失败 | 16 单页中仅释放偶数物理页，再申请 2 页 | 总空闲 8 页但无合法连续二页块；失败不变，单页仍可分配，最终恢复大块 | PASS H17 operations=37 registered=16 restored=16 | PASS |
| H18 | 最大阶与安全容量计算 | 完整 32768 页；申请 32769、SIZE_MAX、32768、16385 | 越界安全 NULL；最大块及向上取整至最大块均可申请并归还 | PASS H18 operations=7 registered=32768 restored=32768 | PASS |
| H19 | 固定种子压力 A | seed=0x20261021，两段共 214 页，10000 步 | 独立位图判定可分配性，每一步检查全部所有权、计数和链表；最终恢复 | PASS H19 operations=10018 registered=214 restored=214 | PASS |
| H20 | 固定种子压力 B | seed=0x13579BDF，相同资源，10000 步 | 不同可复现序列；每一步检查、最终恢复且可继续申请 32 页 | PASS H20 operations=10017 registered=214 restored=214 | PASS |
| H21 | check 保留外部活动分配 | 持有申请 5 页的块，ref=7，调用完整 manager.check | 原分配仍活动且 ref 不变；check 临时块全部归还 | PASS H21 operations=2 registered=128 restored=128 | PASS |
| H22 | 非法零申请 | alloc_pages(0) | 真实分配器断言，全部状态不变 | PASS H22 expected allocator panic; full state unchanged | PASS |
| H23 | 重复释放 | 申请 3 页并归还，再次 free(base,3) | 真实分配器断言，无重复加计数 | PASS H23 expected allocator panic; full state unchanged | PASS |
| H24 | 错误内部起点 | 申请 3 页后 free(base+1,3) | 真实分配器断言，不回收活动块 | PASS H24 expected allocator panic; full state unchanged | PASS |
| H25 | 错误长度但相同取整容量 | 申请 3 页后 free(base,4) | 真实分配器断言，不能仅按取整容量接受 | PASS H25 expected allocator panic; full state unchanged | PASS |
| H26 | 不支持的部分释放 | 申请 3 页后 free(base,2) | 真实分配器断言，完整活动块不变 | PASS H26 expected allocator panic; full state unchanged | PASS |
| H27 | 释放保留页 | 释放未登记的 Reserved 页 | 真实分配器断言，全部状态不变 | PASS H27 expected allocator panic; full state unchanged | PASS |
| H28 | 释放未登记且非 Reserved 页 | 隔离环境清除未登记页保留位后释放 | 仍由登记/分配记录拒绝，不仅依赖 Reserved 位 | PASS H28 expected allocator panic; full state unchanged | PASS |
| H29 | 重复登记 | 两次登记 [8,16) | 真实分配器断言，无重复资源 | PASS H29 expected allocator panic; full state unchanged | PASS |
| H30 | 后段重叠登记的原子拒绝 | 已有 [8,16)，请求 [4,12) | 在首次写入前拒绝；前四个新页也不得被部分登记 | PASS H30 expected allocator panic; full state unchanged | PASS |
| H31 | 登记越过上界 | 256 页描述符域内请求 [250,260) | 真实分配器断言，全部状态不变 | PASS H31 expected allocator panic; full state unchanged | PASS |
| H32 | 登记低于数组下界 | 数值构造 pages 地址减 sizeof(Page) | 先验证地址后访问，明确拒绝 | PASS H32 expected allocator panic; full state unchanged | PASS |
| H33 | 零长度登记 | init_memmap(pages,0) | 真实分配器断言，全部状态不变 | PASS H33 expected allocator panic; full state unchanged | PASS |
| H34 | 登记长度溢出边界 | init_memmap(pages,SIZE_MAX) | 通过差值边界检查拒绝，无越界或整数溢出 | PASS H34 expected allocator panic; full state unchanged | PASS |
| H35 | NULL 释放 | free_pages(NULL,1) | 真实分配器断言，无非法解引用 | PASS H35 expected allocator panic; full state unchanged | PASS |
| H36 | 非描述符对齐登记 | 地址为 (char*)pages+1 | 真实分配器断言，无非对齐描述符访问 | PASS H36 expected allocator panic; full state unchanged | PASS |
| H37 | 释放上界描述符 | free_pages(pages+256,1) | 真实分配器断言，不访问可管理域外页面 | PASS H37 expected allocator panic; full state unchanged | PASS |
| H38 | 零长度释放 | 有效分配首地址，n=0 | 真实分配器断言，活动分配不变 | PASS H38 expected allocator panic; full state unchanged | PASS |
| H39 | 溢出长度释放 | 有效分配首地址，n=SIZE_MAX | 真实分配器断言，活动分配不变 | PASS H39 expected allocator panic; full state unchanged | PASS |
| H40 | 初始化后尚未登记 | 空管理器申请 1 和 SIZE_MAX | 均 NULL，资源与完整状态不变 | PASS H40 operations=2 registered=0 restored=0 | PASS |
| H41 | 有空闲页时的超大申请 | 16 页可用时申请 SIZE_MAX 和最大阶+1 页 | 均 NULL，后续完整 16 页仍可申请 | PASS H41 operations=4 registered=16 restored=16 | PASS |
| H42 | 活动分配期间再次登记 | 登记 16 页并持有 8 页，再登记远处 8 页 | 既有活动分配不变，新增资源累计为 24 页 | PASS H42 operations=6 registered=24 restored=24 | PASS |
| H43 | 登记尾部页面不满足 Reserved 前提 | 未登记区间第 8 页预先清除 Reserved，登记 8 页 | 全部预检后拒绝，前 7 页也不得部分转换 | PASS H43 expected allocator panic; full state unchanged | PASS |
| K01 | 原基础检查回归 | 当前真实内核资源，申请 1 和 3 页 | 占用 1+4，回收后恢复调用前数量；原断言全部保留 | buddy_tests K01: PASS original basic check | PASS |
| K02 | 内核混合分配与回收 | 真实 RAM 连续申请 1、2、3、4、5、8 页 | 容量、PPN 对齐、互斥、页面状态与计数逐步正确，乱序释放恢复 | buddy_tests K02: PASS requests 1,2,3,4,5,8; mixed release | PASS |
| K03 | 内核分配失败不变性 | 当前 F+1、SIZE_MAX、最大阶+1 页 | NULL，状态哈希和精确计数均不变；只读不变量成立 | buddy_tests K03: PASS unavailable and overflowing requests preserve state | PASS |
| K04 | 内核固定种子压力 | seed=0x20261021，64 槽账本，1024 步 | 逐步验证互斥、实际容量、空闲结构、全局资源守恒，最终完全归还 | buddy_tests K04: PASS seed=0x20261021 steps=1024 ownership and conservation | PASS |
| K05 | 内核已有分配保护 | 持有申请 5 页的块，再运行 128 步压力测试 | 既有 8 页描述符与分配记录逐字节不变；最后全部归还 | buddy_tests K05: PASS existing 5-page request survives 128 stress steps | PASS |

## 测试方法与新增函数

- `tests/buddy_host_tests.c`：setup 只重置隔离的 arena。add_region、take、release 经实际 manager 公共接口操作；registered/owners/live 是独立登记集合和申请账本。possible 只扫描独立位图上大小对齐的连续区间，既不使用 Buddy 链表结果判定成功条件，也不重做拆分/合并算法。verify 每次核对空闲覆盖、实际分配覆盖、页面保留/头标志、链表前后链接和页数守恒。
- `save_state/unchanged/__panic`：负面用例及资源不足失败分别比较完整 records、所有描述符、各阶链表根、free_count 和 initialized；被拒绝调用不能产生部分写入。只有真实 buddy_pmm.c 的断言才算预期 panic，测试自身失败不会被当成通过。
- `kern/mm/buddy_checks.inc`：buddy_check_invariants 只读验证所有已登记资源的空闲/活动覆盖、物理 PPN 对齐、计数和合法伙伴已合并状态；不重新登记或重置实际管理器。check_live_ledger 使用原请求计算实际容量并对测试活动块两两检查不重叠。
- buddy_check_live_mix、buddy_check_live_failure、buddy_check_live_stress、buddy_check_preserve_allocation 分别对应 K02–K05。压力循环每一步检查账本和全局结构，结束归还全部临时块。K05 逐字节验证被持有的活动块描述符和记录。

所有 Host TRACE 行记录操作序号、成功分配/释放的 Page 描述符地址、物理页号、原请求 n、实际容量 q 和剩余空闲页数；失败申请记录 n 和未改变的空闲页数。这些是 Host 描述符模拟地址，不是 QEMU 内核虚拟地址。压力序列统计包含循环结束后的清理与后续 32 页可分配性验证：

| 用例 | 序列步数 | 成功申请 | 资源不足失败 | 配对释放 |
|---|---:|---:|---:|---:|
| H19 | 10000 | 2688 | 4642 | 2688 |
| H20 | 10000 | 2721 | 4575 | 2721 |

## 编译、运行与日志

使用原 Ubuntu-22.04 WSL 和既有交叉工具链，无安装或替换。所有命令的完整参数、原始输出、退出码在日志首尾保存；摘要从 results.json 读取实际执行状态。

在 WSL 下进入工作区：

```sh
cd /mnt/d/college_study_materials/junior_year_fall/OS/lab2/buddy_work
python3 tests/run_buddy_tests.py --tag reproduce_host --sanitize
python3 tests/run_buddy_kernel.py --tag reproduce_kernel --build
```

每个 tag 对应新目录，已存在则拒绝覆盖旧记录。run_buddy_tests 的 --cases H01 H02 等选项可复现单项。Host GCC 使用 `-fsanitize=address,undefined -fno-sanitize-recover=all`，错误立即失败；本轮未出现 ASan/UBSan 错误。

真实编译命令为 `make obj/kern/mm/buddy_pmm.o` 和 `make -j2`，均退出 0。新增内核检查经 buddy_pmm.c 的 include 一并交叉编译，不进入任何 Host 模拟路径。QEMU 使用：

```sh
timeout --signal=TERM 25s qemu-system-riscv64 -machine virt -nographic -bios default -kernel bin/ucore.img
```

QEMU 超时退出 124；5 组测试和 check_alloc_page 均已完成，没有 panic。随后独立 GDB 运行在 kern/init/init.c:43 的驻留循环断点停住，PC=`0xffffffffc0200110`，is_panic=0，manager 为 buddy_pmm_manager，GDB 退出 0。GDB 调试 QEMU 由调度脚本主动结束，具体返回值保存在 gdb_final/results.json，不能把它与 timeout 返回码混淆。

本轮基础与专项检查后均恢复 31792 页。旧版恢复 31801 页，本轮内核 end 从 `0xffffffffc0286170` 变为 `0xffffffffc028f170`，增大 0x9000 字节，即 9 页；32 KiB 的只读检查覆盖缓冲区及新增代码随内核保留，因此可用页数相应减少。这是静态测试占用，不是分配回收泄漏。所有测试预期均由运行前状态计算，没有硬编码 31792 或 31801。

主要证据目录 `evidence/buddy_tests/`：

- `original_prompt.txt`：本次原始要求。
- `before/`、`after/`、`buddy_pmm.diff`：源码快照和精确测试入口差异。
- `host_initial/`：原始 41 项用例首轮结果。
- `host_extended/`、`host_final/`：接入内核检查后和新增两项用例后的回归记录。
- `host_checked/`：最终 43 项 ASan/UBSan 检查，逐项日志含全部 TRACE，summary.json 为机器摘要。
- `kernel_before_extension/`：新增入口前的负向连接检查，5 个新标记均不存在，调度器退出 1；旧基础检查正常。该记录是确认测试尚未接入，不能解释为发现 5 个算法 Bug，也不计入最终失败数。
- `kernel_extended/`：真实对象/内核编译日志及 QEMU 的 5 项结果。
- `gdb_final/`：第二次真实 QEMU 完整测试、最终 PC/计数/panic 检查。
- `summary.json`、`integrity_final.json`、`environment.log`：最终汇总、范围核验与环境信息。

## 失败证据和修复记录

Host 首轮未发现算法失败；最终 43 项也未发现算法失败。因此没有算法最小复现输入，也没有算法修复补丁。新增内核测试接入前，测试驱动按预期因缺少 K01–K05 标记失败；保留该日志后只新增测试及其入口，随后同一驱动通过。未删除用例、放宽断言或修改既有分配/释放函数。

## 覆盖含义与验证边界

初始化测试通过逐页耗尽和独立登记集合核对，能够暴露非 2 的幂尾页丢失、未对齐登记扩张及跨保留洞登记。拆分测试进一步申请所有余量，能够发现未使用伙伴丢失或重复入链；只检查一次非 NULL 不具备这项能力。

合法合并和多级合并测试在释放后重新申请完整大块，能够发现“计数恢复但未形成可用大块”的缺陷。非伙伴测试有足够总空闲页却没有合法对齐区间，能够发现仅按相邻地址错误合并。非 2 的幂测试将剩余资源耗尽，独立 owners 位图保证额外独占尾页不被重分配，并验证原请求长度释放可完整回收。

异常测试在首次 panic 时比较全量管理状态，可以发现重复加计数、部分登记和先破坏链表再拒绝等问题。资源守恒测试每步验证登记集合等于空闲覆盖加活动分配实际容量，固定种子序列则覆盖多轮拆分、碎片失败和回收交错。独立 oracle 不接受按同一套 Buddy 算法重算出来的结果作为唯一判据。

全部指定类别均有已执行用例。特定初始化布局和故意 panic 的覆盖在 Host，真实 QEMU 执行的是不重置分配器的 5 组正常专项检查；没有执行 QEMU 负面 panic 用例。单 Hart 串行契约之外的并发安全、所有可能操作序列的形式化穷举、SLUB 不在本轮范围。未运行要求选择 Best-Fit 的官方评分，也不对其结果作声明。
