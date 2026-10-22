/* Real production sources; only the virtual-address platform is adapted. */
#include "../kern/mm/buddy_pmm.c"
#undef PHYSICAL_MEMORY_OFFSET
#undef KERNBASE
#undef KERNTOP
#define HOST_RAM_VA 0x400000000000ULL
#define PHYSICAL_MEMORY_OFFSET (HOST_RAM_VA - DRAM_BASE)
#define KERNBASE (HOST_RAM_VA + KERNEL_BEGIN_PADDR - DRAM_BASE)
#define KERNTOP (HOST_RAM_VA + PHYSICAL_MEMORY_END - DRAM_BASE)
#include "../kern/mm/slub.c"

extern int vprintf(const char *, __builtin_va_list);
extern void exit(int) __attribute__((noreturn));
extern void *mmap(void *,size_t,int,int,int,long);
extern void __asan_poison_memory_region(const volatile void *,size_t);
extern void __asan_unpoison_memory_region(const volatile void *,size_t);

#define TEST_FIRST_PAGE ((KERNEL_BEGIN_PADDR - DRAM_BASE)/PGSIZE)
#define TEST_MAX_PAGES 256
#define TEST_MAX_OBJECTS 1024
#define TEST_HEADER 96
static struct Page descriptors[BUDDY_CAPACITY];
struct Page *pages=descriptors;
const size_t nbase=DRAM_BASE/PGSIZE;
size_t npage;
uint64_t va_pa_offset=PHYSICAL_MEMORY_OFFSET;
const struct pmm_manager *pmm_manager=&buddy_pmm_manager;
static size_t resource_pages, initial_free;
static const char *test_id;
static unsigned int sequence, identity;
static bool stress_mode, raw_mode;
static size_t pmm_original[BUDDY_CAPACITY], pmm_actual[BUDDY_CAPACITY];
static unsigned char pmm_raw[BUDDY_CAPACITY];

int cprintf(const char *fmt,...) {
    __builtin_va_list ap; __builtin_va_start(ap,fmt);
    int result=vprintf(fmt,ap); __builtin_va_end(ap); return result;
}
#define REQUIRE(x) do { if (!(x)) { cprintf("FAIL %s:%d %s\n",test_id,__LINE__,#x); exit(1); } } while (0)
static size_t round_capacity(size_t n) { size_t q=1; while(q<n) q*=2; return q; }

struct Page *alloc_pages(size_t n) {
    struct Page *p=pmm_manager->alloc_pages(n);
    if(p) {
        size_t i=p-pages,q=round_capacity(n);
        REQUIRE(i>=TEST_FIRST_PAGE && q<=npage-nbase-i && pmm_original[i]==0);
        pmm_original[i]=n; pmm_actual[i]=q; pmm_raw[i]=raw_mode;
        cprintf("PMM alloc ppn=0x%llx original=%llu actual=%llu raw=%u\n",page2ppn(p),n,q,(unsigned int)raw_mode);
    }
    return p;
}
void free_pages(struct Page *p,size_t n) {
    size_t i=p-pages;
    REQUIRE(i<npage-nbase && pmm_original[i]==n);
    cprintf("PMM free ppn=0x%llx original=%llu actual=%llu\n",page2ppn(p),n,pmm_actual[i]);
    pmm_manager->free_pages(p,n);
    pmm_original[i]=pmm_actual[i]=0; pmm_raw[i]=0;
}
size_t nr_free_pages(void) { return pmm_manager->nr_free_pages(); }
static struct Page *borrow(size_t n) { raw_mode=1; struct Page *p=alloc_pages(n); raw_mode=0; return p; }

struct object_record { unsigned char *ptr; size_t size; unsigned int signature,id; };
static struct object_record objects[TEST_MAX_OBJECTS];
struct expected_slab {
    uintptr_t address;
    size_t width,pages,capacity,count;
    uint64_t bits[4];
    bool linked;
};
static struct expected_slab groups[TEST_MAX_PAGES];
static struct slub_slab *expected_owner[TEST_MAX_PAGES];
static unsigned char page_busy[TEST_MAX_PAGES];

