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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

#pragma once

/** \file
 * \ingroup bli
 *
 * GHash is a hash-map implementation (unordered key, value pairs).
 *
 * This is also used to implement a 'set' (see #GSet below).
 */

#include "BLI_compiler_attrs.h"
#include "BLI_compiler_compat.h"
#include "BLI_sys_types.h" /* for bool */

#ifdef __cplusplus
extern "C" {
#endif

#define _GHASH_INTERNAL_ATTR
#ifndef GHASH_INTERNAL_API
#  ifdef __GNUC__
#    undef _GHASH_INTERNAL_ATTR
#    define _GHASH_INTERNAL_ATTR __attribute__((deprecated)) /* not deprecated, just private. */
#  endif
#endif

typedef unsigned int (*GHashHashFP)(const void *key);
/** returns false when equal */
typedef bool (*GHashCmpFP)(const void *a, const void *b);
typedef void (*GHashKeyFreeFP)(void *key);
typedef void (*GHashValFreeFP)(void *val);
typedef void *(*GHashKeyCopyFP)(const void *key);
typedef void *(*GHashValCopyFP)(const void *val);

typedef struct GHash GHash;

typedef struct GHashIterator {
  GHash *gh;
  struct Entry *curEntry;
  unsigned int curBucket;
} GHashIterator;

typedef struct GHashIterState {
  unsigned int curr_bucket _GHASH_INTERNAL_ATTR;
} GHashIterState;

enum {
  GHASH_FLAG_ALLOW_DUPES = (1 << 0),  /* Only checked for in debug mode */
  GHASH_FLAG_ALLOW_SHRINK = (1 << 1), /* Allow to shrink buckets' size. */

#ifdef GHASH_INTERNAL_API
  /* Internal usage only */
  /* Whether the GHash is actually used as GSet (no value storage). */
  GHASH_FLAG_IS_GSET = (1 << 16),
#endif
};

/* -------------------------------------------------------------------- */
/** \name GHash API
 *
 * Defined in `BLI_ghash.c`
 * \{ */

GHash *BLI_ghash_new_ex(GHashHashFP hashfp,
                        GHashCmpFP cmpfp,
                        const char *info,
                        const unsigned int nentries_reserve) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
GHash *BLI_ghash_new(GHashHashFP hashfp,
                     GHashCmpFP cmpfp,
                     const char *info) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
GHash *BLI_ghash_copy(const GHash *gh,
                      GHashKeyCopyFP keycopyfp,
                      GHashValCopyFP valcopyfp) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
void BLI_ghash_free(GHash *gh, GHashKeyFreeFP keyfreefp, GHashValFreeFP valfreefp);
void BLI_ghash_reserve(GHash *gh, const unsigned int nentries_reserve);
void BLI_ghash_insert(GHash *gh, void *key, void *val);
bool BLI_ghash_reinsert(
    GHash *gh, void *key, void *val, GHashKeyFreeFP keyfreefp, GHashValFreeFP valfreefp);
void *BLI_ghash_replace_key(GHash *gh, void *key);
void *BLI_ghash_lookup(const GHash *gh, const void *key) ATTR_WARN_UNUSED_RESULT;
void *BLI_ghash_lookup_default(const GHash *gh,
                               const void *key,
                               void *val_default) ATTR_WARN_UNUSED_RESULT;
void **BLI_ghash_lookup_p(GHash *gh, const void *key) ATTR_WARN_UNUSED_RESULT;
bool BLI_ghash_ensure_p(GHash *gh, void *key, void ***r_val) ATTR_WARN_UNUSED_RESULT;
bool BLI_ghash_ensure_p_ex(GHash *gh, const void *key, void ***r_key, void ***r_val)
    ATTR_WARN_UNUSED_RESULT;
bool BLI_ghash_remove(GHash *gh,
                      const void *key,
                      GHashKeyFreeFP keyfreefp,
                      GHashValFreeFP valfreefp);
void BLI_ghash_clear(GHash *gh, GHashKeyFreeFP keyfreefp, GHashValFreeFP valfreefp);
void BLI_ghash_clear_ex(GHash *gh,
                        GHashKeyFreeFP keyfreefp,
                        GHashValFreeFP valfreefp,
                        const unsigned int nentries_reserve);
void *BLI_ghash_popkey(GHash *gh,
                       const void *key,
                       GHashKeyFreeFP keyfreefp) ATTR_WARN_UNUSED_RESULT;
bool BLI_ghash_haskey(const GHash *gh, const void *key) ATTR_WARN_UNUSED_RESULT;
bool BLI_ghash_pop(GHash *gh, GHashIterState *state, void **r_key, void **r_val)
    ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
unsigned int BLI_ghash_len(const GHash *gh) ATTR_WARN_UNUSED_RESULT;
void BLI_ghash_flag_set(GHash *gh, unsigned int flag);
void BLI_ghash_flag_clear(GHash *gh, unsigned int flag);

/** \} */

/* -------------------------------------------------------------------- */
/** \name GHash Iterator
 * \{ */

GHashIterator *BLI_ghashIterator_new(GHash *gh) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;

void BLI_ghashIterator_init(GHashIterator *ghi, GHash *gh);
void BLI_ghashIterator_free(GHashIterator *ghi);
void BLI_ghashIterator_step(GHashIterator *ghi);

BLI_INLINE void *BLI_ghashIterator_getKey(GHashIterator *ghi) ATTR_WARN_UNUSED_RESULT;
BLI_INLINE void *BLI_ghashIterator_getValue(GHashIterator *ghi) ATTR_WARN_UNUSED_RESULT;
BLI_INLINE void **BLI_ghashIterator_getValue_p(GHashIterator *ghi) ATTR_WARN_UNUSED_RESULT;
BLI_INLINE bool BLI_ghashIterator_done(const GHashIterator *ghi) ATTR_WARN_UNUSED_RESULT;

struct _gh_Entry {
  void *next, *key, *val;
};
BLI_INLINE void *BLI_ghashIterator_getKey(GHashIterator *ghi)
{
  return ((struct _gh_Entry *)ghi->curEntry)->key;
}
BLI_INLINE void *BLI_ghashIterator_getValue(GHashIterator *ghi)
{
  return ((struct _gh_Entry *)ghi->curEntry)->val;
}
BLI_INLINE void **BLI_ghashIterator_getValue_p(GHashIterator *ghi)
{
  return &((struct _gh_Entry *)ghi->curEntry)->val;
}
BLI_INLINE bool BLI_ghashIterator_done(const GHashIterator *ghi)
{
  return !ghi->curEntry;
}
/* disallow further access */
#ifdef __GNUC__
#  pragma GCC poison _gh_Entry
#else
#  define _gh_Entry void
#endif

#define GHASH_ITER(gh_iter_, ghash_) \
  for (BLI_ghashIterator_init(&gh_iter_, ghash_); BLI_ghashIterator_done(&gh_iter_) == false; \
       BLI_ghashIterator_step(&gh_iter_))

#define GHASH_ITER_INDEX(gh_iter_, ghash_, i_) \
  for (BLI_ghashIterator_init(&gh_iter_, ghash_), i_ = 0; \
       BLI_ghashIterator_done(&gh_iter_) == false; \
       BLI_ghashIterator_step(&gh_iter_), i_++)

/** \} */

/* -------------------------------------------------------------------- */
/** \name GSet API
 * A 'set' implementation (unordered collection of unique elements).
 *
 * Internally this is a 'GHash' without any keys,
 * which is why this API's are in the same header & source file.
 *
 * \{ */

typedef struct GSet GSet;

typedef GHashHashFP GSetHashFP;
typedef GHashCmpFP GSetCmpFP;
typedef GHashKeyFreeFP GSetKeyFreeFP;
typedef GHashKeyCopyFP GSetKeyCopyFP;

typedef GHashIterState GSetIterState;

GSet *BLI_gset_new_ex(GSetHashFP hashfp,
                      GSetCmpFP cmpfp,
                      const char *info,
                      const unsigned int nentries_reserve) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
GSet *BLI_gset_new(GSetHashFP hashfp,
                   GSetCmpFP cmpfp,
                   const char *info) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
GSet *BLI_gset_copy(const GSet *gs, GSetKeyCopyFP keycopyfp) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
unsigned int BLI_gset_len(const GSet *gs) ATTR_WARN_UNUSED_RESULT;
void BLI_gset_flag_set(GSet *gs, unsigned int flag);
void BLI_gset_flag_clear(GSet *gs, unsigned int flag);
void BLI_gset_free(GSet *gs, GSetKeyFreeFP keyfreefp);
void BLI_gset_insert(GSet *gs, void *key);
bool BLI_gset_add(GSet *gs, void *key);
bool BLI_gset_ensure_p_ex(GSet *gs, const void *key, void ***r_key);
bool BLI_gset_reinsert(GSet *gh, void *key, GSetKeyFreeFP keyfreefp);
void *BLI_gset_replace_key(GSet *gs, void *key);
bool BLI_gset_haskey(const GSet *gs, const void *key) ATTR_WARN_UNUSED_RESULT;
bool BLI_gset_pop(GSet *gs, GSetIterState *state, void **r_key) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
bool BLI_gset_remove(GSet *gs, const void *key, GSetKeyFreeFP keyfreefp);
void BLI_gset_clear_ex(GSet *gs, GSetKeyFreeFP keyfreefp, const unsigned int nentries_reserve);
void BLI_gset_clear(GSet *gs, GSetKeyFreeFP keyfreefp);

/* When set's are used for key & value. */
void *BLI_gset_lookup(const GSet *gs, const void *key) ATTR_WARN_UNUSED_RESULT;
void *BLI_gset_pop_key(GSet *gs, const void *key) ATTR_WARN_UNUSED_RESULT;

/** \} */

/* -------------------------------------------------------------------- */
/** \name GSet Iterator
 * \{ */

/* rely on inline api for now */

/* so we can cast but compiler sees as different */
typedef struct GSetIterator {
  GHashIterator _ghi
#if defined(__GNUC__) && !defined(__clang__)
      __attribute__((deprecated))
#endif
      ;
} GSetIterator;

BLI_INLINE GSetIterator *BLI_gsetIterator_new(GSet *gs)
{
  return (GSetIterator *)BLI_ghashIterator_new((GHash *)gs);
}
BLI_INLINE void BLI_gsetIterator_init(GSetIterator *gsi, GSet *gs)
{
  BLI_ghashIterator_init((GHashIterator *)gsi, (GHash *)gs);
}
BLI_INLINE void BLI_gsetIterator_free(GSetIterator *gsi)
{
  BLI_ghashIterator_free((GHashIterator *)gsi);
}
BLI_INLINE void *BLI_gsetIterator_getKey(GSetIterator *gsi)
{
  return BLI_ghashIterator_getKey((GHashIterator *)gsi);
}
BLI_INLINE void BLI_gsetIterator_step(GSetIterator *gsi)
{
  BLI_ghashIterator_step((GHashIterator *)gsi);
}
BLI_INLINE bool BLI_gsetIterator_done(const GSetIterator *gsi)
{
  return BLI_ghashIterator_done((const GHashIterator *)gsi);
}

#define GSET_ITER(gs_iter_, gset_) \
  for (BLI_gsetIterator_init(&gs_iter_, gset_); BLI_gsetIterator_done(&gs_iter_) == false; \
       BLI_gsetIterator_step(&gs_iter_))

#define GSET_ITER_INDEX(gs_iter_, gset_, i_) \
  for (BLI_gsetIterator_init(&gs_iter_, gset_), i_ = 0; \
       BLI_gsetIterator_done(&gs_iter_) == false; \
       BLI_gsetIterator_step(&gs_iter_), i_++)

/** \} */

/* -------------------------------------------------------------------- */
/** \name GHash/GSet Debugging API's
 * \{ */

/* For testing, debugging only */
#ifdef GHASH_INTERNAL_API
int BLI_ghash_buckets_len(const GHash *gh);
int BLI_gset_buckets_len(const GSet *gs);

double BLI_ghash_calc_quality_ex(GHash *gh,
                                 double *r_load,
                                 double *r_variance,
                                 double *r_prop_empty_buckets,
                                 double *r_prop_overloaded_buckets,
                                 int *r_biggest_bucket);
double BLI_gset_calc_quality_ex(GSet *gs,
                                double *r_load,
                                double *r_variance,
                                double *r_prop_empty_buckets,
                                double *r_prop_overloaded_buckets,
                                int *r_biggest_bucket);
double BLI_ghash_calc_quality(GHash *gh);
double BLI_gset_calc_quality(GSet *gs);
#endif /* GHASH_INTERNAL_API */
/** \} */

/* -------------------------------------------------------------------- */
/** \name GHash/GSet Macros
 * \{ */

#define GHASH_FOREACH_BEGIN(type, var, what) \
  do { \
    GHashIterator gh_iter##var; \
    GHASH_ITER (gh_iter##var, what) { \
      type var = (type)(BLI_ghashIterator_getValue(&gh_iter##var));

#define GHASH_FOREACH_END() \
  } \
  } \
  while (0)

#define GSET_FOREACH_BEGIN(type, var, what) \
  do { \
    GSetIterator gh_iter##var; \
    GSET_ITER (gh_iter##var, what) { \
      type var = (type)(BLI_gsetIterator_getKey(&gh_iter##var));

#define GSET_FOREACH_END() \
  } \
  } \
  while (0)

/** \} */

/* -------------------------------------------------------------------- */
/** \name GHash/GSet Utils
 *
 * Defined in `BLI_ghash_utils.c`
 * \{ */

/**
 * Callbacks for GHash (`BLI_ghashutil_`)
 *
 * \note '_p' suffix denotes void pointer arg,
 * so we can have functions that take correctly typed args too.
 */

unsigned int BLI_ghashutil_ptrhash(const void *key);
bool BLI_ghashutil_ptrcmp(const void *a, const void *b);

unsigned int BLI_ghashutil_strhash_n(const char *key, size_t n);
#define BLI_ghashutil_strhash(key) \
  (CHECK_TYPE_ANY(key, char *, const char *, const char *const), BLI_ghashutil_strhash_p(key))
unsigned int BLI_ghashutil_strhash_p(const void *ptr);
unsigned int BLI_ghashutil_strhash_p_murmur(const void *ptr);
bool BLI_ghashutil_strcmp(const void *a, const void *b);

#define BLI_ghashutil_inthash(key) \
  (CHECK_TYPE_ANY(&(key), int *, const int *), BLI_ghashutil_uinthash((unsigned int)key))
unsigned int BLI_ghashutil_uinthash(unsigned int key);
unsigned int BLI_ghashutil_inthash_p(const void *ptr);
unsigned int BLI_ghashutil_inthash_p_murmur(const void *ptr);
unsigned int BLI_ghashutil_inthash_p_simple(const void *ptr);
bool BLI_ghashutil_intcmp(const void *a, const void *b);

size_t BLI_ghashutil_combine_hash(size_t hash_a, size_t hash_b);

unsigned int BLI_ghashutil_uinthash_v4(const unsigned int key[4]);
#define BLI_ghashutil_inthash_v4(key) \
  (CHECK_TYPE_ANY(key, int *, const int *), BLI_ghashutil_uinthash_v4((const unsigned int *)key))
#define BLI_ghashutil_inthash_v4_p ((GSetHashFP)BLI_ghashutil_uinthash_v4)
#define BLI_ghashutil_uinthash_v4_p ((GSetHashFP)BLI_ghashutil_uinthash_v4)
unsigned int BLI_ghashutil_uinthash_v4_murmur(const unsigned int key[4]);
#define BLI_ghashutil_inthash_v4_murmur(key) \
  (CHECK_TYPE_ANY(key, int *, const int *), \
   BLI_ghashutil_uinthash_v4_murmur((const unsigned int *)key))
#define BLI_ghashutil_inthash_v4_p_murmur ((GSetHashFP)BLI_ghashutil_uinthash_v4_murmur)
#define BLI_ghashutil_uinthash_v4_p_murmur ((GSetHashFP)BLI_ghashutil_uinthash_v4_murmur)
bool BLI_ghashutil_uinthash_v4_cmp(const void *a, const void *b);
#define BLI_ghashutil_inthash_v4_cmp BLI_ghashutil_uinthash_v4_cmp

typedef struct GHashPair {
  const void *first;
  const void *second;
} GHashPair;

GHashPair *BLI_ghashutil_pairalloc(const void *first, const void *second);
unsigned int BLI_ghashutil_pairhash(const void *ptr);
bool BLI_ghashutil_paircmp(const void *a, const void *b);
void BLI_ghashutil_pairfree(void *ptr);

/**
 * Wrapper GHash Creation Functions
 */

GHash *BLI_ghash_ptr_new_ex(const char *info, const unsigned int nentries_reserve)
    ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
GHash *BLI_ghash_ptr_new(const char *info) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
GHash *BLI_ghash_str_new_ex(const char *info, const unsigned int nentries_reserve)
    ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
GHash *BLI_ghash_str_new(const char *info) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
GHash *BLI_ghash_int_new_ex(const char *info, const unsigned int nentries_reserve)
    ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
GHash *BLI_ghash_int_new(const char *info) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
GHash *BLI_ghash_pair_new_ex(const char *info, const unsigned int nentries_reserve)
    ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
GHash *BLI_ghash_pair_new(const char *info) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;

GSet *BLI_gset_ptr_new_ex(const char *info,
                          const unsigned int nentries_reserve) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
GSet *BLI_gset_ptr_new(const char *info);
GSet *BLI_gset_str_new_ex(const char *info,
                          const unsigned int nentries_reserve) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
GSet *BLI_gset_str_new(const char *info);
GSet *BLI_gset_pair_new_ex(const char *info, const unsigned int nentries_reserve)
    ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
GSet *BLI_gset_pair_new(const char *info) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
GSet *BLI_gset_int_new_ex(const char *info,
                          const unsigned int nentries_reserve) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
GSet *BLI_gset_int_new(const char *info) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;

/** \} */

#ifdef __cplusplus
}
#endif
