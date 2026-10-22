/* Normal kernel checks. No reset, forced OOM or deliberately illegal free.
 * Called immediately after slub_init, before other object clients start. */
struct slub_test_object { void *ptr; size_t bytes; unsigned char pattern; };
static struct slub_test_object slub_class_test_objects[251];

static size_t slub_test_width(size_t n) {
    const size_t widths[]={16,32,64,128,256,512,1024,2048};
    for(size_t i=0;i<8;++i) if(n<=widths[i]) return widths[i];
    return n;
}
static size_t slub_test_pages(size_t n) {
    if(n<=2048) return 1;
    return (n+96)/PGSIZE+((n+96)%PGSIZE!=0);
}
static size_t slub_test_rounded(size_t n) { size_t q=1; while(q<n) q*=2; return q; }

static void slub_test_state(void) {
    size_t objects_seen=0,slabs_seen=0,count=npage-nbase;
    for(size_t i=0;i<count;++i) {
        struct slub_slab *h=owner[i];
        if(h==NULL) continue;
        uintptr_t address=(uintptr_t)h;
        assert(address>=KERNBASE && address<KERNTOP && address%PGSIZE==0);
        assert(h->magic==SLUB_MAGIC && h->page_request>0 && h->page_request<=SLUB_PAGE_LIMIT);
        size_t base=h->page-pages,q=slub_test_rounded(h->page_request);
        assert(base<count && q<=count-base && i>=base && i-base<q);
        assert(page2pa(h->page)+va_pa_offset==address);
        assert(owner[base]==h && !PageReserved(pages+i) && !PageProperty(pages+i));
        if(i!=base) continue;
        ++slabs_seen;
        assert(h->class_id<=8 && h->inuse>0 && h->inuse<=h->slots);
        size_t used=0;
        assert(h->slots<=256);
        for(size_t bit=0;bit<256;++bit) {
            bool set=(h->used[bit/64]>>(bit%64))&1;
            if(bit>=h->slots) assert(!set);
            else used+=set;
        }
        assert(used==h->inuse); objects_seen+=used;
        if(h->class_id<8) {
            assert(h->page_request==1 && h->object_bytes==((size_t)16<<h->class_id));
            assert(h->slots==(PGSIZE-96)/h->object_bytes);
            uint64_t seen[4]={0}; size_t at=h->free_head,free_slots=0;
            while(at!=SLUB_NO_SLOT) {
                assert(at<h->slots && !(seen[at/64]&(1ULL<<(at%64))));
                assert(!(h->used[at/64]&(1ULL<<(at%64))));
                seen[at/64]|=1ULL<<(at%64); ++free_slots;
                at=*(uint16_t *)(address+96+at*h->object_bytes);
            }
            assert(free_slots+used==h->slots);
            list_entry_t *head=&partial[h->class_id],*node=head->next; size_t occurrences=0,steps=0;
            while(node!=head) {
                assert(++steps<=slab_count);
                if(node==&h->link) ++occurrences;
                node=node->next;
            }
            assert(occurrences==(used<h->slots?1:0));
        } else assert(h->slots==1 && h->inuse==1 && h->object_bytes<=h->page_request*PGSIZE-96);
    }
    for(size_t c=0;c<8;++c) {
        list_entry_t *head=&partial[c],*node=head->next,*previous=head; size_t steps=0;
        while(node!=head) {
            assert(++steps<=slab_count);
            struct slub_slab *h=to_struct(node,struct slub_slab,link);
            assert((uintptr_t)h>=KERNBASE && (uintptr_t)h<KERNTOP);
            assert(node->prev==previous && h->class_id==c && h->inuse>0 && h->inuse<h->slots);
            assert(owner[h->page-pages]==h);
            previous=node; node=node->next;
        }
        assert(head->prev==previous);
    }
    assert(objects_seen==live_objects && slabs_seen==slab_count);
}

static void slub_test_data(struct slub_test_object *r,bool full) {
    const volatile unsigned char *p=r->ptr;
    if(full || r->bytes<96) {
        for(size_t j=0;j<r->bytes;++j) assert(p[j]==r->pattern);
    } else for(size_t j=0;j<32;++j) {
        assert(p[j]==r->pattern && p[r->bytes-1-j]==r->pattern && p[r->bytes/2+j]==r->pattern);
    }
}

