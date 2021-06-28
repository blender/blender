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

using blender::fn::GVArray_For_GSpan;
using blender::fn::GVArray_For_Span;
using blender::fn::GVArray_Typed;

static bNodeSocketTemplate geo_node_curve_resample_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_INT, N_("Count"), 10, 0, 0, 0, 1, 100000},
    {SOCK_FLOAT, N_("Length"), 0.1f, 0.0f, 0.0f, 0.0f, 0.001f, FLT_MAX, PROP_DISTANCE},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_curve_resample_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

static void geo_node_curve_resample_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
}

static void geo_node_curve_resample_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryCurveResample *data = (NodeGeometryCurveResample *)MEM_callocN(
      sizeof(NodeGeometryCurveResample), __func__);

  data->mode = GEO_NODE_CURVE_SAMPLE_COUNT;
  node->storage = data;
}

static void geo_node_curve_resample_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryCurveResample &node_storage = *(NodeGeometryCurveResample *)node->storage;
  const GeometryNodeCurveSampleMode mode = (GeometryNodeCurveSampleMode)node_storage.mode;

  bNodeSocket *count_socket = ((bNodeSocket *)node->inputs.first)->next;
  bNodeSocket *length_socket = count_socket->next;

  nodeSetSocketAvailability(count_socket, mode == GEO_NODE_CURVE_SAMPLE_COUNT);
  nodeSetSocketAvailability(length_socket, mode == GEO_NODE_CURVE_SAMPLE_LENGTH);
}

namespace blender::nodes {

struct SampleModeParam {
  GeometryNodeCurveSampleMode mode;
  std::optional<float> length;
  std::optional<int> count;
};

static SplinePtr resample_spline(const Spline &input_spline, const int count)
{
  std::unique_ptr<PolySpline> output_spline = std::make_unique<PolySpline>();
  output_spline->set_cyclic(input_spline.is_cyclic());
  output_spline->normal_mode = input_spline.normal_mode;

  if (input_spline.evaluated_edges_size() < 1 || count == 1) {
    output_spline->add_point(input_spline.positions().first(),
                             input_spline.tilts().first(),
                             input_spline.radii().first());
    output_spline->attributes.reallocate(1);
    return output_spline;
  }

  output_spline->resize(count);

  Array<float> uniform_samples = input_spline.sample_uniform_index_factors(count);

  input_spline.sample_with_index_factors<float3>(
      input_spline.evaluated_positions(), uniform_samples, output_spline->positions());

  input_spline.sample_with_index_factors<float>(
      input_spline.interpolate_to_evaluated(input_spline.radii()),
      uniform_samples,
      output_spline->radii());

  input_spline.sample_with_index_factors<float>(
      input_spline.interpolate_to_evaluated(input_spline.tilts()),
      uniform_samples,
      output_spline->tilts());

  output_spline->attributes.reallocate(count);
  input_spline.attributes.foreach_attribute(
      [&](StringRefNull name, const AttributeMetaData &meta_data) {
        std::optional<GSpan> input_attribute = input_spline.attributes.get_for_read(name);
        BLI_assert(input_attribute);
        if (!output_spline->attributes.create(name, meta_data.data_type)) {
          BLI_assert_unreachable();
          return false;
        }
        std::optional<GMutableSpan> output_attribute = output_spline->attributes.get_for_write(
            name);
        if (!output_attribute) {
          BLI_assert_unreachable();
          return false;
        }

        input_spline.sample_with_index_factors(
            *input_spline.interpolate_to_evaluated(*input_attribute),
            uniform_samples,
            *output_attribute);

        return true;
      },
      ATTR_DOMAIN_POINT);

  return output_spline;
}

static std::unique_ptr<CurveEval> resample_curve(const CurveEval &input_curve,
                                                 const SampleModeParam &mode_param)
{
  Span<SplinePtr> input_splines = input_curve.splines();

  std::unique_ptr<CurveEval> output_curve = std::make_unique<CurveEval>();
  output_curve->resize(input_splines.size());
  MutableSpan<SplinePtr> output_splines = output_curve->splines();

  if (mode_param.mode == GEO_NODE_CURVE_SAMPLE_COUNT) {
    threading::parallel_for(input_splines.index_range(), 128, [&](IndexRange range) {
      for (const int i : range) {
        BLI_assert(mode_param.count);
        output_splines[i] = resample_spline(*input_splines[i], *mode_param.count);
      }
    });
  }
  else if (mode_param.mode == GEO_NODE_CURVE_SAMPLE_LENGTH) {
    threading::parallel_for(input_splines.index_range(), 128, [&](IndexRange range) {
      for (const int i : range) {
        const float length = input_splines[i]->length();
        const int count = std::max(int(length / *mode_param.length), 1);
        output_splines[i] = resample_spline(*input_splines[i], count);
      }
    });
  }

  output_curve->attributes = input_curve.attributes;

  return output_curve;
}

static void geo_node_resample_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  geometry_set = bke::geometry_set_realize_instances(geometry_set);

  if (!geometry_set.has_curve()) {
    params.set_output("Geometry", GeometrySet());
    return;
  }

  const CurveEval &input_curve = *geometry_set.get_curve_for_read();
  NodeGeometryCurveResample &node_storage = *(NodeGeometryCurveResample *)params.node().storage;
  const GeometryNodeCurveSampleMode mode = (GeometryNodeCurveSampleMode)node_storage.mode;
  SampleModeParam mode_param;
  mode_param.mode = mode;
  if (mode == GEO_NODE_CURVE_SAMPLE_COUNT) {
    const int count = params.extract_input<int>("Count");
    if (count < 1) {
      params.set_output("Geometry", GeometrySet());
      return;
    }
    mode_param.count.emplace(count);
  }
  else if (mode == GEO_NODE_CURVE_SAMPLE_LENGTH) {
    /* Don't allow asymptotic count increase for low resolution values. */
    const float resolution = std::max(params.extract_input<float>("Length"), 0.0001f);
    mode_param.length.emplace(resolution);
  }

  std::unique_ptr<CurveEval> output_curve = resample_curve(input_curve, mode_param);

  params.set_output("Geometry", GeometrySet::create_with_curve(output_curve.release()));
}

}  // namespace blender::nodes

void register_node_type_geo_curve_resample()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_CURVE_RESAMPLE, "Resample Curve", NODE_CLASS_GEOMETRY, 0);
  node_type_socket_templates(&ntype, geo_node_curve_resample_in, geo_node_curve_resample_out);
  ntype.draw_buttons = geo_node_curve_resample_layout;
  node_type_storage(
      &ntype, "NodeGeometryCurveResample", node_free_standard_storage, node_copy_standard_storage);
  node_type_init(&ntype, geo_node_curve_resample_init);
  node_type_update(&ntype, geo_node_curve_resample_update);
  ntype.geometry_node_execute = blender::nodes::geo_node_resample_exec;
  nodeRegisterType(&ntype);
}
