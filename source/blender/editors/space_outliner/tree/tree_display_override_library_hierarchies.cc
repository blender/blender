/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_key_types.h"
#include "DNA_space_types.h"

#include "BLI_function_ref.hh"
#include "BLI_ghash.h"
#include "BLI_map.hh"

#include "BLI_set.hh"

#include "BLT_translation.h"

#include "BKE_lib_override.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"

#include "../outliner_intern.hh"
#include "common.hh"
#include "tree_display.hh"

namespace blender::ed::outliner {

class AbstractTreeElement;

TreeDisplayOverrideLibraryHierarchies::TreeDisplayOverrideLibraryHierarchies(
    SpaceOutliner &space_outliner)
    : AbstractTreeDisplay(space_outliner)
{
}

ListBase TreeDisplayOverrideLibraryHierarchies::buildTree(const TreeSourceData &source_data)
{
  ListBase tree = {nullptr};

  /* First step: Build "Current File" hierarchy. */
  TreeElement *current_file_te = outliner_add_element(
      &space_outliner_, &tree, source_data.bmain, nullptr, TSE_ID_BASE, -1);
  current_file_te->name = IFACE_("Current File");
  AbstractTreeElement::uncollapse_by_default(current_file_te);
  {
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
       lib = (Library *)lib->id.next)
  {
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

bool TreeDisplayOverrideLibraryHierarchies::is_lazy_built() const
{
  return true;
}

/* -------------------------------------------------------------------- */
/** \name Library override hierarchy building
 * \{ */

class OverrideIDHierarchyBuilder {
  SpaceOutliner &space_outliner_;
  Main &bmain_;
  MainIDRelations &id_relations_;

  struct HierarchyBuildData {
    const ID &override_root_id_;
    /* The ancestor IDs leading to the current ID, to avoid IDs recursing into themselves. Changes
     * with every level of recursion. */
    Set<const ID *> parent_ids{};
    /* The IDs that were already added to #parent_te, to avoid duplicates. Entirely new set with
     * every level of recursion. */
    Set<const ID *> sibling_ids{};
  };

 public:
  OverrideIDHierarchyBuilder(SpaceOutliner &space_outliner,
                             Main &bmain,
                             MainIDRelations &id_relations)
      : space_outliner_(space_outliner), bmain_(bmain), id_relations_(id_relations)
  {
  }

  void build_hierarchy_for_ID(ID &root_id, TreeElement &te_to_expand);

 private:
  void build_hierarchy_for_ID_recursive(const ID &parent_id,
                                        HierarchyBuildData &build_data,
                                        TreeElement &te_to_expand);
};

ListBase TreeDisplayOverrideLibraryHierarchies::build_hierarchy_for_lib_or_main(
    Main *bmain, TreeElement &parent_te, Library *lib)
{
  ListBase tree = {nullptr};

  /* Ensure #Main.relations contains the latest mapping of relations. Must be freed before
   * returning. */
  BKE_main_relations_create(bmain, 0);

  OverrideIDHierarchyBuilder builder(space_outliner_, *bmain, *bmain->relations);

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
        &space_outliner_, &new_base_te->subtree, iter_id, new_base_te, TSE_SOME_ID, 0, false);

    builder.build_hierarchy_for_ID(*iter_id, *new_id_te);
  }
  FOREACH_MAIN_ID_END;

  BKE_main_relations_free(bmain);

  return tree;
}

void OverrideIDHierarchyBuilder::build_hierarchy_for_ID(ID &override_root_id,
                                                        TreeElement &te_to_expand)
{
  HierarchyBuildData build_data{override_root_id};
  build_hierarchy_for_ID_recursive(override_root_id, build_data, te_to_expand);
}

enum ForeachChildReturn {
  FOREACH_CONTINUE,
  FOREACH_BREAK,
};
/* Helpers (defined below). */
static void foreach_natural_hierarchy_child(const MainIDRelations &id_relations,
                                            const ID &parent_id,
                                            FunctionRef<ForeachChildReturn(ID &)> fn);
static bool id_is_in_override_hierarchy(const Main &bmain,
                                        const ID &id,
                                        const ID &relationship_parent_id,
                                        const ID &override_root_id);

void OverrideIDHierarchyBuilder::build_hierarchy_for_ID_recursive(const ID &parent_id,
                                                                  HierarchyBuildData &build_data,
                                                                  TreeElement &te_to_expand)
{
  /* In case this isn't added to the parents yet (does nothing if already there). */
  build_data.parent_ids.add(&parent_id);

  foreach_natural_hierarchy_child(id_relations_, parent_id, [&](ID &id) {
    /* Some IDs can use themselves, early abort. */
    if (&id == &parent_id) {
      return FOREACH_CONTINUE;
    }
    if (!id_is_in_override_hierarchy(bmain_, id, parent_id, build_data.override_root_id_)) {
      return FOREACH_CONTINUE;
    }

    /* Avoid endless recursion: If there is an ancestor for this ID already, it recurses into
     * itself. */
    if (build_data.parent_ids.lookup_key_default(&id, nullptr)) {
      return FOREACH_CONTINUE;
    }

    /* Avoid duplicates: If there is a sibling for this ID already, the same ID is just used
     * multiple times by the same parent. */
    if (build_data.sibling_ids.lookup_key_default(&id, nullptr)) {
      return FOREACH_CONTINUE;
    }

    /* We only want to add children whose parent isn't collapsed. Otherwise, in complex scenes with
     * thousands of relationships, the building can slow down tremendously. Tag the parent to allow
     * un-collapsing, but don't actually add the children. */
    if (!TSELEM_OPEN(TREESTORE(&te_to_expand), &space_outliner_)) {
      te_to_expand.flag |= TE_PRETEND_HAS_CHILDREN;
      return FOREACH_BREAK;
    }

    TreeElement *new_te = outliner_add_element(
        &space_outliner_, &te_to_expand.subtree, &id, &te_to_expand, TSE_SOME_ID, 0, false);

    build_data.sibling_ids.add(&id);

    /* Recurse into this ID. */
    HierarchyBuildData child_build_data{build_data.override_root_id_};
    child_build_data.parent_ids = build_data.parent_ids;
    child_build_data.parent_ids.add(&id);
    child_build_data.sibling_ids.reserve(10);
    build_hierarchy_for_ID_recursive(id, child_build_data, *new_te);

    return FOREACH_CONTINUE;
  });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Helpers for library override hierarchy building
 * \{ */

/**
 * Iterate over the IDs \a parent_id uses. E.g. the child collections and contained objects of a
 * parent collection. Also does special handling for object parenting, so that:
 * - When iterating over a child object, \a fn is executed for the parent instead.
 * - When iterating over a parent object, \a fn is _additionally_ executed for all children. Given
 *   that the parent object isn't skipped, the caller has to ensure it's not added in the hierarchy
 *   twice.
 * This allows us to build the hierarchy in the expected ("natural") order, where parent objects
 * are actual parent elements in the hierarchy, even though in data, the relation goes the other
 * way around (children point to or "use" the parent).
 *
 * Only handles regular object parenting, not cases like the "Child of" constraint. Other Outliner
 * display modes don't show this as parent in the hierarchy either.
 */
static void foreach_natural_hierarchy_child(const MainIDRelations &id_relations,
                                            const ID &parent_id,
                                            FunctionRef<ForeachChildReturn(ID &)> fn)
{
  const MainIDRelationsEntry *relations_of_id = static_cast<MainIDRelationsEntry *>(
      BLI_ghash_lookup(id_relations.relations_from_pointers, &parent_id));

  /* Iterate over all IDs used by the parent ID (e.g. the child-collections of a collection). */
  for (MainIDRelationsEntryItem *to_id_entry = relations_of_id->to_ids; to_id_entry;
       to_id_entry = to_id_entry->next)
  {
    /* An ID pointed to (used) by the ID to recurse into. */
    ID &target_id = **to_id_entry->id_pointer.to;

    /* Don't walk up the hierarchy, e.g. ignore pointers to parent collections. */
    if (to_id_entry->usage_flag & IDWALK_CB_LOOPBACK) {
      continue;
    }

    /* Special case for objects: Process the parent object instead of the child object. Below the
     * parent will add the child objects then. */
    if (GS(target_id.name) == ID_OB) {
      const Object &potential_child_ob = reinterpret_cast<const Object &>(target_id);
      if (potential_child_ob.parent) {
        if (fn(potential_child_ob.parent->id) == FOREACH_BREAK) {
          return;
        }
        continue;
      }
    }

    if (fn(target_id) == FOREACH_BREAK) {
      return;
    }
  }

  /* If the ID is an object, find and iterate over any child objects. */
  if (GS(parent_id.name) == ID_OB) {
    for (MainIDRelationsEntryItem *from_id_entry = relations_of_id->from_ids; from_id_entry;
         from_id_entry = from_id_entry->next)
    {
      ID &potential_child_id = *from_id_entry->id_pointer.from;

      if (GS(potential_child_id.name) != ID_OB) {
        continue;
      }

      const Object &potential_child_ob = reinterpret_cast<Object &>(potential_child_id);
      if (!potential_child_ob.parent || &potential_child_ob.parent->id != &parent_id) {
        continue;
      }

      if (fn(potential_child_id) == FOREACH_BREAK) {
        return;
      }
    }
  }
}

static bool id_is_in_override_hierarchy(const Main &bmain,
                                        const ID &id,
                                        const ID &relationship_parent_id,
                                        const ID &override_root_id)
{
  /* If #id is an embedded ID, this will be set to the owner, which is a real ID and contains the
   * override data. So queries of override data should be done via this, but the actual tree
   * element we add is the embedded ID. */
  const ID *real_override_id = &id;

  if (ID_IS_OVERRIDE_LIBRARY_VIRTUAL(&id)) {
    /* In many cases, `relationship_parent_id` is the owner, but not always (e.g. there can be
     * drivers directly between an object and a shape-key). */
    BKE_lib_override_library_get(const_cast<Main *>(&bmain),
                                 const_cast<ID *>(&id),
                                 const_cast<ID *>(&relationship_parent_id),
                                 const_cast<ID **>(&real_override_id));
  }

  if (!ID_IS_OVERRIDE_LIBRARY(real_override_id)) {
    return false;
  }
  /* Is this ID part of the same override hierarchy? */
  if (real_override_id->override_library->hierarchy_root != &override_root_id) {
    return false;
  }

  return true;
}

/** \} */

}  // namespace blender::ed::outliner
