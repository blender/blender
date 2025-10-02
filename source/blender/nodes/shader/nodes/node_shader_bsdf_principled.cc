/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <map>

#include "node_shader_util.hh"

#include "BLI_math_base.h"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "BKE_node_runtime.hh"

namespace blender::nodes::node_shader_bsdf_principled_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  /**
   * Define static socket numbers to avoid string based lookups for GPU material creation as these
   * could run on animated materials.
   */

  b.use_custom_socket_order();

  b.add_output<decl::Shader>("BSDF");

  b.add_input<decl::Color>("Base Color")
      .default_value({0.8f, 0.8f, 0.8f, 1.0f})
      .description(
          "Color of the material used for diffuse, subsurface, metallic and transmission");
#define SOCK_BASE_COLOR_ID 0
  b.add_input<decl::Float>("Metallic")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "Blends between a dielectric and metallic material model. "
          "At 0.0 the material consists of a diffuse or transmissive base layer, "
          "with a specular reflection layer on top. A value of 1.0 gives a fully specular "
          "reflection tinted with the base color, without diffuse reflection or transmission");
#define SOCK_METALLIC_ID 1
  b.add_input<decl::Float>("Roughness")
      .default_value(0.5f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "Specifies microfacet roughness of the surface for specular reflection and transmission"
          " (0.0 is a perfect mirror reflection, 1.0 is completely rough)");
#define SOCK_ROUGHNESS_ID 2
  b.add_input<decl::Float>("IOR").default_value(1.5f).min(1.0f).max(1000.0f).description(
      "Index of Refraction (IOR) for specular reflection and transmission. "
      "For most materials, the IOR is between 1.0 (vacuum and air) and 4.0 (germanium). "
      "The default value of 1.5 is a good approximation for glass");
#define SOCK_IOR_ID 3
  b.add_input<decl::Float>("Alpha")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description("Controls the transparency of the surface, with 1.0 fully opaque");
#define SOCK_ALPHA_ID 4
  b.add_input<decl::Vector>("Normal").hide_value();
#define SOCK_NORMAL_ID 5
  b.add_input<decl::Float>("Weight").available(false);
#define SOCK_WEIGHT_ID 6

  /* Panel for Diffuse settings. */
  PanelDeclarationBuilder &diffuse = b.add_panel("Diffuse").default_closed(true);
  diffuse.add_input<decl::Float>("Diffuse Roughness")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "Specifies microfacet roughness of the diffuse base"
          " (0.0 is perfect lambertian reflection, 1.0 is completely rough)");
