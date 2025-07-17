/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

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

Curve *blender::io::obj::CurveFromGeometry::create_curve(const OBJImportParams &import_params)
{
  BLI_assert(!curve_geometry_.nurbs_element_.curv_indices.is_empty());

  /* Use of #BKE_id_new_nomain<Curve>(nullptr) limits use for the Curve, see #BKE_curve_add. */
  Curve *curve = BKE_id_new_nomain<Curve>(nullptr);

  curve->flag = CU_3D;
  curve->resolu = curve->resolv = 12;
  /* Only one NURBS spline will be created in the curve object. */
  curve->actnu = 0;

  Nurb *nurb = MEM_callocN<Nurb>(__func__);
  BLI_addtail(BKE_curve_nurbs_get(curve), nurb);
  this->create_nurbs(curve, import_params);

  return curve;
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
 * Get the number of control points repeated for a cyclic curve given the multiplicity at the end
 * of the knot vectors (multiplicity at both ends need to match).
 */
static int cyclic_repeated_points(const int8_t order, const int end_multiplicity)
{
  /* Since multiplicity == order is considered 'cyclic' even if it is only C0 continuous in
   * principle (it is technically discontinuous!), it needs to be clamped to 1.
   */
  return std::max<int>(order - end_multiplicity, 1);
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

  if ((nurb->flagu & (CU_NURB_CUSTOM | CU_NURB_CYCLIC | CU_NURB_ENDPOINT)) == CU_NURB_CUSTOM) {
    /* TODO: If mode is CU_NURB_CUSTOM, but not CU_NURB_CYCLIC and CU_NURB_ENDPOINT, then make
     * curve clamped instead of removing CU_NURB_CUSTOM. */
    nurb->flagu &= ~CU_NURB_CUSTOM;
  }

  const int repeated_points = nurb->flagu & CU_NURB_CYCLIC ?
                                  cyclic_repeated_points(nurb->orderu, multiplicity.first()) :
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
    array_utils::copy<float>(nurbs_geometry.parm, MutableSpan<float>{nurb->knotsu, KNOTSU(nurb)});
  }
  else {
    BKE_nurb_knot_calc_u(nurb);
  }
}

static bool detect_clamped_endpoint(const int8_t degree, const Span<int> multiplicity)
{
  const int8_t order = degree + 1;
  return multiplicity.first() == order && multiplicity.last() == order;
}

static bool detect_knot_mode_cyclic(const int8_t degree,
                                    const Span<int> indices,
                                    const Span<float> knots,
                                    const Span<int> multiplicity)
{
  constexpr float epsilon = 1e-7;
  const int8_t order = degree + 1;

  /* This is a good distinction between the 'cyclic' property and a true periodic NURBS curve. A
   * periodic curve should be smooth to the degree - 1 derivative (which is the maximum possible).
   * Allowing matching `multiplicity > 1` is not a periodic NURBS but can be considered cyclic.
   */
  if (multiplicity.first() != multiplicity.last()) {
    return false;
  }

  /* Multiplicity m is continuous to the `degree - m` derivative and as such
   * `multiplicity == order` is discontinuous.
   * By allowing it, clamped or Bezier curves can still be considered cyclic but
   * ensure [here] that illogical `multiplicities > order` is not considered cyclic.
   */
  if (multiplicity.first() > order || multiplicity.last() > order) {

    return false;
  }

  const int repeated_points = cyclic_repeated_points(order, multiplicity.first());
  const Span<int> indices_tail = indices.take_back(repeated_points);
  for (const int64_t i : indices_tail.index_range()) {
    if (indices[i] != indices_tail[i]) {
      return false;
    }
  }

  if (multiplicity.first() >= degree) {
    /* There is no overlap in the knot spans. */
    return true;
  }

  /* Ensure it matches on both of the knot spans adjacent to the start/end of the parameter range.
   */
  const Span<float> knots_tail = knots.take_back(order + degree);
  for (const int64_t i : knots_tail.index_range().drop_back(1)) {
    const float head_span = knots[i + 1] - knots[i];
    const float tail_span = knots_tail[i + 1] - knots_tail[i];
    if (abs(head_span - tail_span) > epsilon) {
      return false;
    }
  }
  return true;
}

static bool detect_knot_mode_bezier(const int8_t degree, const Span<int> multiplicity)
{
  const int8_t order = degree + 1;
  if (multiplicity.first() != order || multiplicity.last() != order) {
    return false;
  }

  for (const int m : multiplicity.drop_front(1).drop_back(1)) {
    if (m != degree) {
      return false;
    }
  }
  return true;
}

static bool detect_knot_mode_uniform(const int8_t degree,
                                     const Span<float> knots,
                                     const Span<int> multiplicity,
                                     bool clamped)
{
  constexpr float epsilon = 1e-7;

  /* Check if knot count matches multiplicity adjusted for clamped ends. For a uniform non-clamped
   * curve, all multiplicity entries equals 1 and the array size should match.
   */
  const int clamped_offset = clamped * degree;
  if (knots.size() != multiplicity.size() - 2 * clamped_offset) {
    return false;
  }

  /* Ensure it's not a single segment with clamped ends (it would be a Bezier segment). */
  const Span<float> unclamped_knots = knots.drop_front(clamped_offset).drop_back(clamped_offset);
  if (!unclamped_knots.size()) {
    return false;
  }

  /* Verify spacing is uniform (excluding clamped ends). */
  const float uniform_delta = unclamped_knots[1] - unclamped_knots[0];
  for (const int64_t i : knots.index_range().drop_front(2)) {
    if (abs((knots[i] - knots[i - 1]) - uniform_delta) < epsilon) {
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

  if (import_params.close_spline_loops && indices.size() > degree) {
    SET_FLAG_FROM_TEST(
        knot_mode, detect_knot_mode_cyclic(degree, indices, knots, multiplicity), CU_NURB_CYCLIC);
  }

  if (detect_knot_mode_bezier(degree, multiplicity)) {
    /* Currently endpoint flag is not parsed for Bezier, mainly because a clamped Bezier curve in
     * Blender is either:
     * a) A valid Bezier curve for given degree/order with correct number of points to form one.
     * b) Not a valid Bezier curve for given degree, last span/segment is of a lower degree Bezier.
     *
     * Set ENDPOINT to true since legacy Bezier NURBS only validates and compute knots if it
     * contains order + 1 control points unless endpoint is set...?
     */
    SET_FLAG_FROM_TEST(knot_mode, true, CU_NURB_ENDPOINT);
    SET_FLAG_FROM_TEST(knot_mode, true, CU_NURB_BEZIER);
  }
  else {
    const bool clamped = detect_clamped_endpoint(degree, multiplicity);
    SET_FLAG_FROM_TEST(knot_mode, clamped, CU_NURB_ENDPOINT);
    SET_FLAG_FROM_TEST(knot_mode,
                       !detect_knot_mode_uniform(degree, knots, multiplicity, clamped),
                       CU_NURB_CUSTOM);
  }
  return knot_mode;
}

}  // namespace blender::io::obj
