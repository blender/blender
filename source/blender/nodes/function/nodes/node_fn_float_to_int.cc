/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cmath>

#include "BLI_string_utf8.h"

#include "RNA_enum_types.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_float_to_int_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Float>("Float");
  b.add_output<decl::Int>("Integer");
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "rounding_mode", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_label(const bNodeTree * /*tree*/,
                       const bNode *node,
                       char *label,
                       int label_maxncpy)
{
  const char *name;
  bool enum_label = RNA_enum_name(rna_enum_node_float_to_int_items, node->custom1, &name);
  if (!enum_label) {
    name = CTX_N_(BLT_I18NCONTEXT_ID_NODETREE, "Unknown");
  }
  BLI_strncpy_utf8(label, CTX_IFACE_(BLT_I18NCONTEXT_ID_NODETREE, name), label_maxncpy);
}

static const mf::MultiFunction *get_multi_function(const bNode &bnode)
{
  static auto exec_preset = mf::build::exec_presets::AllSpanOrSingle();
  static auto round_fn = mf::build::SI1_SO<float, int>(
      "Round", [](float a) { return int(round(a)); }, exec_preset);
  static auto floor_fn = mf::build::SI1_SO<float, int>(
      "Floor", [](float a) { return int(floor(a)); }, exec_preset);
  static auto ceil_fn = mf::build::SI1_SO<float, int>(
      "Ceiling", [](float a) { return int(ceil(a)); }, exec_preset);
  static auto trunc_fn = mf::build::SI1_SO<float, int>(
      "Truncate", [](float a) { return int(trunc(a)); }, exec_preset);

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

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const mf::MultiFunction *fn = get_multi_function(builder.node());
  builder.set_matching_fn(fn);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  fn_node_type_base(&ntype, "FunctionNodeFloatToInt", FN_NODE_FLOAT_TO_INT);
  ntype.ui_name = "Float to Integer";
  ntype.ui_description =
      "Convert the given floating-point number to an integer, with a choice of methods";
  ntype.enum_name_legacy = "FLOAT_TO_INT";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.labelfunc = node_label;
  ntype.build_multi_function = node_build_multi_function;
  ntype.draw_buttons = node_layout;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_float_to_int_cc