/* Independent input ledger supplies object lengths, expected slot widths and
 * actual page consumption. Internal state is inspected only for comparison. */
static void slub_test_ledger(struct slub_test_object *r,size_t count,size_t baseline,bool full) {
    size_t live=0,pages_used=0;
    for(size_t i=0;i<count;++i) {
        if(r[i].ptr==NULL) continue;
        uintptr_t p=(uintptr_t)r[i].ptr,h=p&~(uintptr_t)4095;
        assert(p>=KERNBASE && p<KERNTOP && r[i].bytes<=KERNTOP-p && p%16==0);
        struct slub_slab *slab=(struct slub_slab *)h;
        size_t width=slub_test_width(r[i].bytes),np=slub_test_pages(r[i].bytes);
        assert(slab->object_bytes==width && slab->page_request==np);
        size_t slot=0;
        if(r[i].bytes<=2048) {
            assert(p>=h+96 && (p-h-96)%width==0);
            slot=(p-h-96)/width; assert(slot<(PGSIZE-96)/width);
        } else assert(p==h+96);
        assert((slab->used[slot/64]>>(slot%64))&1);
        bool first=1;
        for(size_t j=0;j<i;++j) {
            if(!r[j].ptr) continue;
            uintptr_t other=(uintptr_t)r[j].ptr;
            assert(p+r[i].bytes<=other || other+r[j].bytes<=p);
            if((other&~(uintptr_t)4095)==h) first=0;
        }
        if(first) pages_used+=slub_test_rounded(np);
        slub_test_data(r+i,full); ++live;
    }
    assert(live==live_objects && pages_used<=baseline && nr_free_pages()==baseline-pages_used);
    slub_test_state();
}

static void slub_test_allocate(struct slub_test_object *r,size_t n,unsigned char pattern) {
    r->ptr=slub_alloc(n); assert(r->ptr!=NULL); r->bytes=n; r->pattern=pattern;
    memset(r->ptr,(char)pattern,n);
}
static void slub_test_release(struct slub_test_object *r) {
    assert(r->ptr!=NULL); slub_test_data(r,1); slub_free(r->ptr); r->ptr=NULL;
}

