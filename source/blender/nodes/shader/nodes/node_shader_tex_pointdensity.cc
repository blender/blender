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
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 */

#include "node_shader_util.hh"

#include "RE_texture.h"

namespace blender::nodes::node_shader_tex_pointdensity_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>(N_("Vector")).hide_value();
  b.add_output<decl::Color>(N_("Color"));
  b.add_output<decl::Float>(N_("Density"));
}

static void node_shader_init_tex_pointdensity(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeShaderTexPointDensity *point_density = MEM_cnew<NodeShaderTexPointDensity>("new pd node");
  point_density->resolution = 100;
  point_density->radius = 0.3f;
  point_density->space = SHD_POINTDENSITY_SPACE_OBJECT;
  point_density->color_source = SHD_POINTDENSITY_COLOR_PARTAGE;
  node->storage = point_density;
}

static void node_shader_free_tex_pointdensity(bNode *node)
{
  NodeShaderTexPointDensity *point_density = (NodeShaderTexPointDensity *)node->storage;
  PointDensity *pd = &point_density->pd;
  RE_point_density_free(pd);
  BKE_texture_pointdensity_free_data(pd);
  memset(pd, 0, sizeof(*pd));
  MEM_freeN(point_density);
}

static void node_shader_copy_tex_pointdensity(bNodeTree *UNUSED(dest_ntree),
                                              bNode *dest_node,
                                              const bNode *src_node)
{
  dest_node->storage = MEM_dupallocN(src_node->storage);
  NodeShaderTexPointDensity *point_density = (NodeShaderTexPointDensity *)dest_node->storage;
  PointDensity *pd = &point_density->pd;
  memset(pd, 0, sizeof(*pd));
}

}  // namespace blender::nodes::node_shader_tex_pointdensity_cc

/* node type definition */
void register_node_type_sh_tex_pointdensity()
{
  namespace file_ns = blender::nodes::node_shader_tex_pointdensity_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_TEX_POINTDENSITY, "Point Density", NODE_CLASS_TEXTURE);
  ntype.declare = file_ns::node_declare;
  node_type_init(&ntype, file_ns::node_shader_init_tex_pointdensity);
  node_type_storage(&ntype,
                    "NodeShaderTexPointDensity",
                    file_ns::node_shader_free_tex_pointdensity,
                    file_ns::node_shader_copy_tex_pointdensity);

  nodeRegisterType(&ntype);
}
