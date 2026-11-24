/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "UI_interface_layout.hh"
#include "UI_resources.hh"
#include "node_shader_util.hh"

#include "BKE_node_runtime.hh"

#include <map>

namespace blender {

namespace nodes::node_shader_bsdf_open_pbr_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNodeTree *ntree = b.tree_or_null();
  const bool is_gpu_internal = ntree && (ntree->flag & NTREE_IS_GPU_SHADER_INTERNAL);

  b.use_custom_socket_order();

  b.add_output<decl::Shader>("BSDF"_ustr);

  b.add_input<decl::Float>("Weight"_ustr).available(is_gpu_internal);
#define OPENPBR_SOCK_WEIGHT_ID 0
  /* --------------------------------------------------------------------
   * Base Component
   */
  /* TODO(OpenPBR): have a diagram in the manual explaining what is diffuse, opaque etc., and come
   * back here to see if we need to change some terminology. */
  PanelDeclarationBuilder &base = b.add_panel("Base"_ustr).default_closed(false);
  base.description(
      "The bulk material structure, consisting of a mix of metal and dielectric components");
  base.add_input<decl::Float>("Base Weight"_ustr)
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description("Scalar multiplier of the Base Color");
#define OPENPBR_SOCK_BASE_WEIGHT_ID 1
  base.add_input<decl::Color>("Base Color"_ustr)
      .default_value({0.8f, 0.8f, 0.8f, 1.0f})
      .description(
          "Color of the diffuse component, and the metallic reflection at normal incidence");
#define OPENPBR_SOCK_BASE_COLOR_ID 2
  base.add_input<decl::Float>("Base Metalness"_ustr)
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "Blends between dielectric and metallic components. "
          "At 0.0 the material is fully dielectric, at 1.0 it is fully metallic");
#define OPENPBR_SOCK_BASE_METALNESS_ID 3
  base.add_input<decl::Float>("Base Diffuse Roughness"_ustr)
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "Roughness of the diffuse component. 0.0 is perfect Lambertian reflection, 1.0 is fully "
          "Oren-Nayar");
#define OPENPBR_SOCK_DIFFUSE_ROUGHNESS_ID 4
  /* --------------------------------------------------------------------
   * Specular Component
   */
  PanelDeclarationBuilder &specular = b.add_panel("Specular"_ustr).default_closed(false);
  specular.description(
      "Controls the primary specular reflection component of the base dielectric");
  specular.add_input<decl::Float>("Specular Weight"_ustr)
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "Modulates the dielectric reflectivity at normal incidence, and the metallic "
          "reflection. When it is 0.0, the specular reflection disappears entirely; when above "
          "1.0, the reflectivity is increased above the level specified by Specular IOR");
#define OPENPBR_SOCK_SPECULAR_WEIGHT_ID 5
  specular.add_input<decl::Color>("Specular Color"_ustr)
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .description(
          "Tints the metallic reflection at near-grazing incidence, and the whole dielectric "
          "reflection");
#define OPENPBR_SOCK_SPECULAR_COLOR_ID 6
  specular.add_input<decl::Float>("Specular Roughness"_ustr)
      .default_value(0.3f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "Specifies microfacet roughness for both dielectric and metallic components. 0.0 is "
          "perfect mirror reflection, 1.0 is completely rough");
#define OPENPBR_SOCK_SPECULAR_ROUGHNESS_ID 7
  specular.add_input<decl::Float>("Specular Roughness Anisotropy"_ustr)
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "Amount of anisotropy for glossy, transmission and metallic components. "
          "Higher values give elongated highlights along the tangent direction");
#define OPENPBR_SOCK_SPECULAR_ROUGHNESS_ANISOTROPY_ID 8
  specular.add_input<decl::Float>("Specular IOR"_ustr)
      .default_value(1.5f)
      .min(1.0f)
      .max(3.0f)
      .subtype(PROP_FACTOR)
      .description(
          "Controls the refractive index of the dielectric. For most materials, the IOR is "
          "between 1.0 (vacuum and air) and 4.0 (germanium). The default value of 1.5 is a good "
          "approximation for glass");
