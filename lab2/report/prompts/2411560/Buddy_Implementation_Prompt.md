[PROMPT]

**任务**：在当前 RISC-V uCore Lab2 工程中，从零实现 Buddy System（伙伴系统）连续物理页分配器，使其能够通过现有 `pmm_manager` 接口完成可用物理页的初始化、管理、分配、释放和伙伴合并，并接入内核实际运行。

**目标工程**：

- 已完成前置任务的工程：`D:\college_study_materials\junior_year_fall\OS\lab2\code`
- Buddy 独立工作区：`D:\college_study_materials\junior_year_fall\OS\lab2\buddy_work`

**目标文件**：

- 新建 `kern/mm/buddy_pmm.c`：Buddy 分配算法及模块内部状态。
- 新建 `kern/mm/buddy_pmm.h`：Buddy 管理器对外声明。
- 修改独立工作区中的 `kern/mm/pmm.c`：注册并选择 Buddy 作为当前物理内存管理器。

**操作要求**：

1. 必须在实际工程文件中完成代码修改，而不仅是输出示例代码。
2. 先核对最新源码。当前最新版本应当已经实现 `page_init()`、`best_fit_pmm_manager`，并且 `pmm.c` 当前选择 Best-Fit。若 `buddy_work` 仍是早期副本，应先检查差异，再以最新已完成的源码为基础安全同步，不覆盖已有用户文件或 Buddy 研发成果。
3. 所有 Buddy 开发仅在 `buddy_work` 中进行，不修改共享的 `code`、Lab1 或队友已完成的实现。
4. 保留原有 First-Fit、Best-Fit、`page_init()` 和官方测试逻辑，不得删除、跳过或放宽现有检查。
5. 允许新增必要的模块内部辅助函数、数据结构、静态变量和状态标记。具体内部实现方案由你自主设计。
6. 不依赖当前内核尚未提供的 `malloc()` 或 `kmalloc()`，也不得在物理页管理器初始化期间依赖尚未初始化的自身分配服务。
7. 本轮完成 Buddy 的实现、注册、编译及基础运行验证。详细的专项测试函数将在第二份 Prompt 中完成。

**输出要求**：

按照下面的 `[RELY]`、`[GUARANTEE]` 和 `[SPECIFICATION]` 实现完整模块。所有宏、类型和函数以当前实际源码为准，不自行假设其他 uCore 版本的接口。

完成后提供实际修改文件、核心设计说明、编译与运行结果、错误及修复记录。

---

[RELY]

以下是当前 Lab2 工程已经提供的接口和可依赖资源。

### 1. 物理内存管理器接口

来源：`kern/mm/pmm.h`

```c
struct pmm_manager {
    const char *name;
    void (*init)(void);
    void (*init_memmap)(struct Page *base, size_t n);
    struct Page *(*alloc_pages)(size_t n);
    void (*free_pages)(struct Page *base, size_t n);
    size_t (*nr_free_pages)(void);
    void (*check)(void);
};
```

内核现有公共接口：

```c
struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void);
```

当前框架通过 `pmm_manager` 的函数指针调用具体分配器。

### 2. 物理页描述符

来源：`kern/mm/memlayout.h`

```c
struct Page {
    int ref;
    uint64_t flags;
    unsigned int property;
    list_entry_t page_link;
};
```

现有页面状态约定：

- `PG_reserved = 0`：保留页状态位。
- `PG_property = 1`：空闲内存块头部标记。
- `PageReserved(page)`：判断页面是否保留。
- `SetPageReserved(page)` / `ClearPageReserved(page)`：修改保留状态。
- `PageProperty(page)`：判断页面是否为空闲块头部。
- `SetPageProperty(page)` / `ClearPageProperty(page)`：修改空闲块头部状态。
- `property`：现有分配器在空闲块头部记录连续页数。
- `page_link`：描述符内部已有的双向链表节点。

Buddy 可以使用兼容的私有管理状态，但不得混淆已有 Reserved 和 Property 标志的含义。若复用 `property` 或其他字段存储 Buddy 元数据，须明确不同状态下的语义，并保证不会破坏公共接口。

### 3. 地址及页面转换

来源：`kern/mm/mmu.h`、`kern/mm/pmm.h`

```c
#define PGSIZE 4096
#define PGSHIFT 12
#define PPN(la) (((uintptr_t)(la)) >> PGSHIFT)
```

现有转换关系：

```c
static inline ppn_t page2ppn(struct Page *page) {
    return page - pages + nbase;
}

static inline uintptr_t page2pa(struct Page *page) {
    return page2ppn(page) << PGSHIFT;
}
```

其中：

