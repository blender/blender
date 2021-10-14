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

namespace blender::nodes {

static void geo_node_input_spline_resolution_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Int>("Resolution").field_source();
}

static void geo_node_input_spline_resolution_exec(GeoNodeExecParams params)
{
  Field<int> resolution_field = AttributeFieldInput::Create<int>("resolution");
  params.set_output("Resolution", std::move(resolution_field));
}

}  // namespace blender::nodes

void register_node_type_geo_input_spline_resolution()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_INPUT_SPLINE_RESOLUTION, "Spline Resolution", NODE_CLASS_INPUT, 0);
  ntype.geometry_node_execute = blender::nodes::geo_node_input_spline_resolution_exec;
  ntype.declare = blender::nodes::geo_node_input_spline_resolution_declare;
  nodeRegisterType(&ntype);
}
