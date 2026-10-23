# Lab2 AI Prompt 原始归档

本文件按任务归档实验中使用的提示词，各部分署名单独列出。

- 2412565—张宸笛号：功能模块一·任务一（物理内存探测与页描述符初始化）、功能模块二·任务二（Best-Fit 连续物理内存分配算法）。
- 2411560—朱泽帅：Buddy 实现与测试、SLUB 实现与测试，原有四份归档保留在后文。

以下任务一、任务二根据提供的提示词整理，分别收录准备 `[RELY]` 的提示词和完整实现提示词。原文中的工程路径与源码状态描述保留其使用时的上下文。

## 功能模块一：物理内存初始化

### 任务一：实现物理内存探测与页描述符初始化功能

负责人：2412565—张宸笛号。

#### 1. 准备 [RELY] 的提示词

我正在完成 OSLab 任务一：实现物理内存探测与页描述符初始化功能。

目标函数：

```
static void page_init(void);
```

目标文件：
`kern/mm/pmm.c`

请你阅读当前项目的真实源码，帮助我编写符合实验要求的四段式提示词中的 [RELY] 部分。

**本阶段只允许读取和分析源码，禁止修改任何文件，也不要实现 page_init()。**

##### 任务说明

`page_init()` 需要根据设备树提供的物理内存信息，建立可管理物理页的描述符布局，排除 OpenSBI、内核镜像和页描述符数组占用空间，再将完整空闲页面交给当前物理内存管理器管理。

它应正确设置 `va_pa_offset`、`npage`、`pages`，输出物理内存探测范围，并完成初始空闲区注册。

##### 源码检查要求

请从 `kern/mm/pmm.c` 开始，沿着 include、函数声明和函数调用关系，查找相关定义。

重点分析：

1. `page_init()` 和 `pmm_init()` 的定义及调用关系。
2. `get_memory_base()`、`get_memory_size()` 的声明、实现及返回值语义。
3. `va_pa_offset`、`npage`、`pages` 的声明、类型和使用方式。
4. `struct Page` 的完整定义、页面状态相关宏及其语义。
5. `pa2page()` 如何将物理地址转换为 Page 描述符地址。
6. `init_memmap()` 的实际声明、调用方式，以及 `pmm_manager` 的初始化时序。
7. 物理内存上下界、地址转换和页面对齐相关宏定义。
8. 链接符号 `end` 的声明、地址类型及其与内核镜像的关系。
9. OpenSBI 和内核镜像的保留范围在当前项目中的确定方式。

##### 输出格式

请首先输出一份可以直接放入实验提示词的 [RELY]：

[RELY]

###### 1. 相关宏定义

复制项目中实际存在的、与本任务直接相关的宏定义。

###### 2. 数据结构

复制 `struct Page` 及必要的相关数据结构定义。

###### 3. 全局变量和链接符号

复制相关变量的真实声明或定义，并说明其含义。

###### 4. 可用函数与接口

复制相关函数的真实签名，并简要说明其参数、返回值和用途。

###### 5. 关键内存布局约定

说明物理地址、虚拟地址、物理页号与 Page 描述符之间的转换关系，以及相关物理内存边界约定。

##### 额外要求

- 所有代码必须从真实工程文件中提取，不能凭经验编造。
- 每组定义都需要注明来源文件及行号。
- 不要把无关头文件完整复制出来，只保留当前任务所需的最小上下文。
- 如果宏或函数依赖其他重要定义，应继续查找其依赖。
- 区分源码明确规定的事实和你根据源码推导出的结论。
- 对无法确认的内容明确标注“待确认”，不要猜测。
- 最后单独列出可能影响 page_init() 实现的关键注意事项。

本轮只完成源码分析和 [RELY] 整理，不进行任何代码修改。

#### 2. 完整实现提示词

[PROMPT]

**任务**：修改 `kern/mm/pmm.c` 文件，实现 `page_init()` 函数，完成物理内存探测与物理页描述符初始化。

根据设备树提供的物理内存信息，确定内核可管理的物理内存范围，建立物理页与 `Page` 描述符之间的对应关系，排除 OpenSBI、内核镜像以及页描述符数组等已占用区域，并将剩余完整空闲物理页注册到当前物理内存管理器，为后续物理页分配提供正确的初始状态。

**操作要求**：

1. 必须直接修改项目中的 `kern/mm/pmm.c`，而不是仅展示代码。
2. 保留已有代码结构、函数签名及其他功能，只修改完成本任务所必需的代码。
3. 优先使用项目已有的宏定义、数据结构和辅助函数，不得自行假设不存在的接口。
4. 不得引入与当前任务无关的功能，不得修改现有公开接口。

**输出要求**：根据下面的 [RELY]、[GUARANTEE] 和 [SPECIFICATION] 完成实现。修改后检查代码是否满足行为规格，并报告实际完成的验证。

[RELY]

以下代码片段均摘自当前工程。标为“源码事实”的内容有直接定义或调用依据；标为“推导”的内容由这些定义计算得到。

###### 1. 相关宏定义

页大小、物理页号和对齐操作分别定义在 [mmu.h (line 9)](/home/iodwad/OS/lab2/kern/mm/mmu.h:9) 与 [defs.h (line 54)](/home/iodwad/OS/lab2/libs/defs.h:54)：

```
#define PGSIZE          4096
#define PGSHIFT         12
#define PPN(la) (((uintptr_t)(la)) >> PGSHIFT)

#define ROUNDDOWN(a, n) ({                                          \
            size_t __a = (size_t)(a);                               \
            (typeof(a))(__a - __a % (n));                           \
        })

#define ROUNDUP(a, n) ({                                            \
            size_t __n = (size_t)(n);                               \
            (typeof(a))(ROUNDDOWN((size_t)(a) + __n - 1, __n));     \
        })
```

内存边界与映射常量摘自 [memlayout.h (line 5)](/home/iodwad/OS/lab2/kern/mm/memlayout.h:5)，DRAM 起点摘自 [riscv.h (line 152)](/home/iodwad/OS/lab2/libs/riscv.h:152)：

```
#define KERNBASE            0xFFFFFFFFC0200000
#define KMEMSIZE            0x7E00000
#define KERNTOP             (KERNBASE + KMEMSIZE)
#define PHYSICAL_MEMORY_END         0x88000000
#define PHYSICAL_MEMORY_OFFSET      0xFFFFFFFF40000000
#define KERNEL_BEGIN_PADDR          0x80200000
#define KERNEL_BEGIN_VADDR          0xFFFFFFFFC0200000

#define DRAM_BASE          0x80000000
```

页面状态宏摘自 [memlayout.h (line 41)](/home/iodwad/OS/lab2/kern/mm/memlayout.h:41)：

```
#define PG_reserved                 0
#define PG_property                 1

#define SetPageReserved(page)       ((page)->flags |= (1UL << PG_reserved))
#define ClearPageReserved(page)     ((page)->flags &= ~(1UL << PG_reserved))
#define PageReserved(page)          (((page)->flags >> PG_reserved) & 1)
#define SetPageProperty(page)       ((page)->flags |= (1UL << PG_property))
#define ClearPageProperty(page)     ((page)->flags &= ~(1UL << PG_property))
#define PageProperty(page)          (((page)->flags >> PG_property) & 1)
```

**源码事实：**`PG_reserved` 表示不能交给页分配器的保留页；`PG_property` 表示空闲连续块的首个描述符，其 `property` 保存该块页数。语义来自 [memlayout.h (line 34)](/home/iodwad/OS/lab2/kern/mm/memlayout.h:34) 的字段及宏注释，并与 [default_pmm.c (line 18)](/home/iodwad/OS/lab2/kern/mm/default_pmm.c:18) 的实现一致。

###### 2. 数据结构

`Page` 的完整定义摘自 [memlayout.h (line 34)](/home/iodwad/OS/lab2/kern/mm/memlayout.h:34)；其中的链表类型摘自 [list.h (line 17)](/home/iodwad/OS/lab2/libs/list.h:17)：

```
struct list_entry {
    struct list_entry *prev, *next;
};
typedef struct list_entry list_entry_t;

struct Page {
    int ref;
    uint64_t flags;
    unsigned int property;
    list_entry_t page_link;
};
```

物理内存管理器接口摘自 [pmm.h (line 15)](/home/iodwad/OS/lab2/kern/mm/pmm.h:15)：

```
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

相关类型定义摘自 [defs.h (line 22)](/home/iodwad/OS/lab2/libs/defs.h:22)：

```
typedef unsigned long long uint64_t;
typedef uint64_t uintptr_t;
typedef uintptr_t size_t;
typedef size_t ppn_t;
```

###### 3. 全局变量和链接符号

实际定义摘自 [pmm.c (line 14)](/home/iodwad/OS/lab2/kern/mm/pmm.c:14)，对应的外部声明见 [pmm.h (line 78)](/home/iodwad/OS/lab2/kern/mm/pmm.h:78)：

```
struct Page *pages;
size_t npage = 0;
uint64_t va_pa_offset;
const size_t nbase = DRAM_BASE / PGSIZE;
const struct pmm_manager *pmm_manager;
```

**源码事实：**`pages` 是描述符数组的内核虚拟地址；`nbase` 是 DRAM 起点的物理页号。由 `page2ppn()` 和 `pa2page()` 可知，`npage` 用作**物理页号上界**，不是 `pages` 数组的元素数；当内存从 `DRAM_BASE` 连续延伸至该上界时，数组元素数为 `npage - nbase`。`va_pa_offset` 参与虚实地址换算。[pmm.c (line 14)](/home/iodwad/OS/lab2/kern/mm/pmm.c:14)、[pmm.h (line 83)](/home/iodwad/OS/lab2/kern/mm/pmm.h:83)

链接脚本在 [kernel.ld (line 7)](/home/iodwad/OS/lab2/tools/kernel.ld:7) 将内核起始虚拟地址设为 `0xFFFFFFFFC0200000`，并在 `.bss` 之后以 `PROVIDE(end = .);` 定义 `end`。[init.c (line 18)](/home/iodwad/OS/lab2/kern/init/init.c:18) 和 [test_lab2.c (line 14)](/home/iodwad/OS/lab2/tools/test_lab2.c:14) 均以如下形式声明它：

```
extern char end[];
```

**源码事实：**`end` 是内核镜像末尾的**虚拟地址符号**，不是保存地址值的变量；内核已经用它确定需清零的 `.bss` 范围。[init.c (line 28)](/home/iodwad/OS/lab2/kern/init/init.c:28)

###### 4. 可用函数与接口

设备树接口的真实声明见 [dtb.h (line 10)](/home/iodwad/OS/lab2/kern/driver/dtb.h:10)：

```
void dtb_init(void);
uint64_t get_memory_base(void);
uint64_t get_memory_size(void);
```

**源码事实：\**两个 getter 直接返回 `dtb.c` 中初始为零的 `memory_base`、`memory_size`；`dtb_init()` 成功解析 `memory` 节点的 `reg` 属性后才更新它们。[dtb.c (line 78)](/home/iodwad/OS/lab2/kern/driver/dtb.c:78)、[dtb.c (line 103)](/home/iodwad/OS/lab2/kern/driver/dtb.c:103)、[dtb.c (line 144)](/home/iodwad/OS/lab2/kern/driver/dtb.c:144) 因而返回值分别是物理内存\**起始地址**与**字节长度**，不是末地址。[dtb.c (line 129)](/home/iodwad/OS/lab2/kern/driver/dtb.c:129)

物理地址与描述符转换的实际实现见 [pmm.h (line 83)](/home/iodwad/OS/lab2/kern/mm/pmm.h:83)：

```
static inline ppn_t page2ppn(struct Page *page) { return page - pages + nbase; }

static inline uintptr_t page2pa(struct Page *page) {
    return page2ppn(page) << PGSHIFT;
}

static inline struct Page *pa2page(uintptr_t pa) {
    if (PPN(pa) >= npage) {
        panic("pa2page called with invalid pa");
    }
    return &pages[PPN(pa) - nbase];
}
```

虚拟地址转物理地址的宏摘自 [pmm.h (line 54)](/home/iodwad/OS/lab2/kern/mm/pmm.h:54)：

```
#define PADDR(kva)                                                 \
    ({                                                             \
        uintptr_t __m_kva = (uintptr_t)(kva);                      \
        if (__m_kva < KERNBASE) {                                  \
            panic("PADDR called with invalid kva %08lx", __m_kva); \
        }                                                          \
        __m_kva - va_pa_offset;                                    \
    })
