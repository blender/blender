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

static void do_layer_brush_task(Object &ob, const Sculpt &sd, const Brush &brush, PBVHNode *node)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;

  const bool use_persistent_base = !ss.bm && ss.attrs.persistent_co &&
                                   brush.flag & BRUSH_PERSISTENT;

  PBVHVertexIter vd;
  const float bstrength = ss.cache->bstrength;
  SculptOrigVertData orig_data = SCULPT_orig_vert_data_init(ob, *node, undo::Type::Position);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, test, brush.falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(nullptr);

  auto_mask::NodeData automask_data = auto_mask::node_begin(
      ob, ss.cache->automasking.get(), *node);

  BKE_pbvh_vertex_iter_begin (*ss.pbvh, node, vd, PBVH_ITER_UNIQUE) {
    SCULPT_orig_vert_data_update(orig_data, vd);

    if (!sculpt_brush_test_sq_fn(test, orig_data.co)) {
      continue;
    }
    auto_mask::node_update(automask_data, vd);

    const float fade = SCULPT_brush_strength_factor(ss,
                                                    brush,
                                                    vd.co,
                                                    sqrtf(test.dist),
                                                    vd.no,
                                                    vd.fno,
                                                    vd.mask,
                                                    vd.vertex,
                                                    thread_id,
                                                    &automask_data);

    const int vi = vd.index;
    float *disp_factor;
    if (use_persistent_base) {
      disp_factor = (float *)SCULPT_vertex_attr_get(vd.vertex, ss.attrs.persistent_disp);
    }
    else {
      disp_factor = &ss.cache->layer_displacement_factor[vi];
    }

    /* When using persistent base, the layer brush (holding Control) invert mode resets the
     * height of the layer to 0. This makes possible to clean edges of previously added layers
     * on top of the base. */
    /* The main direction of the layers is inverted using the regular brush strength with the
     * brush direction property. */
    if (use_persistent_base && ss.cache->invert) {
      (*disp_factor) += fabsf(fade * bstrength * (*disp_factor)) *
                        ((*disp_factor) > 0.0f ? -1.0f : 1.0f);
    }
    else {
      (*disp_factor) += fade * bstrength * (1.05f - fabsf(*disp_factor));
    }
    const float clamp_mask = 1.0f - vd.mask;
    *disp_factor = clamp_f(*disp_factor, -clamp_mask, clamp_mask);

    float final_co[3];
    float3 normal;

    if (use_persistent_base) {
      normal = SCULPT_vertex_persistent_normal_get(ss, vd.vertex);
      mul_v3_fl(normal, brush.height);
      madd_v3_v3v3fl(
          final_co, SCULPT_vertex_persistent_co_get(ss, vd.vertex), normal, *disp_factor);
    }
    else {
      copy_v3_v3(normal, orig_data.no);
      mul_v3_fl(normal, brush.height);
      madd_v3_v3v3fl(final_co, orig_data.co, normal, *disp_factor);
    }

    float vdisp[3];
    sub_v3_v3v3(vdisp, final_co, vd.co);
    mul_v3_fl(vdisp, fabsf(fade));
    add_v3_v3v3(final_co, vd.co, vdisp);

    SCULPT_clip(sd, ss, vd.co, final_co);
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_layer_brush(const Sculpt &sd, Object &ob, Span<PBVHNode *> nodes)
{
  using namespace blender;
  SculptSession &ss = *ob.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  if (ss.cache->layer_displacement_factor == nullptr) {
    ss.cache->layer_displacement_factor = MEM_cnew_array<float>(SCULPT_vertex_count_get(ss),
                                                                __func__);
  }

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      do_layer_brush_task(ob, sd, brush, nodes[i]);
    }
  });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt Topology Brush
 * \{ */

namespace blender::ed::sculpt_paint::smooth {

static void relax_vertex_interior(SculptSession &ss,
                                  PBVHVertexIter *vd,
                                  const float factor,
                                  const bool filter_boundary_face_sets,
                                  float *r_final_pos)
{
  float smooth_pos[3];
  int avg_count = 0;
  int neighbor_count = 0;
  zero_v3(smooth_pos);

  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vd->vertex, ni) {
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
    copy_v3_v3(r_final_pos, vd->co);
    return;
  }

  if (avg_count == 0) {
    copy_v3_v3(r_final_pos, vd->co);
    return;
  }

  mul_v3_fl(smooth_pos, 1.0f / avg_count);

  const float3 vno = SCULPT_vertex_normal_get(ss, vd->vertex);

  if (is_zero_v3(vno)) {
    copy_v3_v3(r_final_pos, vd->co);
    return;
  }

  float plane[4];
  plane_from_point_normal_v3(plane, vd->co, vno);

  float smooth_closest_plane[3];
  closest_to_plane_v3(smooth_closest_plane, plane, smooth_pos);

  float final_disp[3];
  sub_v3_v3v3(final_disp, smooth_closest_plane, vd->co);

  mul_v3_fl(final_disp, factor);
  add_v3_v3v3(r_final_pos, vd->co, final_disp);
}

static void relax_vertex_boundary(SculptSession &ss,
                                  PBVHVertexIter *vd,
                                  const float factor,
                                  const bool filter_boundary_face_sets,
                                  float *r_final_pos)
{
  float smooth_pos[3];
  float boundary_normal[3];
  int avg_count = 0;
  int neighbor_count = 0;
  zero_v3(smooth_pos);
  zero_v3(boundary_normal);

  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vd->vertex, ni) {
    neighbor_count++;
    if (!filter_boundary_face_sets ||
        (filter_boundary_face_sets && !face_set::vert_has_unique_face_set(ss, ni.vertex)))
    {

      /* When the vertex to relax is boundary, use only connected boundary vertices for the average
       * position. */
      if (!SCULPT_vertex_is_boundary(ss, ni.vertex)) {
        continue;
      }
      add_v3_v3(smooth_pos, SCULPT_vertex_co_get(ss, ni.vertex));
      avg_count++;

      /* Calculate a normal for the constraint plane using the edges of the boundary. */
      float to_neighbor[3];
      sub_v3_v3v3(to_neighbor, SCULPT_vertex_co_get(ss, ni.vertex), vd->co);
      normalize_v3(to_neighbor);
      add_v3_v3(boundary_normal, to_neighbor);
    }
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  /* Don't modify corner vertices. */
  if (neighbor_count <= 2) {
    copy_v3_v3(r_final_pos, vd->co);
    return;
  }

  if (avg_count == 0) {
    copy_v3_v3(r_final_pos, vd->co);
    return;
  }

  mul_v3_fl(smooth_pos, 1.0f / avg_count);

  float3 vno;
  if (avg_count == 2) {
    normalize_v3_v3(vno, boundary_normal);
  }
  else {
    vno = SCULPT_vertex_normal_get(ss, vd->vertex);
  }

  if (is_zero_v3(vno)) {
    copy_v3_v3(r_final_pos, vd->co);
    return;
  }

  float plane[4];
  plane_from_point_normal_v3(plane, vd->co, vno);

  float smooth_closest_plane[3];
  closest_to_plane_v3(smooth_closest_plane, plane, smooth_pos);

  float final_disp[3];
  sub_v3_v3v3(final_disp, smooth_closest_plane, vd->co);

  mul_v3_fl(final_disp, factor);
  add_v3_v3v3(r_final_pos, vd->co, final_disp);
}

void relax_vertex(SculptSession &ss,
                  PBVHVertexIter *vd,
                  const float factor,
                  const bool filter_boundary_face_sets,
                  float *r_final_pos)
{
  if (SCULPT_vertex_is_boundary(ss, vd->vertex)) {
    relax_vertex_boundary(ss, vd, factor, filter_boundary_face_sets, r_final_pos);
  }
  else {
    relax_vertex_interior(ss, vd, factor, filter_boundary_face_sets, r_final_pos);
  }
}

}  // namespace blender::ed::sculpt_paint::smooth