#define OPENPBR_SOCK_SPECULAR_IOR_ID 9
  /* --------------------------------------------------------------------
   * Transmission Component
   */
  PanelDeclarationBuilder &transmission = b.add_panel("Transmission"_ustr).default_closed(true);
  transmission.description(
      "For modeling materials that transmit and refract light into the object interior, ranging "
      "from clear or colored glass and liquids to translucent materials with significant "
      "scattering such as fruit juice, murky water, and opalescent glass");
  transmission.add_input<decl::Float>("Transmission Weight"_ustr)
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "Blends between transmission and other opaque components (glossy-diffuse and "
          "subsurface)");
#define OPENPBR_SOCK_TRANSMISSION_WEIGHT_ID 10
  transmission.add_input<decl::Color>("Transmission Color"_ustr)
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .description("Tint the dielectric refraction");
#define OPENPBR_SOCK_TRANSMISSION_COLOR_ID 11
  transmission.add_input<decl::Float>("Transmission Depth"_ustr)
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_DISTANCE)
      .description(
          "Controls the depth into the medium at which the transmission color is realized; if "
          "zero, interior medium is absent, and transmission color acts as a constant "
          "(on-surface) transmission tint");
#define OPENPBR_SOCK_TRANSMISSION_DEPTH_ID 12
  transmission.add_input<decl::Color>("Transmission Scatter"_ustr)
      .default_value({0.0f, 0.0f, 0.0f, 1.0f})
      .description("Scattering coefficient of the interior medium");
#define OPENPBR_SOCK_TRANSMISSION_SCATTER_ID 13
  transmission.add_input<decl::Float>("Transmission Scatter Anisotropy"_ustr)
      .default_value(0.0f)
      .min(-1.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "Directionality of the scattering of the interior medium. Zero is isotropic, negative "
          "is backward, positive is forward");
#define OPENPBR_SOCK_TRANSMISSION_SCATTER_ANISOTROPY_ID 14
  transmission.add_input<decl::Float>("Transmission Dispersion Scale"_ustr)
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description("Scales the amount of dispersion");
#define OPENPBR_SOCK_TRANSMISSION_DISPERSION_SCALE_ID 15
  transmission.add_input<decl::Float>("Transmission Dispersion Abbe Number"_ustr)
      .default_value(20.0f)
      .min(9.0f)
      .max(91.0f)
      .subtype(PROP_FACTOR)
      .description(
          "Abbe number of the base dielectric medium. Smaller number gives stronger dispersion");
#define OPENPBR_SOCK_TRANSMISSION_DISPERSION_ABBE_NUMBER_ID 16
  /* --------------------------------------------------------------------
   * Subsurface Component
   */
  PanelDeclarationBuilder &subsurface = b.add_panel("Subsurface"_ustr).default_closed(true);
  subsurface.description(
      "Generates a soft appearance due to light scattering under the surface. Used to render "
      "materials such as skin, milk and wax");
  subsurface.add_input<decl::Float>("Subsurface Weight"_ustr)
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description("Blends between diffuse-glossy and subsurface components");
#define OPENPBR_SOCK_SUBSURFACE_WEIGHT_ID 17
  subsurface.add_input<decl::Color>("Subsurface Color"_ustr)
      .default_value({0.8f, 0.8f, 0.8f, 1.0f})
      .description("The observed reflection albedo color of the subsurface");
#define OPENPBR_SOCK_SUBSURFACE_COLOR_ID 18
  subsurface.add_input<decl::Float>("Subsurface Radius"_ustr)
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_DISTANCE)
      .usage_by_bool("Geometry Thin Walled"_ustr, false)
      .description(
          "The average distance that a ray travels through the medium before being absorbed or "
          "scattered. Together with Subsurface Radius Scale, this controls the apparent density "
          "of the medium");
#define OPENPBR_SOCK_SUBSURFACE_RADIUS_ID 19
  subsurface.add_input<decl::Color>("Subsurface Radius Scale"_ustr)
      .default_value({1.0f, 0.5f, 0.25f, 1.0f})
      .description(
          "Multiplier to Subsurface Radius per color channel. The default value approximates "
          "Rayleigh scattering")
      .usage_by_bool("Geometry Thin Walled"_ustr, false);
#define OPENPBR_SOCK_SUBSURFACE_RADIUS_SCALE_ID 20
  subsurface.add_input<decl::Float>("Subsurface Scatter Anisotropy"_ustr)
      .default_value(0.0f)
      .min(-1.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "Directionality of scattering within the subsurface medium. "
          "Zero scatters uniformly in all directions, positive values scatter more in the forward "
          "direction, and negative values scatter more backwards. "
          "For example, skin has been measured to have an anisotropy of 0.8");
