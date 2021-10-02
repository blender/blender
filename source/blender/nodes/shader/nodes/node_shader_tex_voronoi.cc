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

namespace blender::nodes {

static void sh_node_tex_voronoi_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>("Vector").hide_value();
  b.add_input<decl::Float>("W").min(-1000.0f).max(1000.0f);
  b.add_input<decl::Float>("Scale").min(-1000.0f).max(1000.0f).default_value(5.0f);
  b.add_input<decl::Float>("Smoothness")
      .min(0.0f)
      .max(1.0f)
      .default_value(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>("Exponent").min(0.0f).max(32.0f).default_value(0.5f);
  b.add_input<decl::Float>("Randomness")
      .min(0.0f)
      .max(1.0f)
      .default_value(1.0f)
      .subtype(PROP_FACTOR);
  b.add_output<decl::Float>("Distance").no_muted_links();
  b.add_output<decl::Color>("Color").no_muted_links();
  b.add_output<decl::Vector>("Position").no_muted_links();
  b.add_output<decl::Float>("W").no_muted_links();
  b.add_output<decl::Float>("Radius").no_muted_links();
};

}  // namespace blender::nodes

static void node_shader_init_tex_voronoi(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeTexVoronoi *tex = (NodeTexVoronoi *)MEM_callocN(sizeof(NodeTexVoronoi), "NodeTexVoronoi");
  BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
  BKE_texture_colormapping_default(&tex->base.color_mapping);
  tex->dimensions = 3;
  tex->distance = SHD_VORONOI_EUCLIDEAN;
  tex->feature = SHD_VORONOI_F1;

  node->storage = tex;
}

static const char *gpu_shader_get_name(const int feature, const int dimensions)
{
  BLI_assert(feature >= 0 && feature < 5);
  BLI_assert(dimensions > 0 && dimensions < 5);

  switch (feature) {
    case SHD_VORONOI_F1:
      return std::array{
          "node_tex_voronoi_f1_1d",
          "node_tex_voronoi_f1_2d",
          "node_tex_voronoi_f1_3d",
          "node_tex_voronoi_f1_4d",
      }[dimensions - 1];
    case SHD_VORONOI_F2:
      return std::array{
          "node_tex_voronoi_f2_1d",
          "node_tex_voronoi_f2_2d",
          "node_tex_voronoi_f2_3d",
          "node_tex_voronoi_f2_4d",
      }[dimensions - 1];
    case SHD_VORONOI_SMOOTH_F1:
      return std::array{
          "node_tex_voronoi_smooth_f1_1d",
          "node_tex_voronoi_smooth_f1_2d",
          "node_tex_voronoi_smooth_f1_3d",
          "node_tex_voronoi_smooth_f1_4d",
      }[dimensions - 1];
    case SHD_VORONOI_DISTANCE_TO_EDGE:
      return std::array{
          "node_tex_voronoi_distance_to_edge_1d",
          "node_tex_voronoi_distance_to_edge_2d",
          "node_tex_voronoi_distance_to_edge_3d",
          "node_tex_voronoi_distance_to_edge_4d",
      }[dimensions - 1];
    case SHD_VORONOI_N_SPHERE_RADIUS:
      return std::array{
          "node_tex_voronoi_n_sphere_radius_1d",
          "node_tex_voronoi_n_sphere_radius_2d",
          "node_tex_voronoi_n_sphere_radius_3d",
          "node_tex_voronoi_n_sphere_radius_4d",
      }[dimensions - 1];
  }
  return nullptr;
}

static int node_shader_gpu_tex_voronoi(GPUMaterial *mat,
                                       bNode *node,
                                       bNodeExecData *UNUSED(execdata),
                                       GPUNodeStack *in,
                                       GPUNodeStack *out)
{
  node_shader_gpu_default_tex_coord(mat, node, &in[0].link);
  node_shader_gpu_tex_mapping(mat, node, in, out);

  NodeTexVoronoi *tex = (NodeTexVoronoi *)node->storage;
  float metric = tex->distance;

  const char *name = gpu_shader_get_name(tex->feature, tex->dimensions);

  return GPU_stack_link(mat, node, name, in, out, GPU_constant(&metric));
}

static void node_shader_update_tex_voronoi(bNodeTree *UNUSED(ntree), bNode *node)
{
  bNodeSocket *inVectorSock = nodeFindSocket(node, SOCK_IN, "Vector");
  bNodeSocket *inWSock = nodeFindSocket(node, SOCK_IN, "W");
  bNodeSocket *inSmoothnessSock = nodeFindSocket(node, SOCK_IN, "Smoothness");
  bNodeSocket *inExponentSock = nodeFindSocket(node, SOCK_IN, "Exponent");

  bNodeSocket *outDistanceSock = nodeFindSocket(node, SOCK_OUT, "Distance");
  bNodeSocket *outColorSock = nodeFindSocket(node, SOCK_OUT, "Color");
  bNodeSocket *outPositionSock = nodeFindSocket(node, SOCK_OUT, "Position");
  bNodeSocket *outWSock = nodeFindSocket(node, SOCK_OUT, "W");
  bNodeSocket *outRadiusSock = nodeFindSocket(node, SOCK_OUT, "Radius");

  NodeTexVoronoi *tex = (NodeTexVoronoi *)node->storage;

  nodeSetSocketAvailability(inWSock, tex->dimensions == 1 || tex->dimensions == 4);
  nodeSetSocketAvailability(inVectorSock, tex->dimensions != 1);
  nodeSetSocketAvailability(
      inExponentSock,
      tex->distance == SHD_VORONOI_MINKOWSKI && tex->dimensions != 1 &&
          !ELEM(tex->feature, SHD_VORONOI_DISTANCE_TO_EDGE, SHD_VORONOI_N_SPHERE_RADIUS));
  nodeSetSocketAvailability(inSmoothnessSock, tex->feature == SHD_VORONOI_SMOOTH_F1);
  nodeSetSocketAvailability(outDistanceSock, tex->feature != SHD_VORONOI_N_SPHERE_RADIUS);
  nodeSetSocketAvailability(outColorSock,
                            tex->feature != SHD_VORONOI_DISTANCE_TO_EDGE &&
                                tex->feature != SHD_VORONOI_N_SPHERE_RADIUS);
  nodeSetSocketAvailability(outPositionSock,
                            tex->feature != SHD_VORONOI_DISTANCE_TO_EDGE &&
                                tex->feature != SHD_VORONOI_N_SPHERE_RADIUS &&
                                tex->dimensions != 1);
  nodeSetSocketAvailability(outWSock,
                            tex->feature != SHD_VORONOI_DISTANCE_TO_EDGE &&
                                tex->feature != SHD_VORONOI_N_SPHERE_RADIUS &&
                                (tex->dimensions == 1 || tex->dimensions == 4));
  nodeSetSocketAvailability(outRadiusSock, tex->feature == SHD_VORONOI_N_SPHERE_RADIUS);
}

void register_node_type_sh_tex_voronoi(void)
{
  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_TEX_VORONOI, "Voronoi Texture", NODE_CLASS_TEXTURE, 0);
  ntype.declare = blender::nodes::sh_node_tex_voronoi_declare;
  node_type_init(&ntype, node_shader_init_tex_voronoi);
  node_type_storage(
      &ntype, "NodeTexVoronoi", node_free_standard_storage, node_copy_standard_storage);
  node_type_gpu(&ntype, node_shader_gpu_tex_voronoi);
  node_type_update(&ntype, node_shader_update_tex_voronoi);

  nodeRegisterType(&ntype);
}
