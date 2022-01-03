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

namespace blender::nodes::node_fn_slice_string_cc {

static void fn_node_slice_string_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::String>(N_("String"));
  b.add_input<decl::Int>(N_("Position"));
  b.add_input<decl::Int>(N_("Length")).min(0).default_value(10);
  b.add_output<decl::String>(N_("String"));
};

static void fn_node_slice_string_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static blender::fn::CustomMF_SI_SI_SI_SO<std::string, int, int, std::string> slice_fn{
      "Slice", [](const std::string &str, int a, int b) {
        const int len = BLI_strlen_utf8(str.c_str());
        const int start = BLI_str_utf8_offset_from_index(str.c_str(), std::clamp(a, 0, len));
        const int end = BLI_str_utf8_offset_from_index(str.c_str(), std::clamp(a + b, 0, len));
        return str.substr(start, std::max<int>(end - start, 0));
      }};
  builder.set_matching_fn(&slice_fn);
}

}  // namespace blender::nodes::node_fn_slice_string_cc

void register_node_type_fn_slice_string()
{
  namespace file_ns = blender::nodes::node_fn_slice_string_cc;

  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_SLICE_STRING, "Slice String", NODE_CLASS_CONVERTER, 0);
  ntype.declare = file_ns::fn_node_slice_string_declare;
  ntype.build_multi_function = file_ns::fn_node_slice_string_build_multi_function;
  nodeRegisterType(&ntype);
}