static size_t expected_width(size_t n) {
    const size_t classes[]={16,32,64,128,256,512,1024,2048};
    for(size_t i=0;i<8;++i) if(n<=classes[i]) return classes[i];
    return n;
}
static size_t expected_pages(size_t n) {
    if(n<=2048) return 1;
    if(n>SLUB_PAGE_LIMIT*PGSIZE-TEST_HEADER) return 0;
    return (n+TEST_HEADER)/PGSIZE+((n+TEST_HEADER)%PGSIZE!=0);
}
static unsigned char pattern(unsigned int signature,size_t offset) {
    return (unsigned char)(signature+offset*17+(offset>>8)*31);
}
static void fill(struct object_record *r) {
    for(size_t j=0;j<r->size;++j) r->ptr[j]=pattern(r->signature,j);
}
static void check_data(struct object_record *r,bool full) {
    REQUIRE(r->ptr!=NULL && r->size>0);
    if(full || r->size<=96) {
        for(size_t j=0;j<r->size;++j) REQUIRE(r->ptr[j]==pattern(r->signature,j));
    } else {
        for(size_t j=0;j<32;++j) {
            REQUIRE(r->ptr[j]==pattern(r->signature,j));
            size_t at=r->size-1-j;
            REQUIRE(r->ptr[at]==pattern(r->signature,at));
            at=(r->size/2+j)%r->size;
            REQUIRE(r->ptr[at]==pattern(r->signature,at));
        }
    }
}

