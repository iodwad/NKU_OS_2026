# 操作系统实验报告

## 实验基本信息

| 项目 | 内容 |
|---|---|
| 实验名称 | Lab4:进程管理 |
| 小组成员 | 张宸笛号—2412565、朱泽帅—2411560、马梓涵—2413902 |
| 实验实现完成日期 | |
| 实验报告更新日期 | 2026-11-18 |

### 小组分工

实验如何分工：

| 成员 | 负责的练习/模块 |
|---|---|
| 马梓涵—2413902（A） | 页映射建立与删除：`page_insert()`、`page_remove()`；线程与页表切换：`proc_run()`；分页 Challenge：Sv32/Sv39/Sv48 比较、`get_pte()` 相似代码及查找与分配是否拆分。 |
| 朱泽帅—2411560（B） | PCB 初始化与首次运行现场：`alloc_proc()`、`proc_init()`、`kernel_thread()`、`copy_thread()`、`copy_mm()`。 |
| 张宸笛号—2412565（C） | 线程创建、调度与汇编切换：`do_fork()`、`cpu_idle()`、`schedule()`、`switch_to()`；中断 Challenge：`local_intr_save()`、`local_intr_restore()` 的原理。 |

前置代码迁移、接口核对、工程合并与完整流程验证由三人合作。

实验报告如何分工：

| 报告部分 | 负责人 |
|---|---|
| 实验基本信息、第一至第三部分 | 马梓涵—2413902 |
| 4.1 系统初始化与虚拟内存页操作 | 马梓涵—2413902 |
| PCB 初始化、内核线程创建入口与首次运行现场（B 部分） | 朱泽帅—2411560 |
| `do_fork()`、空闲循环、调度器与 `switch_to()`（C 部分） | 张宸笛号—2412565 |
| `proc_run()` 与分页 Challenge | 马梓涵—2413902 |
| 中断 Challenge | 张宸笛号—2412565 |
| 测试与验证、知识点总结、全文整合与检查 | 三人合作 |

---

## 一、实验目的

本实验在 Lab2 的物理内存管理和 Lab3 的中断、异常处理基础上，继续实现页映射操作、内核线程创建与基本调度，让内核能够创建新的线程，并切换到线程的入口函数执行。具体目标如下：

1. **理解虚拟内存管理的基本结构。** 了解 `mm_struct`、`vma_struct` 与页表的关系，理解虚拟地址范围的描述和实际页映射之间的区别，以及内核线程共享内核地址空间的方式。
2. **掌握页映射的建立与删除。** 实现 `page_insert()` 和 `page_remove()`，正确处理首次映射、同页更新、异页替换及重复删除，保持页表项、物理页引用计数和 TLB 状态一致。
3. **理解内核线程的创建过程。** 掌握进程控制块 `proc_struct` 的主要字段，完成 PCB 初始化、内核栈分配、PID 分配和线程登记，理解 `idleproc` 与 `initproc` 的不同创建方式。
4. **掌握线程首次运行和上下文切换。** 区分 `context` 与 `trapframe`，理解新线程如何通过预先构造的现场进入入口函数，以及 `switch_to()` 如何保存和恢复寄存器。
5. **理解基本调度和中断保护。** 完成 `cpu_idle()`、`schedule()` 与 `proc_run()`，从就绪线程中选择下一执行对象，并在切换过程中保护 `current`、页表和寄存器现场的一致性。

---

## 二、实验环境

实验使用 Windows 下的 WSL2 Ubuntu 22.04 环境，面向 RISC-V 64 位架构进行交叉编译。主要工具包括 RISC-V GCC 10.2.0、GNU Make 和 QEMU，使用 QEMU 的 `virt` 机器及 OpenSBI 固件运行内核，代码通过 VS Code 查阅和编辑，并通过 Git、GitHub 管理版本。页表采用 Sv39 分页模式。

### AI 工具使用情况

| 成员 | AI 编程工具 | 底层模型 | 备注 |
|---|---|---|---|
| 张宸笛号—2412565 | ChatGPT | GPT-6.1 | |
| 朱泽帅—2411560 | ChatGPT | GPT-6.1 | |
| 马梓涵—2413902 | ChatGPT | GPT-6.1 | |

---

## 三、实验整体逻辑分析

### 3.1 本章节的逻辑主线

我们把这次实验的主要过程理解为：**先准备好线程要用的内存和运行现场，再把线程创建出来，最后让 CPU 切换过去执行。** Lab2 解决了物理页怎么分配和回收的问题，Lab3 解决了中断、异常发生后如何保存和恢复现场的问题。到了 Lab4，我们需要把这些功能接起来，让内核能够创建和管理线程，并在不同线程之间切换。

从代码上看，这次实验分成三个模块。第一个模块处理页映射，决定虚拟地址对应哪一页物理内存；第二个模块创建线程，准备 PCB、内核栈和第一次运行时的现场；第三个模块负责调度和切换，让选中的线程真正执行起来。我们觉得这里需要分清“创建”和“运行”：创建完成只是把线程需要的东西准备好了，之后还要等调度器选中它、恢复它的现场，它才会开始执行。

