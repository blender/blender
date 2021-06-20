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

static bNodeSocketTemplate geo_node_curve_subdivide_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_STRING, N_("Cuts")},
    {SOCK_INT, N_("Cuts"), 1, 0, 0, 0, 0, 1000},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_curve_subdivide_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

static void geo_node_curve_subdivide_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "cuts_type", 0, IFACE_("Cuts"), ICON_NONE);
}

static void geo_node_curve_subdivide_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryCurveSubdivide *data = (NodeGeometryCurveSubdivide *)MEM_callocN(
      sizeof(NodeGeometryCurveSubdivide), __func__);

  data->cuts_type = GEO_NODE_ATTRIBUTE_INPUT_INTEGER;
  node->storage = data;
}

namespace blender::nodes {

static void geo_node_curve_subdivide_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryPointTranslate &node_storage = *(NodeGeometryPointTranslate *)node->storage;

  update_attribute_input_socket_availabilities(
      *node, "Cuts", (GeometryNodeAttributeInputMode)node_storage.input_type);
}

static Array<int> get_subdivided_offsets(const Spline &spline,
                                         const VArray<int> &cuts,
                                         const int spline_offset)
{
  Array<int> offsets(spline.segments_size() + 1);
  int offset = 0;
  for (const int i : IndexRange(spline.segments_size())) {
    offsets[i] = offset;
    offset = offset + std::max(cuts[spline_offset + i], 0) + 1;
  }
  offsets.last() = offset;
  return offsets;
}

template<typename T>
static void subdivide_attribute(Span<T> src,
                                const Span<int> offsets,
                                const bool is_cyclic,
                                MutableSpan<T> dst)
{
  const int src_size = src.size();
  threading::parallel_for(IndexRange(src_size - 1), 1024, [&](IndexRange range) {
    for (const int i : range) {
      const int cuts = offsets[i + 1] - offsets[i];
      dst[offsets[i]] = src[i];
      const float factor_delta = 1.0f / (cuts + 1.0f);
      for (const int cut : IndexRange(cuts)) {
        const float factor = (cut + 1) * factor_delta;
        dst[offsets[i] + cut] = attribute_math::mix2(factor, src[i], src[i + 1]);
      }
    }
  });

  if (is_cyclic) {
    const int i = src_size - 1;
    const int cuts = offsets[i + 1] - offsets[i];
    dst[offsets[i]] = src.last();
    const float factor_delta = 1.0f / (cuts + 1.0f);
    for (const int cut : IndexRange(cuts)) {
      const float factor = (cut + 1) * factor_delta;
      dst[offsets[i] + cut] = attribute_math::mix2(factor, src.last(), src.first());
    }
  }
  else {
    dst.last() = src.last();
  }
}

/**
 * De Casteljau Bezier subdivision.
 *
 * <pre>
 *           handle_prev         handle_next
 *                O----------------O
 *               /                  \
 *              /      x---O---x     \
 *             /         new_*        \
 *            /                        \
 *           O                          O
 *       point_prev                  point_next
 * </pre>
 */
static void calculate_new_bezier_point(const float3 &point_prev,
                                       float3 &handle_prev,
                                       float3 &new_left_handle,
                                       float3 &new_position,
                                       float3 &new_right_handle,
                                       float3 &handle_next,
                                       const float3 &point_next,
                                       const float parameter)
{
  const float3 center_point = float3::interpolate(handle_prev, handle_next, parameter);

  handle_prev = float3::interpolate(point_prev, handle_prev, parameter);
  handle_next = float3::interpolate(handle_next, point_next, parameter);
  new_left_handle = float3::interpolate(handle_prev, center_point, parameter);
  new_right_handle = float3::interpolate(center_point, handle_next, parameter);
  new_position = float3::interpolate(new_left_handle, new_right_handle, parameter);
}

/**
 * In order to generate a Bezier spline with the same shape as the input spline, apply the
 * De Casteljau algorithm iteratively for the provided number of cuts, constantly updating the
 * previous result point's right handle and the left handle at the end of the segment.
 *
 * \note Non-vector segments in the result spline are given free handles. This could possibly be
 * improved with another pass that sets handles to aligned where possible, but currently that does
 * not provide much benefit for the increased complexity.
 */
