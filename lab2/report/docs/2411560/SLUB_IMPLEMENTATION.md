# 简化 SLUB：实现、接入与基础验证

STATUS：SUCCESS。本轮完成两层分配器实现、内核接入和真实 RISC-V 基础验证。完整专项测试留待第二份 Prompt。

工作目录：`D:\college_study_materials\junior_year_fall\OS\lab2\slub_work`。
WSL：`/mnt/d/college_study_materials/junior_year_fall/OS/lab2/slub_work`。

## 1. 来源、范围和原文

从通过最终审计的 `../buddy_work` 复制源码和测试，共 50 个源/构建文件；复制前核对其最终产物和主要源文件哈希。没有复制 bin、obj、evidence 或 .git，没有用旧 ZIP 覆盖完整 Buddy。原工作区不存在才建立副本，遇到已有 slub_work 会停止而不覆盖。

实际新增生产文件：

- `kern/mm/slub.c`：Slab、对象状态、大小分类、页面来源索引、分配与释放。
- `kern/mm/slub.h`：四个公开接口。
- `kern/mm/slub_checks.inc`：由 slub.c 包含的基础检查；不是另一份分配算法。

现有源码仅修改 `kern/init/init.c`：增加头文件，并在 pmm_init 返回之后调用 slub_init、slub_check。Makefile 本来就自动枚举 kern/mm 下的 C 文件，因此没有修改它。新增验证脚本位于 tests 中，设计与日志位于本目录及 evidence 中。

`buddy_pmm.c/.h`、`buddy_checks.inc`、pmm.c/page_init、First-Fit、Best-Fit、原测试及评分工具与复制时逐字节一致。完整目录哈希确认 buddy_work、共享 code、Lab1 和下载仓库均未被本轮修改，见 `evidence/slub_implementation/final_verification.json`。

本次原始 Prompt 原字节保存在 `evidence/slub_implementation/original_prompt.txt`，SHA-256：

```text
a066c2f5e92d11bc91943db8df924e83d8f89f4d33cd83f50ada7083dc895727
```

复制来的 BUDDY_* 文档是上一阶段历史记录；不能把其中的 43 个 Host 测试算作本轮 SLUB 测试。

## 2. 两层结构与设计选择

实际调用关系如下：

```text
kern_init
  pmm_init
    选择 buddy_pmm_manager
    page_init
    原 Buddy 基础检查及 5 组专项检查
  slub_init                   // 不注册为 pmm_manager
  slub_check
    slub_alloc(字节数)
      小对象：寻找对应大小类的部分空闲 Slab
        没有可用槽位 -> alloc_pages(1) -> 页内 Slab 头 + 槽位
      大对象：alloc_pages(原始页请求数)
      从 Page 描述符得到 PA，再校验并转换为数据 VA
    slub_free(对象 VA)
      安全查找所属 Slab -> 归还槽位
      Slab 完全空闲或大对象释放 -> free_pages(原始 Page*, 原始页请求数)
  原驻留循环
```

固定静态对象池不能体现按需向第一层索取物理页；所有对象直接交给 Buddy 又无法体现页内复用。因此本实现选用单页 Slab、大小类、空闲槽索引和位图。没有 per-CPU、NUMA、多核锁或构造函数等 Linux 扩展机制。目标仍为单 Hart、串行访问。

所有对象和 Slab 头都实际位于通过 alloc_pages 获取的数据页。静态 owner 表只是“物理页描述符下标 → 所属 Slab 头”的来源索引，不提供对象存储空间。这样可以在解引用对象候选页头之前确认其属于 SLUB，代价是 32768×8=262144 字节 BSS。既有 page_init 会把它作为内核镜像的一部分保留，不依赖 malloc、kmalloc 或自身的对象申请。

## 3. 对象大小、元数据和状态

目标 RISC-V ABI 实测 `sizeof(struct slub_slab)=88`；页内按 16 字节取整保留 96 字节，前 96 字节不能交给调用方写入。

