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

  nodeSetSocketAvailability(
      ntree, sockB, ELEM(node->custom1, NODE_BOOLEAN_MATH_AND, NODE_BOOLEAN_MATH_OR));
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

static const fn::MultiFunction *get_multi_function(bNode &bnode)
{
  static fn::CustomMF_SI_SI_SO<bool, bool, bool> and_fn{"And",
                                                        [](bool a, bool b) { return a && b; }};
  static fn::CustomMF_SI_SI_SO<bool, bool, bool> or_fn{"Or",
                                                       [](bool a, bool b) { return a || b; }};
  static fn::CustomMF_SI_SO<bool, bool> not_fn{"Not", [](bool a) { return !a; }};

  switch (bnode.custom1) {
    case NODE_BOOLEAN_MATH_AND:
      return &and_fn;
    case NODE_BOOLEAN_MATH_OR:
      return &or_fn;
    case NODE_BOOLEAN_MATH_NOT:
      return &not_fn;
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
  nodeRegisterType(&ntype);
}
