/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "FN_multi_function_registry.hh"

#include "GPU_material.hh"

#include "node_function_util.hh"
#include "node_shader_util.hh"

namespace blender::nodes::node_fn_get_vector_component_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>("Vector"_ustr).optional_label();
  b.add_input<decl::Int>("Index"_ustr).min(0).max(2);
  b.add_output<decl::Float>("Value"_ustr);
}

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const mf::MultiFunction &fn = fn::multi_function::registry::lookup("float3[int]"_ustr);
  builder.set_matching_fn(&fn);
}

static int node_gpu_material(GPUMaterial *mat,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *in,
                             GPUNodeStack *out)
{
  /* EEVEE represents integer sockets as floats, while the compositor uses native integers. */
  const char *function_name = in[1].type == GPU_FLOAT ? "node_vector_component_float" :
                                                        "node_vector_component";
  return GPU_stack_link(mat, node, function_name, in, out);
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  NodeItem vector = get_input_value("Vector", NodeItem::Type::Vector3);
  NodeItem index =
      get_input_value("Index", NodeItem::Type::Integer).convert(NodeItem::Type::Float);

  return index.if_else(
      NodeItem::CompareOp::Eq,
      val(0.0f),
      vector[0],
      index.if_else(NodeItem::CompareOp::Eq,
                    val(1.0f),
                    vector[1],
                    index.if_else(NodeItem::CompareOp::Eq, val(2.0f), vector[2], val(0.0f))));
}
#endif
NODE_SHADER_MATERIALX_END

static void node_register()
{
  static bke::bNodeType ntype;

  common_node_type_base(&ntype, "FunctionNodeGetVectorComponent"_ustr);
  ntype.ui_name = "Get Vector Component";
  ntype.ui_description = "Get the X, Y or Z component of a vector by index";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.build_multi_function = node_build_multi_function;
  ntype.gpu_fn = node_gpu_material;
  ntype.materialx_fn = node_shader_materialx;
  ntype.default_width = bke::NodeWidth::_160;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_get_vector_component_cc
