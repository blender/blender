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

#include "BKE_scene.h"

namespace blender::nodes::node_shader_output_material_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Shader>(N_("Surface"));
  b.add_input<decl::Shader>(N_("Volume"));
  b.add_input<decl::Vector>(N_("Displacement")).hide_value();
}

static int node_shader_gpu_output_material(GPUMaterial *mat,
                                           bNode *node,
                                           bNodeExecData *UNUSED(execdata),
                                           GPUNodeStack *in,
                                           GPUNodeStack *out)
{
  GPUNodeLink *outlink, *alpha_threshold_link, *shadow_threshold_link;
  Material *ma = GPU_material_get_material(mat);

  static float no_alpha_threshold = -1.0f;
  if (ma) {
    alpha_threshold_link = GPU_uniform((ma->blend_method == MA_BM_CLIP) ? &ma->alpha_threshold :
                                                                          &no_alpha_threshold);
    shadow_threshold_link = GPU_uniform((ma->blend_shadow == MA_BS_CLIP) ? &ma->alpha_threshold :
                                                                           &no_alpha_threshold);
  }
  else {
    alpha_threshold_link = GPU_uniform(&no_alpha_threshold);
    shadow_threshold_link = GPU_uniform(&no_alpha_threshold);
  }

  GPU_stack_link(mat,
                 node,
                 "node_output_material",
                 in,
                 out,
                 alpha_threshold_link,
                 shadow_threshold_link,
                 &outlink);
  GPU_material_output_link(mat, outlink);

  return true;
}

}  // namespace blender::nodes::node_shader_output_material_cc

/* node type definition */
void register_node_type_sh_output_material()
{
  namespace file_ns = blender::nodes::node_shader_output_material_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_OUTPUT_MATERIAL, "Material Output", NODE_CLASS_OUTPUT);
  ntype.declare = file_ns::node_declare;
  node_type_gpu(&ntype, file_ns::node_shader_gpu_output_material);

  ntype.no_muting = true;

  nodeRegisterType(&ntype);
}
