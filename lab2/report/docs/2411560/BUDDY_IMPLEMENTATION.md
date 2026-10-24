# Buddy 实现与基础运行记录（2026-10-21）

STATUS：SUCCESS（限定本轮实现、集成及基础验证；并不表示完整专项测试已经通过）

## 工作区和前置源码

开发目录：`D:\college_study_materials\junior_year_fall\OS\lab2\buddy_work`。

现场检查发现共享 `lab2/code` 仍选择 default，缺少 page_init，Best-Fit 尚未完成，另有用户 Buddy 草稿；这些均未修改。当前已完成前置任务的源码实际在刚下载的团队仓库：

- URL：`https://github.com/iodwad/NKU_OS_2026.git`
- 分支：`lab2`
- Commit：`b2298c6f0be7b2b5001b3d29d7b039cb7f72c19b`
- 工程子目录：`lab2/NKU_OS_2026_lab2/lab2`

先与原先 39 文件准备快照逐个对照，确认将替换的工作区源文件没有准备之后的本地修改，再同步。所有被替换文件位于 `evidence/implementation_20261021/backup_before_sync/`；逐文件来源、旧哈希、新哈希和比较结果在 `sync_changes.json`、`source.json`。大部分文件还有 CRLF/LF 字节差异，不能把同步文件数量直接理解为算法修改数量。原文档和历史日志保留。没有复制上游 obj/bin 或 .git。上游自身未跟踪的 lab2.zip 保持原样。

相对于同步后的完整基线，生产代码只增加 buddy_pmm.c、buddy_pmm.h，以及 pmm.c 的 include 和选择两处修改；page_init、First-Fit、Best-Fit、官方测试、启动代码和 Makefile 均与该基线逐字节一致。最终保护目录哈希证明 Lab1、共享 code、下载仓库均未被本轮改变，见 `integrity_final.json`。

## 内部表示与 SPEC 对应

分阶空闲链表按 order 管理空闲头部，复用 Page.page_link。静态 records 数组逐页记录未登记、空闲头、空闲内部、分配头和分配内部；头部另存 order，分配头保存原始请求 requested。数组上限由 `(PHYSICAL_MEMORY_END-DRAM_BASE)/PGSIZE` 得到，本工程为 32768 页；MAX_ORDER=15，编译断言约束映射容量不能超过它。修改物理映射上界时须审查该限制。

records 在 BSS 中，本工程 ABI 每项 16 字节，共 524288 字节。它随内核镜像尾部由既有 page_init 一并保留，既不使用 malloc/kmalloc，也不调用自身申请页面。实际本轮内核 end 为 `0xffffffffc0286170`，页描述符数组从下一页开始，预留完成后有 31801 页可用。该数量包含新增管理元数据带来的实际内存成本，不能与旧内核的空闲页数直接等同。

Page 字段保持框架语义：保留位只在合法输入区间解除；PG_property 只标记空闲块头；头部 property 为块的页数，其他页面 property=0。私有 order 和原申请长度不挤占 Reserved/Property 位。内核现有单 Hart 串行调用约定保持不变，不提供并发锁。

| 接口 | 实现行为 |
| --- | --- |
| buddy_init | 清零私有状态、初始化各阶空链表；不访问尚未建立的 pages 数组；F=0。仅允许在没有需保留活动分配的初始化阶段调用。 |
| buddy_init_memmap | 先完整校验描述符范围、未登记状态及 Reserved，再修改页面。按真实 PPN 对齐分解任意长度区间，登记全部 n 页；允许后续非重叠登记，F+=n。 |
| buddy_alloc_pages | n=0 断言；先拒绝超容量请求，避免取整溢出；找到最小可用阶并逐级拆分；原申请 n 对应最小 2 的幂 q，F-=q。没有合法块时 NULL，状态不变。 |
| buddy_free_pages | 必须匹配仍存活的分配头及原申请 n；完整预检实际 q 页后才归还全部 q 页，F+=q。错误起点、长度、重复释放、部分释放等通过断言明确拒绝。 |
| buddy_nr_free_pages | 返回实际可再次分配的空闲物理页数；拆分和合并本身不改变计数。 |
| buddy_check | 通过公共接口申请 1 和 3 页，校验占用 1+4、对齐、无重叠、释放第二块时第一块仍活动，最后恢复原总数；不重置全局链表。 |

所有区间检查先用数值地址验证描述符位置和 sizeof(Page) 对齐，再进行指针运算；长度通过 `n <= count-index` 验证，避免 base+n 溢出。拒绝动作使用内核 assert/panic，属于停止并诊断，不是返回错误码；这与 void 初始化/释放接口一致。

伙伴使用 `partner_ppn = ppn XOR (1 << order)`；当前块和同阶伙伴必须是完整空闲块，父块范围也必须位于可管理区间。私有 FREE_HEAD 只由完整登记的空闲块产生，分配会移除此记录，释放先验证整个活动块，因此该记录证明块内没有未登记洞或活动页面。可以合并不同调用登记的相邻合法区间，不能仅凭物理相邻合并。拆分和合并都不更改 free_count，只有登记、成功申请和配对释放更改。

