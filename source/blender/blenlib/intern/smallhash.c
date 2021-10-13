/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

#ifdef __GNUC__
/* I can't even *cast* signed ints in gcc's sign-conversion warning? gcc 10.3.0 -joeedh */
#  pragma GCC diagnostic ignored "-Wsign-conversion"
#endif

#ifdef __GNUC__
/* I can't even *cast* signed ints in gcc's sign-conversion warning? gcc 10.3.0 -joeedh */
#  pragma GCC diagnostic ignored "-Wsign-conversion"
#endif

/** \file
 * \ingroup bli
 *
 * A light stack-friendly hash library, it uses stack space for relatively small,
 * fixed size hash tables but falls back to heap memory once the stack limits reached
 * (#SMSTACKSIZE).
 *
 * based on a doubling hashing approach (non-chaining) which uses more buckets than entries
 * stepping over buckets when two keys share the same hash so any key can find a free bucket.
 *
 * See: https://en.wikipedia.org/wiki/Double_hashing
 *
 * \warning This should _only_ be used for small hashes
 * where allocating a hash every time is unacceptable.
 * Otherwise #GHash should be used instead.
 *
 * #SmallHashEntry.key
 * - `SMHASH_KEY_UNUSED` means the key in the cell has not been initialized.
 *
 * #SmallHashEntry.val
 * - `SMHASH_CELL_UNUSED` means this cell is inside a key series.
 * - `SMHASH_CELL_FREE` means this cell terminates a key series.
 *
 * Note that the values and keys are often pointers or index values,
 * use the maximum values to avoid real pointers colliding with magic numbers.
 */

#include <stdlib.h>
#include <string.h>

#include "BLI_sys_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLI_smallhash.h"

#include "BLI_asan.h"
#include "BLI_strict_flags.h"

/* NOTE: copied from BLO_blend_defs.h, don't use here because we're in BLI. */
#ifdef __BIG_ENDIAN__
/* Big Endian */
#  define MAKE_ID(a, b, c, d) ((int)(a) << 24 | (int)(b) << 16 | (c) << 8 | (d))
#  define MAKE_ID_8(a, b, c, d, e, f, g, h) \
    ((int64_t)(a) << 56 | (int64_t)(b) << 48 | (int64_t)(c) << 40 | (int64_t)(d) << 32 | \
     (int64_t)(e) << 24 | (int64_t)(f) << 16 | (int64_t)(g) << 8 | (h))
#else
/* Little Endian */
#  define MAKE_ID(a, b, c, d) ((int)(d) << 24 | (int)(c) << 16 | (b) << 8 | (a))
#  define MAKE_ID_8(a, b, c, d, e, f, g, h) \
    ((int64_t)(h) << 56 | (int64_t)(g) << 48 | (int64_t)(f) << 40 | (int64_t)(e) << 32 | \
     (int64_t)(d) << 24 | (int64_t)(c) << 16 | (int64_t)(b) << 8 | (a))
#endif

#define SMHASH_KEY_UNUSED (uintptr_t)(MAKE_ID_8('s', 'm', 'h', 'k', 'u', 'n', 'u', 's'))
#define SMHASH_CELL_FREE (uintptr_t)(MAKE_ID_8('s', 'm', 'h', 'c', 'f', 'r', 'e', 'e'))
#define SMHASH_CELL_UNUSED (uintptr_t)(MAKE_ID_8('s', 'm', 'h', 'c', 'u', 'n', 'u', 's'))

#define USE_REMOVE

/* typically this re-assigns 'h' */
#define SMHASH_NEXT(h, hoff) \
  (CHECK_TYPE_INLINE(&(h), uintptr_t *), \
   CHECK_TYPE_INLINE(&(hoff), uintptr_t *), \
   ((h) + (((hoff) = ((hoff)*2) + 1), (hoff))))

BLI_INLINE bool check_stack_move(SmallHash *sh)
{
  if (sh->using_stack && sh->buckets != sh->buckets_stack) {
    sh->buckets = sh->buckets_stack;

    return true;
  }

  return false;
}

BLI_INLINE bool smallhash_val_is_used(const void *val)
{
#ifdef USE_REMOVE
  return !ELEM(val, SMHASH_CELL_FREE, SMHASH_CELL_UNUSED);
#else
  return (val != SMHASH_CELL_FREE);
#endif
}

extern const uint BLI_ghash_hash_sizes[];
#define hashsizes BLI_ghash_hash_sizes

int BLI_smallhash_memuse(SmallHash *sh)
{
  return (int)sh->nbuckets * (int)sizeof(SmallHashEntry) + (int)sizeof(SmallHash);
}

