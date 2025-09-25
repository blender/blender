/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_bsdf_refraction_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Color").default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>("Roughness")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>("IOR").default_value(1.45f).min(0.0f).max(1000.0f);
  b.add_input<decl::Vector>("Normal").hide_value();
  b.add_input<decl::Float>("Weight").available(false);
  b.add_output<decl::Shader>("BSDF");
}

static void node_shader_init_refraction(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom1 = SHD_GLOSSY_BECKMANN;
}

static int node_shader_gpu_bsdf_refraction(GPUMaterial *mat,
                                           bNode *node,
                                           bNodeExecData * /*execdata*/,
                                           GPUNodeStack *in,
                                           GPUNodeStack *out)
{
  if (!in[3].link) {
    GPU_link(mat, "world_normals_get", &in[3].link);
  }

  GPU_material_flag_set(mat, GPU_MATFLAG_REFRACT);
  if (in[0].might_be_tinted()) {
    GPU_material_flag_set(mat, GPU_MATFLAG_REFRACTION_MAYBE_COLORED);
  }

  return GPU_stack_link(mat, node, "node_bsdf_refraction", in, out);
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  if (to_type_ != NodeItem::Type::BSDF) {
    return empty();
  }

  NodeItem color = get_input_value("Color", NodeItem::Type::Color3);
  NodeItem roughness = get_input_value("Roughness", NodeItem::Type::Vector2);
  NodeItem ior = get_input_value("IOR", NodeItem::Type::Float);
  NodeItem normal = get_input_link("Normal", NodeItem::Type::Vector3);

  return create_node("dielectric_bsdf",
                     NodeItem::Type::BSDF,
                     {{"normal", normal},
                      {"tint", color},
                      {"roughness", roughness},
                      {"ior", ior},
                      {"scatter_mode", val(std::string("T"))}});
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_bsdf_refraction_cc

/* node type definition */
void register_node_type_sh_bsdf_refraction()
{
  namespace file_ns = blender::nodes::node_shader_bsdf_refraction_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeBsdfRefraction", SH_NODE_BSDF_REFRACTION);
  ntype.ui_name = "Refraction BSDF";
  ntype.ui_description =
      "Glossy refraction with sharp or microfacet distribution, typically used for materials that "
      "transmit light";
  ntype.enum_name_legacy = "BSDF_REFRACTION";
  ntype.nclass = NODE_CLASS_SHADER;
  ntype.declare = file_ns::node_declare;
  ntype.add_ui_poll = object_shader_nodes_poll;
  blender::bke::node_type_size_preset(ntype, blender::bke::eNodeSizePreset::Middle);
  ntype.initfunc = file_ns::node_shader_init_refraction;
  ntype.gpu_fn = file_ns::node_shader_gpu_bsdf_refraction;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::node_register_type(ntype);
}
