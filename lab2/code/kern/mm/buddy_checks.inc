/* Read-only diagnostics and live tests, included only by buddy_pmm.c.
 * Never reinitialize the real allocator or edit its private lists for a test. */
static unsigned char check_seen[BUDDY_CAPACITY];

static size_t check_capacity(size_t n) {
    size_t q = 1;
    assert(n > 0 && n <= BUDDY_CAPACITY);
    while (q < n) q *= 2;
    return q;
}

static void buddy_check_invariants(void) {
    size_t count = managed_pages(), free_total = 0, allocated_total = 0;
    memset(check_seen, 0, sizeof(check_seen));
    for (unsigned int order = 0; order <= BUDDY_MAX_ORDER; ++order) {
        list_entry_t *root = &free_lists[order], *previous = root;
        list_entry_t *node = list_next(root);
        size_t steps = 0, q = (size_t)1 << order;
        while (node != root) {
            assert(++steps <= count);
            uintptr_t raw = (uintptr_t)node;
            uintptr_t first = (uintptr_t)&pages[0].page_link;
            assert(raw >= first && (raw - first) % sizeof(struct Page) == 0);
            size_t index = (raw - first) / sizeof(struct Page);
            assert(index < count && q <= count - index);
            assert(list_prev(node) == previous);
            assert(records[index].state == BUDDY_FREE_HEAD);
            assert(records[index].order == order);
            assert((nbase + index) % q == 0);
            assert(PageProperty(pages + index) && pages[index].property == q);
            for (size_t j = 0; j < q; ++j) {
                assert(!check_seen[index + j]);
                assert(!PageReserved(pages + index + j));
                assert(page_ref(pages + index + j) == 0);
                if (j != 0) {
                    assert(records[index + j].state == BUDDY_FREE_BODY);
                    assert(!PageProperty(pages + index + j));
                    assert(pages[index + j].property == 0);
                }
                check_seen[index + j] = 1;
            }
            /* A legal same-order free buddy must already have coalesced. */
            ppn_t other_ppn = (nbase + index) ^ q;
            ppn_t parent_ppn = (nbase + index) & ~q;
            if (order < BUDDY_MAX_ORDER && other_ppn >= nbase && parent_ppn >= nbase) {
                size_t other = other_ppn - nbase, parent = parent_ppn - nbase;
                if (other < count && parent < count && 2*q <= count-parent)
                    assert(records[other].state != BUDDY_FREE_HEAD || records[other].order != order);
            }
            free_total += q;
            previous = node; node = list_next(node);
        }
        assert(list_prev(root) == previous);
    }
    for (size_t i = 0; i < count; ++i) {
        if (records[i].state != BUDDY_ALLOC_HEAD) continue;
        size_t q = check_capacity(records[i].requested);
        assert(records[i].order <= BUDDY_MAX_ORDER);
        assert(((size_t)1 << records[i].order) == q);
        assert(q <= count - i && (nbase + i) % q == 0);
        for (size_t j = 0; j < q; ++j) {
            assert(!check_seen[i + j]);
            assert(records[i + j].state == (j ? BUDDY_ALLOC_BODY : BUDDY_ALLOC_HEAD));
            assert(!PageReserved(pages + i + j) && !PageProperty(pages + i + j));
            assert(pages[i + j].property == 0);
            check_seen[i + j] = 2;
        }
        allocated_total += q;
    }
    size_t registered_total = 0;
    for (size_t i = 0; i < count; ++i) {
        if (records[i].state == BUDDY_UNREGISTERED) {
            assert(!check_seen[i] && PageReserved(pages + i));
        } else {
            assert(check_seen[i] != 0);
            ++registered_total;
        }
    }
    assert(free_total == nr_free_pages());
    assert(free_total + allocated_total == registered_total);
}

/* Hash entire allocator state before a resource-failure call. This is a
 * read-only supplementary check; Host failure tests use exact byte snapshots. */
static uint64_t check_hash_bytes(uint64_t hash, const void *data, size_t n) {
    const unsigned char *bytes = data;
    for (size_t i = 0; i < n; ++i) hash = (hash ^ bytes[i]) * 1099511628211ULL;
    return hash;
}
static uint64_t check_state_hash(void) {
    uint64_t hash = check_hash_bytes(1469598103934665603ULL, records, sizeof(records));
    hash = check_hash_bytes(hash, free_lists, sizeof(free_lists));
    hash = check_hash_bytes(hash, pages, managed_pages() * sizeof(struct Page));
    return check_hash_bytes(hash, &free_count, sizeof(free_count));
}

struct check_live_block { struct Page *base; size_t requested, capacity; };

