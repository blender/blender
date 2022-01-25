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

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "RNA_enum_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "NOD_socket_search_link.hh"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_boolean_math_cc {

static void fn_node_boolean_math_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Bool>(N_("Boolean"), "Boolean");
  b.add_input<decl::Bool>(N_("Boolean"), "Boolean_001");
  b.add_output<decl::Bool>(N_("Boolean"));
}

static void fn_node_boolean_math_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "operation", 0, "", ICON_NONE);
}

static void node_boolean_math_update(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *sockB = (bNodeSocket *)BLI_findlink(&node->inputs, 1);

  nodeSetSocketAvailability(ntree, sockB, !ELEM(node->custom1, NODE_BOOLEAN_MATH_NOT));
}

static void node_boolean_math_label(const bNodeTree *UNUSED(ntree),
                                    const bNode *node,
                                    char *label,
                                    int maxlen)
{
  const char *name;
  bool enum_label = RNA_enum_name(rna_enum_node_boolean_math_items, node->custom1, &name);
  if (!enum_label) {
    name = "Unknown";
  }
  BLI_strncpy(label, IFACE_(name), maxlen);
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  if (!params.node_tree().typeinfo->validate_link(
          static_cast<eNodeSocketDatatype>(params.other_socket().type), SOCK_BOOLEAN)) {
    return;
  }

  for (const EnumPropertyItem *item = rna_enum_node_boolean_math_items;
       item->identifier != nullptr;
       item++) {
    if (item->name != nullptr && item->identifier[0] != '\0') {
      NodeBooleanMathOperation operation = static_cast<NodeBooleanMathOperation>(item->value);
      params.add_item(IFACE_(item->name), [operation](LinkSearchOpParams &params) {
        bNode &node = params.add_node("FunctionNodeBooleanMath");
        node.custom1 = operation;
        params.update_and_connect_available_socket(node, "Boolean");
      });
    }
  }
}

static const fn::MultiFunction *get_multi_function(bNode &bnode)
{
  static fn::CustomMF_SI_SI_SO<bool, bool, bool> and_fn{"And",
                                                        [](bool a, bool b) { return a && b; }};
  static fn::CustomMF_SI_SI_SO<bool, bool, bool> or_fn{"Or",
                                                       [](bool a, bool b) { return a || b; }};
  static fn::CustomMF_SI_SO<bool, bool> not_fn{"Not", [](bool a) { return !a; }};
  static fn::CustomMF_SI_SI_SO<bool, bool, bool> nand_fn{"Not And",
                                                         [](bool a, bool b) { return !(a && b); }};
  static fn::CustomMF_SI_SI_SO<bool, bool, bool> nor_fn{"Nor",
                                                        [](bool a, bool b) { return !(a || b); }};
  static fn::CustomMF_SI_SI_SO<bool, bool, bool> xnor_fn{"Equal",
                                                         [](bool a, bool b) { return a == b; }};
  static fn::CustomMF_SI_SI_SO<bool, bool, bool> xor_fn{"Not Equal",
                                                        [](bool a, bool b) { return a != b; }};
  static fn::CustomMF_SI_SI_SO<bool, bool, bool> imply_fn{"Imply",
                                                          [](bool a, bool b) { return !a || b; }};
  static fn::CustomMF_SI_SI_SO<bool, bool, bool> nimply_fn{"Subtract",
                                                           [](bool a, bool b) { return a && !b; }};

  switch (bnode.custom1) {
    case NODE_BOOLEAN_MATH_AND:
      return &and_fn;
    case NODE_BOOLEAN_MATH_OR:
      return &or_fn;
    case NODE_BOOLEAN_MATH_NOT:
      return &not_fn;
    case NODE_BOOLEAN_MATH_NAND:
      return &nand_fn;
    case NODE_BOOLEAN_MATH_NOR:
      return &nor_fn;
    case NODE_BOOLEAN_MATH_XNOR:
      return &xnor_fn;
    case NODE_BOOLEAN_MATH_XOR:
      return &xor_fn;
    case NODE_BOOLEAN_MATH_IMPLY:
      return &imply_fn;
    case NODE_BOOLEAN_MATH_NIMPLY:
      return &nimply_fn;
  }

  BLI_assert_unreachable();
  return nullptr;
}

static void fn_node_boolean_math_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const fn::MultiFunction *fn = get_multi_function(builder.node());
  builder.set_matching_fn(fn);
}

}  // namespace blender::nodes::node_fn_boolean_math_cc

void register_node_type_fn_boolean_math()
{
  namespace file_ns = blender::nodes::node_fn_boolean_math_cc;

  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_BOOLEAN_MATH, "Boolean Math", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::fn_node_boolean_math_declare;
  ntype.labelfunc = file_ns::node_boolean_math_label;
  node_type_update(&ntype, file_ns::node_boolean_math_update);
  ntype.build_multi_function = file_ns::fn_node_boolean_math_build_multi_function;
  ntype.draw_buttons = file_ns::fn_node_boolean_math_layout;
  ntype.gather_link_search_ops = file_ns::node_gather_link_searches;
  nodeRegisterType(&ntype);
}
