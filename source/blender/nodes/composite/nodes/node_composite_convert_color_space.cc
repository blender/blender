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
 * The Original Code is Copyright (C) 2021 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup cmpnodes
 */

#include "node_composite_util.hh"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "IMB_colormanagement.h"

namespace blender::nodes::node_composite_convert_color_space_cc {

static void CMP_NODE_CONVERT_COLOR_SPACE_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_output<decl::Color>(N_("Image"));
}

static void node_composit_init_convert_colorspace(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeConvertColorSpace *ncs = static_cast<NodeConvertColorSpace *>(
      MEM_callocN(sizeof(NodeConvertColorSpace), "node colorspace"));
  const char *first_colorspace = IMB_colormanagement_role_colorspace_name_get(
      COLOR_ROLE_SCENE_LINEAR);
  if (first_colorspace && first_colorspace[0]) {
    STRNCPY(ncs->from_color_space, first_colorspace);
    STRNCPY(ncs->to_color_space, first_colorspace);
  }
  else {
    ncs->from_color_space[0] = 0;
    ncs->to_color_space[0] = 0;
  }
  node->storage = ncs;
}

static void node_composit_buts_convert_colorspace(uiLayout *layout,
                                                  bContext *UNUSED(C),
                                                  PointerRNA *ptr)
{
  uiItemR(layout, ptr, "from_color_space", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "to_color_space", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

}  // namespace blender::nodes::node_composite_convert_color_space_cc

void register_node_type_cmp_convert_color_space(void)
{
  namespace file_ns = blender::nodes::node_composite_convert_color_space_cc;
  static bNodeType ntype;

  cmp_node_type_base(
      &ntype, CMP_NODE_CONVERT_COLOR_SPACE, "Convert Colorspace", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::CMP_NODE_CONVERT_COLOR_SPACE_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_convert_colorspace;
  node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
  node_type_init(&ntype, file_ns::node_composit_init_convert_colorspace);
  node_type_storage(
      &ntype, "NodeConvertColorSpace", node_free_standard_storage, node_copy_standard_storage);

  nodeRegisterType(&ntype);
}
