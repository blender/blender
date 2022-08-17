/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct BLI_mempool;
struct ID;
struct GHash;
struct TreeStoreElem;

/* create and fill hashtable with treestore elements */
GHash *BKE_outliner_treehash_create_from_treestore(BLI_mempool *treestore);

/* full rebuild for already allocated hashtable */
GHash *BKE_outliner_treehash_rebuild_from_treestore(GHash *treehash, BLI_mempool *treestore);

/* clear element usage flags */
void BKE_outliner_treehash_clear_used(GHash *treehash);

/* Add/remove hashtable elements */
void BKE_outliner_treehash_add_element(GHash *treehash, TreeStoreElem *elem);
void BKE_outliner_treehash_remove_element(GHash *treehash, TreeStoreElem *elem);

/* find first unused element with specific type, nr and id */
struct TreeStoreElem *BKE_outliner_treehash_lookup_unused(GHash *treehash,
                                                          short type,
                                                          short nr,
                                                          ID *id);

/* find user or unused element with specific type, nr and id */
struct TreeStoreElem *BKE_outliner_treehash_lookup_any(GHash *treehash,
                                                       short type,
                                                       short nr,
                                                       ID *id);

/* free treehash structure */
void BKE_outliner_treehash_free(GHash *treehash);

#ifdef __cplusplus
}
#endif
