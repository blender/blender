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

/* **************** OUTPUT ******************** */

static bNodeSocketTemplate sh_node_tex_environment_in[] = {
    {SOCK_VECTOR, N_("Vector"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
    {-1, ""},
};

static bNodeSocketTemplate sh_node_tex_environment_out[] = {
    {SOCK_RGBA, N_("Color"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_NO_INTERNAL_LINK},
    {-1, ""},
};

static void node_shader_init_tex_environment(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeTexEnvironment *tex = MEM_callocN(sizeof(NodeTexEnvironment), "NodeTexEnvironment");
  BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
  BKE_texture_colormapping_default(&tex->base.color_mapping);
  tex->projection = SHD_PROJ_EQUIRECTANGULAR;
  BKE_imageuser_default(&tex->iuser);

  node->storage = tex;
}

static int node_shader_gpu_tex_environment(GPUMaterial *mat,
                                           bNode *node,
                                           bNodeExecData *UNUSED(execdata),
                                           GPUNodeStack *in,
                                           GPUNodeStack *out)
{
  Image *ima = (Image *)node->id;
  NodeTexEnvironment *tex = node->storage;

  /* We get the image user from the original node, since GPU image keeps
   * a pointer to it and the dependency refreshes the original. */
  bNode *node_original = node->original ? node->original : node;
  NodeTexImage *tex_original = node_original->storage;
  ImageUser *iuser = &tex_original->iuser;
  eGPUSamplerState sampler_state = GPU_SAMPLER_MAX;

  GPUNodeLink *outalpha;

  if (!ima) {
    return GPU_stack_link(mat, node, "node_tex_environment_empty", in, out);
  }

  if (!in[0].link) {
    GPU_link(mat, "node_tex_environment_texco", GPU_builtin(GPU_VIEW_POSITION), &in[0].link);
    node_shader_gpu_bump_tex_coord(mat, node, &in[0].link);
  }

  node_shader_gpu_tex_mapping(mat, node, in, out);

  /* Compute texture coordinate. */
  if (tex->projection == SHD_PROJ_EQUIRECTANGULAR) {
    /* To fix pole issue we clamp the v coordinate. The clamp value depends on the filter size. */
    float clamp_size = (ELEM(tex->interpolation, SHD_INTERP_CUBIC, SHD_INTERP_SMART)) ? 1.5 : 0.5;
    GPU_link(mat,
             "node_tex_environment_equirectangular",
             in[0].link,
             GPU_constant(&clamp_size),
             GPU_image(mat, ima, iuser, sampler_state),
             &in[0].link);
  }
  else {
    GPU_link(mat, "node_tex_environment_mirror_ball", in[0].link, &in[0].link);
  }

  /* Sample texture with correct interpolation. */
  switch (tex->interpolation) {
    case SHD_INTERP_LINEAR:
      /* Force the highest mipmap and don't do anisotropic filtering.
       * This is to fix the artifact caused by derivatives discontinuity. */
      GPU_link(mat,
               "node_tex_image_linear_no_mip",
               in[0].link,
               GPU_image(mat, ima, iuser, sampler_state),
               &out[0].link,
               &outalpha);
      break;
    case SHD_INTERP_CLOSEST:
      GPU_link(mat,
               "node_tex_image_nearest",
               in[0].link,
               GPU_image(mat, ima, iuser, sampler_state),
               &out[0].link,
               &outalpha);
      break;
    default:
      GPU_link(mat,
               "node_tex_image_cubic",
               in[0].link,
               GPU_image(mat, ima, iuser, sampler_state),
               &out[0].link,
               &outalpha);
      break;
  }

  if (out[0].hasoutput) {
    if (ELEM(ima->alpha_mode, IMA_ALPHA_IGNORE, IMA_ALPHA_CHANNEL_PACKED) ||
        IMB_colormanagement_space_name_is_data(ima->colorspace_settings.name)) {
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

/* node type definition */
void register_node_type_sh_tex_environment(void)
{
  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_TEX_ENVIRONMENT, "Environment Texture", NODE_CLASS_TEXTURE, 0);
  node_type_socket_templates(&ntype, sh_node_tex_environment_in, sh_node_tex_environment_out);
  node_type_init(&ntype, node_shader_init_tex_environment);
  node_type_storage(
      &ntype, "NodeTexEnvironment", node_free_standard_storage, node_copy_standard_storage);
  node_type_gpu(&ntype, node_shader_gpu_tex_environment);
  node_type_label(&ntype, node_image_label);
  node_type_size_preset(&ntype, NODE_SIZE_LARGE);

  nodeRegisterType(&ntype);
}
