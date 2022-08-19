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

  /** Create and fill hash-table with treestore elements */
  static std::unique_ptr<TreeHash> create_from_treestore(BLI_mempool &treestore);

  /** Full rebuild for already allocated hash-table. */
  void rebuild_from_treestore(BLI_mempool &treestore);

  /** Clear element usage flags. */
  void clear_used();

  /** Add hash-table element. */
  void add_element(TreeStoreElem &elem);
  /** Remove hash-table element. */
  void remove_element(TreeStoreElem &elem);

  /** Find first unused element with specific type, nr and id. */
  TreeStoreElem *lookup_unused(short type, short nr, ID *id) const;

  /** Find user or unused element with specific type, nr and id. */
  TreeStoreElem *lookup_any(short type, short nr, ID *id) const;

 private:
  TreeHash() = default;

  void fill_treehash(BLI_mempool &treestore);
};

}  // namespace blender::bke::outliner::treehash