#if 0
BLI_INLINE uintptr_t smallhash_key(const uintptr_t key)
{
#  if 1
  return key;
#  else
  uintptr_t y = (size_t)key;
  /* bottom 3 or 4 bits are likely to be 0; rotate y by 4 to avoid
   * excessive hash collisions for dicts and sets */

  return (uintptr_t)(y >> 4) | ((uintptr_t)y << (sizeof(uintptr_t[8]) - 4));
#  endif
}
#endif

#ifdef _keyrot
#  undef _keyrot
#endif

#define _keyrot(y) ((uintptr_t)(y) >> 4) | ((uintptr_t)(y) << (sizeof(uintptr_t[8]) - 4))
#define smallhash_key(key) sh->use_pointer_hash ? _keyrot(key) : (key)
/**
 * Check if the number of items in the smallhash is large enough to require more buckets.
 */
BLI_INLINE bool smallhash_test_expand_buckets(const uint nentries,
                                              const uint nbuckets,
                                              const uint nfreecells)
{
  if (nfreecells < 3) {
    return true;
  }

  /* (approx * 1.5) */
  return (nentries + (nentries >> 1)) > nbuckets || nfreecells < 3;
}

BLI_INLINE void smallhash_init_empty(SmallHash *sh)
{
  uint i;

  for (i = 0; i < sh->nbuckets; i++) {
    sh->buckets[i].key = SMHASH_KEY_UNUSED;
    sh->buckets[i].val = SMHASH_CELL_FREE;
  }
}

/**
 * Increase initial bucket size to match a reserved amount.
 */
BLI_INLINE void smallhash_buckets_reserve(SmallHash *sh, const uint nentries_reserve)
{
  while (smallhash_test_expand_buckets(nentries_reserve, sh->nbuckets, sh->nbuckets + 5)) {
    sh->nbuckets = hashsizes[++sh->cursize];
    sh->nfreecells = sh->nbuckets;
  }
}

BLI_INLINE SmallHashEntry *smallhash_lookup(SmallHash *sh, const uintptr_t key)
{
  check_stack_move(sh);

  SmallHashEntry *e;
  uintptr_t h = smallhash_key(key);
  uintptr_t hoff = 1;

  BLI_assert(key != SMHASH_KEY_UNUSED);

  /* NOTE: there are always more buckets than entries,
   * so we know there will always be a free bucket if the key isn't found. */
  for (e = &sh->buckets[h % sh->nbuckets]; e->val != SMHASH_CELL_FREE;
       h = SMHASH_NEXT(h, hoff), e = &sh->buckets[h % sh->nbuckets]) {
    if (e->key == key) {
      /* should never happen because unused keys are zero'd */
      BLI_assert(e->val != SMHASH_CELL_UNUSED);
      return e;
    }
  }

  return NULL;
}

BLI_INLINE SmallHashEntry *smallhash_lookup_first_free(SmallHash *sh, const uintptr_t key)
{
  check_stack_move(sh);

  SmallHashEntry *e;
  uintptr_t h = smallhash_key(key);
  uintptr_t hoff = 1;

  for (e = &sh->buckets[h % sh->nbuckets]; smallhash_val_is_used(e->val);
       h = SMHASH_NEXT(h, hoff), e = &sh->buckets[h % sh->nbuckets]) {
    /* pass */
  }

  return e;
}

BLI_INLINE void smallhash_resize_buckets(SmallHash *sh, const uint nbuckets)
{
  check_stack_move(sh);

  SmallHashEntry *buckets_old = sh->buckets;
  const uint nbuckets_old = sh->nbuckets;
  const bool was_alloc = (buckets_old != sh->buckets_stack);
  uint i = 0;

  BLI_assert(sh->nbuckets != nbuckets);
  if (nbuckets <= SMSTACKSIZE) {
    const size_t size = sizeof(*buckets_old) * nbuckets_old;
    buckets_old = alloca(size);
    memcpy(buckets_old, sh->buckets, size);

    sh->buckets = sh->buckets_stack;
  }
  else {
    sh->buckets = MEM_mallocN(sizeof(*sh->buckets) * nbuckets, __func__);
    sh->using_stack = false;
  }

  sh->nbuckets = nbuckets;
  sh->nfreecells = nbuckets;
  sh->nentries = 0;

  smallhash_init_empty(sh);

  for (i = 0; i < nbuckets_old; i++) {
    if (smallhash_val_is_used(buckets_old[i].val)) {
      SmallHashEntry *e = smallhash_lookup_first_free(sh, buckets_old[i].key);

      e->key = buckets_old[i].key;
      e->val = buckets_old[i].val;

      sh->nfreecells--;
      sh->nentries++;
    }
  }

  if (was_alloc && buckets_old) {
    MEM_freeN(buckets_old);
  }
}

