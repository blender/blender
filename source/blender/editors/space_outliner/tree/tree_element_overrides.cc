/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "BKE_collection.h"
#include "BKE_lib_override.hh"

#include "BLI_function_ref.hh"
#include "BLI_listbase_wrapper.hh"
#include "BLI_map.hh"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_space_types.h"

#include "RNA_access.hh"
#include "RNA_path.hh"

#include "../outliner_intern.hh"

#include "tree_display.hh"
#include "tree_element_label.hh"
#include "tree_element_overrides.hh"

#include <stdexcept>

namespace blender::ed::outliner {

class OverrideRNAPathTreeBuilder {
  SpaceOutliner &space_outliner_;
  Map<std::string, TreeElement *> path_te_map;

 public:
  OverrideRNAPathTreeBuilder(SpaceOutliner &space_outliner);
  void build_path(TreeElement &parent, TreeElementOverridesData &override_data, short &index);

 private:
  TreeElement &ensure_label_element_for_prop(
      TreeElement &parent, StringRef elem_path, PointerRNA &ptr, PropertyRNA &prop, short &index);
  TreeElement &ensure_label_element_for_ptr(TreeElement &parent,
                                            StringRef elem_path,
                                            PointerRNA &ptr,
                                            short &index);
  void ensure_entire_collection(TreeElement &te_to_expand,
                                const TreeElementOverridesData &override_data,
                                const char *coll_prop_path,
                                short &index);
};

/* -------------------------------------------------------------------- */
/** \name Base Element
 *
 * Represents an ID that has overridden properties. The expanding will invoke building of tree
 * elements for the full RNA path of the property.
 *
 * \{ */

TreeElementOverridesBase::TreeElementOverridesBase(TreeElement &legacy_te, ID &id)
    : AbstractTreeElement(legacy_te), id(id)
{
  BLI_assert(legacy_te.store_elem->type == TSE_LIBRARY_OVERRIDE_BASE);
  if (legacy_te.parent != nullptr &&
      ELEM(legacy_te.parent->store_elem->type, TSE_SOME_ID, TSE_LAYER_COLLECTION))
  {
    legacy_te.name = IFACE_("Library Overrides");
  }
  else {
    legacy_te.name = id.name + 2;
  }
}

StringRefNull TreeElementOverridesBase::get_warning() const
{
  if (id.flag & LIB_LIB_OVERRIDE_RESYNC_LEFTOVER) {
    return RPT_("This override data-block is not needed anymore, but was detected as user-edited");
  }

  if (ID_IS_OVERRIDE_LIBRARY_REAL(&id) && ID_REAL_USERS(&id) == 0) {
    return RPT_("This override data-block is unused");
  }

  return {};
}

static void iterate_properties_to_display(ID &id,
                                          const bool show_system_overrides,
                                          FunctionRef<void(TreeElementOverridesData &data)> fn)
{
  PointerRNA override_rna_ptr;
  PropertyRNA *override_rna_prop;

  PointerRNA idpoin = RNA_id_pointer_create(&id);

  for (IDOverrideLibraryProperty *override_prop :
       ListBaseWrapper<IDOverrideLibraryProperty>(id.override_library->properties))
  {
    int rnaprop_index = 0;
    const bool is_rna_path_valid = BKE_lib_override_rna_property_find(
        &idpoin, override_prop, &override_rna_ptr, &override_rna_prop, &rnaprop_index);

    /* Check for conditions where the liboverride property should be considered as a system
     * override, if needed. */
    if (is_rna_path_valid && !show_system_overrides) {
      bool do_skip = true;
      bool is_system_override = false;

      /* Matching ID pointers are considered as system overrides. */
      if (ELEM(override_prop->rna_prop_type, PROP_POINTER, PROP_COLLECTION) &&
          RNA_struct_is_ID(RNA_property_pointer_type(&override_rna_ptr, override_rna_prop)))
      {
        for (IDOverrideLibraryPropertyOperation *override_prop_op :
             ListBaseWrapper<IDOverrideLibraryPropertyOperation>(override_prop->operations))
        {
          if ((override_prop_op->flag & LIBOVERRIDE_OP_FLAG_IDPOINTER_MATCH_REFERENCE) == 0) {
            do_skip = false;
            break;
          }
          is_system_override = true;
        }
      }

      /* Animated/driven properties are considered as system overrides. */
      if (!is_system_override && !BKE_lib_override_library_property_is_animated(
                                     &id, override_prop, override_rna_prop, rnaprop_index))
      {
        do_skip = false;
      }

      if (do_skip) {
        continue;
      }
    }

    TreeElementOverridesData data = {
        id, *override_prop, override_rna_ptr, *override_rna_prop, is_rna_path_valid};

    fn(data);
  }
}

void TreeElementOverridesBase::expand(SpaceOutliner &space_outliner) const
{
  BLI_assert(id.override_library != nullptr);

  const bool show_system_overrides = (SUPPORT_FILTER_OUTLINER(&space_outliner) &&
                                      (space_outliner.filter & SO_FILTER_SHOW_SYSTEM_OVERRIDES) !=
                                          0);

  OverrideRNAPathTreeBuilder path_builder(space_outliner);
  short index = 0;

  iterate_properties_to_display(id, show_system_overrides, [&](TreeElementOverridesData &data) {
    path_builder.build_path(legacy_te_, data, index);
  });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Overridden Property
 *
 * Represents an RNA property that was overridden.
 *
 * \{ */

TreeElementOverridesProperty::TreeElementOverridesProperty(TreeElement &legacy_te,
                                                           TreeElementOverridesData &override_data)
    : AbstractTreeElement(legacy_te),
      override_rna_ptr(override_data.override_rna_ptr),
      override_rna_prop(override_data.override_rna_prop),
      rna_path(override_data.override_property.rna_path),
      is_rna_path_valid(override_data.is_rna_path_valid)
{
  BLI_assert(
      ELEM(legacy_te.store_elem->type, TSE_LIBRARY_OVERRIDE, TSE_LIBRARY_OVERRIDE_OPERATION));

  legacy_te.name = RNA_property_ui_name(&override_data.override_rna_prop);
}

StringRefNull TreeElementOverridesProperty::get_warning() const
{
  if (!is_rna_path_valid) {
    return RPT_(
        "This override property does not exist in current data, it will be removed on "
        "next .blend file save");
  }

  return {};
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Overridden Property Operation
 *
 * See #TreeElementOverridesPropertyOperation.
 * \{ */

TreeElementOverridesPropertyOperation::TreeElementOverridesPropertyOperation(
    TreeElement &legacy_te, TreeElementOverridesData &override_data)
    : TreeElementOverridesProperty(legacy_te, override_data)
{
  BLI_assert(legacy_te.store_elem->type == TSE_LIBRARY_OVERRIDE_OPERATION);
  BLI_assert_msg(RNA_property_type(&override_rna_prop) == PROP_COLLECTION,
                 "Override operations are only supported for collections right now");
  /* Quiet Clang Static Analyzer warning by throwing instead of asserting (possible
   * null-dereference). */
  if (!override_data.operation) {
    throw std::invalid_argument("missing operation");
  }

  operation_ = std::make_unique<IDOverrideLibraryPropertyOperation>(*override_data.operation);
  /* Just for extra sanity. */
  operation_->next = operation_->prev = nullptr;

  if (std::optional<PointerRNA> col_item_ptr = get_collection_ptr()) {
    const char *dyn_name = RNA_struct_name_get_alloc(&*col_item_ptr, nullptr, 0, nullptr);
    if (dyn_name) {
      legacy_te.name = dyn_name;
      legacy_te.flag |= TE_FREE_NAME;
    }
    else {
      legacy_te.name = RNA_struct_ui_name(col_item_ptr->type);
    }
  }
}

StringRefNull TreeElementOverridesPropertyOperation::get_override_operation_label() const
{
  switch (operation_->operation) {
    case LIBOVERRIDE_OP_INSERT_AFTER:
    case LIBOVERRIDE_OP_INSERT_BEFORE:
      return RPT_("Added through override");
    case LIBOVERRIDE_OP_REPLACE:
      /* Returning nothing so that drawing code shows actual RNA button instead. */
      return {};
    /* Following cases are not expected in regular situation, but could be found in experimental
     * files. */
    case LIBOVERRIDE_OP_NOOP:
      return RPT_("Protected from override");
    case LIBOVERRIDE_OP_ADD:
      return RPT_("Additive override");
    case LIBOVERRIDE_OP_SUBTRACT:
      return RPT_("Subtractive override");
    case LIBOVERRIDE_OP_MULTIPLY:
      return RPT_("Multiplicative override");
    default:
      BLI_assert_unreachable();
      return {};
  }
}

std::optional<BIFIconID> TreeElementOverridesPropertyOperation::get_icon() const
{
  if (const std::optional<PointerRNA> col_item_ptr = get_collection_ptr()) {
    return RNA_struct_ui_icon(col_item_ptr->type);
  }

  return {};
}

std::optional<PointerRNA> TreeElementOverridesPropertyOperation::get_collection_ptr() const
{
  PointerRNA col_item_ptr;
  if (RNA_property_collection_lookup_int(const_cast<PointerRNA *>(&override_rna_ptr),
                                         &override_rna_prop,
                                         operation_->subitem_local_index,
                                         &col_item_ptr))
  {
    return col_item_ptr;
  }

  return {};
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Helper to build a hierarchy from an RNA path.
 *
 * Builds a nice hierarchy representing the nested structs of the override property's RNA path
 * using UI names and icons. For example `animation_visualization_mothion_path.frame_end` becomes:
 * - Animation Visualization
 *   - Motion Paths
 *     - End Frame
 *
 * Paths are merged so that each RNA sub-path is only represented once in the tree. So there is
 * some finicky path building going on to create a path -> tree-element map.
 *
 * This is more complicated than you'd think it needs to be. Mostly because of RNA collection
 * overrides:
 * - A single override may add (and in future remove) multiple collection items. So all operations
 *   of the override have to be considered.
 * - The order of collection items may matter (e.g. for modifiers), so if collection items are
 *   added/removed, we want to show all other collection items too, in the right order.
 *
 * - If the override is inside some collection item, the collection item has to be built, but the
 * RNA path iterator doesn't
 * \{ */

OverrideRNAPathTreeBuilder::OverrideRNAPathTreeBuilder(SpaceOutliner &space_outliner)
    : space_outliner_(space_outliner)
{
}

void OverrideRNAPathTreeBuilder::build_path(TreeElement &parent,
                                            TreeElementOverridesData &override_data,
                                            short &index)
{
  PointerRNA idpoin = RNA_id_pointer_create(&override_data.id);

  ListBase path_elems = {nullptr};
  if (!RNA_path_resolve_elements(&idpoin, override_data.override_property.rna_path, &path_elems)) {
    return;
  }

  const char *elem_path = nullptr;
  TreeElement *te_to_expand = &parent;

  LISTBASE_FOREACH (PropertyElemRNA *, elem, &path_elems) {
    if (!elem->next) {
      /* The last element is added as #TSE_LIBRARY_OVERRIDE below. */
      break;
    }
    const char *previous_path = elem_path;
    const char *new_path = RNA_path_append(previous_path, &elem->ptr, elem->prop, -1, nullptr);

    te_to_expand = &ensure_label_element_for_prop(
        *te_to_expand, new_path, elem->ptr, *elem->prop, index);

    /* Above the collection property was added (e.g. "Modifiers"), to get the actual collection
     * item the path refers to, we have to peek at the following path element and add a tree
     * element for its pointer (e.g. "My Subdiv Modifier"). */
    if (RNA_property_type(elem->prop) == PROP_COLLECTION) {
      const int coll_item_idx = RNA_property_collection_lookup_index(
          &elem->ptr, elem->prop, &elem->next->ptr);
      const char *coll_item_path = RNA_path_append(
          previous_path, &elem->ptr, elem->prop, coll_item_idx, nullptr);

      te_to_expand = &ensure_label_element_for_ptr(
          *te_to_expand, coll_item_path, elem->next->ptr, index);

      MEM_delete(new_path);
      new_path = coll_item_path;
    }

    if (new_path) {
      MEM_delete(elem_path);
      elem_path = new_path;
    }
  }
  BLI_freelistN(&path_elems);

  /* Special case: Overriding collections, e.g. adding or removing items. In this case we add
   * elements for all collection items to show full context, and indicate which ones were
   * added/removed (currently added only). Note that a single collection override may add/remove
   * multiple items. */
  if (RNA_property_type(&override_data.override_rna_prop) == PROP_COLLECTION) {
    /* Tree element for the actual collection item (e.g. "Modifiers"). Can just use the override
     * ptr & prop here, since they point to the collection property (e.g. `modifiers`). */
    te_to_expand = &ensure_label_element_for_prop(*te_to_expand,
                                                  override_data.override_property.rna_path,
                                                  override_data.override_rna_ptr,
                                                  override_data.override_rna_prop,
                                                  index);

    ensure_entire_collection(*te_to_expand, override_data, elem_path, index);
  }
  /* Some properties have multiple operations (e.g. an array property with multiple changed
   * values), so the element may already be present. At this point they are displayed as a single
   * property in the tree, so don't add it multiple times here. */
  else if (!path_te_map.contains(override_data.override_property.rna_path)) {
    AbstractTreeDisplay::add_element(&space_outliner_,
                                     &te_to_expand->subtree,
                                     &override_data.id,
                                     &override_data,
                                     te_to_expand,
                                     TSE_LIBRARY_OVERRIDE,
                                     index++);
  }

  MEM_delete(elem_path);
}

void OverrideRNAPathTreeBuilder::ensure_entire_collection(
    TreeElement &te_to_expand,
    const TreeElementOverridesData &override_data,
    /* The path of the owning collection property. */
    const char *coll_prop_path,
    short &index)
{
  BLI_assert(tree_element_cast<AbstractTreeElement>(&te_to_expand) != nullptr);

  TreeElement *previous_te = nullptr;
  int item_idx = 0;
  RNA_PROP_BEGIN (&override_data.override_rna_ptr, itemptr, &override_data.override_rna_prop) {
    const char *coll_item_path = RNA_path_append(coll_prop_path,
                                                 &override_data.override_rna_ptr,
                                                 &override_data.override_rna_prop,
                                                 item_idx,
                                                 nullptr);
    IDOverrideLibraryPropertyOperation *item_operation =
        BKE_lib_override_library_property_operation_find(&override_data.override_property,
                                                         nullptr,
                                                         nullptr,
                                                         {},
                                                         {},
                                                         -1,
                                                         item_idx,
                                                         false,
                                                         nullptr);
    TreeElement *current_te = nullptr;

    TreeElement *existing_te = path_te_map.lookup_default(coll_item_path, nullptr);

    if (existing_te) {
      /* Reinsert the element to make sure the order is right. It may have been inserted by a
       * previous override. */
      BLI_remlink(&te_to_expand.subtree, existing_te);
      BLI_insertlinkafter(&te_to_expand.subtree, previous_te, existing_te);
      current_te = existing_te;
    }
    /* Is there an operation for this item (added or removed the item to/from the collection)? If
     * so indicate it as override using #TSE_LIBRARY_OVERRIDE_OPERATION. Otherwise it's just a
     * regular collection we display for context. */
    else if (item_operation) {
      TreeElementOverridesData override_op_data = override_data;
      override_op_data.operation = item_operation;

      current_te = AbstractTreeDisplay::add_element(&space_outliner_,
                                                    &te_to_expand.subtree,
                                                    &override_op_data.id,
                                                    /* Element will store a copy. */
                                                    &override_op_data,
                                                    &te_to_expand,
                                                    TSE_LIBRARY_OVERRIDE_OPERATION,
                                                    index++);
    }
    else {
      current_te = &ensure_label_element_for_ptr(te_to_expand, coll_item_path, itemptr, index);
    }

    MEM_delete(coll_item_path);
    item_idx++;
    previous_te = current_te;
  }
  RNA_PROP_END;
}

static BIFIconID get_property_icon(PointerRNA &ptr, PropertyRNA &prop)
{
  BIFIconID icon = RNA_property_ui_icon(&prop);
  if (icon) {
    return icon;
  }

  /* Try if the collection item type has a dedicated icon (e.g. #ICON_MODIFIER for the
   * #Object.modifiers property). */
  if (RNA_property_type(&prop) == PROP_COLLECTION) {
    const StructRNA *coll_ptr_type = RNA_property_pointer_type(&ptr, &prop);
    icon = RNA_struct_ui_icon(coll_ptr_type);
    if (icon != ICON_DOT) {
      return icon;
    }
  }

  return ICON_NONE;
}

TreeElement &OverrideRNAPathTreeBuilder::ensure_label_element_for_prop(
    TreeElement &parent, StringRef elem_path, PointerRNA &ptr, PropertyRNA &prop, short &index)
{
  return *path_te_map.lookup_or_add_cb(elem_path, [&]() {
    TreeElement *new_te = AbstractTreeDisplay::add_element(&space_outliner_,
                                                           &parent.subtree,
                                                           nullptr,
                                                           (void *)RNA_property_ui_name(&prop),
                                                           &parent,
                                                           TSE_GENERIC_LABEL,
                                                           index++,
                                                           false);
    TreeElementLabel *te_label = tree_element_cast<TreeElementLabel>(new_te);

    te_label->set_icon(get_property_icon(ptr, prop));
    return new_te;
  });
}

TreeElement &OverrideRNAPathTreeBuilder::ensure_label_element_for_ptr(TreeElement &parent,
                                                                      StringRef elem_path,
                                                                      PointerRNA &ptr,
                                                                      short &index)
{
  return *path_te_map.lookup_or_add_cb(elem_path, [&]() {
    const char *dyn_name = RNA_struct_name_get_alloc(&ptr, nullptr, 0, nullptr);

    TreeElement *new_te = AbstractTreeDisplay::add_element(
        &space_outliner_,
        &parent.subtree,
        nullptr,
        (void *)(dyn_name ? dyn_name : RNA_struct_ui_name(ptr.type)),
        &parent,
        TSE_GENERIC_LABEL,
        index++);
    TreeElementLabel *te_label = tree_element_cast<TreeElementLabel>(new_te);
    te_label->set_icon(RNA_struct_ui_icon(ptr.type));

    MEM_delete(dyn_name);

    return new_te;
  });
}

/** \} */

}  // namespace blender::ed::outliner
