// Lab2 SLUB Challenge - Student ID: 2411560
#include <slub.h>
#include <pmm.h>
#include <list.h>
#include <stdio.h>
#include <string.h>

#define SLUB_ALIGNMENT 16
#define SLUB_CLASS_COUNT 8
#define SLUB_PAGE_LIMIT ((PHYSICAL_MEMORY_END - DRAM_BASE) / PGSIZE)
#define SLUB_BITMAP_WORDS 4
#define SLUB_NO_SLOT 0xFFFFU
#define SLUB_MAGIC 0x534C55424C414232ULL

/* Lives inside the data page, never in struct Page. For a large object,
 * page_request is the ORIGINAL Buddy request, not its rounded capacity. */
struct slub_slab {
    uint64_t magic;
    struct Page *page;
    size_t page_request;
    size_t object_bytes;  /* Slot size for small slabs; requested bytes for large. */
    list_entry_t link;
    uint16_t class_id;
    uint16_t slots;
    uint16_t inuse;
    uint16_t free_head;
    uint64_t used[SLUB_BITMAP_WORDS];
};

#define SLUB_HEADER_BYTES ((sizeof(struct slub_slab) + 15) & ~(size_t)15)

static const size_t slot_sizes[SLUB_CLASS_COUNT] = {16,32,64,128,256,512,1024,2048};
static list_entry_t partial[SLUB_CLASS_COUNT];
/* Index only: all payload storage and slab headers come from alloc_pages.
 * The BSS table is reserved as part of the kernel by the existing page_init. */
static struct slub_slab *owner[SLUB_PAGE_LIMIT];
static size_t live_objects, slab_count;
static bool slub_ready;

static size_t slub_page_capacity(size_t requested) {
    assert(requested > 0 && requested <= SLUB_PAGE_LIMIT);
    size_t q = 1;
    while (q < requested) q *= 2;
    return q;
}

/* KADDR is commented out in this project. Check descriptor, detected RAM,
 * supported physical window, offset and high virtual window explicitly.
 * Return NULL before touching data if the PMM result cannot be mapped here. */
static void *slub_page_data(struct Page *base, size_t capacity) {
    if (pages == NULL || npage < nbase || npage - nbase > SLUB_PAGE_LIMIT) return NULL;
    uintptr_t raw = (uintptr_t)base, first = (uintptr_t)pages;
    if (raw < first || (raw - first) % sizeof(struct Page)) return NULL;
    size_t index = (raw - first) / sizeof(struct Page), count = npage - nbase;
    if (index >= count || capacity == 0 || capacity > count - index) return NULL;
    uintptr_t pa = page2pa(base);
    size_t bytes = capacity * PGSIZE; /* capacity <= fixed 32768-page bound */
    if (pa < KERNEL_BEGIN_PADDR || pa >= PHYSICAL_MEMORY_END ||
        bytes > PHYSICAL_MEMORY_END - pa) return NULL;
    if (va_pa_offset != PHYSICAL_MEMORY_OFFSET || pa > (uintptr_t)-1 - va_pa_offset) return NULL;
    uintptr_t va = pa + va_pa_offset;
    if (va < KERNBASE || va >= KERNTOP || bytes > KERNTOP - va) return NULL;
    return (void *)va;
}

static unsigned char *slub_payload(struct slub_slab *slab) {
    return (unsigned char *)slab + SLUB_HEADER_BYTES;
}

static bool slub_slot_used(struct slub_slab *slab, size_t slot) {
    return (slab->used[slot / 64] & (1ULL << (slot % 64))) != 0;
}

/* Internal certificate validation. Does not derive a header by dereferencing
 * an arbitrary user pointer; callers obtain slab only from our source table. */
