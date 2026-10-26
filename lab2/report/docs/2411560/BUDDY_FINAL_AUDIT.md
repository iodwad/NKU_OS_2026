# OS Lab2 Buddy System 最终审计

审计日期：2026-10-21。工程：`D:\college_study_materials\junior_year_fall\OS\lab2\buddy_work`。

**最终判断：PASS。** 在当前课程约定的 128 MiB 映射上限、单 Hart 串行调用、完整配对释放条件下，Buddy 实现与专项测试达到本次给定的任务三验收要求。可以结束 Buddy 任务，进入 SLUB Challenge 的设计阶段。本轮未实现 SLUB，也未修改 Buddy 算法、测试断言、管理器选择或官方评分逻辑。

结论分级：**[运行]** 为本轮实际执行或逐条处理运行记录的结果；**[静态]** 为源码与文档审阅；**[边界]** 为未由现有运行覆盖、需保留的适用条件。PASS 不代表形式化证明所有操作序列或任意内存配置均正确。

## 1. 代码正确性审计

审阅 `kern/mm/buddy_pmm.c`、`buddy_pmm.h`、`pmm.c`，并核对 `memlayout.h`、`pmm.h`、`list.h` 和链接脚本。当前生产文件、测试文件及主要构建产物与上轮最终记录一致；本轮没有改动既有源码。

| 检查点 | 结论与源码依据 | 证据类型 |
|---|---|---|
| 非幂区间完整初始化 | `buddy_pmm.c:114` 先验证整个区间；cursor/remaining 每次推进所发布块大小，直到 remaining=0。不会向下截断 n；F 最后只增加原始 n。13 页、起点 +3 的 29 页、分段登记均通过。 | 静态 + H02/H03/H04/H42 运行 |
| 大小及物理对齐 | `buddy_pmm.c:146` 将 n 向上取整；登记按 `(nbase+cursor)` 对齐。拆分右子块保持对应阶对齐。H06 的 1/2/3/4/5/8 实际容量为 1/2/4/4/8/8；独立账本检查 PPN 对齐。 | 静态 + Host/QEMU 运行 |
| 拆分不重复、不丢失 | 原自由头经 unlink 移除；每降一阶只把右半块入链，左半块继续拆分，最后只将返回的 q 页设为活动。H07 把 16 页全部申请完，并在释放后再次申请完整 16 页。 | 静态 + H07 运行 |
| 伙伴计算单位 | `buddy_pmm.c:80` 使用 `ppn=nbase+index`、`partner_ppn=ppn XOR capacity`；没有对 Page 指针地址异或。描述符地址只用于索引校验和链表操作。 | 静态 + H03/H12/QEMU 运行 |
| 合并边界与所有权 | 合并要求同阶 FREE_HEAD、合法伙伴及父块范围、头部 Property/Reserved 状态有效。FREE_HEAD 只由完整已登记的空闲块产生；分配移除该头，合法释放先预检完整实际容量。因此在模块独占管理状态的前提下，不会跨未登记洞、初始保留区或活动分配。 | 静态 + H04/H08–H12/H17/H42 运行 |
| 非幂请求完整释放 | `records[index].requested` 保存原 n，order 保存 q 的阶；`buddy_pmm.c:184` 检查两者后释放 q。申请 3/5 分别扣除并归还 4/8 页，其额外尾页不会被再次分配。 | 静态 + H13/H14/H15/K02 运行 |
| 拒绝部分或错误释放 | `buddy_pmm.c:187` 要求活动块首部，`:188` 要求原始 n；内部起点、错误 n、部分释放及重复释放在改动状态前失败。 | 静态 + H23–H26 运行及原因复核 |
| 元数据大小、下标、生命周期 | capacity=32768，MAX_ORDER=15。records 每项 16 字节，共 524288 字节；分阶根 16 项、256 字节；检查覆盖缓冲区 32768 字节。下标来自管理域校验或已校验块内拆分/合并。BSS 位于 end 之前，由未修改的 page_init 保留。init 不调用动态分配且不访问尚未建立的 pages。 | 静态 + 目标 ELF/GDB sizeof 运行 |
| 空闲页数 | init=0、登记+n、成功申请−q、释放+q；link/unlink/split/merge 不独立改账。注册不重叠且域至多 32768 页，计数不会累计到 size_t 溢出。 | 静态 + 每步独立所有权账本、QEMU 不变量运行 |
| UB、越界、标志位 | checked_range 在描述符解引用前检查数值地址、sizeof(Page) 对齐及 `n<=count-index`。超大 n 在取整/移位前拒绝；成功路径 wanted/order≤15，块内索引与父块大小有域上界。property 最大 32768，可由 unsigned int 表示。私有状态不借用 Reserved/Property 位。未发现当前合法调用路径中的 UB、整数溢出或越界。 | 静态 + 43 项 ASan/UBSan Host 运行；不构成全路径证明 |

