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

namespace blender::nodes::node_geo_curve_subdivide_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Curve")).supported_type(GEO_COMPONENT_TYPE_CURVE);
  b.add_input<decl::Int>(N_("Cuts"))
      .default_value(1)
      .min(0)
      .max(1000)
      .supports_field()
      .description(N_("The number of control points to create on the segment following each point"));
  b.add_output<decl::Geometry>(N_("Curve"));
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
      const float factor_delta = cuts == 0 ? 1.0f : 1.0f / cuts;
      for (const int cut : IndexRange(cuts)) {
        const float factor = cut * factor_delta;
        dst[offsets[i] + cut] = attribute_math::mix2(factor, src[i], src[i + 1]);
      }
    }
  });

  if (is_cyclic) {
    const int i = src_size - 1;
    const int cuts = offsets[i + 1] - offsets[i];
    dst[offsets[i]] = src.last();
    const float factor_delta = cuts == 0 ? 1.0f : 1.0f / cuts;
    for (const int cut : IndexRange(cuts)) {
      const float factor = cut * factor_delta;
      dst[offsets[i] + cut] = attribute_math::mix2(factor, src.last(), src.first());
    }
  }
  else {
    dst.last() = src.last();
  }
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

  /* The first point in the segment is always copied. */
  dst_positions[offset] = src_positions[index];

  if (src.segment_is_vector(index)) {
    if (is_last_cyclic_segment) {
      dst_type_left.first() = BezierSpline::HandleType::Vector;
    }
    dst_type_left.slice(offset + 1, result_size).fill(BezierSpline::HandleType::Vector);
    dst_type_right.slice(offset, result_size).fill(BezierSpline::HandleType::Vector);

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

    /* Create a Bezier segment to update iteratively for every subdivision
     * and references to the meaningful values for ease of use. */
    BezierSpline temp;
    temp.resize(2);
    float3 &segment_start = temp.positions().first();
    float3 &segment_end = temp.positions().last();
    float3 &handle_prev = temp.handle_positions_right().first();
    float3 &handle_next = temp.handle_positions_left().last();
    segment_start = src_positions[index];
    segment_end = src_positions[next_index];
    handle_prev = src_handles_right[index];
    handle_next = src_handles_left[next_index];

    for (const int cut : IndexRange(result_size - 1)) {
      const float parameter = 1.0f / (result_size - cut);
      const BezierSpline::InsertResult insert = temp.calculate_segment_insertion(0, 1, parameter);

      /* Copy relevant temporary data to the result. */
      dst_handles_right[offset + cut] = insert.handle_prev;
      dst_handles_left[offset + cut + 1] = insert.left_handle;
      dst_positions[offset + cut + 1] = insert.position;

      /* Update the segment to prepare it for the next subdivision. */
      segment_start = insert.position;
      handle_prev = insert.right_handle;
      handle_next = insert.handle_next;
    }

    /* Copy the handles for the last segment from the temporary spline. */
    dst_handles_right[offset + result_size - 1] = handle_prev;
    dst_handles_left[i_segment_last] = handle_next;
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
      [&](const bke::AttributeIDRef &attribute_id, const AttributeMetaData &meta_data) {
        std::optional<GSpan> src = src_spline.attributes.get_for_read(attribute_id);
        BLI_assert(src);

        if (!dst_spline.attributes.create(attribute_id, meta_data.data_type)) {
          /* Since the source spline of the same type had the attribute, adding it should work. */
          BLI_assert_unreachable();
        }

        std::optional<GMutableSpan> dst = dst_spline.attributes.get_for_write(attribute_id);
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
  if (spline.size() <= 1) {
    return spline.copy();
  }

  /* Since we expect to access each value many times, it should be worth it to make sure count
   * of cuts is a real span (especially considering the note below). Using the offset at each
   * point facilitates subdividing in parallel later. */
  Array<int> offsets = get_subdivided_offsets(spline, cuts, spline_offset);
  const int result_size = offsets.last() + int(!spline.is_cyclic());
  SplinePtr new_spline = spline.copy_only_settings();
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

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");
  Field<int> cuts_field = params.extract_input<Field<int>>("Cuts");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (!geometry_set.has_curve()) {
      return;
    }

    const CurveComponent &component = *geometry_set.get_component_for_read<CurveComponent>();
    GeometryComponentFieldContext field_context{component, ATTR_DOMAIN_POINT};
    const int domain_size = component.attribute_domain_size(ATTR_DOMAIN_POINT);

    fn::FieldEvaluator evaluator{field_context, domain_size};
    evaluator.add(cuts_field);
    evaluator.evaluate();
    const VArray<int> &cuts = evaluator.get_evaluated<int>(0);

    if (cuts.is_single() && cuts.get_internal_single() < 1) {
      return;
    }

    std::unique_ptr<CurveEval> output_curve = subdivide_curve(*component.get_for_read(), cuts);
    geometry_set.replace_curve(output_curve.release());
  });
  params.set_output("Curve", geometry_set);
}

}  // namespace blender::nodes::node_geo_curve_subdivide_cc

void register_node_type_geo_curve_subdivide()
{
  namespace file_ns = blender::nodes::node_geo_curve_subdivide_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SUBDIVIDE_CURVE, "Subdivide Curve", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
