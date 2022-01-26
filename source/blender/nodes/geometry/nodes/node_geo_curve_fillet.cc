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

#include "UI_interface.h"
#include "UI_resources.h"

#include "DNA_node_types.h"

#include "node_geometry_util.hh"

#include "BKE_spline.hh"

namespace blender::nodes::node_geo_curve_fillet_cc {

NODE_STORAGE_FUNCS(NodeGeometryCurveFillet)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Curve")).supported_type(GEO_COMPONENT_TYPE_CURVE);
  b.add_input<decl::Int>(N_("Count"))
      .default_value(1)
      .min(1)
      .max(1000)
      .supports_field()
      .make_available([](bNode &node) { node_storage(node).mode = GEO_NODE_CURVE_FILLET_POLY; });
  b.add_input<decl::Float>(N_("Radius"))
      .min(0.0f)
      .max(FLT_MAX)
      .subtype(PropertySubType::PROP_DISTANCE)
      .default_value(0.25f)
      .supports_field();
  b.add_input<decl::Bool>(N_("Limit Radius"))
      .description(
          N_("Limit the maximum value of the radius in order to avoid overlapping fillets"));
  b.add_output<decl::Geometry>(N_("Curve"));
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
}

static void node_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryCurveFillet *data = MEM_cnew<NodeGeometryCurveFillet>(__func__);

  data->mode = GEO_NODE_CURVE_FILLET_BEZIER;
  node->storage = data;
}

struct FilletParam {
  GeometryNodeCurveFilletMode mode;

  /* Number of points to be added. */
  VArray<int> counts;

  /* Radii for fillet arc at all vertices. */
  VArray<float> radii;

  /* Whether or not fillets are allowed to overlap. */
  bool limit_radius;
};

/* A data structure used to store fillet data about all vertices to be filleted. */
struct FilletData {
  Span<float3> positions;
  Array<float3> directions, axes;
  Array<float> radii, angles;
  Array<int> counts;
};

static void node_update(bNodeTree *ntree, bNode *node)
{
  const NodeGeometryCurveFillet &storage = node_storage(*node);
  const GeometryNodeCurveFilletMode mode = (GeometryNodeCurveFilletMode)storage.mode;

  bNodeSocket *poly_socket = ((bNodeSocket *)node->inputs.first)->next;

  nodeSetSocketAvailability(ntree, poly_socket, mode == GEO_NODE_CURVE_FILLET_POLY);
}

/* Function to get the center of a fillet. */
static float3 get_center(const float3 vec_pos2prev,
                         const float3 pos,
                         const float3 axis,
                         const float angle)
{
  float3 vec_pos2center;
  rotate_normalized_v3_v3v3fl(vec_pos2center, vec_pos2prev, axis, M_PI_2 - angle / 2.0f);
  vec_pos2center *= 1.0f / sinf(angle / 2.0f);

  return vec_pos2center + pos;
}

/* Function to get the center of the fillet using fillet data */
static float3 get_center(const float3 vec_pos2prev, const FilletData &fd, const int index)
{
  const float angle = fd.angles[index];
  const float3 axis = fd.axes[index];
  const float3 pos = fd.positions[index];

  return get_center(vec_pos2prev, pos, axis, angle);
}

/* Calculate the direction vectors from each vertex to their previous vertex. */
static Array<float3> calculate_directions(const Span<float3> positions)
{
  const int size = positions.size();
  Array<float3> directions(size);

  for (const int i : IndexRange(size - 1)) {
    directions[i] = math::normalize(positions[i + 1] - positions[i]);
  }
  directions[size - 1] = math::normalize(positions[0] - positions[size - 1]);

  return directions;
}

/* Calculate the axes around which the fillet is built. */
static Array<float3> calculate_axes(const Span<float3> directions)
{
  const int size = directions.size();
  Array<float3> axes(size);

  axes[0] = math::normalize(math::cross(-directions[size - 1], directions[0]));
  for (const int i : IndexRange(1, size - 1)) {
    axes[i] = math::normalize(math::cross(-directions[i - 1], directions[i]));
  }

  return axes;
}