未发现需修复的 Buddy 算法错误，因此没有真实 Bug 的最小复现条件或修复补丁。没有为了满足审计结论修改实现。`init()` 仍是启动期接口，不能用于清空有活动分配的管理器；调用 alloc/free 前须已建立合法 pages/nbase/npage 管理域。

## 2. 测试有效性审计

| 审计问题 | 核查结果 | 证据 |
|---|---|---|
| 是否调用生产源码 | Host 文件第 3 行直接 include `../kern/mm/buddy_pmm.c`；接口包装转发至实际 manager。独立当前构建的 raw 镜像与正在使用的镜像 SHA-256 一致。 | 静态 + 新编译、镜像哈希 |
| 是否存在替代算法 | 没有另一份被测 Buddy。possible() 只扫描独立登记位图和未占用集合上的合法对齐区间，既不拆分也不进行 XOR 合并。 | 静态 |
| 账本是否独立 | registered、owners、live 由测试输入和实际返回结果维护；容量由原始请求计算。预期是否成功由独立位图判定。verify 再把实际链表/记录与独立集合交叉核对，不只让 records 自证。 | 静态 + 压力日志独立回放 |
| 43 项是否分别执行 | 新 results.json 恰为 H01–H43、无缺号、无重复。每项通过独立 subprocess 启动；共用代码分支的 H08/H09、H13/H14、H19/H20 有不同输入或操作序列。 | 本轮运行 + JSON 去重 |
| 19 项非法调用原因 | H22–H39 加 H43，均实际触发生产文件断言。审计额外逐项匹配断言文本、源码行及输入原因；19/19 相符。正常返回未触发断言会失败，测试自身 CHECK 失败也不会算成预期 panic。 | 本轮运行 + negative_assertion_audit.json |
| 正负测试隔离 | Host 每例独立进程；panic 适配器比较 records、全部 arena、各阶根、free_count、initialized 的调用前快照后退出。退出 0 表示捕获并核验预期诊断，不是非法调用正常成功。QEMU 不执行故意 panic 或重置管理器。 | 静态 + 日志 |
| 20000 步是否真实 | H19/H20 的 seed 为 0x20261021/0x13579BDF，各 10000 循环步。审计用 Python 独立回放历史及本轮 TRACE 的每个操作、请求、PPN、容量、页所有权、剩余计数和释放顺序；全部一致。清理与后续 32 页申请另记，不冒充随机步骤。 | 历史与本轮日志回放 |
| QEMU 5 项是否固定 PASS | manager.check 实际连接 `buddy_checks.inc:203`；每个 PASS 前均执行相应检查函数和断言，原基础检查未删除。当前源码从零交叉构建，镜像与活动镜像一致；QEMU 输出 K01–K05 后进入真正驻留循环。 | 静态 + fresh build/QEMU/GDB |
| 资源是否恢复 | Host 每项最终恢复登记总量，压力账本清空；K04 释放所有临时块，K05 保留的已有块及其记录逐字节不变后完整释放。新 QEMU 日志与 GDB 均为 31792 页且无 panic。 | 本轮运行 |
| 报告是否一致 | BUDDY_TEST_REPORT 主表恰有 48 个唯一编号，与 43 Host + 5 QEMU 对应。压力汇总的两行不重复计入测试数。原报告未把 QEMU 负面用例或 ASan 内核覆盖记录为通过。 | 文档与 JSON/原始日志核对 |