static void slub_validate(struct slub_slab *slab) {
    assert(slab != NULL && slab->magic == SLUB_MAGIC);
    assert(slab->page_request > 0 && slab->page_request <= SLUB_PAGE_LIMIT);
    size_t q = slub_page_capacity(slab->page_request);
    assert(slub_page_data(slab->page, q) == (void *)slab);
    size_t index = slab->page - pages;
    for (size_t i = 0; i < q; ++i) {
        assert(owner[index+i] == slab);
        assert(!PageReserved(slab->page+i) && !PageProperty(slab->page+i));
    }
    assert(slab->class_id <= SLUB_CLASS_COUNT);
    if (slab->class_id < SLUB_CLASS_COUNT) {
        assert(slab->page_request == 1);
        assert(slab->object_bytes == slot_sizes[slab->class_id]);
        assert(slab->slots == (PGSIZE-SLUB_HEADER_BYTES)/slab->object_bytes);
        assert(slab->free_head == SLUB_NO_SLOT || slab->free_head < slab->slots);
    } else {
        assert(slab->slots == 1 && slab->object_bytes > slot_sizes[SLUB_CLASS_COUNT-1]);
        assert(slab->object_bytes <= slab->page_request * PGSIZE - SLUB_HEADER_BYTES);
    }
    assert(slab->inuse <= slab->slots);
}

static struct slub_slab *slub_new_slab(unsigned int class_id, size_t bytes) {
    /* Bound before addition, division and power-of-two rounding. */
    if (bytes > SLUB_PAGE_LIMIT * PGSIZE - SLUB_HEADER_BYTES) return NULL;
    size_t total = bytes + SLUB_HEADER_BYTES;
    size_t requested = total / PGSIZE + (total % PGSIZE != 0);
    if (requested == 0 || requested > SLUB_PAGE_LIMIT || requested > npage - nbase) return NULL;
    size_t capacity = slub_page_capacity(requested);
    struct Page *base = alloc_pages(requested);
    if (base == NULL) return NULL;
    struct slub_slab *slab = slub_page_data(base, capacity);
    if (slab == NULL) { free_pages(base, requested); return NULL; }
    size_t index = base - pages;
    for (size_t i = 0; i < capacity; ++i) {
        assert(owner[index+i] == NULL);
        assert(!PageReserved(base+i) && !PageProperty(base+i));
    }
    memset(slab, 0, SLUB_HEADER_BYTES);
    slab->magic = SLUB_MAGIC;
    slab->page = base;
    slab->page_request = requested;
    slab->object_bytes = bytes;
    slab->class_id = class_id;
    slab->slots = class_id < SLUB_CLASS_COUNT ? (PGSIZE-SLUB_HEADER_BYTES)/bytes : 1;
    slab->free_head = class_id < SLUB_CLASS_COUNT ? 0 : SLUB_NO_SLOT;
    list_init(&slab->link);
    if (class_id < SLUB_CLASS_COUNT) {
        assert(requested == 1 && slab->slots > 0 && slab->slots <= SLUB_BITMAP_WORDS * 64);
        for (size_t i = 0; i < slab->slots; ++i) {
            uint16_t next = i+1 == slab->slots ? SLUB_NO_SLOT : i+1;
            *(uint16_t *)(slub_payload(slab) + i*bytes) = next;
        }
    }
    for (size_t i = 0; i < capacity; ++i) owner[index+i] = slab;
    ++slab_count;
    if (class_id < SLUB_CLASS_COUNT) list_add(&partial[class_id], &slab->link);
    return slab;
}

void slub_init(void) {
    static_assert(SLUB_HEADER_BYTES < PGSIZE);
    static_assert((PGSIZE-SLUB_HEADER_BYTES)/SLUB_ALIGNMENT <= SLUB_BITMAP_WORDS * 64);
    static_assert(sizeof(struct slub_slab *) == 8);
    assert(!slub_ready); /* Reinitializing a live object allocator is not supported. */
    assert(pmm_manager != NULL && pages != NULL && npage > nbase);
    assert(npage - nbase <= SLUB_PAGE_LIMIT && va_pa_offset == PHYSICAL_MEMORY_OFFSET);
    for (size_t i = 0; i < SLUB_CLASS_COUNT; ++i) list_init(&partial[i]);
    memset(owner, 0, sizeof(owner));
    live_objects = slab_count = 0;
    slub_ready = 1;
    cprintf("slub_init: lazy slabs, 16-byte alignment, small classes 16..2048 bytes\n");
}

