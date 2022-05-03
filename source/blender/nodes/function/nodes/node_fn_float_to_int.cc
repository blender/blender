/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <cmath>

#include "BLI_noise.hh"
#include "BLI_string.h"

#include "RNA_enum_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_float_to_int_cc {

static void fn_node_float_to_int_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Float>(N_("Float"));
  b.add_output<decl::Int>(N_("Integer"));
}

static void fn_node_float_to_int_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "rounding_mode", 0, "", ICON_NONE);
}

static void node_float_to_int_label(const bNodeTree *UNUSED(ntree),
                                    const bNode *node,
                                    char *label,
                                    int maxlen)
{
  const char *name;
  bool enum_label = RNA_enum_name(rna_enum_node_float_to_int_items, node->custom1, &name);
  if (!enum_label) {
    name = "Unknown";
  }
  BLI_strncpy(label, IFACE_(name), maxlen);
}

static const fn::MultiFunction *get_multi_function(bNode &bnode)
{
  static auto exec_preset = fn::CustomMF_presets::AllSpanOrSingle();
  static fn::CustomMF_SI_SO<float, int> round_fn{
      "Round", [](float a) { return (int)round(a); }, exec_preset};
  static fn::CustomMF_SI_SO<float, int> floor_fn{
      "Floor", [](float a) { return (int)floor(a); }, exec_preset};
  static fn::CustomMF_SI_SO<float, int> ceil_fn{
      "Ceiling", [](float a) { return (int)ceil(a); }, exec_preset};
  static fn::CustomMF_SI_SO<float, int> trunc_fn{
      "Truncate", [](float a) { return (int)trunc(a); }, exec_preset};

  switch (static_cast<FloatToIntRoundingMode>(bnode.custom1)) {
    case FN_NODE_FLOAT_TO_INT_ROUND:
      return &round_fn;
    case FN_NODE_FLOAT_TO_INT_FLOOR:
      return &floor_fn;
    case FN_NODE_FLOAT_TO_INT_CEIL:
      return &ceil_fn;
    case FN_NODE_FLOAT_TO_INT_TRUNCATE:
      return &trunc_fn;
  }

  BLI_assert_unreachable();
  return nullptr;
}

static void fn_node_float_to_int_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const fn::MultiFunction *fn = get_multi_function(builder.node());
  builder.set_matching_fn(fn);
}

}  // namespace blender::nodes::node_fn_float_to_int_cc

void register_node_type_fn_float_to_int()
{
  namespace file_ns = blender::nodes::node_fn_float_to_int_cc;

  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_FLOAT_TO_INT, "Float to Integer", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::fn_node_float_to_int_declare;
  ntype.labelfunc = file_ns::node_float_to_int_label;
  ntype.build_multi_function = file_ns::fn_node_float_to_int_build_multi_function;
  ntype.draw_buttons = file_ns::fn_node_float_to_int_layout;
  nodeRegisterType(&ntype);
}
