/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "BLI_utildefines.h"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_space_types.h"

#include "../outliner_intern.hh"

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
