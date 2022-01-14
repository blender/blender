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

#include "node_shader_util.hh"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes::node_shader_bsdf_hair_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Color")).default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_input<decl::Float>(N_("Offset"))
      .default_value(0.0f)
      .min(-M_PI_2)
      .max(M_PI_2)
      .subtype(PROP_ANGLE);
  b.add_input<decl::Float>(N_("RoughnessU"))
      .default_value(0.1f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("RoughnessV"))
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Vector>(N_("Tangent")).hide_value();
  b.add_output<decl::Shader>(N_("BSDF"));
}

static void node_shader_buts_hair(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "component", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

static int node_shader_gpu_bsdf_hair(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData *UNUSED(execdata),
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "node_bsdf_hair", in, out);
}

}  // namespace blender::nodes::node_shader_bsdf_hair_cc

/* node type definition */
void register_node_type_sh_bsdf_hair()
{
  namespace file_ns = blender::nodes::node_shader_bsdf_hair_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_BSDF_HAIR, "Hair BSDF", NODE_CLASS_SHADER);
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_hair;
  node_type_size(&ntype, 150, 60, 200);
  node_type_gpu(&ntype, file_ns::node_shader_gpu_bsdf_hair);

  nodeRegisterType(&ntype);
}
