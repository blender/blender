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

namespace blender::nodes::node_geo_input_curve_handles_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Vector>(N_("Left")).field_source();
  b.add_output<decl::Vector>(N_("Right")).field_source();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<float3> left_field = AttributeFieldInput::Create<float3>("handle_left");
  Field<float3> right_field = AttributeFieldInput::Create<float3>("handle_right");
  params.set_output("Left", std::move(left_field));
  params.set_output("Right", std::move(right_field));
}

}  // namespace blender::nodes::node_geo_input_curve_handles_cc

void register_node_type_geo_input_curve_handles()
{
  namespace file_ns = blender::nodes::node_geo_input_curve_handles_cc;

  static bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_INPUT_CURVE_HANDLES, "Curve Handle Positions", NODE_CLASS_INPUT);
  node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
