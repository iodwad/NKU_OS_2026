# Lab2 4.6：MMU / Sv39 / 软件TLB验收证据

本目录归档2026-10-25的实际运行。总体PASS，逐项范围见[acceptance.json](./acceptance.json)。没有修改QEMU、Buddy、SLUB、Best-Fit、page_init或生产页表；没有使用截图代替运行。2026-10-24取指案例单独放在[historical_20261024](./historical_20261024/results.json)。

## 环境与源码配置

- Ubuntu 22.04 WSL；QEMU 4.1.1 Debug：`/home/andy/os_lab2_qemu_debug/build/riscv64-softmmu/qemu-system-riscv64`，有DWARF info/line。
- QEMU源码：`/home/andy/os_lab2_qemu_debug/qemu-4.1.1`；系统QEMU 6.2.0未替换。GDB版本与所有哈希见[provenance.json](./provenance.json)。
- 仓库工具起点：`c93ff75`；当前生产默认选择Best-Fit。为复用已验证的Buddy/SLUB组合，Guest源码从该仓库历史提交`b639987b1dd80ab93ea5b54e4cc193505ef27f01`独立解包，只覆盖本次调试工具和带debug-verify入口的Makefile。没有把旧代码覆盖回当前分支。
- 实际新建源码目录：`/home/andy/os_lab2_mmu_acceptance_20261025_214753/repro/code`。最后验收原始输出：`/home/andy/os_lab2_mmu_acceptance_20261025_214753/repro/code/.debug/evidence/verify-20261025-220646`；真实命令为`PATH=/home/andy/opt/riscv/bin:$PATH TZ=Asia/Shanghai make debug-verify`。
- 测试使用自动选择的本地回环端口和独立`.debug/session.json`。原有手工会话PID 25564/25601/25629及2345端口未操作；本次子进程已退出，本次端口已释放。

## 一条命令复现

在WSL内，从仓库根目录执行：

```bash
bash lab2/report/docs/2411560/mmu_debug/reproduce.sh
```

该命令也已从Git历史干净副本实际复跑通过，结果见[reproduction_check.json](./reproduction_check.json)。脚本只在新mktemp副本里编译和运行，不清理已有目录、不切生产管理器、不安装QEMU。已有路径不同时可提供`QEMU_BUILD`、`QEMU_SRC`、`GUEST_GDB_DIR`环境变量。需要本机已有交叉编译器、两个GDB和Debug QEMU；不修改全局PATH或系统调试保护。

若直接在自己的独立Buddy测试副本运行，先为`.debug/qemu-build`和`.debug/qemu-4.1.1`准备现有构建/源码链接，然后`make debug-verify`。不要与该副本的手动make debug混跑。当前生产Best-Fit配置不代表已经通过Buddy/SLUB联合验证。

## 手动复查关键停点

自动入口使用GDB/MI，源码停点全部写入[host-gdb.log](./host-gdb.log)，Guest逐条指令与数据写入[guest-gdb.log](./guest-gdb.log)。复查时可用grep定位以下原命令；断点编号属于本次独立Host，不照搬到其他会话。

```gdb
# Host：load_helper主表比较，首次读取有效局部量后next到victim查询
break accel/tcg/cputlb.c:1271 if addr == 0x41234560
# 每层PTE读取完成，ppn这一行尚未执行，所以先next一次再print
break target/riscv/cpu_helper.c:254 if addr == 0x41234560 && mmu_idx == 1
next
info locals
print/x base
print/x pte_addr
print/x pte
print/x ppn
# 填充缓存项前后核对
break accel/tcg/cputlb.c:835 if vaddr == 0x41234000 && mmu_idx == 1
next
print *te
```

手动Guest探针的恢复是三步，不能省略中间SFENCE：

```gdb
sv39-probe-restore
stepi
sv39-probe-restore-finish
```

Host暂停时先continue Host，再操作Guest。探针仅可在kern_init首次入口、PMM初始化前使用。Bare对照也只在一次性QEMU内完成，使用物理scratch代码地址，最后先恢复SATP再恢复根表与内存。

## 安全检查和恢复

本次内核end=`0xffffffffc02d498c`，PMM的pages=NULL、npage=0；DTB位于0x82200000。Scratch为PA `[0x87ffa000,0x87ffe000)`，在128MiB RAM内，远离OpenSBI、内核/BSS、根表、DTB及未来描述符数组。探针只临时使用尚未登记的尾部页，恢复后才继续内核初始化。

4个页分别保存指令、数据、中间表和末级表；根表第1项原为0。实际校验16384字节scratch、8字节根项逐字节相等，PC/t0/t1/t2/SATP全部恢复。镜像文件没有被探针写回，失败进程也不会把临时RAM写到生产镜像。最终Buddy 5/5、SLUB 7/7，is_panic=0，活动对象/Slab为0，free_count=31723。

## 文件索引

| 文件 | 内容 |
|---|---|
| [result.json](./result.json) | 最后一次完整验收，含三级实测、source-next停点、Bare、恢复和清理 |
| [host-gdb.log](./host-gdb.log)、[guest-gdb.log](./guest-gdb.log)、[qemu.log](./qemu.log) | 完整原始文本输出 |
| [translation_walk.md](./translation_walk.md)、[independent_calculation.json](./independent_calculation.json) | 逐层表与十个读取问题、独立地址计算 |
| [source_trace.md](./source_trace.md) | 取指/数据两条路径、13项分支的摘录和执行范围 |
| [tlb_comparison.md](./tlb_comparison.md)、[breakpoint_counts.json](./breakpoint_counts.json) | 标签、填充次数、Bare和硬件比较 |
| [attempts.json](./attempts.json)、[initial_failure/result.json](./initial_failure/result.json) | 失败与修正过程的结果索引，未覆盖旧记录 |
| [provenance.json](./provenance.json) | 构建/源码/原日志哈希、实际路径及原会话保护检查 |
| [acceptance.json](./acceptance.json) | 逐项验收与未触发路径 |

## 本轮实际修正的调试工具问题

1. 原验证器在Host继续运行后发送`-break-info`而超时；启用MI异步后才能继续接收命令，Guest末尾interrupt同样需要异步。
2. 原恢复命令在GDB Python内部执行stepi后紧接着写寄存器，MI场景下执行顺序不可靠。拆为恢复根项、显式单步SFENCE、完成内存/寄存器恢复三个动作。
3. GDB恢复PC后立即查询出现缓存值0x80000a02；诊断中flush register-cache并invalidate_cached_frames后读回正确高地址。保留显式64位写入和逐寄存器回读核对。
4. 采集PPN必须先跨过254行赋值；本轮扩展日志首次曾过早采集，现先next到256行再记录，并继续单步验证非叶子base更新。

失败均在独立QEMU中结束，属于调试器协调、恢复检查和记录时机问题；没有修改分配或地址翻译算法来制造PASS。第一次失败的Host/Guest日志末110行另存[initial_failure](./initial_failure/host-gdb.log)，所有完整原日志仍在attempts.json所列本地路径。

## 实测范围

已完成新数据三级4KiB映射、真实ld取值、首次主表/victim miss、翻译/填充、重复读取无refill、S-mode Bare对照、资源恢复。第二次JIT快速路径没有逐指令单步；权限失败、无效项、大页错位、A/D原子写回及victim命中交换仅作源码分析，不能标成异常测试通过。
