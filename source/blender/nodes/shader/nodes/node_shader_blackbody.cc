/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "FN_multi_function_builder.hh"
#include "NOD_multi_function.hh"
#include "node_shader_util.hh"

#include "BLI_color.hh"
#include "IMB_colormanagement.hh"

namespace blender::nodes::node_shader_blackbody_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Float>("Temperature").default_value(1500.0f).min(800.0f).max(12000.0f);
  b.add_output<decl::Color>("Color");
}

static int node_shader_gpu_blackbody(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData * /*execdata*/,
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  const int size = CM_TABLE + 1;
  float *data = static_cast<float *>(MEM_mallocN(sizeof(float) * size * 4, "blackbody texture"));

  IMB_colormanagement_blackbody_temperature_to_rgb_table(data, size, 800.0f, 12000.0f);

  float layer;
  GPUNodeLink *ramp_texture = GPU_color_band(mat, size, data, &layer);

  return GPU_stack_link(mat, node, "node_blackbody", in, out, ramp_texture, GPU_constant(&layer));
}

static void sh_node_blackbody_build_multi_function(nodes::NodeMultiFunctionBuilder &builder)
{
  static auto fn = mf::build::SI1_SO<float, ColorGeometry4f>("Blackbody", [](float temperature) {
    float color[4];
    IMB_colormanagement_blackbody_temperature_to_rgb(color, temperature);
    return ColorGeometry4f(color);
  });
  builder.set_matching_fn(fn);
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  NodeItem temperature = get_input_value("Temperature", NodeItem::Type::Float);

  NodeItem res = create_node("blackbody", NodeItem::Type::Color3);
  res.set_input("temperature", temperature);
  return res;
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_blackbody_cc

/* node type definition */
void register_node_type_sh_blackbody()
{
  namespace file_ns = blender::nodes::node_shader_blackbody_cc;

  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_BLACKBODY, "Blackbody", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::node_declare;
  blender::bke::node_type_size_preset(&ntype, blender::bke::eNodeSizePreset::Middle);
  ntype.gpu_fn = file_ns::node_shader_gpu_blackbody;
  ntype.build_multi_function = file_ns::sh_node_blackbody_build_multi_function;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  nodeRegisterType(&ntype);
}
