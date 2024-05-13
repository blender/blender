/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"

#include "FN_multi_function.hh"
#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

namespace blender::nodes::node_shader_value_cc {

static void sh_node_value_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>("Value");
}

static int gpu_shader_value(GPUMaterial *mat,
                            bNode *node,
                            bNodeExecData * /*execdata*/,
                            GPUNodeStack * /*in*/,
                            GPUNodeStack *out)
{
  const bNodeSocket *socket = static_cast<bNodeSocket *>(node->outputs.first);
  float value = static_cast<bNodeSocketValueFloat *>(socket->default_value)->value;
  return GPU_link(mat, "set_value", GPU_uniform(&value), &out->link);
}

static void sh_node_value_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const bNodeSocket *bsocket = (bNodeSocket *)builder.node().outputs.first;
  const bNodeSocketValueFloat *value = (const bNodeSocketValueFloat *)bsocket->default_value;
  builder.construct_and_set_matching_fn<mf::CustomMF_Constant<float>>(value->value);
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  NodeItem value = get_output_default("Value", NodeItem::Type::Float);
  return create_node("constant", NodeItem::Type::Float, {{"value", value}});
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_value_cc

void register_node_type_sh_value()
{
  namespace file_ns = blender::nodes::node_shader_value_cc;

  static blender::bke::bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_VALUE, "Value", NODE_CLASS_INPUT);
  ntype.declare = file_ns::sh_node_value_declare;
  ntype.gpu_fn = file_ns::gpu_shader_value;
  ntype.build_multi_function = file_ns::sh_node_value_build_multi_function;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::nodeRegisterType(&ntype);
}
