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

namespace blender::nodes::node_shader_eevee_specular_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Base Color")).default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_input<decl::Color>(N_("Specular")).default_value({0.03f, 0.03f, 0.03f, 1.0f});
  b.add_input<decl::Float>(N_("Roughness"))
      .default_value(0.2f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Color>(N_("Emissive Color")).default_value({0.0f, 0.0f, 0.0f, 1.0f});
  b.add_input<decl::Float>(N_("Transparency"))
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Vector>(N_("Normal")).hide_value();
  b.add_input<decl::Float>(N_("Clear Coat"))
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Clear Coat Roughness"))
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Vector>(N_("Clear Coat Normal")).hide_value();
  b.add_input<decl::Float>(N_("Ambient Occlusion")).hide_value();
  b.add_output<decl::Shader>(N_("BSDF"));
}

static int node_shader_gpu_eevee_specular(GPUMaterial *mat,
                                          bNode *node,
                                          bNodeExecData *UNUSED(execdata),
                                          GPUNodeStack *in,
                                          GPUNodeStack *out)
{
  static float one = 1.0f;

  /* Normals */
  if (!in[5].link) {
    GPU_link(mat, "world_normals_get", &in[5].link);
  }

  /* Clearcoat Normals */
  if (!in[8].link) {
    GPU_link(mat, "world_normals_get", &in[8].link);
  }

  /* Occlusion */
  if (!in[9].link) {
    GPU_link(mat, "set_value", GPU_constant(&one), &in[9].link);
  }

  GPU_material_flag_set(mat, static_cast<eGPUMatFlag>(GPU_MATFLAG_DIFFUSE | GPU_MATFLAG_GLOSSY));

  return GPU_stack_link(mat, node, "node_eevee_specular", in, out, GPU_constant(&node->ssr_id));
}

}  // namespace blender::nodes::node_shader_eevee_specular_cc

/* node type definition */
void register_node_type_sh_eevee_specular()
{
  namespace file_ns = blender::nodes::node_shader_eevee_specular_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_EEVEE_SPECULAR, "Specular BSDF", NODE_CLASS_SHADER);
  ntype.declare = file_ns::node_declare;
  node_type_gpu(&ntype, file_ns::node_shader_gpu_eevee_specular);

  nodeRegisterType(&ntype);
}
