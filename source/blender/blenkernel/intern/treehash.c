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
 * Contributor(s): Blender Foundation 2013
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file treehash.c
 *  \ingroup bke
 *
 * Tree hash for the outliner space.
 */

#include <stdlib.h>

#include "BKE_treehash.h"

#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_mempool.h"

#include "DNA_outliner_types.h"

#include "MEM_guardedalloc.h"

typedef struct TseGroup
{
	TreeStoreElem **elems;
	int size;
	int allocated;
} TseGroup;

/* Allocate structure for TreeStoreElements;
 * Most of elements in treestore have no duplicates,
 * so there is no need to preallocate memory for more than one pointer */
static TseGroup *tse_group_create(void)
{
	TseGroup *tse_group = MEM_mallocN(sizeof(TseGroup), "TseGroup");
	tse_group->elems = MEM_mallocN(sizeof(TreeStoreElem *), "TseGroupElems");
	tse_group->size = 0;
	tse_group->allocated = 1;
	return tse_group;
}

static void tse_group_add(TseGroup *tse_group, TreeStoreElem *elem)
{
	if (UNLIKELY(tse_group->size == tse_group->allocated)) {
		tse_group->allocated *= 2;
		tse_group->elems = MEM_reallocN(tse_group->elems, sizeof(TreeStoreElem *) * tse_group->allocated);
	}
	tse_group->elems[tse_group->size] = elem;
	tse_group->size++;
}

static void tse_group_free(TseGroup *tse_group)
{
	MEM_freeN(tse_group->elems);
	MEM_freeN(tse_group);
}

static unsigned int tse_hash(const void *ptr)
{
	const TreeStoreElem *tse = ptr;
	union {
		short        h_pair[2];
		unsigned int u_int;
	} hash;

	BLI_assert(tse->type || !tse->nr);

	hash.h_pair[0] = tse->type;
	hash.h_pair[1] = tse->nr;

	hash.u_int ^= BLI_ghashutil_ptrhash(tse->id);

	return hash.u_int;
}

static bool tse_cmp(const void *a, const void *b)
{
	const TreeStoreElem *tse_a = a;
	const TreeStoreElem *tse_b = b;
	return tse_a->type != tse_b->type || tse_a->nr != tse_b->nr || tse_a->id != tse_b->id;
}

static void fill_treehash(void *treehash, BLI_mempool *treestore)
{
	TreeStoreElem *tselem;
	BLI_mempool_iter iter;
	BLI_mempool_iternew(treestore, &iter);
	while ((tselem = BLI_mempool_iterstep(&iter))) {
		BKE_treehash_add_element(treehash, tselem);
	}
}

void *BKE_treehash_create_from_treestore(BLI_mempool *treestore)
{
	GHash *treehash = BLI_ghash_new_ex(tse_hash, tse_cmp, "treehash", BLI_mempool_count(treestore));
	fill_treehash(treehash, treestore);
	return treehash;
}

static void free_treehash_group(void *key)
{
	tse_group_free(key);
}

void *BKE_treehash_rebuild_from_treestore(void *treehash, BLI_mempool *treestore)
{
	BLI_ghash_clear_ex(treehash, NULL, free_treehash_group, BLI_mempool_count(treestore));
	fill_treehash(treehash, treestore);
	return treehash;
}

void BKE_treehash_add_element(void *treehash, TreeStoreElem *elem)
{
	TseGroup *group = BLI_ghash_lookup(treehash, elem);
	if (!group) {
		group = tse_group_create();
		BLI_ghash_insert(treehash, elem, group);
	}
	tse_group_add(group, elem);
}

static TseGroup *BKE_treehash_lookup_group(GHash *th, short type, short nr, struct ID *id)
{
	TreeStoreElem tse_template;
	tse_template.type = type;
	tse_template.nr = type ? nr : 0;  // we're picky! :)
	tse_template.id = id;
	return BLI_ghash_lookup(th, &tse_template);
}

TreeStoreElem *BKE_treehash_lookup_unused(void *treehash, short type, short nr, struct ID *id)
{
	TseGroup *group = BKE_treehash_lookup_group(treehash, type, nr, id);
	if (group) {
		int i;
		for (i = 0; i < group->size; i++) {
			if (!group->elems[i]->used) {
				return group->elems[i];
			}
		}
	}
	return NULL;
}

TreeStoreElem *BKE_treehash_lookup_any(void *treehash, short type, short nr, struct ID *id)
{
	TseGroup *group = BKE_treehash_lookup_group(treehash, type, nr, id);
	return group ? group->elems[0] : NULL;
}

void BKE_treehash_free(void *treehash)
{
	BLI_ghash_free(treehash, NULL, free_treehash_group);
}
