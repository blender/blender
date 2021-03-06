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

#include "BLI_utildefines.h"

#include "../outliner_intern.h"

#include "tree_element_id.hh"

namespace blender::ed::outliner {

TreeElementID::TreeElementID(TreeElement &legacy_te, const ID &id) : AbstractTreeElement(legacy_te)
{
  BLI_assert(legacy_te_.store_elem->type == TSE_SOME_ID);
  BLI_assert(TSE_IS_REAL_ID(legacy_te_.store_elem));

  /* Default, some specific types override this. */
  legacy_te_.name = id.name + 2;
  legacy_te_.idcode = GS(id.name);
}

TreeElementID *TreeElementID::createFromID(TreeElement &legacy_te, const ID &id)
{
  switch (ID_Type type = GS(id.name); type) {
    case ID_LI:
      return new TreeElementIDLibrary(legacy_te, id);
    case ID_SCE:
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
      return new TreeElementID(legacy_te, id);
      /* Deprecated */
    case ID_IP:
      BLI_assert(!"Outliner trying to build tree-element for deprecated ID type");
      return nullptr;
  }

  return nullptr;
}

TreeElementIDLibrary::TreeElementIDLibrary(TreeElement &legacy_te, const ID &id)
    : TreeElementID(legacy_te, id)
{
  legacy_te.name = ((Library &)id).filepath;
}

}  // namespace blender::ed::outliner
