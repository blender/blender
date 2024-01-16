/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 *
 * A general (pointer -> pointer) chaining hash table
 * for 'Abstract Data Types' (known as an ADT Hash Table).
 */

#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_math_base.h"
#include "BLI_mempool.h"
#include "BLI_sys_types.h" /* for intptr_t support */
#include "BLI_utildefines.h"

#define GHASH_INTERNAL_API
#include "BLI_ghash.h" /* own include */

/* keep last */
#include "BLI_strict_flags.h"

/* -------------------------------------------------------------------- */
/** \name Structs & Constants
 * \{ */

#define GHASH_USE_MODULO_BUCKETS

/**
 * Next prime after `2^n` (skipping 2 & 3).
 */
static const uint hashsizes[] = {
    5,       11,      17,      37,      67,       131,      257,      521,       1031,
    2053,    4099,    8209,    16411,   32771,    65537,    131101,   262147,    524309,
    1048583, 2097169, 4194319, 8388617, 16777259, 33554467, 67108879, 134217757, 268435459,
};

#ifdef GHASH_USE_MODULO_BUCKETS
#  define GHASH_MAX_SIZE 27
BLI_STATIC_ASSERT(ARRAY_SIZE(hashsizes) == GHASH_MAX_SIZE, "Invalid 'hashsizes' size");
#else
#  define GHASH_BUCKET_BIT_MIN 2
#  define GHASH_BUCKET_BIT_MAX 28 /* About 268M of buckets... */
#endif

/**
 * \note Max load #GHASH_LIMIT_GROW used to be 3. (pre 2.74).
 * Python uses 0.6666, tommyhashlib even goes down to 0.5.
 * Reducing our from 3 to 0.75 gives huge speedup
 * (about twice quicker pure GHash insertions/lookup,
 * about 25% - 30% quicker 'dynamic-topology' stroke drawing e.g.).
 * Min load #GHASH_LIMIT_SHRINK is a quarter of max load, to avoid resizing to quickly.
 */
#define GHASH_LIMIT_GROW(_nbkt) (((_nbkt) * 3) / 4)
#define GHASH_LIMIT_SHRINK(_nbkt) (((_nbkt) * 3) / 16)

/* WARNING! Keep in sync with ugly _gh_Entry in header!!! */
typedef struct Entry {
  struct Entry *next;

  void *key;
} Entry;

typedef struct GHashEntry {
  Entry e;

  void *val;
} GHashEntry;

typedef Entry GSetEntry;

#define GHASH_ENTRY_SIZE(_is_gset) ((_is_gset) ? sizeof(GSetEntry) : sizeof(GHashEntry))

struct GHash {
  GHashHashFP hashfp;
  GHashCmpFP cmpfp;

  Entry **buckets;
  BLI_mempool *entrypool;
  uint nbuckets;
  uint limit_grow, limit_shrink;
#ifdef GHASH_USE_MODULO_BUCKETS
  uint cursize, size_min;
#else
  uint bucket_mask, bucket_bit, bucket_bit_min;
#endif

