# 三级Sv39数据访存：观察值与独立计算

本次记录来自2026-10-25新建的一次性QEMU，详见[result.json](./result.json)及[Host原始日志](./host-gdb.log)。2026-10-24根级1GiB叶子取指另存historical_20261024，不混作本次三级记录。

| i | VPN索引 | 当前base（PA） | PTE地址（PA） | PTE | PPN | 置1标志 | 分支结果 |
|---|---|---|---|---|---|---|---|
| 0 | 1 | 0x8020a000 | 0x8020a008 | 0x21fff001 | 0x87ffc | V | 下一级 0x87ffc000 |
| 1 | 9 | 0x87ffc000 | 0x87ffc048 | 0x21fff401 | 0x87ffd | V | 下一级 0x87ffd000 |
| 2 | 52 | 0x87ffd000 | 0x87ffd1a0 | 0x21ffecc7 | 0x87ffb | V,R,W,A,D | 4KiB叶子 |

`levels=3`、`ptidxbits=9`、`ptesize=8`在三次循环中相同；`ptshift=18,9,0`。在ppn赋值的254行停住后先执行`next`到256行，才读取PPN，避免采集尚未赋值的局部变量。

## 十个关键问题

1. **base是什么？** 当前页表的Guest物理基址。首次是SATP低44位左移12，之后由非叶子PTE更新；不是宿主指针。
2. **idx来自哪几位？** 分别是VA[38:30]、[29:21]、[20:12]，本次观测1、9、52；0x41234560的低12位为0x560。
3. **ptesize为什么是8？** RV64 Sv39的PTE为64位；4KiB页表容纳512项，每级索引9位。
4. **pte_addr为什么是PA？** base已经是Guest物理页表基址，idx*8是表内字节偏移，两者相加仍是Guest物理地址。
5. **ldq_phys从哪读？** 通过cs->as的物理AddressSpace读取8字节PTE，RAM分支最终由MemoryRegion/RAMBlock解析到宿主指针；不会再次把pte_addr当作Guest虚拟地址走同一页表。Guest的ld则先经过TCG和Guest MMU。
6. **非叶子怎样判断？** V=1且R/W/X全0；本次前两项为0x21fff001、0x21fff401，均只设V。
7. **为什么base=ppn<<12？** 非叶子PPN指向下一张页表所在的物理页；左移12把页号换为页首字节地址，实测新base为0x87ffc000、0x87ffd000。
8. **为什么查1/2/3次？** 根级合法叶子对应1GiB页，查1次；中间叶子对应2MiB页，查2次；本次末级4KiB叶子查3次。中间叶子2MiB分支本轮没有单独触发。
9. **最终PA怎么算？** 叶子PPN=0x87ffb，ptshift=0，故4KiB物理页基址0x87ffb000，加页内偏移0x560得到0x87ffb560。
10. **函数为什么不一定返回字节PA？** 此版本Sv39分支把PPN/VPN低位组合后左移12，返回4KiB页基址；Bare分支直接返回addr，可能已包含偏移。tlb_set_page的调用再按4KiB对齐。

## 单步证据

Host对前两级实际执行`next`，停点依次256→259→261→237，更新后的base也被读取。末级实际经过259、262、265、268、273、276、279、283、286、291/292、295、333、334、337，随后`finish`返回0，读取输出指针得到0x87ffb000，prot=3。

这些是被执行的成功路径；无效PTE、权限失败、大页错位和A/D原子更新没有单独触发。页表项内A/D均已置位，更新比较直接跳过写回。

## 独立交叉核对

```text
root = (0x800000000008020a & ((1<<44)-1)) << 12 = 0x8020a000
VPN2=(VA>>30)&511=1; VPN1=(VA>>21)&511=9; VPN0=(VA>>12)&511=52
PTE_PA0 = 0x8020a000 + 1*8 = 0x8020a008
PTE_PA1 = 0x87ffc000 + 9*8 = 0x87ffc048
PTE_PA2 = 0x87ffd000 + 52*8 = 0x87ffd1a0
leaf_ppn = 0x21ffecc7 >> 10 = 0x87ffb
PA = (leaf_ppn << 12) | 0x560 = 0x87ffb560
```

与get_physical_address输出、tlb_set_page实参和两次Guest加载交叉核对，t0、t2均为0x1122334455667788。计算检查结果见[independent_calculation.json](./independent_calculation.json)。