| 字段 | 含义 |
|---|---|
| magic | 活动 Slab 的标记，归还前清除 |
| page | 第一层返回的首 Page 描述符 |
| page_request | 原始页请求数；不能擅自改为 Buddy 实际容量 |
| object_bytes | 小对象为大小类的槽宽，大对象为原始字节请求 |
| link | 所属大小类的部分空闲链表节点，属于 Slab 头而非 Page |
| class_id | 0–7 为小对象类，8 为直接页分配的大对象 |
| slots/inuse | 槽位总数和活动对象数 |
| free_head | 空闲槽索引，0xFFFF 表示无空闲槽 |
| used[4] | 256 个占用位，最小类最多 250 槽，不会超过位图范围 |

### 3.1 小对象

请求 1–2048 字节向上选择最小合适的类，返回整个槽位中的有效存储，至少满足原始请求。所有指针满足 16 字节对齐，内容不自动清零。

| 槽宽（字节） | 每页槽数 `(4096-96)/槽宽` |
|---:|---:|
| 16 | 250 |
| 32 | 125 |
| 64 | 62 |
| 128 | 31 |
| 256 | 15 |
| 512 | 7 |
| 1024 | 3 |
| 2048 | 1 |

空闲对象的前两个字节保存下一个空闲槽的索引；分配出去后它们属于调用方，分配器不再依靠这些内容判断占用状态。占用状态保存在页头位图。释放时先验证对象首址、槽边界和占用位，之后才写入空闲链。

partial[class] 只链接有可用槽位的 Slab；页满后移出，释放后若再次有空槽且仍有活动对象则重新加入。若释放了最后一个对象，先解除缓存链表及来源记录，再调用 free_pages(page,1)；不保留空页缓存。

### 3.2 大对象

超过 2048 字节走直接连续页路径，不会仅因超过单页就拒绝。页头同样占 96 字节：

```text
total = requested_bytes + 96
page_request = total/4096 + (total%4096 != 0)
q = 不小于 page_request 的最小 2 的幂
```

在做加法前先检查 `requested_bytes <= 32768*4096-96`，避免大请求溢出；再限制原始页请求不超过当前描述符域及支持容量。超过表示上限或没有合法连续页时返回 NULL。支持的表示范围为 2049 至 134217632 字节，但这不是可分配容量承诺：实际成功还受保留页、Buddy 对齐/取整及最大空闲块约束。

| 字节请求 | 原始页请求 | Buddy 实际独占页数 | 释放参数 |
|---:|---:|---:|---|
| 5000 | 2 | 2 | free_pages(page,2) |
| 8200 | 3 | 4 | free_pages(page,3) |

owner 表覆盖这次实际独占的 q 页，额外尾页不能另作对象来源。返回地址为第一页数据 VA+96；必须释放这个准确的首地址。释放前保存 page 和原始 page_request 到局部变量，清除全部来源记录和 Slab 标记，再归还整个 Buddy 分配；free_pages 之后不再访问已归还页中的页头。

## 4. 地址转换与真实例子

当前 pmm.h 的 KADDR 被注释。`slub.c:49` 的 slub_page_data 执行以下检查：

1. pages、npage/nbase 及描述符地址有效；描述符索引和实际 q 页均在检测的 RAM 描述域内。
2. `page2pa(base)` 得到实际页起始 PA；整个块位于本内核支持的 `[0x80200000,0x88000000)` 范围。
3. va_pa_offset 必须等于当前 PHYSICAL_MEMORY_OFFSET，PA 加偏移不溢出，整个 VA 区间位于 `[KERNBASE,KERNTOP)`。
4. 这些范围与 entry.S 中 Sv39 的高地址 1 GiB 映射相容。本实现不任意扩大映射范围，也没有修改启动页表。

GDB 在第一次非 NULL 的 slub_free 入口、对象仍存活时实际取得：

| 地址/量 | 实际值 |
|---|---|
| Page 描述符 VA | 0xffffffffc02dc2f8 |
| 描述符数组下标 | 1043 |
| Slab 数据页 PA | 0x80413000 |
| Slab 数据页 VA | 0xffffffffc0413000 |
| 对象 VA | 0xffffffffc0413080 |
| 对象对应 PA | 0x80413080 |
| 槽宽、保留头长 | 32、96 字节 |