## 实际编译和基础测试

环境为原 Ubuntu-22.04 WSL（22.04.5）、GCC 10.2.0、GNU objcopy 2.35.1、QEMU 6.2.0、GNU Make 4.3、GDB 12.1；没有安装、升级或覆盖工具链。完整版本输出见 environment.log。

以下命令均在该工作区的 WSL 路径下执行，实际命令行、原始输出及子进程退出码已保存。Python 调度脚本本身正常退出不代表内部命令成功，应以各 log 的 EXIT_CODE 和 *_results.json 为准。

1. 基础 host 探针先运行一次：因 buddy_pmm.h/c 尚不存在而编译退出 1，见 red_0.log。这只是记录模块缺失，不是一次已运行算法的行为失败测试。
2. 实现后编译同一个探针，编译/执行分别退出 0。探针使用真实框架头文件和 manager 回调，注册起点 pages+3、长度 29 的区间后运行 buddy_check，恢复 29 页；只作为辅助逻辑验证，不代表 RISC-V 内核运行。见 host_0/1.log。
3. `make obj/kern/mm/buddy_pmm.o` 退出 0，使用工程原始 -Wall -Werror 等参数。见 object_0.log。
4. `make -j2` 退出 0，生成 `bin/kernel`、`bin/ucore.img`。见 kernel_0.log。
5. 原 Makefile 等价启动参数：

```sh
timeout --signal=TERM 8s qemu-system-riscv64 -machine virt -nographic -bios default -device loader,file=bin/ucore.img,addr=0x80200000
```

OpenSBI 显示 `Domain0 Next Address: 0x0000000000000000`，没有进入内核；超时退出 124。见 qemu_loader_0.log。这不是 Buddy 检查失败；不能把这次超时当作正常内核驻留。

使用兼容当前 QEMU/OpenSBI 的同一镜像入口参数，未修改源码或 Makefile：

```sh
timeout --signal=TERM 8s qemu-system-riscv64 -machine virt -nographic -bios default -kernel bin/ucore.img
```

此时 OpenSBI Next Address 为 `0x80200000`，实际输出如下，见 qemu_kernel_0.log：

```text
memory management: buddy_pmm_manager
physical memory: [0x0000000080000000, 0x0000000088000000)
buddy_check: request 1+3 occupies 1+4 pages; restored 31801 free pages
check_alloc_page() succeeded!
satp virtual address: 0xffffffffc0205000
satp physical address: 0x0000000080205000
```

该次退出 124 是受控超时；额外以 GDB 硬件断点确认了实际驻留位置，而不是只凭成功文字判断：在 `kern/init/init.c:43` 停住，PC=`0xffffffffc0200110`，反汇编为跳回自身的 j；free_count=31801，is_panic=0，manager 名字为 buddy_pmm_manager。GDB 退出 0，调试 QEMU 随后由脚本主动终止。见 gdb_resident.log、qemu_gdb_serial.log、kern_init_disassembly.log。没有修改启动页表或 page_init。

## 复现与恢复 Best-Fit

进入 Ubuntu WSL 后：

```sh
cd /mnt/d/college_study_materials/junior_year_fall/OS/lab2/buddy_work
make obj/kern/mm/buddy_pmm.o
make -j2
timeout --signal=TERM 8s qemu-system-riscv64 -machine virt -nographic -bios default -kernel bin/ucore.img
```

若要恢复 Best-Fit，只把 kern/mm/pmm.c 的 `pmm_manager = &buddy_pmm_manager;` 改回 `pmm_manager = &best_fit_pmm_manager;` 后重新 make。原 Best-Fit 源码、头文件及 check 全部保留；pmm_selection.diff 精确记录了相对于 Best-Fit 基线的两处编辑。当前交付仍选 Buddy，没有实际切回 Best-Fit。

官方 Best-Fit 评分会要求 Best-Fit 管理器，故没有把 make grade 当作 Buddy 验收，也没有改动其检查。不要直接对当前 Buddy 选择宣称通过 Best-Fit 评分。

## 尚未执行的项目

下一份 Prompt 再完成专项覆盖：多区间/乱序登记及跨区间合并、各种阶数拆分和多级回收、耗尽及碎片失败恢复、SIZE_MAX/越界/非法指针、重复/重叠登记、错误长度/内部指针/重复释放的隔离 panic 测试、随机序列不变量检查，以及有多个既存活动分配时的完整恢复检查。本轮只实现其必要逻辑，不宣称这些专项测试已通过。

不支持部分释放、并发分配或 SLUB；前两项是本轮契约边界，SLUB 未开发。默认 make qemu 的原有 loader 入口问题仍存在，已有可复现的 -kernel 启动方式。
