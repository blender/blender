/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_hash.h"
#include "BLI_math_matrix_types.hh"
#include "BLI_noise.hh"

#include "NOD_rna_define.hh"
#include "NOD_socket_search_link.hh"

#include "RNA_enum_types.hh"

#include "node_function_util.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_fn_hash_value_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();

  const bNode *node = b.node_or_null();
  if (node) {
    const eNodeSocketDatatype data_type = eNodeSocketDatatype(node->custom1);
    b.add_input(data_type, "Value");
  }
  b.add_input<decl::Int>("Seed", "Seed");
  b.add_output<decl::Int>("Hash");
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = SOCK_INT;
}

static const mf::MultiFunction *get_multi_function(const bNode &bnode)
{
  const eNodeSocketDatatype socket_type = eNodeSocketDatatype(bnode.custom1);

  static auto exec_preset = mf::build::exec_presets::AllSpanOrSingle();

  static auto fn_hash_float = mf::build::SI2_SO<float, int, int>(
      "Hash Float",
      [](float a, int seed) { return noise::hash(noise::hash_float(a), seed); },
      exec_preset);
  static auto fn_hash_vector = mf::build::SI2_SO<float3, int, int>(
      "Hash Vector",
      [](float3 a, int seed) { return noise::hash(noise::hash_float(a), seed); },
      exec_preset);
  static auto fn_hash_color = mf::build::SI2_SO<ColorGeometry4f, int, int>(
      "Hash Color",
      [](ColorGeometry4f a, int seed) { return noise::hash(noise::hash_float(float4(a)), seed); },
      exec_preset);
  static auto fn_hash_int = mf::build::SI2_SO<int, int, int>(
      "Hash Integer",
      [](int a, int seed) { return noise::hash(noise::hash(a), seed); },
      exec_preset);
  static auto fn_hash_string = mf::build::SI2_SO<std::string, int, int>(
      "Hash String",
      [](std::string a, int seed) { return noise::hash(BLI_hash_string(a.c_str()), seed); },
      exec_preset);
  static auto fn_hash_rotation = mf::build::SI2_SO<math::Quaternion, int, int>(
      "Hash Rotation",
      [](math::Quaternion a, int seed) { return noise::hash(noise::hash_float(float4(a)), seed); },
      exec_preset);
  static auto fn_hash_matrix = mf::build::SI2_SO<float4x4, int, int>(
      "Hash Matrix",
      [](float4x4 a, int seed) { return noise::hash(noise::hash_float(a), seed); },
      exec_preset);

  switch (socket_type) {
    case SOCK_MATRIX:
      return &fn_hash_matrix;
    case SOCK_ROTATION:
      return &fn_hash_rotation;
    case SOCK_STRING:
      return &fn_hash_string;
    case SOCK_FLOAT:
      return &fn_hash_float;
    case SOCK_VECTOR:
      return &fn_hash_vector;
    case SOCK_RGBA:
      return &fn_hash_color;
    case SOCK_INT:
      return &fn_hash_int;
    default:
      BLI_assert_unreachable();
      return nullptr;
  }
}

class SocketSearchOp {
 public:
  const StringRef socket_name;
  eNodeSocketDatatype socket_type;
  void operator()(LinkSearchOpParams &params)
  {
    bNode &node = params.add_node("FunctionNodeHashValue");
    node.custom1 = socket_type;
    params.update_and_connect_available_socket(node, socket_name);
  }
};

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const mf::MultiFunction *fn = get_multi_function(builder.node());
  builder.set_matching_fn(fn);
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  eNodeSocketDatatype socket_type = eNodeSocketDatatype(params.other_socket().type);
  if (!ELEM(socket_type,
            SOCK_BOOLEAN,
            SOCK_FLOAT,
            SOCK_INT,
            SOCK_ROTATION,
            SOCK_MATRIX,
            SOCK_VECTOR,
            SOCK_STRING,
            SOCK_RGBA))
  {
    return;
  }

  if (params.in_out() == SOCK_IN) {
    if (socket_type == SOCK_BOOLEAN) {
      socket_type = SOCK_INT;
    }
    params.add_item(IFACE_("Value"), SocketSearchOp{"Value", socket_type});
    params.add_item(IFACE_("Seed"), SocketSearchOp{"Seed", SOCK_INT});
  }
  else {
    if (!ELEM(socket_type, SOCK_STRING)) {
      const int weight = ELEM(params.other_socket().type, SOCK_INT) ? 0 : -1;
      params.add_item(IFACE_("Hash"), SocketSearchOp{"Hash", SOCK_INT}, weight);
    }
  }
}

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(
      srna,
      "data_type",
      "Data Type",
      "",
      rna_enum_node_socket_data_type_items,
      NOD_inline_enum_accessors(custom1),
      SOCK_INT,
      [](bContext * /*C*/, PointerRNA * /*ptr*/, PropertyRNA * /*prop*/, bool *r_free) {
        *r_free = true;
        return enum_items_filter(rna_enum_node_socket_data_type_items,
                                 [](const EnumPropertyItem &item) -> bool {
                                   return ELEM(item.value,
                                               SOCK_FLOAT,
                                               SOCK_INT,
                                               SOCK_MATRIX,
                                               SOCK_ROTATION,
                                               SOCK_VECTOR,
                                               SOCK_STRING,
                                               SOCK_RGBA);
                                 });
      });
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  fn_node_type_base(&ntype, "FunctionNodeHashValue", FN_NODE_HASH_VALUE);
  ntype.ui_name = "Hash Value";
  ntype.ui_description = "Generate a randomized integer using the given input value as a seed";
  ntype.enum_name_legacy = "HASH_VALUE";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.build_multi_function = node_build_multi_function;
  ntype.draw_buttons = node_layout;
  ntype.gather_link_search_ops = node_gather_link_searches;
  blender::bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_hash_value_cc
