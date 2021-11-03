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

static void geo_node_string_join_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::String>(N_("Delimiter"));
  b.add_input<decl::String>(N_("Strings")).multi_input().hide_value();
  b.add_output<decl::String>(N_("String"));
};

static void geo_node_string_join_exec(GeoNodeExecParams params)
{
  Vector<std::string> strings = params.extract_multi_input<std::string>("Strings");
  const std::string delim = params.extract_input<std::string>("Delimiter");

  std::string output;
  for (const int i : strings.index_range()) {
    output += strings[i];
    if (i < (strings.size() - 1)) {
      output += delim;
    }
  }
  params.set_output("String", std::move(output));
}

}  // namespace blender::nodes

void register_node_type_geo_string_join()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_STRING_JOIN, "Join Strings", NODE_CLASS_CONVERTER, 0);
  ntype.geometry_node_execute = blender::nodes::geo_node_string_join_exec;
  ntype.declare = blender::nodes::geo_node_string_join_declare;
  nodeRegisterType(&ntype);
}
