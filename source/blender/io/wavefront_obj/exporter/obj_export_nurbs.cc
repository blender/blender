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
  export_object_eval_ = DEG_get_evaluated_object(depsgraph, curve_object);
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
  world_axes_transform_[3][3] = export_object_eval_->object_to_world().location()[3];
}

const char *OBJCurve::get_curve_name() const
{
  return export_object_eval_->id.name + 2;
}

int OBJCurve::total_splines() const
{
  return BLI_listbase_count(&export_curve_->nurb);
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

int OBJCurve::total_spline_control_points(const int spline_index) const
{
  const Nurb *const nurb = static_cast<Nurb *>(BLI_findlink(&export_curve_->nurb, spline_index));
  int degree = nurb->type == CU_POLY ? 1 : nurb->orderu - 1;
  /* Total control points = Number of points in the curve (+ degree of the
   * curve if it is cyclic). */
  int tot_control_points = nurb->pntsv * nurb->pntsu;
  if (nurb->flagu & CU_NURB_CYCLIC) {
    tot_control_points += degree;
  }
  return tot_control_points;
}

int OBJCurve::get_nurbs_degree(const int spline_index) const
{
  const Nurb *const nurb = static_cast<Nurb *>(BLI_findlink(&export_curve_->nurb, spline_index));
  return nurb->type == CU_POLY ? 1 : nurb->orderu - 1;
}

short OBJCurve::get_nurbs_flagu(const int spline_index) const
{
  const Nurb *const nurb = static_cast<Nurb *>(BLI_findlink(&export_curve_->nurb, spline_index));
  return nurb->flagu;
}

}  // namespace blender::io::obj