/* Calculate the angle of the arc formed by the fillet. */
static Array<float> calculate_angles(const Span<float3> directions)
{
  const int size = directions.size();
  Array<float> angles(size);

  angles[0] = M_PI - angle_v3v3(-directions[size - 1], directions[0]);
  for (const int i : IndexRange(1, size - 1)) {
    angles[i] = M_PI - angle_v3v3(-directions[i - 1], directions[i]);
  }

  return angles;
}

/* Calculate the segment count in each filleted arc. */
static Array<int> calculate_counts(const FilletParam &fillet_param,
                                   const int size,
                                   const int spline_offset,
                                   const bool cyclic)
{
  Array<int> counts(size, 1);
  if (fillet_param.mode == GEO_NODE_CURVE_FILLET_POLY) {
    for (const int i : IndexRange(size)) {
      counts[i] = fillet_param.counts[spline_offset + i];
    }
  }
  if (!cyclic) {
    counts[0] = counts[size - 1] = 0;
  }

  return counts;
}

/* Calculate the radii for the vertices to be filleted. */
static Array<float> calculate_radii(const FilletParam &fillet_param,
                                    const int size,
                                    const int spline_offset)
{
  Array<float> radii(size, 0.0f);
  if (fillet_param.limit_radius) {
    for (const int i : IndexRange(size)) {
      radii[i] = std::max(fillet_param.radii[spline_offset + i], 0.0f);
    }
  }
  else {
    for (const int i : IndexRange(size)) {
      radii[i] = fillet_param.radii[spline_offset + i];
    }
  }

  return radii;
}

/* Calculate the number of vertices added per vertex on the source spline. */
static int calculate_point_counts(MutableSpan<int> point_counts,
                                  const Span<float> radii,
                                  const Span<int> counts)
{
  int added_count = 0;
  for (const int i : IndexRange(point_counts.size())) {
    /* Calculate number of points to be added for the vertex. */
    if (radii[i] != 0.0f) {
      added_count += counts[i];
      point_counts[i] = counts[i] + 1;
    }
  }

  return added_count;
}

static FilletData calculate_fillet_data(const Spline &spline,
                                        const FilletParam &fillet_param,
                                        int &added_count,
                                        MutableSpan<int> point_counts,
                                        const int spline_offset)
{
  const int size = spline.size();

  FilletData fd;
  fd.directions = calculate_directions(spline.positions());
  fd.positions = spline.positions();
  fd.axes = calculate_axes(fd.directions);
  fd.angles = calculate_angles(fd.directions);
  fd.counts = calculate_counts(fillet_param, size, spline_offset, spline.is_cyclic());
  fd.radii = calculate_radii(fillet_param, size, spline_offset);

  added_count = calculate_point_counts(point_counts, fd.radii, fd.counts);

  return fd;
}

