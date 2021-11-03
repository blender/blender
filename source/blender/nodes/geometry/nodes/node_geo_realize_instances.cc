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

#include "node_geometry_util.hh"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes {

static void geo_node_realize_instances_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_output<decl::Geometry>(N_("Geometry"));
}

static void geo_node_realize_instances_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  geometry_set = bke::geometry_set_realize_instances(geometry_set);
  params.set_output("Geometry", std::move(geometry_set));
}

}  // namespace blender::nodes

void register_node_type_geo_realize_instances()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_REALIZE_INSTANCES, "Realize Instances", NODE_CLASS_GEOMETRY, 0);
  ntype.declare = blender::nodes::geo_node_realize_instances_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_realize_instances_exec;
  nodeRegisterType(&ntype);
}
