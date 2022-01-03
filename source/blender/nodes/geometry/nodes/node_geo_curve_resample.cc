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

#include "BLI_array.hh"
#include "BLI_task.hh"
#include "BLI_timeit.hh"

#include "BKE_attribute_math.hh"
#include "BKE_spline.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_resample_cc {

NODE_STORAGE_FUNCS(NodeGeometryCurveResample)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Curve")).supported_type(GEO_COMPONENT_TYPE_CURVE);
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).supports_field().hide_value();
  b.add_input<decl::Int>(N_("Count")).default_value(10).min(1).max(100000).supports_field();
  b.add_input<decl::Float>(N_("Length"))
      .default_value(0.1f)
      .min(0.001f)
      .supports_field()
      .subtype(PROP_DISTANCE);
  b.add_output<decl::Geometry>(N_("Curve"));
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", 0, "", ICON_NONE);
}

static void node_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryCurveResample *data = MEM_cnew<NodeGeometryCurveResample>(__func__);

  data->mode = GEO_NODE_CURVE_RESAMPLE_COUNT;
  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const NodeGeometryCurveResample &storage = node_storage(*node);
  const GeometryNodeCurveResampleMode mode = (GeometryNodeCurveResampleMode)storage.mode;

  bNodeSocket *count_socket = ((bNodeSocket *)node->inputs.first)->next->next;
  bNodeSocket *length_socket = count_socket->next;

  nodeSetSocketAvailability(ntree, count_socket, mode == GEO_NODE_CURVE_RESAMPLE_COUNT);
  nodeSetSocketAvailability(ntree, length_socket, mode == GEO_NODE_CURVE_RESAMPLE_LENGTH);
}

struct SampleModeParam {
  GeometryNodeCurveResampleMode mode;
  std::optional<Field<float>> length;
  std::optional<Field<int>> count;
  Field<bool> selection;
};

static SplinePtr resample_spline(const Spline &src, const int count)
{
  std::unique_ptr<PolySpline> dst = std::make_unique<PolySpline>();
  Spline::copy_base_settings(src, *dst);

  if (src.evaluated_edges_size() < 1 || count == 1) {
    dst->resize(1);
    dst->positions().first() = src.positions().first();
    dst->radii().first() = src.radii().first();
    dst->tilts().first() = src.tilts().first();

    src.attributes.foreach_attribute(
        [&](const AttributeIDRef &attribute_id, const AttributeMetaData &meta_data) {
          std::optional<GSpan> src_attribute = src.attributes.get_for_read(attribute_id);
          if (dst->attributes.create(attribute_id, meta_data.data_type)) {
            std::optional<GMutableSpan> dst_attribute = dst->attributes.get_for_write(
                attribute_id);
            if (dst_attribute) {
              src_attribute->type().copy_assign(src_attribute->data(), dst_attribute->data());
              return true;
            }
          }
          BLI_assert_unreachable();
          return false;
        },
        ATTR_DOMAIN_POINT);
    return dst;
  }

  dst->resize(count);

  Array<float> uniform_samples = src.sample_uniform_index_factors(count);

  src.sample_with_index_factors<float3>(
      src.evaluated_positions(), uniform_samples, dst->positions());

  src.sample_with_index_factors<float>(
      src.interpolate_to_evaluated(src.radii()), uniform_samples, dst->radii());

  src.sample_with_index_factors<float>(
      src.interpolate_to_evaluated(src.tilts()), uniform_samples, dst->tilts());

  src.attributes.foreach_attribute(
      [&](const AttributeIDRef &attribute_id, const AttributeMetaData &meta_data) {
        std::optional<GSpan> input_attribute = src.attributes.get_for_read(attribute_id);
        if (dst->attributes.create(attribute_id, meta_data.data_type)) {
          std::optional<GMutableSpan> output_attribute = dst->attributes.get_for_write(
              attribute_id);
          if (output_attribute) {
            src.sample_with_index_factors(src.interpolate_to_evaluated(*input_attribute),
                                          uniform_samples,
                                          *output_attribute);
            return true;
          }
        }

        BLI_assert_unreachable();
        return false;
      },
      ATTR_DOMAIN_POINT);

  return dst;
}

static SplinePtr resample_spline_evaluated(const Spline &src)
{
  std::unique_ptr<PolySpline> dst = std::make_unique<PolySpline>();
  Spline::copy_base_settings(src, *dst);
  dst->resize(src.evaluated_points_size());

  dst->positions().copy_from(src.evaluated_positions());
  dst->positions().copy_from(src.evaluated_positions());
  src.interpolate_to_evaluated(src.radii()).materialize(dst->radii());
  src.interpolate_to_evaluated(src.tilts()).materialize(dst->tilts());

  src.attributes.foreach_attribute(
      [&](const AttributeIDRef &attribute_id, const AttributeMetaData &meta_data) {
        std::optional<GSpan> src_attribute = src.attributes.get_for_read(attribute_id);
        if (dst->attributes.create(attribute_id, meta_data.data_type)) {
          std::optional<GMutableSpan> dst_attribute = dst->attributes.get_for_write(attribute_id);
          if (dst_attribute) {
            src.interpolate_to_evaluated(*src_attribute).materialize(dst_attribute->data());
            return true;
          }
        }

        BLI_assert_unreachable();
        return true;
      },
      ATTR_DOMAIN_POINT);

  return dst;
}

