/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string_ref.hh"
#include "BLI_string_utf8.h"

#include "UI_interface.hh"

#include "RNA_enum_types.hh"

#include "node_function_util.hh"

#include "NOD_rna_define.hh"
#include "NOD_socket_search_link.hh"

namespace blender::nodes::node_fn_match_string_cc {

enum class MatchStringOperation : int8_t { StartsWith, EndsWith, Contains };

const EnumPropertyItem rna_enum_node_match_string_items[] = {
    {int(MatchStringOperation::StartsWith),
     "STARTS_WITH",
     0,
     "Starts With",
     "True when the first input starts with the second"},
    {int(MatchStringOperation::EndsWith),
     "ENDS_WITH",
     0,
     "Ends With",
     "True when the first input ends with the second"},
    {int(MatchStringOperation::Contains),
     "CONTAINS",
     0,
     "Contains",
     "True when the first input contains the second as a substring"},
    {0, nullptr, 0, nullptr, nullptr},
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::String>("String").hide_label();
  b.add_input<decl::String>("Key").hide_label().description(
      "The string to find in the input string");
  b.add_output<decl::Bool>("Result");
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "operation", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = int(MatchStringOperation::StartsWith);
}

static const mf::MultiFunction *get_multi_function(const bNode &bnode)
{
  const MatchStringOperation operation = MatchStringOperation(bnode.custom1);

  switch (operation) {
    case MatchStringOperation::StartsWith: {
      static auto fn = mf::build::SI2_SO<std::string, std::string, bool>(
          "Starts With", [](const std::string &a, const std::string &b) {
            const StringRef strref_a(a);
            const StringRef strref_b(b);
            return strref_a.startswith(strref_b);
          });
      return &fn;
    }
    case MatchStringOperation::EndsWith: {
      static auto fn = mf::build::SI2_SO<std::string, std::string, bool>(
          "Ends With", [](const std::string &a, const std::string &b) {
            const StringRef strref_a(a);
            const StringRef strref_b(b);
            return strref_a.endswith(strref_b);
          });
      return &fn;
    }
    case MatchStringOperation::Contains: {
      static auto fn = mf::build::SI2_SO<std::string, std::string, bool>(
          "Contains", [](const std::string &a, const std::string &b) {
            const StringRef strref_a(a);
            const StringRef strref_b(b);
            return strref_a.find(strref_b) != StringRef::not_found;
          });
      return &fn;
    }
  }
  BLI_assert_unreachable();
  return nullptr;
}

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const mf::MultiFunction *fn = get_multi_function(builder.node());
  builder.set_matching_fn(fn);
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  if (params.in_out() == SOCK_IN) {
    if (params.node_tree().typeinfo->validate_link(
            static_cast<eNodeSocketDatatype>(params.other_socket().type), SOCK_STRING))
    {
      for (const EnumPropertyItem *item = rna_enum_node_match_string_items;
           item->identifier != nullptr;
           item++)
      {
        if (item->name != nullptr && item->identifier[0] != '\0') {
          MatchStringOperation operation = MatchStringOperation(item->value);
          params.add_item(IFACE_(item->name), [operation](LinkSearchOpParams &params) {
            bNode &node = params.add_node("FunctionNodeMatchString");
            node.custom1 = int8_t(operation);
            params.update_and_connect_available_socket(node, "String");
          });
        }
      }
    }
  }

  else {
    params.add_item(IFACE_("Result"), [](LinkSearchOpParams &params) {
      bNode &node = params.add_node("FunctionNodeMatchString");
      params.update_and_connect_available_socket(node, "Result");
    });
  }
}

static void node_label(const bNodeTree * /*tree*/,
                       const bNode *node,
                       char *label,
                       int label_maxncpy)
{
  const char *name;
  bool enum_label = RNA_enum_name(rna_enum_node_match_string_items, node->custom1, &name);
  if (!enum_label) {
    name = "Unknown";
  }
  BLI_strncpy_utf8(label, IFACE_(name), label_maxncpy);
}

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(srna,
                    "operation",
                    "Operation",
                    "",
                    rna_enum_node_match_string_items,
                    NOD_inline_enum_accessors(custom1),
                    int(MatchStringOperation::StartsWith));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  fn_node_type_base(&ntype, "FunctionNodeMatchString");
  ntype.ui_name = "Match String";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.labelfunc = node_label;
  ntype.gather_link_search_ops = node_gather_link_searches;
  ntype.initfunc = node_init;
  ntype.draw_buttons = node_layout;
  ntype.build_multi_function = node_build_multi_function;

  blender::bke::node_register_type(ntype);
  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_match_string_cc
