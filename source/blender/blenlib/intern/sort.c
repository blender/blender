/* SPDX-FileCopyrightText: 2013 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include <stdlib.h>

#if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 8))
/* do nothing! */
#else

#  include "BLI_utildefines.h"

#  include "BLI_sort.h"

#  ifdef min /* For MSVC. */
#    undef min
#  endif

/* Maintained by FreeBSD. */
/* clang-format off */

/**
 * qsort, copied from FreeBSD source.
 * with only very minor edits, see:
 * http://github.com/freebsd/freebsd/blob/master/sys/libkern/qsort.c
 *
 * \note modified to use glibc arg order for callbacks.
 */
BLI_INLINE char *med3(char *a, char *b, char *c, BLI_sort_cmp_t cmp, void *thunk);
BLI_INLINE void  swapfunc(char *a, char *b, int n, int swaptype);

#define min(a, b)   (a) < (b) ? (a) : (b)
#define swapcode(TYPE, parmi, parmj, n)     \
{                                           \
  long i = (n) / sizeof(TYPE);            \
  TYPE *pi = (TYPE *) (parmi);            \
  TYPE *pj = (TYPE *) (parmj);            \
  do {                                    \
    TYPE    t = *pi;                    \
    *pi++ = *pj;                        \
    *pj++ = t;                          \
  } while (--i > 0);                      \
}
#define SWAPINIT(a, es) swaptype = ((char *)a - (char *)0) % sizeof(long) || \
  es % sizeof(long) ? 2 : es == sizeof(long)? 0 : 1;

BLI_INLINE void swapfunc(char *a, char *b, int n, int swaptype)
{
  if (swaptype <= 1)
    swapcode(long, a, b, n)
  else
    swapcode(char, a, b, n)
}

#define swap(a, b)                  \
  if (swaptype == 0) {            \
    long t = *(long *)(a);      \
    *(long *)(a) = *(long *)(b);\
    *(long *)(b) = t;           \
  } else                          \
    swapfunc(a, b, es, swaptype)

#define vecswap(a, b, n)    if ((n) > 0) swapfunc(a, b, n, swaptype)
#define CMP(t, x, y) (cmp((x), (y), (t)))

BLI_INLINE char *med3(char *a, char *b, char *c, BLI_sort_cmp_t cmp, void *thunk)
{
  return  CMP(thunk, a, b) < 0 ?
         (CMP(thunk, b, c) < 0 ? b : (CMP(thunk, a, c) < 0 ? c : a )) :
         (CMP(thunk, b, c) > 0 ? b : (CMP(thunk, a, c) < 0 ? a : c ));
}

/**
 * Quick sort re-entrant.
 */
void BLI_qsort_r(void *a, size_t n, size_t es, BLI_sort_cmp_t cmp, void *thunk)
{
  char *pa, *pb, *pc, *pd, *pl, *pm, *pn;
  int d, r, swaptype, swap_cnt;

loop:
  SWAPINIT(a, es);
  swap_cnt = 0;
  if (n < 7) {
    for (pm = (char *)a + es; pm < (char *)a + n * es; pm += es) {
      for (pl = pm;
           pl > (char *)a && CMP(thunk, pl - es, pl) > 0;
           pl -= es)
      {
        swap(pl, pl - es);
      }
    }
    return;
  }
  pm = (char *)a + (n / 2) * es;
  if (n > 7) {
    pl = (char *)a;
    pn = (char *)a + (n - 1) * es;
    if (n > 40) {
      d = (n / 8) * es;
      pl = med3(pl, pl + d, pl + 2 * d, cmp, thunk);
      pm = med3(pm - d, pm, pm + d, cmp, thunk);
      pn = med3(pn - 2 * d, pn - d, pn, cmp, thunk);
    }
    pm = med3(pl, pm, pn, cmp, thunk);
  }
  swap((char *)a, pm);
  pa = pb = (char *)a + es;

  pc = pd = (char *)a + (n - 1) * es;
  for (;;) {
    while (pb <= pc && (r = CMP(thunk, pb, a)) <= 0) {
      if (r == 0) {
        swap_cnt = 1;
        swap(pa, pb);
        pa += es;
      }
      pb += es;
    }
    while (pb <= pc && (r = CMP(thunk, pc, a)) >= 0) {
      if (r == 0) {
        swap_cnt = 1;
        swap(pc, pd);
        pd -= es;
      }
      pc -= es;
    }
    if (pb > pc) {
      break;
    }
    swap(pb, pc);
    swap_cnt = 1;
    pb += es;
    pc -= es;
  }
  if (swap_cnt == 0) {  /* Switch to insertion sort */
    for (pm = (char *)a + es; pm < (char *)a + n * es; pm += es) {
      for (pl = pm;
           pl > (char *)a && CMP(thunk, pl - es, pl) > 0;
           pl -= es)
      {
        swap(pl, pl - es);
      }
    }
    return;
  }

  pn = (char *)a + n * es;
  r = min(pa - (char *)a, pb - pa);
  vecswap((char *)a, pb - r, r);
  r = min(pd - pc, pn - pd - es);
  vecswap(pb, pn - r, r);
  if ((r = pb - pa) > es) {
    BLI_qsort_r(a, r / es, es, cmp, thunk);
  }
  if ((r = pd - pc) > es) {
    /* Iterate rather than recurse to save stack space */
    a = pn - r;
    n = r / es;
    goto loop;
  }
}

/* clang-format on */

#endif /* __GLIBC__ */