```

**源码事实：**同一文件中的 `KADDR(pa)` 位于块注释内，当前不是可调用宏。[pmm.h (line 63)](/home/iodwad/OS/lab2/kern/mm/pmm.h:63)

空闲区注册包装函数及实际调用见 [pmm.c (line 35)](/home/iodwad/OS/lab2/kern/mm/pmm.c:35)：

```
static void init_pmm_manager(void) {
    pmm_manager = &default_pmm_manager;
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

static void init_memmap(struct Page *base, size_t n) {
    pmm_manager->init_memmap(base, n);
}
```

这里的 `base` 是首个**可用页的 `struct Page \*`**，`n` 是连续可用页数，函数无返回值。当前选用 `default_pmm_manager`；其 `init_memmap` 实现要求 `n > 0` 且注册前范围内每个描述符均为保留状态，随后将其改为空闲状态并计入空闲页数。[default_pmm.c (line 18)](/home/iodwad/OS/lab2/kern/mm/default_pmm.c:18)、[default_pmm.c (line 246)](/home/iodwad/OS/lab2/kern/mm/default_pmm.c:246)

`pmm_init()` 的声明见 [pmm.h (line 36)](/home/iodwad/OS/lab2/kern/mm/pmm.h:36)。调用顺序是 `kern_init()` 先执行 `dtb_init()`、再执行 `pmm_init()`；后者依次执行 `init_pmm_manager()`、`page_init()`、`check_alloc_page()`。[init.c (line 28)](/home/iodwad/OS/lab2/kern/init/init.c:28)、[pmm.c (line 66)](/home/iodwad/OS/lab2/kern/mm/pmm.c:66) **当前 `pmm.c` 尚无 `page_init()` 的声明或定义**，只有预留位置和调用点。[pmm.c (line 64)](/home/iodwad/OS/lab2/kern/mm/pmm.c:64)

输出接口的声明为 `int cprintf(const char *fmt, ...);`，见 [stdio.h (line 8)](/home/iodwad/OS/lab2/libs/stdio.h:8)。

###### 5. 关键内存布局约定

- **源码事实：**`DRAM_BASE` 为 `0x80000000`；内核物理起点常量为 `0x80200000`，对应虚拟起点 `KERNBASE = 0xFFFFFFFFC0200000`，映射偏移常量为 `PHYSICAL_MEMORY_OFFSET = 0xFFFFFFFF40000000`。[riscv.h (line 152)](/home/iodwad/OS/lab2/libs/riscv.h:152)、[memlayout.h (line 5)](/home/iodwad/OS/lab2/kern/mm/memlayout.h:5)
- **源码事实：**[memlayout.h (line 9)](/home/iodwad/OS/lab2/kern/mm/memlayout.h:9) 的注释将默认 QEMU 配置中的 `[0x80000000, 0x80200000)` 标为 OpenSBI 占用区，并给出默认 RAM 上界 `0x88000000`。这是一项静态布局约定；设备树解析函数只提取内存节点的首组基址和大小。[dtb.c (line 78)](/home/iodwad/OS/lab2/kern/driver/dtb.c:78)
- **推导：**内核镜像占用从 `KERNEL_BEGIN_PADDR` 起，到 `PADDR(end)` 附近；由于 `end` 是末尾符号，能交给页分配器的起点必须考虑它所在页及之后放置的描述符数组。描述符数组自身所占物理页也须保留。依据为链接脚本中的镜像起点、`end` 定义，以及 [test_lab2.c (line 43)](/home/iodwad/OS/lab2/tools/test_lab2.c:43)、[test_lab2.c (line 67)](/home/iodwad/OS/lab2/tools/test_lab2.c:67) 的检查。
- **推导：**对 `DRAM_BASE ≤ pa` 且 `PPN(pa) < npage` 的地址，描述符索引为 `PPN(pa) - nbase`；反向地址为 `(page - pages + nbase) << PGSHIFT`。这里的 `pa2page()` 仅显式检查上界，**没有检查 `pa < DRAM_BASE`**。[pmm.h (line 83)](/home/iodwad/OS/lab2/kern/mm/pmm.h:83)
- **源码事实：**随工程提供的测试期望 `va_pa_offset == PHYSICAL_MEMORY_OFFSET`、`npage == PPN(mem_base + mem_size)`，并检查 `pages` 页对齐、位于 `ROUNDUP(end, PGSIZE)` 之后，且内核尾页与描述符数组首地址对应页仍标为保留。[test_lab2.c (line 19)](/home/iodwad/OS/lab2/tools/test_lab2.c:19)、[test_lab2.c (line 31)](/home/iodwad/OS/lab2/tools/test_lab2.c:31)、[test_lab2.c (line 43)](/home/iodwad/OS/lab2/tools/test_lab2.c:43)、[test_lab2.c (line 67)](/home/iodwad/OS/lab2/tools/test_lab2.c:67)

##### 可能影响 `page_init()` 实现的关键注意事项

1. `init_memmap()` 所接收的范围必须已标记为 `PG_reserved`，且页数大于零；它会自行清除该范围的保留状态。[default_pmm.c (line 18)](/home/iodwad/OS/lab2/kern/mm/default_pmm.c:18)
2. `pages` 的索引基点是 `DRAM_BASE`，所以描述符数量应覆盖从 `nbase` 到 `npage` 的物理页号；OpenSBI 所在页虽不可分配，仍落在该索引范围内。[pmm.c (line 22)](/home/iodwad/OS/lab2/kern/mm/pmm.c:22)、[pmm.h (line 104)](/home/iodwad/OS/lab2/kern/mm/pmm.h:104)
3. `PHYSICAL_MEMORY_END` 是硬编码默认上界，而设备树 getter 提供运行时探测结果；两者是否总相同**待确认**。当前测试明确以 `mem_base + mem_size` 计算 `npage`。[memlayout.h (line 12)](/home/iodwad/OS/lab2/kern/mm/memlayout.h:12)、[test_lab2.c (line 31)](/home/iodwad/OS/lab2/tools/test_lab2.c:31)
4. 设备树解析失败时两个 getter 可返回零；解析器未展示对多段内存或 DTB 保留映射的处理。因此，多段内存与额外保留区的处理要求**待确认**。[dtb.c (line 103)](/home/iodwad/OS/lab2/kern/driver/dtb.c:103)、[dtb.c (line 128)](/home/iodwad/OS/lab2/kern/driver/dtb.c:128)
5. `page_init()` 当前既未定义也未提前声明；若按题目要求定义为 `static` 函数，需留意它在 `pmm_init()` 调用点之前的声明或定义位置。[pmm.c (line 64)](/home/iodwad/OS/lab2/kern/mm/pmm.c:64)

[GUARANTEE]

必须实现以下接口：

```
static void page_init(void);
```

该函数位于 `kern/mm/pmm.c`，不接收参数，也不返回结果，由 `pmm_init()` 调用。

可以根据需要新增模块内部的 `static` 辅助函数，但不得修改现有公开接口。

[SPECIFICATION]

##### page_init

**Pre-Condition**:

- 系统已完成设备树初始化，可以通过现有接口获取物理内存的起始地址和大小。
- 当前物理内存管理器已完成初始化，可以接收空闲物理页。
- 内核链接符号 `end` 可用于确定内核镜像结束位置。
- 当前工程的物理内存布局、地址转换机制及相关宏定义已经可用。

**Post-Condition**:

- 根据设备树信息确定有效物理内存范围，并通过控制台输出探测结果。
- 正确初始化 `va_pa_offset`，使其符合当前工程定义的虚拟地址与物理地址映射关系。
- 正确设置 `npage`，使其表示可管理物理页号的排他上界，并与设备树提供的有效内存范围及现有页号索引规则一致。
- `pages` 指向内核镜像结束位置之后、满足页面对齐要求的合法虚拟地址。
- 描述符数组覆盖当前工程要求管理的全部物理页，描述符数量及索引关系与 `nbase`、`npage`、`pa2page()` 保持一致。
- 所有相关 `Page` 描述符完成必要初始化，其引用计数、页面属性和保留状态符合当前物理内存管理器要求。
- OpenSBI、内核镜像、描述符数组及其他必须保留的区域不会被注册为空闲页面。
- 剩余完整、未占用且满足页面对齐要求的物理页已注册到当前物理内存管理器。

**Case 1：物理内存信息无效或无法完成合法布局**

- 当设备树提供的物理内存起始地址或大小无效、地址范围计算发生溢出，或者物理内存范围与当前内核布局不兼容时，应通过 `panic()` 终止初始化。
- 当物理内存不足以容纳内核及其所需的页描述符数组时，应终止初始化，不得越界访问或注册非法页面。

**Case 2：存在可用的完整空闲页面**

- 当有效物理内存能够容纳内核及页描述符数组，且排除保留区域后仍存在完整空闲页面时，应将这些页面交给当前物理内存管理器。
- 注册的起始位置必须对应首个可分配物理页的 `Page` 描述符，注册数量必须与实际连续可用物理页数一致。
- 注册前相关描述符必须满足当前物理内存管理器对保留状态的要求，注册后的页面状态由物理内存管理器维护。

**Case 3：不存在可用的完整空闲页面**

- 当内核和页描述符数组能够合法建立，但剩余内存不足以形成完整可分配物理页时，不得调用 `init_memmap()` 注册空区间。
- 已占用的物理页面必须继续保持保留状态。

**Requirements**:

- 必须严格区分虚拟地址、物理地址、物理页号及 `Page` 描述符地址。
- 必须保证 `npage` 与当前工程的物理页号索引约定一致，避免描述符索引越界。
- 必须正确处理上下边界和页面对齐，禁止将不完整页面注册为空闲页面。
- 必须确保内核镜像及描述符数组所占用的空间不与可分配区域重叠。
- 调用 `init_memmap()` 时，传入的页面数量必须大于零，起始地址必须是合法的 `struct Page *`。
- 必须使用现有物理内存管理器和地址转换接口，保持与工程现有代码及测试要求兼容。

现在请根据以上 [PROMPT]、[RELY]、[GUARANTEE] 和 [SPECIFICATION]，直接完成 `kern/mm/pmm.c` 中 `page_init()` 的实现。

## 功能模块二：物理内存分配

### 任务二：实现 Best-Fit 连续物理内存分配算法

负责人：2412565—张宸笛号。

#### 1. 准备 [RELY] 的提示词

我正在完成 uCore Lab2 的 Best-Fit 连续物理内存分配实验，需要按照老师要求的四段式提示词框架编写 Prompt：

- [PROMPT]
- [RELY]
- [GUARANTEE]
- [SPECIFICATION]

现在请你**只帮助我准备 [RELY] 部分，不要修改任何代码文件，也不要实现 Best-Fit 算法。**

请阅读以下文件：

1. `kern/mm/default_pmm.c`：分析 First-Fit 的实现方式。
2. `kern/mm/best_fit_pmm.c`：分析现有代码，重点关注 `basic_check()` 和 `best_fit_check()` 的状态依赖。
3. `kern/mm/pmm.h`：查找 `struct pmm_manager` 和相关接口。
4. 其他必要头文件：查找 Page 结构体、链表操作函数、页面状态宏等真实定义。

请根据这些文件，生成可以直接用于 Best-Fit 实验提示词的 `[RELY]` 部分。

具体要求：

1. 提取与任务直接相关的宏定义、数据结构、全局变量和可调用函数。
2. 保留与实际代码完全一致的名称、类型和函数签名。
3. 对关键结构体给出必要的字段定义，并说明字段作用。
4. 说明 `free_list`、`nr_free`、`property`、`page_link` 和 `PageProperty` 之间的关系。
5. 说明 `struct pmm_manager` 的接口约定。
6. 指出两个测试函数对模块内部状态、变量名称和行为的具体要求。
7. 标明重要定义所在的文件路径。
8. 只保留 Best-Fit 实现真正需要的上下文，不要复制无关代码。
9. 不要凭经验虚构任何宏、函数或结构体成员；如果找不到定义，明确说明。

请按照以下格式输出：

[RELY]

###### 宏定义

（真实定义及来源）

###### 数据结构

（真实定义及来源）

###### 可用函数与链表操作

（真实签名及作用）

###### 全局变量和状态约定

（有关状态变量及其不变量）

###### 测试函数依赖

（basic_check 和 best_fit_check 的要求）

另外，请在 [RELY] 之后单独列出建议写进 [GUARANTEE] 的接口清单，并标明哪些签名已由源码确认。

**注意：本次任务仅收集与整理上下文，不得修改任何项目文件。**

#### 2. 完整实现提示词

##### [PROMPT]
**任务：**

参考 `kern/mm/default_pmm.c` 中已有的 First-Fit 连续物理内存分配器，实现完整的 Best-Fit 连续物理内存分配模块。

需要修改以下文件：

1. `kern/mm/best_fit_pmm.c`：在标有 `lab2` 和 `YOUR CODE` 的位置完成 Best-Fit 物理内存管理器，包括初始化、分配、释放、空闲页统计及管理器注册。
2. `kern/mm/pmm.c`：修改 `init_pmm_manager()`，将当前物理内存管理器切换为 `best_fit_pmm_manager`。

**操作要求：**

- 必须直接修改项目中的实际文件，而不是只输出代码片段。
- 在修改前阅读 `default_pmm.c`、`best_fit_pmm.c`、`best_fit_pmm.h`、`pmm.h` 和相关链表实现。
- 尽可能参考 First-Fit 已有的内存管理框架，实现 Best-Fit 的选块策略。
- 保留已有代码，严格禁止修改 `basic_check()` 和 `best_fit_check()` 两个测试函数。
- 除完成模块及切换管理器所必需的代码外，不得修改其他无关文件和接口。

**输出要求：**

根据以下 `[RELY]`、`[GUARANTEE]` 和 `[SPECIFICATION]` 完成代码实现。以项目中的真实定义为准，不引入不必要的数据结构或外部依赖。

完成后说明实际修改的文件、主要实现思路和验证结果。若无法运行测试，须说明原因，不得声称已经通过。

##### [RELY]

###### 宏定义

以下名称和定义均来自现有头文件：

```
// kern/mm/memlayout.h
#define PG_reserved                 0
#define PG_property                 1
#define SetPageReserved(page)       ((page)->flags |= (1UL << PG_reserved))
#define ClearPageReserved(page)     ((page)->flags &= ~(1UL << PG_reserved))
#define PageReserved(page)          (((page)->flags >> PG_reserved) & 1)
#define SetPageProperty(page)       ((page)->flags |= (1UL << PG_property))
#define ClearPageProperty(page)     ((page)->flags &= ~(1UL << PG_property))
#define PageProperty(page)          (((page)->flags >> PG_property) & 1)
#define le2page(le, member)         to_struct((le), struct Page, member)

// kern/mm/pmm.h
#define alloc_page()                alloc_pages(1)
#define free_page(page)             free_pages(page, 1)

// kern/mm/mmu.h
#define PGSIZE                      4096
```

`le2page` 使用的 `to_struct(ptr, type, member)` 定义在 [libs/defs.h (line 65)](/home/iodwad/OS/lab2/libs/defs.h:65)，用于从成员指针取得所属结构体的指针。`assert(x)` 定义在 [kern/debug/assert.h (line 15)](/home/iodwad/OS/lab2/kern/debug/assert.h:15)。页面状态宏及 `le2page` 见 [kern/mm/memlayout.h (line 41)](/home/iodwad/OS/lab2/kern/mm/memlayout.h:41)；单页操作宏见 [kern/mm/pmm.h (line 38)](/home/iodwad/OS/lab2/kern/mm/pmm.h:38)。

###### 数据结构

```
// libs/list.h
struct list_entry {
    struct list_entry *prev, *next;
};
typedef struct list_entry list_entry_t;

// kern/mm/memlayout.h
struct Page {
    int ref;
    uint64_t flags;
    unsigned int property;
    list_entry_t page_link;
};

typedef struct {
    list_entry_t free_list;
    unsigned int nr_free;
} free_area_t;
```

一个 `struct Page` 描述一个物理页。`ref` 是引用计数，`flags` 保存页面状态；**空闲块首页**的 `property` 记录该连续块的页数，`page_link` 是将这个首页挂入空闲链表的节点。`free_area_t.free_list` 是链表头，`free_area_t.nr_free` 是空闲**页**总数。[链表定义 (line 17)](/home/iodwad/OS/lab2/libs/list.h:17) · [页面与空闲区定义 (line 29)](/home/iodwad/OS/lab2/kern/mm/memlayout.h:29)

物理内存管理器的实际接口为：

```
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

其中 `init` 初始化管理状态；`init_memmap` 登记初始连续空闲页；`alloc_pages`、`free_pages` 分配和回收连续页；`nr_free_pages` 返回空闲页数；`check` 是管理器自检入口。[接口定义 (line 15)](/home/iodwad/OS/lab2/kern/mm/pmm.h:15)

###### 可用函数与链表操作

```
// libs/list.h：以下均为 static inline 函数
void list_init(list_entry_t *elm);
void list_add(list_entry_t *listelm, list_entry_t *elm);
void list_add_before(list_entry_t *listelm, list_entry_t *elm);
void list_add_after(list_entry_t *listelm, list_entry_t *elm);
void list_del(list_entry_t *listelm);
bool list_empty(list_entry_t *list);
list_entry_t *list_next(list_entry_t *listelm);
list_entry_t *list_prev(list_entry_t *listelm);
```

`list_init` 将节点初始化为空链表头；`list_add` 把新节点放在指定节点**之后**；`list_add_before` 放在指定节点之前；`list_del` 摘除节点；`list_next`、`list_prev` 用于遍历。`list_empty` 检查链表头是否指向自身。[函数定义 (line 36)](/home/iodwad/OS/lab2/libs/list.h:36)

```
// kern/mm/pmm.h
struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void);
static inline int page_ref(struct Page *page);
static inline void set_page_ref(struct Page *page, int val);
static inline uintptr_t page2pa(struct Page *page);
extern size_t npage;
extern const struct pmm_manager *pmm_manager;
```

前三个函数通过当前的 `pmm_manager` 调用相应回调。测试用 `page_ref` 检查引用计数，用 `page2pa`、`npage` 和 `PGSIZE` 检查分配页的地址范围。[声明与内联函数 (line 34)](/home/iodwad/OS/lab2/kern/mm/pmm.h:34) · [转发实现 (line 42)](/home/iodwad/OS/lab2/kern/mm/pmm.c:42)

###### 全局变量和状态约定

[First-Fit 实现 (line 6)](/home/iodwad/OS/lab2/kern/mm/default_pmm.c:6)采用以下**文件内状态**：

```
static free_area_t free_area;
#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)
```

其链表按页地址排序，每个空闲块只将首页的 `page_link` 挂入 `free_list`；该首页满足 `PageProperty(page)`，`page->property` 是块长。`nr_free` 应等于所有链表节点所代表块长之和。First-Fit 从链表头起选择第一个足够大的块，必要时拆分；释放后按地址插回，并合并物理上相邻的块。[First-Fit 代码 (line 11)](/home/iodwad/OS/lab2/kern/mm/default_pmm.c:11)

**当前 [best_fit_pmm.c (line 1)](/home/iodwad/OS/lab2/kern/mm/best_fit_pmm.c:1) 尚未定义 `free_area`、`free_list`、`nr_free` 或 Best-Fit 管理器实例。\**但该文件中的测试直接引用可读写的 `free_list`、`nr_free`，所以实现必须在测试函数可见的作用域内提供这两个\**准确名称**。沿用 First-Fit 的上述 `free_area_t` 写法是一种与现有类型及测试相符的方式；不能将它描述为 Best-Fit 文件中已经存在的定义。

另需注意：[pmm.c (line 35)](/home/iodwad/OS/lab2/kern/mm/pmm.c:35) 当前将 `pmm_manager` 设为 `&default_pmm_manager`。这一选择会决定 `alloc_pages` 等公共函数实际调用哪个管理器。

###### 测试函数依赖

两个函数均已定义在 [kern/mm/best_fit_pmm.c (line 8)](/home/iodwad/OS/lab2/kern/mm/best_fit_pmm.c:8)，源码注释要求保留它们。

- `basic_check(void)`：连续分配 3 个互不相同的页，要求其引用计数为 0、物理地址在 `npage * PGSIZE` 范围内。它会保存 `free_list` 和 `nr_free`，调用 `list_init(&free_list)` 将链表临时置空，再把 `nr_free` 置 0；空链表时分配必须失败。释放 3 页后，`nr_free` 必须为 3；再次分配至耗尽后必须返回 `NULL`。只释放 `p0` 时，下一次单页分配必须返回同一个 `p0`。最后它恢复保存的状态并回收测试页。[测试代码 (line 8)](/home/iodwad/OS/lab2/kern/mm/best_fit_pmm.c:8)
- `best_fit_check(void)`：先遍历 `free_list`，要求每个节点对应的页都有 `PageProperty`，且各节点 `property` 之和等于 `nr_free_pages()`；随后调用 `basic_check()`。它分配连续 5 页后，再临时清空链表：**此时尚未把 `nr_free` 清零，就先要求 `alloc_page()` 返回 `NULL`**，因此分配不能仅依据计数判断成功。之后它在隔开的两处释放 2 页和 1 页，要求申请 4 页失败；申请 1 页时必须选中较小的、位于 `p0 + 4` 的 1 页块，体现 Best-Fit 选择；申请 2 页也必须成功。最后要求可重新得到连续 5 页、耗尽时返回 `NULL`、`nr_free == 0`，并在恢复原状态及回收后保持链表节点数和空闲页总数与测试前一致。[测试代码 (line 60)](/home/iodwad/OS/lab2/kern/mm/best_fit_pmm.c:60)

##### [GUARANTEE]

必须在 `kern/mm/best_fit_pmm.c` 中实现以下接口：

```
static void best_fit_init(void);

static void best_fit_init_memmap(
    struct Page *base, size_t n);

static struct Page *best_fit_alloc_pages(
    size_t n);

static void best_fit_free_pages(
    struct Page *base, size_t n);

static size_t best_fit_nr_free_pages(void);
```

必须定义管理器实例：

```
const struct pmm_manager best_fit_pmm_manager;
```

其中管理器的 `check` 回调应指向已有的 `best_fit_check()`。

必须在 `kern/mm/pmm.c` 中修改现有的：

```
init_pmm_manager()
```

使其选择 `best_fit_pmm_manager`，而不是 `default_pmm_manager`。

允许根据需要增加模块内部的静态变量、宏定义和辅助函数，但不得修改 `struct pmm_manager` 的公开接口。

必须提供测试函数可直接访问的 `free_list` 和 `nr_free`，并保证其状态符合测试约定。

##### [SPECIFICATION]

###### 1. best_fit_init()

**Pre-Condition：**

- 物理内存管理器尚未完成初始化。

**Post-Condition：**

- 建立初始为空的双向循环空闲链表。
- 空闲物理页总数初始化为 0。
- 模块进入可以登记空闲物理页的状态。

**Requirements：**

- 初始化后的状态必须与两个现有测试函数使用的状态约定兼容。

###### 2. best_fit_init_memmap(struct Page *base, size_t n)

**Pre-Condition：**

- `base` 指向一段连续物理页对应的第一个 `Page` 描述符。
- `n > 0`，表示该段物理页的数量。
- 待初始化的物理页处于保留状态。
- 空闲链表已完成初始化。

**Post-Condition：**

- 将从 `base` 开始的 `n` 个连续物理页初始化为空闲状态。
- 清除相关页面的旧状态，并将引用计数初始化为 0。
- 将整段连续物理页登记为一个空闲块。
- 空闲块首页正确记录块大小及空闲块标志。
- 空闲链表中增加代表该空闲块的节点。
- 空闲物理页总数增加 `n`。
- 空闲链表按物理页地址升序排列。

**Case 1：空闲链表为空**

直接将新空闲块登记为空闲链表的第一个有效节点。

**Case 2：空闲链表不为空**

根据新空闲块的起始地址，找到合适的插入位置，使插入后所有空闲块仍按地址升序排列。

**Requirements：**

- 每个连续空闲块只通过其首页的 `page_link` 加入链表，不能将该块的每个物理页分别加入。
- `property` 表示空闲块包含的连续物理页数。
- 参考现有 First-Fit 的初始空闲区登记语义，确保与其余管理函数和测试兼容。

###### 3. best_fit_alloc_pages(size_t n)

**Pre-Condition：**

- `n > 0`，表示申请的连续物理页数量。
- 物理内存管理器已经初始化。
- 空闲链表中的有效节点分别代表连续空闲块，其首页记录对应块大小。

**Post-Condition：**

- 如果存在满足申请要求的空闲块，则从所有可用空闲块中选择大小最小的一个进行分配。
- 成功时返回分配的连续物理页的首页地址。
- 正确维护空闲链表、页面状态和空闲页计数。
- 不存在满足条件的空闲块时返回 `NULL`，且不改变已有空闲内存管理状态。

**Case 1：不存在足够大的连续空闲块**

遍历空闲链表后，若所有空闲块的大小都小于 `n`，则分配失败，返回 `NULL`。

即使空闲页总数大于等于 `n`，只要没有足够大的连续空闲块，也不能成功分配。

**Case 2：存在大小恰好等于 n 的空闲块**

选择该空闲块，将其从空闲链表中移除，更新相应的页面状态与空闲页计数，并返回其首页。

**Case 3：最佳空闲块的大小大于 n**

选择满足条件且大小最小的空闲块，从中分配连续 `n` 个物理页。

将剩余物理页保留为空闲块，正确维护剩余块的首页、块大小、页面标志和链表关系。

分配完成后，空闲页总数减少 `n`。

**Requirements：**

- 必须实现 Best-Fit，而不是 First-Fit。不能在找到第一个足够大的空闲块后直接结束搜索，除非已找到大小恰好等于 `n` 的块。
- 空闲链表为空时必须返回 `NULL`，不能仅根据 `nr_free` 的值认定分配成功。
- 不得分配已被占用的物理页。
- 如果存在多个同样大小的最佳空闲块，可以选择遍历时最先遇到的块。
- 分配成功后，空闲链表只包含剩余的空闲块，且保持地址有序。

###### 4. best_fit_free_pages(struct Page *base, size_t n)

**Pre-Condition：**

- `base` 指向待释放连续物理页的首页。
- `n > 0`。
- 待释放物理页属于合法的已分配内存区域，且不存在重复释放。

**Post-Condition：**

- 将指定的 `n` 个连续物理页重新设置为空闲状态。
- 正确清理相关页面状态及引用计数。
- 将释放的内存登记到空闲链表中。
- 根据物理地址判断相邻空闲块，并尽可能合并。
- 保持空闲链表按照物理地址升序排列。
- 空闲物理页总数增加 `n`，不能因为合并而重复计数。

**Case 1：释放的内存与现有空闲块均不相邻**

创建一个新的空闲块，并按物理地址顺序插入空闲链表。

**Case 2：释放的内存与前一个空闲块相邻**

将释放的内存与前一个空闲块合并，形成更大的连续空闲块。

**Case 3：释放的内存与后一个空闲块相邻**

将释放的内存与后一个空闲块合并，确保新空闲块的首页和块大小正确。

**Case 4：释放的内存同时与前后两个空闲块相邻**

将三个连续区域合并为一个空闲块，删除不再需要的链表节点，并维护合并后首页的块大小和状态标志。

**Requirements：**

- 只能合并物理地址实际连续的空闲块，不能因为链表节点相邻就直接合并。
- 合并后只能保留一个代表整个空闲块的有效链表节点。
- 不得破坏其他已分配物理页的信息。

###### 5. best_fit_nr_free_pages()

**Pre-Condition：**

- 物理内存管理器已初始化。

**Post-Condition：**

- 返回当前记录的空闲物理页总数。

**Requirements：**

- 正常运行状态下，该值应与空闲链表中所有空闲块的 `property` 之和相等。
- 分配与释放操作必须正确更新计数。
- 保持与现有测试函数直接读取和修改 `nr_free` 的行为兼容。

###### 6. best_fit_pmm_manager

**Post-Condition：**

- 定义完整的 `struct pmm_manager` 实例。
- `name` 标识 Best-Fit 管理器。
- `init`、`init_memmap`、`alloc_pages`、`free_pages` 和 `nr_free_pages` 分别指向对应的 Best-Fit 实现。
- `check` 指向已有的 `best_fit_check()`。

**Requirements：**

- 不得修改 `basic_check()` 和 `best_fit_check()` 的实现。
- 所有回调函数签名必须与 `struct pmm_manager` 一致。

###### 7. init_pmm_manager()

**Pre-Condition：**

- `best_fit_pmm_manager` 已定义，并可被 `pmm.c` 正确引用。

**Post-Condition：**

- 全局 `pmm_manager` 指向 `best_fit_pmm_manager`。
- 后续物理内存初始化、分配、释放及检查使用 Best-Fit 实现。

**Requirements：**

- 仅修改管理器选择及其必需的声明或头文件引用。
- 不修改现有内存管理器公共接口及无关初始化流程。

###### 8. 整体验证要求

- 现有 `basic_check()` 和 `best_fit_check()` 必须保持原样。
- 两个测试函数依赖的状态变量、链表结构及块属性必须兼容。
- 分配、释放、切割与合并后，不得产生损坏的链表节点。
- 不得出现空闲页计数错误、重复分配或错误合并。
- 如果项目提供编译和测试命令，应执行实际验证并报告结果。
- 最终说明相对于 First-Fit 实现所做的修改，以及 Best-Fit 算法可能的改进方向。

---

## 协作者原有归档：Buddy 与 SLUB

负责人：2411560—朱泽帅。以下四节为实际使用的完整原始输入，未用报告核对稿替代。独立文件保留归档原字节，来源与哈希见 docs/2411560/prompt_provenance.json。

## 1. Buddy_Implementation_Prompt

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

---

## 2. Buddy_Test_Prompt

[PROMPT]

**任务**：针对当前已经实现的 uCore Buddy System 伙伴分配器，设计并实现完整的正确性测试函数，验证物理页初始化、分配、拆分、释放、伙伴合并、空闲页统计及异常处理是否符合 Buddy System 的功能规格。

本任务需要真实执行测试，发现并定位实现中的问题，确保测试能够检验算法正确性，而不是仅验证代码可以编译。

**目标工程**：

`D:\college_study_materials\junior_year_fall\OS\lab2\buddy_work`

**相关文件**：

- `kern/mm/buddy_pmm.c`：已经实现的 Buddy 分配器及基础检查。
- `kern/mm/buddy_pmm.h`：管理器声明。
- `kern/mm/pmm.c`：当前使用 Buddy 管理器。
- `kern/mm/pmm.h`：物理内存管理器接口。
- `kern/mm/memlayout.h`：Page 描述符和状态宏。
- `kern/mm/default_pmm.c`：已有物理页分配测试的参考实现。

**操作要求**：

1. 必须针对上一轮实际生成的 Buddy 模块编写并运行测试，不得重新实现一套替代 Buddy 算法作为测试对象。
2. 先阅读当前 `buddy_pmm.c`、`buddy_pmm.h`、`BUDDY_IMPLEMENTATION.md` 和已有测试，确认实际采用的分配容量、元数据及释放契约。
3. 在实际工程中新增或完善测试函数，不只是展示测试代码片段。
4. 保留上一轮已经通过的基础检查，并在此基础上扩展新的功能测试和边界测试。
5. 不修改共享 `code`、Lab1、官方 First-Fit、Best-Fit、`page_init()` 或官方评分测试。
6. 如果测试发现算法错误，应保留失败证据、给出最小复现条件并明确根因。只有确认属于 Buddy 实现错误后才能修改相关代码，且必须重新运行回归测试。
7. 不得删除失败测试、放宽断言或通过硬编码成功输出制造通过结果。
8. 可以为需要重置分配器状态的测试建立独立测试环境，但必须测试真实 Buddy 源码，并将主机测试与 RISC-V QEMU 测试明确区分。

**输出要求**：

依据以下 `[RELY]`、`[GUARANTEE]` 和 `[SPECIFICATION]` 完成测试函数设计、源码修改、实际运行、错误定位及结果整理。

每个测试应明确输入条件、预期结果、实际检查内容及通过标准。

---

[RELY]

### 1. 已实现的 Buddy System

当前工程已经实现：

```c
static void buddy_init(void);

static void buddy_init_memmap(
    struct Page *base, size_t n
);

static struct Page *buddy_alloc_pages(
    size_t n
);

static void buddy_free_pages(
    struct Page *base, size_t n
);

static size_t buddy_nr_free_pages(void);

static void buddy_check(void);
```

并通过：

```c
const struct pmm_manager buddy_pmm_manager;
```

注册到 uCore。

上一轮实现采用分阶空闲链表、静态逐页管理记录，不依赖动态内存分配。

已声明的分配器契约：

- 空闲块的大小为 `2^k` 个物理页。
- 原生 Buddy 块满足物理页号对齐约束。
- 非 2 的幂请求向上取整至最小可容纳的 2 的幂容量。
- `alloc_pages(3)` 实际独占 4 页。
- `free_pages(base,3)` 使用原始申请长度，能够归还完整的 4 页。
- 第一版仅支持完整配对释放，不支持从已分配块中间释放部分页面。
- 只有同阶、完整空闲且属于合法管理范围的伙伴才能合并。
- 非法登记和释放必须被诊断，不能破坏现有管理状态。

所有测试以实际源码及最终实现说明为准。如果实现与上述契约不一致，应先报告差异，不能默默修改测试预期。

### 2. uCore 物理内存管理接口

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

已有公共接口：

```c
struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void);
```

测试应优先从公共接口验证分配器的可观察行为。需要检查内部 Buddy 不变量时，可以使用不改变管理状态的测试辅助逻辑。

### 3. Page 描述符与页面信息

来源：`kern/mm/memlayout.h`

```c
struct Page {
    int ref;
    uint64_t flags;
    unsigned int property;
    list_entry_t page_link;
};
```

已有：

- `PageReserved(page)`：保留状态。
- `PageProperty(page)`：空闲块头状态。
- `page2ppn(page)`：将描述符转换为物理页号。
- `page2pa(page)`：将描述符转换为物理页起始地址。
- `pages`、`nbase`、`npage`：管理描述符数组和物理页号范围。

本实验：

```c
#define PGSIZE 4096
#define PGSHIFT 12
```

测试必须区分 `struct Page *` 与实际物理页地址，不能把描述符地址当作物理页地址进行伙伴判断。

### 4. 已有验证结果

上一轮已经完成：

- Buddy RISC-V 对象编译成功。
- 完整内核编译成功。
- 基础 Host 探针通过，完成 29 页登记及状态恢复。
- QEMU 内核启动成功。
- Buddy 基础检查通过。
- 基础检查后恢复 31801 个空闲页。
- GDB 确认内核正常驻留，未观察到 panic。

上述数字仅作为上一轮的回归参考，不应在新测试中硬编码为通用预期。

实际测试应根据运行前的资源状态计算预期值。

### 5. 测试运行环境

使用现有 Ubuntu 22.04、RISC-V 交叉编译器和 QEMU。

上一轮确认当前工程可以通过使用 `-kernel bin/ucore.img` 启动内核。

本轮应优先复用上一轮已经成功的实际启动参数，不得为了测试而修改内核启动代码或破坏现有 Makefile。

完整运行命令以当前工程和上一轮实际验证记录为准。

---

[GUARANTEE]

必须在真实工程中提供 Buddy System 专项测试功能，至少包括：

```c
static void buddy_check(void);
```

允许新增模块内部 `static` 辅助测试函数。

可以根据测试需要使用单独的测试源文件或隔离测试入口，但不得影响正常内核初始化。

`buddy_pmm_manager.check` 必须能够实际调用并执行 Buddy 检查，不能为空函数。

必须覆盖以下测试类别：

1. 空闲物理页区间初始化。
2. 基本物理页分配与释放。
3. 大块拆分。
4. 合法伙伴合并。
5. 多级伙伴合并。
6. 相邻但非伙伴的块。
7. 非 2 的幂请求及完整回收。
8. 空闲页数量和页面所有权守恒。
9. 分配失败及状态保持。
10. 非法操作与边界条件。
11. 多次分配释放及压力测试。

对于需要重新初始化 Buddy 状态、构造独立内存布局或故意触发断言的测试，应提供独立的测试环境，不能直接清空真实内核正在使用的管理状态。

最终交付：

- 实际新增或完善的测试源码。
- 测试入口与 Buddy 管理器的连接。
- 每项测试的输入、预期结果和实际结果。
- 编译与运行日志。
- 失败测试的最小复现和修复记录。
- 测试覆盖说明。
- 用于实验报告的测试结果汇总。

---

[SPECIFICATION]

## 1. 空闲物理页初始化测试

**Pre-Condition**：

- Buddy 管理器处于合法的初始状态。
- 测试能够提供受控的、有效的物理页描述符区间。
- 测试输入与其他正在使用的内核资源相互隔离。

**Post-Condition**：

- 全部合法输入页面被正确纳入管理。
- 空闲页总数等于实际登记的页面数量。
- 所有空闲块满足 Buddy 大小及对齐要求。
- 不出现遗漏、重复登记或越界页面。

**Case 1：输入页数为 2 的幂**

- 登记一个长度为 16 页且起点合法对齐的空闲区。
- 验证能够完整管理 16 页。
- 验证该内存区间满足标准 Buddy 块组织规则。

**Case 2：输入页数不是 2 的幂**

- 登记一个包含 13 页的连续合法空闲区。
- 验证最终管理页数仍为 13。
- 不允许只登记 8 页而遗漏剩余 5 页，也不允许向外扩张到 16 页。

**Case 3：起点未按大块对齐**

- 使用满足框架地址边界但不满足 8 页或 16 页块对齐的输入区间。
- 验证分配器能够正确组织其中的全部可用页。
- 不允许形成起始页号不满足大小对齐的原生 Buddy 块。

**Case 4：多次登记**

- 分别登记两个不连续的合法区间。
- 验证第二次登记不会清空第一次登记的状态。
- 不得跨越未登记的内存空洞错误合并。
- 若测试相邻区间，则应按实现声明的合并域策略检查。

**Requirements**：

- 需要重置整个管理器的测试应在独立环境中运行。
- 不得在正常 QEMU 内核的运行中随意调用 `buddy_init()` 清空全局分配器。

## 2. 基本分配与释放测试

**Pre-Condition**：

- Buddy 管理器已经管理足够的合法物理页。
- 当前空闲页数量可查询。

**Post-Condition**：

- 成功分配返回有效的首 Page 描述符。
- 对应实际分配块在合法物理内存范围内。
- 多个活动分配不重叠。
- 所有合法分配完全释放后，空闲页数量恢复。

**Case 1：不同页数的申请**

至少验证：

```c
alloc_pages(1);
alloc_pages(2);
alloc_pages(3);
alloc_pages(4);
alloc_pages(5);
alloc_pages(8);
```

检查每次成功分配的实际容量、地址对齐、页面状态和剩余空闲页数量。

**Case 2：连续申请多个块**

- 连续申请多个小块。
- 验证所有活动分配的物理页区间不重叠。
- 不得返回 Reserved 页面或之前尚未释放的页面。

**Case 3：完整释放**

- 使用申请时返回的起始 Page 指针及原始请求页数进行释放。
- 验证所有实际独占页面均被回收。
- 验证空闲页数量恢复。

## 3. Buddy 拆分测试

**Pre-Condition**：

- 存在一个已确认合法、完整空闲的 16 页 Buddy 块。

**Post-Condition**：

- 小块分配能够从较大空闲块中获得合法页面。
- 未使用的其余页面仍然可以再次分配。
- 拆分不改变总资源守恒关系。

**Case 1：16 页块申请 1 页**

- 检查返回的单页有效。
- 实际空闲页数量减少 1。
- 其余 15 页仍然属于可管理资源。

**Case 2：多次拆分**

在受控布局中连续申请：

```c
alloc_pages(1);
alloc_pages(2);
alloc_pages(4);
```

验证三次分配的区间互不重叠，且所有返回块满足大小和对齐约束。

**Requirements**：

- 不能只通过返回非 NULL 判定拆分正确。
- 还需要验证未使用块仍可分配，以及总资源数量守恒。

## 4. 伙伴合并测试

**Pre-Condition**：

- 存在可控的 Buddy 分配状态。
- 可以通过合法分配与释放构造需要检查的伙伴关系。

**Post-Condition**：

- 只有符合 Buddy 关系的空闲块才允许合并。
- 合并后的块大小和对齐合法。
- 不能错误回收其他活动分配。

**Case 1：伙伴尚未释放**

- 释放一个块，但其伙伴仍然被占用。
- 验证不会错误合并。

**Case 2：两个合法伙伴均空闲**

- 先后释放两个同阶合法伙伴。
- 验证能够合并成更大的块。

**Case 3：多级伙伴合并**

- 在受控的 16 页空闲块中，分配多个小块。
- 按不同顺序释放所有块。
- 验证能够恢复完整的 16 页空闲块。

**Case 4：相邻但不是伙伴**

- 构造两个物理地址相邻但不构成合法 Buddy 关系的空闲块。
- 验证不能错误地将它们合并。

**Case 5：伙伴跨越不可管理区域**

- 构造两个中间存在未登记区域或保留页的合法测试区间。
- 验证 Buddy 不跨越非法边界合并。

**Requirements**：

- 合并成功不能仅靠空闲页数恢复来判断。
- 必须进一步验证合并后可以重新分配相应大小的连续大块，或通过合法的内部状态检查直接证明合并结果。

## 5. 非 2 的幂请求测试

**Pre-Condition**：

- Buddy 分配器具有足够容量。
- 实现采用向上取整为 2 的幂的实际独占容量策略。

**Post-Condition**：

- 实际分配和释放的容量一致。
- 不发生隐藏页泄漏。
- 空闲页统计符合实际占用情况。

**Case 1：申请 3 页**

```c
struct Page *p = alloc_pages(3);
```

期望：

- 分配成功时实际独占 4 页。
- 空闲页计数减少 4。
- 不能将第 4 页重新分配给其他请求。

随后：

```c
free_pages(p, 3);
```

应归还完整 4 页。

**Case 2：申请 5 页**

```c
struct Page *p = alloc_pages(5);
```

期望：

- 实际独占 8 页。
- 空闲页数量减少 8。

随后：

```c
free_pages(p, 5);
```

应恢复完整 8 页。

**Case 3：交替分配与释放**

- 混合申请 1、3、5、2、4 页。
- 以不同顺序完整释放。
- 验证所有页面能够正确归还。

## 6. 分配失败测试

**Pre-Condition**：

- 当前空闲页状态已知。
- 测试不会破坏已有活动分配。

**Post-Condition**：

- 无法满足请求时返回 `NULL`。
- 不丢失或重复登记任何页面。
- 所有已有活动分配保持有效。

**Case 1：申请超过当前总空闲容量的页面**

- 验证分配失败。
- 验证空闲页数量不变。

**Case 2：总空闲页数足够，但无法形成满足要求的连续块**

- 在受控的碎片状态下申请较大的 Buddy 块。
- 验证分配器不能错误地将不连续的空闲页当作一个连续块返回。
- 失败后其他合法小块仍可正常分配。

**Case 3：大请求与阶数边界**

- 测试超出分配器最大支持阶数的请求。
- 检查容量计算不会发生移位溢出、整数溢出或越界访问。

## 7. 非法操作与边界测试

**Pre-Condition**：

- 每项非法操作测试具备独立可恢复的测试环境。
- 明确区分正常资源不足与违反接口契约的非法调用。

**Post-Condition**：

- 非法操作得到明确诊断。
- 不得产生重复释放、非法合并或页面计数错误。
- 正常测试不因故意触发断言而中断。

**Case 1：零页申请**

- 检查 `alloc_pages(0)` 按实际约定拒绝非法请求。

**Case 2：重复释放**

- 对同一分配执行第二次释放。
- 验证不会使空闲页数被重复增加。

**Case 3：错误释放起点**

- 使用原分配块内部页面的描述符作为释放起点。
- 验证不能错误地释放整个块。

**Case 4：错误释放长度**

- 使用不匹配原始请求的 n。
- 验证不会回收不属于该次释放操作的页面。

**Case 5：非法登记**

- 测试重复、重叠和越界登记。
- 验证错误输入不能产生额外的可分配页面。

**Requirements**：

- 对预期触发 `assert()` 或 `panic()` 的测试，应使用隔离进程、独立 Host 测试实例或独立 QEMU 运行。
- 不得在正常内核 `buddy_check()` 中直接执行会导致系统 panic 的负面测试。
- 如果某项测试由于环境限制未能实际执行，应标记为未验证，不能记录为通过。

## 8. 空闲页计数及资源守恒测试

**Pre-Condition**：

- 已登记物理页集合与当前活动分配集合已知。

**Post-Condition**：

始终满足：

```text
已登记可管理页数
=
当前空闲页数
+
当前所有活动分配实际独占的页数
```

不允许因为拆分、合并或异常分配而改变资源总量。

**Case 1：拆分**

- 仅改变空闲块组织方式，不应凭空减少或增加可管理页面。

**Case 2：合并**

- 合并前后的空闲页总数不应变化。

**Case 3：分配与释放**

- 成功分配按实际容量减少空闲页数。
- 成功释放按实际容量增加空闲页数。
- 全部分配释放结束后恢复初始容量。

## 9. 压力测试

**Pre-Condition**：

- Buddy 模块已通过基本功能测试。
- 测试环境可记录当前活动分配和初始空闲容量。

**Post-Condition**：

- 不出现活动分配重叠。
- 不出现分配到 Reserved 页的情况。
- 不出现资源泄漏或重复回收。
- 空闲页统计与实际资源状态一致。

**Case 1：重复分配和释放**

- 执行多轮不同大小的分配与释放。
- 每次记录返回的 Page 指针、原始请求 n 和实际独占页数。
- 以不同顺序释放已分配块。

**Case 2：固定随机种子**

- 使用固定随机种子产生可复现的分配与释放序列。
- 建立独立的活动分配账本，逐次检查页面所有权和空闲容量。
- 不得仅通过再次运行 Buddy 的同一套逻辑作为正确性判定依据。

**Case 3：最终状态恢复**

- 将所有成功分配的块完整归还。
- 验证空闲页面数量与初始状态一致。
- 验证不存在遗漏的活动分配。
- 验证分配器仍能正常进行后续分配。

## 10. 测试隔离和内核状态恢复

**Requirements**：

- 正常内核中的 `buddy_check()` 不得重新初始化已经投入使用的全局 Buddy 管理器。
- 不得清空或覆盖其他模块正在使用的物理页。
- 真实 QEMU 内核测试结束后，临时分配必须全部释放。
- 测试不得依赖 First-Fit 或 Best-Fit 特有的返回地址顺序及私有空闲链表状态。
- 隔离测试若使用 Host 环境，应直接编译或调用实际 Buddy 源码，不能复制并修改一份替代实现后测试。
- 必须分别报告 Host 测试、RISC-V 编译和 QEMU 内核运行的结果。

## 11. 编译运行和结果报告

完成测试函数后执行：

**第一层：编译验证**

- 编译 Buddy 源码及新增测试代码。
- 检查类型、符号、宏和函数声明是否正确。

**第二层：独立测试**

- 实际执行需要特定初始化状态或非法输入的隔离测试。
- 保留每项测试真实结果。
- 不得把 Host 测试通过等同于 RISC-V 内核通过。

**第三层：QEMU 集成验证**

- 编译完整 uCore。
- 使用已经验证成功的 QEMU 启动方式运行内核。
- 确认当前 `pmm_manager` 为 Buddy。
- 确认专项测试在真实 RISC-V 内核中实际执行。
- 检查正常执行、无 panic、测试后资源恢复及后续内核行为。

运行输出应清楚区分基础检查和新增专项检查，只有全部相应断言通过后才输出成功信息。

**测试记录**：

请建立：

`D:\college_study_materials\junior_year_fall\OS\lab2\buddy_work\evidence\buddy_tests`

保存：

- 本次实际使用的原始 Prompt。
- 测试源码及新增函数说明。
- 编译和运行命令。
- 原始终端输出及退出状态。
- 各测试用例的通过、失败或未执行状态。
- 失败时的最小复现输入。
- Buddy 实现修改前后的差异与修复原因。
- 重新执行测试的结果。

同时生成 `BUDDY_TEST_REPORT.md`，按表格整理：

| 测试编号 | 测试功能 | 测试输入 | 预期结果 | 实际结果 | 状态 |
|---|---|---|---|---|---|

报告最后简要解释每一类测试为什么能够检验 Buddy System 的正确性，特别说明拆分、合并、非 2 的幂请求、异常输入和资源守恒测试分别可以发现哪些错误。

最终输出：

- STATUS：SUCCESS / PARTIAL / FAILED。
- 新增和修改的文件。
- 实际执行的测试数量。
- 通过、失败和未执行的测试数量。
- RISC-V 编译结果。
- QEMU 集成测试结果。
- 真实发现的 Bug 及修复。
- 尚未完成的验证项目。
- 测试日志和报告路径。

不得将尚未执行的测试记录为通过，不得通过固定成功输出替代真实断言。

---

## 3. SLUB_Implementation_Prompt

[PROMPT]

**任务**：在现有 RISC-V uCore Lab2 的物理内存管理框架上，从零实现一个简化版 SLUB 内存对象分配器，构建“物理页分配 + 页内对象分配”的两层内存管理架构。

第一层复用已有的 Buddy System 物理页分配器，通过 `alloc_pages()` 和 `free_pages()` 提供、回收完整物理页。

第二层新增 SLUB 对象分配器，将底层物理页组织成 Slab，并在其中分配、释放和复用不同大小的内存对象。

实现应体现 SLUB 的核心思想：对象大小分类、Slab 管理、空闲对象复用、按需获取物理页以及完全空闲页面的安全回收。允许简化 Linux SLUB 的具体机制，但必须具有真实的两层分配行为。

**工程目录**：

已完成并通过审计的 Buddy 工程：

`D:\college_study_materials\junior_year_fall\OS\lab2\buddy_work`

本次 SLUB 独立工作区：

`D:\college_study_materials\junior_year_fall\OS\lab2\slub_work`

**目标文件**：

- 新建 `kern/mm/slub.c`：SLUB 对象分配器实现。
- 新建 `kern/mm/slub.h`：SLUB 对外接口声明。
- 修改独立工作区的 `kern/init/init.c`：在物理内存管理器完成初始化后接入 SLUB，并执行基础功能检查。
- 如工程构建要求，允许对 `Makefile` 进行必要的增量配置，但不得改变原有内核启动或评分机制。

**操作要求**：

1. 必须在真实工程文件中实现代码，而不是只输出示例。
2. 先核实 `buddy_work` 中通过最终审计的实际代码和接口，再建立独立的 `slub_work`。不得将旧版 Lab2 ZIP 覆盖到已经完成的 Buddy 工程。
3. 不覆盖已有用户文件，不复制不必要的编译产物，不修改 `buddy_work`、共享 `code` 或 Lab1。
4. 保留已验证的 `buddy_pmm.c`、`buddy_pmm.h`、`page_init()`、First-Fit、Best-Fit、Buddy 专项测试和官方测试。
5. SLUB 应通过现有物理页分配接口使用底层内存，而不是重新实现一套物理页分配算法。
6. 不依赖当前内核尚未提供的 `malloc()`、`kmalloc()`、`new` 或其他动态对象分配服务。
7. 可以新增必要的辅助函数、模块内部静态变量、Slab 元数据和缓存管理结构。具体内部算法由你自主设计。
8. 本轮完成 SLUB 实现、内核接入及基础编译运行验证。完整专项测试将在第二份 Prompt 中单独设计。

**输出要求**：

严格依据以下 `[RELY]`、`[GUARANTEE]` 和 `[SPECIFICATION]` 完成任务。

所有实现应使用当前 uCore 的真实宏、数据结构和函数接口。对设计中的取舍，应解释选择原因及行为后果，不引入与简化 SLUB 无关的 Linux 内核复杂机制。

---

[RELY]

### 1. 已完成的物理页分配层

来源：`kern/mm/pmm.h`

```c
struct pmm_manager {
    const char *name;
    void (*init)(void);
    void (*init_memmap)(struct Page *base, size_t n);
    struct Page *(*alloc_pages)(size_t n);
    void (*free_pages)(struct Page *base, size_t n);
    size_t (*nr_free_pages)(void);
};
```

注：真实结构体还包含 `void (*check)(void)`，具体字段定义必须以工程实际 `pmm.h` 为准。

SLUB 可使用的现有公共接口：

```c
struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void);
```

`alloc_pages(n)` 的参数单位为物理页，返回 `struct Page *`，不是可直接存放对象内容的数据指针。

当前 Buddy System 已通过独立审计，其分配契约为：

- 物理页大小为 4096 字节。
- 成功分配时返回连续物理页对应的首描述符。
- 实际独占容量可向上取整为 2 的幂。
- 完整释放时使用原分配起点和原始请求页数。
- `free_pages(base,n)` 能归还该次实际独占的完整 Buddy 块。
- 不支持从一个活动 Buddy 分配块中间进行部分释放。

SLUB 必须遵守上述接口契约，不得直接假设 Buddy 总是恰好分配 n 页。

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

可以依赖现有 Page 描述符进行物理页管理和地址转换。

需要注意：

- Page 描述符保存物理页状态，不等于物理页的数据空间。
- `PG_reserved` 和 `PG_property` 已有物理页管理语义。
- 不得擅自覆盖 Buddy 用于记录分配状态、块大小和所有权的内部元数据。
- 对象级空闲状态应由 SLUB 自己管理，不能直接把对象状态混入 Buddy 的空闲页链表。

### 3. 页面大小和地址转换

来源：`kern/mm/mmu.h`、`kern/mm/pmm.h`、`kern/mm/memlayout.h`

```c
#define PGSIZE 4096
#define PGSHIFT 12
#define PPN(la) (((uintptr_t)(la)) >> PGSHIFT)
```

已有：

```c
static inline ppn_t page2ppn(struct Page *page);
static inline uintptr_t page2pa(struct Page *page);
static inline struct Page *pa2page(uintptr_t pa);
```

以及：

```c
extern struct Page *pages;
extern size_t npage;
extern const size_t nbase;
extern uint64_t va_pa_offset;
```

当前内核采用 Sv39 启动页表和内核线性映射。需要通过实际映射关系，将底层分配到的物理页转换为内核能够读写的数据地址。

特别注意：

- `struct Page *` 指向描述符，不是物理页的数据区域。
- `page2pa()` 得到物理页起始物理地址。
- SLUB 返回的对象指针必须指向真实可访问的内核虚拟地址。
- 当前 `KADDR` 宏在部分 Lab2 源码中处于注释状态，使用前必须核查。
- 地址转换必须满足当前内核有效映射范围，不能直接对任意物理地址加偏移后就认为可安全访问。
- 不得把 `PADDR(Page*)` 误认为被该 Page 描述的物理页地址。

### 4. 可使用的内核基础设施

包括：

- `libs/list.h`：双向链表及相关操作。
- `libs/string.h`：`memset()`、`memcpy()`、`memcmp()` 等基础函数。
- `kern/debug/assert.h`：断言。
- `panic()`：严重错误处理。
- `cprintf()`：输出初始化和测试信息。
- `kern/init/init.c`：现有内核启动和初始化入口。

必要的结构体、宏和辅助函数应从当前实际源文件读取，不能假设标准 C 库或 Linux 内核接口在此环境中全部可用。

### 5. SLUB 的功能背景

简化 SLUB 使用两层结构：

第一层：物理页分配层。

- 通过 `alloc_pages()` 获取物理页。
- 通过 `free_pages()` 归还不再需要的物理页。

第二层：内存对象分配层。

- 将获得的物理页组织为 Slab。
- 管理不同大小的对象槽位。
- 跟踪对象分配和释放状态。
- 优先复用已有空闲槽位。
- 必要时申请新的物理页。
- 在满足安全条件时归还完全空闲的 Slab。

参考 Linux SLUB 的基本原理即可，不要求实现 per-CPU 缓存、NUMA 策略、多核无锁优化等高级机制。

---

[GUARANTEE]

必须新建 `kern/mm/slub.c` 和 `kern/mm/slub.h`，并实现以下公开接口：

```c
void slub_init(void);

void *slub_alloc(size_t size);

void slub_free(void *ptr);

void slub_check(void);
```

其中：

- `slub_init()`：初始化 SLUB 对象管理器。
- `slub_alloc(size)`：申请至少 size 字节的连续可写对象空间。
- `slub_free(ptr)`：释放先前由 SLUB 返回的对象。
- `slub_check()`：执行基础功能检查；完整专项测试由第二份 Prompt 扩展。

允许根据需要新增静态辅助函数，例如对象大小分类、Slab 获取和释放、空闲对象管理、大对象分配记录和对象地址检查等。

这些辅助函数不需要采用特定名称，但应保持模块内部封装，不改变现有 `pmm_manager` 接口。

必须在独立工作区的 `kern_init()` 中，将 SLUB 初始化安排在 `pmm_init()` 成功完成之后。

不得将 SLUB 注册为 `pmm_manager`，因为 SLUB 不是替代 Buddy 的物理页分配器，而是建立在物理页分配器之上的对象分配层。

---

[SPECIFICATION]

## 1. slub_init(void)

**Pre-Condition**：

- 当前物理内存管理器已经完成初始化。
- `alloc_pages()` 和 `free_pages()` 可以正常工作。
- SLUB 尚未进入正式对象分配阶段。

**Post-Condition**：

- SLUB 的对象缓存、Slab 管理状态和必要元数据初始化完成。
- 尚未发生对象分配时，没有活动的 SLUB 对象。
- 后续能够安全使用 `slub_alloc()` 和 `slub_free()`。

**Case 1：正常初始化**

- 管理器能够正确建立用于不同对象大小的管理状态。
- 对象空闲与占用信息处于一致的初始状态。
- 不应无故损失可用物理页。

**Case 2：底层物理内存不足**

- 即使底层暂时没有足够的可用物理页，也不得破坏内核状态。
- 若采用延迟申请 Slab 的设计，应允许初始化成功，并在实际对象申请时处理内存不足。

**Requirements**：

- 不依赖尚未实现的动态内存分配服务。
- 不修改 Buddy 的内部管理状态或分配接口。
- 初始化必须发生在 PMM 初始化之后。
- 本次仅要求课程当前单 Hart、串行访问环境，不要求多核并发优化。

## 2. slub_alloc(size_t size)

**Pre-Condition**：

- SLUB 已完成初始化。
- size 表示请求对象的字节数。
- 对象申请所涉及的大小、容量及地址计算不得发生整数溢出。

**Post-Condition**：

- 成功时返回可供内核直接读写的对象内存指针。
- 返回空间至少包含 size 个连续有效字节。
- 对象内存符合适当的内存对齐要求，至少满足 16 字节对齐。
- 同时存活的不同对象，其可使用内存范围不得重叠。
- 不得返回指向 Page 描述符或 SLUB 私有元数据的错误指针。
- 内存不足或请求不可满足时返回 NULL，不破坏已有对象及分配状态。

**Case 1：现有 Slab 有可用槽位**

- 对应大小的 Slab 中存在可用对象槽位时，应优先利用已有内存。
- 不应无必要地额外申请新物理页。
- 分配成功后，对应槽位变为已占用状态。

**Case 2：现有 Slab 没有可用槽位**

- 当相应对象缓存没有可用槽位时，可以从第一层物理页管理器获取新的物理内存。
- 新增页面应被正确组织成可管理的 Slab。
- 已有对象和其他 Slab 状态不受影响。

**Case 3：不同大小的对象申请**

- 应支持不同的正整数大小请求，例如 1、24、60、100、256、500、1000 字节。
- 可以按照大小分级管理对象槽位。
- 如果使用内部槽位取整，返回空间必须能够容纳原始请求。
- 不得因为对象大小不是 2 的幂就错误拒绝合法申请。

**Case 4：对象大小超过单页小对象管理范围**

- 当申请大小不适合放入单页 Slab 时，必须提供合法的大对象分配路径。
- 例如 `slub_alloc(5000)` 应在存在足够连续物理内存时能够成功。
- 可以直接利用底层连续物理页分配器，但需要确保实际返回的对象区域足够大。
- 必须保存释放时需要的页面起点、原始页请求数量和对象所有权信息。
- 对超大请求允许因资源或表示范围不足返回 NULL，但不能在正常内存充足时仅因超过一个页面就无条件拒绝。

**Case 5：零字节申请**

- `slub_alloc(0)` 返回 NULL。
- 不得消耗对象槽位或物理页。

**Case 6：物理内存不足**

- 当没有足够的对象槽位，也无法获得新的合法物理页时，返回 NULL。
- 已经分配的其他对象必须保持有效。
- 不得产生部分初始化的活动 Slab、遗失页面或错误对象记录。

**Requirements**：

- 所有内存范围计算必须考虑对齐和元数据占用。
- 不得把整个页面的元数据占用区域错误地算入可写对象容量。
- 返回指针必须具有明确的所有权，能够在 `slub_free()` 中识别其来源。
- 不要求分配内容自动清零；若实现选择清零，必须保证不影响其他对象。
- 不允许使用固定容量的静态对象池冒充完整的动态 Slab 分配机制。

## 3. slub_free(void *ptr)

**Pre-Condition**：

- SLUB 已经初始化。
- 非 NULL 的合法参数应为先前由 `slub_alloc()` 返回、尚未释放的对象首地址。

**Post-Condition**：

- 合法对象被完整释放。
- 释放后的对象槽位能够被后续申请复用。
- 同一 Slab 内其他仍然有效的对象不受影响。
- 释放状态、空闲槽位统计和底层页面所有权保持一致。

**Case 1：释放普通小对象**

- 正确识别对象所属的 Slab 和槽位。
- 仅释放目标槽位。
- 不得因为释放一个小对象就错误归还仍包含活动对象的物理页。

**Case 2：释放后重新申请**

- 已释放的空闲槽位可以被后续兼容大小的对象申请复用。
- 不要求一定返回与之前完全相同的指针，但不能产生隐藏槽位泄漏。

**Case 3：Slab 内所有对象均已释放**

- 当一个 Slab 不再包含任何活动对象时，应能够将其占用的物理页安全归还第一层。
- 第一版采用完全空闲 Slab 及时归还页面的策略，避免无限保留空闲缓存页。
- 必须先完成 Slab 管理状态更新，再按正确接口归还页面，不得留下指向已归还页面的有效 Slab 记录。

**Case 4：释放大对象**

- 对通过多页直接分配路径获得的大对象，正确识别其原始页面分配信息。
- 通过 `free_pages()` 归还该次分配对应的完整物理内存。
- 当 Buddy 对实际独占页面数进行了 2 的幂取整时，必须遵守原始请求长度与释放长度的配对契约，避免页面泄漏。

**Case 5：空指针**

- `slub_free(NULL)` 安全返回。
- 不改变分配器状态。

**Case 6：非法释放**

包括：

- 重复释放。
- 非对象起始地址。
- 不属于 SLUB 的指针。
- 越界地址。
- 处于错误状态的 Slab 对象。

对于可识别的非法操作，应在破坏状态前拒绝并明确诊断。

不得错误释放其他活动对象或将同一物理页重复归还底层分配器。

**Requirements**：

- 对象释放不能仅通过把指针当作 `struct Page *` 处理。
- 必须区分 SLUB 对象地址、Slab 数据区域和物理页描述符。
- 不得在释放单个对象时清除仍在使用的整个物理页。
- 对非法指针的检查不得先无条件解引用任意地址而造成不可控内核异常。

## 4. slub_check(void)

**Pre-Condition**：

- SLUB 和底层物理页管理器均已初始化。
- 测试不覆盖或破坏其他内核模块正在使用的资源。

**Post-Condition**：

- 实际执行基础对象分配和释放检查。
- 基础检查完成后，不留下临时活动对象或页面泄漏。
- 检查失败时通过断言或明确错误信息报告。
- 只有实际检查通过后才能输出成功信息。

**Case 1：基础对象分配**

- 申请不同大小的小对象。
- 验证返回地址非 NULL、满足对齐要求。
- 验证可以在申请范围内实际写入和读回数据。

**Case 2：多个对象同时存在**

- 同时申请多个对象。
- 写入不同的数据模式。
- 验证对象数据互不干扰。

**Case 3：对象释放与复用**

- 释放其中一个对象。
- 验证其他活动对象仍然有效。
- 再次申请对象，验证分配器能够继续正常工作。

**Case 4：大于单页的申请**

- 申请超过 4096 字节的对象。
- 验证实际连续可写空间满足请求。
- 释放后确认没有永久页面泄漏。

**Case 5：状态恢复**

- 基础检查开始前记录可用物理页数。
- 所有临时对象完整释放后，确认底层可用物理页数恢复。
- 不允许硬编码具体空闲页数作为通用断言。

**Requirements**：

- 本轮只要求基础检查。
- 不提前代替第二份 Prompt 完成完整专项测试。
- 不使用固定成功输出冒充实际检查通过。

## 5. 模块级正确性要求

整个 SLUB 分配器必须始终满足：

1. 所有可分配对象空间均由已经合法获得的物理页提供。
2. 返回的对象指针指向有效、可读写的内核虚拟内存。
3. 不同活动对象的可用地址范围不能重叠。
4. 对象槽位与所属 Slab 的状态一致。
5. 释放单个对象不能影响其他活动对象。
6. 同一个槽位不能同时被两个活动申请拥有。
7. Slab 完全空闲后可安全回收其物理页。
8. 大对象释放必须能够正确归还原始底层页面分配。
9. 内存分配失败不得破坏其他活动对象。
10. 不得发生重复回收、非法地址转换或永久资源泄漏。
11. 管理元数据不得覆盖对象有效存储区域。
12. 所有容量、偏移和对齐计算必须防止整数溢出。
13. SLUB 不得改变 Buddy 的既有分配、释放及合并行为。
14. 不要求实现 Linux 完整 SLUB 的 per-CPU、NUMA 或多核无锁优化。

## 6. 编译、运行与交付要求

在 `slub_work` 中完成实现后：

首先编译 SLUB 模块，确保所有公开接口和内部辅助函数符合当前 uCore 的类型与构建约定。

然后完成内核集成：

- 保持已经验证的 Buddy 为第一层物理页分配器。
- 在 `pmm_init()` 完成之后执行 `slub_init()`。
- 在合法初始化顺序下执行 `slub_check()`。
- 不改变现有 Buddy 专项测试或官方评分行为。

在 QEMU 中真实运行并验证：

1. 内核正常启动。
2. Buddy 物理页管理器正常初始化。
3. SLUB 初始化完成。
4. 小对象分配和释放能够执行。
5. 大对象分配和释放能够执行。
6. 基础检查没有触发 panic 或断言失败。
7. 所有测试临时页面被正确回收。
8. 内核最终能够正常进入原有驻留状态。

QEMU 启动方式应参考此前 Buddy 工程已经验证成功的启动参数；不应再次使用已知会导致 OpenSBI 跳转错误的启动方式。

测试时不得固定假设某次启动拥有 31792 或 31801 个空闲页，而应根据当前实际运行状态检查资源恢复。

请保存：

- `slub.c` 和 `slub.h`。
- 内核接入相关代码改动。
- SLUB 模块设计说明。
- Slab 元数据和对象状态管理方式。
- 小对象与大对象的分配路径。
- 物理页到内核虚拟地址的转换过程。
- 真实编译命令与日志。
- 真实 QEMU 运行日志。
- 遇到的 Bug、原因及实际修复记录。
- 尚未验证的功能与限制。

生成 `SLUB_IMPLEMENTATION.md`，解释第一层与第二层的实际调用关系，并把主要函数的实现与本 SPEC 对应起来。

同时保留本次原始 Prompt，作为实验报告中 SLUB Challenge 的第一份 Prompt。

最终输出：

- STATUS：SUCCESS / PARTIAL / FAILED。
- 实际修改的文件。
- SLUB 的具体设计方案。
- 已实现的对象大小范围。
- 小对象和大对象的分配释放行为。
- 与 Buddy 的接口调用关系。
- 编译结果。
- QEMU 运行结果。
- 已验证和未验证项目。
- 真实发现的 Bug 及修复。
- 实验记录路径。

本轮只完成 SLUB 的实现、内核接入及基础验证。完整专项测试函数由第二份正式 Prompt 单独设计。

---

## 4. SLUB_Test_Prompt

[PROMPT]

**任务**：针对已经实现并接入 RISC-V uCore Lab2 内核的简化版 SLUB 内存对象分配器，设计、实现并运行完整的功能测试、边界测试和异常测试，验证两层内存分配架构的正确性。

测试必须覆盖 SLUB 对象的分配、释放、复用、Slab 扩容和回收、大对象处理、内存耗尽、非法操作以及长期运行下的资源守恒。

**工作目录**：

`D:\college_study_materials\junior_year_fall\OS\lab2\slub_work`

**目标文件**：

- `kern/mm/slub.c`：已有 SLUB 实现。
- `kern/mm/slub.h`：已有公开接口。
- `kern/mm/slub_checks.inc`：已有基础检查及测试入口。
- `kern/init/init.c`：已有 SLUB 初始化和测试接入。
- `tests/slub_host_tests.c`：新增隔离 Host 专项测试文件；若当前工程已有等效测试文件，应在保留原代码基础上扩展。
- 根据实际工程需要，允许新增测试辅助源文件和测试构建脚本。

**操作要求**：

1. 必须在真实工程中实现并运行测试，而不只是输出测试代码。
2. 先完整阅读现有 `slub.c`、`slub.h`、`slub_checks.inc`、`SLUB_IMPLEMENTATION.md` 及其实际调用关系，确认对象大小分类、元数据布局、Slab 生命周期和大对象处理方式。
3. 测试必须直接针对上一轮生成的生产版 SLUB 实现，不得复制算法后编写另一份简化分配器作为被测试对象。
4. 保留已有 SLUB 基础检查，在其基础上新增专项测试。
5. 优先通过公开接口验证分配、释放和内存行为。对于 Slab 状态、对象归属和内部不变量，可以增加只读测试辅助检查，不得通过测试代码改变算法状态。
6. 不修改已通过审计的 Buddy、First-Fit、Best-Fit、`page_init()` 和官方评分测试。
7. 不修改 `buddy_work`、共享 `code` 或 Lab1。
8. 测试发现错误时，先保留失败证据并定位根因。只有确认属于 SLUB 实现问题后，才能修改相应实现并重新运行全部回归测试。
9. 不得删除失败测试、放宽断言、伪造执行日志或硬编码测试成功信息。
10. 正常功能测试应能在真实 QEMU 内核中执行；需要特殊内存布局或故意触发断言的测试应使用隔离环境。

**输出要求**：

依据下面的 `[RELY]`、`[GUARANTEE]` 和 `[SPECIFICATION]` 完成测试设计、实际代码修改、编译运行、问题定位及实验记录。

每个测试用例必须明确输入条件、预期结果、实际执行结果和判定依据。

---

[RELY]

### 1. 已实现的 SLUB 分配器

当前工程已经提供：

```c
void slub_init(void);
void *slub_alloc(size_t size);
void slub_free(void *ptr);
void slub_check(void);
```

上一轮已经实现并验证的功能：

- 小对象采用 16～2048 字节大小类。
- 按需通过底层物理页分配器获取单页 Slab。
- 已有空闲槽位可被后续对象分配复用。
- 同一个 Slab 中可以存放多个对象。
- Slab 中所有对象释放后，立即归还底层物理页。
- 大对象使用连续物理页分配路径。
- 大对象释放时保存并使用原始页请求数量。
- `slub_alloc(5000)` 已通过基础测试。
- `slub_alloc(8200)` 已通过基础测试，其原始页请求为 3 页，Buddy 实际独占 4 页。
- 基础对象读写、隔离和页面回收已经在 QEMU 中运行成功。

以上是已有实验结果，不代表新的专项测试已经通过。

测试前应核对真实实现，不能仅根据上述摘要推断所有内部行为。

### 2. 底层物理页管理接口

来源：`kern/mm/pmm.h`

```c
struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void);
```

页面大小为：

```c
#define PGSIZE 4096
#define PGSHIFT 12
```

当前物理页分配器为已验证的 Buddy System。

Buddy 的主要约定：

- 物理页以 4096 字节为单位管理。
- `alloc_pages(n)` 返回连续物理页对应的首个 `struct Page *`。
- 非 2 的幂请求可以向上取整为最小合法 Buddy 块。
- 释放时使用原始请求长度。
- 仅支持完整配对释放，不支持部分释放。
- `nr_free_pages()` 返回实际空闲物理页数量。

SLUB 测试不得把对象指针直接当作 `struct Page *` 使用。

### 3. Page 描述符和地址转换

来源：`kern/mm/memlayout.h`、`kern/mm/pmm.h`

```c
struct Page {
    int ref;
    uint64_t flags;
    unsigned int property;
    list_entry_t page_link;
};
```

现有物理页号、物理地址与内核虚拟地址转换接口应以源码为准。

测试中必须区分：

- Page 描述符地址。
- 物理页的物理地址。
- Slab 的内核虚拟地址。
- SLUB 返回的对象首地址。

只有合法映射、属于当前有效分配的对象内存，才可以进行读写测试。

### 4. 已完成的内核集成

当前 `kern/init/init.c` 已经在 PMM 初始化后调用 SLUB 初始化和基础检查。

上一轮报告的基础验证结果：

- SLUB 编译成功。
- 完整内核编译成功。
- Buddy 原有检查 5/5 通过。
- 小对象分配、读写及复用成功。
- 5000、8200 字节大对象分配及回收成功。
- 基础检查结束后恢复 31726 个空闲物理页。
- GDB 确认无 panic，活动对象及 Slab 清零。

上述空闲页数量受内核镜像大小和静态数据布局影响，不能作为新增测试的固定预期。

应记录测试开始时的实际空闲页数量，再判断测试结束后是否恢复。

### 5. 测试可用资源

可以使用：

- C 语言 `assert()` 和内核已有断言接口。
- `panic()`、`cprintf()`。
- `memset()`、`memcpy()`、`memcmp()`。
- 当前 uCore 的 Page 与地址转换接口。
- 现有 Buddy 的资源查询接口。
- Host 测试环境下的 ASan、UBSan。
- 已成功使用的 RISC-V 交叉编译器及 QEMU。

Host 测试允许建立必要的物理页和地址映射适配层，但必须调用实际 `slub.c`，不得使用替代分配算法。

---

[GUARANTEE]

必须保留并完善现有公开测试入口：

```c
void slub_check(void);
```

允许根据需要新增：

- 小对象分配测试函数。
- 大小类边界测试函数。
- 多对象数据隔离测试函数。
- Slab 扩容及回收测试函数。
- 大对象分配及释放测试函数。
- 资源耗尽测试函数。
- 非法操作隔离测试函数。
- 状态一致性检查函数。
- 固定随机种子的压力测试函数。
- Host 专项测试入口和必要的辅助函数。

这些内部测试函数可以使用 `static` 限定，不需要修改 SLUB 的公开分配接口。

必须覆盖以下测试类别：

1. 不同对象大小的分配与读写。
2. 对象大小类边界。
3. 多个对象同时存在时的数据隔离。
4. 同一 Slab 中的对象槽位复用。
5. Slab 容量耗尽后的扩容。
6. 完全空闲 Slab 的物理页回收。
7. 大于单页的对象申请和完整释放。
8. 对齐要求及对象地址合法性。
9. 分配失败和内存耗尽。
10. 非法指针、重复释放等错误操作。
11. Slab 状态转换及页面资源守恒。
12. 多轮对象分配与释放压力测试。
13. Buddy 和 SLUB 的集成回归。

测试结束后，必须生成逐项结果报告及完整运行证据。

---

[SPECIFICATION]

## 1. 不同大小对象的分配测试

**Pre-Condition**：

- SLUB 已初始化。
- 底层物理页分配器处于有效状态。
- 存在足够的可用物理页。

**Post-Condition**：

- 每次成功申请都返回有效对象指针。
- 对象可使用空间至少达到请求字节数。
- 返回地址满足实现约定的对齐要求，至少满足 16 字节对齐。
- 对象内容能够被正确写入和读取。
- 所有对象释放后资源能够恢复。

**Case 1：小对象**

至少测试以下申请大小：

```text
1, 8, 15, 16,
17, 24, 31, 32,
33, 60, 63, 64,
65, 100, 127, 128,
129, 255, 256, 257,
511, 512, 513,
1023, 1024, 1025,
2047, 2048
```

检查：

- 返回指针有效。
- 内存对齐正确。
- 可以在请求范围内写入并读回数据。
- 没有越界、对象重叠或内部状态错误。

**Case 2：大小类边界**

重点检查 16、32、64、128、256、512、1024、2048 字节及其前后相邻大小。

验证对象被安排到能够容纳请求的合法槽位，不能因为大小类计算错误而分配不足的空间。

**Case 3：零字节申请**

```c
slub_alloc(0);
```

应返回 NULL，不消耗物理页或对象资源。

## 2. 多对象隔离测试

**Pre-Condition**：

- SLUB 具有能够分配多个对象的可用资源。

**Post-Condition**：

- 同时存活的不同对象，其有效内存范围互不重叠。
- 修改一个对象不影响其他对象的数据。

**Case 1：同一大小类中的多个对象**

连续分配多个 100 字节对象。

分别写入不同的数据模式，并在全部对象存活期间检查内容。

**Case 2：不同大小类中的对象**

同时申请 24、100、256、500 和 1000 字节对象。

每个对象写入与其他对象不同的数据模式，验证全部对象内容保持正确。

**Case 3：释放其中一个对象**

释放中间对象后，继续读写其余活动对象。

不得出现其他对象数据被覆盖或提前释放的情况。

**Requirements**：

- 不通过读取已释放对象的内容判断正确性。
- 对象不重叠检查应基于实际申请长度及有效指针范围，避免整数溢出。
- 不能仅凭返回地址不同就认定对象之间不存在重叠。

## 3. 对象槽位复用测试

**Pre-Condition**：

- 已有合法 Slab。
- 测试能够控制若干相同大小对象的分配和释放。

**Post-Condition**：

- 已释放槽位可以重新利用。
- 不产生永久槽位泄漏。
- 不影响其他活动对象。

**Case 1：部分槽位释放**

在同一个大小类中连续申请多个对象。

释放其中一部分对象，但保留其他活动对象。

再次申请相同大小对象，验证已有空闲容量被复用，不无故向 Buddy 申请新的物理页。

**Case 2：重复分配与释放**

反复申请和释放同一大小类对象。

检查对象状态、空闲槽位管理和实际底层页面数量是否一致。

**Case 3：不同大小类隔离**

释放某一大小类的对象，不得错误改变其他大小类的活动对象或空闲槽位状态。

## 4. Slab 扩容测试

**Pre-Condition**：

- 存在一个已经使用部分槽位的 Slab。
- 底层 Buddy 可以提供新的物理页。

**Post-Condition**：

- 当现有 Slab 容量耗尽后，新的合法 Slab 可以被建立。
- 已有对象数据不受影响。
- 新增的物理页与旧 Slab 不发生错误重叠。

**Case 1：填满一个 Slab**

选择固定对象大小，连续申请直到当前 Slab 槽位耗尽。

检查容量、槽位占用数量和页面归属关系。

**Case 2：继续申请**

在第一个 Slab 已满时继续申请相同大小对象。

验证 SLUB 能够按需获得新的物理页并继续服务请求。

**Case 3：多个 Slab 同时存在**

同时保留至少两个 Slab 中的活动对象，验证：

- 对象地址互不重叠。
- 对象数据互不干扰。
- 底层物理页所有权明确。
- 不发生重复分配。

## 5. 完全空闲 Slab 回收测试

**Pre-Condition**：

- 至少存在一个包含活动对象的 Slab。
- SLUB 当前采用“完全空闲后立即归还页面”的策略。

**Post-Condition**：

- Slab 中所有对象释放后，该 Slab 不再占用底层物理页。
- 归还的物理页能够重新被 Buddy 使用。
- 不留下指向已释放 Slab 的有效管理记录。

**Case 1：只释放部分对象**

不能归还仍包含活动对象的物理页。

**Case 2：释放最后一个对象**

应回收完全空闲的 Slab 页面，并更新 SLUB 内部状态。

**Case 3：回收后重新申请**

重新申请相同大小的对象，验证可以正常获取新的 Slab 并执行分配。

**Case 4：不同 Slab 的独立回收**

一个 Slab 完全空闲时，不应影响其他仍包含活动对象的 Slab。

**Requirements**：

- 验证页面回收不能只检查空闲页数变化，还应核实相关 Slab 状态与页面所有权。
- 测试过程中产生的所有对象必须最终完整释放。

## 6. 大对象分配与释放测试

**Pre-Condition**：

- SLUB 已初始化。
- 底层物理页分配器拥有足够的连续物理页。

**Post-Condition**：

- 大对象获得不少于申请大小的连续可写空间。
- 大对象地址满足对齐要求。
- 大对象释放后，其实际占用的全部物理页得到回收。
- 不影响其他活动对象。

**Case 1：大对象大小边界**

至少测试：

```text
2049, 3000, 4095, 4096,
4097, 5000, 8192, 8193, 8200
```

验证每次分配使用足够的底层物理页，能够写入和读取完整请求范围。

**Case 2：5000 字节申请**

验证超过一页的对象能够使用完整连续空间，且正确释放。

**Case 3：8200 字节申请**

该请求需要至少 3 个 4096 字节物理页。

当前 Buddy 对 3 页请求实际独占 4 页。

检查：

- SLUB 保留正确的原始页请求数量。
- 不会错误地只分配两页。
- 释放时传入与分配契约一致的原始页数。
- Buddy 实际独占的完整 4 页能够回收。

**Case 4：大对象与小对象混合分配**

同时保留大对象和不同大小的小对象，验证各自内容及生命周期独立。

**Case 5：接近容量或整数上限的请求**

针对可能产生乘法、加法或向上取整溢出的极大请求，例如 `SIZE_MAX`，验证安全失败，不发生非法地址访问或资源状态损坏。

## 7. 内存不足测试

**Pre-Condition**：

- 使用隔离的可控物理页资源环境。
- 已记录测试前的资源状态。

**Post-Condition**：

- 无法满足申请时返回 NULL。
- 之前成功分配的对象仍然有效。
- 不遗留部分初始化的 Slab 或泄漏页面。

**Case 1：小对象连续申请直至耗尽**

连续申请同一大小类对象，直到不能获取新的 Slab。

检查失败前的所有活动对象仍有效，并能在释放后恢复资源。

**Case 2：大对象申请失败**

申请超过当前实际可用连续物理页容量的大对象。

验证安全失败，不得发生容量溢出或越界分配。

**Case 3：失败后恢复**

释放先前成功分配的对象，再尝试合法分配。

验证 SLUB 在资源不足后仍能继续正常工作。

**Requirements**：

- 不允许为了制造内存不足而耗尽正常内核运行所必需的页面。
- 资源耗尽场景优先放在隔离 Host 测试环境中。

## 8. 非法释放测试

**Pre-Condition**：

- 每个非法输入测试在独立、可恢复的测试环境中运行。
- 明确当前 SLUB 对非法操作的诊断契约。

**Post-Condition**：

- 非法操作被明确拒绝。
- 不得释放其他活动对象。
- 不得重复归还底层物理页。
- 不得破坏 Slab 空闲状态和对象所有权。

**Case 1：空指针释放**

```c
slub_free(NULL);
```

应安全返回且不改变状态。

**Case 2：重复释放**

同一对象释放两次。

验证第二次释放触发预期诊断，不造成重复回收。

**Case 3：非对象首地址**

例如对已分配对象首地址加一个字节后调用释放函数。

验证不能把内部地址错误识别为合法对象起点。

**Case 4：不属于 SLUB 的指针**

使用有效但不属于 SLUB 分配对象的地址，验证能够按照实现契约拒绝。

**Case 5：已释放 Slab 中的旧对象指针**

在所有对象释放并导致 Slab 回收后，尝试再次释放原来的旧指针。

不得重复归还底层页面。

**Requirements**：

- 预期触发 panic 或 assert 的测试必须独立执行。
- 应验证实际失败原因与预期非法操作一致，不能将任意崩溃算作通过。
- 对终止进程的断言测试，不能声称已在该进程终止后观察到未改变的内部状态；可通过终止前状态证据及独立后续测试验证。
- 不得在正常 QEMU 内核检查中直接执行可能使内核永久 panic 的非法操作。

## 9. SLUB 状态与资源守恒测试

**Pre-Condition**：

- 测试环境具有可观察的底层物理页空闲数量。
- SLUB 的活动对象、Slab 和直接大对象分配信息能够被测试记录。

**Post-Condition**：

- 当前所有活动对象具有唯一合法所有权。
- Slab 的对象占用记录与实际对象集合一致。
- 完成全部释放后，无永久资源泄漏。

**Case 1：对象分配**

检查成功分配后，对象和 Slab 管理状态正确变化。

**Case 2：对象释放**

检查对应槽位、Slab 状态及对象所有权正确变化。

**Case 3：Slab 回收**

检查完全空闲 Slab 的页面正确归还底层分配器。

**Case 4：全部对象释放**

记录测试开始前的空闲页数量 F0。

所有临时对象释放结束后，要求：

```text
nr_free_pages() == F0
```

且：

- 没有测试遗留活动对象。
- 没有测试遗留活动 Slab。
- 没有遗留大对象分配记录。
- 相关链表及管理状态一致。

**Requirements**：

- 不得把 31726 页等历史数值写死为通用预期。
- 必须考虑 Buddy 的 2 的幂容量取整。
- 对资源消耗的检查应区分 Slab 数据页、元数据开销和大对象实际独占页。
- 不得用 SLUB 自己返回的统计数据作为唯一正确性依据，应使用独立活动对象记录和底层资源状态交叉核对。

## 10. 压力测试

**Pre-Condition**：

- 基础功能及边界测试已经通过。
- 使用可以复现的测试环境和固定随机种子。

**Post-Condition**：

- 不发生对象地址重叠。
- 不发生错误对象复用。
- 不出现永久页面泄漏。
- 所有仍存活对象的数据保持正确。
- 全部合法分配完整释放后，资源恢复初始状态。

**Case 1：混合大小申请**

随机产生不同大小的小对象和大对象请求。

记录每次成功分配的：

- 对象地址。
- 请求大小。
- 活动状态。
- 数据校验信息。

**Case 2：随机释放**

从活动对象集合中随机选择对象释放。

不得对已经释放的对象再次执行正常释放。

**Case 3：数据完整性**

每轮操作后，对抽样或全部仍存活对象检查其数据模式。

验证其他对象的分配与释放没有破坏当前对象。

**Case 4：多种随机种子**

至少执行两组固定随机种子的测试，每组建议不少于 10000 次分配或释放操作。

根据当前实际测试运行能力合理限制活动对象数量和内存上限，避免将测试环境本身耗尽或超时误判为算法失败。

**Case 5：最终恢复**

释放所有仍然存活的对象。

检查对象账本为空、SLUB 状态一致，底层空闲物理页数量恢复至测试开始时的水平。

**Requirements**：

- 使用独立活动对象账本记录期望所有权。
- 不能使用 SLUB 内部元数据直接生成全部测试预期。
- 保留随机种子和实际执行步数，确保失败能够复现。
- 启用 ASan、UBSan 的 Host 测试应保留真实构建参数和运行日志。

## 11. Buddy 与 SLUB 集成回归

**Pre-Condition**：

- 当前完整 uCore 使用已通过审计的 Buddy 作为物理页分配器。
- SLUB 已在 PMM 初始化之后接入。

**Post-Condition**：

- 新增 SLUB 测试不破坏原 Buddy 行为。
- 真实 QEMU 中仍能执行物理页分配与回收。
- SLUB 小对象和大对象测试完成后，内核继续正常运行。

**Case 1：Buddy 既有检查**

- 保留并重新执行此前的 Buddy 基础及内核专项检查。
- 不修改 Buddy 实现或测试以配合 SLUB。

**Case 2：SLUB 内核专项检查**

- 在 QEMU 中实际执行小对象、大对象、对象隔离、Slab 回收和资源恢复检查。
- 检查实际写读数据，而不是只检查返回指针非 NULL。

**Case 3：检查结束后继续运行**

- 通过 GDB 或已有内核运行证据确认内核未发生 panic。
- SLUB 未遗留测试对象、活动 Slab 或未释放的大对象记录。
- 系统能够正常进入原有驻留状态。

## 12. 测试执行与环境隔离

采用三级验证：

**第一层：RISC-V 编译检查**

- 编译 SLUB 源码及测试代码。
- 编译并链接完整内核。
- 检查所有测试入口实际包含在构建结果中。

**第二层：Host 隔离专项测试**

- 使用实际 SLUB 生产源码。
- 允许提供必要的平台适配层，但不能替换 SLUB 算法。
- 启用 AddressSanitizer 和 UndefinedBehaviorSanitizer。
- 运行对象隔离、耗尽、非法释放、大小边界及随机压力测试。

**第三层：真实 QEMU 内核测试**

- 使用当前已验证的内核启动方式。
- 确保 Buddy 和 SLUB 均真实运行。
- 执行不会故意造成 panic 的正常专项测试。
- 检查测试成功、状态恢复及内核后续运行状态。

测试不得随意清空已经投入使用的全局 Buddy 或 SLUB 管理状态。

需要重置初始化状态的测试，应在独立隔离实例中执行。

Host 测试结果和 QEMU 结果必须分别报告，不能以 Host 测试通过代替 RISC-V 内核验证。

## 13. Bug 处理和回归要求

任何测试失败时：

1. 保存原始失败日志。
2. 记录输入、随机种子和触发操作。
3. 提供最小可复现条件。
4. 判断是测试错误、环境问题、平台适配问题还是 SLUB 实现错误。
5. 如果属于 SLUB 实现错误，说明违反了哪条 SPEC。
6. 只修改必要的 SLUB 实现代码。
7. 保留修改前后的代码差异。
8. 重新执行全部相关测试及原 Buddy 回归测试。
9. 不得删除失败用例或降低断言严格程度。

未发现 Bug 时如实记录，不得编造调试过程。

## 14. 实验记录与最终交付

测试代码和实验记录保存在：

`D:\college_study_materials\junior_year_fall\OS\lab2\slub_work`

建立：

`evidence/slub_tests/`

保留：

- 本次实际使用的原始 Prompt。
- 新增测试源代码。
- 真实执行的编译命令。
- Host 测试及 ASan、UBSan 日志。
- QEMU 内核运行日志。
- GDB 验证记录。
- 测试用例清单。
- 失败及修复记录。
- 最终资源恢复证据。
- 实现代码修改前后的差异。

生成：

`SLUB_TEST_REPORT.md`

至少包含以下内容：

1. 测试环境及真实执行命令。
2. 各测试函数的设计目的。
3. 各测试输入与预期结果。
4. 实际通过、失败和未执行的项目。
5. 小对象和大对象的测试覆盖情况。
6. Slab 扩容、复用与回收的验证证据。
7. 异常操作的隔离测试结果。
8. 压力测试种子、执行步数和状态检查结果。
9. 实际发现的 Bug 及修复。
10. Buddy 和 SLUB 集成运行结果。
11. 尚未覆盖的边界及原因。
12. 为什么这些测试能够有效检验 SLUB 的正确性。

测试汇总格式：

| 编号 | 测试功能 | 输入条件 | 预期行为 | 实际结果 | 状态 |
|---|---|---|---|---|---|

不得把计划执行但尚未运行的测试计入通过数量。

最终输出：

- STATUS：SUCCESS / PARTIAL / FAILED。
- 新增和修改的文件。
- Host 实际测试数量与通过数。
- 非法操作测试数量与结果。
- RISC-V 编译结果。
- QEMU 实际测试数量与通过数。
- ASan、UBSan 结果及适用范围。
- 资源恢复检查结果。
- 实际发现及修复的 Bug。
- 尚未完成的测试项目。
- `SLUB_TEST_REPORT.md` 与日志路径。

本轮只完成 SLUB Challenge 的第二份正式测试 Prompt，不开展与该任务无关的功能开发。

---
