/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_bsdf_metallic_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();

  b.add_output<decl::Shader>("BSDF");
  b.add_default_layout();

  b.add_input<decl::Color>("Base Color")
      .default_value({0.617f, 0.577f, 0.540f, 1.0f})
      .description("Color of the material");
  b.add_input<decl::Color>("Edge Tint")
      .default_value({0.695f, 0.726f, 0.770f, 1.0f})
      .description(
          "Tint reflection at near-grazing incidence to simulate complex index of refraction");
  b.add_input<decl::Vector>("IOR")
      .default_value({2.757f, 2.513f, 2.231f})
      .min(0.0f)
      .max(100.0f)
      .description("Real part of the conductor's refractive index, often called n");
  b.add_input<decl::Vector>("Extinction")
      .default_value({3.867f, 3.404f, 3.009f})
      .min(0.0f)
      .max(100.0f)
      .description("Imaginary part of the conductor's refractive index, often called k");
  b.add_input<decl::Float>("Roughness")
      .default_value(0.5f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "Microfacet roughness of the surface (0.0 is a perfect mirror reflection, 1.0 is "
          "completely rough)");
  b.add_input<decl::Float>("Anisotropy")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "Amount of anisotropy for reflection. Higher values give elongated highlights along the "
          "tangent direction");
  b.add_input<decl::Float>("Rotation")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description("Rotates the direction of anisotropy, with 1.0 going full circle");
  b.add_input<decl::Vector>("Normal").hide_value();
  b.add_input<decl::Vector>("Tangent").hide_value();
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

static void node_shader_buts_metallic(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "distribution", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  layout->prop(ptr, "fresnel_type", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

static void node_shader_init_metallic(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom1 = SHD_GLOSSY_MULTI_GGX;
  node->custom2 = SHD_CONDUCTOR_F82;
}

static int node_shader_gpu_bsdf_metallic(GPUMaterial *mat,
                                         bNode *node,
                                         bNodeExecData * /*execdata*/,
                                         GPUNodeStack *in,
                                         GPUNodeStack *out)
{
  float use_multi_scatter = (node->custom1 == SHD_GLOSSY_MULTI_GGX) ? 1.0f : 0.0f;
  float use_complex_ior = (node->custom2 == SHD_PHYSICAL_CONDUCTOR) ? 1.0f : 0.0f;

  if (!in[7].link) {
    GPU_link(mat, "world_normals_get", &in[7].link);
  }

  GPU_material_flag_set(mat, GPU_MATFLAG_GLOSSY);
  if (use_complex_ior == 0.0f) {
    if (in[0].might_be_tinted() || in[1].might_be_tinted()) {
      GPU_material_flag_set(mat, GPU_MATFLAG_REFLECTION_MAYBE_COLORED);
    }
  }
  else {
    if (in[2].might_be_tinted() || in[3].might_be_tinted()) {
      GPU_material_flag_set(mat, GPU_MATFLAG_REFLECTION_MAYBE_COLORED);
    }
  }

  return GPU_stack_link(mat,
                        node,
                        "node_bsdf_metallic",
                        in,
                        out,
                        GPU_constant(&use_multi_scatter),
                        GPU_constant(&use_complex_ior));
}

static void node_shader_update_metallic(bNodeTree *ntree, bNode *node)
{
  const bool is_physical = (node->custom2 == SHD_PHYSICAL_CONDUCTOR);

  bke::node_set_socket_availability(
      *ntree, *bke::node_find_socket(*node, SOCK_IN, "Base Color"), !is_physical);
  bke::node_set_socket_availability(
      *ntree, *bke::node_find_socket(*node, SOCK_IN, "Edge Tint"), !is_physical);
  bke::node_set_socket_availability(
      *ntree, *bke::node_find_socket(*node, SOCK_IN, "IOR"), is_physical);
  bke::node_set_socket_availability(
      *ntree, *bke::node_find_socket(*node, SOCK_IN, "Extinction"), is_physical);
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  if (to_type_ != NodeItem::Type::BSDF) {
    return empty();
  }

  NodeItem color = get_input_value("Base Color", NodeItem::Type::Color3);
  NodeItem edge_tint = get_input_value("Edge Tint", NodeItem::Type::Color3);
  NodeItem roughness = get_input_value("Roughness", NodeItem::Type::Vector2);
  NodeItem anisotropy = get_input_value("Anisotropy", NodeItem::Type::Color3);
  NodeItem normal = get_input_link("Normal", NodeItem::Type::Vector3);
  NodeItem tangent = get_input_link("Tangent", NodeItem::Type::Vector3);
  NodeItem thin_film_thickness = get_input_value("Thin Film Thickness", NodeItem::Type::Float);
  NodeItem thin_film_ior = get_input_value("Thin Film IOR", NodeItem::Type::Float);

  NodeItem ior_out, extinction_out;
  if (node_->custom2 == SHD_PHYSICAL_CONDUCTOR) {
    ior_out = get_input_value("IOR", NodeItem::Type::Color3);
    extinction_out = get_input_value("Extinction", NodeItem::Type::Color3);
  }
  else {
    NodeItem artistic_ior = create_node("artistic_ior",
                                        NodeItem::Type::Multioutput,
                                        {{"reflectivity", color}, {"edge_color", edge_tint}});
    ior_out = artistic_ior.add_output("ior", NodeItem::Type::Color3);
    extinction_out = artistic_ior.add_output("extinction", NodeItem::Type::Color3);
  }

  return create_node("conductor_bsdf",
                     NodeItem::Type::BSDF,
                     {{"normal", normal},
                      {"tangent", tangent},
                      {"ior", ior_out},
                      {"extinction", extinction_out},
                      {"roughness", roughness},
                      {"thinfilm_thickness", thin_film_thickness},
                      {"thinfilm_ior", thin_film_ior}});
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_bsdf_metallic_cc

/* node type definition */
void register_node_type_sh_bsdf_metallic()
{
  namespace file_ns = blender::nodes::node_shader_bsdf_metallic_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeBsdfMetallic", SH_NODE_BSDF_METALLIC);
  ntype.ui_name = "Metallic BSDF";
  ntype.ui_description = "Metallic reflection with microfacet distribution, and metallic fresnel";
  ntype.enum_name_legacy = "BSDF_METALLIC";
  ntype.nclass = NODE_CLASS_SHADER;
  ntype.declare = file_ns::node_declare;
  ntype.add_ui_poll = object_shader_nodes_poll;
  ntype.draw_buttons = file_ns::node_shader_buts_metallic;
  blender::bke::node_type_size_preset(ntype, blender::bke::eNodeSizePreset::Large);
  ntype.initfunc = file_ns::node_shader_init_metallic;
  ntype.gpu_fn = file_ns::node_shader_gpu_bsdf_metallic;
  ntype.updatefunc = file_ns::node_shader_update_metallic;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::node_register_type(ntype);
}
