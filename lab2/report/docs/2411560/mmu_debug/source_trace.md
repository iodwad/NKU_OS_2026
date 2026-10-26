# QEMU 4.1.1源码路径与关键分支

源码根目录为`/home/andy/os_lab2_qemu_debug/qemu-4.1.1`，版本、源文件SHA-256与未修改检查见[provenance.json](./provenance.json)。行号均取该版本，不套用新版QEMU。历史取指与本轮数据访存分别记录。

## 取指路径

```text
get_page_addr_code (accel/tcg/cputlb.c:1023)
  tlb_index / tlb_entry (include/exec/cpu_ldst.h:163/177)
  tlb_hit (include/exec/cpu-all.h:359)
  若主表不匹配 → victim_tlb_hit (cputlb.c:977)
  若victim也不匹配 → tlb_fill (:868)
  → CPUClass.tlb_fill → riscv_cpu_tlb_fill (cpu_helper.c:435)
  → get_physical_address (:155)
  → tlb_set_page (cputlb.c:843) → tlb_set_page_with_attrs (:697)
```

2026-10-24历史取指在1030行比较标签0→1，重复next跳到1039行。[历史Host日志](./historical_20261024/host_gdb.log)证明重复取指跳过填充分支。本轮没有重跑这一取指实验。victim命中后的交换分支只有静态分析，不能由主表miss直接推断查了页表。

## 数据访存路径

```text
翻译/生成阶段（静态分析）：tcg/i386/tcg-target.inc.c
  tcg_out_qemu_ld :2101 → tcg_out_tlb_load :1699
    生成TLB索引计算、读标签、cmp与jne慢路径；匹配后加addend
  tcg_out_qemu_ld_direct :1996 生成实际宿主加载
  不匹配由tcg_out_qemu_ld_slow_path :1810生成helper调用

运行阶段（本轮实际捕获的首次ld）：
  helper_le_ldq_mmu (accel/tcg/cputlb.c:1446附近)
  → load_helper :1248 → 主表比较 :1271
  → victim_tlb_hit :977，扫描8项后 :1010返回false
  → tlb_fill :868 → riscv_cpu_tlb_fill :435
  → get_physical_address :155 → 三次循环
  → tlb_set_page :843 → tlb_set_page_with_attrs :697
  → :835复制缓存项 → load_helper :1338得到haddr → 从RAM读取8字节
```

TCG生成函数不是每次Guest访问时执行的C查询函数：它们先生成宿主机器码，命中通常直接走生成代码。第一次实际停在load_helper，参数code_read=false、size=8，故不能把它写成get_page_addr_code的数据路径。第二次没有逐指令单步JIT；实测结论限定为主表标签匹配且未重新fill，详见[TLB对比](./tlb_comparison.md)。

## 13项关键分支：摘录、作用和执行范围

| 源码位置 | 关键语句摘录 | 作用与本轮状态 |
|---|---|---|
| cpu_helper.c:171、196–199 | `if (mode == PRV_M ...); case VM_1_10_MBARE: *physical=addr;` | 历史M-mode直通；本轮S-mode、SATP=0实际走197–199的Bare返回。不是把M与S的差异全归因于SATP。 |
| :184–191 | `base = get_field(env->satp,SATP_PPN)<<PGSHIFT; levels=3; ptidxbits=9; ptesize=8;` | 实测MODE=8、根PA=0x8020a000，选择Sv39三层。 |
| :224–228 | `if (masked_msbs != 0 && masked_msbs != mask) return TRANSLATE_FAIL;` | 验证VA高位是bit38的符号扩展；本次0x41234560合法。失败分支：源码分析，未单独触发。 |
| :237–242 | `idx=(addr>>(PGSHIFT+ptshift))&((1<<ptidxbits)-1); pte_addr=base+idx*ptesize;` | 三次实测索引1/9/52，地址见遍历表；ptshift每次减9。 |
| :252–254 | `pte=ldq_phys(cs->as,pte_addr); ppn=pte>>PTE_PPN_SHIFT;` | 本轮在读取和PPN赋值后逐次观察PTE；未另外单步进入ldq_phys内部。物理AddressSpace读取见下文。 |
| :256–258 | `if (!(pte & PTE_V)) return TRANSLATE_FAIL;` | 三项均V=1，实际跳过失败返回；无效PTE分支：源码分析，未单独触发。 |
| :259–261 | `else if (!(pte & (PTE_R\|PTE_W\|PTE_X))) base=ppn<<PGSHIFT;` | 前两次实测执行，base更新至0x87ffc000、0x87ffd000，并next回237行。 |
| :262–288 | 保留W组合检查、U/S/SUM检查、`access_type == MMU_DATA_LOAD`的R/MXR检查 | 最后一项U=0、R/W=1、X=0，S-mode读成功。各权限失败与保留编码：源码分析，未单独触发。 |
| :276–278 | `else if (ppn & ((1ULL<<ptshift)-1)) return TRANSLATE_FAIL;` | 对大页要求低PPN位对齐；本轮4KiB叶子ptshift=0，未触发错位。历史根级大页也合法。错位错误：未单独触发。 |
| :291–329 | `updated_pte=pte\|PTE_A\|(...PTE_D...); if(updated_pte!=pte)` | 本次叶子A/D均1，next从295到333，未写回。原子CAS、并发重试、ROM失败均为源码分析，未单独触发。 |
| :333–349 | `*physical=(ppn\|(vpn&((1L<<ptshift)-1)))<<PGSHIFT; return TRANSLATE_SUCCESS;` | next至337后读到0x87ffb000，finish返回0、prot=3；这是页基址，另加0x560得到字节PA。 |
| cpu_helper.c:471–474；cputlb.c:725、804–835 | `tlb_set_page(...); tn.addend=addend-vaddr_page; copy_tlb_helper_locked(te,&tn);` | 本轮实际命中set_page、next越过复制，并读取addr_read/addr_write/addr_code/addend。不是仅打印PASS。 |
| cpu_helper.c:355–380、475–479；op_helper.c:28–34 | `raise_mmu_exception(...); riscv_raise_exception(...); cpu_loop_exit_restore(...)` | 失败依访问类型和PMP结果选择取指/读/写page fault或access fault；probe=true可直接返回false。本轮没有触发这些异常，属源码分析。 |

## PTE的物理读取与宿主内存

`ldq_phys(cs->as,pte_addr)`接收Guest物理地址，走QEMU物理AddressSpace/MemoryRegion解析，不会把PTE地址再次当成Guest VA查同一页表。`memory_ldst.inc.c:99–142`的64位读取在RAM分支用`qemu_map_ram_ptr(mr->ram_block,addr1)`得到宿主指针；MMIO则调用设备回调。本轮只记录了读取后的PTE，没有把这些内部函数都宣称为GDB已命中。

数据填充在`cputlb.c:725`经`address_space_translate_for_iotlb()`查到物理区域，RAM分支在748行取得宿主页指针，804行保存相对Guest虚拟页的addend。之后`haddr=addr+entry->addend`用于宿主加载；[result.json](./result.json)中host_ram_read记录了实际HVA及其8字节值。HVA受宿主ASLR影响，不是Guest PA；`ram_addr_t`是QEMU RAM块管理地址/偏移，也不是Guest物理地址的别名。
