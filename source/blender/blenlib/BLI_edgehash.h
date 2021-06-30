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
 */

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
  EDGEHASH_FLAG_ALLOW_DUPES = (1 << 0), /* only checked for in debug mode */
};

EdgeHash *BLI_edgehash_new_ex(const char *info, const unsigned int nentries_reserve);
EdgeHash *BLI_edgehash_new(const char *info) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
void BLI_edgehash_free(EdgeHash *eh, EdgeHashFreeFP free_value);
void BLI_edgehash_print(EdgeHash *eh);
void BLI_edgehash_insert(EdgeHash *eh, unsigned int v0, unsigned int v1, void *val);
bool BLI_edgehash_reinsert(EdgeHash *eh, unsigned int v0, unsigned int v1, void *val);
void *BLI_edgehash_lookup(const EdgeHash *eh,
                          unsigned int v0,
                          unsigned int v1) ATTR_WARN_UNUSED_RESULT;
void *BLI_edgehash_lookup_default(const EdgeHash *eh,
                                  unsigned int v0,
                                  unsigned int v1,
                                  void *default_value) ATTR_WARN_UNUSED_RESULT;
void **BLI_edgehash_lookup_p(EdgeHash *eh,
                             unsigned int v0,
                             unsigned int v1) ATTR_WARN_UNUSED_RESULT;
bool BLI_edgehash_ensure_p(EdgeHash *eh, unsigned int v0, unsigned int v1, void ***r_val)
    ATTR_WARN_UNUSED_RESULT;
bool BLI_edgehash_remove(EdgeHash *eh,
                         unsigned int v0,
                         unsigned int v1,
                         EdgeHashFreeFP free_value);

void *BLI_edgehash_popkey(EdgeHash *eh, unsigned int v0, unsigned int v1) ATTR_WARN_UNUSED_RESULT;
bool BLI_edgehash_haskey(const EdgeHash *eh,
                         unsigned int v0,
                         unsigned int v1) ATTR_WARN_UNUSED_RESULT;
int BLI_edgehash_len(const EdgeHash *eh) ATTR_WARN_UNUSED_RESULT;
void BLI_edgehash_clear_ex(EdgeHash *eh, EdgeHashFreeFP free_value, const uint reserve);
void BLI_edgehash_clear(EdgeHash *eh, EdgeHashFreeFP free_value);

EdgeHashIterator *BLI_edgehashIterator_new(EdgeHash *eh) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
void BLI_edgehashIterator_init(EdgeHashIterator *ehi, EdgeHash *eh);
void BLI_edgehashIterator_free(EdgeHashIterator *ehi);

BLI_INLINE void BLI_edgehashIterator_step(EdgeHashIterator *ehi)
{
  ehi->index++;
}
BLI_INLINE bool BLI_edgehashIterator_isDone(const EdgeHashIterator *ehi)
{
  return ehi->index >= ehi->length;
}
BLI_INLINE void BLI_edgehashIterator_getKey(EdgeHashIterator *ehi,
                                            unsigned int *r_v0,
                                            unsigned int *r_v1)
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

EdgeSet *BLI_edgeset_new_ex(const char *info, const unsigned int nentries_reserve)
    ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
EdgeSet *BLI_edgeset_new(const char *info) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
int BLI_edgeset_len(const EdgeSet *es) ATTR_WARN_UNUSED_RESULT;
bool BLI_edgeset_add(EdgeSet *es, unsigned int v0, unsigned int v1);
void BLI_edgeset_insert(EdgeSet *es, unsigned int v0, unsigned int v1);
bool BLI_edgeset_haskey(const EdgeSet *es,
                        unsigned int v0,
                        unsigned int v1) ATTR_WARN_UNUSED_RESULT;
void BLI_edgeset_free(EdgeSet *es);

/* rely on inline api for now */
EdgeSetIterator *BLI_edgesetIterator_new(EdgeSet *es);
void BLI_edgesetIterator_free(EdgeSetIterator *esi);

BLI_INLINE void BLI_edgesetIterator_getKey(EdgeSetIterator *esi,
                                           unsigned int *r_v0,
                                           unsigned int *r_v1)
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
