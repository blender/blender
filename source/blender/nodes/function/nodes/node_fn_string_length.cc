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

#include <iomanip>

#include "node_function_util.hh"

namespace blender::nodes {

static void fn_node_string_length_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::String>(N_("String"));
  b.add_output<decl::Int>(N_("Length"));
};

static void fn_node_string_length_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static fn::CustomMF_SI_SO<std::string, int> str_len_fn{
      "String Length", [](const std::string &a) { return BLI_strlen_utf8(a.c_str()); }};
  builder.set_matching_fn(&str_len_fn);
}

}  // namespace blender::nodes

void register_node_type_fn_string_length()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_STRING_LENGTH, "String Length", NODE_CLASS_CONVERTER, 0);
  ntype.declare = blender::nodes::fn_node_string_length_declare;
  ntype.build_multi_function = blender::nodes::fn_node_string_length_build_multi_function;
  nodeRegisterType(&ntype);
}
