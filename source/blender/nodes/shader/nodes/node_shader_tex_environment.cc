/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"
#include "node_util.hh"

#include "BKE_image.h"
#include "BKE_node_runtime.hh"
#include "BKE_texture.h"

#include "IMB_colormanagement.hh"

#include "DEG_depsgraph_query.hh"

namespace blender::nodes::node_shader_tex_environment_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>("Vector").hide_value();
  b.add_output<decl::Color>("Color").no_muted_links();
}

static void node_shader_init_tex_environment(bNodeTree * /*ntree*/, bNode *node)
{
  NodeTexEnvironment *tex = MEM_cnew<NodeTexEnvironment>("NodeTexEnvironment");
  BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
  BKE_texture_colormapping_default(&tex->base.color_mapping);
  tex->projection = SHD_PROJ_EQUIRECTANGULAR;
  BKE_imageuser_default(&tex->iuser);

  node->storage = tex;
}

static int node_shader_gpu_tex_environment(GPUMaterial *mat,
                                           bNode *node,
                                           bNodeExecData * /*execdata*/,
                                           GPUNodeStack *in,
                                           GPUNodeStack *out)
{
  Image *ima = (Image *)node->id;
  NodeTexEnvironment *tex = (NodeTexEnvironment *)node->storage;

  /* We get the image user from the original node, since GPU image keeps
   * a pointer to it and the dependency refreshes the original. */
  bNode *node_original = node->runtime->original ? node->runtime->original : node;
  NodeTexImage *tex_original = (NodeTexImage *)node_original->storage;
  ImageUser *iuser = &tex_original->iuser;
  GPUSamplerState sampler = {GPU_SAMPLER_FILTERING_LINEAR | GPU_SAMPLER_FILTERING_ANISOTROPIC,
                             GPU_SAMPLER_EXTEND_MODE_REPEAT,
                             GPU_SAMPLER_EXTEND_MODE_REPEAT};
  /* TODO(@fclem): For now assume mipmap is always enabled. */
  if (true) {
    sampler.enable_filtering_flag(GPU_SAMPLER_FILTERING_MIPMAP);
  }

  GPUNodeLink *outalpha;

  /* HACK(@fclem): For lookdev mode: do not compile an empty environment and just create an empty
   * texture entry point. We manually bind to it after #DRW_shgroup_add_material_resources(). */
  if (!ima && !GPU_material_flag_get(mat, GPU_MATFLAG_LOOKDEV_HACK)) {
    return GPU_stack_link(mat, node, "node_tex_environment_empty", in, out);
  }

  if (!in[0].link) {
    GPU_link(mat, "node_tex_coord_position", &in[0].link);
    node_shader_gpu_bump_tex_coord(mat, node, &in[0].link);
  }

  node_shader_gpu_tex_mapping(mat, node, in, out);

  /* Compute texture coordinate. */
  if (tex->projection == SHD_PROJ_EQUIRECTANGULAR) {
    GPU_link(mat, "node_tex_environment_equirectangular", in[0].link, &in[0].link);
    /* To fix pole issue we clamp the v coordinate. */
    sampler.extend_yz = GPU_SAMPLER_EXTEND_MODE_EXTEND;
    /* Force the highest mipmap and don't do anisotropic filtering.
     * This is to fix the artifact caused by derivatives discontinuity. */
    sampler.disable_filtering_flag(GPU_SAMPLER_FILTERING_MIPMAP |
                                   GPU_SAMPLER_FILTERING_ANISOTROPIC);
  }
  else {
    GPU_link(mat, "node_tex_environment_mirror_ball", in[0].link, &in[0].link);
    /* Fix pole issue. */
    sampler.extend_x = GPU_SAMPLER_EXTEND_MODE_EXTEND;
    sampler.extend_yz = GPU_SAMPLER_EXTEND_MODE_EXTEND;
  }

  const char *gpu_fn;
  static const char *names[] = {
      "node_tex_image_linear",
      "node_tex_image_cubic",
  };

  switch (tex->interpolation) {
    case SHD_INTERP_LINEAR:
      gpu_fn = names[0];
      break;
    case SHD_INTERP_CLOSEST:
      sampler.disable_filtering_flag(GPU_SAMPLER_FILTERING_LINEAR | GPU_SAMPLER_FILTERING_MIPMAP);
      gpu_fn = names[0];
      break;
    default:
      gpu_fn = names[1];
      break;
  }

  /* Sample texture with correct interpolation. */
  GPU_link(mat, gpu_fn, in[0].link, GPU_image(mat, ima, iuser, sampler), &out[0].link, &outalpha);

  if (out[0].hasoutput && ima) {
    if (ELEM(ima->alpha_mode, IMA_ALPHA_IGNORE, IMA_ALPHA_CHANNEL_PACKED) ||
        IMB_colormanagement_space_name_is_data(ima->colorspace_settings.name))
    {
      /* Don't let alpha affect color output in these cases. */
      GPU_link(mat, "color_alpha_clear", out[0].link, &out[0].link);
    }
    else {
      /* Always output with premultiplied alpha. */
      if (ima->alpha_mode == IMA_ALPHA_PREMUL) {
        GPU_link(mat, "color_alpha_clear", out[0].link, &out[0].link);
      }
      else {
        GPU_link(mat, "color_alpha_premultiply", out[0].link, &out[0].link);
      }
    }
  }

  return true;
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  NodeItem res = val(MaterialX::Color4(1.0f, 0.0f, 1.0f, 1.0f));

  Image *image = (Image *)node_->id;
  if (!image) {
    return res;
  }

  NodeTexEnvironment *tex_env = static_cast<NodeTexEnvironment *>(node_->storage);

  std::string image_path = image->id.name;
  if (export_image_fn_) {
    Scene *scene = DEG_get_input_scene(depsgraph_);
    Main *bmain = DEG_get_bmain(depsgraph_);
    image_path = export_image_fn_(bmain, scene, image, &tex_env->iuser);
  }

  NodeItem vector = get_input_link("Vector", NodeItem::Type::Vector2);
  if (!vector) {
    vector = texcoord_node();
  }
  /* TODO: texture-coordinates should be translated to spherical coordinates. */

  std::string filtertype;
  switch (tex_env->interpolation) {
    case SHD_INTERP_LINEAR:
      filtertype = "linear";
      break;
    case SHD_INTERP_CLOSEST:
      filtertype = "closest";
      break;
    case SHD_INTERP_CUBIC:
    case SHD_INTERP_SMART:
      filtertype = "cubic";
      break;
    default:
      BLI_assert_unreachable();
  }

  res = create_node("image", NodeItem::Type::Color4);
  res.set_input("file", image_path, NodeItem::Type::Filename);
  res.set_input("texcoord", vector);
  res.set_input("filtertype", val(filtertype));

  return res;
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_tex_environment_cc

/* node type definition */
void register_node_type_sh_tex_environment()
{
  namespace file_ns = blender::nodes::node_shader_tex_environment_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_TEX_ENVIRONMENT, "Environment Texture", NODE_CLASS_TEXTURE);
  ntype.declare = file_ns::node_declare;
  ntype.initfunc = file_ns::node_shader_init_tex_environment;
  node_type_storage(
      &ntype, "NodeTexEnvironment", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_shader_gpu_tex_environment;
  ntype.labelfunc = node_image_label;
  blender::bke::node_type_size_preset(&ntype, blender::bke::eNodeSizePreset::LARGE);
  ntype.materialx_fn = file_ns::node_shader_materialx;

  nodeRegisterType(&ntype);
}