### 2.1 负面测试的断言原因

本轮逐项核对结果保存在 `evidence/final_audit/negative_assertion_audit.json`，可按编号回到 `evidence/buddy_tests/audit_host/Hxx.log`。

| 用例 | 实际拒绝原因 |
|---|---|
| H22 | `n > 0`，buddy_pmm.c:148 |
| H23/H24/H27/H28 | 不是活动分配首部，buddy_pmm.c:187 |
| H25/H26 | 与原始申请 n 不匹配，buddy_pmm.c:188 |
| H29/H30 | 区间已含已登记页，buddy_pmm.c:118 |
| H31/H34/H37/H39 | 超过描述符管理域，buddy_pmm.c:50 |
| H32/H33/H35/H38 | 下界、非空、正长度检查失败，buddy_pmm.c:46 |
| H36 | 不是 sizeof(Page) 对齐的描述符位置，buddy_pmm.c:48 |
| H43 | 登记区间中有页不满足 Reserved 前提，buddy_pmm.c:119 |

原测试适配器只锁定“来自生产 buddy_pmm.c 的断言 + 全状态不变”，没有锁定每例的具体表达式，这是测试维护上的弱点。本轮独立原因核验关闭了当前 19 个结果的可信性疑问；没有改原适配器或断言。后续修改算法时仍应复核具体原因。

### 2.2 ASan/UBSan 的实际范围

实际编译参数包括：

```sh
gcc -fsanitize=address,undefined -fno-sanitize-recover=all \
  -fno-omit-frame-pointer -fno-pie -no-pie -std=gnu99 \
  -fno-builtin -Wall -Werror -Wno-unused -O1 -g \
  -Ilibs -Ikern/mm -Ikern/debug -Ikern/driver \
  tests/buddy_host_tests.c -o <本次Host可执行文件>
```

`audit_host/compile.log` 保存完整命令。审计还用 ldd/nm 确认可执行文件链接 libasan.so.6、libubsan.so.1，并有 sanitizer 符号。H01–H43 全部使用这一可执行文件分别运行，无 sanitizer 错误；`-fno-sanitize-recover=all` 禁止在发现错误后继续假报成功。

覆盖的是 x86-64 Host 中的真实 Buddy 源码、包含的检查逻辑及 Host 测试路径。它们不覆盖真实 RISC-V QEMU 的指令执行、物理数据页或所有逻辑域越界。RISC-V 使用工程原有交叉编译参数，未启用 ASan/UBSan。

## 3. 本轮实际验证命令与结果

以下命令在既有 Ubuntu-22.04 WSL 下执行；绝对路径、cwd、原始输出和退出码在各日志中保留。没有覆盖先前测试日志。

| 执行内容 | 命令/入口 | 结果 |
|---|---|---|
| 保存快照、重建历史版本、读取布局 | `python3 tests/final_audit_evidence.py` | 退出 0；旧 raw 镜像哈希与历史记录一致 |
| 全套现有 Host 测试 | `python3 tests/run_buddy_tests.py --tag audit_host --sanitize` | 退出 0；43/43，含 19 个预期诊断 |
| 当前工程已有集成入口 | `python3 tests/run_buddy_kernel.py --tag audit_kernel --build` | 退出 0；当前构建已最新；QEMU 5/5 |
| 独立当前重建及验证 | `python3 tests/finish_final_audit.py` | 退出 0；隔离副本实际重新编译对象和内核，raw 镜像与活动镜像一致；GDB 及压力回放通过 |
| 旧镜像复跑与最终完整性检查 | `python3 evidence/final_audit/close_audit.py` | 退出 0；旧镜像恢复 31801 页；既有源码、共享目录及产物哈希核对通过 |