  uint nentries;
  uint flag;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal Utility API
 * \{ */

BLI_INLINE void ghash_entry_copy(GHash *gh_dst,
                                 Entry *dst,
                                 const GHash *gh_src,
                                 const Entry *src,
                                 GHashKeyCopyFP keycopyfp,
                                 GHashValCopyFP valcopyfp)
{
  dst->key = (keycopyfp) ? keycopyfp(src->key) : src->key;

  if ((gh_dst->flag & GHASH_FLAG_IS_GSET) == 0) {
    if ((gh_src->flag & GHASH_FLAG_IS_GSET) == 0) {
      ((GHashEntry *)dst)->val = (valcopyfp) ? valcopyfp(((GHashEntry *)src)->val) :
                                               ((GHashEntry *)src)->val;
    }
    else {
      ((GHashEntry *)dst)->val = NULL;
    }
  }
}

/**
 * Get the full hash for a key.
 */
BLI_INLINE uint ghash_keyhash(const GHash *gh, const void *key)
{
  return gh->hashfp(key);
}

/**
 * Get the full hash for an entry.
 */
BLI_INLINE uint ghash_entryhash(const GHash *gh, const Entry *e)
{
  return gh->hashfp(e->key);
}

/**
 * Get the bucket-index for an already-computed full hash.
 */
BLI_INLINE uint ghash_bucket_index(const GHash *gh, const uint hash)
{
#ifdef GHASH_USE_MODULO_BUCKETS
  return hash % gh->nbuckets;
#else
  return hash & gh->bucket_mask;
#endif
}

/**
 * Find the index of next used bucket, starting from \a curr_bucket (\a gh is assumed non-empty).
 */
BLI_INLINE uint ghash_find_next_bucket_index(const GHash *gh, uint curr_bucket)
{
  if (curr_bucket >= gh->nbuckets) {
    curr_bucket = 0;
  }
  if (gh->buckets[curr_bucket]) {
    return curr_bucket;
  }
  for (; curr_bucket < gh->nbuckets; curr_bucket++) {
    if (gh->buckets[curr_bucket]) {
      return curr_bucket;
    }
  }
  for (curr_bucket = 0; curr_bucket < gh->nbuckets; curr_bucket++) {
    if (gh->buckets[curr_bucket]) {
      return curr_bucket;
    }
  }
  BLI_assert_unreachable();
  return 0;
}

/**
 * Expand buckets to the next size up or down.
 */
static void ghash_buckets_resize(GHash *gh, const uint nbuckets)
{
  Entry **buckets_old = gh->buckets;
  Entry **buckets_new;
  const uint nbuckets_old = gh->nbuckets;
  uint i;

  BLI_assert((gh->nbuckets != nbuckets) || !gh->buckets);
  //  printf("%s: %d -> %d\n", __func__, nbuckets_old, nbuckets);

  gh->nbuckets = nbuckets;
#ifdef GHASH_USE_MODULO_BUCKETS
#else
  gh->bucket_mask = nbuckets - 1;
#endif

  buckets_new = (Entry **)MEM_callocN(sizeof(*gh->buckets) * gh->nbuckets, __func__);

  if (buckets_old) {
    if (nbuckets > nbuckets_old) {
      for (i = 0; i < nbuckets_old; i++) {
        for (Entry *e = buckets_old[i], *e_next; e; e = e_next) {
          const uint hash = ghash_entryhash(gh, e);
          const uint bucket_index = ghash_bucket_index(gh, hash);
          e_next = e->next;
          e->next = buckets_new[bucket_index];
          buckets_new[bucket_index] = e;
        }
      }
    }
    else {
      for (i = 0; i < nbuckets_old; i++) {
#ifdef GHASH_USE_MODULO_BUCKETS
        for (Entry *e = buckets_old[i], *e_next; e; e = e_next) {
          const uint hash = ghash_entryhash(gh, e);
          const uint bucket_index = ghash_bucket_index(gh, hash);
          e_next = e->next;
          e->next = buckets_new[bucket_index];
          buckets_new[bucket_index] = e;
        }
#else
        /* No need to recompute hashes in this case, since our mask is just smaller,
         * all items in old bucket 'i' will go in same new bucket (i & new_mask)! */
        const uint bucket_index = ghash_bucket_index(gh, i);
        BLI_assert(!buckets_old[i] ||
                   (bucket_index == ghash_bucket_index(gh, ghash_entryhash(gh, buckets_old[i]))));
        Entry *e;
        for (e = buckets_old[i]; e && e->next; e = e->next) {
          /* pass */
        }
        if (e) {
          e->next = buckets_new[bucket_index];
          buckets_new[bucket_index] = buckets_old[i];
        }
#endif
      }
    }
  }

  gh->buckets = buckets_new;
  if (buckets_old) {
    MEM_freeN(buckets_old);
  }
}

/**
 * Check if the number of items in the GHash is large enough to require more buckets,
 * or small enough to require less buckets, and resize \a gh accordingly.
 */
static void ghash_buckets_expand(GHash *gh, const uint nentries, const bool user_defined)
{
  uint new_nbuckets;

  if (LIKELY(gh->buckets && (nentries < gh->limit_grow))) {
    return;
  }

  new_nbuckets = gh->nbuckets;

#ifdef GHASH_USE_MODULO_BUCKETS
  while ((nentries > gh->limit_grow) && (gh->cursize < GHASH_MAX_SIZE - 1)) {
    new_nbuckets = hashsizes[++gh->cursize];
    gh->limit_grow = GHASH_LIMIT_GROW(new_nbuckets);
  }
#else
  while ((nentries > gh->limit_grow) && (gh->bucket_bit < GHASH_BUCKET_BIT_MAX)) {
    new_nbuckets = 1u << ++gh->bucket_bit;
    gh->limit_grow = GHASH_LIMIT_GROW(new_nbuckets);
  }
#endif

  if (user_defined) {
#ifdef GHASH_USE_MODULO_BUCKETS
    gh->size_min = gh->cursize;
#else
    gh->bucket_bit_min = gh->bucket_bit;
#endif
  }

  if ((new_nbuckets == gh->nbuckets) && gh->buckets) {
    return;
  }

  gh->limit_grow = GHASH_LIMIT_GROW(new_nbuckets);
  gh->limit_shrink = GHASH_LIMIT_SHRINK(new_nbuckets);
  ghash_buckets_resize(gh, new_nbuckets);
}

static void ghash_buckets_contract(GHash *gh,
                                   const uint nentries,
                                   const bool user_defined,
                                   const bool force_shrink)
{
  uint new_nbuckets;

  if (!(force_shrink || (gh->flag & GHASH_FLAG_ALLOW_SHRINK))) {
    return;
  }

  if (LIKELY(gh->buckets && (nentries > gh->limit_shrink))) {
    return;
  }

  new_nbuckets = gh->nbuckets;

#ifdef GHASH_USE_MODULO_BUCKETS
  while ((nentries < gh->limit_shrink) && (gh->cursize > gh->size_min)) {
    new_nbuckets = hashsizes[--gh->cursize];
    gh->limit_shrink = GHASH_LIMIT_SHRINK(new_nbuckets);
  }
#else
  while ((nentries < gh->limit_shrink) && (gh->bucket_bit > gh->bucket_bit_min)) {
    new_nbuckets = 1u << --gh->bucket_bit;
    gh->limit_shrink = GHASH_LIMIT_SHRINK(new_nbuckets);
  }
#endif

  if (user_defined) {
#ifdef GHASH_USE_MODULO_BUCKETS
    gh->size_min = gh->cursize;
#else
    gh->bucket_bit_min = gh->bucket_bit;
#endif
  }

  if ((new_nbuckets == gh->nbuckets) && gh->buckets) {
    return;
  }

  gh->limit_grow = GHASH_LIMIT_GROW(new_nbuckets);
  gh->limit_shrink = GHASH_LIMIT_SHRINK(new_nbuckets);
  ghash_buckets_resize(gh, new_nbuckets);
}

/**
 * Clear and reset \a gh buckets, reserve again buckets for given number of entries.
 */
BLI_INLINE void ghash_buckets_reset(GHash *gh, const uint nentries)
{
  MEM_SAFE_FREE(gh->buckets);

#ifdef GHASH_USE_MODULO_BUCKETS
  gh->cursize = 0;
  gh->size_min = 0;
  gh->nbuckets = hashsizes[gh->cursize];
#else
  gh->bucket_bit = GHASH_BUCKET_BIT_MIN;
  gh->bucket_bit_min = GHASH_BUCKET_BIT_MIN;
  gh->nbuckets = 1u << gh->bucket_bit;
  gh->bucket_mask = gh->nbuckets - 1;
#endif

  gh->limit_grow = GHASH_LIMIT_GROW(gh->nbuckets);
  gh->limit_shrink = GHASH_LIMIT_SHRINK(gh->nbuckets);

  gh->nentries = 0;

  ghash_buckets_expand(gh, nentries, (nentries != 0));
}

/**
 * Internal lookup function.
 * Takes hash and bucket_index arguments to avoid calling #ghash_keyhash and #ghash_bucket_index
 * multiple times.
 */
BLI_INLINE Entry *ghash_lookup_entry_ex(const GHash *gh, const void *key, const uint bucket_index)
{
  Entry *e;
  /* If we do not store GHash, not worth computing it for each entry here!
   * Typically, comparison function will be quicker, and since it's needed in the end anyway... */
  for (e = gh->buckets[bucket_index]; e; e = e->next) {
    if (UNLIKELY(gh->cmpfp(key, e->key) == false)) {
      return e;
    }
  }

  return NULL;
}

/**
 * Internal lookup function, returns previous entry of target one too.
 * Takes bucket_index argument to avoid calling #ghash_keyhash and #ghash_bucket_index
 * multiple times.
 * Useful when modifying buckets somehow (like removing an entry...).
 */
BLI_INLINE Entry *ghash_lookup_entry_prev_ex(GHash *gh,
                                             const void *key,
                                             Entry **r_e_prev,
                                             const uint bucket_index)
{
  /* If we do not store GHash, not worth computing it for each entry here!
   * Typically, comparison function will be quicker, and since it's needed in the end anyway... */
  for (Entry *e_prev = NULL, *e = gh->buckets[bucket_index]; e; e_prev = e, e = e->next) {
    if (UNLIKELY(gh->cmpfp(key, e->key) == false)) {
      *r_e_prev = e_prev;
      return e;
    }
  }

  *r_e_prev = NULL;
  return NULL;
}

/**
 * Internal lookup function. Only wraps #ghash_lookup_entry_ex
 */
BLI_INLINE Entry *ghash_lookup_entry(const GHash *gh, const void *key)
{
  const uint hash = ghash_keyhash(gh, key);
  const uint bucket_index = ghash_bucket_index(gh, hash);
  return ghash_lookup_entry_ex(gh, key, bucket_index);
}

static GHash *ghash_new(GHashHashFP hashfp,
                        GHashCmpFP cmpfp,
                        const char *info,
                        const uint nentries_reserve,
                        const uint flag)
{
  GHash *gh = MEM_mallocN(sizeof(*gh), info);

  gh->hashfp = hashfp;
  gh->cmpfp = cmpfp;

  gh->buckets = NULL;
  gh->flag = flag;

  ghash_buckets_reset(gh, nentries_reserve);
  gh->entrypool = BLI_mempool_create(
      GHASH_ENTRY_SIZE(flag & GHASH_FLAG_IS_GSET), 64, 64, BLI_MEMPOOL_NOP);

  return gh;
}

/**
 * Internal insert function.
 * Takes hash and bucket_index arguments to avoid calling #ghash_keyhash and #ghash_bucket_index
 * multiple times.
 */
BLI_INLINE void ghash_insert_ex(GHash *gh, void *key, void *val, const uint bucket_index)
{
  GHashEntry *e = BLI_mempool_alloc(gh->entrypool);

  BLI_assert((gh->flag & GHASH_FLAG_ALLOW_DUPES) || (BLI_ghash_haskey(gh, key) == 0));
  BLI_assert(!(gh->flag & GHASH_FLAG_IS_GSET));

  e->e.next = gh->buckets[bucket_index];
  e->e.key = key;
  e->val = val;
  gh->buckets[bucket_index] = (Entry *)e;

  ghash_buckets_expand(gh, ++gh->nentries, false);
}

/**
 * Insert function that takes a pre-allocated entry.
 */
BLI_INLINE void ghash_insert_ex_keyonly_entry(GHash *gh,
                                              void *key,
                                              const uint bucket_index,
                                              Entry *e)
{
  BLI_assert((gh->flag & GHASH_FLAG_ALLOW_DUPES) || (BLI_ghash_haskey(gh, key) == 0));

  e->next = gh->buckets[bucket_index];
  e->key = key;
  gh->buckets[bucket_index] = e;

  ghash_buckets_expand(gh, ++gh->nentries, false);
}

/**
 * Insert function that doesn't set the value (use for GSet)
 */
BLI_INLINE void ghash_insert_ex_keyonly(GHash *gh, void *key, const uint bucket_index)
{
  Entry *e = BLI_mempool_alloc(gh->entrypool);

  BLI_assert((gh->flag & GHASH_FLAG_ALLOW_DUPES) || (BLI_ghash_haskey(gh, key) == 0));
  BLI_assert((gh->flag & GHASH_FLAG_IS_GSET) != 0);

  e->next = gh->buckets[bucket_index];
  e->key = key;
  gh->buckets[bucket_index] = e;

  ghash_buckets_expand(gh, ++gh->nentries, false);
}

BLI_INLINE void ghash_insert(GHash *gh, void *key, void *val)
{
  const uint hash = ghash_keyhash(gh, key);
  const uint bucket_index = ghash_bucket_index(gh, hash);

  ghash_insert_ex(gh, key, val, bucket_index);
}

BLI_INLINE bool ghash_insert_safe(GHash *gh,
                                  void *key,
                                  void *val,
                                  const bool override,
                                  GHashKeyFreeFP keyfreefp,
                                  GHashValFreeFP valfreefp)
{
  const uint hash = ghash_keyhash(gh, key);
  const uint bucket_index = ghash_bucket_index(gh, hash);
  GHashEntry *e = (GHashEntry *)ghash_lookup_entry_ex(gh, key, bucket_index);

  BLI_assert(!(gh->flag & GHASH_FLAG_IS_GSET));

  if (e) {
    if (override) {
      if (keyfreefp) {
        keyfreefp(e->e.key);
      }
      if (valfreefp) {
        valfreefp(e->val);
      }
      e->e.key = key;
      e->val = val;
    }
    return false;
  }
  ghash_insert_ex(gh, key, val, bucket_index);
  return true;
}

BLI_INLINE bool ghash_insert_safe_keyonly(GHash *gh,
                                          void *key,
                                          const bool override,
                                          GHashKeyFreeFP keyfreefp)
{
  const uint hash = ghash_keyhash(gh, key);
  const uint bucket_index = ghash_bucket_index(gh, hash);
  Entry *e = ghash_lookup_entry_ex(gh, key, bucket_index);

  BLI_assert((gh->flag & GHASH_FLAG_IS_GSET) != 0);

  if (e) {
    if (override) {
      if (keyfreefp) {
        keyfreefp(e->key);
      }
      e->key = key;
    }
    return false;
  }
  ghash_insert_ex_keyonly(gh, key, bucket_index);
  return true;
}

/**
 * Remove the entry and return it, caller must free from gh->entrypool.
 */
static Entry *ghash_remove_ex(GHash *gh,
                              const void *key,
                              GHashKeyFreeFP keyfreefp,
                              GHashValFreeFP valfreefp,
                              const uint bucket_index)
{
  Entry *e_prev;
  Entry *e = ghash_lookup_entry_prev_ex(gh, key, &e_prev, bucket_index);

  BLI_assert(!valfreefp || !(gh->flag & GHASH_FLAG_IS_GSET));

  if (e) {
    if (keyfreefp) {
      keyfreefp(e->key);
    }
    if (valfreefp) {
      valfreefp(((GHashEntry *)e)->val);
    }

    if (e_prev) {
      e_prev->next = e->next;
    }
    else {
      gh->buckets[bucket_index] = e->next;
    }

    ghash_buckets_contract(gh, --gh->nentries, false, false);
  }

  return e;
}

/**
 * Remove a random entry and return it (or NULL if empty), caller must free from gh->entrypool.
 */
static Entry *ghash_pop(GHash *gh, GHashIterState *state)
{
  uint curr_bucket = state->curr_bucket;
  if (gh->nentries == 0) {
    return NULL;
  }

  /* NOTE: using first_bucket_index here allows us to avoid potential
   * huge number of loops over buckets,
   * in case we are popping from a large ghash with few items in it... */
  curr_bucket = ghash_find_next_bucket_index(gh, curr_bucket);

  Entry *e = gh->buckets[curr_bucket];
  BLI_assert(e);

  ghash_remove_ex(gh, e->key, NULL, NULL, curr_bucket);

  state->curr_bucket = curr_bucket;
  return e;
}

/**
 * Run free callbacks for freeing entries.
 */
static void ghash_free_cb(GHash *gh, GHashKeyFreeFP keyfreefp, GHashValFreeFP valfreefp)
{
  uint i;

  BLI_assert(keyfreefp || valfreefp);
  BLI_assert(!valfreefp || !(gh->flag & GHASH_FLAG_IS_GSET));

  for (i = 0; i < gh->nbuckets; i++) {
    Entry *e;

    for (e = gh->buckets[i]; e; e = e->next) {
      if (keyfreefp) {
        keyfreefp(e->key);
      }
      if (valfreefp) {
        valfreefp(((GHashEntry *)e)->val);
      }
    }
  }
}

/**
 * Copy the GHash.
 */
static GHash *ghash_copy(const GHash *gh, GHashKeyCopyFP keycopyfp, GHashValCopyFP valcopyfp)
{
  GHash *gh_new;
  uint i;
  /* This allows us to be sure to get the same number of buckets in gh_new as in ghash. */
  const uint reserve_nentries_new = MAX2(GHASH_LIMIT_GROW(gh->nbuckets) - 1, gh->nentries);

  BLI_assert(!valcopyfp || !(gh->flag & GHASH_FLAG_IS_GSET));

  gh_new = ghash_new(gh->hashfp, gh->cmpfp, __func__, 0, gh->flag);
  ghash_buckets_expand(gh_new, reserve_nentries_new, false);

  BLI_assert(gh_new->nbuckets == gh->nbuckets);

  for (i = 0; i < gh->nbuckets; i++) {
    Entry *e;

    for (e = gh->buckets[i]; e; e = e->next) {
      Entry *e_new = BLI_mempool_alloc(gh_new->entrypool);
      ghash_entry_copy(gh_new, e_new, gh, e, keycopyfp, valcopyfp);

      /* Warning!
       * This means entries in buckets in new copy will be in reversed order!
       * This shall not be an issue though, since order should never be assumed in ghash. */

      /* NOTE: We can use 'i' here, since we are sure that
       * 'gh' and 'gh_new' have the same number of buckets! */
      e_new->next = gh_new->buckets[i];
      gh_new->buckets[i] = e_new;
    }
  }
  gh_new->nentries = gh->nentries;

  return gh_new;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GHash Public API
 * \{ */

GHash *BLI_ghash_new_ex(GHashHashFP hashfp,
                        GHashCmpFP cmpfp,
                        const char *info,
                        const uint nentries_reserve)
{
  return ghash_new(hashfp, cmpfp, info, nentries_reserve, 0);
}

GHash *BLI_ghash_new(GHashHashFP hashfp, GHashCmpFP cmpfp, const char *info)
{
  return BLI_ghash_new_ex(hashfp, cmpfp, info, 0);
}

GHash *BLI_ghash_copy(const GHash *gh, GHashKeyCopyFP keycopyfp, GHashValCopyFP valcopyfp)
{
  return ghash_copy(gh, keycopyfp, valcopyfp);
}

void BLI_ghash_reserve(GHash *gh, const uint nentries_reserve)
{
  ghash_buckets_expand(gh, nentries_reserve, true);
  ghash_buckets_contract(gh, nentries_reserve, true, false);
}

uint BLI_ghash_len(const GHash *gh)
{
  return gh->nentries;
}

void BLI_ghash_insert(GHash *gh, void *key, void *val)
{
  ghash_insert(gh, key, val);
}

bool BLI_ghash_reinsert(
    GHash *gh, void *key, void *val, GHashKeyFreeFP keyfreefp, GHashValFreeFP valfreefp)
{
  return ghash_insert_safe(gh, key, val, true, keyfreefp, valfreefp);
}

void *BLI_ghash_replace_key(GHash *gh, void *key)
{
  const uint hash = ghash_keyhash(gh, key);
  const uint bucket_index = ghash_bucket_index(gh, hash);
  GHashEntry *e = (GHashEntry *)ghash_lookup_entry_ex(gh, key, bucket_index);
  if (e != NULL) {
    void *key_prev = e->e.key;
    e->e.key = key;
    return key_prev;
  }
  return NULL;
}

void *BLI_ghash_lookup(const GHash *gh, const void *key)
{
  GHashEntry *e = (GHashEntry *)ghash_lookup_entry(gh, key);
  BLI_assert(!(gh->flag & GHASH_FLAG_IS_GSET));
  return e ? e->val : NULL;
}

void *BLI_ghash_lookup_default(const GHash *gh, const void *key, void *val_default)
{
  GHashEntry *e = (GHashEntry *)ghash_lookup_entry(gh, key);
  BLI_assert(!(gh->flag & GHASH_FLAG_IS_GSET));
  return e ? e->val : val_default;
}

void **BLI_ghash_lookup_p(GHash *gh, const void *key)
{
  GHashEntry *e = (GHashEntry *)ghash_lookup_entry(gh, key);
  BLI_assert(!(gh->flag & GHASH_FLAG_IS_GSET));
  return e ? &e->val : NULL;
}

bool BLI_ghash_ensure_p(GHash *gh, void *key, void ***r_val)
{
  const uint hash = ghash_keyhash(gh, key);
  const uint bucket_index = ghash_bucket_index(gh, hash);
  GHashEntry *e = (GHashEntry *)ghash_lookup_entry_ex(gh, key, bucket_index);
  const bool haskey = (e != NULL);

  if (!haskey) {
    e = BLI_mempool_alloc(gh->entrypool);
    ghash_insert_ex_keyonly_entry(gh, key, bucket_index, (Entry *)e);
  }

  *r_val = &e->val;
  return haskey;
}

bool BLI_ghash_ensure_p_ex(GHash *gh, const void *key, void ***r_key, void ***r_val)
{
  const uint hash = ghash_keyhash(gh, key);
  const uint bucket_index = ghash_bucket_index(gh, hash);
  GHashEntry *e = (GHashEntry *)ghash_lookup_entry_ex(gh, key, bucket_index);
  const bool haskey = (e != NULL);

  if (!haskey) {
    /* Pass 'key' in case we resize. */
    e = BLI_mempool_alloc(gh->entrypool);
    ghash_insert_ex_keyonly_entry(gh, (void *)key, bucket_index, (Entry *)e);
    e->e.key = NULL; /* caller must re-assign */
  }

  *r_key = &e->e.key;
  *r_val = &e->val;
  return haskey;
}

bool BLI_ghash_remove(GHash *gh,
                      const void *key,
                      GHashKeyFreeFP keyfreefp,
                      GHashValFreeFP valfreefp)
{
  const uint hash = ghash_keyhash(gh, key);
  const uint bucket_index = ghash_bucket_index(gh, hash);
  Entry *e = ghash_remove_ex(gh, key, keyfreefp, valfreefp, bucket_index);
  if (e) {
    BLI_mempool_free(gh->entrypool, e);
    return true;
  }
  return false;
}

void *BLI_ghash_popkey(GHash *gh, const void *key, GHashKeyFreeFP keyfreefp)
{
  /* Same as above but return the value,
   * no free value argument since it will be returned. */

  const uint hash = ghash_keyhash(gh, key);
  const uint bucket_index = ghash_bucket_index(gh, hash);
  GHashEntry *e = (GHashEntry *)ghash_remove_ex(gh, key, keyfreefp, NULL, bucket_index);
  BLI_assert(!(gh->flag & GHASH_FLAG_IS_GSET));
  if (e) {
    void *val = e->val;
    BLI_mempool_free(gh->entrypool, e);
    return val;
  }
  return NULL;
}

bool BLI_ghash_haskey(const GHash *gh, const void *key)
{
  return (ghash_lookup_entry(gh, key) != NULL);
}

bool BLI_ghash_pop(GHash *gh, GHashIterState *state, void **r_key, void **r_val)
{
  GHashEntry *e = (GHashEntry *)ghash_pop(gh, state);

  BLI_assert(!(gh->flag & GHASH_FLAG_IS_GSET));

  if (e) {
    *r_key = e->e.key;
    *r_val = e->val;

    BLI_mempool_free(gh->entrypool, e);
    return true;
  }

  *r_key = *r_val = NULL;
  return false;
}

void BLI_ghash_clear_ex(GHash *gh,
                        GHashKeyFreeFP keyfreefp,
                        GHashValFreeFP valfreefp,
                        const uint nentries_reserve)
{
  if (keyfreefp || valfreefp) {
    ghash_free_cb(gh, keyfreefp, valfreefp);
  }

  ghash_buckets_reset(gh, nentries_reserve);
  BLI_mempool_clear_ex(gh->entrypool, nentries_reserve ? (int)nentries_reserve : -1);
}

void BLI_ghash_clear(GHash *gh, GHashKeyFreeFP keyfreefp, GHashValFreeFP valfreefp)
{
  BLI_ghash_clear_ex(gh, keyfreefp, valfreefp, 0);
}

void BLI_ghash_free(GHash *gh, GHashKeyFreeFP keyfreefp, GHashValFreeFP valfreefp)
{
  BLI_assert((int)gh->nentries == BLI_mempool_len(gh->entrypool));
  if (keyfreefp || valfreefp) {
    ghash_free_cb(gh, keyfreefp, valfreefp);
  }

  MEM_freeN(gh->buckets);
  BLI_mempool_destroy(gh->entrypool);
  MEM_freeN(gh);
}

void BLI_ghash_flag_set(GHash *gh, uint flag)
{
  gh->flag |= flag;
}

void BLI_ghash_flag_clear(GHash *gh, uint flag)
{
  gh->flag &= ~flag;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GHash Iterator API
 * \{ */

GHashIterator *BLI_ghashIterator_new(GHash *gh)
{
  GHashIterator *ghi = MEM_mallocN(sizeof(*ghi), "ghash iterator");
  BLI_ghashIterator_init(ghi, gh);
  return ghi;
}

void BLI_ghashIterator_init(GHashIterator *ghi, GHash *gh)
{
  ghi->gh = gh;
  ghi->curEntry = NULL;
  ghi->curBucket = UINT_MAX; /* wraps to zero */
  if (gh->nentries) {
    do {
      ghi->curBucket++;
      if (UNLIKELY(ghi->curBucket == ghi->gh->nbuckets)) {
        break;
      }
      ghi->curEntry = ghi->gh->buckets[ghi->curBucket];
    } while (!ghi->curEntry);
  }
}

void BLI_ghashIterator_step(GHashIterator *ghi)
{
  if (ghi->curEntry) {
    ghi->curEntry = ghi->curEntry->next;
    while (!ghi->curEntry) {
      ghi->curBucket++;
      if (ghi->curBucket == ghi->gh->nbuckets) {
        break;
      }
      ghi->curEntry = ghi->gh->buckets[ghi->curBucket];
    }
  }
}

void BLI_ghashIterator_free(GHashIterator *ghi)
{
  MEM_freeN(ghi);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GSet Public API
 * \{ */

GSet *BLI_gset_new_ex(GSetHashFP hashfp,
                      GSetCmpFP cmpfp,
                      const char *info,
                      const uint nentries_reserve)
{
  return (GSet *)ghash_new(hashfp, cmpfp, info, nentries_reserve, GHASH_FLAG_IS_GSET);
}

GSet *BLI_gset_new(GSetHashFP hashfp, GSetCmpFP cmpfp, const char *info)
{
  return BLI_gset_new_ex(hashfp, cmpfp, info, 0);
}

GSet *BLI_gset_copy(const GSet *gs, GHashKeyCopyFP keycopyfp)
{
  return (GSet *)ghash_copy((const GHash *)gs, keycopyfp, NULL);
}

uint BLI_gset_len(const GSet *gs)
{
  return ((GHash *)gs)->nentries;
}

void BLI_gset_insert(GSet *gs, void *key)
{
  const uint hash = ghash_keyhash((GHash *)gs, key);
  const uint bucket_index = ghash_bucket_index((GHash *)gs, hash);
  ghash_insert_ex_keyonly((GHash *)gs, key, bucket_index);
}

bool BLI_gset_add(GSet *gs, void *key)
{
  return ghash_insert_safe_keyonly((GHash *)gs, key, false, NULL);
}

bool BLI_gset_ensure_p_ex(GSet *gs, const void *key, void ***r_key)
{
  const uint hash = ghash_keyhash((GHash *)gs, key);
  const uint bucket_index = ghash_bucket_index((GHash *)gs, hash);
  GSetEntry *e = (GSetEntry *)ghash_lookup_entry_ex((const GHash *)gs, key, bucket_index);
  const bool haskey = (e != NULL);

  if (!haskey) {
    /* Pass 'key' in case we resize */
    e = BLI_mempool_alloc(((GHash *)gs)->entrypool);
    ghash_insert_ex_keyonly_entry((GHash *)gs, (void *)key, bucket_index, (Entry *)e);
    e->key = NULL; /* caller must re-assign */
  }

  *r_key = &e->key;
  return haskey;
}

bool BLI_gset_reinsert(GSet *gs, void *key, GSetKeyFreeFP keyfreefp)
{
  return ghash_insert_safe_keyonly((GHash *)gs, key, true, keyfreefp);
}

void *BLI_gset_replace_key(GSet *gs, void *key)
{
  return BLI_ghash_replace_key((GHash *)gs, key);
}

bool BLI_gset_remove(GSet *gs, const void *key, GSetKeyFreeFP keyfreefp)
{
  return BLI_ghash_remove((GHash *)gs, key, keyfreefp, NULL);
}

bool BLI_gset_haskey(const GSet *gs, const void *key)
{
  return (ghash_lookup_entry((const GHash *)gs, key) != NULL);
}

bool BLI_gset_pop(GSet *gs, GSetIterState *state, void **r_key)
{
  GSetEntry *e = (GSetEntry *)ghash_pop((GHash *)gs, (GHashIterState *)state);

  if (e) {
    *r_key = e->key;

    BLI_mempool_free(((GHash *)gs)->entrypool, e);
    return true;
  }

  *r_key = NULL;
  return false;
}

void BLI_gset_clear_ex(GSet *gs, GSetKeyFreeFP keyfreefp, const uint nentries_reserve)
{
  BLI_ghash_clear_ex((GHash *)gs, keyfreefp, NULL, nentries_reserve);
}

void BLI_gset_clear(GSet *gs, GSetKeyFreeFP keyfreefp)
{
  BLI_ghash_clear((GHash *)gs, keyfreefp, NULL);
}

void BLI_gset_free(GSet *gs, GSetKeyFreeFP keyfreefp)
{
  BLI_ghash_free((GHash *)gs, keyfreefp, NULL);
}

void BLI_gset_flag_set(GSet *gs, uint flag)
{
  ((GHash *)gs)->flag |= flag;
}

void BLI_gset_flag_clear(GSet *gs, uint flag)
{
  ((GHash *)gs)->flag &= ~flag;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GSet Combined Key/Value Usage
 *
 * \note Not typical `set` use, only use when the pointer identity matters.
 * This can be useful when the key references data stored outside the GSet.
 * \{ */

void *BLI_gset_lookup(const GSet *gs, const void *key)
{
  Entry *e = ghash_lookup_entry((const GHash *)gs, key);
  return e ? e->key : NULL;
}

void *BLI_gset_pop_key(GSet *gs, const void *key)
{
  const uint hash = ghash_keyhash((GHash *)gs, key);
  const uint bucket_index = ghash_bucket_index((GHash *)gs, hash);
  Entry *e = ghash_remove_ex((GHash *)gs, key, NULL, NULL, bucket_index);
  if (e) {
    void *key_ret = e->key;
    BLI_mempool_free(((GHash *)gs)->entrypool, e);
    return key_ret;
  }
  return NULL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debugging & Introspection
 * \{ */

int BLI_ghash_buckets_len(const GHash *gh)
{
  return (int)gh->nbuckets;
}
int BLI_gset_buckets_len(const GSet *gs)
{
  return BLI_ghash_buckets_len((const GHash *)gs);
}

double BLI_ghash_calc_quality_ex(GHash *gh,
                                 double *r_load,
                                 double *r_variance,
                                 double *r_prop_empty_buckets,
                                 double *r_prop_overloaded_buckets,
                                 int *r_biggest_bucket)
{
  double mean;
  uint i;

  if (gh->nentries == 0) {
    if (r_load) {
      *r_load = 0.0;
    }
    if (r_variance) {
      *r_variance = 0.0;
    }
    if (r_prop_empty_buckets) {
      *r_prop_empty_buckets = 1.0;
    }
    if (r_prop_overloaded_buckets) {
      *r_prop_overloaded_buckets = 0.0;
    }
    if (r_biggest_bucket) {
      *r_biggest_bucket = 0;
    }

    return 0.0;
  }

  mean = (double)gh->nentries / (double)gh->nbuckets;
  if (r_load) {
    *r_load = mean;
  }
  if (r_biggest_bucket) {
    *r_biggest_bucket = 0;
  }

  if (r_variance) {
    /* We already know our mean (i.e. load factor), easy to compute variance.
     * See https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Two-pass_algorithm
     */
    double sum = 0.0;
    for (i = 0; i < gh->nbuckets; i++) {
      int count = 0;
      Entry *e;
      for (e = gh->buckets[i]; e; e = e->next) {
        count++;
      }
      sum += ((double)count - mean) * ((double)count - mean);
    }
    *r_variance = sum / (double)(gh->nbuckets - 1);
  }

  {
    uint64_t sum = 0;
    uint64_t overloaded_buckets_threshold = (uint64_t)max_ii(GHASH_LIMIT_GROW(1), 1);
    uint64_t sum_overloaded = 0;
    uint64_t sum_empty = 0;

    for (i = 0; i < gh->nbuckets; i++) {
      uint64_t count = 0;
      Entry *e;
      for (e = gh->buckets[i]; e; e = e->next) {
        count++;
      }
      if (r_biggest_bucket) {
        *r_biggest_bucket = max_ii(*r_biggest_bucket, (int)count);
      }
      if (r_prop_overloaded_buckets && (count > overloaded_buckets_threshold)) {
        sum_overloaded++;
      }
      if (r_prop_empty_buckets && !count) {
        sum_empty++;
      }
      sum += count * (count + 1);
    }
    if (r_prop_overloaded_buckets) {
      *r_prop_overloaded_buckets = (double)sum_overloaded / (double)gh->nbuckets;
    }
    if (r_prop_empty_buckets) {
      *r_prop_empty_buckets = (double)sum_empty / (double)gh->nbuckets;
    }
    return ((double)sum * (double)gh->nbuckets /
            ((double)gh->nentries * (gh->nentries + 2 * gh->nbuckets - 1)));
  }
}
double BLI_gset_calc_quality_ex(GSet *gs,
                                double *r_load,
                                double *r_variance,
                                double *r_prop_empty_buckets,
                                double *r_prop_overloaded_buckets,
                                int *r_biggest_bucket)
{
  return BLI_ghash_calc_quality_ex((GHash *)gs,
                                   r_load,
                                   r_variance,
                                   r_prop_empty_buckets,
                                   r_prop_overloaded_buckets,
                                   r_biggest_bucket);
}

double BLI_ghash_calc_quality(GHash *gh)
{
  return BLI_ghash_calc_quality_ex(gh, NULL, NULL, NULL, NULL, NULL);
}
double BLI_gset_calc_quality(GSet *gs)
{
  return BLI_ghash_calc_quality_ex((GHash *)gs, NULL, NULL, NULL, NULL, NULL);
}

/** \} */