#define OPENPBR_SOCK_SUBSURFACE_SCATTER_ANISOTROPY_ID 21
  /* --------------------------------------------------------------------
   * Coat Component
   */
  PanelDeclarationBuilder &coat = b.add_panel("Coat"_ustr).default_closed(true);
  coat.description(
      "A layer of coat on top of the material that transmits and possibly absorbs light, to "
      "simulate the appearance of objects with colored varnish or lacquer");
  coat.add_input<decl::Float>("Coat Weight"_ustr)
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description("Coverage weight of the coat layer");
#define OPENPBR_SOCK_COAT_WEIGHT_ID 22
  coat.add_input<decl::Color>("Coat Color"_ustr)
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .description(
          "Observed tint color of the underlying base at normal incidence due to absorption in "
          "the coat");
#define OPENPBR_SOCK_COAT_COLOR_ID 23
  coat.add_input<decl::Float>("Coat Roughness"_ustr)
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description("Roughness of the coat layer");
#define OPENPBR_SOCK_COAT_ROUGHNESS_ID 24
  coat.add_input<decl::Float>("Coat Roughness Anisotropy"_ustr)
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description("Anisotropy of the coat layer");
#define OPENPBR_SOCK_COAT_ROUGHNESS_ANISOTROPY_ID 25
  coat.add_input<decl::Float>("Coat IOR"_ustr)
      .default_value(1.6f)
      .min(1.0f)
      .max(3.0f)
      .subtype(PROP_FACTOR)
      .description("Refractive index of the coat layer");
#define OPENPBR_SOCK_COAT_IOR_ID 26
  coat.add_input<decl::Float>("Coat Darkening"_ustr)
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "Default to 1 for physically correct darkening effect due to internal reflections in "
          "the coat. When set to 0, the base albedo is boosted so that the observed coated color "
          "matches the specified base layer color");
#define OPENPBR_SOCK_COAT_DARKENING_ID 27
  /* --------------------------------------------------------------------
   * Fuzz Component
   */
  PanelDeclarationBuilder &fuzz = b.add_panel("Fuzz"_ustr).default_closed(true);
  fuzz.description(
      "The topmost scattering layer simulating fuzzy or dusty appearance, producing a specular "
      "highlight at grazing angles");
  fuzz.add_input<decl::Float>("Fuzz Weight"_ustr)
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description("Coverage weight of the fuzz layer");
#define OPENPBR_SOCK_FUZZ_WEIGHT_ID 28
  fuzz.add_input<decl::Color>("Fuzz Color"_ustr)
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .description("Color of the fuzz reflection");
#define OPENPBR_SOCK_FUZZ_COLOR_ID 29
  fuzz.add_input<decl::Float>("Fuzz Roughness"_ustr)
      .default_value(0.5f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "Controls how fiber-like the fuzz layer is. At low roughness the fuzz produces a "
          "high-sheen fabric appearance, while at high roughness it produces a dusty appearance");
#define OPENPBR_SOCK_FUZZ_ROUGHNESS_ID 30
  /* --------------------------------------------------------------------
   * Emission Component
   */
  PanelDeclarationBuilder &emission = b.add_panel("Emission"_ustr).default_closed(true);
  emission.description("Light emission from the base layer");
  emission.add_input<decl::Float>("Emission Luminance"_ustr)
      .default_value(0.0f)
      .min(0.0f)
      .max(1000.0f)
      .subtype(PROP_FACTOR)
      .description("Emission luminance");
  /* TODO(OpenPBR): the unit should be cd/m^2 (nits)", but we don't support physical unit yet. */
#define OPENPBR_SOCK_EMISSION_LUMINANCE_ID 31
  emission.add_input<decl::Color>("Emission Color"_ustr)
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .description("Color of light emission from the surface");
#define OPENPBR_SOCK_EMISSION_COLOR_ID 32
  /* --------------------------------------------------------------------
   * Thin-film Component
   */
  PanelDeclarationBuilder &thinfilm = b.add_panel("Thin Film"_ustr).default_closed(true);
  thinfilm.description(
      "Simulates the effect of rainbow-like color fringes due to inteference of a thin film "
      "sitting on top of the material");
  thinfilm.add_input<decl::Float>("Thin Film Weight"_ustr)
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description("Coverage weight of the thin film");
#define OPENPBR_SOCK_THIN_FILM_WEIGHT_ID 33
  thinfilm.add_input<decl::Float>("Thin Film Thickness"_ustr)
      .default_value(0.5f)
      .min(0.0f)
      .max(1.0f)
      .step(0.01f)
      .subtype(PROP_WAVELENGTH_UM)
      .description("Thickness of the thin film in micrometers");