本实验创建的是内核线程。它们共用内核地址空间，但各有自己的 PCB、内核栈和寄存器现场。内核线程的 `mm` 为 `NULL`，是因为这里不需要单独管理用户地址空间，访问内存时仍然使用内核页表。理解虚拟内存结构时，我们把 `mm_struct` 看成一组虚拟内存区域的管理信息，`vma_struct` 则记录其中一段地址的范围和属性；至于某个虚拟地址最终对应哪一页物理内存，还要由页表项来决定。本次实验先完成这些基本操作，用户进程的地址空间复制、按需分页和换页还没有涉及。

结合代码，各部分的作用可以整理为：

| 部分 | 关键文件/接口 | 主要作用 |
|---|---|---|
| 前置功能与初始化 | `kern/init/init.c`、Lab2/Lab3 对应实现 | 准备物理内存管理、中断和异常处理环境 |
| 虚拟内存结构 | `kern/mm/vmm.c`、`kern/mm/vmm.h` | 管理虚拟内存区域及其组织关系 |
| 页映射操作 | `kern/mm/pmm.c`、`page_insert()`、`page_remove()` | 建立、更新和删除映射，维护引用计数并刷新 TLB |
| 线程创建 | `kern/process/proc.c`、`kern/process/proc.h` | 初始化 PCB，分配内核栈，准备现场并登记线程 |
| 调度选择 | `cpu_idle()`、`kern/schedule/sched.c` | 检查调度需求，从就绪线程中选择下一执行对象 |
| 实际切换 | `proc_run()`、`kern/process/switch.S` | 更新当前线程，切换页表并保存、恢复上下文 |
| 首次运行入口 | `forkret()`、`forkrets`、`__trapret`、`kernel_thread_entry` | 利用构造好的现场，让新线程第一次进入指定函数 |

把这些函数按调用顺序连起来，就比较容易看清整个过程。下面是相关函数补全后的主要执行流程：

```text
内核初始化：
kern_init()
  → pmm_init()：初始化物理内存管理，检查页映射
  → pic_init()、idt_init()：准备中断和异常处理
  → vmm_init()：初始化并检查虚拟内存管理结构
  → proc_init()：建立 idleproc，创建 initproc
  → clock_init()、intr_enable()：设置时钟并打开中断
  → cpu_idle()：进入空闲线程循环

创建 initproc：
kernel_thread(init_main, 参数, 标志)
  → 构造临时 trapframe
  → do_fork()
      → alloc_proc()：分配并初始化 PCB
      → setup_kstack()：分配内核栈
      → copy_mm()：处理地址空间关系
      → copy_thread()：准备新线程的 trapframe 和 context
      → 分配 PID，登记线程，wakeup_proc() 置为就绪

调度并首次运行 initproc：
cpu_idle() → schedule() → proc_run() → switch_to()
  → forkret() → forkrets → __trapret
  → RESTORE_ALL → sret → kernel_thread_entry
  → init_main(参数)
```

这里的 `context` 和 `trapframe` 都保存现场，但用途不一样。`context` 用在线程切换时，保存 `ra`、`sp`、`s0`～`s11` 共 14 个寄存器，让线程以后还能从暂停的位置继续执行。`trapframe` 保存的是 Trap 恢复需要的通用寄存器和相关 CSR，本实验也利用它安排新线程第一次运行的状态。线程切换发生在函数调用的位置，调用者保存寄存器中还需要用到的值，会由编译器按照调用约定处理，所以 `switch_to()` 不用像 Trap 入口那样保存一整套寄存器。

### 3.2 功能的逐步实现

1. **先把前两次实验的代码接进来。** Lab4 还需要用到物理内存初始化、Best-Fit 分配、中断与异常处理以及 Trap 恢复，因此要先把它们补到框架中标有 `lab2`、`lab3` 的位置。这里需要对比新旧代码，只补缺少的部分，保留 Lab4 中的中断保护和 `forkrets` 入口。如果直接拿旧文件覆盖，就可能把这次实验新增的内容一起覆盖掉。原来的注释和学号标记也要保留，方便后面检查。

2. **完成页映射的建立和删除。** `page_insert()` 先调用 `get_pte()` 找到要修改的页表项，缺少中间级页表时再申请页表页，然后设置物理页号和权限。这里要分三种情况：原来没有映射，就给目标页增加一次引用；原来映射的就是这一页，只更新权限，引用计数不变；原来映射到另一页，就减少旧页的引用，必要时释放旧页，再换成新页。如果申请页表页失败，要返回 `-E_NO_MEM`，不能继续写映射。`page_remove()` 则只查找已有映射，找不到就直接返回，找到后清除页表项并减少引用，引用为零时释放物理页。修改映射后还要让对应的 TLB 缓存失效，否则 CPU 可能仍然按旧地址转换结果访问内存。

