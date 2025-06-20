/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"
#include "node_util.hh"
#include "sky_model.h"

#include "BLI_math_rotation.h"
#include "BLI_task.hh"

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

  if (RNA_enum_get(ptr, "sky_type") == SHD_SKY_NISHITA) {
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
    col->prop(ptr, "dust_density", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
    col->prop(ptr, "ozone_density", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  }
}

static void node_shader_init_tex_sky(bNodeTree * /*ntree*/, bNode *node)
{
  NodeTexSky *tex = MEM_callocN<NodeTexSky>("NodeTexSky");
  BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
  BKE_texture_colormapping_default(&tex->base.color_mapping);
  tex->sun_disc = true;
  tex->sun_size = DEG2RADF(0.545f);
  tex->sun_intensity = 1.0f;
  tex->sun_elevation = DEG2RADF(15.0f);
  tex->sun_rotation = 0.0f;
  tex->altitude = 0.0f;
  tex->air_density = 1.0f;
  tex->dust_density = 1.0f;
  tex->ozone_density = 1.0f;
  tex->sky_model = SHD_SKY_NISHITA;
  node->storage = tex;
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

  /* Nishita */

  Array<float> pixels(4 * GPU_SKY_WIDTH * GPU_SKY_HEIGHT);

  threading::parallel_for(IndexRange(GPU_SKY_HEIGHT), 2, [&](IndexRange range) {
    SKY_nishita_skymodel_precompute_texture(pixels.data(),
                                            4,
                                            range.first(),
                                            range.one_after_last(),
                                            GPU_SKY_WIDTH,
                                            GPU_SKY_HEIGHT,
                                            tex->sun_elevation,
                                            tex->altitude,
                                            tex->air_density,
                                            tex->dust_density,
                                            tex->ozone_density);
  });

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
  GPUNodeLink *sky_texture = GPU_image_sky(
      mat, GPU_SKY_WIDTH, GPU_SKY_HEIGHT, pixels.data(), &layer, sampler);
  return GPU_stack_link(mat,
                        node,
                        "node_tex_sky_nishita",
                        in,
                        out,
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
      *ntree, *sockVector, !(tex->sky_model == 0 && tex->sun_disc == 1));
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
  blender::bke::node_type_size_preset(ntype, blender::bke::eNodeSizePreset::Middle);
  ntype.initfunc = file_ns::node_shader_init_tex_sky;
  blender::bke::node_type_storage(
      ntype, "NodeTexSky", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_shader_gpu_tex_sky;
  /* Remove vector input for Nishita sky model. */
  ntype.updatefunc = file_ns::node_shader_update_sky;
  ntype.gather_link_search_ops = file_ns::node_gather_link_searches;

  blender::bke::node_register_type(ntype);
}
