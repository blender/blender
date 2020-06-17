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

static bNodeSocketTemplate sh_node_tex_sky_in[] = {
    {SOCK_VECTOR, N_("Vector"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
    {-1, ""},
};

static bNodeSocketTemplate sh_node_tex_sky_out[] = {
    {SOCK_RGBA, N_("Color"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_NO_INTERNAL_LINK},
    {-1, ""},
};

static void node_shader_init_tex_sky(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeTexSky *tex = MEM_callocN(sizeof(NodeTexSky), "NodeTexSky");
  BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
  BKE_texture_colormapping_default(&tex->base.color_mapping);
  tex->sun_direction[0] = 0.0f;
  tex->sun_direction[1] = 0.0f;
  tex->sun_direction[2] = 1.0f;
  tex->turbidity = 2.2f;
  tex->ground_albedo = 0.3f;
  tex->sun_disc = true;
  tex->sun_size = DEG2RADF(0.545);
  tex->sun_elevation = M_PI_2;
  tex->sun_rotation = 0.0f;
  tex->altitude = 0;
  tex->air_density = 1.0f;
  tex->dust_density = 1.0f;
  tex->ozone_density = 1.0f;
  tex->sky_model = SHD_SKY_NISHITA;
  node->storage = tex;
}

static int node_shader_gpu_tex_sky(GPUMaterial *mat,
                                   bNode *node,
                                   bNodeExecData *UNUSED(execdata),
                                   GPUNodeStack *in,
                                   GPUNodeStack *out)
{
  if (!in[0].link) {
    in[0].link = GPU_attribute(mat, CD_ORCO, "");
  }

  node_shader_gpu_tex_mapping(mat, node, in, out);

  return GPU_stack_link(mat, node, "node_tex_sky", in, out);
}

/* node type definition */
void register_node_type_sh_tex_sky(void)
{
  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_TEX_SKY, "Sky Texture", NODE_CLASS_TEXTURE, 0);
  node_type_socket_templates(&ntype, sh_node_tex_sky_in, sh_node_tex_sky_out);
  node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
  node_type_init(&ntype, node_shader_init_tex_sky);
  node_type_storage(&ntype, "NodeTexSky", node_free_standard_storage, node_copy_standard_storage);
  node_type_gpu(&ntype, node_shader_gpu_tex_sky);

  nodeRegisterType(&ntype);
}
