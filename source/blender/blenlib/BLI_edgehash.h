/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * Contributor(s): Daniel Dunbar
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#ifndef __BLI_EDGEHASH_H__
#define __BLI_EDGEHASH_H__

/** \file BLI_edgehash.h
 *  \ingroup bli
 *  \author Daniel Dunbar
 *  \brief A general unordered 2-int pair hash table ADT.
 */

#include "BLI_compiler_attrs.h"

struct EdgeHash;
typedef struct EdgeHash EdgeHash;

typedef struct EdgeHashIterator {
	EdgeHash *eh;
	struct EdgeEntry *curEntry;
	unsigned int curBucket;
} EdgeHashIterator;

typedef void (*EdgeHashFreeFP)(void *key);

enum {
	EDGEHASH_FLAG_ALLOW_DUPES = (1 << 0),  /* only checked for in debug mode */
};

EdgeHash       *BLI_edgehash_new_ex(const char *info,
                                    const unsigned int nentries_reserve);
EdgeHash       *BLI_edgehash_new(const char *info) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
void            BLI_edgehash_free(EdgeHash *eh, EdgeHashFreeFP valfreefp);
void            BLI_edgehash_insert(EdgeHash *eh, unsigned int v0, unsigned int v1, void *val);
bool            BLI_edgehash_reinsert(EdgeHash *eh, unsigned int v0, unsigned int v1, void *val);
void           *BLI_edgehash_lookup(EdgeHash *eh, unsigned int v0, unsigned int v1) ATTR_WARN_UNUSED_RESULT;
void           *BLI_edgehash_lookup_default(EdgeHash *eh, unsigned int v0, unsigned int v1, void *val_default) ATTR_WARN_UNUSED_RESULT;
void          **BLI_edgehash_lookup_p(EdgeHash *eh, unsigned int v0, unsigned int v1) ATTR_WARN_UNUSED_RESULT;
bool            BLI_edgehash_haskey(EdgeHash *eh, unsigned int v0, unsigned int v1) ATTR_WARN_UNUSED_RESULT;
int             BLI_edgehash_size(EdgeHash *eh) ATTR_WARN_UNUSED_RESULT;
void            BLI_edgehash_clear_ex(EdgeHash *eh, EdgeHashFreeFP valfreefp,
                                      const unsigned int nentries_reserve);
void            BLI_edgehash_clear(EdgeHash *eh, EdgeHashFreeFP valfreefp);
void            BLI_edgehash_flag_set(EdgeHash *eh, unsigned int flag);
void            BLI_edgehash_flag_clear(EdgeHash *eh, unsigned int flag);

EdgeHashIterator   *BLI_edgehashIterator_new(EdgeHash *eh) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
void                BLI_edgehashIterator_init(EdgeHashIterator *ehi, EdgeHash *eh);
void                BLI_edgehashIterator_free(EdgeHashIterator *ehi);
void                BLI_edgehashIterator_step(EdgeHashIterator *ehi);

BLI_INLINE bool   BLI_edgehashIterator_isDone(EdgeHashIterator *ehi) ATTR_WARN_UNUSED_RESULT;
BLI_INLINE void   BLI_edgehashIterator_getKey(EdgeHashIterator *ehi, unsigned int *r_v0, unsigned int *r_v1);
BLI_INLINE void  *BLI_edgehashIterator_getValue(EdgeHashIterator *ehi) ATTR_WARN_UNUSED_RESULT;
BLI_INLINE void **BLI_edgehashIterator_getValue_p(EdgeHashIterator *ehi) ATTR_WARN_UNUSED_RESULT;
BLI_INLINE void   BLI_edgehashIterator_setValue(EdgeHashIterator *ehi, void *val);

struct _eh_Entry { void *next; unsigned int v0, v1; void *val; };
BLI_INLINE void   BLI_edgehashIterator_getKey(EdgeHashIterator *ehi, unsigned int *r_v0, unsigned int *r_v1)
{ *r_v0 = ((struct _eh_Entry *)ehi->curEntry)->v0; *r_v1 = ((struct _eh_Entry *)ehi->curEntry)->v1; }
BLI_INLINE void  *BLI_edgehashIterator_getValue(EdgeHashIterator *ehi) { return ((struct _eh_Entry *)ehi->curEntry)->val; }
BLI_INLINE void **BLI_edgehashIterator_getValue_p(EdgeHashIterator *ehi) { return &((struct _eh_Entry *)ehi->curEntry)->val; }
BLI_INLINE void   BLI_edgehashIterator_setValue(EdgeHashIterator *ehi, void *val) { ((struct _eh_Entry *)ehi->curEntry)->val = val; }
BLI_INLINE bool   BLI_edgehashIterator_isDone(EdgeHashIterator *ehi) { return (((struct _eh_Entry *)ehi->curEntry) == NULL); }
/* disallow further access */
#ifdef __GNUC__
#  pragma GCC poison _eh_Entry
#else
#  define _eh_Entry void
#endif

#define BLI_EDGEHASH_SIZE_GUESS_FROM_LOOPS(totloop)  ((totloop) / 2)
#define BLI_EDGEHASH_SIZE_GUESS_FROM_POLYS(totpoly)  ((totpoly) * 2)

/* *** EdgeSet *** */

struct EdgeSet;
struct EdgeSetIterator;
typedef struct EdgeSet EdgeSet;
typedef struct EdgeSetIterator EdgeSetIterator;

EdgeSet *BLI_edgeset_new_ex(const char *info,
                            const unsigned int nentries_reserve) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
EdgeSet *BLI_edgeset_new(const char *info) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
int      BLI_edgeset_size(EdgeSet *es) ATTR_WARN_UNUSED_RESULT;
bool     BLI_edgeset_add(EdgeSet *es, unsigned int v0, unsigned int v1);
void     BLI_edgeset_insert(EdgeSet *es, unsigned int v0, unsigned int v1);
bool     BLI_edgeset_haskey(EdgeSet *eh, unsigned int v0, unsigned int v1) ATTR_WARN_UNUSED_RESULT;
void     BLI_edgeset_free(EdgeSet *es);
void     BLI_edgeset_flag_set(EdgeSet *es, unsigned int flag);
void     BLI_edgeset_flag_clear(EdgeSet *es, unsigned int flag);

/* rely on inline api for now */
BLI_INLINE EdgeSetIterator *BLI_edgesetIterator_new(EdgeSet *gs) { return (EdgeSetIterator *)BLI_edgehashIterator_new((EdgeHash *)gs); }
BLI_INLINE void BLI_edgesetIterator_free(EdgeSetIterator *esi) { BLI_edgehashIterator_free((EdgeHashIterator *)esi); }
BLI_INLINE void BLI_edgesetIterator_getKey(EdgeSetIterator *esi, unsigned int *r_v0, unsigned int *r_v1) { BLI_edgehashIterator_getKey((EdgeHashIterator *)esi, r_v0, r_v1); }
BLI_INLINE void BLI_edgesetIterator_step(EdgeSetIterator *esi) { BLI_edgehashIterator_step((EdgeHashIterator *)esi); }
BLI_INLINE bool BLI_edgesetIterator_isDone(EdgeSetIterator *esi) { return BLI_edgehashIterator_isDone((EdgeHashIterator *)esi); }

#ifdef DEBUG
double          BLI_edgehash_calc_quality(EdgeHash *eh);
double          BLI_edgeset_calc_quality(EdgeSet *es);
#endif

#endif  /* __BLI_EDGEHASH_H__ */
