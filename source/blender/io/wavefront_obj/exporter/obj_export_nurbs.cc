/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#include <numeric>

#include "BLI_listbase.h"
#include "BLI_utility_mixins.hh"

#include "BKE_curve_legacy_convert.hh"
#include "BKE_curves.hh"
#include "DNA_curve_types.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "obj_export_nurbs.hh"

namespace blender::io::obj {

/* -------------------------------------------------------------------- */
/** \name Utility
 * \{ */

/* Find the multiplicity entry with the valid span occurring on the right side of the related
 * breakpoint/knot. */
static int find_leftmost_span(const int8_t order, const Span<int> multiplicity)
{
  int index = -1;
  int acc = 0;
  while (acc < order) {
    acc += multiplicity[++index];
  }
  BLI_assert(index > -1);
  return index;
}

/* Find the multiplicity entry with the valid span occurring on the left side of the related
 * breakpoint/knot. */
static int find_rightmost_span(const int8_t order, const Span<int> multiplicity)
{
  int index = multiplicity.size();
  int acc = 0;
  while (acc < order) {
    acc += multiplicity[--index];
  }
  BLI_assert(index < multiplicity.size());
  return index;
}

Span<float> valid_nurb_control_point_range(const int8_t order,
                                           const Span<float> knots,
                                           IndexRange &point_range)
{
  /* No consideration for cyclic, export must expand the knot vector! */
  BLI_assert(knots.size() == bke::curves::nurbs::knots_num(point_range.size(), order, false));

  /* This assumes multiplicity < order * 2 */
  const int order2 = order * 2;
  Vector<int> left_mult = bke::curves::nurbs::calculate_multiplicity_sequence(
      knots.slice(0, order2));
  Vector<int> right_mult = bke::curves::nurbs::calculate_multiplicity_sequence(
      knots.slice(knots.size() - order2, order2));

  const int leftmost = find_leftmost_span(order, left_mult);
  const int rightmost = find_rightmost_span(order, right_mult);

  /* For reasonable curve knots they should add up to 0 */
  const int acc_start = std::accumulate(left_mult.begin(), left_mult.begin() + leftmost + 1, 0);
  const int acc_end = std::accumulate(&right_mult[rightmost], right_mult.end(), 0);
  int skip_start = acc_start - order;
  int skip_end = acc_end - order;

  /* Update ranges */
  point_range = point_range.drop_front(skip_start).drop_back(skip_end);
  return knots.drop_front(skip_start).drop_back(skip_end);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name OBJCurves
 * \{ */

OBJCurves::OBJCurves(const bke::CurvesGeometry &curve,
                     const float4x4 &transform,
                     const std::string &name)
    : curve_(curve), transform_(transform), name_(name)
{
}

const float4x4 &OBJCurves::object_transform() const
{
  return transform_;
}

const char *OBJCurves::get_curve_name() const
{
  return name_.c_str();
}

int OBJCurves::total_splines() const
{
  return curve_.curve_num;
}

int OBJCurves::total_spline_vertices(int spline_index) const
{
  return curve_.points_by_curve()[spline_index].size();
}

int OBJCurves::num_control_points_u(int spline_index) const
{
  return bke::curves::nurbs::control_points_num(curve_.points_by_curve()[spline_index].size(),
                                                get_nurbs_degree_u(spline_index) + 1,
                                                get_cyclic_u(spline_index));
}

int OBJCurves::num_control_points_v(int /*spline_index*/) const
{
  return 1;
}

int OBJCurves::get_nurbs_degree_u(int spline_index) const
{
  return curve_.nurbs_orders()[spline_index] - 1;
}

int OBJCurves::get_nurbs_degree_v(int /*spline_index*/) const
{
  return -1;
}

bool OBJCurves::get_cyclic_u(int spline_index) const
{
  return curve_.cyclic()[spline_index];
}

Span<float> OBJCurves::get_knots_u(int spline_index, Vector<float> &knot_buffer) const
{
  const int point_count = curve_.points_by_curve()[spline_index].size();
  const int8_t order = curve_.nurbs_orders()[spline_index];
  const bool cyclic = curve_.cyclic()[spline_index];
  const KnotsMode mode = KnotsMode(curve_.nurbs_knots_modes()[spline_index]);
  const int knot_count = bke::curves::nurbs::knots_num(point_count, order, cyclic);

  knot_buffer.resize(knot_count);
  bke::curves::nurbs::calculate_knots(point_count, mode, order, cyclic, knot_buffer);
  return knot_buffer;
}

Span<float3> OBJCurves::vertex_coordinates(int spline_index,
                                           Vector<float3> & /*dynamic_point_buffer*/) const
{
  const IndexRange point_range = curve_.points_by_curve()[spline_index];
  return curve_.positions().slice(point_range);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name OBJLegacyCurve
 * \{ */

OBJLegacyCurve::OBJLegacyCurve(const Depsgraph *depsgraph, Object *curve_object)
    : export_object_eval_(curve_object)
{
  export_object_eval_ = DEG_get_evaluated(depsgraph, curve_object);
  export_curve_ = static_cast<Curve *>(export_object_eval_->data);
}

const Nurb *OBJLegacyCurve::get_spline(const int spline_index) const
{
  return static_cast<Nurb *>(BLI_findlink(&export_curve_->nurb, spline_index));
}

const char *OBJLegacyCurve::get_curve_name() const
{
  return export_object_eval_->id.name + 2;
}

int OBJLegacyCurve::total_splines() const
{
  return BLI_listbase_count(&export_curve_->nurb);
}

const float4x4 &OBJLegacyCurve::object_transform() const
{
  return export_object_eval_->object_to_world();
}

int OBJLegacyCurve::total_spline_vertices(const int spline_index) const
{
  const Nurb *const nurb = get_spline(spline_index);
  return nurb->pntsu * nurb->pntsv;
}

Span<float3> OBJLegacyCurve::vertex_coordinates(const int spline_index,
                                                Vector<float3> &dynamic_point_buffer) const
{
  const Nurb *const nurb = get_spline(spline_index);
  dynamic_point_buffer.resize(nurb->pntsu);

  for (int64_t i = nurb->pntsu - 1; i >= 0; --i) {
    const BPoint &bpoint = nurb->bp[i];
    copy_v3_v3(dynamic_point_buffer[i], bpoint.vec);
  }

  return dynamic_point_buffer.as_span();
}

int OBJLegacyCurve::num_control_points_u(int spline_index) const
{
  const Nurb *const nurb = get_spline(spline_index);

  return bke::curves::nurbs::control_points_num(
      nurb->pntsu, get_nurbs_degree_u(spline_index) + 1, get_cyclic_u(spline_index));
}

int OBJLegacyCurve::num_control_points_v(int spline_index) const
{
  const Nurb *const nurb = get_spline(spline_index);
  return nurb->pntsv;
}

int OBJLegacyCurve::get_nurbs_degree_u(const int spline_index) const
{
  const Nurb *const nurb = get_spline(spline_index);
  return nurb->type == CU_POLY ? 1 : nurb->orderu - 1;
}

int OBJLegacyCurve::get_nurbs_degree_v(const int spline_index) const
{
  const Nurb *const nurb = get_spline(spline_index);
  return nurb->type == CU_POLY ? 1 : nurb->orderv - 1;
}

bool OBJLegacyCurve::get_cyclic_u(int spline_index) const
{
  const Nurb *const nurb = get_spline(spline_index);
  return bool(nurb->flagu & CU_NURB_CYCLIC);
}

Span<float> OBJLegacyCurve::get_knots_u(int spline_index, Vector<float> &knot_buffer) const
{
  const Nurb *const nurb = get_spline(spline_index);
  const short flag = nurb->flagu;
  const int8_t order = get_nurbs_degree_u(spline_index) + 1; /* Use utility in case of POLY */
  const bool cyclic = flag & CU_NURB_CYCLIC;

  const int knot_count = bke::curves::nurbs::knots_num(nurb->pntsu, order, cyclic);

  if (flag & CU_NURB_CUSTOM) {
    return Span<float>(nurb->knotsu, knot_count);
  }

  knot_buffer.resize(knot_count);
  bke::curves::nurbs::calculate_knots(
      nurb->pntsu, bke::knots_mode_from_legacy(flag), order, cyclic, knot_buffer);
  return knot_buffer;
}

/** \} */

}  // namespace blender::io::obj