static void verify(void) {
    memset(groups,0,sizeof(groups)); memset(expected_owner,0,sizeof(expected_owner));
    memset(page_busy,0,sizeof(page_busy));
    size_t active=0,slabs=0,occupied=0;
    for(size_t i=0;i<TEST_MAX_OBJECTS;++i) {
        struct object_record *r=objects+i;
        if(!r->ptr) continue;
        uintptr_t p=(uintptr_t)r->ptr;
        REQUIRE(p>=KERNBASE && p<KERNTOP && r->size<=KERNTOP-p && p%16==0);
        uintptr_t h=p&~(uintptr_t)(PGSIZE-1);
        size_t index=((h-PHYSICAL_MEMORY_OFFSET)>>PGSHIFT)-nbase;
        REQUIRE(index>=TEST_FIRST_PAGE && index<TEST_FIRST_PAGE+resource_pages);
        size_t relative=index-TEST_FIRST_PAGE,width=expected_width(r->size),np=expected_pages(r->size);
        REQUIRE(np>0);
        struct expected_slab *g=groups+relative;
        if(!g->address) {
            g->address=h; g->width=width; g->pages=np; g->capacity=round_capacity(np); ++slabs;
            REQUIRE(g->capacity<=resource_pages-relative);
            REQUIRE(pmm_original[index]==np && pmm_actual[index]==g->capacity && !pmm_raw[index]);
            for(size_t j=0;j<g->capacity;++j) {
                REQUIRE(expected_owner[relative+j]==NULL);
                expected_owner[relative+j]=(struct slub_slab *)h;
            }
        }
        REQUIRE(g->width==width && g->pages==np);
        size_t slot=0;
        if(r->size<=2048) {
            REQUIRE(p>=h+TEST_HEADER && (p-h-TEST_HEADER)%width==0);
            slot=(p-h-TEST_HEADER)/width;
            REQUIRE(slot<(PGSIZE-TEST_HEADER)/width && r->size<=width);
        } else {
            REQUIRE(p==h+TEST_HEADER && g->count==0);
            REQUIRE(r->size<=np*PGSIZE-TEST_HEADER);
        }
        REQUIRE(!(g->bits[slot/64]&(1ULL<<(slot%64))));
        g->bits[slot/64]|=1ULL<<(slot%64); ++g->count; ++active;
        check_data(r,!stress_mode || sequence%128==0);
    }
    for(size_t i=0;i<resource_pages;++i) {
        size_t index=TEST_FIRST_PAGE+i;
        if(pmm_original[index]) {
            size_t q=pmm_actual[index]; REQUIRE(q==round_capacity(pmm_original[index]) && q<=resource_pages-i);
            for(size_t j=0;j<q;++j) { REQUIRE(!page_busy[i+j]); page_busy[i+j]=1; }
            occupied+=q;
            if(!pmm_raw[index]) REQUIRE(groups[i].address!=0);
        }
        struct expected_slab *g=groups+i;
        if(!g->address) continue;
        struct slub_slab *h=(struct slub_slab *)g->address;
        REQUIRE(h->magic==SLUB_MAGIC && h->page==pages+index && h->page_request==g->pages);
        REQUIRE(h->object_bytes==g->width && h->inuse==g->count);
        size_t capacity=g->width<=2048?(PGSIZE-TEST_HEADER)/g->width:1;
        REQUIRE(h->slots==capacity);
        REQUIRE(h->class_id==(g->width>2048?8:(unsigned int)__builtin_ctzll(g->width)-4));
        for(size_t j=0;j<4;++j) REQUIRE(h->used[j]==g->bits[j]);
        if(g->width<=2048) {
            unsigned char seen_slots[256]={0}; size_t cursor=h->free_head,free_slots=0;
            while(cursor!=SLUB_NO_SLOT) {
                REQUIRE(cursor<capacity && !seen_slots[cursor] && !(g->bits[cursor/64]&(1ULL<<(cursor%64))));
                seen_slots[cursor]=1; ++free_slots;
                /* Only inspect allocator free-list metadata, never freed payload patterns. */
                cursor=*(uint16_t *)(g->address+TEST_HEADER+cursor*g->width);
            }
            REQUIRE(free_slots+g->count==capacity);
        }
    }
    for(size_t c=0;c<8;++c) {
        list_entry_t *head=partial+c,*node=head->next,*previous=head; size_t steps=0;
        while(node!=head) {
            REQUIRE(++steps<=resource_pages);
            uintptr_t value=(uintptr_t)node;
            REQUIRE(value>=KERNBASE && value<KERNTOP);
            struct slub_slab *h=to_struct(node,struct slub_slab,link);
            size_t relative=(((uintptr_t)h-PHYSICAL_MEMORY_OFFSET)>>PGSHIFT)-nbase-TEST_FIRST_PAGE;
            REQUIRE(relative<resource_pages && groups[relative].address==(uintptr_t)h);
            struct expected_slab *g=groups+relative;
            REQUIRE(!g->linked && h->class_id==c && g->count<h->slots && node->prev==previous);
            g->linked=1; previous=node; node=node->next;
        }
        REQUIRE(head->prev==previous);
    }
    for(size_t i=0;i<resource_pages;++i) {
        REQUIRE(owner[TEST_FIRST_PAGE+i]==expected_owner[i]);
        if(groups[i].address) {
            struct slub_slab *h=(struct slub_slab *)groups[i].address;
            REQUIRE(groups[i].linked==(h->class_id<8 && groups[i].count<h->slots));
        }
    }
    for(size_t i=0;i<TEST_FIRST_PAGE;++i) REQUIRE(owner[i]==NULL);
    for(size_t i=TEST_FIRST_PAGE+resource_pages;i<SLUB_PAGE_LIMIT;++i) REQUIRE(owner[i]==NULL);
    REQUIRE(live_objects==active && slab_count==slabs);
    REQUIRE(occupied<=resource_pages && nr_free_pages()==resource_pages-occupied);
    buddy_check_invariants();
}

static bool possible(size_t n) {
    if(n==0) return 0;
    size_t width=expected_width(n),np=expected_pages(n);
    if(np==0) return 0;
    if(n<=2048) {
        for(size_t i=0;i<resource_pages;++i)
            if(groups[i].address && groups[i].width==width && groups[i].count<(PGSIZE-TEST_HEADER)/width) return 1;
    }
    size_t q=round_capacity(np);
    if(q>resource_pages) return 0;
    for(size_t i=0;i+q<=resource_pages;++i) {
        if((nbase+TEST_FIRST_PAGE+i)%q) continue;
        size_t j=0; while(j<q && !page_busy[i+j]) ++j;
        if(j==q) return 1;
    }
    return 0;
}