static std::unique_ptr<CurveEval> resample_curve(const CurveComponent *component,
                                                 const SampleModeParam &mode_param)
{
  const CurveEval *input_curve = component->get_for_read();
  GeometryComponentFieldContext field_context{*component, ATTR_DOMAIN_CURVE};
  const int domain_size = component->attribute_domain_size(ATTR_DOMAIN_CURVE);

  Span<SplinePtr> input_splines = input_curve->splines();

  std::unique_ptr<CurveEval> output_curve = std::make_unique<CurveEval>();
  output_curve->resize(input_splines.size());
  MutableSpan<SplinePtr> output_splines = output_curve->splines();

  if (mode_param.mode == GEO_NODE_CURVE_RESAMPLE_COUNT) {
    fn::FieldEvaluator evaluator{field_context, domain_size};
    evaluator.add(*mode_param.count);
    evaluator.add(mode_param.selection);
    evaluator.evaluate();
    const VArray<int> &cuts = evaluator.get_evaluated<int>(0);
    const VArray<bool> &selections = evaluator.get_evaluated<bool>(1);

    threading::parallel_for(input_splines.index_range(), 128, [&](IndexRange range) {
      for (const int i : range) {
        BLI_assert(mode_param.count);
        if (selections[i] && input_splines[i]->evaluated_points_size() > 0) {
          output_splines[i] = resample_spline(*input_splines[i], std::max(cuts[i], 1));
        }
        else {
          output_splines[i] = input_splines[i]->copy();
        }
      }
    });
  }
  else if (mode_param.mode == GEO_NODE_CURVE_RESAMPLE_LENGTH) {
    fn::FieldEvaluator evaluator{field_context, domain_size};
    evaluator.add(*mode_param.length);
    evaluator.add(mode_param.selection);
    evaluator.evaluate();
    const VArray<float> &lengths = evaluator.get_evaluated<float>(0);
    const VArray<bool> &selections = evaluator.get_evaluated<bool>(1);

    threading::parallel_for(input_splines.index_range(), 128, [&](IndexRange range) {
      for (const int i : range) {
        if (selections[i] && input_splines[i]->evaluated_points_size() > 0) {
          /* Don't allow asymptotic count increase for low resolution values. */
          const float divide_length = std::max(lengths[i], 0.0001f);
          const float spline_length = input_splines[i]->length();
          const int count = std::max(int(spline_length / divide_length) + 1, 1);
          output_splines[i] = resample_spline(*input_splines[i], count);
        }
        else {
          output_splines[i] = input_splines[i]->copy();
        }
      }
    });
  }
  else if (mode_param.mode == GEO_NODE_CURVE_RESAMPLE_EVALUATED) {
    fn::FieldEvaluator evaluator{field_context, domain_size};
    evaluator.add(mode_param.selection);
    evaluator.evaluate();
    const VArray<bool> &selections = evaluator.get_evaluated<bool>(0);

    threading::parallel_for(input_splines.index_range(), 128, [&](IndexRange range) {
      for (const int i : range) {
        if (selections[i] && input_splines[i]->evaluated_points_size() > 0) {
          output_splines[i] = resample_spline_evaluated(*input_splines[i]);
        }
        else {
          output_splines[i] = input_splines[i]->copy();
        }
      }
    });
  }
  output_curve->attributes = input_curve->attributes;
  return output_curve;
}

static void geometry_set_curve_resample(GeometrySet &geometry_set,
                                        const SampleModeParam &mode_param)
{
  if (!geometry_set.has_curve()) {
    return;
  }

  std::unique_ptr<CurveEval> output_curve = resample_curve(
      geometry_set.get_component_for_read<CurveComponent>(), mode_param);

  geometry_set.replace_curve(output_curve.release());
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");

  const NodeGeometryCurveResample &storage = node_storage(params.node());
  const GeometryNodeCurveResampleMode mode = (GeometryNodeCurveResampleMode)storage.mode;

  SampleModeParam mode_param;
  mode_param.mode = mode;
  mode_param.selection = params.extract_input<Field<bool>>("Selection");

  if (mode == GEO_NODE_CURVE_RESAMPLE_COUNT) {
    Field<int> count = params.extract_input<Field<int>>("Count");
    if (count < 1) {
      params.set_default_remaining_outputs();
      return;
    }
    mode_param.count.emplace(count);
  }
  else if (mode == GEO_NODE_CURVE_RESAMPLE_LENGTH) {
    Field<float> resolution = params.extract_input<Field<float>>("Length");
    mode_param.length.emplace(resolution);
  }

  geometry_set.modify_geometry_sets(
      [&](GeometrySet &geometry_set) { geometry_set_curve_resample(geometry_set, mode_param); });

  params.set_output("Curve", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_curve_resample_cc

void register_node_type_geo_curve_resample()
{
  namespace file_ns = blender::nodes::node_geo_curve_resample_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_RESAMPLE_CURVE, "Resample Curve", NODE_CLASS_GEOMETRY, 0);
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_layout;
  node_type_storage(
      &ntype, "NodeGeometryCurveResample", node_free_standard_storage, node_copy_standard_storage);
  node_type_init(&ntype, file_ns::node_init);
  node_type_update(&ntype, file_ns::node_update);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
