/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "BLI_listbase.h"
#include "BLI_listbase_wrapper.hh"

#include "BKE_main.hh"

#include "DNA_space_types.h"

#include "../outliner_intern.hh"
#include "common.hh"
#include "tree_display.hh"

namespace blender::ed::outliner {

/* Convenience/readability. */
template<typename T> using List = ListBaseWrapper<T>;

TreeDisplayOverrideLibraryProperties::TreeDisplayOverrideLibraryProperties(
    SpaceOutliner &space_outliner)
    : AbstractTreeDisplay(space_outliner)
{
}

ListBase TreeDisplayOverrideLibraryProperties::build_tree(const TreeSourceData &source_data)
{
  ListBase tree = add_library_contents(*source_data.bmain);

  for (TreeElement *top_level_te : List<TreeElement>(tree)) {
    TreeStoreElem *tselem = TREESTORE(top_level_te);
    if (!tselem->used) {
      tselem->flag &= ~TSE_CLOSED;
    }
  }

  return tree;
}

ListBase TreeDisplayOverrideLibraryProperties::add_library_contents(Main &mainvar)
{
  ListBase tree = {nullptr};

  const short filter_id_type = id_filter_get();

  ListBase *lbarray[INDEX_ID_MAX];
  int tot;
  if (filter_id_type) {
    lbarray[0] = which_libbase(&mainvar, space_outliner_.filter_id_type);
    tot = 1;
  }
  else {
    tot = set_listbasepointers(&mainvar, lbarray);
  }

  for (int a = 0; a < tot; a++) {
    if (!lbarray[a] || !lbarray[a]->first) {
      continue;
    }

    ID *id = nullptr;

    /* check if there's data in current id list */
    for (ID *id_iter : List<ID>(lbarray[a])) {
      if (ID_IS_OVERRIDE_LIBRARY_REAL(id_iter) && !ID_IS_LINKED(id_iter)) {
        id = id_iter;
        break;
      }
    }

    if (id == nullptr) {
      continue;
    }

    /* Create data-block list parent element on demand. */
    TreeElement *id_base_te = nullptr;
    ListBase *lb_to_expand = &tree;

    if (!filter_id_type) {
      id_base_te = add_element(&tree, nullptr, lbarray[a], nullptr, TSE_ID_BASE, 0);
      id_base_te->directdata = lbarray[a];
      id_base_te->name = outliner_idcode_to_plural(GS(id->name));

      lb_to_expand = &id_base_te->subtree;
    }

    for (ID *id : List<ID>(lbarray[a])) {
      if (ID_IS_OVERRIDE_LIBRARY_REAL(id) && !ID_IS_LINKED(id)) {
        TreeElement *override_tree_element = add_element(
            lb_to_expand, id, nullptr, id_base_te, TSE_LIBRARY_OVERRIDE_BASE, 0);

        if (BLI_listbase_is_empty(&override_tree_element->subtree)) {
          outliner_free_tree_element(override_tree_element, lb_to_expand);
        }
      }
    }
  }

  /* Remove ID base elements that turn out to be empty. */
  LISTBASE_FOREACH_MUTABLE (TreeElement *, te, &tree) {
    if (BLI_listbase_is_empty(&te->subtree)) {
      outliner_free_tree_element(te, &tree);
    }
  }

  return tree;
}

short TreeDisplayOverrideLibraryProperties::id_filter_get() const
{
  if (space_outliner_.filter & SO_FILTER_ID_TYPE) {
    return space_outliner_.filter_id_type;
  }
  return 0;
}

}  // namespace blender::ed::outliner