/* Limit the radius based on angle and radii to prevent overlapping. */
static void limit_radii(FilletData &fd, const bool cyclic)
{
  MutableSpan<float> radii(fd.radii);
  Span<float> angles(fd.angles);
  Span<float3> positions(fd.positions);

  const int size = radii.size();
  const int fillet_count = cyclic ? size : size - 2;
  const int start = cyclic ? 0 : 1;
  Array<float> max_radii(size, FLT_MAX);

  if (cyclic) {
    /* Calculate lengths between adjacent control points. */
    const float len_prev = math::distance(positions[0], positions[size - 1]);
    const float len_next = math::distance(positions[0], positions[1]);

    /* Calculate tangent lengths of fillets in control points. */
    const float tan_len = radii[0] * tan(angles[0] / 2.0f);
    const float tan_len_prev = radii[size - 1] * tan(angles[size - 1] / 2.0f);
    const float tan_len_next = radii[1] * tan(angles[1] / 2.0f);

    float factor_prev = 1.0f, factor_next = 1.0f;
    if (tan_len + tan_len_prev > len_prev) {
      factor_prev = len_prev / (tan_len + tan_len_prev);
    }
    if (tan_len + tan_len_next > len_next) {
      factor_next = len_next / (tan_len + tan_len_next);
    }

    /* Scale max radii by calculated factors. */
    max_radii[0] = radii[0] * std::min(factor_next, factor_prev);
    max_radii[1] = radii[1] * factor_next;
    max_radii[size - 1] = radii[size - 1] * factor_prev;
  }

  /* Initialize max_radii to largest possible radii. */
  float prev_dist = math::distance(positions[1], positions[0]);
  for (const int i : IndexRange(1, size - 2)) {
    const float temp_dist = math::distance(positions[i], positions[i + 1]);
    max_radii[i] = std::min(prev_dist, temp_dist) / tan(angles[i] / 2.0f);
    prev_dist = temp_dist;
  }

  /* Max radii calculations for each index. */
  for (const int i : IndexRange(start, fillet_count - 1)) {
    const float len_next = math::distance(positions[i], positions[i + 1]);
    const float tan_len = radii[i] * tan(angles[i] / 2.0f);
    const float tan_len_next = radii[i + 1] * tan(angles[i + 1] / 2.0f);

    /* Scale down radii if too large for segment. */
    float factor = 1.0f;
    if (tan_len + tan_len_next > len_next) {
      factor = len_next / (tan_len + tan_len_next);
    }
    max_radii[i] = std::min(max_radii[i], radii[i] * factor);
    max_radii[i + 1] = std::min(max_radii[i + 1], radii[i + 1] * factor);
  }

  /* Assign the max_radii to the fillet data's radii. */
  for (const int i : IndexRange(size)) {
    radii[i] = max_radii[i];
  }
}

/*
 * Create a mapping from each vertex in the destination spline to that of the source spline.
 * Used for copying the data from the source spline.
 */
static Array<int> create_dst_to_src_map(const Span<int> point_counts, const int total_points)
{
  Array<int> map(total_points);
  MutableSpan<int> map_span{map};
  int index = 0;

  for (const int i : point_counts.index_range()) {
    map_span.slice(index, point_counts[i]).fill(i);
    index += point_counts[i];
  }

  BLI_assert(index == total_points);

  return map;
}

template<typename T>
static void copy_attribute_by_mapping(const Span<T> src,
                                      MutableSpan<T> dst,
                                      const Span<int> mapping)
{
  for (const int i : dst.index_range()) {
    dst[i] = src[mapping[i]];
  }
}

/* Copy radii and tilts from source spline to destination. Positions are handled later in update
 * positions methods. */
static void copy_common_attributes_by_mapping(const Spline &src,
                                              Spline &dst,
                                              const Span<int> mapping)
{
  copy_attribute_by_mapping(src.radii(), dst.radii(), mapping);
  copy_attribute_by_mapping(src.tilts(), dst.tilts(), mapping);

  src.attributes.foreach_attribute(
      [&](const AttributeIDRef &attribute_id, const AttributeMetaData &meta_data) {
        std::optional<GSpan> src_attribute = src.attributes.get_for_read(attribute_id);
        if (dst.attributes.create(attribute_id, meta_data.data_type)) {
          std::optional<GMutableSpan> dst_attribute = dst.attributes.get_for_write(attribute_id);
          if (dst_attribute) {
            attribute_math::convert_to_static_type(dst_attribute->type(), [&](auto dummy) {
              using T = decltype(dummy);
              copy_attribute_by_mapping(
                  src_attribute->typed<T>(), dst_attribute->typed<T>(), mapping);
            });
            return true;
          }
        }
        BLI_assert_unreachable();
        return false;
      },
      ATTR_DOMAIN_POINT);
}