独立当前副本在 `evidence/final_audit/current_kernel/`，历史副本在 `baseline_kernel/`；均不修改正在使用的源码或共享工程。两份副本内实际执行了 `make`；当前副本先执行 `make obj/kern/mm/buddy_pmm.o`，随后 `make -j2`，均真实编译、退出 0。活动目录自己的 make 因目标已最新而不重新编译，不能把那次输出单独说成“从零构建”。

当前 QEMU 命令仍为：

```sh
timeout --signal=TERM 25s qemu-system-riscv64 -machine virt \
  -nographic -bios default -kernel bin/ucore.img
```

原活动镜像的本轮复跑在全部检查完成后由 timeout 结束，退出 124。独立当前副本另外执行已有 `confirm_buddy_resident.py`：GDB 在 `kern/init/init.c:43` 停住，PC=`0xffffffffc0200110`，指令跳回自身，`is_panic=0`，free_count=31792，管理器名为 buddy_pmm_manager；GDB 退出 0，随后主动结束 QEMU。没有仅靠“没打印 panic”或超时判断正常驻留。

关键证据：

- `evidence/buddy_tests/audit_host/`：本轮 43 项 Host 结果。
- `evidence/buddy_tests/audit_kernel/`：本轮活动镜像 5 项 QEMU 结果。
- `evidence/final_audit/current_object.log`、`current_build.log`：当前源码隔离重编译。
- `evidence/final_audit/current_kernel/evidence/buddy_tests/gdb_final/`：新构建的真实 QEMU/GDB 原始输出。
- `evidence/final_audit/stress_replay.json`：历史和本轮压力日志独立回放结果。
- `evidence/final_audit/integrity_and_verdict.json`：源码范围、历史产物一致性及最终核验。

## 4. 31801 与 31792 页差异

**[运行] 不是根据“新增了测试所以大概少了几页”推断。** 审计从专项测试前保存的完整源码快照重建旧内核；其 raw 镜像 SHA-256 为：

```text
f5f89c0edab191848d03f1d8541975aa055a7e19231238acca707992ba6d2c54
```

与第一轮 `implementation_20261021/integrity_final.json` 完全一致；本轮复跑旧镜像也实际恢复 31801 页。独立当前构建与活动镜像共同的 SHA-256 为：

```text
3700e5eb9a527e9d9226e24c1bb1d12d0c8e3f7e66803c3fb6e8556b7e788ea8
```

来自 nm、readelf、size、GDB sizeof 与未修改 page_init 的计算如下：

| 项目 | 原基础检查版 | 完整专项检查版 |
|---|---:|---:|
| .text 字节数 | 6266 | 10212 |
| .rodata 字节数 | 3248 | 5728 |
| .data 起始虚拟地址 | 0xffffffffc0203000 | 0xffffffffc0204000 |
| .bss 字节数 | 524632 | 557400 |
| records 字节数 | 524288 | 524288 |
| 新增 check_seen 字节数 | 无 | 32768 |
| end 虚拟地址 | 0xffffffffc0286170 | 0xffffffffc028f170 |
| 页描述符数组起始物理地址 | 0x80287000 | 0x80290000 |
| 描述符总字节数 | 0x140000 | 0x140000 |
| free_start 物理地址 | 0x803c7000 | 0x803d0000 |
| 计算及实际恢复空闲页数 | 31801 | 31792 |

RISC-V 目标类型实际 sizeof(Page)=40、sizeof(buddy_record)=16，描述符数量固定为 32768。原 page_init 使用：

```text
pages_pa = ROUNDUP(end,4096) - va_pa_offset
free_start = ROUNDUP(pages_pa + 32768*40,4096)
free_pages_count = (0x88000000-free_start)/4096
```