struct snapshot {
    struct Page descriptors[BUDDY_CAPACITY];
    struct buddy_record records[BUDDY_CAPACITY];
    list_entry_t free_lists[BUDDY_MAX_ORDER+1],partial[8];
    struct slub_slab *owners[SLUB_PAGE_LIMIT];
    size_t original[BUDDY_CAPACITY],actual[BUDDY_CAPACITY];
    unsigned char raw[BUDDY_CAPACITY],data[TEST_MAX_PAGES*PGSIZE];
    size_t free,objects,slabs; bool ready;
};
static struct snapshot snapshot;
static const char *expected_assertion;
struct snapshot_segment { size_t offset,bytes; };
static struct snapshot_segment snapshot_segments[65536];
static size_t snapshot_segment_count;
static void capture_segment(uintptr_t address,size_t bytes) {
    REQUIRE(address>=KERNBASE && address-KERNBASE<=resource_pages*PGSIZE);
    size_t offset=address-KERNBASE;
    REQUIRE(bytes<=resource_pages*PGSIZE-offset && snapshot_segment_count<65536);
    snapshot_segments[snapshot_segment_count++]=(struct snapshot_segment){offset,bytes};
    memcpy(snapshot.data+offset,(void *)address,bytes);
}
static void capture(void) {
    memcpy(snapshot.descriptors,descriptors,sizeof(descriptors)); memcpy(snapshot.records,records,sizeof(records));
    memcpy(snapshot.free_lists,free_lists,sizeof(free_lists)); memcpy(snapshot.partial,partial,sizeof(partial));
    memcpy(snapshot.owners,owner,sizeof(owner)); memcpy(snapshot.original,pmm_original,sizeof(pmm_original));
    memcpy(snapshot.actual,pmm_actual,sizeof(pmm_actual)); memcpy(snapshot.raw,pmm_raw,sizeof(pmm_raw));
    /* Do not inspect freed user payload. Snapshot active data, valid headers,
     * and the two-byte free-list links that are allocator metadata. */
    snapshot_segment_count=0;
    for(size_t i=0;i<TEST_MAX_OBJECTS;++i)
        if(objects[i].ptr) capture_segment((uintptr_t)objects[i].ptr,objects[i].size);
    for(size_t i=0;i<resource_pages;++i) {
        struct expected_slab *g=groups+i;
        if(g->address) {
            capture_segment(g->address,TEST_HEADER);
            if(g->width<=2048) for(size_t slot=0;slot<(PGSIZE-TEST_HEADER)/g->width;++slot)
                if(!(g->bits[slot/64]&(1ULL<<(slot%64))))
                    capture_segment(g->address+TEST_HEADER+slot*g->width,sizeof(uint16_t));
        }
        if(pmm_original[TEST_FIRST_PAGE+i] && pmm_raw[TEST_FIRST_PAGE+i])
            capture_segment(KERNBASE+i*PGSIZE,pmm_actual[TEST_FIRST_PAGE+i]*PGSIZE);
    }
    snapshot.free=nr_free_pages(); snapshot.objects=live_objects; snapshot.slabs=slab_count; snapshot.ready=slub_ready;
}
static bool unchanged(void) {
    bool same=snapshot.free==nr_free_pages() && snapshot.objects==live_objects && snapshot.slabs==slab_count && snapshot.ready==slub_ready &&
      !memcmp(snapshot.descriptors,descriptors,sizeof(descriptors)) && !memcmp(snapshot.records,records,sizeof(records)) &&
      !memcmp(snapshot.free_lists,free_lists,sizeof(free_lists)) && !memcmp(snapshot.partial,partial,sizeof(partial)) &&
      !memcmp(snapshot.owners,owner,sizeof(owner)) && !memcmp(snapshot.original,pmm_original,sizeof(pmm_original)) &&
      !memcmp(snapshot.actual,pmm_actual,sizeof(pmm_actual)) && !memcmp(snapshot.raw,pmm_raw,sizeof(pmm_raw));
    for(size_t i=0;i<snapshot_segment_count;++i) {
        struct snapshot_segment s=snapshot_segments[i];
        if(memcmp(snapshot.data+s.offset,(void *)(KERNBASE+s.offset),s.bytes)) same=0;
    }
    return same;
}
void __panic(const char *file,int line,const char *fmt,...) {
    __builtin_va_list ap,copy; __builtin_va_start(ap,fmt); __builtin_va_copy(copy,ap);
    const char *expression=!strcmp(fmt,"assertion failed: %s")?__builtin_va_arg(copy,const char *):NULL;
    __builtin_va_end(copy); cprintf("PANIC %s:%d ",file,line); vprintf(fmt,ap); __builtin_va_end(ap); cprintf("\n");
    size_t length=strlen(file);
    if(expected_assertion && expression && length>=6 && !strcmp(file+length-6,"slub.c") &&
       !strcmp(expression,expected_assertion) && unchanged()) {
        cprintf("PASS %s exact expected assertion; pre-termination state unchanged\n",test_id); exit(0);
    }
    cprintf("FAIL %s unexpected assertion/cause/state change\n",test_id); exit(1);
}
static void expect(const char *expression) { capture(); expected_assertion=expression; }