static void do_topology_relax_task(Object &ob, const Brush &brush, PBVHNode *node)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;
  const float bstrength = ss.cache->bstrength;

  PBVHVertexIter vd;

  SculptOrigVertData orig_data = SCULPT_orig_vert_data_init(ob, *node, undo::Type::Position);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, test, brush.falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(nullptr);

  auto_mask::NodeData automask_data = auto_mask::node_begin(
      ob, ss.cache->automasking.get(), *node);

  BKE_pbvh_vertex_iter_begin (*ss.pbvh, node, vd, PBVH_ITER_UNIQUE) {
    SCULPT_orig_vert_data_update(orig_data, vd);
    if (!sculpt_brush_test_sq_fn(test, orig_data.co)) {
      continue;
    }
    auto_mask::node_update(automask_data, vd);

    const float fade = SCULPT_brush_strength_factor(ss,
                                                    brush,
                                                    orig_data.co,
                                                    sqrtf(test.dist),
                                                    orig_data.no,
                                                    nullptr,
                                                    vd.mask,
                                                    vd.vertex,
                                                    thread_id,
                                                    &automask_data);

    smooth::relax_vertex(ss, &vd, fade * bstrength, false, vd.co);
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_topology_relax_brush(const Sculpt &sd, Object &ob, Span<PBVHNode *> nodes)
{
  using namespace blender;
  SculptSession &ss = *ob.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(*ss.cache)) {
    return;
  }

  BKE_curvemapping_init(brush.curve);
  SCULPT_boundary_info_ensure(ob);
  for (int i = 0; i < 4; i++) {
    threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
      for (const int i : range) {
        do_topology_relax_task(ob, brush, nodes[i]);
      }
    });
  }
}

/** \} */
