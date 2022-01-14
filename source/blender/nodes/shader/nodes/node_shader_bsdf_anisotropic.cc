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

namespace blender::nodes::node_shader_bsdf_anisotropic_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Color")).default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_input<decl::Float>(N_("Roughness"))
      .default_value(0.5f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Anisotropy")).default_value(0.5f).min(-1.0f).max(1.0f);
  b.add_input<decl::Float>(N_("Rotation"))
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Vector>(N_("Normal")).hide_value();
  b.add_input<decl::Vector>(N_("Tangent")).hide_value();
  b.add_output<decl::Shader>(N_("BSDF"));
}

static void node_shader_buts_anisotropic(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "distribution", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

static void node_shader_init_anisotropic(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->custom1 = SHD_GLOSSY_GGX;
}

static int node_shader_gpu_bsdf_anisotropic(GPUMaterial *mat,
                                            bNode *node,
                                            bNodeExecData *UNUSED(execdata),
                                            GPUNodeStack *in,
                                            GPUNodeStack *out)
{
  if (!in[4].link) {
    GPU_link(mat, "world_normals_get", &in[4].link);
  }

  GPU_material_flag_set(mat, GPU_MATFLAG_GLOSSY);

  float use_multi_scatter = (node->custom1 == SHD_GLOSSY_MULTI_GGX) ? 1.0f : 0.0f;

  return GPU_stack_link(mat,
                        node,
                        "node_bsdf_anisotropic",
                        in,
                        out,
                        GPU_constant(&use_multi_scatter),
                        GPU_constant(&node->ssr_id));
}

}  // namespace blender::nodes::node_shader_bsdf_anisotropic_cc

/* node type definition */
void register_node_type_sh_bsdf_anisotropic()
{
  namespace file_ns = blender::nodes::node_shader_bsdf_anisotropic_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_BSDF_ANISOTROPIC, "Anisotropic BSDF", NODE_CLASS_SHADER);
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_anisotropic;
  node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
  node_type_init(&ntype, file_ns::node_shader_init_anisotropic);
  node_type_gpu(&ntype, file_ns::node_shader_gpu_bsdf_anisotropic);

  nodeRegisterType(&ntype);
}
