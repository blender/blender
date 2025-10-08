/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_hair_info_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>("Is Strand");
#define INTERCEPT_SOCKET_INDEX 1
  b.add_output<decl::Float>("Intercept");
#define LENGTH_SOCKET_INDEX 2
  b.add_output<decl::Float>("Length");
  b.add_output<decl::Float>("Thickness");
  b.add_output<decl::Vector>("Tangent Normal");
  b.add_output<decl::Float>("Random");
}

static int node_shader_gpu_hair_info(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData * /*execdata*/,
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  /* Length: don't request length if not needed. */
  static const float zero = 0;
  GPUNodeLink *length_link = out[LENGTH_SOCKET_INDEX].hasoutput ? GPU_attribute_hair_length(mat) :
                                                                  GPU_constant(&zero);
  GPUNodeLink *intercept_link = out[INTERCEPT_SOCKET_INDEX].hasoutput ?
                                    GPU_attribute_hair_intercept(mat) :
                                    GPU_constant(&zero);
  return GPU_stack_link(mat, node, "node_hair_info", in, out, intercept_link, length_link);
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  /* NOTE: This node doesn't have an implementation in MaterialX. */
  return get_output_default(socket_out_->identifier, NodeItem::Type::Any);
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_hair_info_cc

/* node type definition */
void register_node_type_sh_hair_info()
{
  namespace file_ns = blender::nodes::node_shader_hair_info_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeHairInfo", SH_NODE_HAIR_INFO);
  ntype.ui_name = "Curves Info";
  ntype.ui_description = "Retrieve hair curve information";
  ntype.enum_name_legacy = "HAIR_INFO";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::node_shader_gpu_hair_info;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::node_register_type(ntype);
}