static void slub_test_sizes(void) {
    const size_t sizes[]={1,8,15,16,17,24,31,32,33,60,63,64,65,100,127,128,129,255,256,257,
        511,512,513,1023,1024,1025,2047,2048,2049,3000,4095,4096,4097,5000,8192,8193,8200};
    struct slub_test_object r[37]={{0}}; size_t baseline=nr_free_pages();
    for(size_t i=0;i<37;++i) { slub_test_allocate(r+i,sizes[i],0x20+i); slub_test_ledger(r,37,baseline,1); }
    for(size_t i=37;i>0;--i) { slub_test_release(r+i-1); slub_test_ledger(r,37,baseline,1); }
}
static void slub_test_reuse(void) {
    struct slub_test_object r[31]={{0}}; size_t baseline=nr_free_pages();
    for(size_t i=0;i<31;++i) slub_test_allocate(r+i,100,0x40+i);
    slub_test_ledger(r,31,baseline,1); assert(nr_free_pages()==baseline-1);
    for(size_t i=0;i<31;i+=2) { slub_test_release(r+i); slub_test_ledger(r,31,baseline,1); }
    assert(nr_free_pages()==baseline-1);
    for(size_t i=0;i<31;i+=2) { slub_test_allocate(r+i,100,0x80+i); slub_test_ledger(r,31,baseline,1); }
    assert(nr_free_pages()==baseline-1);
    for(size_t i=0;i<31;++i) { slub_test_release(r+i); slub_test_ledger(r,31,baseline,1); }
}
static void slub_test_classes(void) {
    for(size_t width=16;width<=2048;width*=2) {
        size_t capacity=(PGSIZE-96)/width,baseline=nr_free_pages();
        struct slub_test_object *r=slub_class_test_objects;
        for(size_t i=0;i<=capacity;++i) {
            slub_test_allocate(r+i,width,0x20+i%200);
            slub_test_ledger(r,capacity+1,baseline,1);
            assert(nr_free_pages()==baseline-(i<capacity?1:2));
        }
        uintptr_t first=(uintptr_t)r[0].ptr&~(uintptr_t)4095;
        assert(((uintptr_t)r[capacity].ptr&~(uintptr_t)4095)!=first);
        size_t first_index=((first-va_pa_offset)>>PGSHIFT)-nbase;
        for(size_t i=0;i<capacity;++i) {
            assert(((uintptr_t)r[i].ptr&~(uintptr_t)4095)==first);
            slub_test_release(r+i); slub_test_ledger(r,capacity+1,baseline,1);
        }
        assert(owner[first_index]==NULL && nr_free_pages()==baseline-1);
        slub_test_release(r+capacity); slub_test_ledger(r,capacity+1,baseline,1);
        assert(nr_free_pages()==baseline);
    }
}
static void slub_test_large(void) {
    struct slub_test_object r[3]={{0}}; size_t baseline=nr_free_pages();
    slub_test_allocate(r,8200,0x72); slub_test_allocate(r+1,5000,0x83); slub_test_allocate(r+2,24,0x94);
    slub_test_ledger(r,3,baseline,1);
    struct slub_slab *h=(void *)((uintptr_t)r[0].ptr&~(uintptr_t)4095);
    assert(h->page_request==3 && nr_free_pages()==baseline-7);
    slub_test_release(r); slub_test_ledger(r,3,baseline,1); assert(nr_free_pages()==baseline-3);
    slub_test_release(r+2); slub_test_release(r+1); slub_test_ledger(r,3,baseline,1);
}
static void slub_test_stress(unsigned int steps) {
    struct slub_test_object r[64]={{0}}; size_t baseline=nr_free_pages(); unsigned int seed=0x20261021U;
    const size_t sizes[]={1,15,16,17,24,60,100,256,500,1000,1025,2048,2049,4096,5000,8200};
    for(unsigned int i=0;i<steps;++i) {
        seed=seed*1664525U+1013904223U; size_t slot=(seed>>16)%64;
        if(r[slot].ptr) slub_test_release(r+slot);
        else { seed=seed*1664525U+1013904223U; size_t n=sizes[(seed>>16)%16];
            void *p=slub_alloc(n);
            if(p) { r[slot]=(struct slub_test_object){p,n,0x60+slot}; memset(p,(char)r[slot].pattern,n); }
        }
        slub_test_ledger(r,64,baseline,i%64==0);
    }
    for(size_t i=0;i<64;++i) if(r[i].ptr) { slub_test_release(r+i); slub_test_ledger(r,64,baseline,1); }
    assert(nr_free_pages()==baseline);
}
static void slub_test_buddy_coexist(void) {
    size_t baseline=nr_free_pages(); struct Page *p=alloc_pages(3); assert(p!=NULL);
    assert(nr_free_pages()==baseline-4);
    uintptr_t data=page2pa(p)+va_pa_offset; memset((void *)data,0x5c,3*PGSIZE);
    size_t index=p-pages; for(size_t i=0;i<4;++i) assert(owner[index+i]==NULL);
    slub_test_stress(128);
    slub_check_bytes((void *)data,3*PGSIZE,0x5c);
    for(size_t i=0;i<4;++i) assert(owner[index+i]==NULL);
    free_pages(p,3); assert(nr_free_pages()==baseline); slub_test_state();
}

void slub_check(void) {
    size_t baseline=nr_free_pages();
    assert(slub_ready && live_objects==0 && slab_count==0);
    slub_check_basic(); slub_test_state();
    cprintf("slub_tests T01: PASS original basic checks\n");
    slub_test_sizes(); cprintf("slub_tests T02: PASS 37 sizes and concurrent data isolation\n");
    slub_test_reuse(); cprintf("slub_tests T03: PASS partial-slot reuse without new pages\n");
    slub_test_classes(); cprintf("slub_tests T04: PASS eight classes expand and reclaim independently\n");
    slub_test_large(); cprintf("slub_tests T05: PASS large original 3-page request owns and returns 4\n");
    slub_test_stress(1024); cprintf("slub_tests T06: PASS seed=0x20261021 steps=1024\n");
    slub_test_buddy_coexist(); cprintf("slub_tests T07: PASS raw Buddy allocation survives 128 mixed steps\n");
    assert(nr_free_pages()==baseline && live_objects==0 && slab_count==0);
    cprintf("slub_tests: all 7 checks passed; restored %u free pages\n",(unsigned int)baseline);
}