这里对象位于第二个 32 字节槽：页 VA+96+32。Page 描述符地址只用于描述物理页，不能存放对象；`PADDR(Page*)` 得到的是描述符自身位置，不能替代 `page2pa()`。GDB 实际读回对象的 24 字节均为 0x55，证明检查涉及真实数据地址，不只是比较描述符。

slub_free 在处理非 NULL 参数时先检查 VA/PPN 范围，再查询 owner。无来源记录就断言拒绝，不先读取任意地址处的伪页头。有效来源还需通过 magic、所属页、原始请求、大小类、槽数及占用位检查。重复释放、非首址、指向元数据或其他模块的页等可识别非法操作会在对象状态改变之前诊断。

与通常裸指针分配器一样，某地址释放后被新对象重新使用时，旧裸指针与新合法指针无法仅靠地址区分；没有实现带代际的句柄或完整 UAF 防护。调用方也不能直接把 SLUB 所属页归还 Buddy，或改写其元数据。

## 5. 各接口与 SPEC 的对应

| 函数/位置 | 行为与边界 |
|---|---|
| slub_init，slub.c:136 | 在 PMM 后初始化八个部分空闲链表、来源表和计数；延迟获取 Slab，初始化不调用 alloc_pages。重复初始化会断言，避免遗失活动对象。 |
| slub_alloc，slub.c:150 | 0 返回 NULL；按大小类优先使用现有空槽，无槽时申请页。大对象保存原请求。新页申请失败不建立活动 Slab；若页无法映射，按原请求归还后返回 NULL。 |
| slub_new_slab，slub.c:98 | 先做容量边界检查，调用第一层；布局页头和合法槽位，建立来源记录，不覆盖 Page 或 Buddy 私有记录。 |
| slub_free，slub.c:196 | NULL 安全返回；非 NULL 先校验来源和准确槽首址，只释放对应活动槽；空页立即归还，大对象完整配对归还。 |
| slub_return_slab，slub.c:186 | 保存第一层分配凭据、清除 owner 和 magic，再 free_pages；不在释放后读取页头。 |
| slub_check，slub_checks.inc | 使用公开对象 API；不同模式写读、复用、两种大对象路径、全量临时对象归还。保留运行前已有对象和实际页数基准，不重置 PMM 或 SLUB。 |
| kern/init/init.c:42 | pmm_init 成功执行所有原 Buddy 检查后才初始化和检查 SLUB；pmm_manager 始终为 Buddy。 |

没有修改 struct Page、Reserved/Property 位、Buddy 记录数组或其自由链表。对象级的 link、used、free_head 都在 Slab 数据页中；来源表由 SLUB 独立维护。

## 6. 实际编译、运行与基础覆盖

沿用现有 Ubuntu-22.04 WSL、GCC 10.2.0、QEMU 6.2.0、make 和 gdb-multiarch，没有安装或替换工具。

| 检查 | 实际结果 |
|---|---|
| 基础检查及接入代码先写入，SLUB 尚不存在时 make | 退出 2，链接器报告 slub_init/slub_check 未定义；这是缺模块的接入失败证据，不是已经运行算法的失败测试。 |
| SLUB 首次对象编译 | 退出 2，局部变量拼写错误，见下一节。 |
| 修复后 `make obj/kern/mm/slub.o` | 退出 0，使用原工程 -Wall -Werror 等参数。 |
| `make -j2` | 退出 0，生成 bin/kernel 和 bin/ucore.img。 |
| QEMU Buddy 回归 | 原 5 组检查全部通过。 |
| QEMU SLUB 基础检查 | 下表中的实际断言及内存写读通过，无 panic；超时退出 124。 |
| GDB 独立启动 | 抽查存活对象数据；最终停在原驻留循环，GDB 退出 0。 |

| 基础内容 | 实际输入与检查 |
|---|---|
| 零请求与空指针 | alloc(0)=NULL，free(NULL) 不改变页数。 |
| 不同小对象 | 同时持有 1/24/60/100/256/500/1000 字节对象；16 字节对齐、区间互斥，逐字节读回各自不同模式。 |
| 多对象与复用 | 额外同时持有两个 24 字节对象；释放一个后另一个及七个既有对象内容不变，重新分配不额外索取页面，写入新模式后再次交叉读回。 |
| 大对象 | 同时申请 5000 和 8200 字节，逐字节写读且互不影响其他对象；分别归还 2、4 个实际 Buddy 页。 |
| 状态恢复 | 所有临时对象归还后实际空闲页数恢复，活动对象数和 Slab 数回到检查前值；本次启动为 0。 |

