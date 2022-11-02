/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation. All rights reserved. */

#include "node_shader_util.hh"

#include "DNA_camera_types.h"

namespace blender::nodes::node_shader_virtual_camera_cc {

static void sh_node_virtual_camera_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>(N_("Vector")).implicit_field(implicit_field_inputs::position);
  b.add_output<decl::Color>(N_("Color")).no_muted_links();
  b.add_output<decl::Float>(N_("Alpha")).no_muted_links();
}

static int node_shader_gpu_virtual_camera(GPUMaterial *mat,
                                          bNode *node,
                                          bNodeExecData * /*execdata*/,
                                          GPUNodeStack *in,
                                          GPUNodeStack *out)
{
  Object *object = (Object *)node->id;
  if (object == nullptr || object->type != OB_CAMERA) {
    return GPU_stack_link(mat, node, "node_virtual_camera_empty", in, out);
  }

  Camera *cam = static_cast<Camera *>(object->data);
  const bool virtual_camera_stage = cam->runtime.virtual_camera_stage;
  if (virtual_camera_stage || cam->runtime.virtual_display_texture == nullptr) {
    return GPU_stack_link(mat, node, "node_virtual_camera_empty", in, out);
  }

  GPUNodeLink **texco = &in[0].link;
  if (!*texco) {
    *texco = GPU_attribute(mat, CD_AUTO_FROM_NAME, "");
    node_shader_gpu_bump_tex_coord(mat, node, texco);
  }
  node_shader_gpu_tex_mapping(mat, node, in, out);

  return GPU_stack_link(
      mat, node, "node_virtual_camera", in, out, GPU_image_camera(mat, cam, GPU_SAMPLER_DEFAULT));
}

}  // namespace blender::nodes::node_shader_virtual_camera_cc

void register_node_type_sh_virtual_camera()
{
  namespace file_ns = blender::nodes::node_shader_virtual_camera_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_VIRTUAL_CAMERA, "Virtual Camera", NODE_CLASS_TEXTURE);
  ntype.declare = file_ns::sh_node_virtual_camera_declare;
  node_type_gpu(&ntype, file_ns::node_shader_gpu_virtual_camera);

  nodeRegisterType(&ntype);
}
