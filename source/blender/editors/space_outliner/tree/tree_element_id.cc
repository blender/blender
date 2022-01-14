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

#include "DNA_ID.h"
#include "DNA_space_types.h"

#include "BLI_listbase_wrapper.hh"
#include "BLI_utildefines.h"

#include "BKE_lib_override.h"

#include "BLT_translation.h"

#include "RNA_access.h"

#include "../outliner_intern.hh"
#include "common.hh"
#include "tree_element_id_library.hh"
#include "tree_element_id_scene.hh"

#include "tree_element_id.hh"

namespace blender::ed::outliner {

std::unique_ptr<TreeElementID> TreeElementID::createFromID(TreeElement &legacy_te, ID &id)
{
  switch (ID_Type type = GS(id.name); type) {
    case ID_LI:
      return std::make_unique<TreeElementIDLibrary>(legacy_te, (Library &)id);
    case ID_SCE:
      return std::make_unique<TreeElementIDScene>(legacy_te, (Scene &)id);
    case ID_OB:
    case ID_ME:
    case ID_CU:
    case ID_MB:
    case ID_MA:
    case ID_TE:
    case ID_LT:
    case ID_LA:
    case ID_CA:
    case ID_KE:
    case ID_SCR:
    case ID_WO:
    case ID_SPK:
    case ID_GR:
    case ID_NT:
    case ID_BR:
    case ID_PA:
    case ID_MC:
    case ID_MSK:
    case ID_LS:
    case ID_LP:
    case ID_GD:
    case ID_WS:
    case ID_HA:
    case ID_PT:
    case ID_VO:
    case ID_SIM:
    case ID_WM:
    case ID_IM:
    case ID_VF:
    case ID_TXT:
    case ID_SO:
    case ID_AR:
    case ID_AC:
    case ID_PAL:
    case ID_PC:
    case ID_CF:
      return std::make_unique<TreeElementID>(legacy_te, id);
      /* Deprecated */
    case ID_IP:
      BLI_assert_msg(0, "Outliner trying to build tree-element for deprecated ID type");
      return nullptr;
  }

  return nullptr;
}

/* -------------------------------------------------------------------- */
/* ID Tree-Element Base Class (common/default logic) */

TreeElementID::TreeElementID(TreeElement &legacy_te, ID &id)
    : AbstractTreeElement(legacy_te), id_(id)
{
  BLI_assert(legacy_te_.store_elem->type == TSE_SOME_ID);
  BLI_assert(TSE_IS_REAL_ID(legacy_te_.store_elem));

  /* Default, some specific types override this. */
  legacy_te_.name = id.name + 2;
  legacy_te_.idcode = GS(id.name);
}

void TreeElementID::postExpand(SpaceOutliner &space_outliner) const
{
  const bool lib_overrides_visible = !SUPPORT_FILTER_OUTLINER(&space_outliner) ||
                                     ((space_outliner.filter & SO_FILTER_NO_LIB_OVERRIDE) == 0);

  if (lib_overrides_visible && ID_IS_OVERRIDE_LIBRARY_REAL(&id_)) {
    outliner_add_element(
        &space_outliner, &legacy_te_.subtree, &id_, &legacy_te_, TSE_LIBRARY_OVERRIDE_BASE, 0);
  }
}

bool TreeElementID::expandPoll(const SpaceOutliner &space_outliner) const
{
  const TreeStoreElem *tsepar = legacy_te_.parent ? TREESTORE(legacy_te_.parent) : nullptr;
  return (tsepar == nullptr || tsepar->type != TSE_ID_BASE || space_outliner.filter_id_type);
}

void TreeElementID::expand_animation_data(SpaceOutliner &space_outliner,
                                          const AnimData *anim_data) const
{
  if (outliner_animdata_test(anim_data)) {
    outliner_add_element(
        &space_outliner, &legacy_te_.subtree, &id_, &legacy_te_, TSE_ANIM_DATA, 0);
  }
}

}  // namespace blender::ed::outliner
