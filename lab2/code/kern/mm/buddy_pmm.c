// Lab2 Buddy System - Student ID: 2411560
#include <buddy_pmm.h>
#include <list.h>
#include <stdio.h>
#include <string.h>

/* Bound static bookkeeping by this kernel's physical mapping, not by free RAM.
 * page_init reserves the kernel BSS (including these arrays) before registration.
 * A different mapping limit requires reviewing MAX_ORDER as well. */
#define BUDDY_CAPACITY ((PHYSICAL_MEMORY_END - DRAM_BASE) / PGSIZE)
#define BUDDY_MAX_ORDER 15

enum buddy_state {
    BUDDY_UNREGISTERED = 0,
    BUDDY_FREE_BODY,
    BUDDY_FREE_HEAD,
    BUDDY_ALLOC_HEAD,
    BUDDY_ALLOC_BODY,
};

struct buddy_record {
    size_t requested;       /* Original n, valid only for an allocated head. */
    unsigned char order;   /* Valid only for a free or allocated head. */
    unsigned char state;
};

static struct buddy_record records[BUDDY_CAPACITY];
static list_entry_t free_lists[BUDDY_MAX_ORDER + 1];
static size_t free_count;
static bool initialized;

static size_t block_pages(unsigned int order) {
    return (size_t)1 << order;
}

static size_t managed_pages(void) {
    assert(initialized);
    assert(npage >= nbase && npage - nbase <= BUDDY_CAPACITY);
    return npage - nbase;
}

/* Validate a descriptor numerically before pointer subtraction/dereferencing.
 * The subtraction-based length check also rejects addition overflow. */
static size_t checked_range(struct Page *base, size_t n) {
    size_t count = managed_pages();
    uintptr_t address = (uintptr_t)base, start = (uintptr_t)pages;
    assert(pages != NULL && base != NULL && n > 0 && address >= start);
    uintptr_t displacement = address - start;
    assert(displacement % sizeof(struct Page) == 0);
    size_t index = displacement / sizeof(struct Page);
    assert(index < count && n <= count - index);
    return index;
}

/* These helpers only change block representation, never the free-page count.
 * Page.property keeps its framework meaning: number of pages in a FREE block.
 * Only free heads have PG_property set; all other pages have property == 0. */
static void link_free(size_t index, unsigned int order) {
    struct Page *head = pages + index;
    records[index].state = BUDDY_FREE_HEAD;
    records[index].order = order;
    records[index].requested = 0;
    head->property = block_pages(order);
    SetPageProperty(head);
    list_add(&free_lists[order], &head->page_link);
}

static void unlink_free(size_t index) {
    struct Page *head = pages + index;
    assert(records[index].state == BUDDY_FREE_HEAD && PageProperty(head));
    list_del_init(&head->page_link);
    ClearPageProperty(head);
    head->property = 0;
    records[index].state = BUDDY_FREE_BODY;
}

/* The input block is already entirely registered and free, but not linked.
 * A free-head record certifies that its entire aligned block is registered and
 * free. Thus two such buddies certify their complete parent, even if different
 * init_memmap calls supplied the pages. Unregistered gaps never get heads. */
static void publish_free(size_t index, unsigned int order) {
    size_t count = managed_pages();
    while (order < BUDDY_MAX_ORDER) {
        size_t capacity = block_pages(order);
        ppn_t ppn = nbase + index;
        ppn_t partner_ppn = ppn ^ capacity;
        ppn_t parent_ppn = ppn & ~capacity;
        if (partner_ppn < nbase || parent_ppn < nbase) break;
        size_t partner = partner_ppn - nbase;
        size_t parent = parent_ppn - nbase;
        if (partner >= count || capacity > count - partner ||
            parent >= count || 2 * capacity > count - parent) break;
        if (records[partner].state != BUDDY_FREE_HEAD ||
            records[partner].order != order) break;
        assert(!PageReserved(pages + partner));
        assert(PageProperty(pages + partner));
        assert(pages[partner].property == capacity);
        unlink_free(partner);
        index = parent;
        ++order;
    }
    link_free(index, order);
}

static void buddy_init(void) {
    static_assert(BUDDY_CAPACITY > 0);
    static_assert(BUDDY_CAPACITY <= ((size_t)1 << BUDDY_MAX_ORDER));
    memset(records, 0, sizeof(records));
    for (unsigned int order = 0; order <= BUDDY_MAX_ORDER; ++order)
        list_init(&free_lists[order]);
    free_count = 0;
    initialized = 1;
}

