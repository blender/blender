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

/* **************** WAVE ******************** */

static bNodeSocketTemplate sh_node_tex_wave_in[] = {
    {SOCK_VECTOR, N_("Vector"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
    {SOCK_FLOAT, N_("Scale"), 5.0f, 0.0f, 0.0f, 0.0f, -1000.0f, 1000.0f},
    {SOCK_FLOAT, N_("Distortion"), 0.0f, 0.0f, 0.0f, 0.0f, -1000.0f, 1000.0f},
    {SOCK_FLOAT, N_("Detail"), 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 16.0f},
    {SOCK_FLOAT, N_("Detail Scale"), 1.0f, 0.0f, 0.0f, 0.0f, -1000.0f, 1000.0f},
    {SOCK_FLOAT, N_("Phase Offset"), 0.0f, 0.0f, 0.0f, 0.0f, -1000.0f, 1000.0f},
    {-1, ""},
};

static bNodeSocketTemplate sh_node_tex_wave_out[] = {
    {SOCK_RGBA, N_("Color"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_NO_INTERNAL_LINK},
    {SOCK_FLOAT,
     N_("Fac"),
     0.0f,
     0.0f,
     0.0f,
     0.0f,
     0.0f,
     1.0f,
     PROP_FACTOR,
     SOCK_NO_INTERNAL_LINK},
    {-1, ""},
};

static void node_shader_init_tex_wave(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeTexWave *tex = MEM_callocN(sizeof(NodeTexWave), "NodeTexWave");
  BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
  BKE_texture_colormapping_default(&tex->base.color_mapping);
  tex->wave_type = SHD_WAVE_BANDS;
  tex->bands_direction = SHD_WAVE_BANDS_DIRECTION_X;
  tex->rings_direction = SHD_WAVE_RINGS_DIRECTION_X;
  tex->wave_profile = SHD_WAVE_PROFILE_SIN;
  node->storage = tex;
}

static int node_shader_gpu_tex_wave(GPUMaterial *mat,
                                    bNode *node,
                                    bNodeExecData *UNUSED(execdata),
                                    GPUNodeStack *in,
                                    GPUNodeStack *out)
{
  node_shader_gpu_default_tex_coord(mat, node, &in[0].link);
  node_shader_gpu_tex_mapping(mat, node, in, out);

  NodeTexWave *tex = (NodeTexWave *)node->storage;
  float wave_type = tex->wave_type;
  float bands_direction = tex->bands_direction;
  float rings_direction = tex->rings_direction;
  float wave_profile = tex->wave_profile;

  return GPU_stack_link(mat,
                        node,
                        "node_tex_wave",
                        in,
                        out,
                        GPU_constant(&wave_type),
                        GPU_constant(&bands_direction),
                        GPU_constant(&rings_direction),
                        GPU_constant(&wave_profile));
}

/* node type definition */
void register_node_type_sh_tex_wave(void)
{
  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_TEX_WAVE, "Wave Texture", NODE_CLASS_TEXTURE, 0);
  node_type_socket_templates(&ntype, sh_node_tex_wave_in, sh_node_tex_wave_out);
  node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
  node_type_init(&ntype, node_shader_init_tex_wave);
  node_type_storage(&ntype, "NodeTexWave", node_free_standard_storage, node_copy_standard_storage);
  node_type_gpu(&ntype, node_shader_gpu_tex_wave);

  nodeRegisterType(&ntype);
}
