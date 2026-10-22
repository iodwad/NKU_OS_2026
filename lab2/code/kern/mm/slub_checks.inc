/* Basic integration checks only; the dedicated test prompt will extend these.
 * Temporary objects use the public SLUB API and are all returned. */
static void slub_check_bytes(const void *ptr, size_t n, unsigned char value) {
    const volatile unsigned char *bytes = ptr;
    for (size_t i = 0; i < n; ++i) assert(bytes[i] == value);
}

static void slub_check_distinct(void **objects, const size_t *sizes, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        assert(objects[i] != NULL && ((uintptr_t)objects[i] & 15) == 0);
        for (size_t j = 0; j < i; ++j) {
            uintptr_t a = (uintptr_t)objects[i], b = (uintptr_t)objects[j];
            assert(a + sizes[i] <= b || b + sizes[j] <= a);
        }
    }
}

static void slub_check_basic(void) {
    assert(slub_ready);
    size_t before = nr_free_pages();
    size_t objects_before = live_objects, slabs_before = slab_count;
    assert(slub_alloc(0) == NULL);
    slub_free(NULL);
    assert(nr_free_pages() == before);

    const size_t sizes[] = {1,24,60,100,256,500,1000};
    void *objects[7];
    for (size_t i = 0; i < 7; ++i) {
        objects[i] = slub_alloc(sizes[i]);
        assert(objects[i] != NULL);
        memset(objects[i], (char)(0x20 + i), sizes[i]);
    }
    slub_check_distinct(objects, sizes, 7);
    void *a = slub_alloc(24), *b = slub_alloc(24);
    assert(a != NULL && b != NULL && a != b && a != objects[1] && b != objects[1]);
    assert(((uintptr_t)a & 15) == 0 && ((uintptr_t)b & 15) == 0);
    memset(a, 0x55, 24); memset(b, 0x66, 24);
    size_t partial_free = nr_free_pages();
    slub_free(a);
    assert(nr_free_pages() == partial_free);
    slub_check_bytes(b, 24, 0x66);
    void *reused = slub_alloc(24);
    assert(reused != NULL && nr_free_pages() == partial_free);
    assert(reused != b && reused != objects[1] && ((uintptr_t)reused & 15) == 0);
    memset(reused, 0x77, 24);
    for (size_t i = 0; i < 7; ++i) slub_check_bytes(objects[i], sizes[i], 0x20 + i);
    slub_check_bytes(b, 24, 0x66);
    slub_check_bytes(reused, 24, 0x77);
    cprintf("slub_check: small-object writes, isolation and slot reuse passed\n");

    size_t large_before = nr_free_pages();
    void *large = slub_alloc(5000), *rounded = slub_alloc(8200);
    assert(large != NULL && rounded != NULL);
    void *large_objects[] = {large, rounded};
    const size_t large_sizes[] = {5000,8200};
    slub_check_distinct(large_objects, large_sizes, 2);
    /* A 96-byte header means requests of 2 and 3 pages respectively. The
     * latter owns 4 Buddy pages, but must be freed with original request 3. */
    assert(SLUB_HEADER_BYTES == 96);
    assert(nr_free_pages() == large_before - 2 - 4);
    memset(large, 0x38, 5000); memset(rounded, 0x49, 8200);
    slub_check_bytes(large, 5000, 0x38);
    slub_check_bytes(rounded, 8200, 0x49);
    for (size_t i = 0; i < 7; ++i) slub_check_bytes(objects[i], sizes[i], 0x20 + i);
    slub_check_bytes(b, 24, 0x66); slub_check_bytes(reused, 24, 0x77);
    slub_free(large);
    assert(nr_free_pages() == large_before - 4);
    slub_check_bytes(rounded, 8200, 0x49);
    slub_free(rounded);
    assert(nr_free_pages() == large_before);
    cprintf("slub_check: 5000-byte and 8200-byte writes and paired page return passed\n");

    slub_free(reused); slub_free(b);
    for (size_t i = 7; i > 0; --i) slub_free(objects[i-1]);
    assert(live_objects == objects_before && slab_count == slabs_before);
    assert(nr_free_pages() == before);
    cprintf("slub_check: all basic checks passed; restored %u free pages\n", (unsigned int)before);
}

#include "slub_test_cases.inc"