3. **用 PCB 记录每个线程的信息。** 创建线程时，先通过 `alloc_proc()` 分配并初始化 `proc_struct`。其中，`pid` 是线程编号，`state` 是线程状态，`kstack` 记录内核栈位置，`context` 和 `tf` 保存两类现场，`pgdir` 保存根页表的物理地址。系统用 `current` 记录当前运行的线程，通过 `proc_list` 遍历线程，也可以利用哈希表按 PID 查找。刚分配的 PCB 还不能运行，要等栈和现场等内容准备好，再通过 `wakeup_proc()` 设为 `PROC_RUNNABLE`。这个状态既包括等待 CPU 的线程，也包括正在执行的线程，当前实际运行的是 `current` 指向的那个。

4. **弄清楚 idleproc 和 initproc 是怎么来的。** `proc_init()` 先初始化进程链表和哈希表，再把内核启动时已经在执行的这段程序作为 `idleproc`，设置 PID 为 0，继续使用启动栈，并让 `current` 指向它。所以 `idleproc` 是把已有的执行过程纳入线程管理。接下来才通过 `kernel_thread()` 创建 PID 为 1 的 `initproc`。这个函数先准备好入口函数和参数，再调用 `do_fork()` 分配 PCB、内核栈，设置现场和 PID，最后把线程加入管理结构。本实验只创建内核线程，`copy_mm()` 暂时不复制用户地址空间，各线程使用内核页表。`initproc` 的父线程是创建它的 `idleproc`。如果中途分配失败，已经申请的资源也要按相反顺序释放。

5. **给新线程准备第一次运行的现场。** 新线程还没有执行过，自然也没有以前保存的现场可以恢复，所以要由代码提前准备。`copy_thread()` 把 Trap frame 放到新线程自己的内核栈上，让 `context.ra` 指向 `forkret()`，并设置好 `context.sp`。这样，第一次切换到新线程时，就会先进入 `forkret()`，再经过 `forkrets`、`__trapret` 恢复 Trap frame。由于里面的 `epc` 已经设为 `kernel_thread_entry`，执行 `sret` 后就能到达这个入口。入口把 `s1` 里的参数放到 `a0`，再调用 `s0` 中记录的函数。我们理解这一步的关键是：提前填好现场，就能借用已有的恢复流程，让一个从未运行过的线程开始执行，不需要真的先发生一次中断，运行时仍处于 S 模式。

6. **选出下一个线程，再把 CPU 切换过去。** `cpu_idle()` 不断检查 `need_resched`，发现需要调度时就调用 `schedule()`。`schedule()` 清除当前线程的调度标记，再沿着进程链表查找下一个就绪线程；没有合适的线程时，就选 `idleproc`。选好后，`proc_run()` 保存原来的中断状态并关闭本核中断，记住旧线程，更新 `current`，然后把目标线程的页表信息按 Sv39 格式写入 `satp`，刷新 TLB，最后调用 `switch_to()` 保存旧现场、恢复新现场。汇编读写的位置必须和 `context` 中的字段顺序对应。旧线程以后再被调度回来，会从原来 `switch_to()` 调用的后面继续执行，并恢复之前的中断状态。这里恢复的是原状态，如果之前中断就是关闭的，就不能直接打开。另外，这条基本流程是线程主动让出 CPU，有时钟中断并不代表已经实现了完整的时间片抢占。

7. **最后把整个过程连起来检查。** 页操作需要检查引用计数是否正确，同一页重复映射、删除不存在的映射和内存不足时能否正常处理；线程创建需要检查 PCB、栈和现场有没有设置好，失败后有没有释放资源；调度则要检查是否选中了就绪线程，以及寄存器和中断状态能否正确恢复。把这些部分接好后，应当能看到 `initproc` 从 `idleproc` 手中接过 CPU，执行 `init_main()`。入口函数返回后会进入 `do_exit()`，但当前框架里的这个函数仍是简化处理，完整的线程退出和资源回收还需要后续继续实现。

---

## 四、实验内容与实现

### 4.1 系统初始化与虚拟内存页操作

**负责人：2413902—马梓涵**

#### 4.1.1 功能与接口理解

从 `kern_init()` 开始，内核要先准备好内存管理和中断处理环境，才能进入后面的线程创建。本节对应这一段初始化过程，重点说明 A 负责的 `page_insert()`、`page_remove()` 如何接入现有页表框架，为内存访问和后续线程运行提供基础。

两个函数都位于 `kern/mm/pmm.c`。`page_insert()` 负责把指定虚拟地址映射到一个物理页，也可以更新已有映射；`page_remove()` 负责撤销映射，并在物理页不再被引用时释放它。系统初始化时，`pmm_init()` 会通过 `check_pgdir()` 和 `check_boot_pgdir()` 检查这些操作，然后初始化内核对象分配器。之后再准备中断处理、检查虚拟内存结构，最后进入 `proc_init()` 开始线程管理。