- `pages` 为 Page 描述符数组首地址。
- `nbase` 为 DRAM 起始物理页号。
- `npage` 为可管理物理页号的排他上界。
- 物理页号区间为 `[nbase, npage)`。
- 相邻 `Page` 描述符相差 `sizeof(struct Page)` 字节，但相邻物理页的起始物理地址相差 `PGSIZE` 字节。

伙伴关系必须根据物理页号及块大小确定，不能直接对 `struct Page *` 地址进行物理伙伴计算。

当前 `pa2page()` 只检查页号上界，调用前仍须保证物理页号不小于 `nbase`，避免越界访问。

### 4. 可用的链表结构

来源：`libs/list.h`

```c
typedef struct list_entry list_entry_t;
```

可使用的已有接口包括：

```c
list_init(...)
list_add(...)
list_add_before(...)
list_add_after(...)
list_del(...)
list_del_init(...)
list_empty(...)
list_next(...)
list_prev(...)
```

现有 `le2page(le, page_link)` 可以由链表节点获得所属的 Page 描述符。

这些接口可以用于实现 Buddy 空闲块管理，但不强制采用某种具体链表组织方案。

### 5. 已有物理内存初始化功能

来源：`kern/mm/pmm.c`

当前 `page_init()` 已经实现，负责：

- 根据 DTB 确定物理内存范围。
- 设置 `va_pa_offset`、`npage` 和 `pages`。
- 保留内核及 Page 描述符数组占用空间。
- 将剩余完整空闲物理页交给当前 `pmm_manager` 的 `init_memmap()`。

Buddy 不需要重新实现物理内存探测或 Sv39 启动页表。

`init_memmap(base, n)` 的输入是连续可用物理页的描述符区间，不保证起始物理页号满足大块对齐要求，也不保证 `n` 为 2 的幂。

当前框架使用 4 KiB 页面，DRAM 上界及内核映射约束以 `memlayout.h` 和实际初始化结果为准。

### 6. 已有分配器

参考：

- `kern/mm/default_pmm.c`
- `kern/mm/best_fit_pmm.c`

这两个文件提供了当前 uCore 分配器的初始化、分配、释放和状态管理示例。

Buddy 应遵守相同的公共管理器接口，但不需要模仿 First-Fit 或 Best-Fit 的内部选块顺序、空闲块表示和私有测试假设。

---

[GUARANTEE]

必须新增并注册以下 Buddy System 模块接口：

```c
static void buddy_init(void);

static void buddy_init_memmap(
    struct Page *base,
    size_t n
);

static struct Page *buddy_alloc_pages(
    size_t n
);

static void buddy_free_pages(
    struct Page *base,
    size_t n
);

static size_t buddy_nr_free_pages(void);

static void buddy_check(void);
```

必须定义管理器：

```c
const struct pmm_manager buddy_pmm_manager;
```

并在 `buddy_pmm.h` 中声明：

```c
extern const struct pmm_manager buddy_pmm_manager;
```

以上函数名称作为本模块的统一命名约定。允许增加 `static` 辅助函数、内部宏、空闲块管理结构及必要的分配记录。

必须在独立工作区的 `init_pmm_manager()` 中完成 Buddy 注册，使内核能够实际选择 `buddy_pmm_manager`，并执行其 `init()` 和 `check()`。

不得修改 `struct pmm_manager` 的公共函数签名。

`buddy_check()` 在本轮可以先实现基础分配、释放与状态恢复检查，但必须实际执行检查，不能使用空函数或仅打印成功信息代替。第二份 Prompt 将针对实际 Buddy 实现扩展完整测试。

---

[SPECIFICATION]

## 1. buddy_init(void)

**Pre-Condition**：

- 内核进入物理内存管理器初始化阶段。
- Page 描述符和可分配页面尚未通过本管理器登记。
- 不存在需要保留的 Buddy 活动分配。

**Post-Condition**：

- Buddy 内部管理状态正确初始化。
- 空闲块管理结构有效。
- 初始空闲页数量为 0。
- 后续能够正常调用 `buddy_init_memmap()`。

**Requirements**：

- 不依赖未初始化的动态内存分配服务。
- 不访问尚未登记的物理页。
- 不破坏内核已有保留内存。

## 2. buddy_init_memmap(struct Page *base, size_t n)

**Pre-Condition**：

- Buddy 管理器已初始化。
- `base` 指向合法 Page 描述符。
- `n > 0`。
- `[base, base+n)` 对应一个完整、连续、此前未登记的可用物理页区间。
- 输入范围位于可管理物理内存之内，不发生越界或溢出。

**Post-Condition**：