/* Update the vertex positions and handle positions of a Bezier spline based on fillet data. */
static void update_bezier_positions(const FilletData &fd,
                                    BezierSpline &dst_spline,
                                    const BezierSpline &src_spline,
                                    const Span<int> point_counts)
{
  Span<float> radii(fd.radii);
  Span<float> angles(fd.angles);
  Span<float3> axes(fd.axes);
  Span<float3> positions(fd.positions);
  Span<float3> directions(fd.directions);

  const int size = radii.size();

  int i_dst = 0;
  for (const int i_src : IndexRange(size)) {
    const int count = point_counts[i_src];

    /* Skip if the point count for the vertex is 1. */
    if (count == 1) {
      dst_spline.positions()[i_dst] = src_spline.positions()[i_src];
      dst_spline.handle_types_left()[i_dst] = src_spline.handle_types_left()[i_src];
      dst_spline.handle_types_right()[i_dst] = src_spline.handle_types_right()[i_src];
      dst_spline.handle_positions_left()[i_dst] = src_spline.handle_positions_left()[i_src];
      dst_spline.handle_positions_right()[i_dst] = src_spline.handle_positions_right()[i_src];
      i_dst++;
      continue;
    }

    /* Calculate the angle to be formed between any 2 adjacent vertices within the fillet. */
    const float segment_angle = angles[i_src] / (count - 1);
    /* Calculate the handle length for each added vertex. Equation: L = 4R/3 * tan(A/4) */
    const float handle_length = 4.0f * radii[i_src] / 3.0f * tan(segment_angle / 4.0f);
    /* Calculate the distance by which each vertex should be displaced from their initial position.
     */
    const float displacement = radii[i_src] * tan(angles[i_src] / 2.0f);

    /* Position the end points of the arc and their handles. */
    const int end_i = i_dst + count - 1;
    const float3 prev_dir = i_src == 0 ? -directions[size - 1] : -directions[i_src - 1];
    const float3 next_dir = directions[i_src];
    dst_spline.positions()[i_dst] = positions[i_src] + displacement * prev_dir;
    dst_spline.positions()[end_i] = positions[i_src] + displacement * next_dir;
    dst_spline.handle_positions_right()[i_dst] = dst_spline.positions()[i_dst] -
                                                 handle_length * prev_dir;
    dst_spline.handle_positions_left()[end_i] = dst_spline.positions()[end_i] -
                                                handle_length * next_dir;
    dst_spline.handle_types_right()[i_dst] = dst_spline.handle_types_left()[end_i] =
        BezierSpline::HandleType::Align;
    dst_spline.handle_types_left()[i_dst] = dst_spline.handle_types_right()[end_i] =
        BezierSpline::HandleType::Vector;
    dst_spline.mark_cache_invalid();

    /* Calculate the center of the radius to be formed. */
    const float3 center = get_center(dst_spline.positions()[i_dst] - positions[i_src], fd, i_src);
    /* Calculate the vector of the radius formed by the first vertex. */
    float3 radius_vec = dst_spline.positions()[i_dst] - center;
    float radius;
    radius_vec = math::normalize_and_get_length(radius_vec, radius);

    dst_spline.handle_types_right().slice(1, count - 2).fill(BezierSpline::HandleType::Align);
    dst_spline.handle_types_left().slice(1, count - 2).fill(BezierSpline::HandleType::Align);

    /* For each of the vertices in between the end points. */
    for (const int j : IndexRange(1, count - 2)) {
      int index = i_dst + j;
      /* Rotate the radius by the segment angle and determine its tangent (used for getting handle
       * directions). */
      float3 new_radius_vec, tangent_vec;
      rotate_normalized_v3_v3v3fl(new_radius_vec, radius_vec, -axes[i_src], segment_angle);
      rotate_normalized_v3_v3v3fl(tangent_vec, new_radius_vec, axes[i_src], M_PI_2);
      radius_vec = new_radius_vec;
      tangent_vec *= handle_length;

      /* Adjust the positions of the respective vertex and its handles. */
      dst_spline.positions()[index] = center + new_radius_vec * radius;
      dst_spline.handle_positions_left()[index] = dst_spline.positions()[index] + tangent_vec;
      dst_spline.handle_positions_right()[index] = dst_spline.positions()[index] - tangent_vec;
    }

    i_dst += count;
  }
}

