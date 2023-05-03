/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_compiler_attrs.h"
#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

struct EdgeHash;
typedef struct EdgeHash EdgeHash;

struct _EdgeHash_Edge {
  uint v_low, v_high;
};

struct _EdgeHash_Entry {
  struct _EdgeHash_Edge edge;
  void *value;
};

typedef struct EdgeHashIterator {
  struct _EdgeHash_Entry *entries;
  uint length;
  uint index;
} EdgeHashIterator;

typedef void (*EdgeHashFreeFP)(void *key);

enum {
  /**
   * Only checked for in debug mode.
   */
  EDGEHASH_FLAG_ALLOW_DUPES = (1 << 0),
};

EdgeHash *BLI_edgehash_new_ex(const char *info, unsigned int nentries_reserve);
EdgeHash *BLI_edgehash_new(const char *info) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
void BLI_edgehash_free(EdgeHash *eh, EdgeHashFreeFP free_value);
void BLI_edgehash_print(EdgeHash *eh);
/**
 * Insert edge (\a v0, \a v1) into hash with given value, does
 * not check for duplicates.
 */
void BLI_edgehash_insert(EdgeHash *eh, unsigned int v0, unsigned int v1, void *val);
/**
 * Assign a new value to a key that may already be in edgehash.
 */
bool BLI_edgehash_reinsert(EdgeHash *eh, unsigned int v0, unsigned int v1, void *val);
/**
 * Return value for given edge (\a v0, \a v1), or NULL if
 * if key does not exist in hash. (If need exists
 * to differentiate between key-value being NULL and
 * lack of key then see #BLI_edgehash_lookup_p().
 */
void *BLI_edgehash_lookup(const EdgeHash *eh,
                          unsigned int v0,
                          unsigned int v1) ATTR_WARN_UNUSED_RESULT;
/**
 * A version of #BLI_edgehash_lookup which accepts a fallback argument.
 */
void *BLI_edgehash_lookup_default(const EdgeHash *eh,
                                  unsigned int v0,
                                  unsigned int v1,
                                  void *default_value) ATTR_WARN_UNUSED_RESULT;
/**
 * Return pointer to value for given edge (\a v0, \a v1),
 * or NULL if key does not exist in hash.
 */
void **BLI_edgehash_lookup_p(EdgeHash *eh,
                             unsigned int v0,
                             unsigned int v1) ATTR_WARN_UNUSED_RESULT;
/**
 * Ensure \a (v0, v1) is exists in \a eh.
 *
 * This handles the common situation where the caller needs ensure a key is added to \a eh,
 * constructing a new value in the case the key isn't found.
 * Otherwise use the existing value.
 *
 * Such situations typically incur multiple lookups, however this function
 * avoids them by ensuring the key is added,
 * returning a pointer to the value so it can be used or initialized by the caller.
 *
 * \return true when the value didn't need to be added.
 * (when false, the caller _must_ initialize the value).
 */
bool BLI_edgehash_ensure_p(EdgeHash *eh, unsigned int v0, unsigned int v1, void ***r_val)
    ATTR_WARN_UNUSED_RESULT;
/**
 * Remove \a key (v0, v1) from \a eh, or return false if the key wasn't found.
 *
 * \param v0, v1: The key to remove.
 * \param free_value: Optional callback to free the value.
 * \return true if \a key was removed from \a eh.
 */
bool BLI_edgehash_remove(EdgeHash *eh,
                         unsigned int v0,
                         unsigned int v1,
                         EdgeHashFreeFP free_value);

/**
 * Remove \a key (v0, v1) from \a eh, returning the value or NULL if the key wasn't found.
 *
 * \param v0, v1: The key to remove.
 * \return the value of \a key int \a eh or NULL.
 */
void *BLI_edgehash_popkey(EdgeHash *eh, unsigned int v0, unsigned int v1) ATTR_WARN_UNUSED_RESULT;
/**
 * Return boolean true/false if edge (v0,v1) in hash.
 */
bool BLI_edgehash_haskey(const EdgeHash *eh,
                         unsigned int v0,
                         unsigned int v1) ATTR_WARN_UNUSED_RESULT;
/**
 * Return number of keys in hash.
 */
int BLI_edgehash_len(const EdgeHash *eh) ATTR_WARN_UNUSED_RESULT;
/**
 * Remove all edges from hash.
 */
void BLI_edgehash_clear_ex(EdgeHash *eh, EdgeHashFreeFP free_value, uint reserve);
/**
 * Wraps #BLI_edgehash_clear_ex with zero entries reserved.
 */