void BLI_smallhash_init_ex(SmallHash *sh, const uint nentries_reserve)
{
  /* assume 'sh' is uninitialized */

  sh->nentries = 0;
  sh->cursize = 2;
  sh->using_stack = true;
  sh->nbuckets = hashsizes[sh->cursize];
  sh->nfreecells = sh->nbuckets;

  sh->buckets = sh->buckets_stack;

  if (nentries_reserve) {
    smallhash_buckets_reserve(sh, nentries_reserve);

    if (sh->nbuckets > SMSTACKSIZE) {
      sh->buckets = MEM_mallocN(sizeof(*sh->buckets) * sh->nbuckets, __func__);

      sh->using_stack = false;
    }
  }

  smallhash_init_empty(sh);
}

void BLI_smallhash_init(SmallHash *sh)
{
  BLI_smallhash_init_ex(sh, 0);
}

/* NOTE: does *not* free *sh itself!  only the direct data! */
void BLI_smallhash_release(SmallHash *sh)
{
  check_stack_move(sh);

  if (sh->buckets && sh->buckets != sh->buckets_stack) {
    MEM_freeN(sh->buckets);
  }
}

bool BLI_smallhash_ensure_p(SmallHash *sh, uintptr_t key, void ***item)
{
  check_stack_move(sh);

  SmallHashEntry *e = NULL;
  uintptr_t h = smallhash_key(key);
  uintptr_t hoff = 1;

  if (UNLIKELY(smallhash_test_expand_buckets(sh->nentries, sh->nbuckets, sh->nfreecells))) {
    smallhash_resize_buckets(sh, hashsizes[++sh->cursize]);
  }

  BLI_assert(key != SMHASH_KEY_UNUSED);

  /* NOTE: there are always more buckets than entries,
   * so we know there will always be a free bucket if the key isn't found. */
  for (e = &sh->buckets[h % sh->nbuckets]; e->val != SMHASH_CELL_FREE;
       h = SMHASH_NEXT(h, hoff), e = &sh->buckets[h % sh->nbuckets]) {
    if (e->key == key) {
      /* should never happen because unused keys are zero'd */
      BLI_assert(e->val != SMHASH_CELL_UNUSED);
      break;
    }
  }

  bool ret;

  if (e->val == SMHASH_CELL_FREE || e->val == SMHASH_CELL_UNUSED) {
    sh->nentries++;
    sh->nfreecells--;

    ret = false;
    e->val = NULL;
  }
  else {
    ret = true;
  }

  e->key = key;

  *item = &e->val;

  return ret;
}

void BLI_smallhash_insert(SmallHash *sh, uintptr_t key, void *item)
{
  check_stack_move(sh);

  SmallHashEntry *e;

  BLI_assert(key != SMHASH_KEY_UNUSED);
  BLI_assert(smallhash_val_is_used(item));
  BLI_assert(BLI_smallhash_haskey(sh, key) == false);

  if (UNLIKELY(smallhash_test_expand_buckets(sh->nentries, sh->nbuckets, sh->nfreecells))) {
    smallhash_resize_buckets(sh, hashsizes[++sh->cursize]);
  }

  e = smallhash_lookup_first_free(sh, key);

  if (e->val == SMHASH_CELL_FREE) {
    sh->nentries++;
    sh->nfreecells--;
  }
  else if (e->val == SMHASH_CELL_UNUSED) {
    sh->nentries++;
  }

  e->key = key;
  e->val = item;
}

/**
 * Inserts a new value to a key that may already be in ghash.
 *
 * Avoids #BLI_smallhash_remove, #BLI_smallhash_insert calls (double lookups)
 *
 * \returns true if a new key has been added.
 */
bool BLI_smallhash_reinsert(SmallHash *sh, uintptr_t key, void *item)
{
  SmallHashEntry *e = smallhash_lookup(sh, key);
  if (e) {
    e->val = item;
    return false;
  }

  BLI_smallhash_insert(sh, key, item);
  return true;
}

#ifdef USE_REMOVE
bool BLI_smallhash_remove(SmallHash *sh, uintptr_t key)
{
  check_stack_move(sh);

  // SmallHashEntry *e = smallhash_lookup(sh, key);

  SmallHashEntry *e;
  uintptr_t h = smallhash_key(key);
  uintptr_t hoff = 1;

  for (e = &sh->buckets[h % sh->nbuckets]; e->val != SMHASH_CELL_FREE;
       h = SMHASH_NEXT(h, hoff), e = &sh->buckets[h % sh->nbuckets]) {
    if (e->key == key) {
      /* should never happen because unused keys are zero'd */
      BLI_assert(e->val != SMHASH_CELL_UNUSED);
      break;
    }
  }

  if (e && e->key == key) {
    h = SMHASH_NEXT(h, hoff);
    SmallHashEntry *e2 = &sh->buckets[h & sh->nbuckets];

    e->key = SMHASH_KEY_UNUSED;
    e->val = SMHASH_CELL_UNUSED;

    sh->nentries--;

    return true;
  }
  else {
    return false;
  }
}