static int take(size_t n) {
    bool available=possible(n); capture();
    unsigned char *p=slub_alloc(n); ++sequence;
    REQUIRE((p!=NULL)==available);
    if(!p) { REQUIRE(unchanged()); verify(); cprintf("TRACE op=%u fail size=%llu free=%llu\n",sequence,n,nr_free_pages()); return -1; }
    int slot=0; while(slot<TEST_MAX_OBJECTS && objects[slot].ptr) ++slot;
    REQUIRE(slot<TEST_MAX_OBJECTS);
    unsigned int id=++identity;
    objects[slot]=(struct object_record){p,n,(id*2654435761U)^(unsigned int)n,id};
    fill(objects+slot); verify();
    cprintf("TRACE op=%u alloc slot=%u id=%u ptr=%p size=%llu signature=%u free=%llu\n",sequence,(unsigned int)slot,id,p,n,objects[slot].signature,nr_free_pages());
    return slot;
}
static void release(int slot) {
    REQUIRE(slot>=0 && slot<TEST_MAX_OBJECTS && objects[slot].ptr);
    struct object_record r=objects[slot]; check_data(&r,1);
    slub_free(r.ptr); objects[slot].ptr=NULL; ++sequence; verify();
    cprintf("TRACE op=%u free slot=%u id=%u ptr=%p size=%llu signature=%u free=%llu\n",sequence,(unsigned int)slot,r.id,r.ptr,r.size,r.signature,nr_free_pages());
}
static void drain(void) {
    for(int i=TEST_MAX_OBJECTS-1;i>=0;--i) if(objects[i].ptr) release(i);
    verify(); REQUIRE(nr_free_pages()==initial_free && live_objects==0 && slab_count==0);
}
static void setup(size_t count,bool initialize_slub) {
    resource_pages=count; REQUIRE(count<=TEST_MAX_PAGES);
    npage=nbase+TEST_FIRST_PAGE+count;
    for(size_t i=0;i<BUDDY_CAPACITY;++i) SetPageReserved(pages+i);
    pmm_manager->init();
    if(count) pmm_manager->init_memmap(pages+TEST_FIRST_PAGE,count);
    initial_free=nr_free_pages(); REQUIRE(initial_free==count);
    __asan_poison_memory_region((void *)HOST_RAM_VA,PHYSICAL_MEMORY_END-DRAM_BASE);
    __asan_unpoison_memory_region((void *)KERNBASE,count*PGSIZE);
    memset((void *)KERNBASE,0,count*PGSIZE);
    if(initialize_slub) { slub_init(); REQUIRE(nr_free_pages()==initial_free); verify(); }
}
static unsigned int random_next(unsigned int *seed) { *seed=*seed*1664525U+1013904223U; return *seed; }

