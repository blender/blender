/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

#include "IMB_colormanagement.hh"

namespace blender::nodes::node_shader_wavelength_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Wavelength")
      .default_value(500.0f)
      .min(380.0f)
      .max(780.0f)
      .subtype(PROP_WAVELENGTH);
  b.add_output<decl::Color>("Color");
}

static int node_shader_gpu_wavelength(GPUMaterial *mat,
                                      bNode *node,
                                      bNodeExecData * /*execdata*/,
                                      GPUNodeStack *in,
                                      GPUNodeStack *out)
{
  const int size = CM_TABLE + 1;
  float *data = MEM_malloc_arrayN<float>(size * 4, "cie_xyz texture");

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

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeWavelength", SH_NODE_WAVELENGTH);
  ntype.ui_name = "Wavelength";
  ntype.ui_description = "Convert a wavelength value to an RGB value";
  ntype.enum_name_legacy = "WAVELENGTH";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = file_ns::node_declare;
  blender::bke::node_type_size_preset(ntype, blender::bke::eNodeSizePreset::Middle);
  ntype.gpu_fn = file_ns::node_shader_gpu_wavelength;

  blender::bke::node_register_type(ntype);
}
