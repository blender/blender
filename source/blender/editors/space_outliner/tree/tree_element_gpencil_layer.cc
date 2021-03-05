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

#include "BLI_utildefines.h"

#include "DNA_gpencil_types.h"

#include "../outliner_intern.h"

#include "tree_element_gpencil_layer.hh"

namespace blender::ed::outliner {

TreeElementGPencilLayer::TreeElementGPencilLayer(TreeElement &legacy_te, bGPDlayer &gplayer)
    : AbstractTreeElement(legacy_te)
{
  BLI_assert(legacy_te.store_elem->type == TSE_GP_LAYER);
  /* this element's info */
  legacy_te.name = gplayer.info;
  legacy_te.directdata = &gplayer;
}

}  // namespace blender::ed::outliner
