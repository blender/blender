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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes::node_shader_mapping_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>(N_("Vector"))
      .default_value({0.0f, 0.0f, 0.0f})
      .min(-FLT_MAX)
      .max(FLT_MAX);
  b.add_input<decl::Vector>(N_("Location"))
      .default_value({0.0f, 0.0f, 0.0f})
      .min(-FLT_MAX)
      .max(FLT_MAX)
      .subtype(PROP_TRANSLATION);
  b.add_input<decl::Vector>(N_("Rotation"))
      .default_value({0.0f, 0.0f, 0.0f})
      .min(-FLT_MAX)
      .max(FLT_MAX)
      .subtype(PROP_EULER);
  b.add_input<decl::Vector>(N_("Scale"))
      .default_value({1.0f, 1.0f, 1.0f})
      .min(-FLT_MAX)
      .max(FLT_MAX)
      .subtype(PROP_XYZ);
  b.add_output<decl::Vector>(N_("Vector"));
}

static void node_shader_buts_mapping(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "vector_type", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

static const char *gpu_shader_get_name(int mode)
{
  switch (mode) {
    case NODE_MAPPING_TYPE_POINT:
      return "mapping_point";
    case NODE_MAPPING_TYPE_TEXTURE:
      return "mapping_texture";
    case NODE_MAPPING_TYPE_VECTOR:
      return "mapping_vector";
    case NODE_MAPPING_TYPE_NORMAL:
      return "mapping_normal";
  }
  return nullptr;
}

static int gpu_shader_mapping(GPUMaterial *mat,
                              bNode *node,
                              bNodeExecData *UNUSED(execdata),
                              GPUNodeStack *in,
                              GPUNodeStack *out)
{
  if (gpu_shader_get_name(node->custom1)) {
    return GPU_stack_link(mat, node, gpu_shader_get_name(node->custom1), in, out);
  }

  return 0;
}

static void node_shader_update_mapping(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *sock = nodeFindSocket(node, SOCK_IN, "Location");
  nodeSetSocketAvailability(
      ntree, sock, ELEM(node->custom1, NODE_MAPPING_TYPE_POINT, NODE_MAPPING_TYPE_TEXTURE));
}

}  // namespace blender::nodes::node_shader_mapping_cc

void register_node_type_sh_mapping()
{
  namespace file_ns = blender::nodes::node_shader_mapping_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_MAPPING, "Mapping", NODE_CLASS_OP_VECTOR);
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_mapping;
  node_type_gpu(&ntype, file_ns::gpu_shader_mapping);
  node_type_update(&ntype, file_ns::node_shader_update_mapping);

  nodeRegisterType(&ntype);
}
