/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "BKE_collection.h"
#include "BKE_lib_override.h"

#include "BLI_utildefines.h"

#include "BLI_listbase_wrapper.hh"

#include "BLT_translation.h"

#include "DNA_space_types.h"

#include "RNA_access.h"

#include "../outliner_intern.hh"

#include "tree_element_overrides.hh"

namespace blender::ed::outliner {

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

void TreeElementOverridesBase::expand(SpaceOutliner &space_outliner) const
{
  BLI_assert(id.override_library != nullptr);

  const bool show_system_overrides = (SUPPORT_FILTER_OUTLINER(&space_outliner) &&
                                      (space_outliner.filter & SO_FILTER_SHOW_SYSTEM_OVERRIDES) !=
                                          0);
  PointerRNA idpoin;
  RNA_id_pointer_create(&id, &idpoin);

  PointerRNA override_rna_ptr;
  PropertyRNA *override_rna_prop;
  short index = 0;

  for (auto *override_prop :
       ListBaseWrapper<IDOverrideLibraryProperty>(id.override_library->properties)) {
    const bool is_rna_path_valid = BKE_lib_override_rna_property_find(
        &idpoin, override_prop, &override_rna_ptr, &override_rna_prop);
    if (is_rna_path_valid && !show_system_overrides &&
        ELEM(override_prop->rna_prop_type, PROP_POINTER, PROP_COLLECTION) &&
        RNA_struct_is_ID(RNA_property_pointer_type(&override_rna_ptr, override_rna_prop))) {
      bool do_continue = true;
      for (auto *override_prop_op :
           ListBaseWrapper<IDOverrideLibraryPropertyOperation>(override_prop->operations)) {
        if ((override_prop_op->flag & IDOVERRIDE_LIBRARY_FLAG_IDPOINTER_MATCH_REFERENCE) == 0) {
          do_continue = false;
          break;
        }
      }

      if (do_continue) {
        continue;
      }
    }

    TreeElementOverridesData data = {
        id, *override_prop, override_rna_ptr, *override_rna_prop, is_rna_path_valid};
    outliner_add_element(
        &space_outliner, &legacy_te_.subtree, &data, &legacy_te_, TSE_LIBRARY_OVERRIDE, index++);
  }
}

TreeElementOverridesProperty::TreeElementOverridesProperty(TreeElement &legacy_te,
                                                           TreeElementOverridesData &override_data)
    : AbstractTreeElement(legacy_te),
      override_rna_ptr(override_data.override_rna_ptr),
      override_rna_prop(override_data.override_rna_prop)
{
  BLI_assert(legacy_te.store_elem->type == TSE_LIBRARY_OVERRIDE);

  legacy_te.name = override_data.override_property.rna_path;
  /* Abusing this for now, better way to do it is also pending current refactor of the whole tree
   * code to use C++. */
  legacy_te.directdata = POINTER_FROM_UINT(override_data.is_rna_path_valid);
}

}  // namespace blender::ed::outliner
