[PROMPT]

任务：
在 Lab4 的 kern/mm/pmm.c 中实现 page_insert() 和 page_remove()，完成虚拟地址与物理页之间的映射维护，保证页表项、物理页引用计数和 TLB 保持一致。

操作要求：
先阅读当前工程的 pmm.c、pmm.h、mmu.h 和相关检查函数，再修改实际文件，只在“LAB4 function1 YOUR CODE”对应区域补全代码。保留已有逻辑、测试和注释，将本人实现部分的 YOUR CODE 替换为我提供的学号；未提供学号时先保留，不要猜测。

不要修改其他成员负责的函数，也不要整体覆盖文件。如果发现已有接口存在影响实现的问题，先说明具体位置和影响。

输出要求：
给出修改内容、关键设计和实际检查结果。使用项目已有接口，不引入无关功能。按现有 Makefile 尝试编译，并检查新增映射、同页重映射、异页替换和重复删除等情况。受其他模块缺失影响而无法完成的测试，要明确说明。

[RELY]

目标环境为 RISC-V RV64、Sv39，基本页大小为 4096 字节。

框架已有接口：
- get_pte(pde_t *pgdir, uintptr_t la, bool create)
  返回末级 PTE 的内核虚拟地址；create 为真时允许分配中间页表，为假时只查找，无法取得时返回 NULL。
- pte2page()：从有效 PTE 得到物理页描述符。
- page2ppn()：取得物理页号。
- page_ref_inc()、page_ref_dec()：调整引用计数，返回调整后的值。
- free_page()：释放一个物理页。
- tlb_invalidate(pgdir, la)：使对应地址的 TLB 翻译失效。

pmm.h 中已有的 PTE 构造函数：
static inline pte_t pte_create(uintptr_t ppn, int type)
{
    return (ppn << PTE_PPN_SHIFT) | PTE_V | type;
}

相关宏：
#define PTE_PPN_SHIFT 10
#define PTE_V 0x001
#define PTE_R 0x002
#define PTE_W 0x004
#define PTE_X 0x008
#define PTE_U 0x010

struct Page 的 ref 字段记录物理页引用计数，通过现有接口操作，不改变结构体。

框架已有 check_pgdir() 和 check_boot_pgdir()，用于检查映射、权限、引用计数和地址访问。

课程原始骨架未提供 page_remove_pte() 的实现；如需统一删除逻辑，可在本模块内增加 static 辅助函数。若团队当前版本已经实现了该函数，先检查其副作用，再决定是否复用。

以上信息需与当前项目核对，不要重新定义已有宏和接口。

[GUARANTEE]

必须实现以下接口，签名保持不变：

int page_insert(pde_t *pgdir, struct Page *page,
                uintptr_t la, uint32_t perm);

void page_remove(pde_t *pgdir, uintptr_t la);

允许增加必要的 static 删除辅助函数，不修改公开接口或已有检查函数。

[SPECIFICATION]

page_insert

Pre-Condition：
pgdir 指向有效的 Sv39 根页表，page 是可用于映射的物理页描述符，物理页分配器已初始化。la 和 perm 遵循工程现有接口约定。

Post-Condition：
成功返回 0，la 所在虚拟页映射到 page，权限按 perm 和框架编码约定设置，相关引用计数与映射一致，后续访问不会继续使用旧的 TLB 翻译。

Case 1：原地址没有有效映射。
建立新映射，新物理页的引用计数增加一次，不影响其他地址的映射。

Case 2：原地址已经映射到同一个物理页。
更新权限，物理页引用计数净变化为零，不能因先删除而错误释放仍需使用的页面。

Case 3：原地址映射到另一个物理页。
撤销该地址对旧页的引用，引用归零时按工程约定释放旧页；建立新映射并增加新页引用，不影响旧页在其他地址上的映射。

Case 4：无法取得所需页表项。
返回 -E_NO_MEM，不改变原有有效映射，也不增加目标物理页引用。若已有 get_pte() 在多级分配失败时遗留中间页表页，应说明原因和接口修改需求，不擅自重写它。

Requirements：
映射建立可能失败的步骤应在破坏旧映射之前完成。遵循现有地址索引和权限约定，不擅自增加与框架不同的地址对齐限制。引用计数、释放和 TLB 操作不得因复用辅助函数而重复执行。

page_remove

Pre-Condition：
pgdir 指向有效根页表，物理页和页表管理接口可用。

Post-Condition：
la 所在虚拟页不再保留原有效映射；对应物理页的引用计数、释放状态和 TLB 翻译保持一致。

Case 1：存在有效映射。
撤销映射，物理页引用计数减少一次，归零时按工程约定释放，并使旧 TLB 翻译失效。

Case 2：不存在页表或有效映射。
直接返回，不分配页表，不修改引用计数。

Requirements：
重复删除同一地址应安全。不要删除其他映射，也不要自行增加中间页表回收策略。
