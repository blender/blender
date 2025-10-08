/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"
#include "node_util.hh"
#include "sky_hosek.h"
#include "sky_nishita.h"

#include "BKE_context.hh"
#include "BKE_scene.hh"
#include "BKE_texture.h"

#include "RNA_access.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "NOD_socket_search_link.hh"

namespace blender::nodes::node_shader_tex_sky_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>("Vector").hide_value();
  b.add_output<decl::Color>("Color").no_muted_links();
}

static void node_shader_buts_tex_sky(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  layout->prop(ptr, "sky_type", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);

  if (RNA_enum_get(ptr, "sky_type") == SHD_SKY_PREETHAM) {
    layout->prop(ptr, "sun_direction", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
    layout->prop(ptr, "turbidity", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  }
  else if (RNA_enum_get(ptr, "sky_type") == SHD_SKY_HOSEK) {
    layout->prop(ptr, "sun_direction", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
    layout->prop(ptr, "turbidity", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
    layout->prop(ptr, "ground_albedo", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  }
  else {
    Scene *scene = CTX_data_scene(C);
    if (BKE_scene_uses_blender_eevee(scene)) {
      layout->label(RPT_("Sun disc not available in EEVEE"), ICON_ERROR);
    }
    layout->prop(ptr, "sun_disc", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);

    uiLayout *col;
    if (RNA_boolean_get(ptr, "sun_disc")) {
      col = &layout->column(true);
      col->prop(ptr, "sun_size", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
      col->prop(ptr, "sun_intensity", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
    }

    col = &layout->column(true);
    col->prop(ptr, "sun_elevation", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
    col->prop(ptr, "sun_rotation", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);

    layout->prop(ptr, "altitude", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);

    col = &layout->column(true);
    col->prop(ptr, "air_density", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
    col->prop(ptr, "aerosol_density", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
    col->prop(ptr, "ozone_density", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  }
}

static void node_shader_init_tex_sky(bNodeTree * /*ntree*/, bNode *node)
{
  NodeTexSky *tex = MEM_callocN<NodeTexSky>("NodeTexSky");
  BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
  BKE_texture_colormapping_default(&tex->base.color_mapping);
  tex->sun_direction[0] = 0.0f;
  tex->sun_direction[1] = 0.0f;
  tex->sun_direction[2] = 1.0f;
  tex->turbidity = 2.2f;
  tex->ground_albedo = 0.3f;
  tex->sun_disc = true;
  tex->sun_size = DEG2RADF(0.545f);
  tex->sun_intensity = 1.0f;
  tex->sun_elevation = DEG2RADF(15.0f);
  tex->sun_rotation = 0.0f;
  tex->altitude = 100.0f;
  tex->air_density = 1.0f;
  tex->aerosol_density = 1.0f;
  tex->ozone_density = 1.0f;
  tex->sky_model = SHD_SKY_MULTIPLE_SCATTERING;
  node->storage = tex;
}

struct SkyModelPreetham {
  float config_Y[5], config_x[5], config_y[5]; /* named after xyY color space */
  float radiance[3];
};

static float sky_perez_function(const float *lam, float theta, float gamma)
{
  float ctheta = cosf(theta);
  float cgamma = cosf(gamma);

  return (1.0 + lam[0] * expf(lam[1] / ctheta)) *
         (1.0 + lam[2] * expf(lam[3] * gamma) + lam[4] * cgamma * cgamma);
}

static void sky_precompute_old(SkyModelPreetham *sunsky, const float sun_angles[], float turbidity)
{
  float theta = sun_angles[0];
  float theta2 = theta * theta;
  float theta3 = theta2 * theta;
  float T = turbidity;
  float T2 = T * T;
  float chi = (4.0f / 9.0f - T / 120.0f) * (M_PI - 2.0f * theta);

  sunsky->radiance[0] = (4.0453f * T - 4.9710f) * tanf(chi) - 0.2155f * T + 2.4192f;
  sunsky->radiance[0] *= 0.06f;

  sunsky->radiance[1] = (0.00166f * theta3 - 0.00375f * theta2 + 0.00209f * theta) * T2 +
                        (-0.02903f * theta3 + 0.06377f * theta2 - 0.03202f * theta + 0.00394f) *
                            T +
                        (0.11693f * theta3 - 0.21196f * theta2 + 0.06052f * theta + 0.25886f);

  sunsky->radiance[2] = (0.00275f * theta3 - 0.00610f * theta2 + 0.00317f * theta) * T2 +
                        (-0.04214f * theta3 + 0.08970f * theta2 - 0.04153f * theta + 0.00516f) *
                            T +
                        (0.15346f * theta3 - 0.26756f * theta2 + 0.06670f * theta + 0.26688f);

  sunsky->config_Y[0] = (0.1787f * T - 1.4630f);
  sunsky->config_Y[1] = (-0.3554f * T + 0.4275f);
  sunsky->config_Y[2] = (-0.0227f * T + 5.3251f);
  sunsky->config_Y[3] = (0.1206f * T - 2.5771f);
  sunsky->config_Y[4] = (-0.0670f * T + 0.3703f);

  sunsky->config_x[0] = (-0.0193f * T - 0.2592f);
  sunsky->config_x[1] = (-0.0665f * T + 0.0008f);
  sunsky->config_x[2] = (-0.0004f * T + 0.2125f);
  sunsky->config_x[3] = (-0.0641f * T - 0.8989f);
  sunsky->config_x[4] = (-0.0033f * T + 0.0452f);

  sunsky->config_y[0] = (-0.0167f * T - 0.2608f);
  sunsky->config_y[1] = (-0.0950f * T + 0.0092f);
  sunsky->config_y[2] = (-0.0079f * T + 0.2102f);
  sunsky->config_y[3] = (-0.0441f * T - 1.6537f);
  sunsky->config_y[4] = (-0.0109f * T + 0.0529f);

  sunsky->radiance[0] /= sky_perez_function(sunsky->config_Y, 0, theta);
  sunsky->radiance[1] /= sky_perez_function(sunsky->config_x, 0, theta);
  sunsky->radiance[2] /= sky_perez_function(sunsky->config_y, 0, theta);
}

static int node_shader_gpu_tex_sky(GPUMaterial *mat,
                                   bNode *node,
                                   bNodeExecData * /*execdata*/,
                                   GPUNodeStack *in,
                                   GPUNodeStack *out)
{
  node_shader_gpu_default_tex_coord(mat, node, &in[0].link);
  node_shader_gpu_tex_mapping(mat, node, in, out);
  NodeTexSky *tex = (NodeTexSky *)node->storage;
  float sun_angles[2]; /* [0]=theta=zenith angle  [1]=phi=azimuth */
  sun_angles[0] = acosf(tex->sun_direction[2]);
  sun_angles[1] = atan2f(tex->sun_direction[0], tex->sun_direction[1]);

  if (tex->sky_model == SHD_SKY_PREETHAM) {
    /* Preetham */
    SkyModelPreetham sunsky;
    sky_precompute_old(&sunsky, sun_angles, tex->turbidity);
    XYZ_to_RGB xyz_to_rgb;
    get_XYZ_to_RGB_for_gpu(&xyz_to_rgb);
    return GPU_stack_link(mat,
                          node,
                          "node_tex_sky_preetham",
                          in,
                          out,
                          /* Pass config_Y/x/y as 3x(vec4+float) */
                          GPU_uniform(&sunsky.config_Y[0]),
                          GPU_uniform(&sunsky.config_Y[4]),
                          GPU_uniform(&sunsky.config_x[0]),
                          GPU_uniform(&sunsky.config_x[4]),
                          GPU_uniform(&sunsky.config_y[0]),
                          GPU_uniform(&sunsky.config_y[4]),
                          GPU_uniform(sun_angles),
                          GPU_uniform(sunsky.radiance),
                          GPU_uniform(xyz_to_rgb.r),
                          GPU_uniform(xyz_to_rgb.g),
                          GPU_uniform(xyz_to_rgb.b));
  }
  if (tex->sky_model == SHD_SKY_HOSEK) {
    /* Hosek / Wilkie */
    sun_angles[0] = fmin(M_PI_2, sun_angles[0]); /* clamp to horizon */
    SKY_ArHosekSkyModelState *sky_state = SKY_arhosek_xyz_skymodelstate_alloc_init(
        tex->turbidity, tex->ground_albedo, fmax(0.0, M_PI_2 - sun_angles[0]));
    /* Pass sky_state->configs[3][9] as 3*(vec4+vec4)+vec3 */
    float config_x07[8], config_y07[8], config_z07[8], config_xyz8[3];
    for (int i = 0; i < 8; ++i) {
      config_x07[i] = float(sky_state->configs[0][i]);
      config_y07[i] = float(sky_state->configs[1][i]);
      config_z07[i] = float(sky_state->configs[2][i]);
    }
    for (int i = 0; i < 3; ++i) {
      config_xyz8[i] = float(sky_state->configs[i][8]);
    }
    float radiance[3];
    for (int i = 0; i < 3; i++) {
      radiance[i] = sky_state->radiances[i] * (2 * M_PI / 683);
    }
    SKY_arhosekskymodelstate_free(sky_state);
    XYZ_to_RGB xyz_to_rgb;
    get_XYZ_to_RGB_for_gpu(&xyz_to_rgb);
    return GPU_stack_link(mat,
                          node,
                          "node_tex_sky_hosekwilkie",
                          in,
                          out,
                          GPU_uniform(&config_x07[0]),
                          GPU_uniform(&config_x07[4]),
                          GPU_uniform(&config_y07[0]),
                          GPU_uniform(&config_y07[4]),
                          GPU_uniform(&config_z07[0]),
                          GPU_uniform(&config_z07[4]),
                          GPU_uniform(config_xyz8),
                          GPU_uniform(sun_angles),
                          GPU_uniform(radiance),
                          GPU_uniform(xyz_to_rgb.r),
                          GPU_uniform(xyz_to_rgb.g),
                          GPU_uniform(xyz_to_rgb.b));
  }

  /* Nishita */
  Array<float> pixels(4 * GPU_SKY_WIDTH * GPU_SKY_HEIGHT);

  if (tex->sky_model == SHD_SKY_SINGLE_SCATTERING) {
    SKY_single_scattering_precompute_texture(pixels.data(),
                                             4,
                                             GPU_SKY_WIDTH,
                                             GPU_SKY_HEIGHT,
                                             tex->sun_elevation,
                                             tex->altitude,
                                             tex->air_density,
                                             tex->aerosol_density,
                                             tex->ozone_density);
  }
  else {
    SKY_multiple_scattering_precompute_texture(pixels.data(),
                                               4,
                                               GPU_SKY_WIDTH,
                                               GPU_SKY_HEIGHT,
                                               tex->sun_elevation,
                                               tex->altitude,
                                               tex->air_density,
                                               tex->aerosol_density,
                                               tex->ozone_density);
  }

  float sun_rotation = fmodf(tex->sun_rotation, 2.0f * M_PI);
  if (sun_rotation < 0.0f) {
    sun_rotation += 2.0f * M_PI;
  }
  sun_rotation = 2.0f * M_PI - sun_rotation;

  XYZ_to_RGB xyz_to_rgb;
  get_XYZ_to_RGB_for_gpu(&xyz_to_rgb);

  /* To fix pole issue we clamp the v coordinate. */
  GPUSamplerState sampler = {GPU_SAMPLER_FILTERING_LINEAR,
                             GPU_SAMPLER_EXTEND_MODE_REPEAT,
                             GPU_SAMPLER_EXTEND_MODE_EXTEND};
  float layer;
  float sky_type = (tex->sky_model == SHD_SKY_SINGLE_SCATTERING) ? 0.0f : 1.0f;
  GPUNodeLink *sky_texture = GPU_image_sky(
      mat, GPU_SKY_WIDTH, GPU_SKY_HEIGHT, pixels.data(), &layer, sampler);
  return GPU_stack_link(mat,
                        node,
                        "node_tex_sky_nishita",
                        in,
                        out,
                        GPU_constant(&sky_type),
                        GPU_constant(&sun_rotation),
                        GPU_uniform(xyz_to_rgb.r),
                        GPU_uniform(xyz_to_rgb.g),
                        GPU_uniform(xyz_to_rgb.b),
                        sky_texture,
                        GPU_constant(&layer));
}

static void node_shader_update_sky(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *sockVector = bke::node_find_socket(*node, SOCK_IN, "Vector");

  NodeTexSky *tex = (NodeTexSky *)node->storage;
  bke::node_set_socket_availability(
      *ntree,
      *sockVector,
      !(ELEM(tex->sky_model, SHD_SKY_SINGLE_SCATTERING, SHD_SKY_MULTIPLE_SCATTERING) &&
        tex->sun_disc == 1));
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const NodeDeclaration &declaration = *params.node_type().static_declaration;
  if (params.in_out() == SOCK_OUT) {
    search_link_ops_for_declarations(params, declaration.outputs);
    return;
  }
  if (params.node_tree().typeinfo->validate_link(eNodeSocketDatatype(params.other_socket().type),
                                                 SOCK_FLOAT))
  {
    params.add_item(IFACE_("Vector"), [](LinkSearchOpParams &params) {
      bNode &node = params.add_node("ShaderNodeTexSky");
      NodeTexSky *tex = (NodeTexSky *)node.storage;
      tex->sun_disc = false;
      params.update_and_connect_available_socket(node, "Vector");
    });
  }
}

}  // namespace blender::nodes::node_shader_tex_sky_cc

/* node type definition */
void register_node_type_sh_tex_sky()
{
  namespace file_ns = blender::nodes::node_shader_tex_sky_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeTexSky", SH_NODE_TEX_SKY);
  ntype.ui_name = "Sky Texture";
  ntype.ui_description = "Generate a procedural sky texture";
  ntype.enum_name_legacy = "TEX_SKY";
  ntype.nclass = NODE_CLASS_TEXTURE;
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_tex_sky;
  blender::bke::node_type_size_preset(ntype, blender::bke::eNodeSizePreset::Default);
  ntype.initfunc = file_ns::node_shader_init_tex_sky;
  blender::bke::node_type_storage(
      ntype, "NodeTexSky", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_shader_gpu_tex_sky;
  ntype.updatefunc = file_ns::node_shader_update_sky;
  ntype.gather_link_search_ops = file_ns::node_gather_link_searches;

  blender::bke::node_register_type(ntype);
}