```text
kern_init()
  → pmm_init()
      → 初始化物理页管理
      → check_pgdir()、check_boot_pgdir()
          → 检查 page_insert()、page_remove() 等页表操作
      → kmalloc_init()
  → pic_init()、idt_init()
  → vmm_init()
  → proc_init()：进入线程创建阶段
```

本部分使用的主要接口如下：

| 接口或数据 | 在本部分中的作用 |
|---|---|
| `get_pte(pgdir, la, create)` | 找到目标虚拟地址对应的最后一级页表项；`create=1` 时允许分配缺少的中间级页表 |
| `struct Page`、`page_ref_inc()`、`page_ref_dec()` | 记录物理页及其引用计数，判断是否还可以释放 |
| `page2ppn()`、`pte_create()`、`pte2page()` | 在物理页、物理页号和页表项之间转换 |
| `free_page()` | 把不再被引用的物理页交回分配器 |
| `tlb_invalidate()` | 使修改地址对应的 TLB 翻译缓存失效 |

这里要分清 `struct Page` 和 PTE：前者是物理页的管理信息，后者是页表中的地址转换记录。建立映射时，两边的信息都要更新；只改 PTE 或只改引用计数都不完整。

#### 4.1.2 初始化顺序与前置代码迁移

这一阶段先要把 Lab2、Lab3 的基础功能补齐。物理内存管理需要 `page_init()` 和 Best-Fit 分配算法；中断处理需要 Lab3 的处理函数、`RESTORE_ALL` 和从汇编入口调用 `trap()` 的代码。迁移时只补框架中缺少的实现，保留 `lab2`、`lab3` 注释和原学号，也保留 Lab4 已有的中断保护及 `forkrets` 入口。

`page_init()` 先从设备树取得 RAM 的起点和大小，排除内核及页描述符数组占用的空间，再把剩余完整页登记到空闲链表。Best-Fit 在这些空闲块中寻找满足需求的最小块，负责后续分配与回收。本次沿用 Lab4 的 `default_pmm_manager` 接口，在其对应文件中接入前面的 Best-Fit 实现，避免只复制算法后出现接口名称对不上的问题。

`pmm_init()` 会先初始化物理页管理器和可用页范围，再取得启动页表的虚拟地址与物理地址，检查页映射，最后调用 `kmalloc_init()`。这样，后面的 PCB 等内核对象才有可用的分配接口。`pic_init()` 和 `idt_init()` 准备中断处理，`vmm_init()` 检查虚拟内存区域的管理结构，随后 `proc_init()` 才开始建立线程。时钟设置和全局中断开启位于线程初始化之后。

我们理解这里的顺序是：先让内核能够正确分配和访问内存，再让线程管理代码使用这些接口。物理页分配器决定哪些页面可以分配，页映射函数决定虚拟地址访问哪一个物理页，两部分要配合起来。

#### 4.1.3 页映射的建立与删除

`page_insert()` 先调用 `get_pte(pgdir, la, 1)`，拿到目标 PTE 后才开始修改引用计数。如果申请页表页失败，就返回 `-E_NO_MEM`，不继续建立映射。拿到 PTE 后，需要区分以下情况：

| 原有情况 | 处理方式 |
|---|---|
| 目标地址还没有有效映射 | 新物理页的引用计数加一，写入 PPN、权限和有效位 |
| 已经映射到同一个物理页 | 更新权限，引用计数的净变化为零 |
| 已经映射到另一个物理页 | 撤销旧页的一个引用，必要时释放旧页，再写入新页映射 |

实现中先增加新页的引用。如果发现原来映射的就是同一页，就减掉刚增加的这一次引用；如果是另一页，就调用 `page_remove_pte()` 删除旧映射。核心代码如下：

```c
page_ref_inc(page);
if (*ptep & PTE_V)
{
    if (pte2page(*ptep) == page)
    {
        page_ref_dec(page);
    }
    else
    {
        page_remove_pte(pgdir, la, ptep);
    }
}
*ptep = pte_create(page2ppn(page), perm);
tlb_invalidate(pgdir, la);
```

同一页重复映射时，不能先把旧页释放，再建立新映射。因为新旧是同一个物理页，如果先减到零，它就会被放回空闲链表。现在的写法只更新权限，不会误释放这页，也不会多留一次引用。

`page_remove()` 使用 `get_pte(pgdir, la, 0)` 查找已有 PTE，不会为了删除操作额外申请页表。如果找不到 PTE，或者 PTE 没有有效位，就直接返回。对有效映射，辅助函数 `page_remove_pte()` 按下面的顺序处理：

```text
取得旧映射对应的 Page
  → 清空 PTE
  → 使对应地址的 TLB 缓存失效
  → 引用计数减一
  → 引用为零时释放物理页
```

删除的是一个地址上的映射。如果还有其他地址映射到这页，它的引用计数就不会归零，因此暂时不能释放。对同一地址重复调用删除也应当安全：第一次清掉映射后，后面的调用不会再次减少引用计数。本实现不会在删除单个映射时自动回收中间级页表页，这部分与映射到的数据页是两类资源。