真实运行输出：

```text
memory management: buddy_pmm_manager
buddy_tests: all 5 live checks passed; restored 31726 free pages
slub_init: lazy slabs, 16-byte alignment, small classes 16..2048 bytes
slub_check: small-object writes, isolation and slot reuse passed
slub_check: 5000-byte and 8200-byte writes and paired page return passed
slub_check: all basic checks passed; restored 31726 free pages
```

GDB 最终 PC=`0xffffffffc0200118`，位于 kern/init/init.c:46 的自跳转循环；is_panic=0、live_objects=0、slab_count=0。整张 owner 表全零，八个 partial 链表均为空，free_count=31726。没有仅靠固定成功文字或超时判定成功。

31726 是本次内核布局得到的数量，不是测试的硬编码预期。相比 Buddy 工程 31792，SLUB 的 256 KiB 来源索引、类链表/计数、代码和字符串扩大了内核镜像及页对齐保留范围；所有对象释放后恢复的是本次真实基准。实际 end 为 0xffffffffc02d1204，详见 environment_and_layout.log。

## 7. 真实错误及修复

首次对象编译时，slub_free 的断言误写 `slub->inuse`，应为当前局部变量 `slab->inuse`。编译器定位未声明的 slub；只修正这一标识符后重新编译通过，没有放宽或删除断言。错误日志、原始失败源码和单行补丁分别保存在：

- `evidence/slub_implementation/object/terminal.log`
- `evidence/slub_implementation/object/slub_before_fix.c`
- `evidence/slub_implementation/compile_fix.diff`

首次编译前自检还移除了一个多余的 Page 容器转换，缓存链表最终只使用 Slab 自身 link 的 to_struct 转换。该项没有作为已运行发现的算法 Bug 统计。完成修正后的基础 QEMU 检查未观察到运行期分配、释放或数据破坏错误。

## 8. 尚未验证和接口限制

本轮没有实现第二份 Prompt 的完整专项测试。以下行为已有实现，但不能从本轮基础结果宣称全部验证通过：

- 每个大小类的满页/部分空闲/空页全部转换及多 Slab 场景；尤其 2048 字节单槽类。
- 2049 等分界值、各种多页长度、接近上限及 SIZE_MAX 的完整边界测试。
- 强制底层耗尽、失败恢复和长期碎片场景。
- 重复释放、内部指针、元数据指针、非 SLUB 地址等隔离断言测试。
- 固定种子的对象压力测试、完整独立对象账本、Host ASan/UBSan。

运行环境仅单 Hart；不支持并发、任意动态映射变更、空页保留缓存、完整 Linux SLUB 特性或通用 UAF 防护。SLUB 管理的页只能通过 slub_free 归还；大对象释放所需原始页请求保存在页头，不要求调用方传长度。

## 9. 复现和证据位置

```sh
cd /mnt/d/college_study_materials/junior_year_fall/OS/lab2/slub_work
make obj/kern/mm/slub.o
make -j2
timeout --signal=TERM 25s qemu-system-riscv64 -machine virt -nographic -bios default -kernel bin/ucore.img
```

不要换回已知会使 OpenSBI 跳到 0x0 的通用 loader 启动方式。新日志可使用 `python3 tests/run_slub_validation.py qemu --tag <新目录名>`；已有目录拒绝覆盖。GDB 脚本首次使用 `tests/confirm_slub_resident.py`，已完成的记录在 gdb 目录，应保留原输出。

`evidence/slub_implementation/` 保存原 Prompt、来源与哈希、red/object/object_fixed/kernel/qemu 的命令和退出码、GDB 的存活对象样本及驻留状态、源码快照、带行号源码、集成及修复差异、环境与最终完整性结果。引导创建脚本为上级 `prepare_slub_workspace.py`；本目录测试脚本不修改原 Buddy 工程。

报告按 academic-report-workflow 的证据要求整理。本轮完成后停止，不开始 SLUB 专项测试、不 Push。