新增检查的代码和只读字符串使后续页对齐数据段起点后移 4096 字节；BSS 因 check_seen 增加 32768 字节。end 与 free_start 合计后移 36864=`0x9000` 字节，即 9 页。源码公式、两代 ELF 和两次实际初始化输出一致，未发现永久回收泄漏。

## 5. 未覆盖风险及严重程度

| 风险或限制 | 严重程度 | 审计判断 |
|---|---|---|
| 负面驱动未逐例锁定断言表达式 | 中，测试维护风险 | 当前 19 项原因已由本轮外部核验确认，无现存算法故障；以后换实现不能只看其 PASS 文本。 |
| Host arena 大于多数用例的逻辑域，sanitizer 无法单独发现仍在 arena 内的逻辑越界 | 中，覆盖限制 | 当前依靠 checked_range 的静态上界证明、独立位图/所有权/计数交叉检查补足；未在所有成功路径设置逻辑域外 canary。 |
| 所有阶、所有碎片序列没有穷举；随机压力主要为小域，最大阶专例主要验证整块边界 | 低，本轮非阻塞 | 已覆盖要求中的 16 页拆分、多级恢复、最大阶/取整边界及真实大内存布局；不把 20000 步称为全路径证明。 |
| 已登记空闲块内部若被其他代码直接改成 Reserved、改写 page_link 或私有元数据 | 接口约束；违规使用可严重破坏状态 | 合并依赖 FREE_HEAD 对整个合法块的证明，没有每次复扫伙伴全部成员。动态保留没有对应 API，不属于当前合法调用。SLUB 必须通过配对页接口取得/归还自身页面，不直接修改仍由 Buddy 管理的空闲状态。 |
| 仅完整配对释放，不支持 First-Fit 式中间区间释放 | 已声明的兼容性限制 | free 必须传原始 n；不能传取整 q，除非原 n=q。官方 First-Fit/Best-Fit 专项用例不能直接当 Buddy 测试。 |
| 静态 32768 页管理容量、MAX_ORDER=15、单 Hart 无锁 | 已声明的范围限制 | 更大映射或并发调用须另行设计。512 KiB 管理记录和 32 KiB 检查缓冲区是真实静态成本，不是隐藏空闲页。 |
| 完整性检查和分配/释放的页扫描有开销 | 低，性能边界 | 本轮验收正确性，不承诺严格 O(log n) 实现性能或面向生产环境的压力吞吐。 |
| 无 RISC-V sanitizer、无 QEMU 故意 panic、未验证分配页内容写读模式 | 低，明确范围 | 特定布局与非法操作由 Host 覆盖；QEMU 覆盖真实接口、所有权和恢复，不把 Host 结果等同于全部 RISC-V 路径。 |

以上不是已发现的 Buddy 算法 Bug；没有严重的、需要先修复才能继续课程实验的问题。后续 SLUB 若扩展了以上契约，需重新验收对应集成行为。

## 6. 课程交付完整性与两份 Prompt 的依据

