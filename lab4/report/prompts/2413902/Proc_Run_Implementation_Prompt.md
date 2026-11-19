[PROMPT]

任务：
在 Lab4 的 kern/process/proc.c 中实现 proc_run()，完成当前线程指针、Sv39 页表和执行上下文的切换。

操作要求：
先阅读 proc.c、proc.h、kern/process/switch.S、kern/sync/sync.h、libs/riscv.h，以及 pmm.c 中的 enable_paging()，再修改实际文件。

只在“LAB4 function3 YOUR CODE”对应区域实现本人负责的 proc_run()，保留其他成员的代码和标记。本人实现部分的 YOUR CODE 使用我提供的学号；未提供时先保留。

不要实现或修改 schedule()、switch_to()、do_fork() 和线程初始化函数，不改 struct context、struct trapframe 或公共接口。

输出要求：
给出修改内容，解释 current、satp、上下文和中断状态的切换关系，并记录实际编译和联调结果。如果 switch_to() 或线程初始化尚未完成，明确指出依赖，不替其他成员补写。

[RELY]

proc.h 中与本任务相关的字段如下，仅用于理解已有结构，不重新定义：

struct proc_struct：
- state：线程状态；
- pgdir：页表根物理地址，具体赋值需核对初始化代码；
- context：线程切换上下文。

struct context 的字段顺序为：
ra、sp、s0、s1、s2、s3、s4、s5、s6、s7、
s8、s9、s10、s11；
字段类型均为 uintptr_t。

已有全局变量：
extern struct proc_struct *idleproc, *initproc, *current;
extern uintptr_t boot_pgdir_pa;

已有切换接口：
void switch_to(struct context *from, struct context *to);

switch_to() 由 C 成员实现，负责保存旧线程的 ra、sp、s0～s11，并恢复目标线程对应寄存器，不负责选择线程或切换页表。

已有中断保护接口：
local_intr_save(flag);
local_intr_restore(flag);

它们通过 bool 类型的 flag 保存进入前的 SIE 状态，必要时关闭中断，并在恢复时依据旧状态处理，不是无条件开中断。

已有分页接口和常量：
- write_csr(satp, value)
- flush_tlb()
- RISCV_PGSHIFT = 12
- SATP_MODE_SV39 = 8

SATP_MODE_SV39 是模式编号，尚未移入 RV64 satp 的 MODE 字段，不能直接与物理页号按位或。

课程骨架 enable_paging() 使用的编码参照为：
write_csr(satp, 0x8000000000000000 |
                (boot_pgdir_pa >> RISCV_PGSHIFT));

libs/riscv.h 中的 lsatp() 使用 Sv32 编码，不能用于本实验。

本阶段内核线程共享内核地址空间。页表物理地址的来源，应核对当前工程的 proc->pgdir 和 boot_pgdir_pa 约定，不能把页表虚拟地址直接写入 satp。

[GUARANTEE]

必须实现以下接口，签名保持不变：

void proc_run(struct proc_struct *proc);

复用现有中断、分页和 switch_to() 接口，不新增调度策略或公开函数。

[SPECIFICATION]

proc_run

Pre-Condition：
current 指向有效的当前线程。proc 是调度器选出的可运行线程，具有有效页表和已准备好的 context；目标内核栈、切换代码及相关内核数据在目标页表中可访问。

Post-Condition：
需要切换时，目标线程成为 current，CPU 使用目标线程对应的地址空间和执行上下文。旧线程的上下文已保存，之后可被再次调度恢复。

Case 1：目标线程就是 current。
避免无意义的页表和上下文切换，保持当前状态不变。

Case 2：切换到另一个已经运行过的线程。
保护切换过程，保存旧线程上下文，更新 current 和页表，恢复目标上下文，使目标线程从上次暂停的位置继续执行。

Case 3：第一次切换到新线程。
使用初始化阶段准备好的 context，由 switch_to() 进入 forkret 后续恢复链路。本函数不重新构造 trapframe，也不直接调用线程目标函数。

Case 4：旧线程以后再次被调度回来。
它会从此前的 switch_to() 调用后继续执行，并恢复这次切换前保存的中断状态。不能假设调用 switch_to() 后会立即在原线程中返回。

Requirements：
切换关键区应避免本核中断造成 current、satp 与实际执行现场不一致。先保留旧线程指针，再更新 current；按 Sv39 正确设置页表并刷新 TLB，之后进行寄存器上下文切换。

中断状态按进入前的状态恢复，不无条件开启。不得在本函数中选择下一个线程、分配资源或修改进程链表。
