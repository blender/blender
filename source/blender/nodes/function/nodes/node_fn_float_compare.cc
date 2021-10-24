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

#include <cmath>

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "RNA_enum_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_function_util.hh"

namespace blender::nodes {

static void fn_node_float_compare_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Float>("A").min(-10000.0f).max(10000.0f);
  b.add_input<decl::Float>("B").min(-10000.0f).max(10000.0f);
  b.add_input<decl::Float>("Epsilon").default_value(0.001f).min(-10000.0f).max(10000.0f);
  b.add_output<decl::Bool>("Result");
};

static void geo_node_float_compare_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "operation", 0, "", ICON_NONE);
}

static void node_float_compare_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  bNodeSocket *sockEpsilon = (bNodeSocket *)BLI_findlink(&node->inputs, 2);

  nodeSetSocketAvailability(
      sockEpsilon, ELEM(node->custom1, NODE_FLOAT_COMPARE_EQUAL, NODE_FLOAT_COMPARE_NOT_EQUAL));
}

static void node_float_compare_label(bNodeTree *UNUSED(ntree),
                                     bNode *node,
                                     char *label,
                                     int maxlen)
{
  const char *name;
  bool enum_label = RNA_enum_name(rna_enum_node_float_compare_items, node->custom1, &name);
  if (!enum_label) {
    name = "Unknown";
  }
  BLI_strncpy(label, IFACE_(name), maxlen);
}

static const fn::MultiFunction *get_multi_function(bNode &node)
{
  static fn::CustomMF_SI_SI_SO<float, float, bool> less_than_fn{
      "Less Than", [](float a, float b) { return a < b; }};
  static fn::CustomMF_SI_SI_SO<float, float, bool> less_equal_fn{
      "Less Equal", [](float a, float b) { return a <= b; }};
  static fn::CustomMF_SI_SI_SO<float, float, bool> greater_than_fn{
      "Greater Than", [](float a, float b) { return a > b; }};
  static fn::CustomMF_SI_SI_SO<float, float, bool> greater_equal_fn{
      "Greater Equal", [](float a, float b) { return a >= b; }};
  static fn::CustomMF_SI_SI_SI_SO<float, float, float, bool> equal_fn{
      "Equal", [](float a, float b, float epsilon) { return std::abs(a - b) <= epsilon; }};
  static fn::CustomMF_SI_SI_SI_SO<float, float, float, bool> not_equal_fn{
      "Not Equal", [](float a, float b, float epsilon) { return std::abs(a - b) > epsilon; }};

  switch (node.custom1) {
    case NODE_FLOAT_COMPARE_LESS_THAN:
      return &less_than_fn;
    case NODE_FLOAT_COMPARE_LESS_EQUAL:
      return &less_equal_fn;
    case NODE_FLOAT_COMPARE_GREATER_THAN:
      return &greater_than_fn;
    case NODE_FLOAT_COMPARE_GREATER_EQUAL:
      return &greater_equal_fn;
    case NODE_FLOAT_COMPARE_EQUAL:
      return &equal_fn;
    case NODE_FLOAT_COMPARE_NOT_EQUAL:
      return &not_equal_fn;
  }

  BLI_assert_unreachable();
  return nullptr;
}

static void fn_node_float_compare_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const fn::MultiFunction *fn = get_multi_function(builder.node());
  builder.set_matching_fn(fn);
}

}  // namespace blender::nodes

void register_node_type_fn_float_compare()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_COMPARE_FLOATS, "Compare Floats", NODE_CLASS_CONVERTER, 0);
  ntype.declare = blender::nodes::fn_node_float_compare_declare;
  node_type_label(&ntype, blender::nodes::node_float_compare_label);
  node_type_update(&ntype, blender::nodes::node_float_compare_update);
  ntype.build_multi_function = blender::nodes::fn_node_float_compare_build_multi_function;
  ntype.draw_buttons = blender::nodes::geo_node_float_compare_layout;
  nodeRegisterType(&ntype);
}
