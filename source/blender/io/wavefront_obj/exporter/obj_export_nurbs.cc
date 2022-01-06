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

/** \file
 * \ingroup obj
 */

#include "BLI_float3.hh"
#include "BLI_listbase.h"
#include "BLI_math.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "IO_wavefront_obj.h"
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

void OBJCurve::set_world_axes_transform(const eTransformAxisForward forward,
                                        const eTransformAxisUp up)
{
  float axes_transform[3][3];
  unit_m3(axes_transform);
  /* +Y-forward and +Z-up are the Blender's default axis settings. */
  mat3_from_axis_conversion(OBJ_AXIS_Y_FORWARD, OBJ_AXIS_Z_UP, forward, up, axes_transform);
  /* mat3_from_axis_conversion returns a transposed matrix! */
  transpose_m3(axes_transform);
  mul_m4_m3m4(world_axes_transform_, axes_transform, export_object_eval_->obmat);
  /* #mul_m4_m3m4 does not transform last row of #Object.obmat, i.e. location data. */
  mul_v3_m3v3(world_axes_transform_[3], axes_transform, export_object_eval_->obmat[3]);
  world_axes_transform_[3][3] = export_object_eval_->obmat[3][3];
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
                                    const float scaling_factor) const
{
  const Nurb *const nurb = static_cast<Nurb *>(BLI_findlink(&export_curve_->nurb, spline_index));
  float3 r_coord;
  const BPoint &bpoint = nurb->bp[vertex_index];
  copy_v3_v3(r_coord, bpoint.vec);
  mul_m4_v3(world_axes_transform_, r_coord);
  mul_v3_fl(r_coord, scaling_factor);
  return r_coord;
}

int OBJCurve::total_spline_control_points(const int spline_index) const
{
  const Nurb *const nurb = static_cast<Nurb *>(BLI_findlink(&export_curve_->nurb, spline_index));
  const int r_nurbs_degree = nurb->orderu - 1;
  /* Total control points = Number of points in the curve (+ degree of the
   * curve if it is cyclic). */
  int r_tot_control_points = nurb->pntsv * nurb->pntsu;
  if (nurb->flagu & CU_NURB_CYCLIC) {
    r_tot_control_points += r_nurbs_degree;
  }
  return r_tot_control_points;
}

int OBJCurve::get_nurbs_degree(const int spline_index) const
{
  const Nurb *const nurb = static_cast<Nurb *>(BLI_findlink(&export_curve_->nurb, spline_index));
  return nurb->orderu - 1;
}

}  // namespace blender::io::obj
