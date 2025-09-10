/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_ID.h"
#include "DNA_space_types.h"

#include "BKE_anim_data.hh"

#include "../outliner_intern.hh"
#include "common.hh"
#include "tree_element_id_action.hh"
#include "tree_element_id_armature.hh"
#include "tree_element_id_collection.hh"
#include "tree_element_id_curve.hh"
#include "tree_element_id_gpencil_legacy.hh"
#include "tree_element_id_grease_pencil.hh"
#include "tree_element_id_library.hh"
#include "tree_element_id_linestyle.hh"
#include "tree_element_id_mesh.hh"
#include "tree_element_id_metaball.hh"
#include "tree_element_id_object.hh"
#include "tree_element_id_scene.hh"
#include "tree_element_id_texture.hh"

#include "tree_element_id.hh"

namespace blender::ed::outliner {

std::unique_ptr<TreeElementID> TreeElementID::create_from_id(TreeElement &legacy_te, ID &id)
{
  if (ID_TYPE_IS_DEPRECATED(GS(id.name))) {
    BLI_assert_msg(0, "Outliner trying to build tree-element for deprecated ID type");
    return nullptr;
  }

  switch (ID_Type type = GS(id.name); type) {
    case ID_LI:
      return std::make_unique<TreeElementIDLibrary>(legacy_te, (Library &)id);
    case ID_SCE:
      return std::make_unique<TreeElementIDScene>(legacy_te, (Scene &)id);
    case ID_ME:
      return std::make_unique<TreeElementIDMesh>(legacy_te, (Mesh &)id);
    case ID_CU_LEGACY:
      return std::make_unique<TreeElementIDCurve>(legacy_te, (Curve &)id);
    case ID_MB:
      return std::make_unique<TreeElementIDMetaBall>(legacy_te, (MetaBall &)id);
    case ID_TE:
      return std::make_unique<TreeElementIDTexture>(legacy_te, (Tex &)id);
    case ID_LS:
      return std::make_unique<TreeElementIDLineStyle>(legacy_te, (FreestyleLineStyle &)id);
    case ID_GD_LEGACY:
      return std::make_unique<TreeElementIDGPLegacy>(legacy_te, (bGPdata &)id);
    case ID_GP:
      return std::make_unique<TreeElementIDGreasePencil>(legacy_te, (GreasePencil &)id);
    case ID_GR:
      return std::make_unique<TreeElementIDCollection>(legacy_te, (Collection &)id);
    case ID_AR:
      return std::make_unique<TreeElementIDArmature>(legacy_te, (bArmature &)id);
    case ID_OB:
      return std::make_unique<TreeElementIDObject>(legacy_te, (Object &)id);
    case ID_AC:
      return std::make_unique<TreeElementIDAction>(legacy_te, (bAction &)id);
    case ID_MA:
    case ID_LT:
    case ID_LA:
    case ID_CA:
    case ID_KE:
    case ID_SCR:
    case ID_WO:
    case ID_SPK:
    case ID_NT:
    case ID_BR:
    case ID_PA:
    case ID_MC:
    case ID_MSK:
    case ID_LP:
    case ID_WS:
    case ID_CV:
    case ID_PT:
    case ID_VO:
    case ID_WM:
    case ID_IM:
    case ID_VF:
    case ID_TXT:
    case ID_SO:
    case ID_PAL:
    case ID_PC:
    case ID_CF:
      return std::make_unique<TreeElementID>(legacy_te, id);
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

bool TreeElementID::expand_poll(const SpaceOutliner &space_outliner) const
{
  const TreeStoreElem *tsepar = legacy_te_.parent ? TREESTORE(legacy_te_.parent) : nullptr;
  return (tsepar == nullptr || tsepar->type != TSE_ID_BASE || space_outliner.filter_id_type);
}

void TreeElementID::expand(SpaceOutliner & /*space_outliner*/) const
{
  /* Not all IDs support animation data. Will be null then. */
  AnimData *anim_data = BKE_animdata_from_id(&id_);
  if (anim_data) {
    expand_animation_data(anim_data);
  }
}

void TreeElementID::expand_animation_data(AnimData *anim_data) const
{
  if (outliner_animdata_test(anim_data)) {
    add_element(&legacy_te_.subtree, &id_, anim_data, &legacy_te_, TSE_ANIM_DATA, 0);
  }
}

}  // namespace blender::ed::outliner