/* Update the vertex positions of a Poly spline based on fillet data. */
static void update_poly_positions(const FilletData &fd,
                                  Spline &dst_spline,
                                  const Spline &src_spline,
                                  const Span<int> point_counts)
{
  Span<float> radii(fd.radii);
  Span<float> angles(fd.angles);
  Span<float3> axes(fd.axes);
  Span<float3> positions(fd.positions);
  Span<float3> directions(fd.directions);

  const int size = radii.size();

  int i_dst = 0;
  for (const int i_src : IndexRange(size)) {
    const int count = point_counts[i_src];

    /* Skip if the point count for the vertex is 1. */
    if (count == 1) {
      dst_spline.positions()[i_dst] = src_spline.positions()[i_src];
      i_dst++;
      continue;
    }

    const float segment_angle = angles[i_src] / (count - 1);
    const float displacement = radii[i_src] * tan(angles[i_src] / 2.0f);

    /* Position the end points of the arc. */
    const int end_i = i_dst + count - 1;
    const float3 prev_dir = i_src == 0 ? -directions[size - 1] : -directions[i_src - 1];
    const float3 next_dir = directions[i_src];
    dst_spline.positions()[i_dst] = positions[i_src] + displacement * prev_dir;
    dst_spline.positions()[end_i] = positions[i_src] + displacement * next_dir;

    /* Calculate the center of the radius to be formed. */
    const float3 center = get_center(dst_spline.positions()[i_dst] - positions[i_src], fd, i_src);
    /* Calculate the vector of the radius formed by the first vertex. */
    float3 radius_vec = dst_spline.positions()[i_dst] - center;

    for (const int j : IndexRange(1, count - 2)) {
      /* Rotate the radius by the segment angle */
      float3 new_radius_vec;
      rotate_normalized_v3_v3v3fl(new_radius_vec, radius_vec, -axes[i_src], segment_angle);
      radius_vec = new_radius_vec;

      dst_spline.positions()[i_dst + j] = center + new_radius_vec;
    }

    i_dst += count;
  }
}

static SplinePtr fillet_spline(const Spline &spline,
                               const FilletParam &fillet_param,
                               const int spline_offset)
{
  const int size = spline.size();
  const bool cyclic = spline.is_cyclic();

  if (size < 3) {
    return spline.copy();
  }

  /* Initialize the point_counts with 1s (at least one vertex on dst for each vertex on src). */
  Array<int> point_counts(size, 1);

  int added_count = 0;
  /* Update point_counts array and added_count. */
  FilletData fd = calculate_fillet_data(
      spline, fillet_param, added_count, point_counts, spline_offset);
  if (fillet_param.limit_radius) {
    limit_radii(fd, cyclic);
  }

  const int total_points = added_count + size;
  const Array<int> dst_to_src = create_dst_to_src_map(point_counts, total_points);
  SplinePtr dst_spline_ptr = spline.copy_only_settings();
  (*dst_spline_ptr).resize(total_points);
  copy_common_attributes_by_mapping(spline, *dst_spline_ptr, dst_to_src);

  switch (spline.type()) {
    case Spline::Type::Bezier: {
      const BezierSpline &src_spline = static_cast<const BezierSpline &>(spline);
      BezierSpline &dst_spline = static_cast<BezierSpline &>(*dst_spline_ptr);
      if (fillet_param.mode == GEO_NODE_CURVE_FILLET_POLY) {
        dst_spline.handle_types_left().fill(BezierSpline::HandleType::Vector);
        dst_spline.handle_types_right().fill(BezierSpline::HandleType::Vector);
        update_poly_positions(fd, dst_spline, src_spline, point_counts);
      }
      else {
        update_bezier_positions(fd, dst_spline, src_spline, point_counts);
      }
      break;
    }
    case Spline::Type::Poly: {
      update_poly_positions(fd, *dst_spline_ptr, spline, point_counts);
      break;
    }
    case Spline::Type::NURBS: {
      const NURBSpline &src_spline = static_cast<const NURBSpline &>(spline);
      NURBSpline &dst_spline = static_cast<NURBSpline &>(*dst_spline_ptr);
      copy_attribute_by_mapping(src_spline.weights(), dst_spline.weights(), dst_to_src);
      update_poly_positions(fd, dst_spline, src_spline, point_counts);
      break;
    }
  }

  return dst_spline_ptr;
}

