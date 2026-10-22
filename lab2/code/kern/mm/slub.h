// Lab2 SLUB Challenge - Student ID: 2411560
#ifndef __KERN_MM_SLUB_H__
#define __KERN_MM_SLUB_H__

#include <defs.h>

/* Single-Hart object allocation. Payloads are at least 16-byte aligned.
 * Contents are unspecified. Free only exact, live returned pointers. */
void slub_init(void);
void *slub_alloc(size_t size);
void slub_free(void *ptr);
void slub_check(void);

#endif /* !__KERN_MM_SLUB_H__ */