static void run(const char *id) {
    size_t resources=128;
    if(!strcmp(id,"expand") || !strcmp(id,"oom_small") || !strcmp(id,"oom_16")) resources=2;
    if(!strcmp(id,"oom_large")) resources=4;
    if(!strcmp(id,"no_pages")) resources=0;
    if(!strcmp(id,"free_slots_oom")) resources=1;
    if(!strcmp(id,"fragmented")) resources=8;
    setup(resources,strcmp(id,"N16")!=0);
    if(!strncmp(id,"size_",5)) {
        size_t n=(size_t)strtol(id+5,NULL,10); int s=take(n); REQUIRE(s>=0); release(s);
    } else if(!strcmp(id,"zero")) {
        REQUIRE(take(0)==-1); capture(); slub_free(NULL); REQUIRE(unchanged());
        int s=take(100); capture(); slub_free(NULL); REQUIRE(unchanged()); release(s);
    } else if(!strcmp(id,"same_isolation")) {
        int slots[20]; for(int i=0;i<20;++i) { slots[i]=take(100); REQUIRE(slots[i]>=0); }
        release(slots[10]);
        for(int i=0;i<20;++i) if(i!=10) { objects[slots[i]].signature^=0x5a; fill(objects+slots[i]); }
        verify(); drain();
    } else if(!strcmp(id,"mixed_isolation")) {
        size_t sizes[]={24,100,256,500,1000,5000,8200}; int slots[7];
        for(int i=0;i<7;++i) { slots[i]=take(sizes[i]); REQUIRE(slots[i]>=0); }
        release(slots[3]); verify(); drain();
    } else if(!strcmp(id,"reuse")) {
        int slots[31]; for(int i=0;i<31;++i) { slots[i]=take(100); REQUIRE(slots[i]>=0); }
        size_t f=nr_free_pages(); for(int i=0;i<31;i+=2) release(slots[i]);
        REQUIRE(nr_free_pages()==f); for(int i=0;i<16;++i) REQUIRE(take(100)>=0);
        REQUIRE(nr_free_pages()==f); drain();
    } else if(!strcmp(id,"expand")) {
        int slots[32]; for(int i=0;i<32;++i) { slots[i]=take(100); REQUIRE(slots[i]>=0); REQUIRE(nr_free_pages()==initial_free-(i<31?1:2)); }
        uintptr_t h=(uintptr_t)objects[slots[0]].ptr&~(uintptr_t)4095;
        for(int i=0;i<31;++i) { REQUIRE(((uintptr_t)objects[slots[i]].ptr&~(uintptr_t)4095)==h); release(slots[i]); }
        REQUIRE(nr_free_pages()==1);
        struct Page *p=borrow(1); REQUIRE(p && page2pa(p)==h-PHYSICAL_MEMORY_OFFSET);
        free_pages(p,1); verify(); release(slots[31]); REQUIRE(take(100)>=0); drain();
    } else if(!strcmp(id,"classes")) {
        size_t sizes[]={16,32,64,128,256,512,1024,2048};
        for(size_t c=0;c<8;++c) { size_t capacity=(PGSIZE-TEST_HEADER)/sizes[c];
            for(size_t i=0;i<=capacity;++i) REQUIRE(take(sizes[c])>=0);
            REQUIRE(nr_free_pages()==initial_free-2); drain();
        }
    } else if(!strcmp(id,"reclaim")) {
        int a=take(24),b=take(24); REQUIRE(a>=0 && b>=0);
        size_t idx=(((uintptr_t)objects[a].ptr-PHYSICAL_MEMORY_OFFSET)>>12)-nbase;
        release(a); REQUIRE(owner[idx]!=NULL && nr_free_pages()==initial_free-1);
        release(b); REQUIRE(owner[idx]==NULL && nr_free_pages()==initial_free);
        REQUIRE(take(24)>=0); drain();
    } else if(!strcmp(id,"large_pair")) {
        int a=take(8200),b=take(24); REQUIRE(a>=0 && b>=0);
        size_t idx=(((uintptr_t)objects[a].ptr-PHYSICAL_MEMORY_OFFSET)>>12)-nbase;
        REQUIRE(pmm_original[idx]==3 && pmm_actual[idx]==4);
        size_t f=nr_free_pages(); release(a); REQUIRE(nr_free_pages()==f+4);
        for(size_t i=0;i<4;++i) { REQUIRE(owner[idx+i]==NULL); }
        release(b);
    } else if(!strcmp(id,"huge")) {
        REQUIRE(take(100)>=0);
        size_t sizes[]={(size_t)-1,(size_t)1<<63,SLUB_PAGE_LIMIT*PGSIZE-TEST_HEADER+1,SLUB_PAGE_LIMIT*PGSIZE-TEST_HEADER};
        for(size_t i=0;i<4;++i) { REQUIRE(take(sizes[i])==-1); }
        drain();
    } else if(!strcmp(id,"no_pages")) { REQUIRE(take(1)==-1 && take(5000)==-1); }
    else if(!strcmp(id,"oom_small") || !strcmp(id,"oom_16")) {
        size_t n=!strcmp(id,"oom_16")?16:1000,capacity=2*((PGSIZE-TEST_HEADER)/expected_width(n));
        for(size_t i=0;i<capacity;++i) REQUIRE(take(n)>=0);
        REQUIRE(nr_free_pages()==0 && take(n)==-1); drain(); REQUIRE(take(n)>=0); drain();
    } else if(!strcmp(id,"free_slots_oom")) {
        int a=take(1000),b=take(1000); REQUIRE(a>=0&&b>=0&&nr_free_pages()==0);
        REQUIRE(take(1000)>=0 && take(1000)==-1); release(a); REQUIRE(take(1000)>=0); drain();
    } else if(!strcmp(id,"oom_large")) {
        int a=take(8200); REQUIRE(a>=0 && nr_free_pages()==0 && take(5000)==-1);
        release(a); REQUIRE(take(5000)>=0); drain();
    } else if(!strcmp(id,"fragmented")) {
        struct Page *held[8]; for(int i=0;i<8;++i) { held[i]=borrow(1); REQUIRE(held[i]); }
        for(int i=0;i<8;++i) if((page2ppn(held[i])&1)==0) { free_pages(held[i],1); held[i]=NULL; }
        verify(); REQUIRE(take(5000)==-1); REQUIRE(take(2048)>=0);
        for(int i=0;i<8;++i) if(held[i]) free_pages(held[i],1);
        drain(); REQUIRE(take(8200)>=0); drain();
    } else if(!strcmp(id,"raw_coexist")) {
        struct Page *p=borrow(3); REQUIRE(p); void *data=(void *)(page2pa(p)+va_pa_offset); memset(data,0x56,3*PGSIZE);
        REQUIRE(take(100)>=0 && take(8200)>=0); verify();
        const unsigned char *bytes=data; for(size_t i=0;i<3*PGSIZE;++i) REQUIRE(bytes[i]==0x56);
        free_pages(p,3); drain();
    } else if(!strcmp(id,"basic")) { slub_check(); verify(); REQUIRE(nr_free_pages()==initial_free); }
    else if(!strcmp(id,"stress_a") || !strcmp(id,"stress_b")) {
        stress_mode=1; unsigned int seed=!strcmp(id,"stress_a")?0x20261021U:0x13579BDFU;
        const size_t sizes[]={1,8,15,16,17,24,32,33,60,64,65,100,128,129,256,257,500,512,513,1000,1024,1025,2048,2049,3000,4095,4096,4097,5000,8192,8193,8200};
        cprintf("SEED 0x%x steps=10000\n",seed);
        for(unsigned int i=0;i<10000;++i) {
            unsigned int slot=(random_next(&seed)>>16)%64;
            if(objects[slot].ptr) release(slot);
            else (void)take(sizes[(random_next(&seed)>>16)%(sizeof(sizes)/sizeof(sizes[0]))]);
        }
        stress_mode=0; drain(); REQUIRE(take(8200)>=0); drain();
    } else if(!strcmp(id,"N01")) {
        int a=take(100),b=take(100); REQUIRE(a>=0&&b>=0); void *old=objects[a].ptr; release(a);
        expect("slab->inuse > 0 && slub_slot_used(slab, slot)"); slub_free(old);
    } else if(!strcmp(id,"N02")) {
        int a=take(2048); REQUIRE(a>=0); void *old=objects[a].ptr; release(a); expect("slab != NULL"); slub_free(old);
    } else if(!strcmp(id,"N03")) {
        int a=take(100); expect("displacement % slab->object_bytes == 0"); slub_free(objects[a].ptr+1);
    } else if(!strcmp(id,"N04") || !strcmp(id,"N15")) {
        int a=take(8200); expect("va == payload && slab->inuse == 1 && slab->used[0] == 1"); slub_free(objects[a].ptr+(!strcmp(id,"N04")?1:4096));
    } else if(!strcmp(id,"N05")) {
        int a=take(100); expect("va >= payload"); slub_free((void *)((uintptr_t)objects[a].ptr&~(uintptr_t)4095));
    } else if(!strcmp(id,"N06")) {
        int local=0; expect("va >= KERNBASE && va < KERNTOP && va >= va_pa_offset"); slub_free(&local);
    } else if(!strcmp(id,"N07")) {
        struct Page *p=borrow(1); REQUIRE(p); expect("slab != NULL"); slub_free((void *)(page2pa(p)+va_pa_offset));
    } else if(!strcmp(id,"N08")) {
        int a=take(100); expect("displacement % slab->object_bytes == 0"); slub_free(objects[a].ptr+100);
    } else if(!strcmp(id,"N09")) {
        int a=take(100); expect("slab->inuse > 0 && slub_slot_used(slab, slot)"); slub_free(objects[a].ptr+128);
    } else if(!strcmp(id,"N10")) {
        expect("va >= KERNBASE && va < KERNTOP && va >= va_pa_offset"); slub_free((void *)(uintptr_t)-1);
    } else if(!strcmp(id,"N11")) {
        expect("ppn >= nbase && ppn < npage && ppn - nbase < SLUB_PAGE_LIMIT"); slub_free((void *)(KERNBASE+resource_pages*PGSIZE));
    } else if(!strcmp(id,"N12")) {
        int a=take(8200); REQUIRE(a>=0); void *old=objects[a].ptr; release(a);
        expect("slab != NULL"); slub_free(old);
    } else if(!strcmp(id,"N13")) {
        REQUIRE(take(100)>=0);
        expect("va >= KERNBASE && va < KERNTOP && va >= va_pa_offset"); slub_free(pages);
    } else if(!strcmp(id,"N14")) {
        int a=take(100); expect("displacement < (size_t)slab->slots * slab->object_bytes");
        slub_free((void *)(((uintptr_t)objects[a].ptr&~(uintptr_t)4095)+4095));
    } else if(!strcmp(id,"N16")) { expect("slub_ready"); (void)slub_alloc(24); }
    else if(!strcmp(id,"N17")) { REQUIRE(take(100)>=0); expect("!slub_ready"); slub_init(); }
    else { cprintf("Unknown case %s\n",id); exit(2); }
    REQUIRE(expected_assertion==NULL); drain();
}
int main(int argc,char **argv) {
    if(argc!=2) return 2;
    test_id=argv[1];
    void *mapped=mmap((void *)HOST_RAM_VA,PHYSICAL_MEMORY_END-DRAM_BASE,3,0x100022,-1,0);
    if(mapped!=(void *)HOST_RAM_VA) { cprintf("ENVIRONMENT FAILURE: fixed non-replacing mmap failed\n"); return 3; }
    REQUIRE(sizeof(struct slub_slab)==88 && SLUB_HEADER_BYTES==TEST_HEADER);
    run(test_id);
    cprintf("PASS %s operations=%u restored=%llu objects=0 slabs=0\n",test_id,sequence,nr_free_pages());
    return 0;
}