void *slub_alloc(size_t size) {
    assert(slub_ready);
    if (size == 0) return NULL;
    unsigned int class_id = 0;
    while (class_id < SLUB_CLASS_COUNT && slot_sizes[class_id] < size) ++class_id;
    struct slub_slab *slab;
    if (class_id == SLUB_CLASS_COUNT) {
        slab = slub_new_slab(class_id, size);
        if (slab == NULL) return NULL;
        slab->inuse = 1; slab->used[0] = 1;
        ++live_objects;
        return slub_payload(slab);
    }
    if (list_empty(&partial[class_id])) {
        slab = slub_new_slab(class_id, slot_sizes[class_id]);
        if (slab == NULL) return NULL;
    } else {
        /* The cache list belongs to a slab header, not a Page descriptor. */
        slab = to_struct(list_next(&partial[class_id]), struct slub_slab, link);
    }
    slub_validate(slab);
    assert(slab->class_id == class_id && slab->inuse < slab->slots);
    size_t slot = slab->free_head;
    assert(slot < slab->slots && !slub_slot_used(slab, slot));
    unsigned char *object = slub_payload(slab) + slot * slab->object_bytes;
    uint16_t next = *(uint16_t *)object;
    assert(next == SLUB_NO_SLOT || (next < slab->slots && next != slot && !slub_slot_used(slab,next)));
    slab->free_head = next;
    slab->used[slot/64] |= 1ULL << (slot%64);
    ++slab->inuse; ++live_objects;
    if (slab->inuse == slab->slots) list_del_init(&slab->link);
    return object;
}

/* All source links and ownership entries are gone before returning pages.
 * Nothing accesses the header after free_pages, which may immediately reuse it. */
static void slub_return_slab(struct slub_slab *slab) {
    struct Page *base = slab->page;
    size_t requested = slab->page_request, capacity = slub_page_capacity(requested);
    size_t index = base - pages;
    for (size_t i = 0; i < capacity; ++i) owner[index+i] = NULL;
    slab->magic = 0;
    --slab_count;
    free_pages(base, requested);
}

void slub_free(void *ptr) {
    if (ptr == NULL) return;
    assert(slub_ready);
    uintptr_t va = (uintptr_t)ptr;
    assert(va >= KERNBASE && va < KERNTOP && va >= va_pa_offset);
    uintptr_t pa = va - va_pa_offset;
    ppn_t ppn = PPN(pa);
    assert(ppn >= nbase && ppn < npage && ppn - nbase < SLUB_PAGE_LIMIT);
    struct slub_slab *slab = owner[ppn - nbase];
    assert(slab != NULL); /* No arbitrary-page header dereference. */
    slub_validate(slab);
    uintptr_t payload = (uintptr_t)slub_payload(slab);
    if (slab->class_id == SLUB_CLASS_COUNT) {
        assert(va == payload && slab->inuse == 1 && slab->used[0] == 1);
        slab->inuse = 0; slab->used[0] = 0;
        --live_objects;
        slub_return_slab(slab);
        return;
    }
    assert(va >= payload);
    size_t displacement = va - payload;
    assert(displacement < (size_t)slab->slots * slab->object_bytes);
    assert(displacement % slab->object_bytes == 0);
    size_t slot = displacement / slab->object_bytes;
    assert(slab->inuse > 0 && slub_slot_used(slab, slot));
    bool was_full = slab->inuse == slab->slots;
    slab->used[slot/64] &= ~(1ULL << (slot%64));
    *(uint16_t *)ptr = slab->free_head;
    slab->free_head = slot;
    --slab->inuse; --live_objects;
    if (slab->inuse == 0) {
        if (!was_full) list_del_init(&slab->link);
        slub_return_slab(slab);
    } else if (was_full) list_add(&partial[slab->class_id], &slab->link);
}

#include "slub_checks.inc"
