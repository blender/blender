/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

#include "BLI_math_vector.h"

namespace blender::nodes::node_shader_bsdf_ray_portal_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Color").default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Vector>("Position").hide_value();
  b.add_input<decl::Vector>("Direction").hide_value();
  b.add_input<decl::Float>("Weight").unavailable();
  b.add_output<decl::Shader>("BSDF");
}

static int node_shader_gpu_bsdf_ray_portal(GPUMaterial *mat,
                                           bNode *node,
                                           bNodeExecData * /*execdata*/,
                                           GPUNodeStack *in,
                                           GPUNodeStack *out)
{
  if (in[0].link || !is_zero_v3(in[0].vec)) {
    GPU_material_flag_set(mat, GPU_MATFLAG_TRANSPARENT);
  }
  return GPU_stack_link(mat, node, "node_bsdf_ray_portal", in, out);
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  switch (to_type_) {
    case NodeItem::Type::BSDF: {
      NodeItem color = get_input_value("Color", NodeItem::Type::Color3);
      /* Returning diffuse node as BSDF component */
      return create_node("oren_nayar_diffuse_bsdf", NodeItem::Type::BSDF, {{"color", color}});
    }
    case NodeItem::Type::SurfaceOpacity: {
      NodeItem color = get_input_value("Color", NodeItem::Type::Color3);
      /* Returning: 1 - <average of color components> */
      return val(1.0f) - color.dotproduct(val(1.0f / 3.0f));
    }
    default:
      break;
  }
  return empty();
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_bsdf_ray_portal_cc

/* node type definition */
void register_node_type_sh_bsdf_ray_portal()
{
  namespace file_ns = blender::nodes::node_shader_bsdf_ray_portal_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_BSDF_RAY_PORTAL, "Ray Portal BSDF", NODE_CLASS_SHADER);
  ntype.add_ui_poll = object_shader_nodes_poll;
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::node_shader_gpu_bsdf_ray_portal;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  nodeRegisterType(&ntype);
}
