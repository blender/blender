/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_ID.h"
#include "DNA_collection_types.h"
#include "DNA_key_types.h"
#include "DNA_space_types.h"

#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_set.hh"

#include "BLT_translation.h"

#include "BKE_collection.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"

#include "../outliner_intern.hh"
#include "common.hh"
#include "tree_display.hh"
#include "tree_element_id.hh"

namespace blender::ed::outliner {

class AbstractTreeElement;

TreeDisplayOverrideLibraryHierarchies::TreeDisplayOverrideLibraryHierarchies(
    SpaceOutliner &space_outliner)
    : AbstractTreeDisplay(space_outliner)
{
}

/* XXX Remove expanded subtree, we add our own items here. Expanding should probably be
 * optional. */
static void remove_expanded_children(TreeElement &te)
{
  outliner_free_tree(&te.subtree);
}

ListBase TreeDisplayOverrideLibraryHierarchies::buildTree(const TreeSourceData &source_data)
{
  ListBase tree = {nullptr};

  /* First step: Build "Current File" hierarchy. */
  TreeElement *current_file_te = outliner_add_element(
      &space_outliner_, &tree, source_data.bmain, nullptr, TSE_ID_BASE, -1);
  current_file_te->name = IFACE_("Current File");
  {
    AbstractTreeElement::uncollapse_by_default(current_file_te);
    build_hierarchy_for_lib_or_main(source_data.bmain, *current_file_te);

    /* Add dummy child if there's nothing to display. */
    if (BLI_listbase_is_empty(&current_file_te->subtree)) {
      TreeElement *dummy_te = outliner_add_element(
          &space_outliner_, &current_file_te->subtree, nullptr, current_file_te, TSE_ID_BASE, 0);
      dummy_te->name = IFACE_("No Library Overrides");
    }
  }

  /* Second step: Build hierarchies for external libraries. */
  for (Library *lib = (Library *)source_data.bmain->libraries.first; lib;
       lib = (Library *)lib->id.next) {
    TreeElement *tenlib = outliner_add_element(
        &space_outliner_, &tree, lib, nullptr, TSE_SOME_ID, 0);
    build_hierarchy_for_lib_or_main(source_data.bmain, *tenlib, lib);
  }

  /* Remove top level library elements again that don't contain any overrides. */
  LISTBASE_FOREACH_MUTABLE (TreeElement *, top_level_te, &tree) {
    if (top_level_te == current_file_te) {
      continue;
    }

    if (BLI_listbase_is_empty(&top_level_te->subtree)) {
      outliner_free_tree_element(top_level_te, &tree);
    }
  }

  return tree;
}

ListBase TreeDisplayOverrideLibraryHierarchies::build_hierarchy_for_lib_or_main(
    Main *bmain, TreeElement &parent_te, Library *lib)
{
  ListBase tree = {nullptr};

  /* Keep track over which ID base elements were already added, and expand them once added. */
  Map<ID_Type, TreeElement *> id_base_te_map;
  /* Index for the ID base elements ("Objects", "Materials", etc). */
  int base_index = 0;

  ID *iter_id;
  FOREACH_MAIN_ID_BEGIN (bmain, iter_id) {
    if (!ID_IS_OVERRIDE_LIBRARY_REAL(iter_id) || !ID_IS_OVERRIDE_LIBRARY_HIERARCHY_ROOT(iter_id)) {
      continue;
    }
    if (iter_id->lib != lib) {
      continue;
    }

    TreeElement *new_base_te = id_base_te_map.lookup_or_add_cb(GS(iter_id->name), [&]() {
      TreeElement *new_te = outliner_add_element(&space_outliner_,
                                                 &parent_te.subtree,
                                                 lib ? (void *)lib : bmain,
                                                 &parent_te,
                                                 TSE_ID_BASE,
                                                 base_index++);
      new_te->name = outliner_idcode_to_plural(GS(iter_id->name));
      return new_te;
    });

    TreeElement *new_id_te = outliner_add_element(
        &space_outliner_, &new_base_te->subtree, iter_id, new_base_te, TSE_SOME_ID, 0);
    remove_expanded_children(*new_id_te);

    build_hierarchy_for_ID(bmain, *iter_id, *tree_element_cast<TreeElementID>(new_id_te));
  }
  FOREACH_MAIN_ID_END;

  return tree;
}

struct BuildHierarchyForeachIDCbData {
  /* Don't allow copies, the sets below would need deep copying. */
  BuildHierarchyForeachIDCbData(const BuildHierarchyForeachIDCbData &) = delete;

