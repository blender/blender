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

static void fn_node_replace_string_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::String>(N_("String"));
  b.add_input<decl::String>(N_("Find")).description(N_("The string to find in the input string"));
  b.add_input<decl::String>(N_("Replace"))
      .description(N_("The string to replace each match with"));
  b.add_output<decl::String>(N_("String"));
};

static std::string replace_all(std::string str, const std::string &from, const std::string &to)
{
  if (from.length() <= 0) {
    return str;
  }
  const size_t step = to.length() > 0 ? to.length() : 1;

  size_t offset = 0;
  while ((offset = str.find(from, offset)) != std::string::npos) {
    str.replace(offset, from.length(), to);
    offset += step;
  }
  return str;
}

static void fn_node_replace_string_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static fn::CustomMF_SI_SI_SI_SO<std::string, std::string, std::string, std::string> substring_fn{
      "Replace", [](const std::string &str, const std::string &find, const std::string &replace) {
        return replace_all(str, find, replace);
      }};
  builder.set_matching_fn(&substring_fn);
}

}  // namespace blender::nodes

void register_node_type_fn_replace_string()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_REPLACE_STRING, "Replace String", NODE_CLASS_CONVERTER, 0);
  ntype.declare = blender::nodes::fn_node_replace_string_declare;
  ntype.build_multi_function = blender::nodes::fn_node_replace_string_build_multi_function;
  nodeRegisterType(&ntype);
}
