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

#include <algorithm>
#include <numeric>

#include "UI_interface.h"
#include "UI_resources.h"

#include "BLI_math_base_safe.h"

#include "node_geometry_util.hh"

namespace blender::nodes {

static void geo_node_attribute_statistic_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::Float>(N_("Attribute")).hide_value().supports_field();
  b.add_input<decl::Vector>(N_("Attribute"), "Attribute_001").hide_value().supports_field();

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

static void geo_node_attribute_statistic_layout(uiLayout *layout,
                                                bContext *UNUSED(C),
                                                PointerRNA *ptr)
{
  uiItemR(layout, ptr, "data_type", 0, "", ICON_NONE);
  uiItemR(layout, ptr, "domain", 0, "", ICON_NONE);
}

static void geo_node_attribute_statistic_init(bNodeTree *UNUSED(tree), bNode *node)
{
  node->custom1 = CD_PROP_FLOAT;
  node->custom2 = ATTR_DOMAIN_POINT;
}

static void geo_node_attribute_statistic_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  bNodeSocket *socket_geo = (bNodeSocket *)node->inputs.first;
  bNodeSocket *socket_float_attr = socket_geo->next;
  bNodeSocket *socket_float3_attr = socket_float_attr->next;

  bNodeSocket *socket_float_mean = (bNodeSocket *)node->outputs.first;
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

  const CustomDataType data_type = static_cast<CustomDataType>(node->custom1);

  nodeSetSocketAvailability(socket_float_attr, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(socket_float_mean, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(socket_float_median, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(socket_float_sum, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(socket_float_min, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(socket_float_max, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(socket_float_range, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(socket_float_std, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(socket_float_variance, data_type == CD_PROP_FLOAT);

  nodeSetSocketAvailability(socket_float3_attr, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(socket_vector_mean, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(socket_vector_median, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(socket_vector_sum, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(socket_vector_min, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(socket_vector_max, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(socket_vector_range, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(socket_vector_std, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(socket_vector_variance, data_type == CD_PROP_FLOAT3);
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

  return sum_of_squared_differences / (data.size() - 1);
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
static void set_empty(CustomDataType data_type, GeoNodeExecParams &params)
{
  if (data_type == CD_PROP_FLOAT) {
    params.set_output("Mean", 0.0f);
    params.set_output("Median", 0.0f);
    params.set_output("Sum", 0.0f);
    params.set_output("Min", 0.0f);
    params.set_output("Max", 0.0f);
    params.set_output("Range", 0.0f);
    params.set_output("Standard Deviation", 0.0f);
    params.set_output("Variance", 0.0f);
  }
  else if (data_type == CD_PROP_FLOAT3) {
    params.set_output("Mean_001", float3{0.0f, 0.0f, 0.0f});
    params.set_output("Median_001", float3{0.0f, 0.0f, 0.0f});
    params.set_output("Sum_001", float3{0.0f, 0.0f, 0.0f});
    params.set_output("Min_001", float3{0.0f, 0.0f, 0.0f});
    params.set_output("Max_001", float3{0.0f, 0.0f, 0.0f});
    params.set_output("Range_001", float3{0.0f, 0.0f, 0.0f});
    params.set_output("Standard Deviation_001", float3{0.0f, 0.0f, 0.0f});
    params.set_output("Variance_001", float3{0.0f, 0.0f, 0.0f});
  }
}

static void geo_node_attribute_statistic_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.get_input<GeometrySet>("Geometry");

  const bNode &node = params.node();
  const CustomDataType data_type = static_cast<CustomDataType>(node.custom1);
  const AttributeDomain domain = static_cast<AttributeDomain>(node.custom2);

  int64_t total_size = 0;
  Vector<const GeometryComponent *> components = geometry_set.get_components_for_read();

  for (const GeometryComponent *component : components) {
    if (component->attribute_domain_supported(domain)) {
      total_size += component->attribute_domain_size(domain);
    }
  }
  if (total_size == 0) {
    set_empty(data_type, params);
    return;
  }

  switch (data_type) {
    case CD_PROP_FLOAT: {
      const Field<float> input_field = params.get_input<Field<float>>("Attribute");
      Array<float> data = Array<float>(total_size);
      int offset = 0;
      for (const GeometryComponent *component : components) {
        if (component->attribute_domain_supported(domain)) {
          GeometryComponentFieldContext field_context{*component, domain};
          const int domain_size = component->attribute_domain_size(domain);
          fn::FieldEvaluator data_evaluator{field_context, domain_size};
          MutableSpan<float> component_result = data.as_mutable_span().slice(offset, domain_size);
          data_evaluator.add_with_destination(input_field, component_result);
          data_evaluator.evaluate();
          offset += domain_size;
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

      if (total_size != 0) {
        if (sort_required) {
          std::sort(data.begin(), data.end());
          median = median_of_sorted_span(data);

          min = data.first();
          max = data.last();
          range = max - min;
        }
        if (sum_required || variance_required) {
          sum = compute_sum<float>(data);
          mean = sum / total_size;

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

      Array<float3> data = Array<float3>(total_size);
      int offset = 0;
      for (const GeometryComponent *component : components) {
        if (component->attribute_domain_supported(domain)) {
          GeometryComponentFieldContext field_context{*component, domain};
          const int domain_size = component->attribute_domain_size(domain);
          fn::FieldEvaluator data_evaluator{field_context, domain_size};
          MutableSpan<float3> component_result = data.as_mutable_span().slice(offset, domain_size);
          data_evaluator.add_with_destination(input_field, component_result);
          data_evaluator.evaluate();
          offset += domain_size;
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
        data_x.reinitialize(total_size);
        data_y.reinitialize(total_size);
        data_z.reinitialize(total_size);
        for (const int i : data.index_range()) {
          data_x[i] = data[i].x;
          data_y[i] = data[i].y;
          data_z[i] = data[i].z;
        }
      }

      if (total_size != 0) {
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
          mean = sum / total_size;

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

}  // namespace blender::nodes

void register_node_type_geo_attribute_statistic()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_ATTRIBUTE_STATISTIC, "Attribute Statistic", NODE_CLASS_ATTRIBUTE, 0);

  ntype.declare = blender::nodes::geo_node_attribute_statistic_declare;
  node_type_init(&ntype, blender::nodes::geo_node_attribute_statistic_init);
  node_type_update(&ntype, blender::nodes::geo_node_attribute_statistic_update);
  ntype.geometry_node_execute = blender::nodes::geo_node_attribute_statistic_exec;
  ntype.draw_buttons = blender::nodes::geo_node_attribute_statistic_layout;
  nodeRegisterType(&ntype);
}
