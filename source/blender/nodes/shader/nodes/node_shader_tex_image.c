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

static bNodeSocketTemplate sh_node_tex_image_in[] = {
    {SOCK_VECTOR, 1, N_("Vector"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
    {-1, 0, ""},
};

static bNodeSocketTemplate sh_node_tex_image_out[] = {
    {SOCK_RGBA,
     0,
     N_("Color"),
     0.0f,
     0.0f,
     0.0f,
     0.0f,
     0.0f,
     1.0f,
     PROP_NONE,
     SOCK_NO_INTERNAL_LINK},
    {SOCK_FLOAT,
     0,
     N_("Alpha"),
     0.0f,
     0.0f,
     0.0f,
     0.0f,
     0.0f,
     1.0f,
     PROP_NONE,
     SOCK_NO_INTERNAL_LINK},
    {-1, 0, ""},
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
  static const char *names[] = {
      "node_tex_image_linear",
      "node_tex_image_nearest",
      "node_tex_image_cubic",
      "node_tex_image_smart",
  };
  static const char *names_box[] = {
      "tex_box_sample_linear",
      "tex_box_sample_nearest",
      "tex_box_sample_cubic",
      "tex_box_sample_smart",
  };
  static const char *names_clip[] = {
      "tex_clip_linear",
      "tex_clip_nearest",
      "tex_clip_cubic",
      "tex_clip_smart",
  };

  Image *ima = (Image *)node->id;
  NodeTexImage *tex = node->storage;

  /* We get the image user from the original node, since GPU image keeps
   * a pointer to it and the dependency refreshes the original. */
  bNode *node_original = node->original ? node->original : node;
  NodeTexImage *tex_original = node_original->storage;
  ImageUser *iuser = &tex_original->iuser;

  const char *gpu_node_name = (tex->projection == SHD_PROJ_BOX) ? names_box[tex->interpolation] :
                                                                  names[tex->interpolation];
  bool do_texco_extend = (tex->extension != SHD_IMAGE_EXTENSION_REPEAT);
  const bool do_texco_clip = (tex->extension == SHD_IMAGE_EXTENSION_CLIP);

  if (do_texco_extend && (tex->projection != SHD_PROJ_BOX) &&
      ELEM(tex->interpolation, SHD_INTERP_CUBIC, SHD_INTERP_SMART)) {
    gpu_node_name = "node_tex_image_cubic_extend";
    /* We do it inside the sampling function */
    do_texco_extend = false;
  }

  GPUNodeLink *norm, *col1, *col2, *col3, *input_coords, *gpu_image;
  GPUNodeLink *vnor, *ob_mat, *blend;
  GPUNodeLink **texco = &in[0].link;

  if (!ima) {
    return GPU_stack_link(mat, node, "node_tex_image_empty", in, out);
  }

  if (!*texco) {
    *texco = GPU_attribute(CD_MTFACE, "");
  }

  node_shader_gpu_tex_mapping(mat, node, in, out);

  switch (tex->projection) {
    case SHD_PROJ_FLAT:
      if (do_texco_clip) {
        GPU_link(mat, "set_rgb", *texco, &input_coords);
      }
      if (do_texco_extend) {
        GPU_link(mat, "point_texco_clamp", *texco, GPU_image(ima, iuser), texco);
      }
      GPU_stack_link(mat, node, gpu_node_name, in, out, GPU_image(ima, iuser));
      break;

    case SHD_PROJ_BOX:
      vnor = GPU_builtin(GPU_WORLD_NORMAL);
      ob_mat = GPU_builtin(GPU_OBJECT_MATRIX);
      blend = GPU_uniform(&tex->projection_blend);
      gpu_image = GPU_image(ima, iuser);

      /* equivalent to normal_world_to_object */
      GPU_link(mat, "normal_transform_transposed_m4v3", vnor, ob_mat, &norm);
      GPU_link(mat, gpu_node_name, *texco, norm, GPU_image(ima, iuser), &col1, &col2, &col3);
      GPU_stack_link(
          mat, node, "node_tex_image_box", in, out, norm, col1, col2, col3, gpu_image, blend);
      break;

    case SHD_PROJ_SPHERE:
      GPU_link(mat, "point_texco_remap_square", *texco, texco);
      GPU_link(mat, "point_map_to_sphere", *texco, texco);
      if (do_texco_clip) {
        GPU_link(mat, "set_rgb", *texco, &input_coords);
      }
      if (do_texco_extend) {
        GPU_link(mat, "point_texco_clamp", *texco, GPU_image(ima, iuser), texco);
      }
      GPU_stack_link(mat, node, gpu_node_name, in, out, GPU_image(ima, iuser));
      break;

    case SHD_PROJ_TUBE:
      GPU_link(mat, "point_texco_remap_square", *texco, texco);
      GPU_link(mat, "point_map_to_tube", *texco, texco);
      if (do_texco_clip) {
        GPU_link(mat, "set_rgb", *texco, &input_coords);
      }
      if (do_texco_extend) {
        GPU_link(mat, "point_texco_clamp", *texco, GPU_image(ima, iuser), texco);
      }
      GPU_stack_link(mat, node, gpu_node_name, in, out, GPU_image(ima, iuser));
      break;
  }

  if (tex->projection != SHD_PROJ_BOX) {
    if (do_texco_clip) {
      gpu_node_name = names_clip[tex->interpolation];
      in[0].link = input_coords;
      GPU_stack_link(mat, node, gpu_node_name, in, out, GPU_image(ima, iuser), out[0].link);
    }
  }

  if (out[0].hasoutput) {
    if (out[1].hasoutput &&
        !IMB_colormanagement_space_name_is_data(ima->colorspace_settings.name)) {
      GPU_link(mat, "tex_color_alpha_unpremultiply", out[0].link, &out[0].link);
    }
    else {
      GPU_link(mat, "tex_color_alpha_clear", out[0].link, &out[0].link);
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
