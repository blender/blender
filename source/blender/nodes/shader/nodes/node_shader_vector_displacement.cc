/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_vector_displacement_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  /* FIXME The caption is Vector, but the input is a Color. Maybe we could name it Color Vector? */
  b.add_input<decl::Color>("Vector").hide_value();
  b.add_input<decl::Float>("Midlevel")
      .default_value(0.0f)
      .min(0.0f)
      .max(1000.0f)
      .description(
          "Neutral displacement value that causes no displacement.\n"
          "Lower values cause the surface to move inwards, "
          "higher values push the surface outwards");
  b.add_input<decl::Float>("Scale").default_value(1.0f).min(0.0f).max(1000.0f).description(
      "Increase or decrease the amount of displacement");
  b.add_output<decl::Vector>("Displacement");
}

static void node_shader_init_vector_displacement(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom1 = SHD_SPACE_TANGENT; /* space */
}

static int gpu_shader_vector_displacement(GPUMaterial *mat,
                                          bNode *node,
                                          bNodeExecData * /*execdata*/,
                                          GPUNodeStack *in,
                                          GPUNodeStack *out)
{
  switch (node->custom1) {
    case SHD_SPACE_TANGENT:
      return GPU_stack_link(mat,
                            node,
                            "node_vector_displacement_tangent",
                            in,
                            out,
                            GPU_attribute(mat, CD_TANGENT, ""));
    case SHD_SPACE_OBJECT:
      return GPU_stack_link(mat, node, "node_vector_displacement_object", in, out);
    case SHD_SPACE_WORLD:
    default:
      return GPU_stack_link(mat, node, "node_vector_displacement_world", in, out);
  }
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  /* NOTE: Mid-level input and Space feature don't have an implementation in MaterialX. */
  // NodeItem midlevel = get_input_value("midlevel", NodeItem::Type::Float);
  NodeItem vector = get_input_link("Vector", NodeItem::Type::Vector3);
  NodeItem scale = get_input_value("Scale", NodeItem::Type::Float);

  return create_node("displacement",
                     NodeItem::Type::DisplacementShader,
                     {{"displacement", vector}, {"scale", scale}});
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_vector_displacement_cc

/* node type definition */
void register_node_type_sh_vector_displacement()
{
  namespace file_ns = blender::nodes::node_shader_vector_displacement_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(
      &ntype, SH_NODE_VECTOR_DISPLACEMENT, "Vector Displacement", NODE_CLASS_OP_VECTOR);
  ntype.declare = file_ns::node_declare;
  ntype.initfunc = file_ns::node_shader_init_vector_displacement;
  ntype.gpu_fn = file_ns::gpu_shader_vector_displacement;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::nodeRegisterType(&ntype);
}
