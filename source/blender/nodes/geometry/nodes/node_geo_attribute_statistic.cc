/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>
#include <numeric>

#include "UI_interface.h"
#include "UI_resources.h"

#include "BLI_math_base_safe.h"

#include "NOD_socket_search_link.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_attribute_statistic_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).field_on_all().hide_value();
  b.add_input<decl::Float>(N_("Attribute")).hide_value().field_on_all();
  b.add_input<decl::Vector>(N_("Attribute"), "Attribute_001").hide_value().field_on_all();

  b.add_output<decl::Float>(N_("Mean"));
  b.add_output<decl::Float>(N_("Median"));
  b.add_output<decl::Float>(N_("Sum"));
  b.add_output<decl::Float>(N_("Min"));
  b.add_output<decl::Float>(N_("Max"));
  b.add_output<decl::Float>(N_("Range"));
  b.add_output<decl::Float>(N_("Standard Deviation"));
  b.add_output<decl::Float>(N_("Variance"));

  b.add_output<decl::Vector>(N_("Mean"), "Mean_001");
  b.add_output<decl::Vector>(N_("Median"), "Median_001");
  b.add_output<decl::Vector>(N_("Sum"), "Sum_001");
  b.add_output<decl::Vector>(N_("Min"), "Min_001");
  b.add_output<decl::Vector>(N_("Max"), "Max_001");
  b.add_output<decl::Vector>(N_("Range"), "Range_001");
  b.add_output<decl::Vector>(N_("Standard Deviation"), "Standard Deviation_001");
  b.add_output<decl::Vector>(N_("Variance"), "Variance_001");
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "data_type", 0, "", ICON_NONE);
  uiItemR(layout, ptr, "domain", 0, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = CD_PROP_FLOAT;
  node->custom2 = ATTR_DOMAIN_POINT;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *socket_geo = static_cast<bNodeSocket *>(node->inputs.first);
  bNodeSocket *socket_selection = socket_geo->next;
  bNodeSocket *socket_float_attr = socket_selection->next;
  bNodeSocket *socket_float3_attr = socket_float_attr->next;

  bNodeSocket *socket_float_mean = static_cast<bNodeSocket *>(node->outputs.first);
  bNodeSocket *socket_float_median = socket_float_mean->next;
  bNodeSocket *socket_float_sum = socket_float_median->next;
  bNodeSocket *socket_float_min = socket_float_sum->next;
  bNodeSocket *socket_float_max = socket_float_min->next;
  bNodeSocket *socket_float_range = socket_float_max->next;
  bNodeSocket *socket_float_std = socket_float_range->next;
  bNodeSocket *socket_float_variance = socket_float_std->next;

  bNodeSocket *socket_vector_mean = socket_float_variance->next;
  bNodeSocket *socket_vector_median = socket_vector_mean->next;
  bNodeSocket *socket_vector_sum = socket_vector_median->next;
  bNodeSocket *socket_vector_min = socket_vector_sum->next;
  bNodeSocket *socket_vector_max = socket_vector_min->next;
  bNodeSocket *socket_vector_range = socket_vector_max->next;
  bNodeSocket *socket_vector_std = socket_vector_range->next;
  bNodeSocket *socket_vector_variance = socket_vector_std->next;

  const eCustomDataType data_type = eCustomDataType(node->custom1);

  bke::nodeSetSocketAvailability(ntree, socket_float_attr, data_type == CD_PROP_FLOAT);
  bke::nodeSetSocketAvailability(ntree, socket_float_mean, data_type == CD_PROP_FLOAT);
  bke::nodeSetSocketAvailability(ntree, socket_float_median, data_type == CD_PROP_FLOAT);
  bke::nodeSetSocketAvailability(ntree, socket_float_sum, data_type == CD_PROP_FLOAT);
  bke::nodeSetSocketAvailability(ntree, socket_float_min, data_type == CD_PROP_FLOAT);
  bke::nodeSetSocketAvailability(ntree, socket_float_max, data_type == CD_PROP_FLOAT);
  bke::nodeSetSocketAvailability(ntree, socket_float_range, data_type == CD_PROP_FLOAT);
  bke::nodeSetSocketAvailability(ntree, socket_float_std, data_type == CD_PROP_FLOAT);
  bke::nodeSetSocketAvailability(ntree, socket_float_variance, data_type == CD_PROP_FLOAT);

  bke::nodeSetSocketAvailability(ntree, socket_float3_attr, data_type == CD_PROP_FLOAT3);
  bke::nodeSetSocketAvailability(ntree, socket_vector_mean, data_type == CD_PROP_FLOAT3);
  bke::nodeSetSocketAvailability(ntree, socket_vector_median, data_type == CD_PROP_FLOAT3);
  bke::nodeSetSocketAvailability(ntree, socket_vector_sum, data_type == CD_PROP_FLOAT3);
  bke::nodeSetSocketAvailability(ntree, socket_vector_min, data_type == CD_PROP_FLOAT3);
  bke::nodeSetSocketAvailability(ntree, socket_vector_max, data_type == CD_PROP_FLOAT3);
  bke::nodeSetSocketAvailability(ntree, socket_vector_range, data_type == CD_PROP_FLOAT3);
  bke::nodeSetSocketAvailability(ntree, socket_vector_std, data_type == CD_PROP_FLOAT3);
  bke::nodeSetSocketAvailability(ntree, socket_vector_variance, data_type == CD_PROP_FLOAT3);
}

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
      return {};
  }
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const bNodeType &node_type = params.node_type();
  const NodeDeclaration &declaration = *params.node_type().fixed_declaration;
  search_link_ops_for_declarations(params, declaration.inputs.as_span().take_front(2));

  const std::optional<eCustomDataType> type = node_type_from_other_socket(params.other_socket());
  if (!type) {
    return;
  }

  if (params.in_out() == SOCK_IN) {
    params.add_item(IFACE_("Attribute"), [node_type, type](LinkSearchOpParams &params) {
      bNode &node = params.add_node(node_type);
      node.custom1 = *type;
      params.update_and_connect_available_socket(node, "Attribute");
    });
  }
  else {
    for (const StringRefNull name :
         {"Mean", "Median", "Sum", "Min", "Max", "Range", "Standard Deviation", "Variance"})
    {
      params.add_item(IFACE_(name.c_str()), [node_type, name, type](LinkSearchOpParams &params) {
        bNode &node = params.add_node(node_type);
        node.custom1 = *type;
        params.update_and_connect_available_socket(node, name);
      });
    }
  }
}

