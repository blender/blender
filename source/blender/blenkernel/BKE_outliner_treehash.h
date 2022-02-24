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
struct TreeStoreElem;

/* create and fill hashtable with treestore elements */
void *BKE_outliner_treehash_create_from_treestore(struct BLI_mempool *treestore);

/* full rebuild for already allocated hashtable */
void *BKE_outliner_treehash_rebuild_from_treestore(void *treehash, struct BLI_mempool *treestore);

/* clear element usage flags */
void BKE_outliner_treehash_clear_used(void *treehash);

/* Add/remove hashtable elements */
void BKE_outliner_treehash_add_element(void *treehash, struct TreeStoreElem *elem);
void BKE_outliner_treehash_remove_element(void *treehash, struct TreeStoreElem *elem);

/* find first unused element with specific type, nr and id */
struct TreeStoreElem *BKE_outliner_treehash_lookup_unused(void *treehash,
                                                          short type,
                                                          short nr,
                                                          struct ID *id);

/* find user or unused element with specific type, nr and id */
struct TreeStoreElem *BKE_outliner_treehash_lookup_any(void *treehash,
                                                       short type,
                                                       short nr,
                                                       struct ID *id);

/* free treehash structure */
void BKE_outliner_treehash_free(void *treehash);

#ifdef __cplusplus
}
#endif
