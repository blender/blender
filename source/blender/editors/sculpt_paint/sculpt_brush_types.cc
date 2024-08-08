/* SPDX-FileCopyrightText: 2006 by Nicholas Bishop. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 * Implements the Sculpt Mode tools.
 */

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_span.hh"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "DNA_brush_types.h"
#include "DNA_customdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_brush.hh"
#include "BKE_ccg.hh"
#include "BKE_colortools.hh"
#include "BKE_kelvinlet.h"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"

#include "ED_view3d.hh"

#include "paint_intern.hh"
#include "sculpt_intern.hh"

#include "bmesh.hh"

#include <cmath>
#include <cstdlib>
#include <cstring>

using blender::float3;
using blender::MutableSpan;
using blender::Span;

void sculpt_project_v3_normal_align(const SculptSession &ss,
                                    const float normal_weight,
                                    float grab_delta[3])
{
  /* Signed to support grabbing in (to make a hole) as well as out. */
  const float len_signed = dot_v3v3(ss.cache->sculpt_normal_symm, grab_delta);

  /* This scale effectively projects the offset so dragging follows the cursor,
   * as the normal points towards the view, the scale increases. */
  float len_view_scale;
  {
    float view_aligned_normal[3];
    project_plane_v3_v3v3(
        view_aligned_normal, ss.cache->sculpt_normal_symm, ss.cache->view_normal);
    len_view_scale = fabsf(dot_v3v3(view_aligned_normal, ss.cache->sculpt_normal_symm));
    len_view_scale = (len_view_scale > FLT_EPSILON) ? 1.0f / len_view_scale : 1.0f;
  }

  mul_v3_fl(grab_delta, 1.0f - normal_weight);
  madd_v3_v3fl(
      grab_delta, ss.cache->sculpt_normal_symm, (len_signed * normal_weight) * len_view_scale);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt Topology Brush
 * \{ */

namespace blender::ed::sculpt_paint::smooth {

static void relax_vertex_interior(SculptSession &ss,
                                  PBVHVertRef vert,
                                  const float factor,
                                  const bool filter_boundary_face_sets,
                                  float *r_final_pos)
{
  const float3 &co = SCULPT_vertex_co_get(ss, vert);
  float smooth_pos[3];
  int avg_count = 0;
  int neighbor_count = 0;
  zero_v3(smooth_pos);

  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vert, ni) {
    neighbor_count++;
    if (!filter_boundary_face_sets ||
        (filter_boundary_face_sets && !face_set::vert_has_unique_face_set(ss, ni.vertex)))
    {

      add_v3_v3(smooth_pos, SCULPT_vertex_co_get(ss, ni.vertex));
      avg_count++;
    }
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  /* Don't modify corner vertices. */
  if (neighbor_count <= 2) {
    copy_v3_v3(r_final_pos, co);
    return;
  }

  if (avg_count == 0) {
    copy_v3_v3(r_final_pos, co);
    return;
  }

  mul_v3_fl(smooth_pos, 1.0f / avg_count);

  const float3 vno = SCULPT_vertex_normal_get(ss, vert);

  if (is_zero_v3(vno)) {
    copy_v3_v3(r_final_pos, co);
    return;
  }

  float plane[4];
  plane_from_point_normal_v3(plane, co, vno);

  float smooth_closest_plane[3];
  closest_to_plane_v3(smooth_closest_plane, plane, smooth_pos);

  float final_disp[3];
  sub_v3_v3v3(final_disp, smooth_closest_plane, co);

  mul_v3_fl(final_disp, factor);
  add_v3_v3v3(r_final_pos, co, final_disp);
}

static void relax_vertex_boundary(SculptSession &ss,
                                  PBVHVertRef vert,
                                  const float factor,
                                  const bool filter_boundary_face_sets,
                                  float *r_final_pos)
{
  const float3 &co = SCULPT_vertex_co_get(ss, vert);
  float smooth_pos[3];
  float boundary_normal[3];
  int avg_count = 0;
  int neighbor_count = 0;
  zero_v3(smooth_pos);
  zero_v3(boundary_normal);

  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vert, ni) {
    neighbor_count++;
    if (!filter_boundary_face_sets ||
        (filter_boundary_face_sets && !face_set::vert_has_unique_face_set(ss, ni.vertex)))
    {

      /* When the vertex to relax is boundary, use only connected boundary vertices for the average
       * position. */
      if (!boundary::vert_is_boundary(ss, ni.vertex)) {
        continue;
      }
      add_v3_v3(smooth_pos, SCULPT_vertex_co_get(ss, ni.vertex));
      avg_count++;

      /* Calculate a normal for the constraint plane using the edges of the boundary. */
      float to_neighbor[3];
      sub_v3_v3v3(to_neighbor, SCULPT_vertex_co_get(ss, ni.vertex), co);
      normalize_v3(to_neighbor);
      add_v3_v3(boundary_normal, to_neighbor);
    }
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  /* Don't modify corner vertices. */
  if (neighbor_count <= 2) {
    copy_v3_v3(r_final_pos, co);
    return;
  }

  if (avg_count == 0) {
    copy_v3_v3(r_final_pos, co);
    return;
  }

  mul_v3_fl(smooth_pos, 1.0f / avg_count);

  float3 vno;
  if (avg_count == 2) {
    normalize_v3_v3(vno, boundary_normal);
  }
  else {
    vno = SCULPT_vertex_normal_get(ss, vert);
  }

  if (is_zero_v3(vno)) {
    copy_v3_v3(r_final_pos, co);
    return;
  }

  float plane[4];
  plane_from_point_normal_v3(plane, co, vno);

  float smooth_closest_plane[3];
  closest_to_plane_v3(smooth_closest_plane, plane, smooth_pos);

  float final_disp[3];
  sub_v3_v3v3(final_disp, smooth_closest_plane, co);

  mul_v3_fl(final_disp, factor);
  add_v3_v3v3(r_final_pos, co, final_disp);
}

void relax_vertex(SculptSession &ss,
                  PBVHVertRef vert,
                  const float factor,
                  const bool filter_boundary_face_sets,
                  float *r_final_pos)
{
  if (boundary::vert_is_boundary(ss, vert)) {
    relax_vertex_boundary(ss, vert, factor, filter_boundary_face_sets, r_final_pos);
  }
  else {
    relax_vertex_interior(ss, vert, factor, filter_boundary_face_sets, r_final_pos);
  }
}

}  // namespace blender::ed::sculpt_paint::smooth
