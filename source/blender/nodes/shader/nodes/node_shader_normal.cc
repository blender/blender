/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_normal_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>("Normal")
      .default_value({0.0f, 0.0f, 1.0f})
      .min(-1.0f)
      .max(1.0f)
      .subtype(PROP_DIRECTION)
      .description(
          "Normal direction vector.\n"
          "\u2022 LMB click and drag on the sphere to set the direction of the normal.\n"
          "\u2022 Holding Ctrl while dragging snaps to 45 degree rotation increments");
  b.add_output<decl::Vector>("Normal")
      .default_value({0.0f, 0.0f, 1.0f})
      .min(-1.0f)
      .max(1.0f)
      .subtype(PROP_DIRECTION);
  b.add_output<decl::Float>("Dot");
}

static int gpu_shader_normal(GPUMaterial *mat,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *in,
                             GPUNodeStack *out)
{
  GPUNodeLink *vec = GPU_uniform(out[0].vec);
  return GPU_stack_link(mat, node, "normal_new_shading", in, out, vec);
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  NodeItem res = get_output_default("Normal", NodeItem::Type::Vector3);

  if (STREQ(socket_out_->identifier, "Dot")) {
    return res.dotproduct(get_input_value("Normal", NodeItem::Type::Vector3));
  }

  return res;
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_normal_cc

void register_node_type_sh_normal()
{
  namespace file_ns = blender::nodes::node_shader_normal_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeNormal", SH_NODE_NORMAL);
  ntype.ui_name = "Normal";
  ntype.ui_description = "Generate a normal vector and a dot product";
  ntype.enum_name_legacy = "NORMAL";
  ntype.nclass = NODE_CLASS_OP_VECTOR;
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::gpu_shader_normal;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::node_register_type(ntype);
}
