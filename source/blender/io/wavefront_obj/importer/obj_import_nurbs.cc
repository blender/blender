/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#include "BKE_curve_legacy_convert.hh"
#include "BKE_curves.hh"
#include "BKE_lib_id.hh"
#include "BKE_object.hh"

#include "BLI_array_utils.hh"
#include "BLI_listbase.h"
#include "BLI_math_vector.h"

#include "DNA_curve_types.h"

#include "IO_wavefront_obj.hh"

#include "importer_mesh_utils.hh"
#include "obj_import_nurbs.hh"
#include "obj_import_objects.hh"

namespace blender::io::obj {

Curves *blender::io::obj::CurveFromGeometry::create_curve(const OBJImportParams &import_params)
{
  BLI_assert(!curve_geometry_.nurbs_element_.curv_indices.is_empty());

  Curves *curves_id = bke::curves_new_nomain(0, 0);
  bke::CurvesGeometry &curves = curves_id->geometry.wrap();
  this->create_nurbs(curves, import_params);
  return curves_id;
}

Object *CurveFromGeometry::create_curve_object(Main *bmain, const OBJImportParams &import_params)
{
  if (curve_geometry_.nurbs_element_.curv_indices.is_empty()) {
    return nullptr;
  }
  std::string ob_name = get_geometry_name(curve_geometry_.geometry_name_,
                                          import_params.collection_separator);
  if (ob_name.empty() && !curve_geometry_.nurbs_element_.group_.empty()) {
    ob_name = curve_geometry_.nurbs_element_.group_;
  }
  if (ob_name.empty()) {
    ob_name = "Untitled";
  }

  Curve *curve = BKE_curve_add(bmain, ob_name.c_str(), OB_CURVES_LEGACY);
  Object *obj = BKE_object_add_only_object(bmain, OB_CURVES_LEGACY, ob_name.c_str());

  curve->flag = CU_3D;
  curve->resolu = curve->resolv = 12;
  /* Only one NURBS spline will be created in the curve object. */
  curve->actnu = 0;

  Nurb *nurb = MEM_callocN<Nurb>(__func__);
  BLI_addtail(BKE_curve_nurbs_get(curve), nurb);
  this->create_nurbs(curve, import_params);

  obj->data = curve;
  transform_object(obj, import_params);

  return obj;
}

static int8_t get_valid_nurbs_degree(const NurbsElement &element)
{
  /* Use max(1, min()) to avoid undefined clamp behavior when curve_indices.size() == 0 */
  const int degree = std::max(1, std::min<int>(element.degree, element.curv_indices.size() - 1));
  return degree + 1 > std::numeric_limits<int8_t>::max() ? std::numeric_limits<int8_t>::max() - 1 :
                                                           int8_t(degree);
}

/**
 * Get the number of control points repeated for a cyclic curve given the multiplicity found
 * at the endpoints (assumes cyclic curve).
 */
static int repeating_cyclic_point_num(const int8_t order, const Span<float> knots)
{
  /* Due to the additional start knot, drop first.
   */
  Vector<int> multiplicity = bke::curves::nurbs::calculate_multiplicity_sequence(
      knots.slice(1, order - 1));

  BLI_assert(order > multiplicity.first());
  return order - multiplicity.first();
}

void CurveFromGeometry::create_nurbs(Curve *curve, const OBJImportParams &import_params)
{
  const NurbsElement &nurbs_geometry = curve_geometry_.nurbs_element_;
  const int8_t degree = get_valid_nurbs_degree(nurbs_geometry);
  Nurb *nurb = static_cast<Nurb *>(curve->nurb.first);

  nurb->type = CU_NURBS;
  nurb->flag = CU_3D;
  nurb->next = nurb->prev = nullptr;
  /* BKE_nurb_points_add later on will update pntsu. If this were set to total curve points,
   * we get double the total points in viewport. */
  nurb->pntsu = 0;
  /* Total points = pntsu * pntsv. */
  nurb->pntsv = 1;
  nurb->orderu = nurb->orderv = degree + 1;
  nurb->resolu = nurb->resolv = curve->resolu;

  const Vector<int> multiplicity = bke::curves::nurbs::calculate_multiplicity_sequence(
      nurbs_geometry.parm);
  nurb->flagu = this->detect_knot_mode(
      import_params, degree, nurbs_geometry.curv_indices, nurbs_geometry.parm, multiplicity);

  const int repeated_points = nurb->flagu & CU_NURB_CYCLIC ?
                                  repeating_cyclic_point_num(nurb->orderu, nurbs_geometry.parm) :
                                  0;
  const Span<int> indices = nurbs_geometry.curv_indices.as_span().slice(
      nurbs_geometry.curv_indices.index_range().drop_back(repeated_points));

  BKE_nurb_points_add(nurb, indices.size());
  for (const int i : indices.index_range()) {
    BPoint &bpoint = nurb->bp[i];
    copy_v3_v3(bpoint.vec, global_vertices_.vertices[indices[i]]);
    bpoint.vec[3] = (global_vertices_.vertex_weights.size() > indices[i]) ?
                        global_vertices_.vertex_weights[indices[i]] :
                        1.0f;
    bpoint.weight = 1.0f;
  }

  if (nurb->flagu & CU_NURB_CUSTOM) {
    BKE_nurb_knot_alloc_u(nurb);
    MutableSpan<float> knots_dst_u{nurb->knotsu, KNOTSU(nurb)};
    array_utils::copy<float>(nurbs_geometry.parm, knots_dst_u);
  }
  else {
    BKE_nurb_knot_calc_u(nurb);
  }
}

void CurveFromGeometry::create_nurbs(bke::CurvesGeometry &curves,
                                     const OBJImportParams &import_params)
{
  const NurbsElement &nurbs_geometry = curve_geometry_.nurbs_element_;
  const int8_t degree = get_valid_nurbs_degree(nurbs_geometry);
  const int8_t order = degree + 1;

  const Vector<int> multiplicity = bke::curves::nurbs::calculate_multiplicity_sequence(
      nurbs_geometry.parm);
  const short knot_flag = this->detect_knot_mode(
      import_params, degree, nurbs_geometry.curv_indices, nurbs_geometry.parm, multiplicity);

  const bool is_cyclic = knot_flag & CU_NURB_CYCLIC;
  const int repeated_points = is_cyclic ? repeating_cyclic_point_num(order, nurbs_geometry.parm) :
                                          0;
  const Span<int> indices = nurbs_geometry.curv_indices.as_span().slice(
      nurbs_geometry.curv_indices.index_range().drop_back(repeated_points));

  const int points_num = indices.size();
  const int curve_index = 0;
  curves.resize(points_num, 1);

  MutableSpan<int8_t> types = curves.curve_types_for_write();
  MutableSpan<bool> cyclic = curves.cyclic_for_write();
  MutableSpan<int8_t> orders = curves.nurbs_orders_for_write();
  MutableSpan<int8_t> modes = curves.nurbs_knots_modes_for_write();
  types.first() = CURVE_TYPE_NURBS;
  cyclic.first() = is_cyclic;
  orders.first() = order;
  modes.first() = bke::knots_mode_from_legacy(knot_flag);
  curves.update_curve_types();

  const OffsetIndices points_by_curve = curves.points_by_curve();
  const IndexRange point_range = points_by_curve[curve_index];

  MutableSpan<float3> positions = curves.positions_for_write().slice(point_range);
  MutableSpan<float> weights = curves.nurbs_weights_for_write().slice(point_range);
  for (const int i : indices.index_range()) {
    positions[i] = global_vertices_.vertices[indices[i]];
    weights[i] = (global_vertices_.vertex_weights.size() > indices[i]) ?
                     global_vertices_.vertex_weights[indices[i]] :
                     1.0f;
  }

  if (modes.first() == NURBS_KNOT_MODE_CUSTOM) {
    OffsetIndices<int> knot_offsets = curves.nurbs_custom_knots_by_curve();
    curves.nurbs_custom_knots_update_size();
    MutableSpan<float> knots = curves.nurbs_custom_knots_for_write().slice(
        knot_offsets[curve_index]);

    array_utils::copy<float>(nurbs_geometry.parm, knots);
  }
}

static bool detect_clamped_endpoint(const int8_t degree, const Span<int> multiplicity)
{
  const int8_t order = degree + 1;
  /* Consider any combination of following patterns as clamped:
   *
   * O ..
   * 1 d ..
   */
  const bool begin_clamped = multiplicity.first() == order ||
                             (multiplicity.first() == 1 && multiplicity[1] == degree);
  const bool end_clamped = multiplicity.last() == order ||
                           (multiplicity.last() == 1 && multiplicity.last(1) == degree);
  return begin_clamped && end_clamped;
}

static bool almost_equal_relative(const float a, const float b, const float epsilon)
{
  const float abs_diff = std::abs(b - a);
  return abs_diff < a * epsilon;
}

static bool detect_knot_mode_cyclic(const int8_t degree,
                                    const Span<int> indices,
                                    const Span<float> knots,
                                    const Span<int> multiplicity,
                                    const bool is_clamped)
{
  constexpr float epsilon = 1e-4;
  const int8_t order = degree + 1;

  const int repeated_points = repeating_cyclic_point_num(order, knots);
  BLI_assert(repeated_points > 0);
  const Span<int> indices_tail = indices.take_back(repeated_points);
  for (const int64_t i : indices_tail.index_range()) {
    if (indices[i] != indices_tail[i]) {
      return false;
    }
  }

  /* Multiplicity m is continuous to the `degree - m` derivative and as such
   * `multiplicity == degree` is discontinuous. Due to the superfluous knots
   * the first/last entry can be up to `order`, remaining up to `degree`.
   */
  if (multiplicity.first() > order || multiplicity.last() > order) {
    return false;
  }
  for (const int m : multiplicity.drop_front(1).drop_back(1)) {
    if (m > degree) {
      return false;
    }
  }

  if (is_clamped) {
    /* Clamped curves are discontinuous at the ends and have no overlapping spans. */
    return true;
  }

  /* Ensure it matches on both of the knot spans adjacent to the start/end of the parameter range.
   */
  const Span<float> knots_tail = knots.take_back(2 * degree + 1);
  for (const int64_t i : knots_tail.index_range().drop_back(1)) {
    const float head_span = knots[i + 1] - knots[i];
    const float tail_span = knots_tail[i + 1] - knots_tail[i];
    if (!almost_equal_relative(head_span, tail_span, epsilon)) {
      return false;
    }
  }
  return true;
}

static bool detect_knot_mode_bezier_clamped(const int8_t degree,
                                            const int num_points,
                                            const Span<int> multiplicity)
{
  const int8_t order = degree + 1;
  /* Don't treat polylines as Beziers. */
  if (order == 2) {
    return false;
  }

  /* Allow patterns:
    O d ..
    1 d d ..
  */
  if (multiplicity[0] < order && (multiplicity[0] != 1 || multiplicity[1] < degree)) {
    return false;
  }

  Span<int> mdegree_span = multiplicity.drop_front(1);
  if (multiplicity.size() == 2) {
    /* Single segment, allow patterns:
     * O a
     * where a > 0
     */
    if (multiplicity.first() != order) {
      return false;
    }
  }
  else {
    /* Allow patterns:
      .. d O+
      .. d d 1
    */
    if (multiplicity.last() != order &&
        (multiplicity.last() == 1 && multiplicity.last(1) != degree))
    {
      /* No match to the valid patterns. */
      return false;
    }

    const int remainder = (num_points - 1) % degree;
    if (multiplicity.last() != order + remainder &&
        (multiplicity.last() != 1 || multiplicity.last(1) < degree))
    {
      return false;
    }
  }
  mdegree_span = mdegree_span.drop_back(1);

  /* Verify all other knots are of degree multiplicity */
  for (const int m : mdegree_span) {
    if (m != degree) {
      return false;
    }
  }
  return true;
}

static bool detect_knot_mode_uniform(const int8_t degree,
                                     const Span<float> knots,
                                     const Span<int> multiplicity,
                                     const bool clamped)
{
  constexpr float epsilon = 1e-4;

  /* Check if knot count matches multiplicity adjusted for clamped ends. For a uniform non-clamped
   * curve, all multiplicity entries equals 1 and the array size should match.
   */
  const int O1_clamps = int(multiplicity.first() == 1) + int(multiplicity.last() == 1);
  const int clamped_offset = clamped ? 2 * degree - O1_clamps : 0;
  if (knots.size() != multiplicity.size() + clamped_offset) {
    return false;
  }

  /* Ensure it's not a single segment with clamped ends (it would be a Bezier segment). */
  const Span<float> unclamped_knots = knots.drop_front(clamped_offset).drop_back(clamped_offset);
  if (unclamped_knots.size() == 2) {
    return false;
  }
  if (unclamped_knots.size() < 2) {
    /* Classify single point as uniform? */
    return true;
  }

  /* Verify spacing is uniform (excluding clamped ends). */
  const float uniform_delta = unclamped_knots[1] - unclamped_knots[0];
  for (const int64_t i : unclamped_knots.index_range().drop_front(2)) {
    const float delta = unclamped_knots[i] - unclamped_knots[i - 1];
    if (!almost_equal_relative(delta, uniform_delta, epsilon)) {
      return false;
    }
  }
  return true;
}

short CurveFromGeometry::detect_knot_mode(const OBJImportParams &import_params,
                                          const int8_t degree,
                                          const Span<int> indices,
                                          const Span<float> knots,
                                          const Span<int> multiplicity)
{
  short knot_mode = 0;

  const bool is_clamped = detect_clamped_endpoint(degree, multiplicity);

  const bool is_bezier = detect_knot_mode_bezier_clamped(degree, indices.size(), multiplicity);
  if (is_bezier) {
    SET_FLAG_FROM_TEST(knot_mode, true, CU_NURB_ENDPOINT);
    SET_FLAG_FROM_TEST(knot_mode, true, CU_NURB_BEZIER);
  }
  else {
    const bool is_uniform = detect_knot_mode_uniform(degree, knots, multiplicity, is_clamped);
    SET_FLAG_FROM_TEST(knot_mode, is_clamped, CU_NURB_ENDPOINT);
    SET_FLAG_FROM_TEST(knot_mode, !is_uniform, CU_NURB_CUSTOM);
  }

  const bool check_cyclic = import_params.close_spline_loops && indices.size() > degree;
  const bool no_custom_cyclic = knot_mode & CU_NURB_CUSTOM;
  if (check_cyclic && !no_custom_cyclic) {
    const bool is_cyclic = detect_knot_mode_cyclic(
        degree, indices, knots, multiplicity, is_clamped);
    SET_FLAG_FROM_TEST(knot_mode, is_cyclic, CU_NURB_CYCLIC);
  }

  return knot_mode;
}

}  // namespace blender::io::obj
