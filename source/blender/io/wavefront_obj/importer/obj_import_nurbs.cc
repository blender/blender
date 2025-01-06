/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#include "BKE_lib_id.hh"
#include "BKE_object.hh"

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

  Curve *curve = static_cast<Curve *>(BKE_id_new_nomain(ID_CU_LEGACY, nullptr));

  BKE_curve_init(curve, OB_CURVES_LEGACY);

  curve->flag = CU_3D;
  curve->resolu = curve->resolv = 12;
  /* Only one NURBS spline will be created in the curve object. */
  curve->actnu = 0;

  Nurb *nurb = static_cast<Nurb *>(MEM_callocN(sizeof(Nurb), __func__));
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

  Nurb *nurb = static_cast<Nurb *>(MEM_callocN(sizeof(Nurb), __func__));
  BLI_addtail(BKE_curve_nurbs_get(curve), nurb);
  this->create_nurbs(curve, import_params);

  obj->data = curve;
  transform_object(obj, import_params);

  return obj;
}

void CurveFromGeometry::create_nurbs(Curve *curve, const OBJImportParams &import_params)
{
  const NurbsElement &nurbs_geometry = curve_geometry_.nurbs_element_;
  const int degree = nurbs_geometry.degree;
  Nurb *nurb = static_cast<Nurb *>(curve->nurb.first);

  nurb->type = CU_NURBS;
  nurb->flag = CU_3D;
  nurb->next = nurb->prev = nullptr;
  /* BKE_nurb_points_add later on will update pntsu. If this were set to total curve points,
   * we get double the total points in viewport. */
  nurb->pntsu = 0;
  /* Total points = pntsu * pntsv. */
  nurb->pntsv = 1;
  nurb->orderu = nurb->orderv = (nurbs_geometry.degree + 1 > SHRT_MAX) ? 4 :
                                                                         nurbs_geometry.degree + 1;
  nurb->resolu = nurb->resolv = curve->resolu;

  nurb->flagu = this->detect_knot_mode(import_params,
                                       degree,
                                       nurbs_geometry.curv_indices,
                                       nurbs_geometry.parm,
                                       nurbs_geometry.range);

  const Span<int> indices = nurbs_geometry.curv_indices.as_span().slice(
      nurbs_geometry.curv_indices.index_range().drop_front(nurb->flagu & CU_NURB_CYCLIC ? degree :
                                                                                          0));

  BKE_nurb_points_add(nurb, indices.size());
  for (const int i : indices.index_range()) {
    BPoint &bpoint = nurb->bp[i];
    copy_v3_v3(bpoint.vec, global_vertices_.vertices[indices[i]]);
    bpoint.vec[3] = (global_vertices_.vertex_weights.size() > indices[i]) ?
                        global_vertices_.vertex_weights[indices[i]] :
                        1.0f;
    bpoint.weight = 1.0f;
  }

  BKE_nurb_knot_calc_u(nurb);
}

short CurveFromGeometry::detect_knot_mode(const OBJImportParams &import_params,
                                          const int degree,
                                          const Span<int> indices,
                                          const Span<float> knots,
                                          const float2 range)
{
  short knot_mode = 0;

  if (import_params.close_spline_loops && indices.size() > degree) {
    const Span<int> indices_tail = indices.take_back(degree);
    bool is_cyclic = true;
    for (const int i : IndexRange(degree)) {
      if (indices[i] != indices_tail[i]) {
        is_cyclic = false;
        break;
      }
    }
    const Span<float> knots_tail = knots.take_back(2 * degree + 1);
    for (const int i : IndexRange(degree - 1)) {
      const float head_span = knots[i + 1] - knots[i];
      const float tail_span = knots_tail[i + 1] - knots_tail[i];
      if (abs(head_span - tail_span) > 0.0001f) {
        is_cyclic = false;
        break;
      }
    }
    if (is_cyclic) {
      knot_mode = CU_NURB_CYCLIC;
    }
  }

  /* Figure out whether curve should have U endpoint flag set:
   * the parameters should have at least (degree+1) values on each end,
   * and their values should match curve range. */
  bool do_endpoints = false;
  int order = degree + 1;
  if (knots.size() >= order * 2) {
    do_endpoints = true;
    for (int i = 0; i < order; ++i) {
      if (abs(knots[i] - range.x) > 0.0001f || abs(knots.last(i) - range.y) > 0.0001f) {
        do_endpoints = false;
        break;
      }
    }
  }
  IndexRange inner_knots = knots.index_range();
  if (do_endpoints) {
    knot_mode |= CU_NURB_ENDPOINT;
    inner_knots = inner_knots.size() > 2 * degree ?
                      inner_knots.drop_front(degree).drop_back(degree) :
                      IndexRange();
  }
  if (!inner_knots.is_empty()) {
    const float first_step = knots[inner_knots.first() + 1] - knots[inner_knots.first()];
    bool is_spacing_equal = true;
    bool is_bezier_knot = degree > 1;
    int repeats = 0;
    for (const int i : inner_knots.drop_front(1).drop_back(1)) {
      const float step = knots[i + 1] - knots[i];
      if (abs(step - first_step) > 0.0001f) {
        is_spacing_equal = false;
        if (step == 0.0f) {
          repeats++;
          if (repeats > degree - 1) {
            is_bezier_knot = false;
          }
        }
      }
      else if (repeats == degree - 1) {
        repeats = 0;
      }
      else {
        is_bezier_knot = false;
      }
    }
    if (!is_spacing_equal && is_bezier_knot) {
      knot_mode |= CU_NURB_BEZIER;
    }
  }
  return knot_mode;
}

}  // namespace blender::io::obj
