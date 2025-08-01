/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_attribute_math.hh"

#include "BLI_array.hh"
#include "BLI_generic_virtual_array.hh"
#include "BLI_math_vector.h"
#include "BLI_vector.hh"
#include "BLI_virtual_array.hh"

#include "NOD_rna_define.hh"
#include "NOD_socket_search_link.hh"

#include "RNA_enum_types.hh"

#include "node_geometry_util.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include <numeric>

namespace blender::nodes::node_geo_field_average_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();

  if (node != nullptr) {
    const eCustomDataType data_type = eCustomDataType(node->custom1);
    b.add_input(data_type, "Value")
        .supports_field()
        .description("The values the mean and median will be calculated from");
  }

  b.add_input<decl::Int>("Group ID", "Group Index")
      .supports_field()
      .hide_value()
      .description("An index used to group values together for multiple separate operations");

  if (node != nullptr) {
    const eCustomDataType data_type = eCustomDataType(node->custom1);
    b.add_output(data_type, "Mean")
        .field_source_reference_all()
        .description("The sum of all values in each group divided by the size of said group");
    b.add_output(data_type, "Median")
        .translation_context(BLT_I18NCONTEXT_ID_NODETREE)
        .field_source_reference_all()
        .description(
            "The middle value in each group when all values are sorted from lowest to highest");
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

enum class Operation { Mean = 0, Median = 1 };

static std::optional<eCustomDataType> node_type_from_other_socket(const bNodeSocket &socket)
{
  switch (socket.type) {
    case SOCK_FLOAT:
    case SOCK_BOOLEAN:
    case SOCK_INT:
      return CD_PROP_FLOAT;
    case SOCK_VECTOR:
    case SOCK_RGBA:
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
        IFACE_("Mean"),
        [type](LinkSearchOpParams &params) {
          bNode &node = params.add_node("GeometryNodeFieldAverage");
          node.custom1 = *type;
          params.update_and_connect_available_socket(node, "Mean");
        },
        0);
    params.add_item(
        CTX_IFACE_(BLT_I18NCONTEXT_ID_NODETREE, "Median"),
        [type](LinkSearchOpParams &params) {
          bNode &node = params.add_node("GeometryNodeFieldAverage");
          node.custom1 = *type;
          params.update_and_connect_available_socket(node, "Median");
        },
        -1);
  }
  else {
    params.add_item(
        IFACE_("Value"),
        [type](LinkSearchOpParams &params) {
          bNode &node = params.add_node("GeometryNodeFieldAverage");
          node.custom1 = *type;
          params.update_and_connect_available_socket(node, "Value");
        },
        0);
  }
}

template<typename T> static T calculate_median(MutableSpan<T> values)
{
  if constexpr (std::is_same<T, float3>::value) {
    Array<float> x_vals(values.size()), y_vals(values.size()), z_vals(values.size());

    for (const int i : values.index_range()) {
      float3 value = values[i];
      x_vals[i] = value.x;
      y_vals[i] = value.y;
      z_vals[i] = value.z;
    }

    return float3(calculate_median<float>(x_vals),
                  calculate_median<float>(y_vals),
                  calculate_median<float>(z_vals));
  }
  else {
    const auto middle_itr = values.begin() + values.size() / 2;
    std::nth_element(values.begin(), middle_itr, values.end());
    if (values.size() % 2 == 0) {
      const auto left_middle_itr = std::max_element(values.begin(), middle_itr);
      return math::midpoint<T>(*left_middle_itr, *middle_itr);
    }
    return *middle_itr;
  }
}

class FieldAverageInput final : public bke::GeometryFieldInput {
 private:
  GField input_;
  Field<int> group_index_;
  AttrDomain source_domain_;
  Operation operation_;

 public:
  FieldAverageInput(const AttrDomain source_domain,
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
        const VArraySpan<T> values = g_values.typed<T>();

        if (operation_ == Operation::Mean) {
          if (group_indices.is_single()) {
            const T mean = std::reduce(values.begin(), values.end(), T()) / domain_size;
            g_outputs = VArray<T>::from_single(mean, domain_size);
          }
          else {
            Map<int, std::pair<T, int>> sum_and_counts;
            for (const int i : values.index_range()) {
              auto &pair = sum_and_counts.lookup_or_add(group_indices[i], std::make_pair(T(), 0));
              pair.first = pair.first + values[i];
              pair.second = pair.second + 1;
            }

            Array<T> outputs(domain_size);
            for (const int i : values.index_range()) {
              const auto &pair = sum_and_counts.lookup(group_indices[i]);
              outputs[i] = pair.first / pair.second;
            }
            g_outputs = VArray<T>::from_container(std::move(outputs));
          }
        }
        else {
          if (group_indices.is_single()) {
            Array<T> sorted_values(values);
            T median = calculate_median<T>(sorted_values);
            g_outputs = VArray<T>::from_single(median, domain_size);
          }
          else {
            Map<int, Vector<T>> groups;
            for (const int i : values.index_range()) {
              groups.lookup_or_add(group_indices[i], Vector<T>()).append(values[i]);
            }

            Map<int, T> medians;
            for (MutableMapItem<int, Vector<T>> group : groups.items()) {
              medians.add(group.key, calculate_median<T>(group.value));
            }

            Array<T> outputs(domain_size);
            for (const int i : values.index_range()) {
              outputs[i] = medians.lookup(group_indices[i]);
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
    if (const FieldAverageInput *other_field = dynamic_cast<const FieldAverageInput *>(&other)) {
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
  if (params.output_is_required("Mean")) {
    params.set_output<GField>(
        "Mean",
        GField{std::make_shared<FieldAverageInput>(
            source_domain, input_field, group_index_field, Operation::Mean)});
  }
  if (params.output_is_required("Median")) {
    params.set_output<GField>(
        "Median",
        GField{std::make_shared<FieldAverageInput>(
            source_domain, input_field, group_index_field, Operation::Median)});
  }
}

static void node_rna(StructRNA *srna)
{
  static EnumPropertyItem items[] = {
      {CD_PROP_FLOAT, "FLOAT", 0, "Float", "Floating-point value"},
      {CD_PROP_FLOAT3, "FLOAT_VECTOR", 0, "Vector", "3D vector with floating-point values"},
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

  geo_node_type_base(&ntype, "GeometryNodeFieldAverage");
  ntype.ui_name = "Field Average";
  ntype.ui_description = "Calculate the mean and median of a given field";
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

}  // namespace blender::nodes::node_geo_field_average_cc
