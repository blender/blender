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
#ifndef __BKE_TREEHASH_H__
#define __BKE_TREEHASH_H__

/** \file BKE_treehash.h
 *  \ingroup bke
 */

struct ID;
struct GHash;
struct BLI_mempool;
struct TreeStoreElem;

/* create and fill hashtable with treestore elements */
void *BKE_treehash_create_from_treestore(struct BLI_mempool *treestore);

/* full rebuild for already allocated hashtable */
void *BKE_treehash_rebuild_from_treestore(void *treehash, struct BLI_mempool *treestore);

/* full rebuild for already allocated hashtable */
void BKE_treehash_add_element(void *treehash, struct TreeStoreElem *elem);

/* find first unused element with specific type, nr and id */
struct TreeStoreElem *BKE_treehash_lookup_unused(void *treehash, short type, short nr, struct ID *id);

/* find user or unused element with specific type, nr and id */
struct TreeStoreElem *BKE_treehash_lookup_any(void *treehash, short type, short nr, struct ID *id);

/* free treehash structure */
void BKE_treehash_free(void *treehash);

#endif
