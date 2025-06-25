/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "BKE_node_runtime.hh"

#include "NOD_multi_function.hh"

#include "GPU_material.hh"

#include "node_composite_util.hh"

/* **************** NORMAL  ******************** */

namespace blender::nodes::node_composite_normal_cc {

static void cmp_node_normal_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_output<decl::Vector>("Normal")
      .default_value({0.0f, 0.0f, 1.0f})
      .min(-1.0f)
      .max(1.0f)
      .subtype(PROP_DIRECTION);
}

using namespace blender::compositor;

/* The vector value is stored in the default value of the output socket. */
static float3 get_normal(const bNode &node)
{
  const bNodeSocket &normal_output = node.output_by_identifier("Normal");
  const float3 node_normal = normal_output.default_value_typed<bNodeSocketValueVector>()->value;
  return math::normalize(node_normal);
}

static int node_gpu_material(GPUMaterial *material,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack * /*inputs*/,
                             GPUNodeStack *outputs)
{
  const float3 normal = get_normal(*node);
  return GPU_link(material, "set_vector", GPU_uniform(normal), &outputs->link);
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  const float3 normal = get_normal(builder.node());
  builder.construct_and_set_matching_fn<mf::CustomMF_Constant<float3>>(normal);
}

}  // namespace blender::nodes::node_composite_normal_cc

static void register_node_type_cmp_normal()
{
  namespace file_ns = blender::nodes::node_composite_normal_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeNormal", CMP_NODE_NORMAL);
  ntype.ui_name = "Normal";
  ntype.ui_description = "Input normalized normal values to other nodes in the tree";
  ntype.enum_name_legacy = "NORMAL";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::cmp_node_normal_declare;
  ntype.gpu_fn = file_ns::node_gpu_material;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_normal)
