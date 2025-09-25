/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_attribute_math.hh"

#include "BLI_array.hh"
#include "BLI_generic_virtual_array.hh"
#include "BLI_virtual_array.hh"

#include "NOD_rna_define.hh"
#include "NOD_socket_search_link.hh"

#include "RNA_enum_types.hh"

#include "node_geometry_util.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_geo_field_min_and_max_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();

  if (node != nullptr) {
    const eCustomDataType data_type = eCustomDataType(node->custom1);
    b.add_input(data_type, "Value")
        .supports_field()
        .description("The values the minimum and maximum will be calculated from");
  }

  b.add_input<decl::Int>("Group ID", "Group Index")
      .supports_field()
      .hide_value()
      .description("An index used to group values together for multiple separate operations");

  if (node != nullptr) {
    const eCustomDataType data_type = eCustomDataType(node->custom1);
    b.add_output(data_type, "Min")
        .field_source_reference_all()
        .description("The lowest value in each group");
    b.add_output(data_type, "Max")
        .field_source_reference_all()
        .description("The highest value in each group");
  }
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
  layout->prop(ptr, "domain", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = CD_PROP_FLOAT;
  node->custom2 = int16_t(AttrDomain::Point);
}

enum class Operation { Min = 0, Max = 1 };

static std::optional<eCustomDataType> node_type_from_other_socket(const bNodeSocket &socket)
{
  switch (socket.type) {
    case SOCK_FLOAT:
      return CD_PROP_FLOAT;
    case SOCK_BOOLEAN:
    case SOCK_INT:
      return CD_PROP_INT32;
    case SOCK_VECTOR:
    case SOCK_RGBA:
    case SOCK_ROTATION:
      return CD_PROP_FLOAT3;
    default:
      return std::nullopt;
  }
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const NodeDeclaration &declaration = *params.node_type().static_declaration;
  search_link_ops_for_declarations(params, declaration.inputs);

  const std::optional<eCustomDataType> type = node_type_from_other_socket(params.other_socket());
  if (!type) {
    return;
  }
  if (params.in_out() == SOCK_OUT) {
    params.add_item(
        IFACE_("Min"),
        [type](LinkSearchOpParams &params) {
          bNode &node = params.add_node("GeometryNodeFieldMinAndMax");
          node.custom1 = *type;
          params.update_and_connect_available_socket(node, "Min");
        },
        0);
    params.add_item(
        IFACE_("Max"),
        [type](LinkSearchOpParams &params) {
          bNode &node = params.add_node("GeometryNodeFieldMinAndMax");
          node.custom1 = *type;
          params.update_and_connect_available_socket(node, "Max");
        },
        -1);
  }
  else {
    params.add_item(
        IFACE_("Value"),
        [type](LinkSearchOpParams &params) {
          bNode &node = params.add_node("GeometryNodeFieldMinAndMax");
          node.custom1 = *type;
          params.update_and_connect_available_socket(node, "Value");
        },
        0);
  }
}

template<typename T> struct MinMaxInfo {
  static inline const T min_initial_value = []() {
    if constexpr (std::is_same_v<T, float3>) {
      return float3(std::numeric_limits<float>::max());
    }
    else {
      return std::numeric_limits<T>::max();
    }
  }();

  static inline const T max_initial_value = []() {
    if constexpr (std::is_same_v<T, float3>) {
      return float3(std::numeric_limits<float>::lowest());
    }
    else {
      return std::numeric_limits<T>::lowest();
    }
  }();
};

class FieldMinMaxInput final : public bke::GeometryFieldInput {
 private:
  GField input_;
  Field<int> group_index_;
  AttrDomain source_domain_;
  Operation operation_;

 public:
  FieldMinMaxInput(const AttrDomain source_domain,
                   GField input,
                   Field<int> group_index,
                   Operation operation)
      : bke::GeometryFieldInput(input.cpp_type(), "Calculation"),
        input_(std::move(input)),
        group_index_(std::move(group_index)),
        source_domain_(source_domain),
        operation_(operation)
  {
  }

