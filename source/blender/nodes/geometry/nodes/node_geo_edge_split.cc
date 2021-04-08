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
 */

#include "DNA_modifier_types.h"

#include "node_geometry_util.hh"

extern "C" {
Mesh *doEdgeSplit(const Mesh *mesh, EdgeSplitModifierData *emd);
}

static bNodeSocketTemplate geo_node_edge_split_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_BOOLEAN, N_("Edge Angle"), true},
    {SOCK_FLOAT,
     N_("Angle"),
     DEG2RADF(30.0f),
     0.0f,
     0.0f,
     0.0f,
     0.0f,
     DEG2RADF(180.0f),
     PROP_ANGLE},
    {SOCK_BOOLEAN, N_("Sharp Edges")},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_edge_split_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

namespace blender::nodes {
static void geo_node_edge_split_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  geometry_set = geometry_set_realize_instances(geometry_set);

  if (!geometry_set.has_mesh()) {
    params.set_output("Geometry", std::move(geometry_set));
    return;
  }

  const bool use_sharp_flag = params.extract_input<bool>("Sharp Edges");
  const bool use_edge_angle = params.extract_input<bool>("Edge Angle");

  if (!use_edge_angle && !use_sharp_flag) {
    params.set_output("Geometry", std::move(geometry_set));
    return;
  }

  const float split_angle = params.extract_input<float>("Angle");
  const Mesh *mesh_in = geometry_set.get_mesh_for_read();

  /* Use modifier struct to pass arguments to the modifier code. */
  EdgeSplitModifierData emd;
  memset(&emd, 0, sizeof(EdgeSplitModifierData));
  emd.split_angle = split_angle;
  if (use_edge_angle) {
    emd.flags = MOD_EDGESPLIT_FROMANGLE;
  }
  if (use_sharp_flag) {
    emd.flags |= MOD_EDGESPLIT_FROMFLAG;
  }

  Mesh *mesh_out = doEdgeSplit(mesh_in, &emd);
  geometry_set.replace_mesh(mesh_out);

  params.set_output("Geometry", std::move(geometry_set));
}
}  // namespace blender::nodes

void register_node_type_geo_edge_split()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_EDGE_SPLIT, "Edge Split", NODE_CLASS_GEOMETRY, 0);
  node_type_socket_templates(&ntype, geo_node_edge_split_in, geo_node_edge_split_out);
  ntype.geometry_node_execute = blender::nodes::geo_node_edge_split_exec;
  nodeRegisterType(&ntype);
}
