/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2008 Blender Foundation */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_compiler_attrs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uintptr_t key;
  void *val;
} SmallHashEntry;

/**
 * How much stack space to use before dynamically allocating memory.
 * set to match one of the values in 'hashsizes' to avoid too many mallocs.
 */
#define SMSTACKSIZE 131
typedef struct SmallHash {
  unsigned int nbuckets;
  unsigned int nentries;
  unsigned int cursize;

  SmallHashEntry *buckets;
  SmallHashEntry buckets_stack[SMSTACKSIZE];
} SmallHash;

typedef struct {
  const SmallHash *sh;
  unsigned int i;
} SmallHashIter;

void BLI_smallhash_init_ex(SmallHash *sh, unsigned int nentries_reserve) ATTR_NONNULL(1);
void BLI_smallhash_init(SmallHash *sh) ATTR_NONNULL(1);
/**
 * \note does *not* free *sh itself! only the direct data!
 */
void BLI_smallhash_release(SmallHash *sh) ATTR_NONNULL(1);
void BLI_smallhash_insert(SmallHash *sh, uintptr_t key, void *item) ATTR_NONNULL(1);
/**
 * Inserts a new value to a key that may already be in #GHash.
 *
 * Avoids #BLI_smallhash_remove, #BLI_smallhash_insert calls (double lookups)
 *
 * \returns true if a new key has been added.
 */
bool BLI_smallhash_reinsert(SmallHash *sh, uintptr_t key, void *item) ATTR_NONNULL(1);
bool BLI_smallhash_remove(SmallHash *sh, uintptr_t key) ATTR_NONNULL(1);
void *BLI_smallhash_lookup(const SmallHash *sh, uintptr_t key)
    ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;
void **BLI_smallhash_lookup_p(const SmallHash *sh, uintptr_t key)
    ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;
bool BLI_smallhash_haskey(const SmallHash *sh, uintptr_t key) ATTR_NONNULL(1);
int BLI_smallhash_len(const SmallHash *sh) ATTR_NONNULL(1);
void *BLI_smallhash_iternext(SmallHashIter *iter, uintptr_t *key)
    ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;
void **BLI_smallhash_iternext_p(SmallHashIter *iter, uintptr_t *key)
    ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;
void *BLI_smallhash_iternew(const SmallHash *sh, SmallHashIter *iter, uintptr_t *key)
    ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;
void **BLI_smallhash_iternew_p(const SmallHash *sh, SmallHashIter *iter, uintptr_t *key)
    ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;
/* void BLI_smallhash_print(SmallHash *sh); */ /* UNUSED */

#ifdef DEBUG
/**
 * Measure how well the hash function performs
 * (1.0 is perfect - no stepping needed).
 *
 * Smaller is better!
 */
double BLI_smallhash_calc_quality(SmallHash *sh);
#endif

#ifdef __cplusplus
}
#endif
