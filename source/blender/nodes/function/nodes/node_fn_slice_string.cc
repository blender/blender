/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string_utf8.h"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_slice_string_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::String>("String").optional_label();
  b.add_output<decl::String>("String").align_with_previous();
  b.add_input<decl::Int>("Position");
  b.add_input<decl::Int>("Length").min(0).default_value(10);
}

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static auto slice_fn = mf::build::SI3_SO<std::string, int, int, std::string>(
      "Slice", [](const std::string &str, int a, int b) {
        const int start = BLI_str_utf8_offset_from_index(str.c_str(), str.size(), std::max(0, a));
        const int end = BLI_str_utf8_offset_from_index(
            str.c_str(), str.size(), std::max(0, a + b));
        return str.substr(start, std::max<int>(end - start, 0));
      });
  builder.set_matching_fn(&slice_fn);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  fn_node_type_base(&ntype, "FunctionNodeSliceString", FN_NODE_SLICE_STRING);
  ntype.ui_name = "Slice String";
  ntype.ui_description = "Extract a string segment from a larger string";
  ntype.enum_name_legacy = "SLICE_STRING";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.build_multi_function = node_build_multi_function;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_slice_string_cc