#### 4.1.4 SPEC 的编写

编写 SPEC 时，我们先看函数的输入和现有辅助接口，再把容易出错的分支单独列出来。页映射最需要检查的是：改完后 PTE 是否指向正确物理页，引用计数有没有多加或少减，以及 CPU 会不会继续使用旧的翻译缓存。

| 函数 | 前置条件 | 需要满足的结果 |
|---|---|---|
| `page_insert()` | 页表根和目标物理页有效，物理页管理已初始化，权限参数符合调用要求 | 成功返回 0；正确建立或更新映射；同页重映射不改变引用计数，异页替换正确减少旧引用；页表分配失败返回 `-E_NO_MEM`，不破坏原有有效映射 |
| `page_remove()` | 页表根有效，地址可以尚未映射 | 有映射时清除 PTE、减少引用并按需释放物理页；无映射和重复删除时直接返回；不创建页表 |

SPEC 还要求复用原有地址转换、引用计数和 TLB 接口，不改变函数签名，也不修改框架自带的检查函数。同页更新和重复删除需要单独写出来，因为它们虽然不会产生新的映射关系，却很容易造成引用计数错误。

#### 4.1.5 Prompt 的编写

页映射部分使用一份四段式提示词，把两个函数放在一起说明，因为它们共用 PTE、物理页引用计数和删除辅助函数。各部分的内容如下：

| 提示词部分 | 主要内容 |
|---|---|
| `[PROMPT]` | 指定在 `pmm.c` 中实现 `page_insert()`、`page_remove()`，允许增加内部辅助函数，保留其余框架 |
| `[RELY]` | 提供 `get_pte()`、`pte_create()`、`pte2page()`、`page2ppn()`、引用计数、物理页释放和 TLB 接口，以及实际 PTE 编码 |
| `[GUARANTEE]` | 列出两个函数的原型，说明成功返回、失败返回和删除行为 |
| `[SPECIFICATION]` | 写清首次映射、同页更新、异页替换、重复删除和内存不足时应有的结果 |

### 4.2 进程管理初始化与内核线程首次运行

**负责人：2411560—朱泽帅**

#### 4.2.1 模块功能描述

`kern_init()` 完成物理内存、异常入口和虚拟内存初始化后调用 `proc_init()`。这一阶段要把正在执行的启动线程纳入进程管理，并创建一个等待调度的内核线程。B 组在 [`proc.c`](../code/kern/process/proc.c) 的 `LAB4 function2 2411560` 标记下实现五个函数。PCB 和寄存器上下文定义在 [`proc.h`](../code/kern/process/proc.h)，首次恢复复用 Lab3 的 [`trapentry.S`](../code/kern/trap/trapentry.S) 和 Lab4 的 [线程入口汇编](../code/kern/process/entry.S)。

```text
proc_init()
  ├─ alloc_proc() → idleproc（PID 0，沿用 bootstack）
  └─ kernel_thread(init_main, "Hello world!!", 0)
       └─ do_fork() [C] → alloc_proc() → setup_kstack()
                        → copy_mm() → copy_thread()
                        → 分配 PID、登记 PCB、wakeup_proc()
     取得 PID 后，find_proc(pid) → initproc（PID 1）
```

`proc_init()` 返回时，启动线程仍是 `current`。init 的预期状态为 `PROC_RUNNABLE`，它有独立内核栈和首次执行现场，由后续调度决定何时开始运行。

#### 4.2.2 功能说明：设计原理及核心函数理解

**（1）`alloc_proc()`。** 用 `kmalloc(sizeof(*proc))` 分配整个 PCB，失败时返回 `NULL`。分配成功后对整块内存清零，再设置 `state = PROC_UNINIT`、`pid = -1` 和 `pgdir = boot_pgdir_pa`。PID 为 `-1` 表示还未取得正式编号。两个链表节点初始化为自环，便于后续登记。普通子线程取得内核栈和有效现场后，由 `do_fork()` 完成登记和唤醒。

**（2）`proc_init()`。** 初始化全局进程链表 `proc_list` 及每个 PID 哈希桶。idle 沿用 `bootstack`，PID 为 0，并设置 `need_resched = 1`。它计入 `nr_process`，由调度器作为后备线程使用。创建 init 前先设置 `current = idleproc`，使 `do_fork()` 能取得正确的父线程。创建成功后用返回的 PID 查找 PCB，断言它存在且 PID 为 1，再设名为 `init`。idle 的 PCB 分配失败、init 创建失败都由 `panic()` 终止启动。