static void subdivide_bezier_segment(const BezierSpline &src,
                                     const int index,
                                     const int offset,
                                     const int result_size,
                                     Span<float3> src_positions,
                                     Span<float3> src_handles_left,
                                     Span<float3> src_handles_right,
                                     MutableSpan<float3> dst_positions,
                                     MutableSpan<float3> dst_handles_left,
                                     MutableSpan<float3> dst_handles_right,
                                     MutableSpan<BezierSpline::HandleType> dst_type_left,
                                     MutableSpan<BezierSpline::HandleType> dst_type_right)
{
  const bool is_last_cyclic_segment = index == (src.size() - 1);
  const int next_index = is_last_cyclic_segment ? 0 : index + 1;
  if (src.segment_is_vector(index)) {
    if (is_last_cyclic_segment) {
      dst_type_left.first() = BezierSpline::HandleType::Vector;
    }
    dst_type_left.slice(offset + 1, result_size).fill(BezierSpline::HandleType::Vector);
    dst_type_right.slice(offset, result_size).fill(BezierSpline::HandleType::Vector);

    dst_positions[offset] = src_positions[index];
    const float factor_delta = 1.0f / result_size;
    for (const int cut : IndexRange(result_size)) {
      const float factor = cut * factor_delta;
      dst_positions[offset + cut] = attribute_math::mix2(
          factor, src_positions[index], src_positions[next_index]);
    }
  }
  else {
    if (is_last_cyclic_segment) {
      dst_type_left.first() = BezierSpline::HandleType::Free;
    }
    dst_type_left.slice(offset + 1, result_size).fill(BezierSpline::HandleType::Free);
    dst_type_right.slice(offset, result_size).fill(BezierSpline::HandleType::Free);

    const int i_segment_last = is_last_cyclic_segment ? 0 : offset + result_size;
    dst_positions[offset] = src_positions[index];
    dst_handles_right[offset] = src_handles_right[index];
    dst_handles_left[i_segment_last] = src_handles_left[next_index];

    for (const int cut : IndexRange(result_size - 1)) {
      const float parameter = 1.0f / (result_size - cut);
      calculate_new_bezier_point(dst_positions[offset + cut],
                                 dst_handles_right[offset + cut],
                                 dst_handles_left[offset + cut + 1],
                                 dst_positions[offset + cut + 1],
                                 dst_handles_right[offset + cut + 1],
                                 dst_handles_left[i_segment_last],
                                 src_positions[next_index],
                                 parameter);
    }
  }
}

static void subdivide_bezier_spline(const BezierSpline &src,
                                    const Span<int> offsets,
                                    BezierSpline &dst)
{
  Span<float3> src_positions = src.positions();
  Span<float3> src_handles_left = src.handle_positions_left();
  Span<float3> src_handles_right = src.handle_positions_right();
  MutableSpan<float3> dst_positions = dst.positions();
  MutableSpan<float3> dst_handles_left = dst.handle_positions_left();
  MutableSpan<float3> dst_handles_right = dst.handle_positions_right();
  MutableSpan<BezierSpline::HandleType> dst_type_left = dst.handle_types_left();
  MutableSpan<BezierSpline::HandleType> dst_type_right = dst.handle_types_right();

  threading::parallel_for(IndexRange(src.size() - 1), 512, [&](IndexRange range) {
    for (const int i : range) {
      subdivide_bezier_segment(src,
                               i,
                               offsets[i],
                               offsets[i + 1] - offsets[i],
                               src_positions,
                               src_handles_left,
                               src_handles_right,
                               dst_positions,
                               dst_handles_left,
                               dst_handles_right,
                               dst_type_left,
                               dst_type_right);
    }
  });

  if (src.is_cyclic()) {
    const int i_last = src.size() - 1;
    subdivide_bezier_segment(src,
                             i_last,
                             offsets[i_last],
                             offsets.last() - offsets[i_last],
                             src_positions,
                             src_handles_left,
                             src_handles_right,
                             dst_positions,
                             dst_handles_left,
                             dst_handles_right,
                             dst_type_left,
                             dst_type_right);
  }
  else {
    dst_positions.last() = src_positions.last();
  }
}

static void subdivide_builtin_attributes(const Spline &src_spline,
                                         const Span<int> offsets,
                                         Spline &dst_spline)
{
  const bool is_cyclic = src_spline.is_cyclic();
  subdivide_attribute<float>(src_spline.radii(), offsets, is_cyclic, dst_spline.radii());
  subdivide_attribute<float>(src_spline.tilts(), offsets, is_cyclic, dst_spline.tilts());
  switch (src_spline.type()) {
    case Spline::Type::Poly: {
      const PolySpline &src = static_cast<const PolySpline &>(src_spline);
      PolySpline &dst = static_cast<PolySpline &>(dst_spline);
      subdivide_attribute<float3>(src.positions(), offsets, is_cyclic, dst.positions());
      break;
    }
    case Spline::Type::Bezier: {
      const BezierSpline &src = static_cast<const BezierSpline &>(src_spline);
      BezierSpline &dst = static_cast<BezierSpline &>(dst_spline);
      subdivide_bezier_spline(src, offsets, dst);
      dst.mark_cache_invalid();
      break;
    }
    case Spline::Type::NURBS: {
      const NURBSpline &src = static_cast<const NURBSpline &>(src_spline);
      NURBSpline &dst = static_cast<NURBSpline &>(dst_spline);
      subdivide_attribute<float3>(src.positions(), offsets, is_cyclic, dst.positions());
      subdivide_attribute<float>(src.weights(), offsets, is_cyclic, dst.weights());
      break;
    }
  }
}