static std::unique_ptr<CurveEval> fillet_curve(const CurveEval &input_curve,
                                               const FilletParam &fillet_param)
{
  Span<SplinePtr> input_splines = input_curve.splines();

  std::unique_ptr<CurveEval> output_curve = std::make_unique<CurveEval>();
  const int num_splines = input_splines.size();
  output_curve->resize(num_splines);
  MutableSpan<SplinePtr> output_splines = output_curve->splines();
  Array<int> spline_offsets = input_curve.control_point_offsets();

  threading::parallel_for(input_splines.index_range(), 128, [&](IndexRange range) {
    for (const int i : range) {
      output_splines[i] = fillet_spline(*input_splines[i], fillet_param, spline_offsets[i]);
    }
  });
  output_curve->attributes = input_curve.attributes;

  return output_curve;
}

static void calculate_curve_fillet(GeometrySet &geometry_set,
                                   const GeometryNodeCurveFilletMode mode,
                                   const Field<float> &radius_field,
                                   const std::optional<Field<int>> &count_field,
                                   const bool limit_radius)
{
  if (!geometry_set.has_curve()) {
    return;
  }

  FilletParam fillet_param;
  fillet_param.mode = mode;

  CurveComponent &component = geometry_set.get_component_for_write<CurveComponent>();
  GeometryComponentFieldContext field_context{component, ATTR_DOMAIN_POINT};
  const int domain_size = component.attribute_domain_size(ATTR_DOMAIN_POINT);
  fn::FieldEvaluator field_evaluator{field_context, domain_size};

  field_evaluator.add(radius_field);

  if (mode == GEO_NODE_CURVE_FILLET_POLY) {
    field_evaluator.add(*count_field);
  }

  field_evaluator.evaluate();

  fillet_param.radii = field_evaluator.get_evaluated<float>(0);
  if (fillet_param.radii.is_single() && fillet_param.radii.get_internal_single() < 0.0f) {
    return;
  }

  if (mode == GEO_NODE_CURVE_FILLET_POLY) {
    fillet_param.counts = field_evaluator.get_evaluated<int>(1);
  }

  fillet_param.limit_radius = limit_radius;

  const CurveEval &input_curve = *geometry_set.get_curve_for_read();
  std::unique_ptr<CurveEval> output_curve = fillet_curve(input_curve, fillet_param);

  geometry_set.replace_curve(output_curve.release());
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");

  const NodeGeometryCurveFillet &storage = node_storage(params.node());
  const GeometryNodeCurveFilletMode mode = (GeometryNodeCurveFilletMode)storage.mode;

  Field<float> radius_field = params.extract_input<Field<float>>("Radius");
  const bool limit_radius = params.extract_input<bool>("Limit Radius");

  std::optional<Field<int>> count_field;
  if (mode == GEO_NODE_CURVE_FILLET_POLY) {
    count_field.emplace(params.extract_input<Field<int>>("Count"));
  }

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    calculate_curve_fillet(geometry_set, mode, radius_field, count_field, limit_radius);
  });

  params.set_output("Curve", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_curve_fillet_cc

void register_node_type_geo_curve_fillet()
{
  namespace file_ns = blender::nodes::node_geo_curve_fillet_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_FILLET_CURVE, "Fillet Curve", NODE_CLASS_GEOMETRY);
  ntype.draw_buttons = file_ns::node_layout;
  node_type_storage(
      &ntype, "NodeGeometryCurveFillet", node_free_standard_storage, node_copy_standard_storage);
  ntype.declare = file_ns::node_declare;
  node_type_init(&ntype, file_ns::node_init);
  node_type_update(&ntype, file_ns::node_update);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