| 要求 | 本次核查 |
|---|---|
| 第一份实现 Prompt 原文 | 原附件仍完整存在。审计前工程内缺少独立副本；本轮按原字节复制为 `evidence/final_audit/implementation_prompt.txt`，SHA-256 为 `35720eba7ddb4acb4db3e92d3af37bf2fc954b68715090d8d452d4253d568cce`，与原附件相同。不能声称上一轮已在项目内归档。 |
| 第二份测试 Prompt 原文 | 上轮已有 `evidence/buddy_tests/original_prompt.txt`；与原附件及本轮 `final_audit/test_prompt.txt` 均逐字节一致，SHA-256 为 `c9bb37edfd16a1d91e316abe3b16ffc5fce2e83b71cc858f8b2135173654d4ec`。 |
| SPEC 接口、正常行为和边界 | `BUDDY_INTERFACE_SPEC.md` 是准备阶段的方案中立规约；其中保留了当时未选定的策略。最终采用的向上取整与完整配对释放，以第一份 Prompt、`BUDDY_IMPLEMENTATION.md` 和真实源码为准；这些文档共同记录 init/登记/分配/失败/释放/计数/生命周期边界。不能单读旧 SPEC 的备选表误以为当前支持精确 n 页或部分释放。 |
| Prompt 设计依据 | 两份原文、实现/测试计划及本节说明共同保留，见下文。 |
| 测试覆盖与有效性 | `BUDDY_TEST_REPORT.md` 有逐项 48 行结果表和覆盖解释；本审计另外检查独立 oracle、真实断言原因、压力轨迹及内核入口。 |
| 真实 Bug 与修复 | 实现轮、专项测试轮、本审计均未发现已确认的算法 Bug；明确记录“无算法修复”，未编造经历。连接新检查前缺少 K01–K05 标记的历史失败，是测试尚未接入，不是 5 个算法错误。 |
| 源码及真实证据 | kern/mm、tests、两轮 evidence 原始输出齐全；本轮另有两代内核重建、真实 QEMU/GDB、类型尺寸和哈希证据。 |
| 恢复 Best-Fit | 实现说明保留操作：把 pmm.c:38 改回 `pmm_manager = &best_fit_pmm_manager;` 后重新 make。Best-Fit 源码、声明和 check 均保留；本轮未切换。 |
| 小组共享工程与评分 | 审计前后 Lab1、共享 code、下载仓库树哈希未变；现有 Buddy 源码和测试未修改。官方 test_lab2.c:106 明确要求 Best-Fit，所以当前 Buddy 选择不能宣称通过该评分；未修改、跳过或欺骗它。 |

### 6.1 第一份 Prompt 的设计依据

第一份以真实 pmm_manager、Page、PPN 转换和 init_memmap 作为 RELY，避免误套其他 uCore 版本；GUARANTEE 固定可链接回调和实际管理器入口；SPEC 将区间输入、成功/失败、资源守恒及异常诊断拆开，避免只写一个能返回地址的算法。n 与 q 的区别、原始请求配对释放、非幂/不对齐/多区间登记是现有接口最容易歧义的部分，因此原文给出可观察规则。静态元数据要求消除了尚未初始化就调用自身分配服务的循环依赖。独立工作区和不改官方检查的约束用于保护队友前置代码及原评分口径。

### 6.2 第二份 Prompt 的设计依据

第二份把“实现看起来正确”转化为可核验的输入和预期状态：拆分必须把剩余页申请出来；合并必须重新申请完整大块；非幂申请必须证明尾页未重分配；失败必须保留已有所有权和计数。特定布局及预期 panic 在 Host 隔离，真实内核只执行可归还的检查，防止测试清空正在使用的资源。独立逐页账本和固定种子轨迹用于防止 Buddy 自己的错误元数据成为唯一 oracle。三层验证区分 Host 逻辑、RISC-V 编译和 QEMU 实际执行；上述区分在日志与报告中均保留。

本审计生成报告、审计脚本、原文副本、独立构建和日志，没有自动重写旧 SPEC、原测试或生产算法。已有报告保留历史状态，并由 README 指向最新结果。

## 7. 最终验收判断

**PASS。** 现有正确性与课程要求的测试类别得到真实验证；未发现需要修复的算法错误。两代内存数量差异已由同源重建、链接布局、初始化公式和实际 QEMU 输出闭合解释。当前 43 Host + 5 QEMU 的最终结果可信，未将重复回归、压力清理操作或构建检查膨胀为额外用例数。

Buddy 任务可以结束，并进入 SLUB Challenge。开始下一阶段前应保留本目录的实现、报告和 evidence；SLUB 调用页分配器时继续遵守“原始 n、原始 base、完整释放”以及单 Hart 的现有契约。本轮到此停止，不实现 SLUB、不 Push。