static void subdivide_dynamic_attributes(const Spline &src_spline,
                                         const Span<int> offsets,
                                         Spline &dst_spline)
{
  const bool is_cyclic = src_spline.is_cyclic();
  src_spline.attributes.foreach_attribute(
      [&](StringRefNull name, const AttributeMetaData &meta_data) {
        std::optional<GSpan> src = src_spline.attributes.get_for_read(name);
        BLI_assert(src);

        if (!dst_spline.attributes.create(name, meta_data.data_type)) {
          /* Since the source spline of the same type had the attribute, adding it should work. */
          BLI_assert_unreachable();
        }

        std::optional<GMutableSpan> dst = dst_spline.attributes.get_for_write(name);
        BLI_assert(dst);

        attribute_math::convert_to_static_type(dst->type(), [&](auto dummy) {
          using T = decltype(dummy);
          subdivide_attribute<T>(src->typed<T>(), offsets, is_cyclic, dst->typed<T>());
        });
        return true;
      },
      ATTR_DOMAIN_POINT);
}

static SplinePtr subdivide_spline(const Spline &spline,
                                  const VArray<int> &cuts,
                                  const int spline_offset)
{
  /* Since we expect to access each value many times, it should be worth it to make sure the
   * attribute is a real span (especially considering the note below). Using the offset at each
   * point facilitates subdividing in parallel later. */
  Array<int> offsets = get_subdivided_offsets(spline, cuts, spline_offset);
  const int result_size = offsets.last() + int(!spline.is_cyclic());
  SplinePtr new_spline = spline.copy_settings();
  new_spline->resize(result_size);
  subdivide_builtin_attributes(spline, offsets, *new_spline);
  subdivide_dynamic_attributes(spline, offsets, *new_spline);
  return new_spline;
}

/**
 * \note Passing the virtual array for the entire spline is possibly quite inefficient here when
 * the attribute was on the point domain and stored separately for each spline already, and it
 * prevents some other optimizations like skipping splines with a single attribute value of < 1.
 * However, it allows the node to access builtin attribute easily, so it the makes most sense this
 * way until the attribute API is refactored.
 */
static std::unique_ptr<CurveEval> subdivide_curve(const CurveEval &input_curve,
                                                  const VArray<int> &cuts)
{
  const Array<int> control_point_offsets = input_curve.control_point_offsets();
  const Span<SplinePtr> input_splines = input_curve.splines();

  std::unique_ptr<CurveEval> output_curve = std::make_unique<CurveEval>();
  output_curve->resize(input_splines.size());
  output_curve->attributes = input_curve.attributes;
  MutableSpan<SplinePtr> output_splines = output_curve->splines();

  threading::parallel_for(input_splines.index_range(), 128, [&](IndexRange range) {
    for (const int i : range) {
      output_splines[i] = subdivide_spline(*input_splines[i], cuts, control_point_offsets[i]);
    }
  });

  return output_curve;
}

static void geo_node_subdivide_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  geometry_set = bke::geometry_set_realize_instances(geometry_set);

  if (!geometry_set.has_curve()) {
    params.set_output("Geometry", geometry_set);
    return;
  }

  const CurveComponent &component = *geometry_set.get_component_for_read<CurveComponent>();
  GVArray_Typed<int> cuts = params.get_input_attribute<int>(
      "Cuts", component, ATTR_DOMAIN_POINT, 0);
  if (cuts->is_single() && cuts->get_internal_single() < 1) {
    params.set_output("Geometry", geometry_set);
    return;
  }

  std::unique_ptr<CurveEval> output_curve = subdivide_curve(*component.get_for_read(), *cuts);

  params.set_output("Geometry", GeometrySet::create_with_curve(output_curve.release()));
}

}  // namespace blender::nodes

void register_node_type_geo_curve_subdivide()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_CURVE_SUBDIVIDE, "Curve Subdivide", NODE_CLASS_GEOMETRY, 0);
  node_type_socket_templates(&ntype, geo_node_curve_subdivide_in, geo_node_curve_subdivide_out);
  ntype.draw_buttons = geo_node_curve_subdivide_layout;
  node_type_storage(&ntype,
                    "NodeGeometryCurveSubdivide",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  node_type_init(&ntype, geo_node_curve_subdivide_init);
  node_type_update(&ntype, blender::nodes::geo_node_curve_subdivide_update);
  ntype.geometry_node_execute = blender::nodes::geo_node_subdivide_exec;
  nodeRegisterType(&ntype);
}
