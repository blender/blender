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

/* **************** MUSGRAVE ******************** */

static bNodeSocketTemplate sh_node_tex_musgrave_in[] = {
    {SOCK_VECTOR, N_("Vector"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
    {SOCK_FLOAT, N_("W"), 0.0f, 0.0f, 0.0f, 0.0f, -1000.0f, 1000.0f},
    {SOCK_FLOAT, N_("Scale"), 5.0f, 0.0f, 0.0f, 0.0f, -1000.0f, 1000.0f},
    {SOCK_FLOAT, N_("Detail"), 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 16.0f},
    {SOCK_FLOAT, N_("Dimension"), 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f},
    {SOCK_FLOAT, N_("Lacunarity"), 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f},
    {SOCK_FLOAT, N_("Offset"), 0.0f, 0.0f, 0.0f, 0.0f, -1000.0f, 1000.0f},
    {SOCK_FLOAT, N_("Gain"), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f},
    {-1, ""},
};

static bNodeSocketTemplate sh_node_tex_musgrave_out[] = {
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

static void node_shader_init_tex_musgrave(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeTexMusgrave *tex = MEM_callocN(sizeof(NodeTexMusgrave), "NodeTexMusgrave");
  BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
  BKE_texture_colormapping_default(&tex->base.color_mapping);
  tex->musgrave_type = SHD_MUSGRAVE_FBM;
  tex->dimensions = 3;

  node->storage = tex;
}

static int node_shader_gpu_tex_musgrave(GPUMaterial *mat,
                                        bNode *node,
                                        bNodeExecData *UNUSED(execdata),
                                        GPUNodeStack *in,
                                        GPUNodeStack *out)
{
  node_shader_gpu_default_tex_coord(mat, node, &in[0].link);
  node_shader_gpu_tex_mapping(mat, node, in, out);

  NodeTexMusgrave *tex = (NodeTexMusgrave *)node->storage;
  int dimensions = tex->dimensions;
  int type = tex->musgrave_type;

  static const char *names[][5] = {
      [SHD_MUSGRAVE_MULTIFRACTAL] =
          {
              "",
              "node_tex_musgrave_multi_fractal_1d",
              "node_tex_musgrave_multi_fractal_2d",
              "node_tex_musgrave_multi_fractal_3d",
              "node_tex_musgrave_multi_fractal_4d",
          },
      [SHD_MUSGRAVE_FBM] =
          {
              "",
              "node_tex_musgrave_fBm_1d",
              "node_tex_musgrave_fBm_2d",
              "node_tex_musgrave_fBm_3d",
              "node_tex_musgrave_fBm_4d",
          },
      [SHD_MUSGRAVE_HYBRID_MULTIFRACTAL] =
          {
              "",
              "node_tex_musgrave_hybrid_multi_fractal_1d",
              "node_tex_musgrave_hybrid_multi_fractal_2d",
              "node_tex_musgrave_hybrid_multi_fractal_3d",
              "node_tex_musgrave_hybrid_multi_fractal_4d",
          },
      [SHD_MUSGRAVE_RIDGED_MULTIFRACTAL] =
          {
              "",
              "node_tex_musgrave_ridged_multi_fractal_1d",
              "node_tex_musgrave_ridged_multi_fractal_2d",
              "node_tex_musgrave_ridged_multi_fractal_3d",
              "node_tex_musgrave_ridged_multi_fractal_4d",
          },
      [SHD_MUSGRAVE_HETERO_TERRAIN] =
          {
              "",
              "node_tex_musgrave_hetero_terrain_1d",
              "node_tex_musgrave_hetero_terrain_2d",
              "node_tex_musgrave_hetero_terrain_3d",
              "node_tex_musgrave_hetero_terrain_4d",
          },
  };

  BLI_assert(type >= 0 && type < 5);
  BLI_assert(dimensions > 0 && dimensions < 5);

  return GPU_stack_link(mat, node, names[type][dimensions], in, out);
}

static void node_shader_update_tex_musgrave(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeTexMusgrave *tex = (NodeTexMusgrave *)node->storage;

  bNodeSocket *inVectorSock = nodeFindSocket(node, SOCK_IN, "Vector");
  bNodeSocket *inWSock = nodeFindSocket(node, SOCK_IN, "W");
  bNodeSocket *inOffsetSock = nodeFindSocket(node, SOCK_IN, "Offset");
  bNodeSocket *inGainSock = nodeFindSocket(node, SOCK_IN, "Gain");

  nodeSetSocketAvailability(inVectorSock, tex->dimensions != 1);
  nodeSetSocketAvailability(inWSock, tex->dimensions == 1 || tex->dimensions == 4);
  nodeSetSocketAvailability(inOffsetSock,
                            tex->musgrave_type != SHD_MUSGRAVE_MULTIFRACTAL &&
                                tex->musgrave_type != SHD_MUSGRAVE_FBM);
  nodeSetSocketAvailability(inGainSock,
                            tex->musgrave_type == SHD_MUSGRAVE_HYBRID_MULTIFRACTAL ||
                                tex->musgrave_type == SHD_MUSGRAVE_RIDGED_MULTIFRACTAL);
}

void register_node_type_sh_tex_musgrave(void)
{
  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_TEX_MUSGRAVE, "Musgrave Texture", NODE_CLASS_TEXTURE, 0);
  node_type_socket_templates(&ntype, sh_node_tex_musgrave_in, sh_node_tex_musgrave_out);
  node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
  node_type_init(&ntype, node_shader_init_tex_musgrave);
  node_type_storage(
      &ntype, "NodeTexMusgrave", node_free_standard_storage, node_copy_standard_storage);
  node_type_gpu(&ntype, node_shader_gpu_tex_musgrave);
  node_type_update(&ntype, node_shader_update_tex_musgrave);

  nodeRegisterType(&ntype);
}
