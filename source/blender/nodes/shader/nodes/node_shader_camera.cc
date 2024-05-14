/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_camera_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Vector>("View Vector");
  b.add_output<decl::Float>("View Z Depth");
  b.add_output<decl::Float>("View Distance");
}

static int gpu_shader_camera(GPUMaterial *mat,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *in,
                             GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "camera", in, out);
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  /* NOTE: This node doesn't have an implementation in MaterialX. */
  return get_output_default(socket_out_->name, NodeItem::Type::Any);
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_camera_cc

void register_node_type_sh_camera()
{
  namespace file_ns = blender::nodes::node_shader_camera_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_CAMERA, "Camera Data", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::gpu_shader_camera;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::nodeRegisterType(&ntype);
}
