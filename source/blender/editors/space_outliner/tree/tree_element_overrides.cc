/*
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
 */

/** \file
 * \ingroup spoutliner
 */

#include "BKE_collection.h"
#include "BKE_lib_override.h"

#include "BLI_utildefines.h"

#include "BLI_listbase_wrapper.hh"

#include "BLT_translation.h"

#include "RNA_access.h"

#include "../outliner_intern.h"
#include "tree_display.h"

#include "tree_element_overrides.hh"

namespace blender::ed::outliner {

TreeElementOverridesBase::TreeElementOverridesBase(TreeElement &legacy_te, ID &id)
    : AbstractTreeElement(legacy_te), id_(id)
{
  BLI_assert(legacy_te.store_elem->type == TSE_LIBRARY_OVERRIDE_BASE);
  legacy_te.name = IFACE_("Library Overrides");
}

void TreeElementOverridesBase::expand(SpaceOutliner &space_outliner) const
{
  BLI_assert(id_.override_library != nullptr);

  PointerRNA idpoin;
  RNA_id_pointer_create(&id_, &idpoin);

  PointerRNA override_rna_ptr;
  PropertyRNA *override_rna_prop;
  short index = 0;

  for (auto *override_prop :
       ListBaseWrapper<IDOverrideLibraryProperty>(id_.override_library->properties)) {
    if (!BKE_lib_override_rna_property_find(
            &idpoin, override_prop, &override_rna_ptr, &override_rna_prop)) {
      /* This is fine, override properties list is not always fully up-to-date with current
       * RNA/IDProps etc., this gets cleaned up when re-generating the overrides rules,
       * no error here. */
      continue;
    }

    TreeElementOverridesData data = {id_, *override_prop};
    outliner_add_element(
        &space_outliner, &legacy_te_.subtree, &data, &legacy_te_, TSE_LIBRARY_OVERRIDE, index++);
  }
}

TreeElementOverridesProperty::TreeElementOverridesProperty(TreeElement &legacy_te,
                                                           TreeElementOverridesData &override_data)
    : AbstractTreeElement(legacy_te),
      id_(override_data.id),
      override_prop_(override_data.override_property)
{
  BLI_assert(legacy_te.store_elem->type == TSE_LIBRARY_OVERRIDE);

  legacy_te.name = override_prop_.rna_path;
}

}  // namespace blender::ed::outliner
