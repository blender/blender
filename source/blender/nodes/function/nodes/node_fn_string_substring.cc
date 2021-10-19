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

#include "BLI_string_utf8.h"

#include "node_function_util.hh"

namespace blender::nodes {

static void fn_node_string_substring_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::String>("String");
  b.add_input<decl::Int>("Position");
  b.add_input<decl::Int>("Length").min(0);
  b.add_output<decl::String>("String");
};

}  // namespace blender::nodes

static void fn_node_string_substring_build_multi_function(
    blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static blender::fn::CustomMF_SI_SI_SI_SO<std::string, int, int, std::string> substring_fn{
      "Substring", [](const std::string &str, int a, int b) {
        const int len = BLI_strlen_utf8(str.c_str());
        const int start = BLI_str_utf8_offset_from_index(str.c_str(), std::clamp(a, 0, len));
        const int end = BLI_str_utf8_offset_from_index(str.c_str(), std::clamp(a + b, 0, len));
        return str.substr(start, std::max<int>(end - start, 0));
      }};
  builder.set_matching_fn(&substring_fn);
}

void register_node_type_fn_string_substring()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_STRING_SUBSTRING, "String Substring", NODE_CLASS_CONVERTER, 0);
  ntype.declare = blender::nodes::fn_node_string_substring_declare;
  ntype.build_multi_function = fn_node_string_substring_build_multi_function;
  nodeRegisterType(&ntype);
}