- 全部 `n` 个合法物理页被纳入 Buddy 管理。
- 所有原生 Buddy 空闲块大小均为 `2^k` 页，并满足相应的物理页号对齐要求。
- 不遗漏页面、不重复登记、不包含输入范围之外的页面。
- 空闲页数量准确增加 `n`。

**Case 1：输入长度为 2 的幂且起点合法对齐**

- 能够正确登记全部页面。
- 登记后的空闲块满足 Buddy 大小及对齐不变量。

**Case 2：输入长度不是 2 的幂**

- 仍应完整管理所有合法空闲页面。
- 可以将区间组织成多个不同阶数的 Buddy 块。
- 不得为了凑齐 2 的幂而舍弃尾部页面或增加越界页面。

**Case 3：输入起始页号不满足大块对齐**

- 应根据实际物理页号建立合法的空闲块。
- 不得假设任意输入起点都能直接作为最大阶块的起点。

**Case 4：多次登记不同空闲区间**

- 后续登记不得重置此前已经登记的资源。
- 不同区间的所有权和空闲页计数必须保持正确。
- 相邻合法区间可以在满足完整伙伴条件时合并，但不能跨越未登记区域或 Reserved 页面合并。

**Requirements**：

- 对重叠、重复或越界登记应明确拒绝，不能重复计算空闲页。
- 只将本次确认可用的页面转换为可分配状态。
- 不得解除输入区间之外页面的保留状态。

## 3. buddy_alloc_pages(size_t n)

**Pre-Condition**：

- Buddy 管理器已经初始化。
- 已有合法的可用物理页登记。
- `n > 0`，且容量计算不会发生整数溢出。

**Post-Condition**：

- 成功时返回一段至少包含 `n` 个连续物理页的有效首描述符。
- 实际独占块大小为不小于 `n` 的最小 2 的幂。
- 分配块满足相应的物理页号对齐要求。
- 已分配页面不会被其他活动请求再次分配。
- 空闲页数按实际占用容量减少。

**Case 1：存在满足请求的空闲块**

- 成功返回合法的 Page 描述符指针。
- 分配后的状态和统计与实际占用的页面一致。

**Case 2：只有更大的空闲块**

- 能够通过合法拆分获得满足请求的块。
- 拆分产生的其他空闲部分仍可被后续分配。
- 拆分不得造成页面丢失、重复登记或越界。

**Case 3：请求页数不是 2 的幂**

- 采用向上取整至 2 的幂的容量策略。
- 例如请求 3 页时，实际独占 4 页。
- 实际独占的额外页面在释放之前不能再分配给其他请求。
- 必须保留足够的分配记录，以供 `free_pages(base,n)` 正确回收。

**Case 4：没有足够的合法空闲块**

- 返回 `NULL`。
- 不改变已有活动分配的所有权。
- 不得泄漏页面、破坏空闲状态或造成计数漂移。

**Requirements**：

- 不得分配 Reserved 页面。
- 不得返回重叠的活动分配块。
- 对零页请求按框架约定进行非法参数诊断。
- 对超过支持容量或可能造成溢出的请求安全处理。
- 不强制要求返回最低物理地址或遵循 First-Fit、Best-Fit 的选块顺序。

## 4. buddy_free_pages(struct Page *base, size_t n)

本次实现采用完整配对释放契约。

调用者使用当初 `alloc_pages(n)` 返回的 `base`，并传入原始申请页数 `n`，分配器应归还该次实际独占的完整 Buddy 块。

**Pre-Condition**：

- `base` 是仍然有效的分配记录的起始 Page 描述符。
- `n` 与原始请求页数一致。
- 对应分配尚未释放。

**Post-Condition**：

- 此次实际独占的所有物理页都被正确回收。
- 空闲页数量按实际回收容量增加。
- 原有的其他活动分配不受影响。
- 释放后的空闲块符合 Buddy 大小及对齐要求。
- 合法伙伴能够继续合并为更大的空闲块。

**Case 1：伙伴尚未空闲**

- 正确释放当前块。
- 不合并仍被占用的伙伴。

**Case 2：存在同阶空闲伙伴**

- 能够识别并合并合法伙伴。
- 合并后不留下重复的空闲块记录。

**Case 3：可以多级合并**

- 支持逐级合并。
- 当原有大块中的全部子块都释放且满足伙伴条件时，可以恢复原有大块。

**Case 4：原始请求不是 2 的幂**

- 根据原始分配记录恢复实际占用容量。
- 例如 `alloc_pages(3)` 后执行 `free_pages(base,3)`，应归还完整的 4 页块。
- 不得仅归还 3 页而造成剩余页面泄漏。

**Case 5：非法释放**