#define SOCK_DIFFUSE_ROUGHNESS_ID 7

  /* Panel for Subsurface scattering settings. */
  PanelDeclarationBuilder &sss = b.add_panel("Subsurface").default_closed(true);
  sss.add_layout([](uiLayout *layout, bContext * /*C*/, PointerRNA *ptr) {
    layout->prop(ptr, "subsurface_method", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  });
  sss.add_input<decl::Float>("Subsurface Weight")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .short_label("Weight")
      .description(
          "Blend between diffuse surface and subsurface scattering. "
          "Typically should be zero or one (either fully diffuse or subsurface)");
#define SOCK_SUBSURFACE_WEIGHT_ID 8
  sss.add_input<decl::Vector>("Subsurface Radius")
      .default_value({1.0f, 0.2f, 0.1f})
      .min(0.0f)
      .max(100.0f)
      .short_label("Radius")
      .description("Scattering radius per color channel (RGB), multiplied with Scale");
#define SOCK_SUBSURFACE_RADIUS_ID 9
  sss.add_input<decl::Float>("Subsurface Scale")
      .default_value(0.05f)
      .min(0.0f)
      .max(10.0f)
      .subtype(PROP_DISTANCE)
      .short_label("Scale")
      .description("Scale factor of the subsurface scattering radius");
#define SOCK_SUBSURFACE_SCALE_ID 10
  sss.add_input<decl::Float>("Subsurface IOR")
      .default_value(1.4f)
      .min(1.01f)
      .max(3.8f)
      .subtype(PROP_FACTOR)
      .short_label("IOR")
      .description("Index of Refraction (IOR) used for rays that enter the subsurface component");
#define SOCK_SUBSURFACE_IOR_ID 11
  sss.add_input<decl::Float>("Subsurface Anisotropy")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .short_label("Anisotropy")
      .description(
          "Directionality of volume scattering within the subsurface medium. "
          "Zero scatters uniformly in all directions, with higher values "
          "scattering more strongly forward. For example, skin has been measured "
          "to have an anisotropy of 0.8");
#define SOCK_SUBSURFACE_ANISOTROPY_ID 12

  /* Panel for Specular settings. */
  PanelDeclarationBuilder &spec = b.add_panel("Specular").default_closed(true);
  spec.add_layout([](uiLayout *layout, bContext * /*C*/, PointerRNA *ptr) {
    layout->prop(ptr, "distribution", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  });
  spec.add_input<decl::Float>("Specular IOR Level")
      .default_value(0.5f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .short_label("IOR Level")
      .description(
          "Adjustment to the Index of Refraction (IOR) to increase or decrease specular intensity "
          "(0.5 means no adjustment, 0 removes all reflections, 1 doubles them at normal "
          "incidence)");
#define SOCK_SPECULAR_ID 13
  spec.add_input<decl::Color>("Specular Tint")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .short_label("Tint")
      .description(
          "Tint dielectric reflection at normal incidence for artistic control, and metallic "
          "reflection at near-grazing incidence to simulate complex index of refraction")
      .translation_context(BLT_I18NCONTEXT_ID_NODETREE);
#define SOCK_SPECULAR_TINT_ID 14
  spec.add_input<decl::Float>("Anisotropic")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "Amount of anisotropy for specular reflection. "
          "Higher values give elongated highlights along the tangent direction");
#define SOCK_ANISOTROPIC_ID 15
  spec.add_input<decl::Float>("Anisotropic Rotation")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description("Rotates the direction of anisotropy, with 1.0 going full circle");
#define SOCK_ANISOTROPIC_ROTATION_ID 16
  spec.add_input<decl::Vector>("Tangent").hide_value().description(
      "Controls the tangent direction for anisotropy");
#define SOCK_TANGENT_ID 17

  /* Panel for Transmission settings. */
  PanelDeclarationBuilder &transmission = b.add_panel("Transmission").default_closed(true);
  transmission.add_input<decl::Float>("Transmission Weight")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .short_label("Weight")
      .description("Blend between transmission and other base layer components");
#define SOCK_TRANSMISSION_WEIGHT_ID 18

  /* Panel for Coat settings. */
  PanelDeclarationBuilder &coat = b.add_panel("Coat").default_closed(true);
  coat.add_input<decl::Float>("Coat Weight")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .short_label("Weight")
      .description(
          "Controls the intensity of the coat layer, both the reflection and the tinting. "
          "Typically should be zero or one for physically-based materials");
#define SOCK_COAT_WEIGHT_ID 19
  coat.add_input<decl::Float>("Coat Roughness")
      .default_value(0.03f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .short_label("Roughness")
      .description("The roughness of the coat layer");
#define SOCK_COAT_ROUGHNESS_ID 20
  coat.add_input<decl::Float>("Coat IOR")
      .default_value(1.5f)
      .min(1.0f)
      .max(4.0f)
      .short_label("IOR")
      .description(
          "The Index of Refraction (IOR) of the coat layer "
          "(affects its reflectivity as well as the falloff of coat tinting)");
#define SOCK_COAT_IOR_ID 21
  coat.add_input<decl::Color>("Coat Tint")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .short_label("Tint")
      .description(
          "Adds a colored tint to the coat layer by modeling absorption in the layer. "
          "Saturation increases at shallower angles, as the light travels farther "
          "through the medium (depending on the Coat IOR)")
      .translation_context(BLT_I18NCONTEXT_ID_NODETREE);
#define SOCK_COAT_TINT_ID 22
  coat.add_input<decl::Vector>("Coat Normal").short_label("Normal").hide_value();
#define SOCK_COAT_NORMAL_ID 23

  /* Panel for Sheen settings. */
  PanelDeclarationBuilder &sheen = b.add_panel("Sheen").default_closed(true);
  sheen.add_input<decl::Float>("Sheen Weight")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .short_label("Weight")
      .description(
          "Intensity of the sheen layer, which simulates very small fibers on the surface");
#define SOCK_SHEEN_WEIGHT_ID 24
  sheen.add_input<decl::Float>("Sheen Roughness")
      .default_value(0.5f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .short_label("Roughness")
      .description(
          "Roughness of the sheen layer. Low and high roughness values produce fuzzy or dusty "
          "appearance, respectively");
#define SOCK_SHEEN_ROUGHNESS_ID 25
  sheen.add_input<decl::Color>("Sheen Tint")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .translation_context(BLT_I18NCONTEXT_ID_NODETREE)
      .short_label("Tint")
      .description("Color of the sheen reflection");
#define SOCK_SHEEN_TINT_ID 26

  /* Panel for Emission settings. */
  PanelDeclarationBuilder &emis = b.add_panel("Emission").default_closed(true);
  emis.add_input<decl::Color>("Emission Color")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .short_label("Color")
      .description("Color of light emission from the surface");
#define SOCK_EMISSION_ID 27
  emis.add_input<decl::Float>("Emission Strength")
      .default_value(0.0)
      .min(0.0f)
      .max(1000000.0f)
      .short_label("Strength")
      .description(
          "Strength of the emitted light. A value of 1.0 ensures "
          "that the object in the image has the exact same color as the Emission Color")
      .translation_context(BLT_I18NCONTEXT_AMOUNT);
#define SOCK_EMISSION_STRENGTH_ID 28

  /* Panel for Thin Film settings. */
  PanelDeclarationBuilder &film = b.add_panel("Thin Film").default_closed(true);
  film.add_input<decl::Float>("Thin Film Thickness")
      .default_value(0.0)
      .min(0.0f)
      .max(100000.0f)
      .subtype(PROP_WAVELENGTH)
      .description("Thickness of the film in nanometers");
#define SOCK_THIN_FILM_THICKNESS_ID 29
  film.add_input<decl::Float>("Thin Film IOR")
      .default_value(1.33f)
      .min(1.0f)
      .max(1000.0f)
      .description("Index of refraction (IOR) of the thin film");
#define SOCK_THIN_FILM_IOR_ID 30
}

static void node_shader_init_principled(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom1 = SHD_GLOSSY_MULTI_GGX;
  node->custom2 = SHD_SUBSURFACE_RANDOM_WALK;
}

static bool might_have_tinted_specular(const GPUNodeStack &base_color,
                                       const GPUNodeStack &metallic,
                                       const GPUNodeStack &specular_tint)
{
  if (metallic.socket_not_zero()) {
    /* Metals might have colored specular. */
    return base_color.might_be_tinted() || specular_tint.might_be_tinted();
  }
  /* Dielectrics get colored if tint is used. */
  return specular_tint.might_be_tinted();
}

static int node_shader_gpu_bsdf_principled(GPUMaterial *mat,
                                           bNode *node,
                                           bNodeExecData * /*execdata*/,
                                           GPUNodeStack *in,
                                           GPUNodeStack *out)
{
  /* Normals */
  if (!in[SOCK_NORMAL_ID].link) {
    GPU_link(mat, "world_normals_get", &in[SOCK_NORMAL_ID].link);
  }

  /* Coat Normals */
  if (!in[SOCK_COAT_NORMAL_ID].link) {
    GPU_link(mat, "world_normals_get", &in[SOCK_COAT_NORMAL_ID].link);
  }

#if 0 /* Not used at the moment. */
  /* Tangents */
  if (!in[SOCK_TANGENT_ID].link) {
    GPUNodeLink *orco = GPU_attribute(CD_ORCO, "");
    GPU_link(mat, "tangent_orco_z", orco, &in[SOCK_TANGENT_ID].link);
    GPU_link(mat, "node_tangent", in[SOCK_TANGENT_ID].link, &in[SOCK_TANGENT_ID].link);
  }
#endif

  bool use_diffuse = in[SOCK_SHEEN_WEIGHT_ID].socket_not_zero() ||
                     (in[SOCK_METALLIC_ID].socket_not_one() &&
                      in[SOCK_TRANSMISSION_WEIGHT_ID].socket_not_one());
  bool use_subsurf = in[SOCK_SUBSURFACE_WEIGHT_ID].socket_not_zero() && use_diffuse;
  bool use_refract = in[SOCK_METALLIC_ID].socket_not_one() &&
                     in[SOCK_TRANSMISSION_WEIGHT_ID].socket_not_zero();
  bool use_transparency = in[SOCK_ALPHA_ID].socket_not_one();
  bool use_coat = in[SOCK_COAT_WEIGHT_ID].socket_not_zero();

  eGPUMaterialFlag flag = GPU_MATFLAG_GLOSSY;
  if (use_diffuse) {
    flag |= GPU_MATFLAG_DIFFUSE;
  }
  if (use_refract) {
    flag |= GPU_MATFLAG_REFRACT;
  }
  if (use_subsurf) {
    flag |= GPU_MATFLAG_SUBSURFACE;
  }
  if (use_transparency) {
    flag |= GPU_MATFLAG_TRANSPARENT;
  }
  if (use_coat) {
    flag |= GPU_MATFLAG_COAT;
  }

  if (might_have_tinted_specular(
          in[SOCK_BASE_COLOR_ID], in[SOCK_METALLIC_ID], in[SOCK_SPECULAR_TINT_ID]))
  {
    flag |= GPU_MATFLAG_REFLECTION_MAYBE_COLORED;
  }
  if (use_refract && in[SOCK_BASE_COLOR_ID].might_be_tinted()) {
    flag |= GPU_MATFLAG_REFRACTION_MAYBE_COLORED;
  }
  if (use_coat && in[SOCK_COAT_TINT_ID].might_be_tinted()) {
    flag |= GPU_MATFLAG_REFLECTION_MAYBE_COLORED;
  }

  GPU_material_flag_set(mat, flag);

  /* Make constant link for the cases we optimize. This allows the driver to constant fold.
   * Note that doing so specialize the final tree topology, and thus the shader becomes less
   * reusable. So to be used with care.
   * Also note that we do note override existing links. This is because it would leak the current
   * nodes otherwise. */
  const float zero = 0.0f;
  if (!use_coat && in[SOCK_COAT_WEIGHT_ID].link == nullptr) {
    in[SOCK_COAT_WEIGHT_ID].link = GPU_constant(&zero);
  }
  if (!use_subsurf && in[SOCK_SUBSURFACE_WEIGHT_ID].link == nullptr) {
    in[SOCK_SUBSURFACE_WEIGHT_ID].link = GPU_constant(&zero);
  }
  if (!use_refract && in[SOCK_TRANSMISSION_WEIGHT_ID].link == nullptr) {
    in[SOCK_TRANSMISSION_WEIGHT_ID].link = GPU_constant(&zero);
  }

  float use_multi_scatter = (node->custom1 == SHD_GLOSSY_MULTI_GGX) ? 1.0f : 0.0f;

  return GPU_stack_link(
      mat, node, "node_bsdf_principled", in, out, GPU_constant(&use_multi_scatter));
}

static void node_shader_update_principled(bNodeTree *ntree, bNode *node)
{
  const int sss_method = node->custom2;

  bke::node_set_socket_availability(*ntree,
                                    *bke::node_find_socket(*node, SOCK_IN, "Subsurface IOR"),
                                    sss_method == SHD_SUBSURFACE_RANDOM_WALK_SKIN);
  bke::node_set_socket_availability(
      *ntree,
      *bke::node_find_socket(*node, SOCK_IN, "Subsurface Anisotropy"),
      sss_method != SHD_SUBSURFACE_BURLEY);
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  using InputsType = std::map<std::string, NodeItem>;

  /* NOTE: commented inputs aren't used for node creation. */
  auto bsdf_inputs = [&]() -> InputsType {
    return {
        {"base_color", get_input_value("Base Color", NodeItem::Type::Color3)},
        {"diffuse_roughness", get_input_value("Diffuse Roughness", NodeItem::Type::Float)},
        {"subsurface", get_input_value("Subsurface Weight", NodeItem::Type::Float)},
        {"subsurface_scale", get_input_value("Subsurface Scale", NodeItem::Type::Float)},
#  if MATERIALX_MAJOR_VERSION <= 1 && MATERIALX_MINOR_VERSION <= 38
        {"subsurface_radius", get_input_value("Subsurface Radius", NodeItem::Type::Vector3)},
#  else
        {"subsurface_radius", get_input_value("Subsurface Radius", NodeItem::Type::Color3)},
#  endif
        //{"subsurface_ior", get_input_value("Subsurface IOR", NodeItem::Type::Vector3)},
        {"subsurface_anisotropy", get_input_value("Subsurface Anisotropy", NodeItem::Type::Float)},
        {"metallic", get_input_value("Metallic", NodeItem::Type::Float)},
        {"specular", get_input_value("Specular IOR Level", NodeItem::Type::Float)},
        {"specular_tint", get_input_value("Specular Tint", NodeItem::Type::Color3)},
        {"roughness", get_input_value("Roughness", NodeItem::Type::Float)},
        {"anisotropic", get_input_value("Anisotropic", NodeItem::Type::Float)},
        {"anisotropic_rotation", get_input_value("Anisotropic Rotation", NodeItem::Type::Float)},
        {"sheen", get_input_value("Sheen Weight", NodeItem::Type::Float)},
        {"sheen_roughness", get_input_value("Sheen Roughness", NodeItem::Type::Float)},
        {"sheen_tint", get_input_value("Sheen Tint", NodeItem::Type::Color3)},
        {"coat", get_input_value("Coat Weight", NodeItem::Type::Float)},
        {"coat_roughness", get_input_value("Coat Roughness", NodeItem::Type::Float)},
        {"coat_ior", get_input_value("Coat IOR", NodeItem::Type::Float)},
        {"coat_tint", get_input_value("Coat Tint", NodeItem::Type::Color3)},
        {"ior", get_input_value("IOR", NodeItem::Type::Float)},
        {"transmission", get_input_value("Transmission Weight", NodeItem::Type::Float)},
        {"thin_film_thickness", get_input_value("Thin Film Thickness", NodeItem::Type::Float)},
        {"thin_film_IOR", get_input_value("Thin Film IOR", NodeItem::Type::Float)},
        {"alpha", get_input_value("Alpha", NodeItem::Type::Float)},
        {"normal", get_input_link("Normal", NodeItem::Type::Vector3)},
        {"coat_normal", get_input_link("Coat Normal", NodeItem::Type::Vector3)},
        {"tangent", get_input_link("Tangent", NodeItem::Type::Vector3)},
    };
  };

  auto edf_inputs = [&]() -> InputsType {
    return {
        {"emission", get_input_value("Emission Strength", NodeItem::Type::Float)},
        {"emission_color", get_input_value("Emission Color", NodeItem::Type::Color3)},
    };
  };

  NodeItem res = empty();

  switch (to_type_) {
    case NodeItem::Type::BSDF: {
      auto in = bsdf_inputs();

      NodeItem roughness = in["roughness"];
      NodeItem diffuse_roughness = in["diffuse_roughness"];
      NodeItem anisotropy = in["anisotropic"];
      NodeItem rotation = in["anisotropic_rotation"] * val(360.0f);
      NodeItem base_color = in["base_color"];
      NodeItem specular = in["specular"];
      NodeItem coat = in["coat"];
      NodeItem ior = in["ior"];
      NodeItem normal = in["normal"];
      NodeItem tangent = in["tangent"];
      NodeItem coat_normal = in["coat_normal"];

      NodeItem n_main_tangent = empty();
      if (tangent && normal) {
        NodeItem n_tangent_rotate_normalize = tangent.rotate(rotation, normal).normalize();
        n_main_tangent = anisotropy.if_else(
            NodeItem::CompareOp::Greater, val(0.0f), n_tangent_rotate_normalize, tangent);
      }

      NodeItem n_coat_roughness_vector = create_node(
          "roughness_anisotropy",
          NodeItem::Type::Vector2,
          {{"roughness", in["coat_roughness"]}, {"anisotropy", anisotropy}});

      NodeItem n_coat_bsdf = create_node("dielectric_bsdf",
                                         NodeItem::Type::BSDF,
                                         {{"weight", coat},
                                          {"tint", in["coat_tint"]},
                                          {"ior", in["coat_ior"]},
                                          {"scatter_mode", val(std::string("R"))},
                                          {"roughness", n_coat_roughness_vector},
                                          {"normal", coat_normal}});

      if (tangent && coat_normal) {
        NodeItem n_coat_tangent_rotate_normalize =
            tangent.rotate(rotation, coat_normal).normalize();
        NodeItem n_coat_tangent = anisotropy.if_else(
            NodeItem::CompareOp::Greater, val(0.0f), n_coat_tangent_rotate_normalize, tangent);

        n_coat_bsdf.set_input("tangent", n_coat_tangent);
      }

      NodeItem thin_film_thickness = in["thin_film_thickness"];
      NodeItem thin_film_ior = in["thin_film_IOR"];
      NodeItem n_thin_film_bsdf = create_node(
          "thin_film_bsdf",
          NodeItem::Type::BSDF,
          {{"thickness", thin_film_thickness}, {"ior", thin_film_ior}});

      NodeItem n_artistic_ior = create_node(
          "artistic_ior",
          NodeItem::Type::Multioutput,
          {{"reflectivity", base_color * val(1.0f)}, {"edge_color", base_color * specular}});

      NodeItem n_ior_out = n_artistic_ior.add_output("ior", NodeItem::Type::Color3);
      NodeItem n_extinction_out = n_artistic_ior.add_output("extinction", NodeItem::Type::Color3);

      NodeItem n_coat_affect_roughness_multiply2 = coat * val(0.0f) * in["coat_roughness"];
      NodeItem n_coat_affected_roughness = n_coat_affect_roughness_multiply2.mix(roughness,
                                                                                 val(1.0f));

      NodeItem n_main_roughness = create_node(
          "roughness_anisotropy",
          NodeItem::Type::Vector2,
          {{"roughness", n_coat_affected_roughness}, {"anisotropy", anisotropy}});

      NodeItem n_metal_bsdf = create_node("conductor_bsdf",
                                          NodeItem::Type::BSDF,
                                          {{"ior", n_ior_out},
                                           {"extinction", n_extinction_out},
                                           {"roughness", n_main_roughness},
                                           {"normal", normal},
                                           {"tangent", n_main_tangent}});

      NodeItem n_specular_bsdf = create_node("dielectric_bsdf",
                                             NodeItem::Type::BSDF,
                                             {{"weight", specular},
                                              {"tint", in["specular_tint"]},
                                              {"ior", ior},
                                              {"scatter_mode", val(std::string("R"))},
                                              {"roughness", n_main_roughness},
                                              {"normal", normal},
                                              {"tangent", n_main_tangent}});

      NodeItem n_coat_affected_transmission_roughness = n_coat_affect_roughness_multiply2.mix(
          (roughness + roughness).clamp(), val(1.0f));

      NodeItem n_transmission_roughness = create_node(
          "roughness_anisotropy",
          NodeItem::Type::Vector2,
          {{"roughness", n_coat_affected_transmission_roughness}, {"anisotropy", anisotropy}});

      NodeItem n_transmission_bsdf = create_node("dielectric_bsdf",
                                                 NodeItem::Type::BSDF,
                                                 {{"tint", base_color},
                                                  {"ior", ior},
                                                  {"roughness", n_transmission_roughness},
                                                  {"normal", normal},
                                                  {"tangent", n_main_tangent}});

      NodeItem n_coat_gamma = coat.clamp(0.0f, 1.0f) * val(0.0f) + val(1.0f);
      NodeItem n_coat_affected_subsurface_color = base_color.max(val(0.0f)) ^ n_coat_gamma;
      NodeItem n_translucent_bsdf = create_node(
          "translucent_bsdf",
          NodeItem::Type::BSDF,
          {{"color", n_coat_affected_subsurface_color}, {"normal", normal}});

      NodeItem n_subsurface_bsdf = create_node(
          "subsurface_bsdf",
          NodeItem::Type::BSDF,
          {{"color", n_coat_affected_subsurface_color},
           {"radius", in["subsurface_radius"] * in["subsurface_scale"]},
           {"anisotropy", in["subsurface_anisotropy"]},
           {"normal", normal}});

      NodeItem n_sheen_bsdf = create_node("sheen_bsdf",
                                          NodeItem::Type::BSDF,
                                          {{"weight", in["sheen"]},
                                           {"color", in["sheen_tint"]},
                                           {"roughness", in["sheen_roughness"]},
                                           {"normal", normal}});

      NodeItem n_diffuse_bsdf = create_node("oren_nayar_diffuse_bsdf",
                                            NodeItem::Type::BSDF,
                                            {{"color", base_color.max(val(0.0f)) ^ n_coat_gamma},
                                             {"roughness", diffuse_roughness},
                                             {"weight", val(1.0f)},
                                             {"normal", normal}});

      NodeItem n_subsurface_mix = in["subsurface"].mix(n_diffuse_bsdf, n_subsurface_bsdf);

      NodeItem n_sheen_layer = create_node(
          "layer", NodeItem::Type::BSDF, {{"top", n_sheen_bsdf}, {"base", n_subsurface_mix}});

      NodeItem n_transmission_mix = in["transmission"].mix(n_sheen_layer, n_transmission_bsdf);

      NodeItem n_specular_layer = create_node(
          "layer", NodeItem::Type::BSDF, {{"top", n_specular_bsdf}, {"base", n_transmission_mix}});

      NodeItem n_metalness_mix = in["metallic"].mix(n_specular_layer, n_metal_bsdf);

      NodeItem n_thin_film_layer = create_node(
          "layer", NodeItem::Type::BSDF, {{"top", n_thin_film_bsdf}, {"base", n_metalness_mix}});

      NodeItem n_coat_attenuation = coat.mix(val(MaterialX::Color3(1.0f, 1.0f, 1.0f)),
                                             in["coat_tint"]);

      res = create_node("layer",
                        NodeItem::Type::BSDF,
                        {{"top", n_coat_bsdf}, {"base", n_thin_film_layer * n_coat_attenuation}});
      break;
    }

    case NodeItem::Type::EDF: {
      auto in = edf_inputs();
      res = create_node(
          "uniform_edf", NodeItem::Type::EDF, {{"color", in["emission_color"] * in["emission"]}});
      break;
    }

    case NodeItem::Type::SurfaceShader: {
      auto in = bsdf_inputs();
      auto e_in = edf_inputs();
      in.insert(e_in.begin(), e_in.end());

      NodeItem base_color = in["base_color"];

      NodeItem anisotropy = in["anisotropic"];
      NodeItem tangent = in["tangent"];
      if (anisotropy) {
        /* Anisotropy scaled down to approximately match the principled BSDF. */
        anisotropy = anisotropy * val(0.7f);

        /* Rotation is offset by 90 degrees and inverted to approximately align visually with
         * principled BSDF direction. */
        NodeItem rotation = -((in["anisotropic_rotation"] * val(360.0f)) + val(90.0f));

        /* Only create a normal node locally if we need to use it to rotate the tangent vector.
         * we don't actually pass this to the exported material. */
        NodeItem normal = in["normal"];
        if (!normal) {
          const std::string world = "world";
          normal =
              create_node("normal", NodeItem::Type::Vector3, {{"space", val(world)}}).normalize();
        }

        if (!tangent) {
          const std::string world = "world";
          tangent =
              create_node("tangent", NodeItem::Type::Vector3, {{"space", val(world)}}).normalize();
        }

        NodeItem n_tangent_rotate_normalize = tangent.rotate(rotation, normal).normalize();
        tangent = anisotropy.if_else(
            NodeItem::CompareOp::Greater, val(0.0f), n_tangent_rotate_normalize, tangent);
      }

      /* Enable OpenPBR thin film only if thickness > 0. */
      NodeItem thin_film_thickness = in["thin_film_thickness"] * val(0.001f);
      NodeItem thin_film_weight = thin_film_thickness.if_else(
          NodeItem::CompareOp::Greater, val(0.0f), val(1.0f), val(0.0f));

      /* "specular" here is "Specular IOR Level" in principled BSDF
       * 0 = no specular
       * 0.5 = full weight specular
       * 1 = double specular weight */
      NodeItem specular_weight = in["specular"] * val(2.0f);

      res = create_node("open_pbr_surface",
                        NodeItem::Type::SurfaceShader,
                        {{"base_weight", val(1.0f)},
                         {"base_color", base_color},
                         {"base_diffuse_roughness", in["diffuse_roughness"]},
                         {"base_metalness", in["metallic"]},
                         {"specular_weight", specular_weight},
                         {"specular_color", in["specular_tint"]},
                         {"specular_roughness", in["roughness"]},
                         {"specular_ior", in["ior"]},
                         {"specular_roughness_anisotropy", anisotropy},
                         {"transmission_weight", in["transmission"]},
                         {"transmission_color", base_color},
                         {"subsurface_weight", in["subsurface"]},
                         {"subsurface_color", base_color},
                         {"subsurface_radius_scale", in["subsurface_radius"]},
                         {"subsurface_radius", in["subsurface_scale"]},
                         {"subsurface_scatter_anisotropy", in["subsurface_anisotropy"]},
                         {"fuzz_weight", in["sheen"]},
                         {"fuzz_color", in["sheen_tint"]},
                         {"fuzz_roughness", in["sheen_roughness"]},
                         {"coat_weight", in["coat"]},
                         {"coat_color", in["coat_tint"]},
                         {"coat_roughness", in["coat_roughness"]},
                         {"coat_ior", in["coat_ior"]},
                         /* Principled BSDF does not support anisotropy for the coat
                          *  {"coat_roughness_anisotropy", anisotropic},
                          *  {"geometry_coat_tangent", tangent}, */
                         {"emission_luminance", in["emission"]},
                         {"emission_color", in["emission_color"]},
                         {"thin_film_weight", thin_film_weight},
                         {"thin_film_thickness", thin_film_thickness},
                         {"thin_film_ior", in["thin_film_IOR"]},
                         {"geometry_normal", in["normal"]},
                         {"geometry_coat_normal", in["coat_normal"]},
                         {"geometry_tangent", tangent},
                         {"geometry_opacity", in["alpha"]}});
      break;
    }

    case NodeItem::Type::SurfaceOpacity: {
      res = get_input_value("Alpha", NodeItem::Type::Float);
      break;
    }

    default:
      BLI_assert_unreachable();
  }
  return res;
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_bsdf_principled_cc

/* node type definition */
void register_node_type_sh_bsdf_principled()
{
  namespace file_ns = blender::nodes::node_shader_bsdf_principled_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeBsdfPrincipled", SH_NODE_BSDF_PRINCIPLED);
  ntype.ui_name = "Principled BSDF";
  ntype.ui_description =
      "Physically-based, easy-to-use shader for rendering surface materials, based on the OpenPBR "
      "model";
  ntype.enum_name_legacy = "BSDF_PRINCIPLED";
  ntype.nclass = NODE_CLASS_SHADER;
  ntype.declare = file_ns::node_declare;
  ntype.add_ui_poll = object_shader_nodes_poll;
  blender::bke::node_type_size_preset(ntype, blender::bke::eNodeSizePreset::Large);
  ntype.initfunc = file_ns::node_shader_init_principled;
  ntype.gpu_fn = file_ns::node_shader_gpu_bsdf_principled;
  ntype.updatefunc = file_ns::node_shader_update_principled;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::node_register_type(ntype);
}
