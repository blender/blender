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

#include "BLI_string.h"

#include "RNA_enum_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_function_util.hh"

static bNodeSocketTemplate fn_node_float_to_int_in[] = {
    {SOCK_FLOAT, N_("Float"), 0.0, 0.0, 0.0, 0.0, -FLT_MAX, FLT_MAX},
    {-1, ""},
};

static bNodeSocketTemplate fn_node_float_to_int_out[] = {
    {SOCK_INT, N_("Integer")},
    {-1, ""},
};

static void fn_node_float_to_int_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "rounding_mode", 0, "", ICON_NONE);
}

static void node_float_to_int_label(bNodeTree *UNUSED(ntree), bNode *node, char *label, int maxlen)
{
  const char *name;
  bool enum_label = RNA_enum_name(rna_enum_node_float_to_int_items, node->custom1, &name);
  if (!enum_label) {
    name = "Unknown";
  }
  BLI_strncpy(label, IFACE_(name), maxlen);
}

static const blender::fn::MultiFunction &get_multi_function(bNode &bnode)
{
  static blender::fn::CustomMF_SI_SO<float, int> round_fn{"Round",
                                                          [](float a) { return (int)round(a); }};
  static blender::fn::CustomMF_SI_SO<float, int> floor_fn{"Floor",
                                                          [](float a) { return (int)floor(a); }};
  static blender::fn::CustomMF_SI_SO<float, int> ceil_fn{"Ceiling",
                                                         [](float a) { return (int)ceil(a); }};
  static blender::fn::CustomMF_SI_SO<float, int> trunc_fn{"Truncate",
                                                          [](float a) { return (int)trunc(a); }};

  switch (static_cast<FloatToIntRoundingMode>(bnode.custom1)) {
    case FN_NODE_FLOAT_TO_INT_ROUND:
      return round_fn;
    case FN_NODE_FLOAT_TO_INT_FLOOR:
      return floor_fn;
    case FN_NODE_FLOAT_TO_INT_CEIL:
      return ceil_fn;
    case FN_NODE_FLOAT_TO_INT_TRUNCATE:
      return trunc_fn;
  }

  BLI_assert_unreachable();
  return blender::fn::dummy_multi_function;
}

static void node_float_to_int_expand_in_mf_network(blender::nodes::NodeMFNetworkBuilder &builder)
{
  const blender::fn::MultiFunction &fn = get_multi_function(builder.bnode());
  builder.set_matching_fn(fn);
}

void register_node_type_fn_float_to_int()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_FLOAT_TO_INT, "Float to Integer", NODE_CLASS_CONVERTOR, 0);
  node_type_socket_templates(&ntype, fn_node_float_to_int_in, fn_node_float_to_int_out);
  node_type_label(&ntype, node_float_to_int_label);
  ntype.expand_in_mf_network = node_float_to_int_expand_in_mf_network;
  ntype.draw_buttons = fn_node_float_to_int_layout;
  nodeRegisterType(&ntype);
}