  Main &bmain;
  SpaceOutliner &space_outliner;
  ID &override_root_id;

  /* The tree element to expand. Changes with every level of recursion. */
  TreeElementID *parent_te;
  /* The ancestor IDs leading to the current ID, to avoid IDs recursing into themselves. Changes
   * with every level of recursion. */
  Set<ID *> parent_ids{};
  /* The IDs that were already added to #parent_te, to avoid duplicates. Entirely new set with
   * every level of recursion. */
  Set<ID *> sibling_ids{};
};

static int build_hierarchy_foreach_ID_cb(LibraryIDLinkCallbackData *cb_data)
{
  if (!*cb_data->id_pointer) {
    return IDWALK_RET_NOP;
  }

  BuildHierarchyForeachIDCbData &build_data = *reinterpret_cast<BuildHierarchyForeachIDCbData *>(
      cb_data->user_data);
  /* Note that this may be an embedded ID (see #real_override_id). */
  ID &id = **cb_data->id_pointer;
  /* If #id is an embedded ID, this will be set to the owner, which is a real ID and contains the
   * override data. So queries of override data should be done via this, but the actual tree
   * element we add is the embedded ID. */
  const ID *real_override_id = &id;

  if (ID_IS_OVERRIDE_LIBRARY_VIRTUAL(&id)) {
    if (GS(id.name) == ID_KE) {
      Key *key = (Key *)&id;
      real_override_id = key->from;
    }
    else if (id.flag & LIB_EMBEDDED_DATA) {
      /* TODO Needs double-checking if this handles all embedded IDs correctly. */
      real_override_id = cb_data->id_owner;
    }
  }

  if (!ID_IS_OVERRIDE_LIBRARY(real_override_id)) {
    return IDWALK_RET_NOP;
  }
  /* Is this ID part of the same override hierarchy? */
  if (real_override_id->override_library->hierarchy_root != &build_data.override_root_id) {
    return IDWALK_RET_NOP;
  }

  /* Avoid endless recursion: If there is an ancestor for this ID already, it recurses into itself.
   */
  if (build_data.parent_ids.lookup_key_default(&id, nullptr)) {
    return IDWALK_RET_NOP;
  }

  /* Avoid duplicates: If there is a sibling for this ID already, the same ID is just used multiple
   * times by the same parent. */
  if (build_data.sibling_ids.lookup_key_default(&id, nullptr)) {
    return IDWALK_RET_NOP;
  }

  TreeElement *new_te = outliner_add_element(&build_data.space_outliner,
                                             &build_data.parent_te->getLegacyElement().subtree,
                                             &id,
                                             &build_data.parent_te->getLegacyElement(),
                                             TSE_SOME_ID,
                                             0);
  remove_expanded_children(*new_te);
  build_data.sibling_ids.add(&id);

  BuildHierarchyForeachIDCbData child_build_data{build_data.bmain,
                                                 build_data.space_outliner,
                                                 build_data.override_root_id,
                                                 tree_element_cast<TreeElementID>(new_te)};
  child_build_data.parent_ids = build_data.parent_ids;
  child_build_data.parent_ids.add(&id);
  child_build_data.sibling_ids.reserve(10);
  BKE_library_foreach_ID_link(
      &build_data.bmain, &id, build_hierarchy_foreach_ID_cb, &child_build_data, IDWALK_READONLY);

  return IDWALK_RET_NOP;
}

void TreeDisplayOverrideLibraryHierarchies::build_hierarchy_for_ID(Main *bmain,
                                                                   ID &override_root_id,
                                                                   TreeElementID &te_id) const
{
  BuildHierarchyForeachIDCbData build_data{*bmain, space_outliner_, override_root_id, &te_id};
  build_data.parent_ids.add(&override_root_id);

  BKE_library_foreach_ID_link(
      bmain, &te_id.get_ID(), build_hierarchy_foreach_ID_cb, &build_data, IDWALK_READONLY);
}

}  // namespace blender::ed::outliner