#define OPENPBR_SOCK_THIN_FILM_THICKNESS_ID 34
  thinfilm.add_input<decl::Float>("Thin Film IOR"_ustr)
      .default_value(1.4f)
      .min(1.0f)
      .max(3.0f)
      .subtype(PROP_FACTOR)
      .description("Refractive index of the thin film");
#define OPENPBR_SOCK_THIN_FILM_IOR_ID 35
  /* --------------------------------------------------------------------
   * Geometry Component
   */
  PanelDeclarationBuilder &geometry = b.add_panel("Geometry"_ustr).default_closed(false);
  geometry.description("Geometry-related properties");
  geometry.add_input<decl::Float>("Geometry Opacity"_ustr)
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "Controls the transparency of the material, at 0.0 it is fully transparent, and at 1.0 "
          "fully opaque");
#define OPENPBR_SOCK_GEOMETRY_OPACITY_ID 36
  geometry.add_input<decl::Bool>("Geometry Thin Walled"_ustr)
      .default_value(false)
      .description(
          "When enabled, the geometry becomes a thin structure with the same material on both "
          "sides. Useful for modeling thin objects such as window, papers and leaves");
#define OPENPBR_SOCK_GEOMETRY_THIN_WALLED_ID 37
  geometry.add_input<decl::Vector>("Geometry Normal"_ustr)
      .hide_value()
      .description("Shading normal of the base layer");
#define OPENPBR_SOCK_GEOMETRY_NORMAL_ID 38
  geometry.add_input<decl::Vector>("Geometry Tangent"_ustr)
      .hide_value()
      .description("Tangent direction of the anisotropy on the base layer");
#define OPENPBR_SOCK_GEOMETRY_TANGENT_ID 39
  geometry.add_input<decl::Vector>("Geometry Coat Normal"_ustr)
      .hide_value()
      .description("Shading normal of the coat layer");
#define OPENPBR_SOCK_GEOMETRY_COAT_NORMAL_ID 40
  geometry.add_input<decl::Vector>("Geometry Coat Tangent"_ustr)
      .hide_value()
      .description("Tangent direction of the anisotropy on the coat layer");
#define OPENPBR_SOCK_GEOMETRY_COAT_TANGENT_ID 41
}

