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

#include "node_function_util.hh"
#include <iomanip>

namespace blender::nodes {

static void fn_node_value_to_string_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("Value"));
  b.add_input<decl::Int>(N_("Decimals")).min(0);
  b.add_output<decl::String>(N_("String"));
};

static void fn_node_value_to_string_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static fn::CustomMF_SI_SI_SO<float, int, std::string> to_str_fn{
      "Value To String", [](float a, int b) {
        std::stringstream stream;
        stream << std::fixed << std::setprecision(std::max(0, b)) << a;
        return stream.str();
      }};
  builder.set_matching_fn(&to_str_fn);
}

}  // namespace blender::nodes

void register_node_type_fn_value_to_string()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_VALUE_TO_STRING, "Value to String", NODE_CLASS_CONVERTER, 0);
  ntype.declare = blender::nodes::fn_node_value_to_string_declare;
  ntype.build_multi_function = blender::nodes::fn_node_value_to_string_build_multi_function;
  nodeRegisterType(&ntype);
}