包括重复释放、错误起点、错误释放长度、Reserved 页或未登记页。

- 应明确诊断非法操作。
- 不得静默释放其他活动分配拥有的页面。
- 不得重复增加空闲页数。

**Requirements**：

- 第一版只要求支持与原分配对应的完整释放。
- 不要求支持从一个活动分配块中间释放部分页面。
- 对不支持的部分释放必须明确拒绝，不能错误地回收整个分配块。
- 不能仅根据物理地址相邻就合并，必须满足完整的合法伙伴条件。

## 5. buddy_nr_free_pages(void)

**Pre-Condition**：

- Buddy 管理器已经初始化。

**Post-Condition**：

- 返回当前真正空闲、可以再次用于分配的物理页总数。
- 返回值的单位是页，而不是字节、空闲块数量或 order。

**Case 1：刚初始化且尚未登记空闲页**

- 返回 0。

**Case 2：登记新的可用页**

- 返回值准确增加登记的合法页数。

**Case 3：成功分配**

- 返回值减少实际独占的页面数量。

**Case 4：成功释放**

- 返回值增加实际归还的页面数量。

**Case 5：仅执行空闲块拆分或合并**

- 单纯的内部结构变化不应改变空闲物理页总数。

## 6. buddy_check(void)

**Pre-Condition**：

- Buddy 管理器已完成初始化并拥有可用物理页。
- 测试使用当前真实的 Buddy 分配器接口。

**Post-Condition**：

- 实际执行基础的分配、释放与统计检查。
- 测试正常结束时，不留下临时活动分配或永久修改正常内核所需的管理状态。
- 检查失败时通过断言或明确错误信息报告，不得无条件宣称通过。

**Case 1：基本分配与释放**

- 验证能够申请和释放合法的物理页。
- 验证返回的 Page 描述符有效。

**Case 2：空闲页计数**

- 验证分配后计数减少、释放后计数恢复。
- 特别检查非 2 的幂请求的实际独占容量。

**Case 3：状态恢复**

- 基础测试结束后，原有可用物理页总数和活动分配所有权保持正确。

**Requirements**：

- 不调用与 Buddy 私有状态不兼容的 First-Fit 或 Best-Fit 专项测试。
- 不通过随意重置正在使用的全局空闲链表来构造测试场景。
- 第二份正式 Prompt 将在此基础上扩展完整的拆分、伙伴合并、非法输入及边界测试。

## 7. 模块级正确性要求

整个 Buddy 分配器必须始终满足以下不变量：

1. 原生空闲块大小为 2 的幂，起始物理页号满足对应对齐约束。
2. 空闲块之间不能重叠。
3. 已分配块之间不能重叠。
4. 空闲块不能包含正在被使用或保留的物理页。
5. 任意物理页不能同时被多个活动分配拥有。
6. 只有同阶、完整空闲、地址关系合法、合并范围完全有效的伙伴才能合并。
7. 实际空闲页数必须与内部管理状态一致。
8. 分配与释放成对完成后，不得产生永久页面泄漏。
9. 不依赖尚未初始化的动态内存分配服务。
10. 不要求多核并发安全；当前目标是课程现有单 Hart、串行调用环境。

## 8. 编译、运行和交付要求

完成模块后，在 `buddy_work` 中进行真实验证。

首先编译 Buddy 模块，确认其与当前 `pmm_manager`、`struct Page` 和工具链兼容。

随后尝试完整工程编译和 QEMU 启动，确认内核实际选择 Buddy：

```text
memory management: buddy_pmm_manager
```

运行成功后，应观察到 Buddy 的实际初始化和基础检查结果，不能只依靠打印固定成功信息判断正确性。

如果 QEMU 正常进入内核无限循环，应使用合理的受控超时或手动退出方式，区分正常驻留与崩溃。

注意：当前官方 Best-Fit 评分测试会检查管理器是否为 `best_fit_pmm_manager`，因此 Buddy 测试应与 Best-Fit 专项评分分开。不得修改、删除或欺骗官方测试。应保留能够恢复 Best-Fit 作为当前管理器的方式。

保存：

- 所有实际修改的源文件。
- Buddy 的内部管理结构说明。
- 各函数与 SPEC 的对应关系。
- 实际执行的编译及 QEMU 命令。
- 原始终端输出和退出状态。
- 出现的编译或运行错误及真实修复过程。
- 尚未通过或尚未执行的测试项目。

完成后输出 STATUS（SUCCESS / PARTIAL / FAILED），列出修改文件、实际验证结果和未完成项。

本轮只实现 Buddy System 及其基础检查，不提前完成第二份 Prompt 所要求的完整专项测试，也不开发 SLUB。