static int node_shader_gpu_bsdf_open_pbr(GPUMaterial *mat,
                                         bNode *node,
                                         bNodeExecData * /*execdata*/,
                                         GPUNodeStack *in,
                                         GPUNodeStack *out)
{
  /* Normals */
  if (!in[OPENPBR_SOCK_GEOMETRY_NORMAL_ID].link) {
    GPU_link(
        mat, "world_normals_get", GPU_shading_data(), &in[OPENPBR_SOCK_GEOMETRY_NORMAL_ID].link);
  }

  /* Coat Normals */
  if (!in[OPENPBR_SOCK_GEOMETRY_COAT_NORMAL_ID].link) {
    GPU_link(mat,
             "world_normals_get",
             GPU_shading_data(),
             &in[OPENPBR_SOCK_GEOMETRY_COAT_NORMAL_ID].link);
  }

  const GPUNodeStack metalness = in[OPENPBR_SOCK_BASE_METALNESS_ID];
  const GPUNodeStack base_color = in[OPENPBR_SOCK_BASE_COLOR_ID];
  const GPUNodeStack specular_color = in[OPENPBR_SOCK_SPECULAR_COLOR_ID];
  const GPUNodeStack transmission = in[OPENPBR_SOCK_TRANSMISSION_WEIGHT_ID];
  const GPUNodeStack thin_wall = in[OPENPBR_SOCK_GEOMETRY_THIN_WALLED_ID];

  const bool use_fuzz = in[OPENPBR_SOCK_FUZZ_WEIGHT_ID].socket_not_zero();
  const bool use_diffuse = in[OPENPBR_SOCK_FUZZ_WEIGHT_ID].socket_not_zero() ||
                           (metalness.socket_not_one() && transmission.socket_not_one());
  const bool use_diffuse_roughness = use_diffuse &&
                                     (in[OPENPBR_SOCK_DIFFUSE_ROUGHNESS_ID].socket_not_zero() ||
                                      use_fuzz);
  const bool use_subsurf = in[OPENPBR_SOCK_SUBSURFACE_WEIGHT_ID].socket_not_zero() && use_diffuse;
  const bool use_refract = metalness.socket_not_one() && transmission.socket_not_zero();
  const bool use_transparency = in[OPENPBR_SOCK_GEOMETRY_OPACITY_ID].socket_not_one();
  const bool use_coat = in[OPENPBR_SOCK_COAT_WEIGHT_ID].socket_not_zero();
  const bool use_translucent = use_subsurf && thin_wall.socket_not_zero();

  eGPUMaterialFlag flag = GPU_MATFLAG_GLOSSY;
  if (use_diffuse) {
    flag |= GPU_MATFLAG_DIFFUSE;
  }
  if (use_diffuse_roughness) {
    flag |= GPU_MATFLAG_DIFFUSE_ROUGHNESS;
  }
  if (use_refract) {
    flag |= GPU_MATFLAG_REFRACT;
  }
  if (use_subsurf && thin_wall.socket_not_one()) {
    flag |= GPU_MATFLAG_SUBSURFACE;
  }
  if (use_transparency) {
    flag |= GPU_MATFLAG_TRANSPARENT;
  }
  if (use_coat) {
    flag |= GPU_MATFLAG_COAT;
  }
  if (use_translucent) {
    flag |= GPU_MATFLAG_TRANSLUCENT;
  }

  /* Coat tints lower layers. */
  const bool coat_might_be_tinted = use_coat && in[OPENPBR_SOCK_COAT_COLOR_ID].might_be_tinted();

  const bool refraction_might_be_tinted =
      use_refract &&
      (in[OPENPBR_SOCK_TRANSMISSION_COLOR_ID].might_be_tinted() || coat_might_be_tinted);

  bool specular_might_be_tinted = specular_color.might_be_tinted() ||
                                  (metalness.socket_not_zero() && base_color.might_be_tinted());
  specular_might_be_tinted |= coat_might_be_tinted;
  /* Multiscatter GGX can tint the reflection lobe. See `bsdf_lut`. */
  specular_might_be_tinted |= refraction_might_be_tinted;

  if (specular_might_be_tinted) {
    /* Specular reflection. */
    flag |= GPU_MATFLAG_REFLECTION_MAYBE_COLORED;
  }
  if (refraction_might_be_tinted) {
    /* Specular refraction. */
    flag |= GPU_MATFLAG_REFRACTION_MAYBE_COLORED;
  }

  GPU_material_flag_set(mat, flag);

  /* Make constant link for the cases we optimize. This allows the driver to constant fold.
   * Note that doing so specialize the final tree topology, and thus the shader becomes less
   * reusable. So to be used with care.
   * Also note that we do note override existing links. This is because it would leak the current
   * nodes otherwise. */
  const float zero = 0.0f;
  if (!use_coat && in[OPENPBR_SOCK_COAT_WEIGHT_ID].link == nullptr) {
    in[OPENPBR_SOCK_COAT_WEIGHT_ID].link = GPU_constant(&zero);
  }
  if (!use_subsurf && in[OPENPBR_SOCK_SUBSURFACE_WEIGHT_ID].link == nullptr) {
    in[OPENPBR_SOCK_SUBSURFACE_WEIGHT_ID].link = GPU_constant(&zero);
  }
  if (!use_refract && in[OPENPBR_SOCK_TRANSMISSION_WEIGHT_ID].link == nullptr) {
    in[OPENPBR_SOCK_TRANSMISSION_WEIGHT_ID].link = GPU_constant(&zero);
  }

  return GPU_stack_link(
      mat, node, "node_bsdf_open_pbr", in, out, GPU_kernel_globals(), GPU_shading_data());
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  using InputsType = std::map<std::string, NodeItem>;

  auto bsdf_inputs = [&]() -> InputsType {
    return {
        /* Base Component */
        {"base_weight", get_input_value("Base Weight", NodeItem::Type::Float)},
        {"base_color", get_input_value("Base Color", NodeItem::Type::Color3)},
        {"base_metalness", get_input_value("Base Metalness", NodeItem::Type::Float)},
        {"base_diffuse_roughness",
         get_input_value("Base Diffuse Roughness", NodeItem::Type::Float)},
        /* Specular Component */
        {"specular_weight", get_input_value("Specular Weight", NodeItem::Type::Float)},
        {"specular_color", get_input_value("Specular Color", NodeItem::Type::Color3)},
        {"specular_roughness", get_input_value("Specular Roughness", NodeItem::Type::Float)},
        {"specular_roughness_anisotropy",
         get_input_value("Specular Roughness Anisotropy", NodeItem::Type::Float)},
        {"specular_ior", get_input_value("Specular IOR", NodeItem::Type::Float)},
        /* Transmission Component */
        {"transmission_weight", get_input_value("Transmission Weight", NodeItem::Type::Float)},
        {"transmission_color", get_input_value("Transmission Color", NodeItem::Type::Color3)},
        {"transmission_depth", get_input_value("Transmission Depth", NodeItem::Type::Float)},
        {"transmission_scatter", get_input_value("Transmission Scatter", NodeItem::Type::Color3)},
        {"transmission_scatter_anisotropy",
         get_input_value("Transmission Scatter Anisotropy", NodeItem::Type::Float)},
        {"transmission_dispersion_scale",
         get_input_value("Transmission Dispersion Scale", NodeItem::Type::Float)},
        {"transmission_dispersion_abbe_number",
         get_input_value("Transmission Dispersion Abbe Number", NodeItem::Type::Float)},
        /* Subsurface Component */
        {"subsurface_weight", get_input_value("Subsurface Weight", NodeItem::Type::Float)},
        {"subsurface_color", get_input_value("Subsurface Color", NodeItem::Type::Color3)},
        {"subsurface_radius", get_input_value("Subsurface Radius", NodeItem::Type::Float)},
#  if MATERIALX_MAJOR_VERSION <= 1 && MATERIALX_MINOR_VERSION <= 38
        {"subsurface_radius_scale",
         get_input_value("Subsurface Radius Scale", NodeItem::Type::Vector3)},
#  else
        {"subsurface_radius_scale",
         get_input_value("Subsurface Radius Scale", NodeItem::Type::Color3)},
#  endif
        {"subsurface_scatter_anisotropy",
         get_input_value("Subsurface Scatter Anisotropy", NodeItem::Type::Float)},
        /* Coat Component */
        {"coat_weight", get_input_value("Coat Weight", NodeItem::Type::Float)},
        {"coat_color", get_input_value("Coat Color", NodeItem::Type::Color3)},
        {"coat_roughness", get_input_value("Coat Roughness", NodeItem::Type::Float)},
        {"coat_roughness_anisotropy",
         get_input_value("Coat Roughness Anisotropy", NodeItem::Type::Float)},
        {"coat_ior", get_input_value("Coat IOR", NodeItem::Type::Float)},
        {"coat_darkening", get_input_value("Coat Darkening", NodeItem::Type::Float)},
        /* Fuzz Component */
        {"fuzz_weight", get_input_value("Fuzz Weight", NodeItem::Type::Float)},
        {"fuzz_color", get_input_value("Fuzz Color", NodeItem::Type::Color3)},
        {"fuzz_roughness", get_input_value("Fuzz Roughness", NodeItem::Type::Float)},
        /* Thin Film Component */
        {"thin_film_weight", get_input_value("Thin Film Weight", NodeItem::Type::Float)},
        {"thin_film_thickness", get_input_value("Thin Film Thickness", NodeItem::Type::Float)},
        {"thin_film_ior", get_input_value("Thin Film IOR", NodeItem::Type::Float)},
        /* Geometry Component */
        {"geometry_opacity", get_input_value("Geometry Opacity", NodeItem::Type::Float)},
        {"geometry_thin_walled", get_input_value("Geometry Thin Walled", NodeItem::Type::Boolean)},
        {"geometry_normal", get_input_link("Geometry Normal", NodeItem::Type::Vector3)},
        {"geometry_tangent", get_input_link("Geometry Tangent", NodeItem::Type::Vector3)},
        {"geometry_coat_normal", get_input_link("Geometry Coat Normal", NodeItem::Type::Vector3)},
        {"geometry_coat_tangent",
         get_input_link("Geometry Coat Tangent", NodeItem::Type::Vector3)},
    };
  };

  auto edf_inputs = [&]() -> InputsType {
    return {
        {"emission_luminance", get_input_value("Emission Luminance", NodeItem::Type::Float)},
        {"emission_color", get_input_value("Emission Color", NodeItem::Type::Color3)},
    };
  };

  NodeItem res = empty();

  switch (to_type_) {
    case NodeItem::Type::BSDF: {
      break;
    }
    case NodeItem::Type::EDF: {
      auto in = edf_inputs();
      res = create_node("uniform_edf",
                        NodeItem::Type::EDF,
                        {{"color", in["emission_color"] * in["emission_luminance"]}});
      break;
    }
    case NodeItem::Type::SurfaceShader: {
      auto in = bsdf_inputs();
      auto e_in = edf_inputs();
      in.insert(e_in.begin(), e_in.end());

      res = create_node(
          "open_pbr_surface",
          NodeItem::Type::SurfaceShader,
          {{"base_weight", in["base_weight"]},
           {"base_color", in["base_color"]},
           {"base_metalness", in["base_metalness"]},
           {"base_diffuse_roughness", in["base_diffuse_roughness"]},

           {"specular_weight", in["specular_weight"]},
           {"specular_color", in["specular_color"]},
           {"specular_roughness", in["specular_roughness"]},
           {"specular_roughness_anisotropy", in["specular_roughness_anisotropy"]},
           {"specular_ior", in["specular_ior"]},

           {"transmission_weight", in["transmission_weight"]},
           {"transmission_color", in["transmission_color"]},
           {"transmission_depth", in["transmission_depth"]},
           {"transmission_scatter", in["transmission_scatter"]},
           {"transmission_scatter_anisotropy", in["transmission_scatter_anisotropy"]},
           {"transmission_dispersion_scale", in["transmission_dispersion_scale"]},
           {"transmission_dispersion_abbe_number", in["transmission_dispersion_abbe_number"]},

           {"subsurface_weight", in["subsurface_weight"]},
           {"subsurface_color", in["subsurface_color"]},
           {"subsurface_radius", in["subsurface_radius"]},
           {"subsurface_radius_scale", in["subsurface_radius_scale"]},
           {"subsurface_scatter_anisotropy", in["subsurface_scatter_anisotropy"]},

           {"coat_weight", in["coat_weight"]},
           {"coat_color", in["coat_color"]},
           {"coat_roughness", in["coat_roughness"]},
           {"coat_roughness_anisotropy", in["coat_roughness_anisotropy"]},
           {"coat_ior", in["coat_ior"]},
           {"coat_darkening", in["coat_darkening"]},

           {"fuzz_weight", in["fuzz_weight"]},
           {"fuzz_color", in["fuzz_color"]},
           {"fuzz_roughness", in["fuzz_roughness"]},

           {"emission_luminance", in["emission_luminance"]},
           {"emission_color", in["emission_color"]},

           {"thin_film_weight", in["thin_film_weight"]},
           {"thin_film_thickness", in["thin_film_thickness"]},
           {"thin_film_ior", in["thin_film_ior"]},

           {"geometry_opacity", in["geometry_opacity"]},
           {"geometry_thin_walled", in["geometry_thin_walled"]},
           {"geometry_normal", in["geometry_normal"]},
           {"geometry_tangent", in["geometry_tangent"]},
           {"geometry_coat_normal", in["geometry_coat_normal"]},
           {"geometry_coat_tangent", in["geometry_coat_tangent"]}

          });
      break;
    }
    case NodeItem::Type::SurfaceOpacity: {
      res = get_input_value("Geometry Opacity", NodeItem::Type::Float);
      break;
    }

    default:
      BLI_assert_unreachable();
  }
  return res;
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace nodes::node_shader_bsdf_open_pbr_cc

/* node type definition */
void register_node_type_sh_bsdf_open_pbr()
{
  namespace file_ns = nodes::node_shader_bsdf_open_pbr_cc;

  static bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeBsdfOpenPBR"_ustr, SH_NODE_BSDF_OPEN_PBR);
  ntype.ui_name = "OpenPBR";
  ntype.ui_description = "OpenPBR Ueber-Shader material model (based on rev. 1.1.2).";
  ntype.enum_name_legacy = "BSDF_OPEN_PBR";
  ntype.nclass = NODE_CLASS_SHADER;
  ntype.declare = file_ns::node_declare;
  ntype.gather_link_search_ops = search_link_ops_for_shader_bsdf_node;
  ntype.add_ui_poll = object_shader_nodes_poll;
  ntype.default_width = bke::NodeWidth::_240;
  ntype.gpu_fn = file_ns::node_shader_gpu_bsdf_open_pbr;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  bke::node_register_type(ntype);
}

}  // namespace blender