template<typename T> static T compute_sum(const Span<T> data)
{
  return std::accumulate(data.begin(), data.end(), T());
}

static float compute_variance(const Span<float> data, const float mean)
{
  if (data.size() <= 1) {
    return 0.0f;
  }

  float sum_of_squared_differences = std::accumulate(
      data.begin(), data.end(), 0.0f, [mean](float accumulator, float value) {
        float difference = mean - value;
        return accumulator + difference * difference;
      });

  return sum_of_squared_differences / data.size();
}

static float median_of_sorted_span(const Span<float> data)
{
  if (data.is_empty()) {
    return 0.0f;
  }

  const float median = data[data.size() / 2];

  /* For spans of even length, the median is the average of the middle two elements. */
  if (data.size() % 2 == 0) {
    return (median + data[data.size() / 2 - 1]) * 0.5f;
  }
  return median;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.get_input<GeometrySet>("Geometry");
  const bNode &node = params.node();
  const eCustomDataType data_type = eCustomDataType(node.custom1);
  const eAttrDomain domain = eAttrDomain(node.custom2);
  Vector<const GeometryComponent *> components = geometry_set.get_components_for_read();

  const Field<bool> selection_field = params.get_input<Field<bool>>("Selection");

  switch (data_type) {
    case CD_PROP_FLOAT: {
      const Field<float> input_field = params.get_input<Field<float>>("Attribute");
      Vector<float> data;
      for (const GeometryComponent *component : components) {
        const std::optional<AttributeAccessor> attributes = component->attributes();
        if (!attributes.has_value()) {
          continue;
        }
        if (attributes->domain_supported(domain)) {
          const bke::GeometryFieldContext field_context{*component, domain};
          fn::FieldEvaluator data_evaluator{field_context, attributes->domain_size(domain)};
          data_evaluator.add(input_field);
          data_evaluator.set_selection(selection_field);
          data_evaluator.evaluate();
          const VArray<float> component_data = data_evaluator.get_evaluated<float>(0);
          const IndexMask selection = data_evaluator.get_evaluated_selection_as_mask();

          const int next_data_index = data.size();
          data.resize(next_data_index + selection.size());
          MutableSpan<float> selected_data = data.as_mutable_span().slice(next_data_index,
                                                                          selection.size());
          for (const int i : selection.index_range()) {
            selected_data[i] = component_data[selection[i]];
          }
        }
      }

      float mean = 0.0f;
      float median = 0.0f;
      float sum = 0.0f;
      float min = 0.0f;
      float max = 0.0f;
      float range = 0.0f;
      float standard_deviation = 0.0f;
      float variance = 0.0f;
      const bool sort_required = params.output_is_required("Min") ||
                                 params.output_is_required("Max") ||
                                 params.output_is_required("Range") ||
                                 params.output_is_required("Median");
      const bool sum_required = params.output_is_required("Sum") ||
                                params.output_is_required("Mean");
      const bool variance_required = params.output_is_required("Standard Deviation") ||
                                     params.output_is_required("Variance");

      if (data.size() != 0) {
        if (sort_required) {
          std::sort(data.begin(), data.end());
          median = median_of_sorted_span(data);

          min = data.first();
          max = data.last();
          range = max - min;
        }
        if (sum_required || variance_required) {
          sum = compute_sum<float>(data);
          mean = sum / data.size();

          if (variance_required) {
            variance = compute_variance(data, mean);
            standard_deviation = std::sqrt(variance);
          }
        }
      }

      if (sum_required) {
        params.set_output("Sum", sum);
        params.set_output("Mean", mean);
      }
      if (sort_required) {
        params.set_output("Min", min);
        params.set_output("Max", max);
        params.set_output("Range", range);
        params.set_output("Median", median);
      }
      if (variance_required) {
        params.set_output("Standard Deviation", standard_deviation);
        params.set_output("Variance", variance);
      }
      break;
    }
    case CD_PROP_FLOAT3: {
      const Field<float3> input_field = params.get_input<Field<float3>>("Attribute_001");
      Vector<float3> data;
      for (const GeometryComponent *component : components) {
        const std::optional<AttributeAccessor> attributes = component->attributes();
        if (!attributes.has_value()) {
          continue;
        }
        if (attributes->domain_supported(domain)) {
          const bke::GeometryFieldContext field_context{*component, domain};
          fn::FieldEvaluator data_evaluator{field_context, attributes->domain_size(domain)};
          data_evaluator.add(input_field);
          data_evaluator.set_selection(selection_field);
          data_evaluator.evaluate();
          const VArray<float3> component_data = data_evaluator.get_evaluated<float3>(0);
          const IndexMask selection = data_evaluator.get_evaluated_selection_as_mask();

          const int next_data_index = data.size();
          data.resize(data.size() + selection.size());
          MutableSpan<float3> selected_data = data.as_mutable_span().slice(next_data_index,
                                                                           selection.size());
          for (const int i : selection.index_range()) {
            selected_data[i] = component_data[selection[i]];
          }
        }
      }

      float3 median{0};
      float3 min{0};
      float3 max{0};
      float3 range{0};
      float3 sum{0};
      float3 mean{0};
      float3 variance{0};
      float3 standard_deviation{0};
      const bool sort_required = params.output_is_required("Min_001") ||
                                 params.output_is_required("Max_001") ||
                                 params.output_is_required("Range_001") ||
                                 params.output_is_required("Median_001");
      const bool sum_required = params.output_is_required("Sum_001") ||
                                params.output_is_required("Mean_001");
      const bool variance_required = params.output_is_required("Standard Deviation_001") ||
                                     params.output_is_required("Variance_001");

      Array<float> data_x;
      Array<float> data_y;
      Array<float> data_z;
      if (sort_required || variance_required) {
        data_x.reinitialize(data.size());
        data_y.reinitialize(data.size());
        data_z.reinitialize(data.size());
        for (const int i : data.index_range()) {
          data_x[i] = data[i].x;
          data_y[i] = data[i].y;
          data_z[i] = data[i].z;
        }
      }

      if (data.size() != 0) {
        if (sort_required) {
          std::sort(data_x.begin(), data_x.end());
          std::sort(data_y.begin(), data_y.end());
          std::sort(data_z.begin(), data_z.end());

          const float x_median = median_of_sorted_span(data_x);
          const float y_median = median_of_sorted_span(data_y);
          const float z_median = median_of_sorted_span(data_z);
          median = float3(x_median, y_median, z_median);

          min = float3(data_x.first(), data_y.first(), data_z.first());
          max = float3(data_x.last(), data_y.last(), data_z.last());
          range = max - min;
        }
        if (sum_required || variance_required) {
          sum = compute_sum(data.as_span());
          mean = sum / data.size();

          if (variance_required) {
            const float x_variance = compute_variance(data_x, mean.x);
            const float y_variance = compute_variance(data_y, mean.y);
            const float z_variance = compute_variance(data_z, mean.z);
            variance = float3(x_variance, y_variance, z_variance);
            standard_deviation = float3(
                std::sqrt(variance.x), std::sqrt(variance.y), std::sqrt(variance.z));
          }
        }
      }

      if (sum_required) {
        params.set_output("Sum_001", sum);
        params.set_output("Mean_001", mean);
      }
      if (sort_required) {
        params.set_output("Min_001", min);
        params.set_output("Max_001", max);
        params.set_output("Range_001", range);
        params.set_output("Median_001", median);
      }
      if (variance_required) {
        params.set_output("Standard Deviation_001", standard_deviation);
        params.set_output("Variance_001", variance);
      }
      break;
    }
    default:
      break;
  }
}

}  // namespace blender::nodes::node_geo_attribute_statistic_cc

void register_node_type_geo_attribute_statistic()
{
  namespace file_ns = blender::nodes::node_geo_attribute_statistic_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_ATTRIBUTE_STATISTIC, "Attribute Statistic", NODE_CLASS_ATTRIBUTE);

  ntype.declare = file_ns::node_declare;
  ntype.initfunc = file_ns::node_init;
  ntype.updatefunc = file_ns::node_update;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.gather_link_search_ops = file_ns::node_gather_link_searches;
  nodeRegisterType(&ntype);
}
