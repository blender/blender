/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 *
 * Hash table of tree-store elements (#TreeStoreElem) for fast lookups via a (id, type, index)
 * tuple as key.
 *
 * The Outliner may have to perform many lookups for rebuilding complex trees, so this should be
 * treated as performance sensitive.
 */

#include <memory>

struct BLI_mempool;
struct ID;
struct GHash;
struct TreeStoreElem;

namespace blender::bke::outliner::treehash {

class TreeHash {
  GHash *treehash_ = nullptr;

 public:
  ~TreeHash();

  /* create and fill hashtable with treestore elements */
  static std::unique_ptr<TreeHash> create_from_treestore(BLI_mempool &treestore);

  /* full rebuild for already allocated hashtable */
  void rebuild_from_treestore(BLI_mempool &treestore);

  /* clear element usage flags */
  void clear_used();

  /* Add/remove hashtable elements */
  void add_element(TreeStoreElem &elem);
  void remove_element(TreeStoreElem &elem);

  /* find first unused element with specific type, nr and id */
  TreeStoreElem *lookup_unused(short type, short nr, ID *id) const;

  /* find user or unused element with specific type, nr and id */
  TreeStoreElem *lookup_any(short type, short nr, ID *id) const;

 private:
  TreeHash() = default;

  void fill_treehash(BLI_mempool &treestore);
};

}  // namespace blender::bke::outliner::treehash
