# 软件TLB、重复读取与分页模式对照

## 第一次与第二次ld

本轮Guest指令是`ld t0,0(t1)`和`ld t2,0(t1)`，访问VA=0x41234560、宽8字节、S-mode、MMU_DATA_LOAD。原始停点与计数见[Host日志](./host-gdb.log)、[Guest日志](./guest-gdb.log)、[breakpoint_counts.json](./breakpoint_counts.json)。

| 观察 | 第一次 | 第二次 |
|---|---|---|
| 主表索引 | 52 | 同一目标页，读取前后标签均为0x41234000 |
| 初始读标签 | 0xffffffffffffffff，比较不匹配 | 读取前addr_read=0x41234000，比较匹配 |
| victim | 实际扫描8项，finish返回false | 未单独在JIT中跟踪victim分支 |
| riscv_cpu_tlb_fill累计命中 | 1 | 仍为1，断点保持启用 |
| get_physical_address累计命中 | 1，三个PTE停点 | 仍为1 |
| tlb_set_page累计命中 | 1 | 仍为1 |
| Guest结果 | t0=0x1122334455667788 | t2=0x1122334455667788 |

第一条还在load_helper:1338停住、next后读取了真实haddr及宿主RAM的8字节，再由Guest完成加载。填充后的addr_read与addr_write为0x41234000，addr_code为全1，因为本页R/W=1但X=0。addend是宿主RAM地址与Guest虚拟页的差，不是物理页号。

第二次的准确结论是“主表标签在访问前后匹配，且没有重新进入填充/页表翻译”。没有对TCG生成的那段机器码逐条单步，不把这一结果夸大成直接观测了JIT中每条命中跳转。单看一个断点未命中不足以排除victim命中等其他路径。

## 标签、索引与victim

- `tlb_index()`在include/exec/cpu_ldst.h:163–168，用虚拟页号和当前表的mask选槽；这不是Sv39某一级的页表索引，槽相同仍可能是不同虚拟页，必须比完整标签。
- `tlb_hit()`在include/exec/cpu-all.h:348–361比较页标签及无效位；read/write/code三个字段分别表达访问许可和慢路径标志，避免只读映射被误用于写入或执行。
- 主表miss后还会查victim小缓存，命中可将缓存项及iotlb交换回来，无需页表遍历。本轮第一次victim实测false，命中交换分支只有源码分析。
- `tlb_set_page`→with_attrs先解析Guest PA所属区域，再根据prot分别构造三个标签，写入iotlb、addend及主表项；因此软件TLB远不止“物理地址数组”。

## S-mode Bare与Sv39：同为8字节数据加载

为减少特权级混杂，本轮在同一个一次性Guest、同样S-mode下，改用物理scratch代码地址执行同样的加载；两种地址指向同一批物理数据。切换前后执行sfence.vma，最后恢复SATP、寄存器、根PTE及临时内存，再运行内核回归。

| 项目 | Sv39 | Bare |
|---|---|---|
| env->priv / mmu_idx | 1 / 1 | 1 / 1 |
| satp | 0x800000000008020a | 0 |
| ld访问地址 | VA 0x41234560 | Guest物理直通地址0x87ffb560 |
| get_physical_address | 三次查表，返回4KiB页基址0x87ffb000 | 实际next经过197–199，返回完整addr=0x87ffb560 |
| PTE停点计数 | 3 | 0 |
| tlb_set_page实参 | VA页0x41234000→PA页0x87ffb000，prot=3 | 地址页0x87ffb000→同名PA页，prot=7 |
| Guest读取值 | 两次均0x1122334455667788 | 两次均0x1122334455667788 |

Bare的两次ld确实都执行并返回正确数据，但为安全完成模式恢复，该组观察断点在第一次填充后禁用；不使用Bare计数声称第二次避免了fill。Sv39重复加载的填充断点则保持启用，计数证据完整。

这不是“同一个数值VA”的对照，而是同访问类型、同特权级、同宽度、同物理目标的可比较案例。Bare没有Guest页表，QEMU仍需判断Guest物理地址属于RAM还是设备，并把它转换成宿主可访问地址，因而仍使用软件TLB。

2026-10-24的M-mode取指0x1000直通保留在[历史结果](./historical_20261024/results.json)。源码在171行先处理M-mode直接返回，Bare在196–199另行处理；不能将M-mode与S-mode的所有差异都归因于satp。

## 与真实CPU硬件TLB的区别

| 比较点 | QEMU 4.1.1软件TLB | 真实CPU硬件TLB |
|---|---|---|
| 共同功能 | 缓存转换和许可；失效后需要补充转换 | 同样缓存转换与许可，具体结构由处理器实现 |
| 实现位置 | 宿主进程的CPUTLBEntry与TCG生成代码 | 处理器MMU内的硬件结构 |
| 缓存表示 | 分MMU上下文的标签、addend、iotlb、特殊访问标志及victim缓存 | 硬件页号、权限、ASID等；组织方式不等同于这些C字段 |
| 权限处理 | 页表翻译产生prot，分别编码read/write/code与慢路径 | 硬件按照架构规则检查，可能与缓存查询并行 |
| 地址终点 | 还要把Guest PA映射到MemoryRegion/RAMBlock与宿主HVA | 物理地址送往真实缓存/内存/设备系统 |
| 失效 | sfence等Guest操作触发QEMU软件缓存失效，调试/翻译块也参与软件控制 | 架构失效指令由硬件执行，微架构细节因处理器而异 |
| 时间与统计 | 受TCG、宿主缓存、操作系统调度及GDB断点影响 | 受真实流水线、页表遍历器和缓存层级影响 |

因此不能用本次GDB单步次数、软件TLB函数调用次数估计真实CPU的TLB延迟或性能。
