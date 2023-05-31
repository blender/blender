/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

#include "IMB_colormanagement.h"

namespace blender::nodes::node_shader_wavelength_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Wavelength").default_value(500.0f).min(380.0f).max(780.0f);
  b.add_output<decl::Color>("Color");
}

static int node_shader_gpu_wavelength(GPUMaterial *mat,
                                      bNode *node,
                                      bNodeExecData * /*execdata*/,
                                      GPUNodeStack *in,
                                      GPUNodeStack *out)
{
  const int size = CM_TABLE + 1;
  float *data = static_cast<float *>(MEM_mallocN(sizeof(float) * size * 4, "cie_xyz texture"));

  IMB_colormanagement_wavelength_to_rgb_table(data, size);

  float layer;
  GPUNodeLink *ramp_texture = GPU_color_band(mat, size, data, &layer);
  return GPU_stack_link(mat, node, "node_wavelength", in, out, ramp_texture, GPU_constant(&layer));
}

}  // namespace blender::nodes::node_shader_wavelength_cc

/* node type definition */
void register_node_type_sh_wavelength()
{
  namespace file_ns = blender::nodes::node_shader_wavelength_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_WAVELENGTH, "Wavelength", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::node_declare;
  blender::bke::node_type_size_preset(&ntype, blender::bke::eNodeSizePreset::MIDDLE);
  ntype.gpu_fn = file_ns::node_shader_gpu_wavelength;

  nodeRegisterType(&ntype);
}