static void check_live_ledger(struct check_live_block *blocks, size_t slots, size_t initial_free) {
    size_t occupied = 0;
    for (size_t i = 0; i < slots; ++i) {
        if (blocks[i].base == NULL) continue;
        struct check_live_block *b = blocks + i;
        size_t index = checked_range(b->base, b->capacity);
        assert(b->capacity == check_capacity(b->requested));
        ppn_t ppn = page2ppn(b->base);
        assert(ppn % b->capacity == 0);
        assert(records[index].state == BUDDY_ALLOC_HEAD);
        assert(records[index].requested == b->requested);
        for (size_t j = 0; j < b->capacity; ++j) {
            assert(!PageReserved(b->base + j) && !PageProperty(b->base + j));
            assert(records[index+j].state == (j ? BUDDY_ALLOC_BODY : BUDDY_ALLOC_HEAD));
        }
        for (size_t j = 0; j < i; ++j) {
            if (blocks[j].base == NULL) continue;
            ppn_t other = page2ppn(blocks[j].base);
            assert(ppn + b->capacity <= other || other + blocks[j].capacity <= ppn);
        }
        occupied += b->capacity;
    }
    assert(occupied <= initial_free && nr_free_pages() == initial_free - occupied);
    buddy_check_invariants();
}

static void buddy_check_live_mix(void) {
    size_t before = nr_free_pages();
    size_t requests[] = {1, 2, 3, 4, 5, 8};
    struct check_live_block blocks[6] = {{0}};
    for (size_t i = 0; i < 6; ++i) {
        blocks[i].base = alloc_pages(requests[i]);
        assert(blocks[i].base != NULL);
        blocks[i].requested = requests[i];
        blocks[i].capacity = check_capacity(requests[i]);
        check_live_ledger(blocks, 6, before);
    }
    size_t release_order[] = {2, 0, 5, 1, 4, 3};
    for (size_t i = 0; i < 6; ++i) {
        struct check_live_block *b = blocks + release_order[i];
        free_pages(b->base, b->requested); b->base = NULL;
        check_live_ledger(blocks, 6, before);
    }
    assert(nr_free_pages() == before);
}

static void buddy_check_live_failure(void) {
    size_t before = nr_free_pages();
    uint64_t snapshot = check_state_hash();
    assert(alloc_pages(before + 1) == NULL);
    assert(check_state_hash() == snapshot);
    assert(alloc_pages((size_t)-1) == NULL);
    assert(check_state_hash() == snapshot);
    assert(alloc_pages(BUDDY_CAPACITY + 1) == NULL);
    assert(check_state_hash() == snapshot && nr_free_pages() == before);
    buddy_check_invariants();
}

static void buddy_check_live_stress(unsigned int iterations) {
    struct check_live_block blocks[64] = {{0}};
    unsigned int seed = 0x20261021U;
    size_t before = nr_free_pages();
    for (unsigned int step = 0; step < iterations; ++step) {
        seed = seed * 1664525U + 1013904223U;
        struct check_live_block *b = blocks + ((seed >> 16) % 64);
        if (b->base != NULL) {
            free_pages(b->base, b->requested); b->base = NULL;
        } else {
            seed = seed * 1664525U + 1013904223U;
            size_t n = 1 + (seed >> 16) % 17;
            size_t prior = nr_free_pages();
            b->base = alloc_pages(n);
            if (b->base != NULL) {
                b->requested = n; b->capacity = check_capacity(n);
            } else assert(nr_free_pages() == prior);
        }
        check_live_ledger(blocks, 64, before);
    }
    for (size_t i = 0; i < 64; ++i) {
        if (blocks[i].base != NULL) {
            free_pages(blocks[i].base, blocks[i].requested); blocks[i].base = NULL;
        }
        check_live_ledger(blocks, 64, before);
    }
    assert(nr_free_pages() == before);
}

static void buddy_check_preserve_allocation(void) {
    size_t before = nr_free_pages();
    struct Page *held = alloc_pages(5);
    assert(held != NULL && nr_free_pages() == before - 8);
    size_t index = held - pages;
    struct Page original_pages[8];
    struct buddy_record original_records[8];
    memcpy(original_pages, held, sizeof(original_pages));
    memcpy(original_records, records + index, sizeof(original_records));
    buddy_check_live_stress(128);
    assert(memcmp(original_pages, held, sizeof(original_pages)) == 0);
    assert(memcmp(original_records, records + index, sizeof(original_records)) == 0);
    free_pages(held, 5);
    assert(nr_free_pages() == before);
    buddy_check_invariants();
}

static void buddy_check(void) {
    size_t before = nr_free_pages();
    buddy_check_invariants();
    buddy_check_basic();
    buddy_check_invariants();
    cprintf("buddy_tests K01: PASS original basic check\n");
    buddy_check_live_mix();
    cprintf("buddy_tests K02: PASS requests 1,2,3,4,5,8; mixed release\n");
    buddy_check_live_failure();
    cprintf("buddy_tests K03: PASS unavailable and overflowing requests preserve state\n");
    buddy_check_live_stress(1024);
    cprintf("buddy_tests K04: PASS seed=0x20261021 steps=1024 ownership and conservation\n");
    buddy_check_preserve_allocation();
    cprintf("buddy_tests K05: PASS existing 5-page request survives 128 stress steps\n");
    assert(nr_free_pages() == before);
    cprintf("buddy_tests: all 5 live checks passed; restored %u free pages\n", (unsigned int)before);
}
