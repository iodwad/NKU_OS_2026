/* Compile the actual allocator in this translation unit to permit read-only
 * invariant inspection. No substitute allocator or system-header ABI types. */
#include "../kern/mm/buddy_pmm.c"
extern int vprintf(const char *, __builtin_va_list);
extern void exit(int) __attribute__((noreturn));

#define LIVE_LIMIT 512
static struct Page arena[BUDDY_CAPACITY];
struct Page *pages = arena;
const size_t nbase = DRAM_BASE / PGSIZE;
size_t npage;
uint64_t va_pa_offset;
const struct pmm_manager *pmm_manager = &buddy_pmm_manager;
static unsigned char registered[BUDDY_CAPACITY], seen[BUDDY_CAPACITY];
static unsigned short owners[BUDDY_CAPACITY];
struct allocation { struct Page *base; size_t n, capacity; };
static struct allocation live[LIVE_LIMIT];
static size_t domain, registered_count;
static const char *case_name;
static unsigned int operations;

int cprintf(const char *fmt, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    int result = vprintf(fmt, ap);
    __builtin_va_end(ap);
    return result;
}
#define CHECK(condition) do { if (!(condition)) { \
    cprintf("FAIL %s line %d: %s\n", case_name, __LINE__, #condition); exit(1); \
} } while (0)

struct snapshot {
    struct buddy_record meta[BUDDY_CAPACITY];
    struct Page descriptors[BUDDY_CAPACITY];
    list_entry_t lists[BUDDY_MAX_ORDER + 1];
    size_t count;
    bool ready;
};
static struct snapshot saved;
static bool expected_panic;

static void save_state(void) {
    memcpy(saved.meta, records, sizeof(records));
    memcpy(saved.descriptors, arena, sizeof(arena));
    memcpy(saved.lists, free_lists, sizeof(free_lists));
    saved.count = free_count;
    saved.ready = initialized;
}
static bool unchanged(void) {
    return saved.count == free_count && saved.ready == initialized &&
        memcmp(saved.meta, records, sizeof(records)) == 0 &&
        memcmp(saved.descriptors, arena, sizeof(arena)) == 0 &&
        memcmp(saved.lists, free_lists, sizeof(free_lists)) == 0;
}
void __panic(const char *file, int line, const char *fmt, ...) {
    cprintf("PANIC %s:%d ", file, line);
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    vprintf(fmt, ap);
    __builtin_va_end(ap);
    cprintf("\n");
    if (expected_panic) {
        /* Require the production allocator's file suffix, not a harness assert. */
        size_t len = strlen(file), suffix = strlen("buddy_pmm.c");
        if (len >= suffix && strcmp(file + len - suffix, "buddy_pmm.c") == 0 && unchanged()) {
            cprintf("PASS %s expected allocator panic; full state unchanged\n", case_name);
            exit(0);
        }
    }
    cprintf("FAIL %s unexpected panic or state mutation\n", case_name);
    exit(1);
}
struct Page *alloc_pages(size_t n) { return pmm_manager->alloc_pages(n); }
void free_pages(struct Page *p, size_t n) { pmm_manager->free_pages(p,n); }
size_t nr_free_pages(void) { return pmm_manager->nr_free_pages(); }

static size_t rounded(size_t n) {
    size_t q = 1;
    CHECK(n > 0 && n <= BUDDY_CAPACITY);
    while (q < n) q *= 2;
    return q;
}

/* Independent oracle: scan registered, unowned pages for an aligned interval.
 * It neither splits blocks nor applies the allocator's XOR merge procedure. */
static bool possible(size_t n) {
    if (n == 0 || n > domain) return 0;
    size_t q = rounded(n);
    for (size_t i = 0; i + q <= domain; ++i) {
        if ((nbase + i) % q) continue;
        size_t j = 0;
        while (j < q && registered[i+j] && owners[i+j] == 0) ++j;
        if (j == q) return 1;
    }
    return 0;
}