**（3）`kernel_thread()`。** 用局部 trapframe 保存目标函数 `fn`、参数 `arg` 和第一次恢复地址。汇编入口 `kernel_thread_entry` 执行 `move a0, s1` 和 `jalr s0`，因此函数地址放在 `s0`，参数放在 `s1`，`epc` 存放的是汇编入口地址。构造的 `sstatus` 清除 `SIE`，并设置 `SPP` 和 `SPIE`，使恢复现场期间中断保持关闭，`sret` 返回 S 模式时恢复中断使能。调用 `do_fork(clone_flags | CLONE_VM, 0, &tf)` 后，直接返回子线程 PID 或错误码。局部 `tf` 的有效期到本次调用返回为止，子线程必须在此之前获得自己的副本。

**（4）`copy_mm()`。** 内核线程没有独立的用户地址空间管理对象，`mm` 设为 `NULL`，根页表物理地址仍取 `boot_pgdir_pa`。实际写入 Sv39 `satp` 的步骤由 `proc_run()` 完成。Lab4 没有提供完整的用户地址空间复制、共享和回收机制，所以遇到非空的 `current->mm` 时返回 `-E_INVAL`，由调用方回滚已申请的资源。

**（5）`copy_thread()`。** 在子线程内核栈高地址端保存独立的 trapframe。栈大小为 8192 字节，RV64 trapframe 大小为 288 字节，所以 `child_tf = kstack + 7904`。栈基址按页对齐，`7904` 是 16 的倍数。设置子线程返回值 `a0 = 0` 后，分别准备两个恢复阶段需要的栈指针。

| 恢复阶段 | 使用的现场 | 本实现设置 |
|---|---|---|
| `switch_to()` 恢复调度上下文 | `context.ra` 和 `context.sp` | `ra` 指向 `forkret()`，`sp` 指向 `child_tf` |
| `__trapret` 恢复首次执行现场 | `tf->epc`、`tf->gpr.sp` 及其他寄存器 | `epc` 指向 `kernel_thread_entry`。初始栈参数为 0 时，用 `child_tf` 作为恢复后的 `sp`，运行栈从该处向低地址增长 |

第一次切入时，`forkret()` 通过 `current->tf` 找到 trapframe，`forkrets` 将它交给 `__trapret`。第二阶段恢复 `tf->gpr.s0`、`tf->gpr.s1` 后，入口汇编才能调用 `fn(arg)`。对于 init，这一调用是 `init_main("Hello world!!")`。

#### 4.2.3 SPEC 与 Prompt 设计

##### SPEC 的形成

五个函数由同一条创建路径串接，规格按各自的输入、输出和资源归属编写。最容易在交接时出错的是栈由谁分配、现场在什么时候复制、页表字段使用哪种地址，因此将这些条件列入验收。

| 函数 | 前置条件 | 需要满足的结果 |
|---|---|---|
| `alloc_proc()` | 内核动态内存分配可用 | 返回处于 `PROC_UNINIT` 的 PCB 或 `NULL` |
| `proc_init()` | 内存及 Trap 初始化已完成 | 先建立链表和哈希表，再设置 PID 0 的 idle，最后创建并查得 PID 1 的 init |
| `kernel_thread()` | `current` 有效，线程入口和参数已确定 | 构造符合 RV64 入口约定的临时 trapframe，转交 `do_fork()` |
| `copy_mm()` | 子线程 PCB 已分配 | 内核线程使用启动根页表的物理地址，遇到非空父 `mm` 时明确报错 |
| `copy_thread()` | 子线程拥有已分配的内核栈，源 trapframe 有效 | 新线程持有自己的现场，两个阶段均能取得有效的栈指针 |

##### 实现 Prompt

本模块使用一份四段式实现 Prompt。`[PROMPT]` 指定 B 组代码范围和验收方式。`[RELY]` 提供 PCB、trapframe、入口汇编、页表地址及现有测试断言。`[GUARANTEE]` 固定五个函数的签名和职责。`[SPECIFICATION]` 分别列出前置条件、结果和边界情形，包括分配失败、零栈参数及父线程拥有用户地址空间时的处理。原始输入见 [B 组实现 Prompt](./prompts/2411560/Process_Initialization_Implementation_Prompt.md)。

#### 4.2.4 实现迭代过程

先核对 `proc.h`、`trap.h` 和 `RESTORE_ALL` 的字段顺序，再检查 `proc_run()` 写 `satp` 时对 `pgdir` 的用法。实现时让 `alloc_proc()` 和 `copy_mm()` 都按物理地址约定设置根页表，并按 `kernel_thread_entry` 的寄存器使用顺序填充 trapframe。

单文件编译前专门检查了 `esp == 0` 的路径。创建内核线程时传入的 0 表示由 `copy_thread()` 选择初始栈，`RESTORE_ALL` 会直接从 `gpr.sp` 加载寄存器，所以这个字段必须填入有效地址。采用 `child_tf` 后，恢复完成的线程可以在其下方使用内核栈。另一个交接细节是 `set_proc_name()` 固定从源地址读取 `PROC_NAME_LEN` 字节，因此两个名称都放在足长的静态字符数组中。

#### 4.2.5 测试设计与验证