void BLI_edgehash_clear(EdgeHash *eh, EdgeHashFreeFP free_value);

/**
 * Create a new #EdgeHashIterator. The hash table must not be mutated
 * while the iterator is in use, and the iterator will step exactly
 * #BLI_edgehash_len(eh) times before becoming done.
 */
EdgeHashIterator *BLI_edgehashIterator_new(EdgeHash *eh) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
/**
 * Initialize an already allocated #EdgeHashIterator. The hash table must not
 * be mutated while the iterator is in use, and the iterator will
 * step exactly BLI_edgehash_len(eh) times before becoming done.
 *
 * \param ehi: The #EdgeHashIterator to initialize.
 * \param eh: The #EdgeHash to iterate over.
 */
void BLI_edgehashIterator_init(EdgeHashIterator *ehi, EdgeHash *eh);
/**
 * Free an #EdgeHashIterator.
 */
void BLI_edgehashIterator_free(EdgeHashIterator *ehi);

BLI_INLINE void BLI_edgehashIterator_step(EdgeHashIterator *ehi)
{
  ehi->index++;
}
BLI_INLINE bool BLI_edgehashIterator_isDone(const EdgeHashIterator *ehi)
{
  return ehi->index >= ehi->length;
}
BLI_INLINE void BLI_edgehashIterator_getKey(EdgeHashIterator *ehi, int *r_v0, int *r_v1)
{
  struct _EdgeHash_Edge edge = ehi->entries[ehi->index].edge;
  *r_v0 = edge.v_low;
  *r_v1 = edge.v_high;
}
BLI_INLINE void *BLI_edgehashIterator_getValue(EdgeHashIterator *ehi)
{
  return ehi->entries[ehi->index].value;
}
BLI_INLINE void **BLI_edgehashIterator_getValue_p(EdgeHashIterator *ehi)
{
  return &ehi->entries[ehi->index].value;
}
BLI_INLINE void BLI_edgehashIterator_setValue(EdgeHashIterator *ehi, void *val)
{
  ehi->entries[ehi->index].value = val;
}

#define BLI_EDGEHASH_SIZE_GUESS_FROM_LOOPS(totloop) ((totloop) / 2)
#define BLI_EDGEHASH_SIZE_GUESS_FROM_POLYS(totpoly) ((totpoly)*2)

/* *** EdgeSet *** */

struct EdgeSet;
typedef struct EdgeSet EdgeSet;

typedef struct EdgeSetIterator {
  struct _EdgeHash_Edge *edges;
  uint length;
  uint index;
} EdgeSetIterator;

EdgeSet *BLI_edgeset_new_ex(const char *info,
                            unsigned int nentries_reserve) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
EdgeSet *BLI_edgeset_new(const char *info) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
int BLI_edgeset_len(const EdgeSet *es) ATTR_WARN_UNUSED_RESULT;
/**
 * A version of BLI_edgeset_insert which checks first if the key is in the set.
 * \returns true if a new key has been added.
 *
 * \note #EdgeHash has no equivalent to this because typically the value would be different.
 */
bool BLI_edgeset_add(EdgeSet *es, unsigned int v0, unsigned int v1);
/**
 * Adds the key to the set (no checks for unique keys!).
 * Matching #BLI_edgehash_insert
 */
void BLI_edgeset_insert(EdgeSet *es, unsigned int v0, unsigned int v1);
bool BLI_edgeset_haskey(const EdgeSet *es,
                        unsigned int v0,
                        unsigned int v1) ATTR_WARN_UNUSED_RESULT;
void BLI_edgeset_free(EdgeSet *es);

/* rely on inline api for now */

EdgeSetIterator *BLI_edgesetIterator_new(EdgeSet *es);
void BLI_edgesetIterator_free(EdgeSetIterator *esi);

BLI_INLINE void BLI_edgesetIterator_getKey(EdgeSetIterator *esi, int *r_v0, int *r_v1)
{
  struct _EdgeHash_Edge edge = esi->edges[esi->index];
  *r_v0 = edge.v_low;
  *r_v1 = edge.v_high;
}
BLI_INLINE void BLI_edgesetIterator_step(EdgeSetIterator *esi)
{
  esi->index++;
}
BLI_INLINE bool BLI_edgesetIterator_isDone(const EdgeSetIterator *esi)
{
  return esi->index >= esi->length;
}

#ifdef __cplusplus
}
#endif
