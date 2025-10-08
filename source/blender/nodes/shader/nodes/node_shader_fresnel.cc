/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_fresnel_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("IOR").default_value(1.5f).min(0.0f).max(1000.0f);
  b.add_input<decl::Vector>("Normal").hide_value();
  b.add_output<decl::Float>("Factor", "Fac");
}

static int node_shader_gpu_fresnel(GPUMaterial *mat,
                                   bNode *node,
                                   bNodeExecData * /*execdata*/,
                                   GPUNodeStack *in,
                                   GPUNodeStack *out)
{
  if (!in[1].link) {
    GPU_link(mat, "world_normals_get", &in[1].link);
  }

  return GPU_stack_link(mat, node, "node_fresnel", in, out);
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  /* TODO: some outputs expected be implemented within the next iteration
   * (see node-definition `<artistic_ior>`). */
  return get_input_value("IOR", NodeItem::Type::Float);
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_fresnel_cc

/* node type definition */
void register_node_type_sh_fresnel()
{
  namespace file_ns = blender::nodes::node_shader_fresnel_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeFresnel", SH_NODE_FRESNEL);
  ntype.ui_name = "Fresnel";
  ntype.ui_description =
      "Produce a blending factor depending on the angle between the surface normal and the view "
      "direction using Fresnel equations.\nTypically used for mixing reflections at grazing "
      "angles";
  ntype.enum_name_legacy = "FRESNEL";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::node_shader_gpu_fresnel;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::node_register_type(ntype);
}