五个函数的实现提交为 [`9571469`](https://github.com/iodwad/NKU_OS_2026/commit/957146972d049c5cb9a7695d70c7596724eb7581)。以下结果来自 B 组交付时在 WSL2 Ubuntu 22.04 下的实际编译和反汇编。

| 检查 | 实测结果 |
|---|---|
| 编译 `proc.c` 对象文件 | 使用真实 Makefile 和 RV64 交叉工具链，目标退出码为 0。加入 `-fkeep-static-functions` 保留暂时没有调用方的静态函数。 |
| `riscv64-unknown-elf-objdump -dr proc.o` | 退出码为 0。对象文件包含五个函数，反汇编可见 7904 字节栈偏移、子线程 `a0` 清零、零栈参数分支以及 `context.ra/sp` 写入。 |
| 完整 `make -j4` | 修改前后退出码均为 2。修改前缺少 `proc_init` 和 `cpu_idle`，修改后 `proc.c` 编译完成，链接仍缺少 C 组的 `do_fork` 和 `cpu_idle`。 |

仓库提供的 [`test_lab4_process_create()` 和 `test_lab4_schedule()`](../code/tools/test_lab4.c) 分别检查 idle/init 的 PID、状态和执行现场，以及两个内核线程能否以不同参数被调度运行。[评分脚本](../code/tools/grade.sh) 还会检查 init 的串口输出。B 组交付时完整内核仍缺少链接符号，所以未执行 QEMU 和 `make grade`，上述运行时检查留待 A/B/C 集成验证。

### 4.3 内核线程创建、调度与上下文切换

**负责人（2412565—张宸笛号）**

#### 4.3.1 模块功能描述

上一节的 `kernel_thread()` 已经准备好入口函数、参数和临时 trapframe。接下来由 `do_fork()` 把新线程登记到进程表。`proc_init()` 返回后，启动线程进入 `cpu_idle()`，由 `schedule()` 选中 `initproc`，再经 `proc_run()` 和 `switch_to()` 切到它的执行现场。[进程管理源码](../code/kern/process/proc.c) · [调度器源码](../code/kern/schedule/sched.c) · [切换汇编](../code/kern/process/switch.S)

```text
kernel_thread() → do_fork() → 准备内核栈和执行现场 → 登记并唤醒 initproc
cpu_idle() → schedule() → proc_run() → switch_to()
  → forkret() → forkrets → __trapret → kernel_thread_entry → init_main()
```

#### 4.3.2 功能说明与核心函数

**（1）`do_fork()`。** 先检查进程数，接着分配 PCB 和独立内核栈，再调用 `copy_mm()`、`copy_thread()`。临时 trapframe 复制到子线程的栈上以后，子线程便有了自己的首次运行现场。父线程从 `do_fork()` 得到子线程 PID，子线程第一次恢复时的 `a0` 由 `copy_thread()` 设为 0。

准备完成后，`do_fork()` 在关中断期间分配 PID，将 `parent` 设为 `current`，把 PCB 登记到 `proc_list` 和 PID 哈希表，再增加 `nr_process`。最后调用 `wakeup_proc()`，让子线程进入 `PROC_RUNNABLE` 状态。失败的分支都发生在登记之前，因此只需要释放本次已经申请到的资源。

| 创建失败的位置 | 返回值 | 资源处理 |
|---|---|---|
| 进程数达到 `MAX_PROCESS` | `-E_NO_FREE_PROC` | 直接返回 |
| PCB 分配失败 | `-E_NO_MEM` | 直接返回 |
| 内核栈分配失败 | `setup_kstack()` 的错误码 | 释放 PCB |
| 地址空间准备失败 | `copy_mm()` 的错误码 | 先释放内核栈，再释放 PCB |

**（2）`cpu_idle()`。** 循环检查 `current->need_resched`，标记置位时调用 `schedule()`。`proc_init()` 已经把 idle 的标记设为 1，所以启动线程第一次进入空闲循环就会调度。

**（3）`schedule()`。** 先保存本核的中断状态并关闭中断，再清除当前线程的重调度标记。idle 从链表的第一个有效节点开始找，普通线程从自己的下一个节点开始找。遍历到表尾后绕回，跳过链表头，只选 `PROC_RUNNABLE`。如果只有当前线程就绪，绕一圈后会再次选中它。没有就绪的普通线程时选 `idleproc`。选中后给目标的 `runs` 加一，目标与当前线程不同时调用 `proc_run()`。

**（4）`switch_to()`。** `a0` 指向旧线程的 `context`，`a1` 指向新线程的 `context`。汇编用 `sd` 保存旧线程的 `ra`、`sp`、`s0`–`s11`，再用 `ld` 恢复新线程的同一组寄存器。`ret` 根据恢复后的 `ra` 决定从哪里继续。运行过的线程回到上一次切换的位置，第一次运行的线程进入 `copy_thread()` 设置的 `forkret()`。[`struct context` 的定义](../code/kern/process/proc.h) · [首次恢复入口](../code/kern/trap/trapentry.S)

#### 4.3.3 SPEC 与 Prompt 设计

##### SPEC 的形成

我们先核对函数之间的接口，再按各函数的前置条件和执行结果写规格。线程创建要写清登记时机和失败时的清理顺序，调度要写清从哪里开始找，以及找不到就绪线程时怎么处理。

| 函数 | 前置条件 | 需要满足的结果 |
|---|---|---|
| `do_fork()` | 进程管理已初始化，调用者提供有效的 trapframe | 成功返回 PID，子线程可被查到和调度。失败时释放本次取得的资源 |
| `cpu_idle()` | 启动线程已成为 `idleproc` | 持续检查重调度标记，标记置位时调用调度器 |
| `schedule()` | 当前线程是 idle 或已登记的普通线程 | 按链表顺序选就绪线程，必要时回退 idle，并在调用返回时恢复原中断状态 |
| `switch_to()` | 旧现场可以写入，目标现场已准备好 | 保存旧线程的寄存器，从目标线程恢复出的返回地址继续执行 |

##### 实现 Prompt

四个函数各用一份提示词。`[RELY]` 从仓库摘录各函数所需的宏、字段和接口，`[GUARANTEE]` 列出函数签名和职责，`[SPECIFICATION]` 按上表写前置条件和预期结果，并补上分配失败、调度回退和首次切换等分支。[`do_fork()` 提示词](./prompts/2412565/Do_Fork_Implementation_Prompt.md) · [`cpu_idle()` 提示词](./prompts/2412565/Cpu_Idle_Implementation_Prompt.md) · [`schedule()` 提示词](./prompts/2412565/Schedule_Implementation_Prompt.md) · [`switch_to()` 提示词](./prompts/2412565/Switch_To_Implementation_Prompt.md)

#### 4.3.4 实现迭代过程

实现时先核对 `copy_thread()` 对子线程内核栈的要求，再把 PID 分配和进程表登记放到现场准备之后。接着用空链表和只有当前线程就绪两种情况检查遍历的终止位置。汇编部分按照 `proc.h` 的字段顺序写偏移，RV64 上每个字段占 8 字节，`ra` 和 `sp` 分别位于偏移 0 和 8，`s0` 到 `s11` 依次位于偏移 16 到 104。

第一次运行 `make grade` 时，内核已经编译成功，但评分没有收到串口输出。手动连接 QEMU 后看到 GDB 无法识别目标架构，因此在 Makefile 的默认 GDB 命令中加上内核 ELF `bin/kernel`。GDB 从 ELF 取得 RV64 架构信息后，评分脚本可以继续运行 QEMU。[Makefile](../code/Makefile)

#### 4.3.5 测试设计与验证

| 检查 | 实测结果 |
|---|---|
| `make -j4` | RV64 内核编译和链接成功 |
| `make qemu` | 串口输出 `this initproc, pid = 1, name = "init"` 和 `To U: "Hello world!!".` |
| `make grade` | PMM、Process Create、Schedule 三项通过，总分 **30/30** |
| 创建失败补测 | 在临时副本中触发进程数上限、PCB 和内核栈分配失败，以及 `copy_mm()` 失败，核对了错误码、释放操作和进程登记状态 |
| 调度与汇编补测 | 在 QEMU 中检查空链表回退、跳过阻塞线程、当前线程唯一就绪及切换后的中断恢复。用编译器与 `objdump` 对照了 14 个寄存器字段的访存偏移 |

进程创建用例检查了 idle 和 init 的 PID、父进程和初始现场。调度用例用不同参数创建两个内核线程，让它们主动调用 `schedule()`，再检查两个线程都执行过各自的入口。[测试源码](../code/tools/test_lab4.c) · [评分脚本](../code/tools/grade.sh)

---

## 五、Challenge

### 5.1 本地中断的保存与恢复

**负责人（2412565—张宸笛号）**

`sstatus` 的第 1 位是 `SIE`，掩码为 `0x2`，用于控制 S 模式中断使能。`local_intr_save(intr_flag)` 先用 `csrr` 读取 `sstatus`。如果 `SIE` 为 1，`intr_disable()` 就通过 `csrrc` 清除这一位，保存值为 1。如果原先的 `SIE` 为 0，保存值就是 0。离开临界区时，`local_intr_restore(intr_flag)` 根据保存值处理。保存值为 1 时，通过 `intr_enable()` 中的 `csrrs` 重新开启中断。保存值为 0 时，保持关闭。[保存与恢复宏](../code/kern/sync/sync.h) · [中断开关实现](../code/kern/driver/intr.c) · [CSR 宏定义](../code/libs/riscv.h)

以一次开着中断进入的调度为例，`schedule()` 保存 1 并关闭中断，里面的 `proc_run()` 此时保存的是 0。原线程切回来后，`proc_run()` 先按 0 恢复，仍然处在关中断状态。随后 `schedule()` 按 1 恢复，才重新开启中断。这段保护作用于当前 CPU，多核共同访问进程结构时还需要锁等同步机制。