  GVArray get_varray_for_context(const bke::GeometryFieldContext &context,
                                 const IndexMask & /*mask*/) const final
  {
    const AttributeAccessor attributes = *context.attributes();
    const int64_t domain_size = attributes.domain_size(source_domain_);
    if (domain_size == 0) {
      return {};
    }

    const bke::GeometryFieldContext source_context{context, source_domain_};
    fn::FieldEvaluator evaluator{source_context, domain_size};
    evaluator.add(input_);
    evaluator.add(group_index_);
    evaluator.evaluate();
    const GVArray g_values = evaluator.get_evaluated(0);
    const VArray<int> group_indices = evaluator.get_evaluated<int>(1);

    GVArray g_outputs;

    bke::attribute_math::convert_to_static_type(g_values.type(), [&](auto dummy) {
      using T = decltype(dummy);
      if constexpr (is_same_any_v<T, int, float, float3>) {
        const VArray<T> values = g_values.typed<T>();

        if (operation_ == Operation::Min) {
          if (group_indices.is_single()) {
            T result = MinMaxInfo<T>::min_initial_value;
            for (const int i : values.index_range()) {
              result = math::min(result, values[i]);
            }
            g_outputs = VArray<T>::from_single(result, domain_size);
          }
          else {
            Map<int, T> results;
            for (const int i : values.index_range()) {
              T &value = results.lookup_or_add(group_indices[i], MinMaxInfo<T>::min_initial_value);
              value = math::min(value, values[i]);
            }
            Array<T> outputs(domain_size);
            for (const int i : values.index_range()) {
              outputs[i] = results.lookup(group_indices[i]);
            }
            g_outputs = VArray<T>::from_container(std::move(outputs));
          }
        }
        else {
          if (group_indices.is_single()) {
            T result = MinMaxInfo<T>::max_initial_value;
            for (const int i : values.index_range()) {
              result = math::max(result, values[i]);
            }
            g_outputs = VArray<T>::from_single(result, domain_size);
          }
          else {
            Map<int, T> results;
            for (const int i : values.index_range()) {
              T &value = results.lookup_or_add(group_indices[i], MinMaxInfo<T>::max_initial_value);
              value = math::max(value, values[i]);
            }
            Array<T> outputs(domain_size);
            for (const int i : values.index_range()) {
              outputs[i] = results.lookup(group_indices[i]);
            }
            g_outputs = VArray<T>::from_container(std::move(outputs));
          }
        }
      }
    });

    return attributes.adapt_domain(std::move(g_outputs), source_domain_, context.domain());
  }

  void for_each_field_input_recursive(FunctionRef<void(const FieldInput &)> fn) const final
  {
    input_.node().for_each_field_input_recursive(fn);
    group_index_.node().for_each_field_input_recursive(fn);
  }

  uint64_t hash() const override
  {
    return get_default_hash(input_, group_index_, source_domain_, operation_);
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    if (const FieldMinMaxInput *other_field = dynamic_cast<const FieldMinMaxInput *>(&other)) {
      return input_ == other_field->input_ && group_index_ == other_field->group_index_ &&
             source_domain_ == other_field->source_domain_ &&
             operation_ == other_field->operation_;
    }
    return false;
  }

  std::optional<AttrDomain> preferred_domain(
      const GeometryComponent & /*component*/) const override
  {
    return source_domain_;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  const AttrDomain source_domain = AttrDomain(params.node().custom2);

  const Field<int> group_index_field = params.extract_input<Field<int>>("Group Index");
  const GField input_field = params.extract_input<GField>("Value");
  if (params.output_is_required("Min")) {
    params.set_output<GField>("Min",
                              GField{std::make_shared<FieldMinMaxInput>(
                                  source_domain, input_field, group_index_field, Operation::Min)});
  }
  if (params.output_is_required("Max")) {
    params.set_output<GField>("Max",
                              GField{std::make_shared<FieldMinMaxInput>(
                                  source_domain, input_field, group_index_field, Operation::Max)});
  }
}

static void node_rna(StructRNA *srna)
{
  static EnumPropertyItem items[] = {
      {CD_PROP_FLOAT, "FLOAT", ICON_NODE_SOCKET_FLOAT, "Float", "Floating-point value"},
      {CD_PROP_INT32, "INT", ICON_NODE_SOCKET_INT, "Integer", "32-bit integer"},
      {CD_PROP_FLOAT3,
       "FLOAT_VECTOR",
       ICON_NODE_SOCKET_VECTOR,
       "Vector",
       "3D vector with floating-point values"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_node_enum(srna,
                    "data_type",
                    "Data Type",
                    "Type of data the outputs are calculated from",
                    items,
                    NOD_inline_enum_accessors(custom1),
                    CD_PROP_FLOAT);

  RNA_def_node_enum(srna,
                    "domain",
                    "Domain",
                    "",
                    rna_enum_attribute_domain_items,
                    NOD_inline_enum_accessors(custom2),
                    int(AttrDomain::Point),
                    nullptr,
                    true);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeFieldMinAndMax");
  ntype.ui_name = "Field Min & Max";
  ntype.ui_description = "Calculate the minimum and maximum of a given field";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.initfunc = node_init;
  ntype.draw_buttons = node_layout;
  ntype.declare = node_declare;
  ntype.gather_link_search_ops = node_gather_link_searches;
  blender::bke::node_register_type(ntype);
  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_field_min_and_max_cc