bool BLI_smallhash_remove_p(SmallHash *sh, uintptr_t key, void **val)
{
  SmallHashEntry *e = smallhash_lookup(sh, key);

  if (e) {
    *val = e->val;

    e->key = SMHASH_KEY_UNUSED;
    e->val = SMHASH_CELL_UNUSED;
    sh->nentries--;

    return true;
  }
  else {
    return false;
  }
}
#endif

void *BLI_smallhash_lookup(SmallHash *sh, uintptr_t key)
{
  SmallHashEntry *e = smallhash_lookup(sh, key);

  return e ? e->val : NULL;
}

void **BLI_smallhash_lookup_p(SmallHash *sh, uintptr_t key)
{
  SmallHashEntry *e = smallhash_lookup(sh, key);

  return e ? &e->val : NULL;
}

void BLI_smallhash_clear(SmallHash *sh, uintptr_t key)
{
  check_stack_move(sh);

  SmallHashEntry *e = sh->buckets;

  for (uint i = 0; i < sh->nbuckets; i++, e++) {
    e->key = SMHASH_KEY_UNUSED;
    e->val = SMHASH_CELL_FREE;
  }

  sh->nentries = 0;
}

bool BLI_smallhash_haskey(SmallHash *sh, uintptr_t key)
{
  SmallHashEntry *e = smallhash_lookup(sh, key);

  return (e != NULL);
}

int BLI_smallhash_len(SmallHash *sh)
{
  return (int)sh->nentries;
}

BLI_INLINE SmallHashEntry *smallhash_iternext(SmallHashIter *iter, uintptr_t *key)
{
  while (iter->i < iter->sh->nbuckets) {
    if (smallhash_val_is_used(iter->sh->buckets[iter->i].val)) {
      if (key) {
        *key = iter->sh->buckets[iter->i].key;
      }

      return &iter->sh->buckets[iter->i++];
    }

    iter->i++;
  }

  return NULL;
}

void *BLI_smallhash_iternext(SmallHashIter *iter, uintptr_t *key)
{
  SmallHashEntry *e = smallhash_iternext(iter, key);

  return e ? e->val : NULL;
}

void **BLI_smallhash_iternext_p(SmallHashIter *iter, uintptr_t *key)
{
  SmallHashEntry *e = smallhash_iternext(iter, key);

  return e ? &e->val : NULL;
}

void *BLI_smallhash_iternew(SmallHash *sh, SmallHashIter *iter, uintptr_t *key)
{
  check_stack_move(sh);

  iter->sh = sh;
  iter->i = 0;

  return BLI_smallhash_iternext(iter, key);
}

void **BLI_smallhash_iternew_p(SmallHash *sh, SmallHashIter *iter, uintptr_t *key)
{
  check_stack_move(sh);

  iter->sh = sh;
  iter->i = 0;

  return BLI_smallhash_iternext_p(iter, key);
}

/* -------------------------------------------------------------------- */
/** \name Debugging & Introspection
 * \{ */

/* NOTE(campbell): this was called _print_smhash in knifetool.c
 * it may not be intended for general use. */
#if 0
void BLI_smallhash_print(SmallHash *sh)
{
  uint i, linecol = 79, c = 0;

  printf("{");
  for (i = 0; i < sh->nbuckets; i++) {
    if (sh->buckets[i].val == SMHASH_CELL_UNUSED) {
      printf("--u-");
    }
    else if (sh->buckets[i].val == SMHASH_CELL_FREE) {
      printf("--f-");
    }
    else {
      printf("%2x", (uint)sh->buckets[i].key);
    }

    if (i != sh->nbuckets - 1) {
      printf(", ");
    }

    c += 6;

    if (c >= linecol) {
      printf("\n ");
      c = 0;
    }
  }

  fflush(stdout);
}
#endif

#ifdef DEBUG
/**
 * Measure how well the hash function performs
 * (1.0 is perfect - no stepping needed).
 *
 * Smaller is better!
 */
double BLI_smallhash_calc_quality(SmallHash *sh)
{
  uint64_t sum = 0;
  uint i;

  if (sh->nentries == 0) {
    return -1.0;
  }

  for (i = 0; i < sh->nbuckets; i++) {
    if (sh->buckets[i].key != SMHASH_KEY_UNUSED) {
      uint64_t count = 0;
      SmallHashEntry *e, *e_final = &sh->buckets[i];
      uintptr_t h = smallhash_key(e_final->key);
      uintptr_t hoff = 1;

      for (e = &sh->buckets[h % sh->nbuckets]; e != e_final;
           h = SMHASH_NEXT(h, hoff), e = &sh->buckets[h % sh->nbuckets]) {
        count += 1;
      }

      sum += count;
    }
  }
  return ((double)(sh->nentries + sum) / (double)sh->nentries);
}
#endif

/** \} */
