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
 *
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup cmpnodes
 */

#include "DNA_movieclip_types.h"

#include "BLI_math_base.h"
#include "BLI_math_color.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_composite_util.hh"

/* **************** Keying Screen  ******************** */

namespace blender::nodes {

static void cmp_node_keyingscreen_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Color>(N_("Screen"));
}

}  // namespace blender::nodes

static void node_composit_init_keyingscreen(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeKeyingScreenData *data = MEM_cnew<NodeKeyingScreenData>(__func__);
  node->storage = data;
}

static void node_composit_buts_keyingscreen(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;

  uiTemplateID(layout,
               C,
               ptr,
               "clip",
               nullptr,
               nullptr,
               nullptr,
               UI_TEMPLATE_ID_FILTER_ALL,
               false,
               nullptr);

  if (node->id) {
    MovieClip *clip = (MovieClip *)node->id;
    uiLayout *col;
    PointerRNA tracking_ptr;

    RNA_pointer_create(&clip->id, &RNA_MovieTracking, &clip->tracking, &tracking_ptr);

    col = uiLayoutColumn(layout, true);
    uiItemPointerR(col, ptr, "tracking_object", &tracking_ptr, "objects", "", ICON_OBJECT_DATA);
  }
}

void register_node_type_cmp_keyingscreen()
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_KEYINGSCREEN, "Keying Screen", NODE_CLASS_MATTE);
  ntype.declare = blender::nodes::cmp_node_keyingscreen_declare;
  ntype.draw_buttons = node_composit_buts_keyingscreen;
  node_type_init(&ntype, node_composit_init_keyingscreen);
  node_type_storage(
      &ntype, "NodeKeyingScreenData", node_free_standard_storage, node_copy_standard_storage);

  nodeRegisterType(&ntype);
}
