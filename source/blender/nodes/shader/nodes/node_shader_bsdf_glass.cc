/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_bsdf_glass_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();

  b.add_output<decl::Shader>("BSDF");

  b.add_default_layout();

  b.add_input<decl::Color>("Color").default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>("Roughness")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>("IOR").default_value(1.5f).min(0.0f).max(1000.0f);
  b.add_input<decl::Vector>("Normal").hide_value();
  b.add_input<decl::Float>("Weight").available(false);

  PanelDeclarationBuilder &film = b.add_panel("Thin Film").default_closed(true);
  film.add_input<decl::Float>("Thin Film Thickness")
      .default_value(0.0)
      .min(0.0f)
      .max(100000.0f)
      .subtype(PROP_WAVELENGTH)
      .description("Thickness of the film in nanometers");
  film.add_input<decl::Float>("Thin Film IOR")
      .default_value(1.33f)
      .min(1.0f)
      .max(1000.0f)
      .description("Index of refraction (IOR) of the thin film");
}

static void node_shader_buts_glass(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "distribution", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

static void node_shader_init_glass(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom1 = SHD_GLOSSY_MULTI_GGX;
}

static int node_shader_gpu_bsdf_glass(GPUMaterial *mat,
                                      bNode *node,
                                      bNodeExecData * /*execdata*/,
                                      GPUNodeStack *in,
                                      GPUNodeStack *out)
{
  if (!in[3].link) {
    GPU_link(mat, "world_normals_get", &in[3].link);
  }

  GPU_material_flag_set(mat, GPU_MATFLAG_GLOSSY | GPU_MATFLAG_REFRACT);

  if (in[0].might_be_tinted()) {
    GPU_material_flag_set(
        mat, GPU_MATFLAG_REFLECTION_MAYBE_COLORED | GPU_MATFLAG_REFRACTION_MAYBE_COLORED);
  }

  float use_multi_scatter = (node->custom1 == SHD_GLOSSY_MULTI_GGX) ? 1.0f : 0.0f;

  return GPU_stack_link(mat, node, "node_bsdf_glass", in, out, GPU_constant(&use_multi_scatter));
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
  NodeItem thin_film_thickness = get_input_value("Thin Film Thickness", NodeItem::Type::Float);
  NodeItem thin_film_ior = get_input_value("Thin Film IOR", NodeItem::Type::Float);

  return create_node("dielectric_bsdf",
                     NodeItem::Type::BSDF,
                     {{"normal", normal},
                      {"tint", color},
                      {"roughness", roughness},
                      {"ior", ior},
                      {"thinfilm_thickness", thin_film_thickness},
                      {"thinfilm_ior", thin_film_ior},
                      {"scatter_mode", val(std::string("RT"))}});
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_bsdf_glass_cc

/* node type definition */
void register_node_type_sh_bsdf_glass()
{
  namespace file_ns = blender::nodes::node_shader_bsdf_glass_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeBsdfGlass", SH_NODE_BSDF_GLASS);
  ntype.ui_name = "Glass BSDF";
  ntype.ui_description = "Glass-like shader mixing refraction and reflection at grazing angles";
  ntype.enum_name_legacy = "BSDF_GLASS";
  ntype.nclass = NODE_CLASS_SHADER;
  ntype.declare = file_ns::node_declare;
  ntype.add_ui_poll = object_shader_nodes_poll;
  blender::bke::node_type_size_preset(ntype, blender::bke::eNodeSizePreset::Middle);
  ntype.draw_buttons = file_ns::node_shader_buts_glass;
  ntype.initfunc = file_ns::node_shader_init_glass;
  ntype.gpu_fn = file_ns::node_shader_gpu_bsdf_glass;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::node_register_type(ntype);
}
