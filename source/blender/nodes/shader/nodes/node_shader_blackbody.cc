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

namespace blender::nodes::node_shader_blackbody_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("Temperature")).default_value(1500.0f).min(800.0f).max(12000.0f);
  b.add_output<decl::Color>(N_("Color"));
}

static int node_shader_gpu_blackbody(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData *UNUSED(execdata),
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  const int size = CM_TABLE + 1;
  float *data = static_cast<float *>(MEM_mallocN(sizeof(float) * size * 4, "blackbody texture"));

  blackbody_temperature_to_rgb_table(data, size, 965.0f, 12000.0f);

  float layer;
  GPUNodeLink *ramp_texture = GPU_color_band(mat, size, data, &layer);

  return GPU_stack_link(mat, node, "node_blackbody", in, out, ramp_texture, GPU_constant(&layer));
}

}  // namespace blender::nodes::node_shader_blackbody_cc

/* node type definition */
void register_node_type_sh_blackbody()
{
  namespace file_ns = blender::nodes::node_shader_blackbody_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_BLACKBODY, "Blackbody", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::node_declare;
  node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
  node_type_gpu(&ntype, file_ns::node_shader_gpu_blackbody);

  nodeRegisterType(&ntype);
}
