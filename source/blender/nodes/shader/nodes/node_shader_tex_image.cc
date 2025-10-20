/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"
#include "node_util.hh"

#include "BKE_image.hh"
#include "BKE_node_runtime.hh"
#include "BKE_texture.h"

#include "IMB_colormanagement.hh"

#include "DEG_depsgraph_query.hh"

namespace blender::nodes::node_shader_tex_image_cc {

static void sh_node_tex_image_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>("Vector").implicit_field(NODE_DEFAULT_INPUT_POSITION_FIELD);
  b.add_output<decl::Color>("Color").no_muted_links();
  b.add_output<decl::Float>("Alpha").no_muted_links();
}

static void node_shader_init_tex_image(bNodeTree * /*ntree*/, bNode *node)
{
  NodeTexImage *tex = MEM_callocN<NodeTexImage>(__func__);
  BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
  BKE_texture_colormapping_default(&tex->base.color_mapping);
  BKE_imageuser_default(&tex->iuser);

  node->storage = tex;
}

static int node_shader_gpu_tex_image(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData * /*execdata*/,
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  Image *ima = (Image *)node->id;
  NodeTexImage *tex = (NodeTexImage *)node->storage;

  /* We get the image user from the original node, since GPU image keeps
   * a pointer to it and the dependency refreshes the original. */
  bNode *node_original = node->runtime->original ? node->runtime->original : node;
  NodeTexImage *tex_original = (NodeTexImage *)node_original->storage;
  ImageUser *iuser = &tex_original->iuser;

  if (!ima) {
    return GPU_stack_link(mat, node, "node_tex_image_empty", in, out);
  }

  GPUNodeLink **texco = &in[0].link;
  if (!*texco) {
    *texco = GPU_attribute(mat, CD_AUTO_FROM_NAME, "");
    node_shader_gpu_bump_tex_coord(mat, node, texco);
  }

  node_shader_gpu_tex_mapping(mat, node, in, out);

  GPUSamplerState sampler_state = GPUSamplerState::default_sampler();

  switch (tex->extension) {
    case SHD_IMAGE_EXTENSION_EXTEND:
      sampler_state.extend_x = GPU_SAMPLER_EXTEND_MODE_EXTEND;
      sampler_state.extend_yz = GPU_SAMPLER_EXTEND_MODE_EXTEND;
      break;
    case SHD_IMAGE_EXTENSION_REPEAT:
      sampler_state.extend_x = GPU_SAMPLER_EXTEND_MODE_REPEAT;
      sampler_state.extend_yz = GPU_SAMPLER_EXTEND_MODE_REPEAT;
      break;
    case SHD_IMAGE_EXTENSION_CLIP:
      sampler_state.extend_x = GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER;
      sampler_state.extend_yz = GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER;
      break;
    case SHD_IMAGE_EXTENSION_MIRROR:
      sampler_state.extend_x = GPU_SAMPLER_EXTEND_MODE_MIRRORED_REPEAT;
      sampler_state.extend_yz = GPU_SAMPLER_EXTEND_MODE_MIRRORED_REPEAT;
      break;
    default:
      break;
  }

  if (tex->interpolation != SHD_INTERP_CLOSEST) {
    /* TODO(fclem): For now assume mipmap is always enabled. */
    sampler_state.filtering = GPU_SAMPLER_FILTERING_ANISOTROPIC | GPU_SAMPLER_FILTERING_LINEAR |
                              GPU_SAMPLER_FILTERING_MIPMAP;
  }
  const bool use_cubic = ELEM(tex->interpolation, SHD_INTERP_CUBIC, SHD_INTERP_SMART);

  /* Only use UDIM tiles if projection is flat.
   * Otherwise treat the first tile as a single image. (See #141776). */
  const bool use_udim = ima->source == IMA_SRC_TILED && tex->projection == SHD_PROJ_FLAT;
  if (use_udim) {
    const char *gpu_node_name = use_cubic ? "node_tex_tile_cubic" : "node_tex_tile_linear";
    GPUNodeLink *gpu_image, *gpu_image_tile_mapping;
    GPU_image_tiled(mat, ima, iuser, sampler_state, &gpu_image, &gpu_image_tile_mapping);
    /* UDIM tiles needs a `sampler2DArray` and `sampler1DArray` for tile mapping. */
    GPU_stack_link(mat, node, gpu_node_name, in, out, gpu_image, gpu_image_tile_mapping);
  }
  else {
    const char *gpu_node_name = use_cubic ? "node_tex_image_cubic" : "node_tex_image_linear";

    switch (tex->projection) {
      case SHD_PROJ_FLAT: {
        GPUNodeLink *gpu_image = GPU_image(mat, ima, iuser, sampler_state);
        GPU_stack_link(mat, node, gpu_node_name, in, out, gpu_image);
        break;
      }
      case SHD_PROJ_BOX: {
        gpu_node_name = use_cubic ? "tex_box_sample_cubic" : "tex_box_sample_linear";
        GPUNodeLink *vnor, *wnor, *col1, *col2, *col3;
        GPUNodeLink *blend = GPU_uniform(&tex->projection_blend);
        GPUNodeLink *gpu_image = GPU_image(mat, ima, iuser, sampler_state);
        GPU_link(mat, "world_normals_get", &vnor);
        GPU_link(mat, "normal_transform_world_to_object", vnor, &wnor);
        GPU_link(mat, gpu_node_name, in[0].link, wnor, gpu_image, &col1, &col2, &col3);
        GPU_link(mat, "tex_box_blend", wnor, col1, col2, col3, blend, &out[0].link, &out[1].link);
        break;
      }
      case SHD_PROJ_SPHERE: {
        /* This projection is known to have a derivative discontinuity.
         * Hide it by turning off mipmapping. */
        sampler_state.disable_filtering_flag(GPU_SAMPLER_FILTERING_MIPMAP);
        GPUNodeLink *gpu_image = GPU_image(mat, ima, iuser, sampler_state);
        GPU_link(mat, "point_texco_remap_square", *texco, texco);
        GPU_link(mat, "point_map_to_sphere", *texco, texco);
        GPU_stack_link(mat, node, gpu_node_name, in, out, gpu_image);
        break;
      }
      case SHD_PROJ_TUBE: {
        /* This projection is known to have a derivative discontinuity.
         * Hide it by turning off mipmapping. */
        sampler_state.disable_filtering_flag(GPU_SAMPLER_FILTERING_MIPMAP);
        GPUNodeLink *gpu_image = GPU_image(mat, ima, iuser, sampler_state);
        GPU_link(mat, "point_texco_remap_square", *texco, texco);
        GPU_link(mat, "point_map_to_tube", *texco, texco);
        GPU_stack_link(mat, node, gpu_node_name, in, out, gpu_image);
        break;
      }
    }
  }

  if (out[0].hasoutput) {
    if (ELEM(ima->alpha_mode, IMA_ALPHA_IGNORE, IMA_ALPHA_CHANNEL_PACKED) ||
        IMB_colormanagement_space_name_is_data(ima->colorspace_settings.name))
    {
      /* Don't let alpha affect color output in these cases. */
      GPU_link(mat, "color_alpha_clear", out[0].link, &out[0].link);
    }
    else {
      /* Output premultiplied alpha depending on alpha socket usage. This makes
       * it so that if we blend the color with a transparent shader using alpha as
       * a factor, we don't multiply alpha into the color twice. And if we do
       * not, then there will be no artifacts from zero alpha areas. */
      if (ima->alpha_mode == IMA_ALPHA_PREMUL) {
        if (out[1].hasoutput) {
          GPU_link(mat, "color_alpha_unpremultiply", out[0].link, &out[0].link);
        }
        else {
          GPU_link(mat, "color_alpha_clear", out[0].link, &out[0].link);
        }
      }
      else {
        if (out[1].hasoutput) {
          GPU_link(mat, "color_alpha_clear", out[0].link, &out[0].link);
        }
        else {
          GPU_link(mat, "color_alpha_premultiply", out[0].link, &out[0].link);
        }
      }
    }
  }

  return true;
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  /* Getting node name for Color output. This name will be used for <image> node. */
  std::string image_node_name = node_name("Color");

  NodeItem res = graph_.get_node(image_node_name);
  if (!res.node) {
    res = val(MaterialX::Color4(1.0f, 0.0f, 1.0f, 1.0f));

    Image *image = (Image *)node_->id;
    if (image) {
      NodeTexImage *tex_image = static_cast<NodeTexImage *>(node_->storage);

      std::string image_path = image->id.name;
      if (graph_.export_params.image_fn) {
        Scene *scene = DEG_get_input_scene(graph_.depsgraph);
        Main *bmain = DEG_get_bmain(graph_.depsgraph);
        image_path = graph_.export_params.image_fn(bmain, scene, image, &tex_image->iuser);
      }

      NodeItem vector = get_input_link("Vector", NodeItem::Type::Vector2);
      if (!vector) {
        vector = texcoord_node();
      }
      /* TODO: add math to vector depending of tex_image->projection */

      std::string filtertype;
      switch (tex_image->interpolation) {
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
      std::string addressmode;
      switch (tex_image->extension) {
        case SHD_IMAGE_EXTENSION_REPEAT:
          addressmode = "periodic";
          break;
        case SHD_IMAGE_EXTENSION_EXTEND:
          addressmode = "clamp";
          break;
        case SHD_IMAGE_EXTENSION_CLIP:
          addressmode = "constant";
          break;
        case SHD_IMAGE_EXTENSION_MIRROR:
          addressmode = "mirror";
          break;
        default:
          BLI_assert_unreachable();
      }

      NodeItem::Type node_type = NodeItem::Type::Color4;
      const char *node_colorspace = nullptr;

      const char *image_colorspace = image->colorspace_settings.name;
      if (IMB_colormanagement_space_name_is_data(image_colorspace)) {
        node_type = NodeItem::Type::Vector4;
      }
      else if (IMB_colormanagement_space_name_is_scene_linear(image_colorspace)) {
        node_colorspace = "lin_rec709";
      }
      else if (IMB_colormanagement_space_name_is_srgb(image_colorspace)) {
        node_colorspace = "srgb_texture";
      }

      res = create_node("image",
                        node_type,
                        {{"texcoord", vector},
                         {"filtertype", val(filtertype)},
                         {"uaddressmode", val(addressmode)},
                         {"vaddressmode", val(addressmode)}});
      res.set_input("file", image_path, NodeItem::Type::Filename);
      res.node->setName(image_node_name);
      if (node_colorspace) {
        res.node->setAttribute("colorspace", node_colorspace);
      }
    }
  }

  if (STREQ(socket_out_->identifier, "Alpha")) {
    res = res[3];
  }
  return res;
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_tex_image_cc

void register_node_type_sh_tex_image()
{
  namespace file_ns = blender::nodes::node_shader_tex_image_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeTexImage", SH_NODE_TEX_IMAGE);
  ntype.ui_name = "Image Texture";
  ntype.ui_description = "Sample an image file as a texture";
  ntype.enum_name_legacy = "TEX_IMAGE";
  ntype.nclass = NODE_CLASS_TEXTURE;
  ntype.declare = file_ns::sh_node_tex_image_declare;
  ntype.initfunc = file_ns::node_shader_init_tex_image;
  blender::bke::node_type_storage(
      ntype, "NodeTexImage", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_shader_gpu_tex_image;
  ntype.labelfunc = node_image_label;
  blender::bke::node_type_size_preset(ntype, blender::bke::eNodeSizePreset::Large);
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::node_register_type(ntype);
}