static void verify(void) {
    memset(seen,0,sizeof(seen));
    size_t listed = 0, occupied = 0;
    for (unsigned int order = 0; order <= BUDDY_MAX_ORDER; ++order) {
        list_entry_t *sentinel = &free_lists[order], *node = sentinel->next;
        list_entry_t *previous = sentinel;
        size_t steps = 0, q = (size_t)1 << order;
        while (node != sentinel) {
            CHECK(++steps <= domain);
            uintptr_t raw = (uintptr_t)node, first = (uintptr_t)&arena[0].page_link;
            CHECK(raw >= first && (raw-first) % sizeof(struct Page) == 0);
            size_t index = (raw-first) / sizeof(struct Page);
            CHECK(index < domain && q <= domain-index);
            CHECK(node->prev == previous && node->next->prev == node);
            CHECK((nbase+index) % q == 0);
            CHECK(arena[index].property == q && PageProperty(arena+index));
            CHECK(records[index].state == BUDDY_FREE_HEAD && records[index].order == order);
            for (size_t j = 0; j < q; ++j) {
                size_t at = index+j;
                CHECK(registered[at] && owners[at] == 0 && !seen[at]);
                CHECK(!PageReserved(arena+at) && arena[at].ref == 0);
                if (j) CHECK(!PageProperty(arena+at) && arena[at].property == 0 && records[at].state == BUDDY_FREE_BODY);
                seen[at] = 1;
            }
            listed += q;
            previous = node; node = node->next;
        }
        CHECK(sentinel->prev == previous);
    }
    for (size_t slot = 0; slot < LIVE_LIMIT; ++slot) {
        if (!live[slot].base) continue;
        size_t index = live[slot].base-arena, q = live[slot].capacity;
        CHECK(q == rounded(live[slot].n) && (nbase+index) % q == 0);
        CHECK(records[index].requested == live[slot].n);
        CHECK(records[index].state == BUDDY_ALLOC_HEAD);
        for (size_t j = 0; j < q; ++j) {
            CHECK(index+j < domain && registered[index+j]);
            CHECK(owners[index+j] == slot+1 && !seen[index+j]);
            CHECK(!PageReserved(arena+index+j) && !PageProperty(arena+index+j));
            CHECK(arena[index+j].property == 0);
            if (j) CHECK(records[index+j].state == BUDDY_ALLOC_BODY);
            seen[index+j] = 2;
        }
        occupied += q;
    }
    for (size_t i = 0; i < domain; ++i) {
        CHECK(registered[i] ? seen[i] != 0 : seen[i] == 0);
        if (!registered[i]) CHECK(PageReserved(arena+i) && records[i].state == BUDDY_UNREGISTERED);
    }
    CHECK(listed == nr_free_pages());
    CHECK(listed + occupied == registered_count);
}

static void setup(size_t count) {
    CHECK(count <= BUDDY_CAPACITY);
    memset(arena,0,sizeof(arena)); memset(registered,0,sizeof(registered));
    memset(owners,0,sizeof(owners)); memset(live,0,sizeof(live));
    domain = count; npage = nbase+count; registered_count = 0;
    for (size_t i = 0; i < BUDDY_CAPACITY; ++i) SetPageReserved(arena+i);
    pmm_manager->init();
    CHECK(nr_free_pages() == 0);
    verify();
}
static void add_region(size_t start, size_t length) {
    size_t before = nr_free_pages();
    pmm_manager->init_memmap(arena+start,length);
    for (size_t i = start; i < start+length; ++i) {
        CHECK(!registered[i]); registered[i] = 1;
    }
    registered_count += length;
    CHECK(nr_free_pages() == before+length); verify();
}
static int take(size_t n) {
    bool available = possible(n);
    size_t before = nr_free_pages();
    save_state();
    struct Page *p = alloc_pages(n);
    ++operations;
    CHECK((p != NULL) == available);
    if (p == NULL) {
        CHECK(unchanged()); verify();
        cprintf("TRACE op=%u alloc_fail n=%llu free=%llu\n",operations,n,before);
        return -1;
    }
    size_t slot = 0;
    while (slot < LIVE_LIMIT && live[slot].base) ++slot;
    CHECK(slot < LIVE_LIMIT);
    size_t q = rounded(n), index = p-arena;
    CHECK(index < domain && q <= domain-index && (nbase+index) % q == 0);
    for (size_t i = 0; i < q; ++i) {
        CHECK(registered[index+i] && owners[index+i] == 0);
        owners[index+i] = slot+1;
    }
    live[slot] = (struct allocation){p,n,q};
    CHECK(nr_free_pages() == before-q); verify();
    cprintf("TRACE op=%u alloc base=%p ppn=0x%llx n=%llu q=%llu free=%llu\n",
            operations,(void *)p,page2ppn(p),n,q,nr_free_pages());
    return slot;
}
static void release(int slot) {
    CHECK(slot >= 0 && slot < LIVE_LIMIT && live[slot].base != NULL);
    size_t before = nr_free_pages(), q = live[slot].capacity;
    size_t index = live[slot].base-arena;
    struct allocation original = live[slot];
    free_pages(live[slot].base,live[slot].n);
    ++operations;
    for (size_t i = 0; i < q; ++i) { CHECK(owners[index+i] == slot+1); owners[index+i] = 0; }
    live[slot].base = NULL;
    CHECK(nr_free_pages() == before+q); verify();
    cprintf("TRACE op=%u free base=%p ppn=0x%llx n=%llu q=%llu free=%llu\n",
            operations,(void *)original.base,page2ppn(original.base),original.n,q,nr_free_pages());
}
static void drain(void) {
    for (int i = LIVE_LIMIT-1; i >= 0; --i) if (live[i].base) release(i);
    CHECK(nr_free_pages() == registered_count); verify();
}
static void expect_panic(void) { save_state(); expected_panic = 1; }
static unsigned int rng(unsigned int *seed) { *seed = *seed*1664525U+1013904223U; return *seed; }

