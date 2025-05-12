/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"

#include "BKE_curve_legacy_convert.hh"
#include "BKE_curves.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "IO_wavefront_obj.hh"
#include "obj_export_nurbs.hh"

namespace blender::io::obj {
OBJCurve::OBJCurve(const Depsgraph *depsgraph,
                   const OBJExportParams &export_params,
                   Object *curve_object)
    : export_object_eval_(curve_object)
{
  export_object_eval_ = DEG_get_evaluated(depsgraph, curve_object);
  export_curve_ = static_cast<Curve *>(export_object_eval_->data);
  set_world_axes_transform(export_params.forward_axis, export_params.up_axis);
}

void OBJCurve::set_world_axes_transform(const eIOAxis forward, const eIOAxis up)
{
  float axes_transform[3][3];
  unit_m3(axes_transform);
  /* +Y-forward and +Z-up are the Blender's default axis settings. */
  mat3_from_axis_conversion(forward, up, IO_AXIS_Y, IO_AXIS_Z, axes_transform);
  mul_m4_m3m4(world_axes_transform_, axes_transform, export_object_eval_->object_to_world().ptr());
  /* #mul_m4_m3m4 does not transform last row of #Object.object_to_world, i.e. location data. */
  mul_v3_m3v3(
      world_axes_transform_[3], axes_transform, export_object_eval_->object_to_world().location());
  world_axes_transform_[3][3] = export_object_eval_->object_to_world()[3][3];
}

const char *OBJCurve::get_curve_name() const
{
  return export_object_eval_->id.name + 2;
}

int OBJCurve::total_splines() const
{
  return BLI_listbase_count(&export_curve_->nurb);
}

const Nurb *OBJCurve::get_spline(const int spline_index) const
{
  return static_cast<Nurb *>(BLI_findlink(&export_curve_->nurb, spline_index));
}

int OBJCurve::total_spline_vertices(const int spline_index) const
{
  const Nurb *const nurb = static_cast<Nurb *>(BLI_findlink(&export_curve_->nurb, spline_index));
  return nurb->pntsu * nurb->pntsv;
}

float3 OBJCurve::vertex_coordinates(const int spline_index,
                                    const int vertex_index,
                                    const float global_scale) const
{
  const Nurb *const nurb = static_cast<Nurb *>(BLI_findlink(&export_curve_->nurb, spline_index));
  float3 r_coord;
  const BPoint &bpoint = nurb->bp[vertex_index];
  copy_v3_v3(r_coord, bpoint.vec);
  mul_m4_v3(world_axes_transform_, r_coord);
  mul_v3_fl(r_coord, global_scale);
  return r_coord;
}

int OBJCurve::num_control_points_u(int spline_index) const
{
  const Nurb *const nurb = static_cast<Nurb *>(BLI_findlink(&export_curve_->nurb, spline_index));
  return nurb->pntsu + (nurb->flagu & CU_NURB_CYCLIC ? get_nurbs_degree_u(spline_index) : 0);
}

int OBJCurve::num_control_points_v(int spline_index) const
{
  const Nurb *const nurb = static_cast<Nurb *>(BLI_findlink(&export_curve_->nurb, spline_index));
  return nurb->pntsv + (nurb->flagv & CU_NURB_CYCLIC ? get_nurbs_degree_v(spline_index) : 0);
}

int OBJCurve::get_nurbs_degree_u(const int spline_index) const
{
  const Nurb *const nurb = static_cast<Nurb *>(BLI_findlink(&export_curve_->nurb, spline_index));
  return nurb->type == CU_POLY ? 1 : nurb->orderu - 1;
}

int OBJCurve::get_nurbs_degree_v(const int spline_index) const
{
  const Nurb *const nurb = static_cast<Nurb *>(BLI_findlink(&export_curve_->nurb, spline_index));
  return nurb->type == CU_POLY ? 1 : nurb->orderv - 1;
}

short OBJCurve::get_nurbs_flagu(const int spline_index) const
{
  const Nurb *const nurb = static_cast<Nurb *>(BLI_findlink(&export_curve_->nurb, spline_index));
  return nurb->flagu;
}

Span<float> OBJCurve::get_knots_u(int spline_index, Vector<float> &knot_buffer) const
{
  const Nurb *const nurb = static_cast<Nurb *>(BLI_findlink(&export_curve_->nurb, spline_index));
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

}  // namespace blender::io::obj