static void buddy_init_memmap(struct Page *base, size_t n) {
    size_t index = checked_range(base, n);
    /* Reject overlap/re-registration before touching any page or list. */
    for (size_t i = 0; i < n; ++i) {
        assert(records[index + i].state == BUDDY_UNREGISTERED);
        assert(PageReserved(base + i));
    }
    for (size_t i = 0; i < n; ++i) {
        struct Page *page = base + i;
        ClearPageReserved(page);
        ClearPageProperty(page);
        page->property = 0;
        set_page_ref(page, 0);
        list_init(&page->page_link);
        records[index + i].state = BUDDY_FREE_BODY;
    }
    size_t cursor = index, remaining = n;
    while (remaining != 0) {
        unsigned int order = 0;
        while (order < BUDDY_MAX_ORDER) {
            size_t next = block_pages(order + 1);
            if (next > remaining || ((nbase + cursor) & (next - 1))) break;
            ++order;
        }
        size_t capacity = block_pages(order);
        publish_free(cursor, order);
        cursor += capacity;
        remaining -= capacity;
    }
    free_count += n;
}

static struct Page *buddy_alloc_pages(size_t n) {
    size_t count = managed_pages();
    assert(n > 0);
    /* Check before rounding/shifting: even SIZE_MAX cannot overflow here. */
    if (n > count || n > free_count) return NULL;
    unsigned int wanted = 0;
    while (block_pages(wanted) < n) ++wanted;
    unsigned int order = wanted;
    while (order <= BUDDY_MAX_ORDER && list_empty(&free_lists[order])) ++order;
    if (order > BUDDY_MAX_ORDER) return NULL;

    struct Page *head = le2page(list_next(&free_lists[order]), page_link);
    size_t index = checked_range(head, block_pages(order));
    assert(records[index].state == BUDDY_FREE_HEAD);
    assert(records[index].order == order);
    assert(((nbase + index) & (block_pages(order) - 1)) == 0);
    for (size_t i = 0; i < block_pages(order); ++i) {
        assert(!PageReserved(head + i));
        assert(records[index + i].state ==
               (i == 0 ? BUDDY_FREE_HEAD : BUDDY_FREE_BODY));
    }
    unlink_free(index);
    while (order > wanted) {
        --order;
        link_free(index + block_pages(order), order);
    }
    size_t capacity = block_pages(wanted);
    for (size_t i = 0; i < capacity; ++i) {
        records[index + i].state = BUDDY_ALLOC_BODY;
        records[index + i].requested = 0;
    }
    records[index].state = BUDDY_ALLOC_HEAD;
    records[index].order = wanted;
    records[index].requested = n;
    free_count -= capacity;
    return head;
}

static void buddy_free_pages(struct Page *base, size_t n) {
    size_t index = checked_range(base, n);
    /* An exact live allocation record is required; rounding n alone is unsafe. */
    assert(records[index].state == BUDDY_ALLOC_HEAD);
    assert(records[index].requested == n);
    unsigned int order = records[index].order;
    assert(order <= BUDDY_MAX_ORDER);
    size_t capacity = block_pages(order);
    assert(capacity <= managed_pages() - index);
    assert(((nbase + index) & (capacity - 1)) == 0);
    for (size_t i = 0; i < capacity; ++i) {
        assert(!PageReserved(base + i) && !PageProperty(base + i));
        assert(records[index + i].state ==
               (i == 0 ? BUDDY_ALLOC_HEAD : BUDDY_ALLOC_BODY));
    }
    /* Only after the full validation may ownership or counters change. */
    for (size_t i = 0; i < capacity; ++i) {
        records[index + i].state = BUDDY_FREE_BODY;
        records[index + i].requested = 0;
        base[i].property = 0;
        set_page_ref(base + i, 0);
    }
    publish_free(index, order);
    free_count += capacity;
}

static size_t buddy_nr_free_pages(void) {
    assert(initialized);
    return free_count;
}

static void buddy_check_basic(void) {
    assert(pmm_manager == &buddy_pmm_manager);
    size_t before = nr_free_pages();
    struct Page *one = alloc_pages(1);
    assert(one != NULL && nr_free_pages() == before - 1);
    struct Page *three = alloc_pages(3);
    assert(three != NULL && nr_free_pages() == before - 5);
    ppn_t first = page2ppn(one), second = page2ppn(three);
    assert((second & 3) == 0);
    assert(first < second || first >= second + 4);
    assert(!PageReserved(one) && !PageProperty(one));
    assert(!PageReserved(three) && !PageProperty(three));
    free_pages(three, 3);
    assert(nr_free_pages() == before - 1);
    assert(records[one - pages].state == BUDDY_ALLOC_HEAD);
    free_pages(one, 1);
    assert(nr_free_pages() == before);
    cprintf("buddy_check: request 1+3 occupies 1+4 pages; restored %u free pages\n",
            (unsigned int)before);
}

#include "buddy_checks.inc"

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
