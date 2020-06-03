/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

#include "../node_shader_util.h"

#include "GPU_draw.h"

/* **************** OUTPUT ******************** */

static bNodeSocketTemplate sh_node_tex_image_in[] = {
    {SOCK_VECTOR, N_("Vector"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
    {-1, ""},
};

static bNodeSocketTemplate sh_node_tex_image_out[] = {
    {SOCK_RGBA, N_("Color"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_NO_INTERNAL_LINK},
    {SOCK_FLOAT,
     N_("Alpha"),
     0.0f,
     0.0f,
     0.0f,
     0.0f,
     0.0f,
     1.0f,
     PROP_NONE,
     SOCK_NO_INTERNAL_LINK},
    {-1, ""},
};

static void node_shader_init_tex_image(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeTexImage *tex = MEM_callocN(sizeof(NodeTexImage), "NodeTexImage");
  BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
  BKE_texture_colormapping_default(&tex->base.color_mapping);
  BKE_imageuser_default(&tex->iuser);

  node->storage = tex;
}

static int node_shader_gpu_tex_image(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData *UNUSED(execdata),
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  Image *ima = (Image *)node->id;
  NodeTexImage *tex = node->storage;

  /* We get the image user from the original node, since GPU image keeps
   * a pointer to it and the dependency refreshes the original. */
  bNode *node_original = node->original ? node->original : node;
  NodeTexImage *tex_original = node_original->storage;
  ImageUser *iuser = &tex_original->iuser;

  if (!ima) {
    return GPU_stack_link(mat, node, "node_tex_image_empty", in, out);
  }

  GPUNodeLink **texco = &in[0].link;
  if (!*texco) {
    *texco = GPU_attribute(mat, CD_MTFACE, "");
    node_shader_gpu_bump_tex_coord(mat, node, texco);
  }

  node_shader_gpu_tex_mapping(mat, node, in, out);

  eGPUSamplerState sampler_state = 0;

  switch (tex->extension) {
    case SHD_IMAGE_EXTENSION_REPEAT:
      sampler_state |= GPU_SAMPLER_REPEAT;
      break;
    case SHD_IMAGE_EXTENSION_CLIP:
      sampler_state |= GPU_SAMPLER_CLAMP_BORDER;
      break;
    default:
      break;
  }

  if (tex->interpolation != SHD_INTERP_CLOSEST) {
    sampler_state |= GPU_SAMPLER_ANISO | GPU_SAMPLER_FILTER;
    sampler_state |= GPU_get_mipmap() ? GPU_SAMPLER_MIPMAP : 0;
  }
  const bool use_cubic = ELEM(tex->interpolation, SHD_INTERP_CUBIC, SHD_INTERP_SMART);

  if (ima->source == IMA_SRC_TILED) {
    const char *gpu_node_name = use_cubic ? "node_tex_tile_cubic" : "node_tex_tile_linear";
    GPUNodeLink *gpu_image = GPU_image(mat, ima, iuser, sampler_state);
    GPUNodeLink *gpu_image_tile_mapping = GPU_image_tiled_mapping(mat, ima, iuser);
    /* UDIM tiles needs a samper2DArray and sampler1DArray for tile mapping. */
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
        GPUNodeLink *wnor, *col1, *col2, *col3;
        GPUNodeLink *vnor = GPU_builtin(GPU_WORLD_NORMAL);
        GPUNodeLink *ob_mat = GPU_builtin(GPU_OBJECT_MATRIX);
        GPUNodeLink *blend = GPU_uniform(&tex->projection_blend);
        GPUNodeLink *gpu_image = GPU_image(mat, ima, iuser, sampler_state);
        /* equivalent to normal_world_to_object */
        GPU_link(mat, "normal_transform_transposed_m4v3", vnor, ob_mat, &wnor);
        GPU_link(mat, gpu_node_name, in[0].link, wnor, gpu_image, &col1, &col2, &col3);
        GPU_link(mat, "tex_box_blend", wnor, col1, col2, col3, blend, &out[0].link, &out[1].link);
        break;
      }
      case SHD_PROJ_SPHERE: {
        /* This projection is known to have a derivative discontinuity.
         * Hide it by turning off mipmapping. */
        sampler_state &= ~GPU_SAMPLER_MIPMAP;
        GPUNodeLink *gpu_image = GPU_image(mat, ima, iuser, sampler_state);
        GPU_link(mat, "point_texco_remap_square", *texco, texco);
        GPU_link(mat, "point_map_to_sphere", *texco, texco);
        GPU_stack_link(mat, node, gpu_node_name, in, out, gpu_image);
        break;
      }
      case SHD_PROJ_TUBE: {
        /* This projection is known to have a derivative discontinuity.
         * Hide it by turning off mipmapping. */
        sampler_state &= ~GPU_SAMPLER_MIPMAP;
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
        IMB_colormanagement_space_name_is_data(ima->colorspace_settings.name)) {
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

/* node type definition */
void register_node_type_sh_tex_image(void)
{
  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_TEX_IMAGE, "Image Texture", NODE_CLASS_TEXTURE, 0);
  node_type_socket_templates(&ntype, sh_node_tex_image_in, sh_node_tex_image_out);
  node_type_init(&ntype, node_shader_init_tex_image);
  node_type_storage(
      &ntype, "NodeTexImage", node_free_standard_storage, node_copy_standard_storage);
  node_type_gpu(&ntype, node_shader_gpu_tex_image);
  node_type_label(&ntype, node_image_label);
  node_type_size_preset(&ntype, NODE_SIZE_LARGE);

  nodeRegisterType(&ntype);
}