static void run_case(const char *id) {
    setup(256);
    if (!strcmp(id,"H01")) { add_region(0,16); CHECK(take(16)>=0); drain(); }
    else if (!strcmp(id,"H02")) { add_region(0,13); for(int i=0;i<13;++i) CHECK(take(1)>=0); CHECK(take(1)==-1); drain(); }
    else if (!strcmp(id,"H03")) { add_region(3,29); for(int i=0;i<29;++i) CHECK(take(1)>=0); CHECK(take(1)==-1); drain(); }
    else if (!strcmp(id,"H04")) { add_region(0,8); add_region(16,8); CHECK(take(16)==-1); CHECK(take(8)>=0); CHECK(take(8)>=0); drain(); }
    else if (!strcmp(id,"H05")) { add_region(8,8); add_region(0,8); CHECK(take(16)>=0); drain(); }
    else if (!strcmp(id,"H06")) {
        add_region(0,64); size_t ns[]={1,2,3,4,5,8}; int slots[6];
        for(int i=0;i<6;++i) { slots[i]=take(ns[i]); CHECK(slots[i]>=0); }
        for(int i=5;i>=0;--i) release(slots[i]);
    }
    else if (!strcmp(id,"H07")) {
        add_region(0,16); CHECK(take(1)>=0); CHECK(take(2)>=0); CHECK(take(4)>=0);
        for(int i=0;i<9;++i) { CHECK(take(1)>=0); }
        CHECK(take(1)==-1); drain(); CHECK(take(16)>=0); drain();
    }
    else if (!strcmp(id,"H08") || !strcmp(id,"H09")) {
        add_region(0,16); int a=take(8),b=take(8); CHECK(a>=0&&b>=0); release(a);
        CHECK(take(16)==-1); if(!strcmp(id,"H09")) { release(b); CHECK(take(16)>=0); } drain();
    }
    else if (!strcmp(id,"H10") || !strcmp(id,"H11")) {
        add_region(0,16); int slots[16]; for(int i=0;i<16;++i) { slots[i]=take(1); CHECK(slots[i]>=0); }
        for(int i=0;i<16;++i) release(slots[!strcmp(id,"H10")?i:(i*5)%16]);
        CHECK(take(16)>=0); drain();
    }
    else if (!strcmp(id,"H12")) { add_region(4,4); add_region(8,4); CHECK(nr_free_pages()==8); CHECK(take(8)==-1); CHECK(take(4)>=0); CHECK(take(4)>=0); drain(); }
    else if (!strcmp(id,"H13") || !strcmp(id,"H14")) {
        add_region(0,16); int slot=take(!strcmp(id,"H13")?3:5); CHECK(slot>=0);
        while(take(1)>=0) {} drain(); CHECK(take(16)>=0); drain();
    }
    else if (!strcmp(id,"H15")) {
        add_region(0,32); size_t ns[]={1,3,5,2,4}; int slots[5];
        for(int i=0;i<5;++i) { slots[i]=take(ns[i]); CHECK(slots[i]>=0); }
        int order[]={2,0,4,1,3}; for(int i=0;i<5;++i) release(slots[order[i]]);
        CHECK(take(32)>=0); drain();
    }
    else if (!strcmp(id,"H16")) { add_region(0,16); CHECK(take(17)==-1); CHECK(take(16)>=0); drain(); }
    else if (!strcmp(id,"H17")) {
        add_region(0,16); int slots[16]; for(int i=0;i<16;++i) { slots[i]=take(1); CHECK(slots[i]>=0); }
        for(int i=0;i<16;++i) if(((live[slots[i]].base-arena)&1)==0) release(slots[i]);
        CHECK(nr_free_pages()==8); CHECK(take(2)==-1); CHECK(take(1)>=0); drain(); CHECK(take(16)>=0); drain();
    }
    else if (!strcmp(id,"H18")) {
        setup(BUDDY_CAPACITY); add_region(0,BUDDY_CAPACITY);
        CHECK(take(BUDDY_CAPACITY+1)==-1); CHECK(take((size_t)-1)==-1);
        CHECK(take(BUDDY_CAPACITY)>=0); CHECK(take(1)==-1); drain();
        CHECK(take(BUDDY_CAPACITY/2+1)>=0); drain();
    }
    else if (!strcmp(id,"H19") || !strcmp(id,"H20")) {
        add_region(3,77); add_region(96,137);
        unsigned int seed=!strcmp(id,"H19")?0x20261021U:0x13579BDFU;
        for(int i=0;i<10000;++i) {
            unsigned int slot=(rng(&seed)>>16)%64;
            if(live[slot].base) release(slot); else (void)take(1+(rng(&seed)>>16)%33);
        }
        drain(); CHECK(take(32)>=0); drain();
    }
    else if (!strcmp(id,"H21")) {
        add_region(0,128); int held=take(5); CHECK(held>=0);
        set_page_ref(live[held].base,7);
        pmm_manager->check(); verify(); CHECK(live[held].base!=NULL);
        CHECK(page_ref(live[held].base)==7); release(held);
    }
    else if (!strcmp(id,"H22")) { add_region(0,16); expect_panic(); (void)alloc_pages(0); }
    else if (!strcmp(id,"H23")) { add_region(0,16); int slot=take(3); struct Page*p=live[slot].base; release(slot); expect_panic(); free_pages(p,3); }
    else if (!strcmp(id,"H24")) { add_region(0,16); int slot=take(3); expect_panic(); free_pages(live[slot].base+1,3); }
    else if (!strcmp(id,"H25")) { add_region(0,16); int slot=take(3); expect_panic(); free_pages(live[slot].base,4); }
    else if (!strcmp(id,"H26")) { add_region(0,16); int slot=take(3); expect_panic(); free_pages(live[slot].base,2); }
    else if (!strcmp(id,"H27")) { add_region(0,16); expect_panic(); free_pages(arena+32,1); }
    else if (!strcmp(id,"H28")) { add_region(0,16); ClearPageReserved(arena+32); expect_panic(); free_pages(arena+32,1); }
    else if (!strcmp(id,"H29")) { add_region(8,8); expect_panic(); pmm_manager->init_memmap(arena+8,8); }
    else if (!strcmp(id,"H30")) { add_region(8,8); expect_panic(); pmm_manager->init_memmap(arena+4,8); }
    else if (!strcmp(id,"H31")) { expect_panic(); pmm_manager->init_memmap(arena+250,10); }
    else if (!strcmp(id,"H32")) { expect_panic(); pmm_manager->init_memmap((struct Page *)((uintptr_t)arena-sizeof(struct Page)),1); }
    else if (!strcmp(id,"H33")) { expect_panic(); pmm_manager->init_memmap(arena,0); }
    else if (!strcmp(id,"H34")) { expect_panic(); pmm_manager->init_memmap(arena,(size_t)-1); }
    else if (!strcmp(id,"H35")) { expect_panic(); free_pages(NULL,1); }
    else if (!strcmp(id,"H36")) { expect_panic(); pmm_manager->init_memmap((struct Page *)((uintptr_t)arena+1),1); }
    else if (!strcmp(id,"H37")) { expect_panic(); free_pages(arena+256,1); }
    else if (!strcmp(id,"H38")) { add_region(0,16); int slot=take(1); expect_panic(); free_pages(live[slot].base,0); }
    else if (!strcmp(id,"H39")) { add_region(0,16); int slot=take(1); expect_panic(); free_pages(live[slot].base,(size_t)-1); }
    else if (!strcmp(id,"H40")) { CHECK(take(1)==-1); CHECK(take((size_t)-1)==-1); }
    else if (!strcmp(id,"H41")) { add_region(0,16); CHECK(take((size_t)-1)==-1); CHECK(take(BUDDY_CAPACITY+1)==-1); CHECK(take(16)>=0); drain(); }
    else if (!strcmp(id,"H42")) {
        add_region(0,16); int held=take(8); CHECK(held>=0);
        add_region(32,8); CHECK(live[held].base!=NULL);
        CHECK(take(8)>=0); CHECK(take(8)>=0); drain();
    }
    else if (!strcmp(id,"H43")) {
        ClearPageReserved(arena+7); expect_panic(); pmm_manager->init_memmap(arena,8);
    }
    else { cprintf("unknown case %s\n",id); exit(2); }
    CHECK(!expected_panic); drain();
}
int main(int argc,char **argv) {
    if(argc!=2) return 2;
    case_name=argv[1]; run_case(case_name);
    cprintf("PASS %s operations=%u registered=%u restored=%u\n",case_name,operations,(unsigned int)registered_count,(unsigned int)nr_free_pages());
    return 0;
}
