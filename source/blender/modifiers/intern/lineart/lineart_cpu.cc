/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* \file
 * \ingroup editors
 */

#include <algorithm>

#include "MOD_lineart.hh"

#include "BLI_bounds.hh"
#include "BLI_listbase.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.h"
#include "BLI_sort.hh"
#include "BLI_task.h"
#include "BLI_time.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BKE_attribute.hh"
#include "BKE_camera.h"
#include "BKE_collection.hh"
#include "BKE_curves.hh"
#include "BKE_customdata.hh"
#include "BKE_deform.hh"
#include "BKE_geometry_set.hh"
#include "BKE_global.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_lib_id.hh"
#include "BKE_material.hh"
#include "BKE_mesh.hh"
#include "BKE_object.hh"
#include "BKE_scene.hh"

#include "DEG_depsgraph_query.hh"

#include "DNA_camera_types.h"
#include "DNA_collection_types.h"
#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "RE_pipeline.h"
#include "intern/render_types.h"

#include "ED_grease_pencil.hh"

#include "GEO_join_geometries.hh"

#include "lineart_intern.hh"

using blender::float3;
using blender::int3;
using blender::MutableSpan;
using namespace blender::bke;
using blender::bke::mesh::corner_tri_get_real_edges;

struct LineartIsecSingle {
  double v1[3], v2[3];
  LineartTriangle *tri1, *tri2;
};

struct LineartIsecThread {
  int thread_id;

  /* Scheduled work range. */
  LineartElementLinkNode *pending_from;
  LineartElementLinkNode *pending_to;
  int index_from;
  int index_to;

  /* Thread intersection result data. */
  LineartIsecSingle *array;
  int current;
  int max;
  int count_test;

  /* For individual thread reference. */
  LineartData *ld;
};

struct LineartIsecData {
  LineartData *ld;
  LineartIsecThread *threads;
  int thread_count;
};

static void lineart_bounding_area_link_edge(LineartData *ld,
                                            LineartBoundingArea *root_ba,
                                            LineartEdge *e);

static bool lineart_get_edge_bounding_areas(
    LineartData *ld, LineartEdge *e, int *rowbegin, int *rowend, int *colbegin, int *colend);

static bool lineart_triangle_edge_image_space_occlusion(const LineartTriangle *tri,
                                                        const LineartEdge *e,
                                                        const double *override_camera_loc,
                                                        const bool override_cam_is_persp,
                                                        const bool allow_overlapping_edges,
                                                        const double m_view_projection[4][4],
                                                        const double camera_dir[3],
                                                        const float cam_shift_x,
                                                        const float cam_shift_y,
                                                        double *from,
                                                        double *to);

static void lineart_bounding_area_link_triangle(LineartData *ld,
                                                LineartBoundingArea *root_ba,
                                                LineartTriangle *tri,
                                                double l_r_u_b[4],
                                                int recursive,
                                                int recursive_level,
                                                bool do_intersection,
                                                LineartIsecThread *th);

static void lineart_free_bounding_area_memory(LineartBoundingArea *ba, bool recursive);

static void lineart_free_bounding_area_memories(LineartData *ld);

static void lineart_discard_segment(LineartData *ld, LineartEdgeSegment *es)
{
  BLI_spin_lock(&ld->lock_cuts);

  memset(es, 0, sizeof(LineartEdgeSegment));

  /* Storing the node for potentially reuse the memory for new segment data.
   * Line Art data is not freed after all calculations are done. */
  BLI_addtail(&ld->wasted_cuts, es);

  BLI_spin_unlock(&ld->lock_cuts);
}

static LineartEdgeSegment *lineart_give_segment(LineartData *ld)
{
  BLI_spin_lock(&ld->lock_cuts);

  /* See if there is any already allocated memory we can reuse. */
  if (ld->wasted_cuts.first) {
    LineartEdgeSegment *es = (LineartEdgeSegment *)BLI_pophead(&ld->wasted_cuts);
    BLI_spin_unlock(&ld->lock_cuts);
    memset(es, 0, sizeof(LineartEdgeSegment));
    return es;
  }
  BLI_spin_unlock(&ld->lock_cuts);

  /* Otherwise allocate some new memory. */
  return (LineartEdgeSegment *)lineart_mem_acquire_thread(ld->edge_data_pool,
                                                          sizeof(LineartEdgeSegment));
}

void lineart_edge_cut(LineartData *ld,
                      LineartEdge *e,
                      double start,
                      double end,
                      uchar material_mask_bits,
                      uchar mat_occlusion,
                      uint32_t shadow_bits)
{
  LineartEdgeSegment *i_seg, *prev_seg;
  LineartEdgeSegment *cut_start_before = nullptr, *cut_end_before = nullptr;
  LineartEdgeSegment *new_seg1 = nullptr, *new_seg2 = nullptr;
  int untouched = 0;

  /* If for some reason the occlusion function may give a result that has zero length, or reversed
   * in direction, or NAN, we take care of them here. */
  if (LRT_DOUBLE_CLOSE_ENOUGH(start, end)) {
    return;
  }
  if (LRT_DOUBLE_CLOSE_ENOUGH(start, 1) || LRT_DOUBLE_CLOSE_ENOUGH(end, 0)) {
    return;
  }
  if (UNLIKELY(start != start)) {
    start = 0.0;
  }
  if (UNLIKELY(end != end)) {
    end = 0.0;
  }

  if (start > end) {
    double t = start;
    start = end;
    end = t;
  }

  /* Begin looking for starting position of the segment. */
  /* Not using a list iteration macro because of it more clear when using for loops to iterate
   * through the segments. */
  LISTBASE_FOREACH (LineartEdgeSegment *, seg, &e->segments) {
    if (LRT_DOUBLE_CLOSE_ENOUGH(seg->ratio, start)) {
      cut_start_before = seg;
      new_seg1 = cut_start_before;
      break;
    }
    if (seg->next == nullptr) {
      break;
    }
    i_seg = seg->next;
    if (i_seg->ratio > start + 1e-09 && start > seg->ratio) {
      cut_start_before = i_seg;
      new_seg1 = lineart_give_segment(ld);
      break;
    }
  }
  if (!cut_start_before && LRT_DOUBLE_CLOSE_ENOUGH(1, end)) {
    untouched = 1;
  }
  for (LineartEdgeSegment *seg = cut_start_before; seg; seg = seg->next) {
    /* We tried to cut ratio existing cutting point (e.g. where the line's occluded by a triangle
     * strip). */
    if (LRT_DOUBLE_CLOSE_ENOUGH(seg->ratio, end)) {
      cut_end_before = seg;
      new_seg2 = cut_end_before;
      break;
    }
    /* This check is to prevent `es->ratio == 1.0` (where we don't need to cut because we are ratio
     * the end point). */
    if (!seg->next && LRT_DOUBLE_CLOSE_ENOUGH(1, end)) {
      cut_end_before = seg;
      new_seg2 = cut_end_before;
      untouched = 1;
      break;
    }
    /* When an actual cut is needed in the line. */
    if (seg->ratio > end) {
      cut_end_before = seg;
      new_seg2 = lineart_give_segment(ld);
      break;
    }
  }

  /* When we still can't find any existing cut in the line, we allocate new ones. */
  if (new_seg1 == nullptr) {
    new_seg1 = lineart_give_segment(ld);
  }
  if (new_seg2 == nullptr) {
    if (untouched) {
      new_seg2 = new_seg1;
      cut_end_before = new_seg2;
    }
    else {
      new_seg2 = lineart_give_segment(ld);
    }
  }

  if (cut_start_before) {
    if (cut_start_before != new_seg1) {
      /* Insert cutting points for when a new cut is needed. */
      i_seg = cut_start_before->prev ? cut_start_before->prev : nullptr;
      if (i_seg) {
        new_seg1->occlusion = i_seg->occlusion;
        new_seg1->material_mask_bits = i_seg->material_mask_bits;
        new_seg1->shadow_mask_bits = i_seg->shadow_mask_bits;
      }
      BLI_insertlinkbefore(&e->segments, cut_start_before, new_seg1);
    }
    /* Otherwise we already found a existing cutting point, no need to insert a new one. */
  }
  else {
    /* We have yet to reach a existing cutting point even after we searched the whole line, so we
     * append the new cut to the end. */
    i_seg = static_cast<LineartEdgeSegment *>(e->segments.last);
    new_seg1->occlusion = i_seg->occlusion;
    new_seg1->material_mask_bits = i_seg->material_mask_bits;
    new_seg1->shadow_mask_bits = i_seg->shadow_mask_bits;
    BLI_addtail(&e->segments, new_seg1);
  }
  if (cut_end_before) {
    /* The same manipulation as on "cut_start_before". */
    if (cut_end_before != new_seg2) {
      i_seg = cut_end_before->prev ? cut_end_before->prev : nullptr;
      if (i_seg) {
        new_seg2->occlusion = i_seg->occlusion;
        new_seg2->material_mask_bits = i_seg->material_mask_bits;
        new_seg2->shadow_mask_bits = i_seg->shadow_mask_bits;
      }
      BLI_insertlinkbefore(&e->segments, cut_end_before, new_seg2);
    }
  }
  else {
    i_seg = static_cast<LineartEdgeSegment *>(e->segments.last);
    new_seg2->occlusion = i_seg->occlusion;
    new_seg2->material_mask_bits = i_seg->material_mask_bits;
    new_seg2->shadow_mask_bits = i_seg->shadow_mask_bits;
    if (!untouched) {
      BLI_addtail(&e->segments, new_seg2);
    }
  }

  /* If we touched the cut list, we assign the new cut position based on new cut position,
   * this way we accommodate precision lost due to multiple cut inserts. */
  new_seg1->ratio = start;
  if (!untouched) {
    new_seg2->ratio = end;
  }
  else {
    /* For the convenience of the loop below. */
    new_seg2 = new_seg2->next;
  }

  /* Register 1 level of occlusion for all touched segments. */
  for (LineartEdgeSegment *seg = new_seg1; seg && seg != new_seg2; seg = seg->next) {
    seg->occlusion += mat_occlusion;
    seg->material_mask_bits |= material_mask_bits;

    /* The enclosed shape flag will override regular lit/shaded
     * flags. See LineartEdgeSegment::shadow_mask_bits for details. */
    if (shadow_bits == LRT_SHADOW_MASK_ENCLOSED_SHAPE) {
      if (seg->shadow_mask_bits & LRT_SHADOW_MASK_ILLUMINATED ||
          e->flags & MOD_LINEART_EDGE_FLAG_LIGHT_CONTOUR)
      {
        seg->shadow_mask_bits |= LRT_SHADOW_MASK_INHIBITED;
      }
      else if (seg->shadow_mask_bits & LRT_SHADOW_MASK_SHADED) {
        seg->shadow_mask_bits |= LRT_SHADOW_MASK_ILLUMINATED_SHAPE;
      }
    }
    else {
      seg->shadow_mask_bits |= shadow_bits;
    }
  }

  /* Reduce adjacent cutting points of the same level, which saves memory. */
  int8_t min_occ = 127;
  prev_seg = nullptr;
  LISTBASE_FOREACH_MUTABLE (LineartEdgeSegment *, seg, &e->segments) {

    if (prev_seg && prev_seg->occlusion == seg->occlusion &&
        prev_seg->material_mask_bits == seg->material_mask_bits &&
        prev_seg->shadow_mask_bits == seg->shadow_mask_bits)
    {
      BLI_remlink(&e->segments, seg);
      /* This puts the node back to the render buffer, if more cut happens, these unused nodes get
       * picked first. */
      lineart_discard_segment(ld, seg);
      continue;
    }

    min_occ = std::min<int8_t>(min_occ, seg->occlusion);

    prev_seg = seg;
  }
  e->min_occ = min_occ;
}

/**
 * To see if given line is connected to an adjacent intersection line.
 */
BLI_INLINE bool lineart_occlusion_is_adjacent_intersection(LineartEdge *e, LineartTriangle *tri)
{
  return (((e->target_reference & LRT_LIGHT_CONTOUR_TARGET) == tri->target_reference) ||
          (((e->target_reference >> 32) & LRT_LIGHT_CONTOUR_TARGET) == tri->target_reference));
}

static void lineart_bounding_area_triangle_reallocate(LineartBoundingArea *ba)
{
  ba->max_triangle_count *= 2;
  ba->linked_triangles = static_cast<LineartTriangle **>(
      MEM_recallocN(ba->linked_triangles, sizeof(LineartTriangle *) * ba->max_triangle_count));
}

static void lineart_bounding_area_line_add(LineartBoundingArea *ba, LineartEdge *e)
{
  /* In case of too many lines concentrating in one point, do not add anymore, these lines will
   * be either shorter than a single pixel, or will still be added into the list of other less
   * dense areas. */
  if (ba->line_count >= 65535) {
    return;
  }
  if (ba->line_count >= ba->max_line_count) {
    LineartEdge **new_array = MEM_malloc_arrayN<LineartEdge *>(ba->max_line_count * 2, __func__);
    memcpy(new_array, ba->linked_lines, sizeof(LineartEdge *) * ba->max_line_count);
    ba->max_line_count *= 2;
    MEM_freeN(ba->linked_lines);
    ba->linked_lines = new_array;
  }
  ba->linked_lines[ba->line_count] = e;
  ba->line_count++;
}

static void lineart_occlusion_single_line(LineartData *ld, LineartEdge *e, int thread_id)
{
  LineartTriangleThread *tri;
  double l, r;
  LRT_EDGE_BA_MARCHING_BEGIN(e->v1->fbcoord, e->v2->fbcoord)
  {
    for (int i = 0; i < nba->triangle_count; i++) {
      tri = (LineartTriangleThread *)nba->linked_triangles[i];
      /* If we are already testing the line in this thread, then don't do it. */
      if (tri->testing_e[thread_id] == e || (tri->base.flags & LRT_TRIANGLE_INTERSECTION_ONLY) ||
          /* Ignore this triangle if an intersection line directly comes from it, */
          lineart_occlusion_is_adjacent_intersection(e, (LineartTriangle *)tri) ||
          /* Or if this triangle isn't effectively occluding anything nor it's providing a
           * material flag. */
          ((!tri->base.mat_occlusion) && (!tri->base.material_mask_bits)))
      {
        continue;
      }
      tri->testing_e[thread_id] = e;
      if (lineart_triangle_edge_image_space_occlusion((const LineartTriangle *)tri,
                                                      e,
                                                      ld->conf.camera_pos,
                                                      ld->conf.cam_is_persp,
                                                      ld->conf.allow_overlapping_edges,
                                                      ld->conf.view_projection,
                                                      ld->conf.view_vector,
                                                      ld->conf.shift_x,
                                                      ld->conf.shift_y,
                                                      &l,
                                                      &r))
      {
        lineart_edge_cut(ld, e, l, r, tri->base.material_mask_bits, tri->base.mat_occlusion, 0);
        if (e->min_occ > ld->conf.max_occlusion_level) {
          /* No need to calculate any longer on this line because no level more than set value is
           * going to show up in the rendered result. */
          return;
        }
      }
    }
    LRT_EDGE_BA_MARCHING_NEXT(e->v1->fbcoord, e->v2->fbcoord)
  }
  LRT_EDGE_BA_MARCHING_END
}

static int lineart_occlusion_make_task_info(LineartData *ld, LineartRenderTaskInfo *rti)
{
  int res = 0;
  int starting_index;

  BLI_spin_lock(&ld->lock_task);

  starting_index = ld->scheduled_count;
  ld->scheduled_count += LRT_THREAD_EDGE_COUNT;

  BLI_spin_unlock(&ld->lock_task);

  if (starting_index >= ld->pending_edges.next) {
    res = 0;
  }
  else {
    rti->pending_edges.array = &ld->pending_edges.array[starting_index];
    int remaining = ld->pending_edges.next - starting_index;
    rti->pending_edges.max = std::min(remaining, LRT_THREAD_EDGE_COUNT);
    res = 1;
  }

  return res;
}

static void lineart_occlusion_worker(TaskPool *__restrict /*pool*/, LineartRenderTaskInfo *rti)
{
  LineartData *ld = rti->ld;
  LineartEdge *eip;

  while (lineart_occlusion_make_task_info(ld, rti)) {
    for (int i = 0; i < rti->pending_edges.max; i++) {
      eip = rti->pending_edges.array[i];
      lineart_occlusion_single_line(ld, eip, rti->thread_id);
    }
  }
}

void lineart_main_occlusion_begin(LineartData *ld)
{
  int thread_count = ld->thread_count;
  LineartRenderTaskInfo *rti = MEM_calloc_arrayN<LineartRenderTaskInfo>(thread_count, __func__);
  int i;

  TaskPool *tp = BLI_task_pool_create(nullptr, TASK_PRIORITY_HIGH);

  for (i = 0; i < thread_count; i++) {
    rti[i].thread_id = i;
    rti[i].ld = ld;
    BLI_task_pool_push(tp, (TaskRunFunction)lineart_occlusion_worker, &rti[i], false, nullptr);
  }
  BLI_task_pool_work_and_wait(tp);
  BLI_task_pool_free(tp);

  MEM_freeN(rti);
}

/**
 * Test if v lies with in the triangle formed by v0, v1, and v2.
 * Returns false when v is exactly on the edge.
 *
 * For v to be inside the triangle, it needs to be at the same side of v0->v1, v1->v2, and
 * `v2->v0`, where the "side" is determined by checking the sign of `cross(v1-v0, v1-v)` and so on.
 */
static bool lineart_point_inside_triangle(const double v[2],
                                          const double v0[2],
                                          const double v1[2],
                                          const double v2[2])
{
  double cl, c, cl0;

  cl = (v0[0] - v[0]) * (v1[1] - v[1]) - (v0[1] - v[1]) * (v1[0] - v[0]);
  c = cl0 = cl;

  cl = (v1[0] - v[0]) * (v2[1] - v[1]) - (v1[1] - v[1]) * (v2[0] - v[0]);
  if (c * cl <= 0) {
    return false;
  }

  c = cl;

  cl = (v2[0] - v[0]) * (v0[1] - v[1]) - (v2[1] - v[1]) * (v0[0] - v[0]);
  if (c * cl <= 0) {
    return false;
  }

  c = cl;

  if (c * cl0 <= 0) {
    return false;
  }

  return true;
}

static int lineart_point_on_line_segment(double v[2], double v0[2], double v1[2])
{
  /* `c1 != c2` by default. */
  double c1 = 1, c2 = 0;
  double l0[2], l1[2];

  sub_v2_v2v2_db(l0, v, v0);
  sub_v2_v2v2_db(l1, v, v1);

  if (v1[0] == v0[0] && v1[1] == v0[1]) {
    return 0;
  }

  if (!LRT_DOUBLE_CLOSE_ENOUGH(v1[0], v0[0])) {
    c1 = ratiod(v0[0], v1[0], v[0]);
  }
  else {
    if (LRT_DOUBLE_CLOSE_ENOUGH(v[0], v1[0])) {
      c2 = ratiod(v0[1], v1[1], v[1]);
      return (c2 >= -DBL_TRIANGLE_LIM && c2 <= 1 + DBL_TRIANGLE_LIM);
    }
    return false;
  }

  if (!LRT_DOUBLE_CLOSE_ENOUGH(v1[1], v0[1])) {
    c2 = ratiod(v0[1], v1[1], v[1]);
  }
  else {
    if (LRT_DOUBLE_CLOSE_ENOUGH(v[1], v1[1])) {
      c1 = ratiod(v0[0], v1[0], v[0]);
      return (c1 >= -DBL_TRIANGLE_LIM && c1 <= 1 + DBL_TRIANGLE_LIM);
    }
    return false;
  }

  if (LRT_DOUBLE_CLOSE_ENOUGH(c1, c2) && c1 >= 0 && c1 <= 1) {
    return 1;
  }

  return 0;
}

enum LineartPointTri {
  LRT_OUTSIDE_TRIANGLE = 0,
  LRT_ON_TRIANGLE = 1,
  LRT_INSIDE_TRIANGLE = 2,
};

/**
 * Same algorithm as lineart_point_inside_triangle(), but returns differently:
 * 0-outside 1-on the edge 2-inside.
 */
static LineartPointTri lineart_point_triangle_relation(double v[2],
                                                       double v0[2],
                                                       double v1[2],
                                                       double v2[2])
{
  double cl, c;
  double r;
  if (lineart_point_on_line_segment(v, v0, v1) || lineart_point_on_line_segment(v, v1, v2) ||
      lineart_point_on_line_segment(v, v2, v0))
  {
    return LRT_ON_TRIANGLE;
  }

  cl = (v0[0] - v[0]) * (v1[1] - v[1]) - (v0[1] - v[1]) * (v1[0] - v[0]);
  c = cl;

  cl = (v1[0] - v[0]) * (v2[1] - v[1]) - (v1[1] - v[1]) * (v2[0] - v[0]);
  if ((r = c * cl) < 0) {
    return LRT_OUTSIDE_TRIANGLE;
  }

  c = cl;

  cl = (v2[0] - v[0]) * (v0[1] - v[1]) - (v2[1] - v[1]) * (v0[0] - v[0]);
  if ((r = c * cl) < 0) {
    return LRT_OUTSIDE_TRIANGLE;
  }

  c = cl;

  cl = (v0[0] - v[0]) * (v1[1] - v[1]) - (v0[1] - v[1]) * (v1[0] - v[0]);
  if ((r = c * cl) < 0) {
    return LRT_OUTSIDE_TRIANGLE;
  }

  if (r == 0) {
    return LRT_ON_TRIANGLE;
  }

  return LRT_INSIDE_TRIANGLE;
}

/**
 * Similar with #lineart_point_inside_triangle, but in 3d.
 * Returns false when not co-planar.
 */
static bool lineart_point_inside_triangle3d(double v[3], double v0[3], double v1[3], double v2[3])
{
  double l[3], r[3];
  double N1[3], N2[3];
  double d;

  sub_v3_v3v3_db(l, v1, v0);
  sub_v3_v3v3_db(r, v, v1);
  cross_v3_v3v3_db(N1, l, r);

  sub_v3_v3v3_db(l, v2, v1);
  sub_v3_v3v3_db(r, v, v2);
  cross_v3_v3v3_db(N2, l, r);

  if ((d = dot_v3v3_db(N1, N2)) < 0) {
    return false;
  }

  sub_v3_v3v3_db(l, v0, v2);
  sub_v3_v3v3_db(r, v, v0);
  cross_v3_v3v3_db(N1, l, r);

  if ((d = dot_v3v3_db(N1, N2)) < 0) {
    return false;
  }

  sub_v3_v3v3_db(l, v1, v0);
  sub_v3_v3v3_db(r, v, v1);
  cross_v3_v3v3_db(N2, l, r);

  if ((d = dot_v3v3_db(N1, N2)) < 0) {
    return false;
  }

  return true;
}

/**
 * The following `lineart_memory_get_XXX_space` functions are for allocating new memory for some
 * modified geometries in the culling stage.
 */
static LineartElementLinkNode *lineart_memory_get_triangle_space(LineartData *ld)
{
  /* We don't need to allocate a whole bunch of triangles because the amount of clipped triangles
   * are relatively small. */
  LineartTriangle *render_triangles = static_cast<LineartTriangle *>(
      lineart_mem_acquire(&ld->render_data_pool, 64 * ld->sizeof_triangle));

  LineartElementLinkNode *eln = static_cast<LineartElementLinkNode *>(
      lineart_list_append_pointer_pool_sized(&ld->geom.triangle_buffer_pointers,
                                             &ld->render_data_pool,
                                             render_triangles,
                                             sizeof(LineartElementLinkNode)));
  eln->element_count = 64;
  eln->flags |= LRT_ELEMENT_IS_ADDITIONAL;

  return eln;
}

static LineartElementLinkNode *lineart_memory_get_vert_space(LineartData *ld)
{
  LineartVert *render_vertices = static_cast<LineartVert *>(
      lineart_mem_acquire(&ld->render_data_pool, sizeof(LineartVert) * 64));

  LineartElementLinkNode *eln = static_cast<LineartElementLinkNode *>(
      lineart_list_append_pointer_pool_sized(&ld->geom.vertex_buffer_pointers,
                                             &ld->render_data_pool,
                                             render_vertices,
                                             sizeof(LineartElementLinkNode)));
  eln->element_count = 64;
  eln->flags |= LRT_ELEMENT_IS_ADDITIONAL;

  return eln;
}

static LineartElementLinkNode *lineart_memory_get_edge_space(LineartData *ld)
{
  LineartEdge *render_edges = static_cast<LineartEdge *>(
      lineart_mem_acquire(ld->edge_data_pool, sizeof(LineartEdge) * 64));

  LineartElementLinkNode *eln = static_cast<LineartElementLinkNode *>(
      lineart_list_append_pointer_pool_sized(&ld->geom.line_buffer_pointers,
                                             ld->edge_data_pool,
                                             render_edges,
                                             sizeof(LineartElementLinkNode)));
  eln->element_count = 64;
  eln->crease_threshold = ld->conf.crease_threshold;
  eln->flags |= LRT_ELEMENT_IS_ADDITIONAL;

  return eln;
}

static void lineart_triangle_post(LineartTriangle *tri, LineartTriangle *orig)
{
  /* Just re-assign normal and set cull flag. */
  copy_v3_v3_db(tri->gn, orig->gn);
  tri->flags = LRT_CULL_GENERATED;
  tri->intersection_mask = orig->intersection_mask;
  tri->material_mask_bits = orig->material_mask_bits;
  tri->mat_occlusion = orig->mat_occlusion;
  tri->intersection_priority = orig->intersection_priority;
  tri->target_reference = orig->target_reference;
}

static void lineart_triangle_set_cull_flag(LineartTriangle *tri, uchar flag)
{
  uchar intersection_only = (tri->flags & LRT_TRIANGLE_INTERSECTION_ONLY);
  tri->flags = flag;
  tri->flags |= intersection_only;
}

static bool lineart_edge_match(LineartTriangle *tri, LineartEdge *e, int v1, int v2)
{
  return ((tri->v[v1] == e->v1 && tri->v[v2] == e->v2) ||
          (tri->v[v2] == e->v1 && tri->v[v1] == e->v2));
}

static void lineart_discard_duplicated_edges(LineartEdge *old_e)
{
  LineartEdge *e = old_e;
  while (e->flags & MOD_LINEART_EDGE_FLAG_NEXT_IS_DUPLICATION) {
    e++;
    e->flags |= MOD_LINEART_EDGE_FLAG_CHAIN_PICKED;
  }
}

/**
 * Does near-plane cut on 1 triangle only. When cutting with far-plane, the camera vectors gets
 * reversed by the caller so don't need to implement one in a different direction.
 */
static void lineart_triangle_cull_single(LineartData *ld,
                                         LineartTriangle *tri,
                                         int in0,
                                         int in1,
                                         int in2,
                                         double cam_pos[3],
                                         double view_dir[3],
                                         bool allow_boundaries,
                                         double m_view_projection[4][4],
                                         Object *ob,
                                         int *r_v_count,
                                         int *r_e_count,
                                         int *r_t_count,
                                         LineartElementLinkNode *v_eln,
                                         LineartElementLinkNode *e_eln,
                                         LineartElementLinkNode *t_eln)
{
  double span_v1[3], span_v2[3], dot_v1, dot_v2;
  double a;
  int v_count = *r_v_count;
  int e_count = *r_e_count;
  int t_count = *r_t_count;
  uint16_t new_flag = 0;

  LineartEdge *new_e, *e, *old_e;
  LineartEdgeSegment *es;

  if (tri->flags & (LRT_CULL_USED | LRT_CULL_GENERATED | LRT_CULL_DISCARD)) {
    return;
  }

  /* See definition of tri->intersecting_verts and the usage in
   * lineart_geometry_object_load() for details. */
  LineartTriangleAdjacent *tri_adj = reinterpret_cast<LineartTriangleAdjacent *>(
      tri->intersecting_verts);

  LineartVert *vt = &((LineartVert *)v_eln->pointer)[v_count];
  LineartTriangle *tri1 = static_cast<LineartTriangle *>(
      (void *)(((uchar *)t_eln->pointer) + ld->sizeof_triangle * t_count));
  LineartTriangle *tri2 = static_cast<LineartTriangle *>(
      (void *)(((uchar *)t_eln->pointer) + ld->sizeof_triangle * (t_count + 1)));

  new_e = &((LineartEdge *)e_eln->pointer)[e_count];
  /* Init `edge` to the last `edge` entry. */
  e = new_e;

#define INCREASE_EDGE \
  new_e = &((LineartEdge *)e_eln->pointer)[e_count]; \
  e_count++; \
  e = new_e; \
  es = static_cast<LineartEdgeSegment *>( \
      lineart_mem_acquire(&ld->render_data_pool, sizeof(LineartEdgeSegment))); \
  BLI_addtail(&e->segments, es);

#define SELECT_EDGE(e_num, v1_link, v2_link, new_tri) \
  if (tri_adj->e[e_num]) { \
    old_e = tri_adj->e[e_num]; \
    new_flag = old_e->flags; \
    old_e->flags = MOD_LINEART_EDGE_FLAG_CHAIN_PICKED; \
    lineart_discard_duplicated_edges(old_e); \
    INCREASE_EDGE \
    e->v1 = (v1_link); \
    e->v2 = (v2_link); \
    e->v1->index = (v1_link)->index; \
    e->v2->index = (v1_link)->index; \
    e->flags = new_flag; \
    e->object_ref = ob; \
    e->t1 = ((old_e->t1 == tri) ? (new_tri) : (old_e->t1)); \
    e->t2 = ((old_e->t2 == tri) ? (new_tri) : (old_e->t2)); \
    lineart_add_edge_to_array(&ld->pending_edges, e); \
  }

#define RELINK_EDGE(e_num, new_tri) \
  if (tri_adj->e[e_num]) { \
    old_e = tri_adj->e[e_num]; \
    old_e->t1 = ((old_e->t1 == tri) ? (new_tri) : (old_e->t1)); \
    old_e->t2 = ((old_e->t2 == tri) ? (new_tri) : (old_e->t2)); \
  }

#define REMOVE_TRIANGLE_EDGE \
  if (tri_adj->e[0]) { \
    tri_adj->e[0]->flags = MOD_LINEART_EDGE_FLAG_CHAIN_PICKED; \
    lineart_discard_duplicated_edges(tri_adj->e[0]); \
  } \
  if (tri_adj->e[1]) { \
    tri_adj->e[1]->flags = MOD_LINEART_EDGE_FLAG_CHAIN_PICKED; \
    lineart_discard_duplicated_edges(tri_adj->e[1]); \
  } \
  if (tri_adj->e[2]) { \
    tri_adj->e[2]->flags = MOD_LINEART_EDGE_FLAG_CHAIN_PICKED; \
    lineart_discard_duplicated_edges(tri_adj->e[2]); \
  }

  switch (in0 + in1 + in2) {
    case 0: /* Triangle is visible. Ignore this triangle. */
      return;
    case 3:
      /* Triangle completely behind near plane, throw it away
       * also remove render lines form being computed. */
      lineart_triangle_set_cull_flag(tri, LRT_CULL_DISCARD);
      REMOVE_TRIANGLE_EDGE
      return;
    case 2:
      /* Two points behind near plane, cut those and
       * generate 2 new points, 3 lines and 1 triangle. */
      lineart_triangle_set_cull_flag(tri, LRT_CULL_USED);

      /**
       * (!in0) means "when point 0 is visible".
       * conditions for point 1, 2 are the same idea.
       *
       * \code{.txt}identify
       * 1-----|-------0
       * |     |   ---
       * |     |---
       * |  ---|
       * 2--   |
       *     (near)---------->(far)
       * Will become:
       *       |N******0
       *       |*  ***
       *       |N**
       *       |
       *       |
       *     (near)---------->(far)
       * \endcode
       */
      if (!in0) {

        /* Cut point for line 2---|-----0. */
        sub_v3_v3v3_db(span_v1, tri->v[0]->gloc, cam_pos);
        sub_v3_v3v3_db(span_v2, cam_pos, tri->v[2]->gloc);
        dot_v1 = dot_v3v3_db(span_v1, view_dir);
        dot_v2 = dot_v3v3_db(span_v2, view_dir);
        a = dot_v1 / (dot_v1 + dot_v2);
        /* Assign it to a new point. */
        interp_v3_v3v3_db(vt[0].gloc, tri->v[0]->gloc, tri->v[2]->gloc, a);
        mul_v4_m4v3_db(vt[0].fbcoord, m_view_projection, vt[0].gloc);
        vt[0].index = tri->v[2]->index;

        /* Cut point for line 1---|-----0. */
        sub_v3_v3v3_db(span_v1, tri->v[0]->gloc, cam_pos);
        sub_v3_v3v3_db(span_v2, cam_pos, tri->v[1]->gloc);
        dot_v1 = dot_v3v3_db(span_v1, view_dir);
        dot_v2 = dot_v3v3_db(span_v2, view_dir);
        a = dot_v1 / (dot_v1 + dot_v2);
        /* Assign it to another new point. */
        interp_v3_v3v3_db(vt[1].gloc, tri->v[0]->gloc, tri->v[1]->gloc, a);
        mul_v4_m4v3_db(vt[1].fbcoord, m_view_projection, vt[1].gloc);
        vt[1].index = tri->v[1]->index;

        /* New line connecting two new points. */
        INCREASE_EDGE
        if (allow_boundaries) {
          e->flags = MOD_LINEART_EDGE_FLAG_CONTOUR;
          lineart_add_edge_to_array(&ld->pending_edges, e);
        }
        /* NOTE: inverting `e->v1/v2` (left/right point) doesn't matter as long as
         * `tri->edge` and `tri->v` has the same sequence. and the winding direction
         * can be either CW or CCW but needs to be consistent throughout the calculation. */
        e->v1 = &vt[1];
        e->v2 = &vt[0];
        /* Only one adjacent triangle, because the other side is the near plane. */
        /* Use `tl` or `tr` doesn't matter. */
        e->t1 = tri1;
        e->object_ref = ob;

        /* New line connecting original point 0 and a new point, only when it's a selected line. */
        SELECT_EDGE(2, tri->v[0], &vt[0], tri1)
        /* New line connecting original point 0 and another new point. */
        SELECT_EDGE(0, tri->v[0], &vt[1], tri1)

        /* Re-assign triangle point array to two new points. */
        tri1->v[0] = tri->v[0];
        tri1->v[1] = &vt[1];
        tri1->v[2] = &vt[0];

        lineart_triangle_post(tri1, tri);

        v_count += 2;
        t_count += 1;
      }
      else if (!in2) {
        sub_v3_v3v3_db(span_v1, tri->v[2]->gloc, cam_pos);
        sub_v3_v3v3_db(span_v2, cam_pos, tri->v[0]->gloc);
        dot_v1 = dot_v3v3_db(span_v1, view_dir);
        dot_v2 = dot_v3v3_db(span_v2, view_dir);
        a = dot_v1 / (dot_v1 + dot_v2);
        interp_v3_v3v3_db(vt[0].gloc, tri->v[2]->gloc, tri->v[0]->gloc, a);
        mul_v4_m4v3_db(vt[0].fbcoord, m_view_projection, vt[0].gloc);
        vt[0].index = tri->v[0]->index;

        sub_v3_v3v3_db(span_v1, tri->v[2]->gloc, cam_pos);
        sub_v3_v3v3_db(span_v2, cam_pos, tri->v[1]->gloc);
        dot_v1 = dot_v3v3_db(span_v1, view_dir);
        dot_v2 = dot_v3v3_db(span_v2, view_dir);
        a = dot_v1 / (dot_v1 + dot_v2);
        interp_v3_v3v3_db(vt[1].gloc, tri->v[2]->gloc, tri->v[1]->gloc, a);
        mul_v4_m4v3_db(vt[1].fbcoord, m_view_projection, vt[1].gloc);
        vt[1].index = tri->v[1]->index;

        INCREASE_EDGE
        if (allow_boundaries) {
          e->flags = MOD_LINEART_EDGE_FLAG_CONTOUR;
          lineart_add_edge_to_array(&ld->pending_edges, e);
        }
        e->v1 = &vt[0];
        e->v2 = &vt[1];
        e->t1 = tri1;
        e->object_ref = ob;

        SELECT_EDGE(2, tri->v[2], &vt[0], tri1)
        SELECT_EDGE(1, tri->v[2], &vt[1], tri1)

        tri1->v[0] = &vt[0];
        tri1->v[1] = &vt[1];
        tri1->v[2] = tri->v[2];

        lineart_triangle_post(tri1, tri);

        v_count += 2;
        t_count += 1;
      }
      else if (!in1) {
        sub_v3_v3v3_db(span_v1, tri->v[1]->gloc, cam_pos);
        sub_v3_v3v3_db(span_v2, cam_pos, tri->v[2]->gloc);
        dot_v1 = dot_v3v3_db(span_v1, view_dir);
        dot_v2 = dot_v3v3_db(span_v2, view_dir);
        a = dot_v1 / (dot_v1 + dot_v2);
        interp_v3_v3v3_db(vt[0].gloc, tri->v[1]->gloc, tri->v[2]->gloc, a);
        mul_v4_m4v3_db(vt[0].fbcoord, m_view_projection, vt[0].gloc);
        vt[0].index = tri->v[2]->index;

        sub_v3_v3v3_db(span_v1, tri->v[1]->gloc, cam_pos);
        sub_v3_v3v3_db(span_v2, cam_pos, tri->v[0]->gloc);
        dot_v1 = dot_v3v3_db(span_v1, view_dir);
        dot_v2 = dot_v3v3_db(span_v2, view_dir);
        a = dot_v1 / (dot_v1 + dot_v2);
        interp_v3_v3v3_db(vt[1].gloc, tri->v[1]->gloc, tri->v[0]->gloc, a);
        mul_v4_m4v3_db(vt[1].fbcoord, m_view_projection, vt[1].gloc);
        vt[1].index = tri->v[0]->index;

        INCREASE_EDGE
        if (allow_boundaries) {
          e->flags = MOD_LINEART_EDGE_FLAG_CONTOUR;
          lineart_add_edge_to_array(&ld->pending_edges, e);
        }
        e->v1 = &vt[1];
        e->v2 = &vt[0];
        e->t1 = tri1;
        e->object_ref = ob;

        SELECT_EDGE(1, tri->v[1], &vt[0], tri1)
        SELECT_EDGE(0, tri->v[1], &vt[1], tri1)

        tri1->v[0] = &vt[0];
        tri1->v[1] = tri->v[1];
        tri1->v[2] = &vt[1];

        lineart_triangle_post(tri1, tri);

        v_count += 2;
        t_count += 1;
      }
      break;
    case 1:
      /* One point behind near plane, cut those and
       * generate 2 new points, 4 lines and 2 triangles. */
      lineart_triangle_set_cull_flag(tri, LRT_CULL_USED);

      /**
       * (in0) means "when point 0 is invisible".
       * conditions for point 1, 2 are the same idea.
       * \code{.txt}
       * 0------|----------1
       *   --   |          |
       *     ---|          |
       *        |--        |
       *        |  ---     |
       *        |     ---  |
       *        |        --2
       *      (near)---------->(far)
       * Will become:
       *        |N*********1
       *        |*     *** |
       *        |*  ***    |
       *        |N**       |
       *        |  ***     |
       *        |     ***  |
       *        |        **2
       *      (near)---------->(far)
       * \endcode
       */
      if (in0) {
        /* Cut point for line 0---|------1. */
        sub_v3_v3v3_db(span_v1, tri->v[1]->gloc, cam_pos);
        sub_v3_v3v3_db(span_v2, cam_pos, tri->v[0]->gloc);
        dot_v1 = dot_v3v3_db(span_v1, view_dir);
        dot_v2 = dot_v3v3_db(span_v2, view_dir);
        a = dot_v2 / (dot_v1 + dot_v2);
        /* Assign to a new point. */
        interp_v3_v3v3_db(vt[0].gloc, tri->v[0]->gloc, tri->v[1]->gloc, a);
        mul_v4_m4v3_db(vt[0].fbcoord, m_view_projection, vt[0].gloc);
        vt[0].index = tri->v[0]->index;

        /* Cut point for line 0---|------2. */
        sub_v3_v3v3_db(span_v1, tri->v[2]->gloc, cam_pos);
        sub_v3_v3v3_db(span_v2, cam_pos, tri->v[0]->gloc);
        dot_v1 = dot_v3v3_db(span_v1, view_dir);
        dot_v2 = dot_v3v3_db(span_v2, view_dir);
        a = dot_v2 / (dot_v1 + dot_v2);
        /* Assign to other new point. */
        interp_v3_v3v3_db(vt[1].gloc, tri->v[0]->gloc, tri->v[2]->gloc, a);
        mul_v4_m4v3_db(vt[1].fbcoord, m_view_projection, vt[1].gloc);
        vt[1].index = tri->v[0]->index;

        /* New line connects two new points. */
        INCREASE_EDGE
        if (allow_boundaries) {
          e->flags = MOD_LINEART_EDGE_FLAG_CONTOUR;
          lineart_add_edge_to_array(&ld->pending_edges, e);
        }
        e->v1 = &vt[1];
        e->v2 = &vt[0];
        e->t1 = tri1;
        e->object_ref = ob;

        /* New line connects new point 0 and old point 1,
         * this is a border line. */

        SELECT_EDGE(0, tri->v[1], &vt[0], tri1)
        SELECT_EDGE(2, tri->v[2], &vt[1], tri2)
        RELINK_EDGE(1, tri2)

        /* We now have one triangle closed. */
        tri1->v[0] = tri->v[1];
        tri1->v[1] = &vt[1];
        tri1->v[2] = &vt[0];
        /* Close the second triangle. */
        tri2->v[0] = &vt[1];
        tri2->v[1] = tri->v[1];
        tri2->v[2] = tri->v[2];

        lineart_triangle_post(tri1, tri);
        lineart_triangle_post(tri2, tri);

        v_count += 2;
        t_count += 2;
      }
      else if (in1) {

        sub_v3_v3v3_db(span_v1, tri->v[1]->gloc, cam_pos);
        sub_v3_v3v3_db(span_v2, cam_pos, tri->v[2]->gloc);
        dot_v1 = dot_v3v3_db(span_v1, view_dir);
        dot_v2 = dot_v3v3_db(span_v2, view_dir);
        a = dot_v1 / (dot_v1 + dot_v2);
        interp_v3_v3v3_db(vt[0].gloc, tri->v[1]->gloc, tri->v[2]->gloc, a);
        mul_v4_m4v3_db(vt[0].fbcoord, m_view_projection, vt[0].gloc);
        vt[0].index = tri->v[1]->index;

        sub_v3_v3v3_db(span_v1, tri->v[1]->gloc, cam_pos);
        sub_v3_v3v3_db(span_v2, cam_pos, tri->v[0]->gloc);
        dot_v1 = dot_v3v3_db(span_v1, view_dir);
        dot_v2 = dot_v3v3_db(span_v2, view_dir);
        a = dot_v1 / (dot_v1 + dot_v2);
        interp_v3_v3v3_db(vt[1].gloc, tri->v[1]->gloc, tri->v[0]->gloc, a);
        mul_v4_m4v3_db(vt[1].fbcoord, m_view_projection, vt[1].gloc);
        vt[1].index = tri->v[1]->index;

        INCREASE_EDGE
        if (allow_boundaries) {
          e->flags = MOD_LINEART_EDGE_FLAG_CONTOUR;
          lineart_add_edge_to_array(&ld->pending_edges, e);
        }
        e->v1 = &vt[1];
        e->v2 = &vt[0];

        e->t1 = tri1;
        e->object_ref = ob;

        SELECT_EDGE(1, tri->v[2], &vt[0], tri1)
        SELECT_EDGE(0, tri->v[0], &vt[1], tri2)
        RELINK_EDGE(2, tri2)

        tri1->v[0] = tri->v[2];
        tri1->v[1] = &vt[1];
        tri1->v[2] = &vt[0];

        tri2->v[0] = &vt[1];
        tri2->v[1] = tri->v[2];
        tri2->v[2] = tri->v[0];

        lineart_triangle_post(tri1, tri);
        lineart_triangle_post(tri2, tri);

        v_count += 2;
        t_count += 2;
      }
      else if (in2) {

        sub_v3_v3v3_db(span_v1, tri->v[2]->gloc, cam_pos);
        sub_v3_v3v3_db(span_v2, cam_pos, tri->v[0]->gloc);
        dot_v1 = dot_v3v3_db(span_v1, view_dir);
        dot_v2 = dot_v3v3_db(span_v2, view_dir);
        a = dot_v1 / (dot_v1 + dot_v2);
        interp_v3_v3v3_db(vt[0].gloc, tri->v[2]->gloc, tri->v[0]->gloc, a);
        mul_v4_m4v3_db(vt[0].fbcoord, m_view_projection, vt[0].gloc);
        vt[0].index = tri->v[2]->index;

        sub_v3_v3v3_db(span_v1, tri->v[2]->gloc, cam_pos);
        sub_v3_v3v3_db(span_v2, cam_pos, tri->v[1]->gloc);
        dot_v1 = dot_v3v3_db(span_v1, view_dir);
        dot_v2 = dot_v3v3_db(span_v2, view_dir);
        a = dot_v1 / (dot_v1 + dot_v2);
        interp_v3_v3v3_db(vt[1].gloc, tri->v[2]->gloc, tri->v[1]->gloc, a);
        mul_v4_m4v3_db(vt[1].fbcoord, m_view_projection, vt[1].gloc);
        vt[1].index = tri->v[2]->index;

        INCREASE_EDGE
        if (allow_boundaries) {
          e->flags = MOD_LINEART_EDGE_FLAG_CONTOUR;
          lineart_add_edge_to_array(&ld->pending_edges, e);
        }
        e->v1 = &vt[1];
        e->v2 = &vt[0];

        e->t1 = tri1;
        e->object_ref = ob;

        SELECT_EDGE(2, tri->v[0], &vt[0], tri1)
        SELECT_EDGE(1, tri->v[1], &vt[1], tri2)
        RELINK_EDGE(0, tri2)

        tri1->v[0] = tri->v[0];
        tri1->v[1] = &vt[1];
        tri1->v[2] = &vt[0];

        tri2->v[0] = &vt[1];
        tri2->v[1] = tri->v[0];
        tri2->v[2] = tri->v[1];

        lineart_triangle_post(tri1, tri);
        lineart_triangle_post(tri2, tri);

        v_count += 2;
        t_count += 2;
      }
      break;
  }
  *r_v_count = v_count;
  *r_e_count = e_count;
  *r_t_count = t_count;

#undef INCREASE_EDGE
#undef SELECT_EDGE
#undef RELINK_EDGE
#undef REMOVE_TRIANGLE_EDGE
}

void lineart_main_cull_triangles(LineartData *ld, bool clip_far)
{
  LineartTriangle *tri;
  LineartElementLinkNode *v_eln, *t_eln, *e_eln;
  double (*m_view_projection)[4] = ld->conf.view_projection;
  int i;
  int v_count = 0, t_count = 0, e_count = 0;
  Object *ob;
  bool allow_boundaries = ld->conf.allow_boundaries;
  double cam_pos[3];
  double clip_start = ld->conf.near_clip, clip_end = ld->conf.far_clip;
  double view_dir[3], clip_advance[3];

  copy_v3_v3_db(view_dir, ld->conf.view_vector);
  copy_v3_v3_db(clip_advance, ld->conf.view_vector);
  copy_v3_v3_db(cam_pos, ld->conf.camera_pos);

  if (clip_far) {
    /* Move starting point to end plane. */
    mul_v3db_db(clip_advance, -clip_end);
    add_v3_v3_db(cam_pos, clip_advance);

    /* "reverse looking". */
    mul_v3db_db(view_dir, -1.0f);
  }
  else {
    /* Clip Near. */
    mul_v3db_db(clip_advance, -clip_start);
    add_v3_v3_db(cam_pos, clip_advance);
  }

  v_eln = lineart_memory_get_vert_space(ld);
  t_eln = lineart_memory_get_triangle_space(ld);
  e_eln = lineart_memory_get_edge_space(ld);

  /* Additional memory space for storing generated points and triangles. */
#define LRT_CULL_ENSURE_MEMORY \
  if (v_count > 60) { \
    v_eln->element_count = v_count; \
    v_eln = lineart_memory_get_vert_space(ld); \
    v_count = 0; \
  } \
  if (t_count > 60) { \
    t_eln->element_count = t_count; \
    t_eln = lineart_memory_get_triangle_space(ld); \
    t_count = 0; \
  } \
  if (e_count > 60) { \
    e_eln->element_count = e_count; \
    e_eln = lineart_memory_get_edge_space(ld); \
    e_count = 0; \
  }

#define LRT_CULL_DECIDE_INSIDE \
  /* These three represents points that are in the clipping range or not. */ \
  in0 = 0, in1 = 0, in2 = 0; \
  if (clip_far) { \
    /* Point outside far plane. */ \
    if (tri->v[0]->fbcoord[use_w] > clip_end) { \
      in0 = 1; \
    } \
    if (tri->v[1]->fbcoord[use_w] > clip_end) { \
      in1 = 1; \
    } \
    if (tri->v[2]->fbcoord[use_w] > clip_end) { \
      in2 = 1; \
    } \
  } \
  else { \
    /* Point inside near plane. */ \
    if (tri->v[0]->fbcoord[use_w] < clip_start) { \
      in0 = 1; \
    } \
    if (tri->v[1]->fbcoord[use_w] < clip_start) { \
      in1 = 1; \
    } \
    if (tri->v[2]->fbcoord[use_w] < clip_start) { \
      in2 = 1; \
    } \
  }

  int use_w = 3;
  int in0 = 0, in1 = 0, in2 = 0;

  if (!ld->conf.cam_is_persp) {
    clip_start = -1;
    clip_end = 1;
    use_w = 2;
  }

  /* Then go through all the other triangles. */
  LISTBASE_FOREACH (LineartElementLinkNode *, eln, &ld->geom.triangle_buffer_pointers) {
    if (eln->flags & LRT_ELEMENT_IS_ADDITIONAL) {
      continue;
    }
    ob = static_cast<Object *>(eln->object_ref);
    for (i = 0; i < eln->element_count; i++) {
      /* Select the triangle in the array. */
      tri = static_cast<LineartTriangle *>(
          (void *)(((uchar *)eln->pointer) + ld->sizeof_triangle * i));

      if (tri->flags & LRT_CULL_DISCARD) {
        continue;
      }

      LRT_CULL_DECIDE_INSIDE
      LRT_CULL_ENSURE_MEMORY
      lineart_triangle_cull_single(ld,
                                   tri,
                                   in0,
                                   in1,
                                   in2,
                                   cam_pos,
                                   view_dir,
                                   allow_boundaries,
                                   m_view_projection,
                                   ob,
                                   &v_count,
                                   &e_count,
                                   &t_count,
                                   v_eln,
                                   e_eln,
                                   t_eln);
    }
    t_eln->element_count = t_count;
    v_eln->element_count = v_count;
  }

#undef LRT_CULL_ENSURE_MEMORY
#undef LRT_CULL_DECIDE_INSIDE
}

void lineart_main_free_adjacent_data(LineartData *ld)
{
  while (
      LinkData *link = static_cast<LinkData *>(BLI_pophead(&ld->geom.triangle_adjacent_pointers)))
  {
    MEM_freeN(link->data);
  }
  LISTBASE_FOREACH (LineartElementLinkNode *, eln, &ld->geom.triangle_buffer_pointers) {
    LineartTriangle *tri = static_cast<LineartTriangle *>(eln->pointer);
    int i;
    for (i = 0; i < eln->element_count; i++) {
      /* See definition of tri->intersecting_verts and the usage in
       * lineart_geometry_object_load() for detailed. */
      tri->intersecting_verts = nullptr;
      tri = (LineartTriangle *)(((uchar *)tri) + ld->sizeof_triangle);
    }
  }
}

void lineart_main_perspective_division(LineartData *ld)
{
  LISTBASE_FOREACH (LineartElementLinkNode *, eln, &ld->geom.vertex_buffer_pointers) {
    LineartVert *vt = static_cast<LineartVert *>(eln->pointer);
    for (int i = 0; i < eln->element_count; i++) {
      if (ld->conf.cam_is_persp) {
        /* Do not divide Z, we use Z to back transform cut points in later chaining process. */
        vt[i].fbcoord[0] /= vt[i].fbcoord[3];
        vt[i].fbcoord[1] /= vt[i].fbcoord[3];
        /* Re-map z into (0-1) range, because we no longer need NDC (Normalized Device Coordinates)
         * at the moment.
         * The algorithm currently doesn't need Z for operation, we use W instead. If Z is needed
         * in the future, the line below correctly transforms it to view space coordinates. */
        // `vt[i].fbcoord[2] = -2 * vt[i].fbcoord[2] / (far - near) - (far + near) / (far - near);
      }
      /* Shifting is always needed. */
      vt[i].fbcoord[0] -= ld->conf.shift_x * 2;
      vt[i].fbcoord[1] -= ld->conf.shift_y * 2;
    }
  }
}

void lineart_main_discard_out_of_frame_edges(LineartData *ld)
{
  LineartEdge *e;
  const float bounds[4][2] = {{-1.0f, -1.0f}, {-1.0f, 1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}};

#define LRT_VERT_OUT_OF_BOUND(v) \
  (v->fbcoord[0] < -1 || v->fbcoord[0] > 1 || v->fbcoord[1] < -1 || v->fbcoord[1] > 1)

  LISTBASE_FOREACH (LineartElementLinkNode *, eln, &ld->geom.line_buffer_pointers) {
    e = (LineartEdge *)eln->pointer;
    for (int i = 0; i < eln->element_count; i++) {
      if (!e[i].v1 || !e[i].v2) {
        e[i].flags = MOD_LINEART_EDGE_FLAG_CHAIN_PICKED;
        continue;
      }
      const blender::float2 vec1(e[i].v1->fbcoord), vec2(e[i].v2->fbcoord);
      if (LRT_VERT_OUT_OF_BOUND(e[i].v1) && LRT_VERT_OUT_OF_BOUND(e[i].v2)) {
        /* A line could still cross the image border even when both of the vertices are out of
         * bound. */
        if (isect_seg_seg_v2(bounds[0], bounds[1], vec1, vec2) == ISECT_LINE_LINE_NONE &&
            isect_seg_seg_v2(bounds[0], bounds[2], vec1, vec2) == ISECT_LINE_LINE_NONE &&
            isect_seg_seg_v2(bounds[1], bounds[3], vec1, vec2) == ISECT_LINE_LINE_NONE &&
            isect_seg_seg_v2(bounds[2], bounds[3], vec1, vec2) == ISECT_LINE_LINE_NONE)
        {
          e[i].flags = MOD_LINEART_EDGE_FLAG_CHAIN_PICKED;
        }
      }
    }
  }
}

struct LineartEdgeNeighbor {
  int e;
  uint16_t flags;
  int v1, v2;
};

struct VertData {
  blender::Span<blender::float3> positions;
  LineartVert *v_arr;
  double (*model_view)[4];
  double (*model_view_proj)[4];
};

static void lineart_mvert_transform_task(void *__restrict userdata,
                                         const int i,
                                         const TaskParallelTLS *__restrict /*tls*/)
{
  VertData *vert_task_data = (VertData *)userdata;
  double co[4];
  LineartVert *v = &vert_task_data->v_arr[i];
  copy_v3db_v3fl(co, vert_task_data->positions[i]);
  mul_v3_m4v3_db(v->gloc, vert_task_data->model_view, co);
  mul_v4_m4v3_db(v->fbcoord, vert_task_data->model_view_proj, co);
  v->index = i;
}

static const int LRT_MESH_EDGE_TYPES[] = {
    MOD_LINEART_EDGE_FLAG_EDGE_MARK,
    MOD_LINEART_EDGE_FLAG_CONTOUR,
    MOD_LINEART_EDGE_FLAG_CREASE,
    MOD_LINEART_EDGE_FLAG_MATERIAL,
    MOD_LINEART_EDGE_FLAG_LOOSE,
    MOD_LINEART_EDGE_FLAG_CONTOUR_SECONDARY,
};

#define LRT_MESH_EDGE_TYPES_COUNT 6

static int lineart_edge_type_duplication_count(int eflag)
{
  int count = 0;
  /* See eLineartEdgeFlag for details. */
  for (int i = 0; i < LRT_MESH_EDGE_TYPES_COUNT; i++) {
    if (eflag & LRT_MESH_EDGE_TYPES[i]) {
      count++;
    }
  }
  return count;
}

/**
 * Because we have a variable size for #LineartTriangle, we need an access helper.
 * See #LineartTriangleThread for more info.
 */
static LineartTriangle *lineart_triangle_from_index(LineartData *ld,
                                                    LineartTriangle *rt_array,
                                                    int index)
{
  int8_t *b = (int8_t *)rt_array;
  b += (index * ld->sizeof_triangle);
  return (LineartTriangle *)b;
}

struct EdgeFeatData {
  LineartData *ld;
  Mesh *mesh;
  Object *ob_eval;                     /* For evaluated materials. */
  blender::Span<int> material_indices; /* May be empty. */
  blender::Span<blender::int2> edges;
  blender::Span<int> corner_verts;
  blender::Span<int> corner_edges;
  blender::Span<int3> corner_tris;
  blender::Span<int> tri_faces;
  LineartTriangle *tri_array;
  blender::VArray<bool> sharp_edges;
  blender::VArray<bool> sharp_faces;
  LineartVert *v_array;
  float crease_threshold;
  bool use_auto_smooth;
  bool use_freestyle_face;
  blender::VArray<bool> freestyle_face;
  bool use_freestyle_edge;
  blender::VArray<bool> freestyle_edge;
  LineartEdgeNeighbor *edge_nabr;
};

struct EdgeFeatReduceData {
  int feat_edges;
};

static void feat_data_sum_reduce(const void *__restrict /*userdata*/,
                                 void *__restrict chunk_join,
                                 void *__restrict chunk)
{
  EdgeFeatReduceData *feat_chunk_join = (EdgeFeatReduceData *)chunk_join;
  EdgeFeatReduceData *feat_chunk = (EdgeFeatReduceData *)chunk;
  feat_chunk_join->feat_edges += feat_chunk->feat_edges;
}

static void lineart_identify_corner_tri_feature_edges(void *__restrict userdata,
                                                      const int i,
                                                      const TaskParallelTLS *__restrict tls)
{
  EdgeFeatData *e_feat_data = (EdgeFeatData *)userdata;
  EdgeFeatReduceData *reduce_data = (EdgeFeatReduceData *)tls->userdata_chunk;
  Object *ob_eval = e_feat_data->ob_eval;
  LineartEdgeNeighbor *edge_nabr = e_feat_data->edge_nabr;
  const blender::Span<int3> corner_tris = e_feat_data->corner_tris;
  const blender::Span<int> tri_faces = e_feat_data->tri_faces;
  const blender::Span<int> material_indices = e_feat_data->material_indices;

  uint16_t edge_flag_result = 0;

  /* Because the edge neighbor array contains loop edge pairs, we only need to process the first
   * edge in the pair. Otherwise we would add the same edge that the loops represent twice. */
  if (i < edge_nabr[i].e) {
    return;
  }

  bool face_mark_filtered = false;
  bool enable_face_mark = (e_feat_data->use_freestyle_face &&
                           e_feat_data->ld->conf.filter_face_mark);
  bool only_contour = false;
  if (enable_face_mark) {
    bool ff1 = false;
    bool ff2 = false;
    if (const blender::VArray<bool> &freestyle_face = e_feat_data->freestyle_face) {
      if (freestyle_face[tri_faces[i / 3]]) {
        ff1 = true;
      }
    }
    if (edge_nabr[i].e > -1 && e_feat_data->freestyle_face) {
      ff2 = e_feat_data->freestyle_face[tri_faces[edge_nabr[i].e / 3]];
    }
    else {
      /* Handle mesh boundary cases: We want mesh boundaries to respect
       * `filter_face_mark_boundaries` option the same way as face mark boundaries, and the code
       * path is simper when it's assuming both ff1 and ff2 not nullptr. */
      ff2 = ff1;
    }
    if (e_feat_data->ld->conf.filter_face_mark_boundaries ^
        e_feat_data->ld->conf.filter_face_mark_invert)
    {
      if (ff1 || ff2) {
        face_mark_filtered = true;
      }
    }
    else {
      if (ff1 && ff2 && (ff2 != ff1)) {
        face_mark_filtered = true;
      }
    }
    if (e_feat_data->ld->conf.filter_face_mark_invert) {
      face_mark_filtered = !face_mark_filtered;
    }
    if (!face_mark_filtered) {
      edge_nabr[i].flags = MOD_LINEART_EDGE_FLAG_INHIBIT;
      if (e_feat_data->ld->conf.filter_face_mark_keep_contour) {
        only_contour = true;
      }
    }
  }

  if (enable_face_mark && !face_mark_filtered && !only_contour) {
    return;
  }

  /* Mesh boundary */
  if (edge_nabr[i].e == -1) {
    edge_nabr[i].flags = MOD_LINEART_EDGE_FLAG_CONTOUR;
    reduce_data->feat_edges += 1;
    return;
  }

  LineartTriangle *tri1, *tri2;
  LineartVert *vert;
  LineartData *ld = e_feat_data->ld;

  int f1 = i / 3, f2 = edge_nabr[i].e / 3;

  /* The mesh should already be triangulated now, so we can assume each face is a triangle. */
  tri1 = lineart_triangle_from_index(ld, e_feat_data->tri_array, f1);
  tri2 = lineart_triangle_from_index(ld, e_feat_data->tri_array, f2);

  vert = &e_feat_data->v_array[edge_nabr[i].v1];

  double view_vector_persp[3];
  double *view_vector = view_vector_persp;
  double dot_v1 = 0, dot_v2 = 0;
  double result;
  bool material_back_face = ((tri1->flags | tri2->flags) & LRT_TRIANGLE_MAT_BACK_FACE_CULLING);

  if (ld->conf.use_contour || ld->conf.use_back_face_culling || material_back_face) {
    if (ld->conf.cam_is_persp) {
      sub_v3_v3v3_db(view_vector, ld->conf.camera_pos, vert->gloc);
    }
    else {
      view_vector = ld->conf.view_vector;
    }

    dot_v1 = dot_v3v3_db(view_vector, tri1->gn);
    dot_v2 = dot_v3v3_db(view_vector, tri2->gn);

    if ((result = dot_v1 * dot_v2) <= 0 && (dot_v1 + dot_v2)) {
      edge_flag_result |= MOD_LINEART_EDGE_FLAG_CONTOUR;
    }

    if (ld->conf.use_back_face_culling) {
      if (dot_v1 < 0) {
        tri1->flags |= LRT_CULL_DISCARD;
      }
      if (dot_v2 < 0) {
        tri2->flags |= LRT_CULL_DISCARD;
      }
    }
    if (material_back_face) {
      if (tri1->flags & LRT_TRIANGLE_MAT_BACK_FACE_CULLING && dot_v1 < 0) {
        tri1->flags |= LRT_CULL_DISCARD;
      }
      if (tri2->flags & LRT_TRIANGLE_MAT_BACK_FACE_CULLING && dot_v2 < 0) {
        tri2->flags |= LRT_CULL_DISCARD;
      }
    }
  }

  if (ld->conf.use_contour_secondary) {
    view_vector = view_vector_persp;
    if (ld->conf.cam_is_persp_secondary) {
      sub_v3_v3v3_db(view_vector, vert->gloc, ld->conf.camera_pos_secondary);
    }
    else {
      view_vector = ld->conf.view_vector_secondary;
    }

    dot_v1 = dot_v3v3_db(view_vector, tri1->gn);
    dot_v2 = dot_v3v3_db(view_vector, tri2->gn);

    if ((result = dot_v1 * dot_v2) <= 0 && (dot_v1 + dot_v2)) {
      edge_flag_result |= MOD_LINEART_EDGE_FLAG_CONTOUR_SECONDARY;
    }
  }

  if (!only_contour) {
    if (ld->conf.use_crease) {
      bool do_crease = true;
      if (!ld->conf.force_crease && !e_feat_data->use_auto_smooth &&
          (!e_feat_data->sharp_faces[tri_faces[f1]]) && (!e_feat_data->sharp_faces[tri_faces[f2]]))
      {
        do_crease = false;
      }
      if (do_crease && (dot_v3v3_db(tri1->gn, tri2->gn) < e_feat_data->crease_threshold)) {
        edge_flag_result |= MOD_LINEART_EDGE_FLAG_CREASE;
      }
    }

    int mat1 = material_indices.is_empty() ? 0 : material_indices[tri_faces[f1]];
    int mat2 = material_indices.is_empty() ? 0 : material_indices[tri_faces[f2]];

    if (mat1 != mat2) {
      Material *m1 = BKE_object_material_get_eval(ob_eval, mat1 + 1);
      Material *m2 = BKE_object_material_get_eval(ob_eval, mat2 + 1);
      if (m1 && m2 &&
          ((m1->lineart.mat_occlusion == 0 && m2->lineart.mat_occlusion != 0) ||
           (m2->lineart.mat_occlusion == 0 && m1->lineart.mat_occlusion != 0)))
      {
        if (ld->conf.use_contour) {
          edge_flag_result |= MOD_LINEART_EDGE_FLAG_CONTOUR;
        }
      }
      if (ld->conf.use_material) {
        edge_flag_result |= MOD_LINEART_EDGE_FLAG_MATERIAL;
      }
    }
  }
  else {                     /* only_contour */
    if (!edge_flag_result) { /* Other edge types inhibited */
      return;
    }
  }

  const int3 real_edges = corner_tri_get_real_edges(e_feat_data->edges,
                                                    e_feat_data->corner_verts,
                                                    e_feat_data->corner_edges,
                                                    corner_tris[i / 3]);

  if (real_edges[i % 3] >= 0) {
    if (ld->conf.use_crease && ld->conf.sharp_as_crease &&
        e_feat_data->sharp_edges[real_edges[i % 3]])
    {
      edge_flag_result |= MOD_LINEART_EDGE_FLAG_CREASE;
    }

    if (ld->conf.use_edge_marks && e_feat_data->use_freestyle_edge) {
      if (e_feat_data->freestyle_edge[real_edges[i % 3]]) {
        edge_flag_result |= MOD_LINEART_EDGE_FLAG_EDGE_MARK;
      }
    }
  }

  edge_nabr[i].flags = edge_flag_result;

  if (edge_flag_result) {
    /* Only allocate for feature edge (instead of all edges) to save memory.
     * If allow duplicated edges, one edge gets added multiple times if it has multiple types.
     */
    reduce_data->feat_edges += e_feat_data->ld->conf.allow_duplicated_types ?
                                   lineart_edge_type_duplication_count(edge_flag_result) :
                                   1;
  }
}

struct LooseEdgeData {
  int loose_count;
  int *loose_array;
};

void lineart_add_edge_to_array(LineartPendingEdges *pe, LineartEdge *e)
{
  if (pe->next >= pe->max || !pe->max) {
    if (!pe->max) {
      pe->max = 1000;
    }

    LineartEdge **new_array = MEM_malloc_arrayN<LineartEdge *>(size_t(pe->max) * 2,
                                                               "LineartPendingEdges array");
    if (LIKELY(pe->array)) {
      memcpy(new_array, pe->array, sizeof(LineartEdge *) * pe->max);
      MEM_freeN(pe->array);
    }
    pe->max *= 2;
    pe->array = new_array;
  }
  pe->array[pe->next] = e;
  pe->next++;
}
static void lineart_add_edge_to_array_thread(LineartObjectInfo *obi, LineartEdge *e)
{
  lineart_add_edge_to_array(&obi->pending_edges, e);
}

void lineart_finalize_object_edge_array_reserve(LineartPendingEdges *pe, int count)
{
  /* NOTE: For simplicity, this function doesn't actually do anything
   * if you already have data in #pe. */

  if (pe->max || pe->array || count == 0) {
    return;
  }

  pe->max = count;
  LineartEdge **new_array = MEM_malloc_arrayN<LineartEdge *>(size_t(pe->max),
                                                             "LineartPendingEdges array final");
  pe->array = new_array;
}

static void lineart_finalize_object_edge_array(LineartPendingEdges *pe, LineartObjectInfo *obi)
{
  /* In case of line art "occlusion only" or contour not enabled, it's possible for an object to
   * not produce any feature lines. */
  if (!obi->pending_edges.array) {
    return;
  }
  memcpy(&pe->array[pe->next],
         obi->pending_edges.array,
         sizeof(LineartEdge *) * obi->pending_edges.next);
  MEM_freeN(obi->pending_edges.array);
  pe->next += obi->pending_edges.next;
}

static void lineart_triangle_adjacent_assign(LineartTriangle *tri,
                                             LineartTriangleAdjacent *tri_adj,
                                             LineartEdge *e)
{
  if (lineart_edge_match(tri, e, 0, 1)) {
    tri_adj->e[0] = e;
  }
  else if (lineart_edge_match(tri, e, 1, 2)) {
    tri_adj->e[1] = e;
  }
  else if (lineart_edge_match(tri, e, 2, 0)) {
    tri_adj->e[2] = e;
  }
}

struct TriData {
  LineartObjectInfo *ob_info;
  blender::Span<blender::float3> positions;
  blender::Span<int> corner_verts;
  blender::Span<int3> corner_tris;
  blender::Span<int> tri_faces;
  blender::Span<int> material_indices;
  LineartVert *vert_arr;
  LineartTriangle *tri_arr;
  int lineart_triangle_size;
  LineartTriangleAdjacent *tri_adj;
};

static void lineart_load_tri_task(void *__restrict userdata,
                                  const int i,
                                  const TaskParallelTLS *__restrict /*tls*/)
{
  TriData *tri_task_data = (TriData *)userdata;
  LineartObjectInfo *ob_info = tri_task_data->ob_info;
  const blender::Span<blender::float3> positions = tri_task_data->positions;
  const blender::Span<int> corner_verts = tri_task_data->corner_verts;
  const int3 &corner_tri = tri_task_data->corner_tris[i];
  const int face_i = tri_task_data->tri_faces[i];
  const blender::Span<int> material_indices = tri_task_data->material_indices;

  LineartVert *vert_arr = tri_task_data->vert_arr;
  LineartTriangle *tri = tri_task_data->tri_arr;

  tri = (LineartTriangle *)(((uchar *)tri) + tri_task_data->lineart_triangle_size * i);

  int v1 = corner_verts[corner_tri[0]];
  int v2 = corner_verts[corner_tri[1]];
  int v3 = corner_verts[corner_tri[2]];

  tri->v[0] = &vert_arr[v1];
  tri->v[1] = &vert_arr[v2];
  tri->v[2] = &vert_arr[v3];

  /* Material mask bits and occlusion effectiveness assignment. */
  Material *mat = BKE_object_material_get(
      ob_info->original_ob_eval, material_indices.is_empty() ? 1 : material_indices[face_i] + 1);
  tri->material_mask_bits |= ((mat && (mat->lineart.flags & LRT_MATERIAL_MASK_ENABLED)) ?
                                  mat->lineart.material_mask_bits :
                                  0);
  tri->mat_occlusion |= (mat ? mat->lineart.mat_occlusion : 1);
  tri->intersection_priority = ((mat && (mat->lineart.flags &
                                         LRT_MATERIAL_CUSTOM_INTERSECTION_PRIORITY)) ?
                                    mat->lineart.intersection_priority :
                                    ob_info->intersection_priority);
  tri->flags |= (mat && (mat->blend_flag & MA_BL_CULL_BACKFACE)) ?
                    LRT_TRIANGLE_MAT_BACK_FACE_CULLING :
                    0;

  tri->intersection_mask = ob_info->override_intersection_mask;

  tri->target_reference = (ob_info->obindex | (i & LRT_OBINDEX_LOWER));

  double gn[3];
  float no[3];
  normal_tri_v3(no, positions[v1], positions[v2], positions[v3]);
  copy_v3db_v3fl(gn, no);
  mul_v3_mat3_m4v3_db(tri->gn, ob_info->normal, gn);
  normalize_v3_db(tri->gn);

  if (ob_info->usage == OBJECT_LRT_INTERSECTION_ONLY) {
    tri->flags |= LRT_TRIANGLE_INTERSECTION_ONLY;
  }
  else if (ob_info->usage == OBJECT_LRT_FORCE_INTERSECTION) {
    tri->flags |= LRT_TRIANGLE_FORCE_INTERSECTION;
  }
  else if (ELEM(ob_info->usage, OBJECT_LRT_NO_INTERSECTION, OBJECT_LRT_OCCLUSION_ONLY)) {
    tri->flags |= LRT_TRIANGLE_NO_INTERSECTION;
  }

  /* Re-use this field to refer to adjacent info, will be cleared after culling stage. */
  tri->intersecting_verts = static_cast<LinkNode *>((void *)&tri_task_data->tri_adj[i]);
}
struct EdgeNeighborData {
  LineartEdgeNeighbor *edge_nabr;
  LineartAdjacentEdge *adj_e;
  blender::Span<int> corner_verts;
  blender::Span<int3> corner_tris;
  blender::Span<int> tri_faces;
};

static void lineart_edge_neighbor_init_task(void *__restrict userdata,
                                            const int i,
                                            const TaskParallelTLS *__restrict /*tls*/)
{
  EdgeNeighborData *en_data = (EdgeNeighborData *)userdata;
  LineartAdjacentEdge *adj_e = &en_data->adj_e[i];
  const int3 &tri = en_data->corner_tris[i / 3];
  LineartEdgeNeighbor *edge_nabr = &en_data->edge_nabr[i];
  const blender::Span<int> corner_verts = en_data->corner_verts;

  adj_e->e = i;
  adj_e->v1 = corner_verts[tri[i % 3]];
  adj_e->v2 = corner_verts[tri[(i + 1) % 3]];
  if (adj_e->v1 > adj_e->v2) {
    std::swap(adj_e->v1, adj_e->v2);
  }
  edge_nabr->e = -1;

  edge_nabr->v1 = adj_e->v1;
  edge_nabr->v2 = adj_e->v2;
  edge_nabr->flags = 0;
}

static void lineart_sort_adjacent_items(LineartAdjacentEdge *ai, int length)
{
  blender::parallel_sort(
      ai, ai + length, [](const LineartAdjacentEdge &p1, const LineartAdjacentEdge &p2) {
        int a = p1.v1 - p2.v1;
        int b = p1.v2 - p2.v2;
        /* `parallel_sort()` requires `cmp()` to return true when the first element needs to appear
         * before the second element in the sorted array, false otherwise (strict weak ordering),
         * see https://en.cppreference.com/w/cpp/named_req/Compare. */
        if (a < 0) {
          return true;
        }
        if (a > 0) {
          return false;
        }
        return b < 0;
      });
}

static LineartEdgeNeighbor *lineart_build_edge_neighbor(Mesh *mesh, int total_edges)
{
  /* Because the mesh is triangulated, so `mesh->edges_num` should be reliable? */
  LineartAdjacentEdge *adj_e = MEM_malloc_arrayN<LineartAdjacentEdge>(size_t(total_edges),
                                                                      "LineartAdjacentEdge arr");
  LineartEdgeNeighbor *edge_nabr = MEM_malloc_arrayN<LineartEdgeNeighbor>(
      size_t(total_edges), "LineartEdgeNeighbor arr");

  TaskParallelSettings en_settings;
  BLI_parallel_range_settings_defaults(&en_settings);
  /* Set the minimum amount of edges a thread has to process. */
  en_settings.min_iter_per_thread = 50000;

  EdgeNeighborData en_data;
  en_data.adj_e = adj_e;
  en_data.edge_nabr = edge_nabr;
  en_data.corner_verts = mesh->corner_verts();
  en_data.corner_tris = mesh->corner_tris();
  en_data.tri_faces = mesh->corner_tri_faces();

  BLI_task_parallel_range(0, total_edges, &en_data, lineart_edge_neighbor_init_task, &en_settings);

  lineart_sort_adjacent_items(adj_e, total_edges);

  for (int i = 0; i < total_edges - 1; i++) {
    if (adj_e[i].v1 == adj_e[i + 1].v1 && adj_e[i].v2 == adj_e[i + 1].v2) {
      edge_nabr[adj_e[i].e].e = adj_e[i + 1].e;
      edge_nabr[adj_e[i + 1].e].e = adj_e[i].e;
    }
  }

  MEM_freeN(adj_e);

  return edge_nabr;
}

static void lineart_geometry_object_load(LineartObjectInfo *ob_info,
                                         LineartData *la_data,
                                         ListBase *shadow_elns)
{
  using namespace blender;
  Mesh *mesh = ob_info->original_me;
  if (!mesh->edges_num) {
    return;
  }

  /* Triangulate. */
  const Span<int3> corner_tris = mesh->corner_tris();
  const AttributeAccessor attributes = mesh->attributes();
  const VArraySpan material_indices = *attributes.lookup<int>("material_index", AttrDomain::Face);

  /* If we allow duplicated edges, one edge should get added multiple times if is has been
   * classified as more than one edge type. This is so we can create multiple different line type
   * chains containing the same edge. */
  LineartVert *la_v_arr = static_cast<LineartVert *>(lineart_mem_acquire_thread(
      &la_data->render_data_pool, sizeof(LineartVert) * mesh->verts_num));
  LineartTriangle *la_tri_arr = static_cast<LineartTriangle *>(lineart_mem_acquire_thread(
      &la_data->render_data_pool, corner_tris.size() * la_data->sizeof_triangle));

  Object *orig_ob = ob_info->original_ob;

  BLI_spin_lock(&la_data->lock_task);
  LineartElementLinkNode *elem_link_node = static_cast<LineartElementLinkNode *>(
      lineart_list_append_pointer_pool_sized_thread(&la_data->geom.vertex_buffer_pointers,
                                                    &la_data->render_data_pool,
                                                    la_v_arr,
                                                    sizeof(LineartElementLinkNode)));
  BLI_spin_unlock(&la_data->lock_task);

  elem_link_node->obindex = ob_info->obindex;
  elem_link_node->element_count = mesh->verts_num;
  elem_link_node->object_ref = orig_ob;
  ob_info->v_eln = elem_link_node;

  bool use_auto_smooth = false;
  float crease_angle = 0;
  if (orig_ob->lineart.flags & OBJECT_LRT_OWN_CREASE) {
    crease_angle = cosf(M_PI - orig_ob->lineart.crease_threshold);
  }
  else {
    crease_angle = la_data->conf.crease_threshold;
  }

  /* FIXME(Yiming): Hack for getting clean 3D text, the seam that extruded text object creates
   * erroneous detection on creases. Future configuration should allow options. */
  if (orig_ob->type == OB_FONT) {
    elem_link_node->flags |= LRT_ELEMENT_BORDER_ONLY;
  }

  BLI_spin_lock(&la_data->lock_task);
  elem_link_node = static_cast<LineartElementLinkNode *>(
      lineart_list_append_pointer_pool_sized_thread(&la_data->geom.triangle_buffer_pointers,
                                                    &la_data->render_data_pool,
                                                    la_tri_arr,
                                                    sizeof(LineartElementLinkNode)));
  BLI_spin_unlock(&la_data->lock_task);

  int usage = ob_info->usage;

  elem_link_node->element_count = corner_tris.size();
  elem_link_node->object_ref = orig_ob;
  elem_link_node->flags = eLineArtElementNodeFlag(
      elem_link_node->flags |
      ((usage == OBJECT_LRT_NO_INTERSECTION) ? LRT_ELEMENT_NO_INTERSECTION : 0));

  /* Note this memory is not from pool, will be deleted after culling. */
  LineartTriangleAdjacent *tri_adj = MEM_calloc_arrayN<LineartTriangleAdjacent>(
      size_t(corner_tris.size()), "LineartTriangleAdjacent");
  /* Link is minimal so we use pool anyway. */
  BLI_spin_lock(&la_data->lock_task);
  lineart_list_append_pointer_pool_thread(
      &la_data->geom.triangle_adjacent_pointers, &la_data->render_data_pool, tri_adj);
  BLI_spin_unlock(&la_data->lock_task);

  /* Convert all vertices to lineart verts. */
  TaskParallelSettings vert_settings;
  BLI_parallel_range_settings_defaults(&vert_settings);
  /* Set the minimum amount of verts a thread has to process. */
  vert_settings.min_iter_per_thread = 4000;

  VertData vert_data;
  vert_data.positions = mesh->vert_positions();
  vert_data.v_arr = la_v_arr;
  vert_data.model_view = ob_info->model_view;
  vert_data.model_view_proj = ob_info->model_view_proj;

  BLI_task_parallel_range(
      0, mesh->verts_num, &vert_data, lineart_mvert_transform_task, &vert_settings);

  /* Convert all mesh triangles into lineart triangles.
   * Also create an edge map to get connectivity between edges and triangles. */
  TaskParallelSettings tri_settings;
  BLI_parallel_range_settings_defaults(&tri_settings);
  /* Set the minimum amount of triangles a thread has to process. */
  tri_settings.min_iter_per_thread = 4000;

  TriData tri_data;
  tri_data.ob_info = ob_info;
  tri_data.positions = mesh->vert_positions();
  tri_data.corner_tris = corner_tris;
  tri_data.tri_faces = mesh->corner_tri_faces();
  tri_data.corner_verts = mesh->corner_verts();
  tri_data.material_indices = material_indices;
  tri_data.vert_arr = la_v_arr;
  tri_data.tri_arr = la_tri_arr;
  tri_data.lineart_triangle_size = la_data->sizeof_triangle;
  tri_data.tri_adj = tri_adj;

  uint32_t total_edges = corner_tris.size() * 3;

  BLI_task_parallel_range(0, corner_tris.size(), &tri_data, lineart_load_tri_task, &tri_settings);

  /* Check for contour lines in the mesh.
   * IE check if the triangle edges lies in area where the triangles go from front facing to back
   * facing.
   */
  EdgeFeatReduceData edge_reduce = {0};
  TaskParallelSettings edge_feat_settings;
  BLI_parallel_range_settings_defaults(&edge_feat_settings);
  /* Set the minimum amount of edges a thread has to process. */
  edge_feat_settings.min_iter_per_thread = 4000;
  edge_feat_settings.userdata_chunk = &edge_reduce;
  edge_feat_settings.userdata_chunk_size = sizeof(EdgeFeatReduceData);
  edge_feat_settings.func_reduce = feat_data_sum_reduce;

  const VArray<bool> sharp_edges = *attributes.lookup_or_default<bool>(
      "sharp_edge", AttrDomain::Edge, false);
  const VArray<bool> sharp_faces = *attributes.lookup_or_default<bool>(
      "sharp_face", AttrDomain::Face, false);

  EdgeFeatData edge_feat_data = {nullptr};
  edge_feat_data.ld = la_data;
  edge_feat_data.mesh = mesh;
  edge_feat_data.ob_eval = ob_info->original_ob_eval;
  edge_feat_data.material_indices = material_indices;
  edge_feat_data.edges = mesh->edges();
  edge_feat_data.corner_verts = mesh->corner_verts();
  edge_feat_data.corner_edges = mesh->corner_edges();
  edge_feat_data.corner_tris = corner_tris;
  edge_feat_data.tri_faces = mesh->corner_tri_faces();
  edge_feat_data.sharp_edges = sharp_edges;
  edge_feat_data.sharp_faces = sharp_faces;
  edge_feat_data.edge_nabr = lineart_build_edge_neighbor(mesh, total_edges);
  edge_feat_data.tri_array = la_tri_arr;
  edge_feat_data.v_array = la_v_arr;
  edge_feat_data.crease_threshold = crease_angle;
  edge_feat_data.use_auto_smooth = use_auto_smooth;
  edge_feat_data.freestyle_face = *attributes.lookup<bool>("freestyle_face", AttrDomain::Face);
  edge_feat_data.freestyle_edge = *attributes.lookup<bool>("freestyle_edge", AttrDomain::Edge);
  edge_feat_data.use_freestyle_face = bool(edge_feat_data.freestyle_face);
  edge_feat_data.use_freestyle_edge = bool(edge_feat_data.freestyle_edge);

  BLI_task_parallel_range(0,
                          total_edges,
                          &edge_feat_data,
                          lineart_identify_corner_tri_feature_edges,
                          &edge_feat_settings);

  LooseEdgeData loose_data = {0};

  if (la_data->conf.use_loose) {
    /* Only identifying floating edges at this point because other edges has been taken care of
     * inside #lineart_identify_corner_tri_feature_edges function. */
    const LooseEdgeCache &loose_edges = mesh->loose_edges();
    loose_data.loose_array = MEM_malloc_arrayN<int>(size_t(loose_edges.count), __func__);
    if (loose_edges.count > 0) {
      loose_data.loose_count = 0;
      for (const int64_t edge_i : IndexRange(mesh->edges_num)) {
        if (loose_edges.is_loose_bits[edge_i]) {
          loose_data.loose_array[loose_data.loose_count] = int(edge_i);
          loose_data.loose_count++;
        }
      }
    }
  }

  int allocate_la_e = edge_reduce.feat_edges + loose_data.loose_count;

  LineartEdge *la_edge_arr = static_cast<LineartEdge *>(
      lineart_mem_acquire_thread(la_data->edge_data_pool, sizeof(LineartEdge) * allocate_la_e));
  LineartEdgeSegment *la_seg_arr = static_cast<LineartEdgeSegment *>(lineart_mem_acquire_thread(
      la_data->edge_data_pool, sizeof(LineartEdgeSegment) * allocate_la_e));
  BLI_spin_lock(&la_data->lock_task);
  elem_link_node = static_cast<LineartElementLinkNode *>(
      lineart_list_append_pointer_pool_sized_thread(&la_data->geom.line_buffer_pointers,
                                                    la_data->edge_data_pool,
                                                    la_edge_arr,
                                                    sizeof(LineartElementLinkNode)));
  BLI_spin_unlock(&la_data->lock_task);
  elem_link_node->element_count = allocate_la_e;
  elem_link_node->object_ref = orig_ob;
  elem_link_node->obindex = ob_info->obindex;

  LineartElementLinkNode *shadow_eln = nullptr;
  if (shadow_elns) {
    shadow_eln = lineart_find_matching_eln(shadow_elns, ob_info->obindex);
  }

  /* Start of the edge/seg arr */
  LineartEdge *la_edge;
  LineartEdgeSegment *la_seg;
  la_edge = la_edge_arr;
  la_seg = la_seg_arr;

  for (int i = 0; i < total_edges; i++) {
    LineartEdgeNeighbor *edge_nabr = &edge_feat_data.edge_nabr[i];

    if (i < edge_nabr->e) {
      continue;
    }

    /* Not a feature line, so we skip. */
    if (edge_nabr->flags == 0) {
      continue;
    }

    LineartEdge *edge_added = nullptr;

    /* See eLineartEdgeFlag for details. */
    for (int flag_bit = 0; flag_bit < LRT_MESH_EDGE_TYPES_COUNT; flag_bit++) {
      int use_type = LRT_MESH_EDGE_TYPES[flag_bit];
      if (!(use_type & edge_nabr->flags)) {
        continue;
      }

      la_edge->v1 = &la_v_arr[edge_nabr->v1];
      la_edge->v2 = &la_v_arr[edge_nabr->v2];
      int findex = i / 3;
      la_edge->t1 = lineart_triangle_from_index(la_data, la_tri_arr, findex);
      if (!edge_added) {
        lineart_triangle_adjacent_assign(la_edge->t1, &tri_adj[findex], la_edge);
      }
      if (edge_nabr->e != -1) {
        findex = edge_nabr->e / 3;
        la_edge->t2 = lineart_triangle_from_index(la_data, la_tri_arr, findex);
        if (!edge_added) {
          lineart_triangle_adjacent_assign(la_edge->t2, &tri_adj[findex], la_edge);
        }
      }
      la_edge->flags = use_type;
      la_edge->object_ref = orig_ob;
      la_edge->edge_identifier = LRT_EDGE_IDENTIFIER(ob_info, la_edge);
      BLI_addtail(&la_edge->segments, la_seg);

      if (shadow_eln) {
        /* TODO(Yiming): It's gonna be faster to do this operation after second stage occlusion if
         * we only need visible segments to have shadow info, however that way we lose information
         * on "shadow behind transparency window" type of region. */
        LineartEdge *shadow_e = lineart_find_matching_edge(shadow_eln, la_edge->edge_identifier);
        if (shadow_e) {
          lineart_register_shadow_cuts(la_data, la_edge, shadow_e);
        }
      }

      if (ELEM(usage,
               OBJECT_LRT_INHERIT,
               OBJECT_LRT_INCLUDE,
               OBJECT_LRT_NO_INTERSECTION,
               OBJECT_LRT_FORCE_INTERSECTION))
      {
        lineart_add_edge_to_array_thread(ob_info, la_edge);
      }

      if (edge_added) {
        edge_added->flags |= MOD_LINEART_EDGE_FLAG_NEXT_IS_DUPLICATION;
      }

      edge_added = la_edge;

      la_edge++;
      la_seg++;

      if (!la_data->conf.allow_duplicated_types) {
        break;
      }
    }
  }

  if (loose_data.loose_array) {
    const Span<int2> edges = mesh->edges();
    for (int i = 0; i < loose_data.loose_count; i++) {
      const int2 &edge = edges[loose_data.loose_array[i]];
      la_edge->v1 = &la_v_arr[edge[0]];
      la_edge->v2 = &la_v_arr[edge[1]];
      la_edge->flags = MOD_LINEART_EDGE_FLAG_LOOSE;
      la_edge->object_ref = orig_ob;
      la_edge->edge_identifier = LRT_EDGE_IDENTIFIER(ob_info, la_edge);
      BLI_addtail(&la_edge->segments, la_seg);
      if (ELEM(usage,
               OBJECT_LRT_INHERIT,
               OBJECT_LRT_INCLUDE,
               OBJECT_LRT_NO_INTERSECTION,
               OBJECT_LRT_FORCE_INTERSECTION))
      {
        lineart_add_edge_to_array_thread(ob_info, la_edge);
        if (shadow_eln) {
          LineartEdge *shadow_e = lineart_find_matching_edge(shadow_eln, la_edge->edge_identifier);
          if (shadow_e) {
            lineart_register_shadow_cuts(la_data, la_edge, shadow_e);
          }
        }
      }
      la_edge++;
      la_seg++;
    }
    MEM_SAFE_FREE(loose_data.loose_array);
  }

  MEM_freeN(edge_feat_data.edge_nabr);

  if (ob_info->free_use_mesh) {
    BKE_id_free(nullptr, mesh);
  }
}

static void lineart_object_load_worker(TaskPool *__restrict /*pool*/,
                                       LineartObjectLoadTaskInfo *olti)
{
  for (LineartObjectInfo *obi = olti->pending; obi; obi = obi->next) {
    lineart_geometry_object_load(obi, olti->ld, olti->shadow_elns);
  }
}

static uchar lineart_intersection_mask_check(Collection *c, Object *ob)
{
  LISTBASE_FOREACH (CollectionChild *, cc, &c->children) {
    uchar result = lineart_intersection_mask_check(cc->collection, ob);
    if (result) {
      return result;
    }
  }

  if (BKE_collection_has_object(c, (Object *)(ob->id.orig_id))) {
    if (c->lineart_flags & COLLECTION_LRT_USE_INTERSECTION_MASK) {
      return c->lineart_intersection_mask;
    }
  }

  return 0;
}

static uchar lineart_intersection_priority_check(Collection *c, Object *ob)
{
  if (ob->lineart.flags & OBJECT_LRT_OWN_INTERSECTION_PRIORITY) {
    return ob->lineart.intersection_priority;
  }

  LISTBASE_FOREACH (CollectionChild *, cc, &c->children) {
    uchar result = lineart_intersection_priority_check(cc->collection, ob);
    if (result) {
      return result;
    }
  }
  if (BKE_collection_has_object(c, (Object *)(ob->id.orig_id))) {
    if (c->lineart_flags & COLLECTION_LRT_USE_INTERSECTION_PRIORITY) {
      return c->lineart_intersection_priority;
    }
  }
  return 0;
}

/**
 * See if this object in such collection is used for generating line art,
 * Disabling a collection for line art will doable all objects inside.
 */
static int lineart_usage_check(Collection *c, Object *ob, bool is_render)
{

  if (!c) {
    return OBJECT_LRT_INHERIT;
  }

  int object_has_special_usage = (ob->lineart.usage != OBJECT_LRT_INHERIT);

  if (object_has_special_usage) {
    return ob->lineart.usage;
  }

  if (c->gobject.first) {
    if (BKE_collection_has_object(c, (Object *)(ob->id.orig_id))) {
      if ((is_render && (c->flag & COLLECTION_HIDE_RENDER)) ||
          ((!is_render) && (c->flag & COLLECTION_HIDE_VIEWPORT)))
      {
        return OBJECT_LRT_EXCLUDE;
      }
      if (ob->lineart.usage == OBJECT_LRT_INHERIT) {
        switch (c->lineart_usage) {
          case COLLECTION_LRT_OCCLUSION_ONLY:
            return OBJECT_LRT_OCCLUSION_ONLY;
          case COLLECTION_LRT_EXCLUDE:
            return OBJECT_LRT_EXCLUDE;
          case COLLECTION_LRT_INTERSECTION_ONLY:
            return OBJECT_LRT_INTERSECTION_ONLY;
          case COLLECTION_LRT_NO_INTERSECTION:
            return OBJECT_LRT_NO_INTERSECTION;
          case COLLECTION_LRT_FORCE_INTERSECTION:
            return OBJECT_LRT_FORCE_INTERSECTION;
        }
        return OBJECT_LRT_INHERIT;
      }
      return ob->lineart.usage;
    }
  }

  LISTBASE_FOREACH (CollectionChild *, cc, &c->children) {
    int result = lineart_usage_check(cc->collection, ob, is_render);
    if (result > OBJECT_LRT_INHERIT) {
      return result;
    }
  }

  return OBJECT_LRT_INHERIT;
}

static void lineart_geometry_load_assign_thread(LineartObjectLoadTaskInfo *olti_list,
                                                LineartObjectInfo *obi,
                                                int thread_count,
                                                int this_face_count)
{
  LineartObjectLoadTaskInfo *use_olti = olti_list;
  uint64_t min_face = use_olti->total_faces;
  for (int i = 0; i < thread_count; i++) {
    if (olti_list[i].total_faces < min_face) {
      min_face = olti_list[i].total_faces;
      use_olti = &olti_list[i];
    }
  }

  use_olti->total_faces += this_face_count;
  obi->next = use_olti->pending;
  use_olti->pending = obi;
}

static bool lineart_geometry_check_visible(double model_view_proj[4][4],
                                           double shift_x,
                                           double shift_y,
                                           Mesh *use_mesh)
{
  using namespace blender;
  if (!use_mesh) {
    return false;
  }
  const std::optional<Bounds<float3>> bounds = use_mesh->bounds_min_max();
  if (!bounds.has_value()) {
    return false;
  }
  const std::array<float3, 8> corners = blender::bounds::corners(*bounds);

  double co[8][4];
  double tmp[3];
  for (int i = 0; i < 8; i++) {
    copy_v3db_v3fl(co[i], corners[i]);
    copy_v3_v3_db(tmp, co[i]);
    mul_v4_m4v3_db(co[i], model_view_proj, tmp);
    co[i][0] -= shift_x * 2 * co[i][3];
    co[i][1] -= shift_y * 2 * co[i][3];
  }

  bool cond[6] = {true, true, true, true, true, true};
  /* Because for a point to be inside clip space, it must satisfy `-Wc <= XYCc <= Wc`, here if
   * all verts falls to the same side of the clip space border, we know it's outside view. */
  for (int i = 0; i < 8; i++) {
    cond[0] &= (co[i][0] < -co[i][3]);
    cond[1] &= (co[i][0] > co[i][3]);
    cond[2] &= (co[i][1] < -co[i][3]);
    cond[3] &= (co[i][1] > co[i][3]);
    cond[4] &= (co[i][2] < -co[i][3]);
    cond[5] &= (co[i][2] > co[i][3]);
  }
  for (int i = 0; i < 6; i++) {
    if (cond[i]) {
      return false;
    }
  }
  return true;
}

static void lineart_object_load_single_instance(LineartData *ld,
                                                Depsgraph *depsgraph,
                                                Scene *scene,
                                                Object *ob,
                                                Object *ref_ob,
                                                const float use_mat[4][4],
                                                bool is_render,
                                                LineartObjectLoadTaskInfo *olti,
                                                int thread_count,
                                                int obindex)
{
  LineartObjectInfo *obi = static_cast<LineartObjectInfo *>(
      lineart_mem_acquire(&ld->render_data_pool, sizeof(LineartObjectInfo)));
  obi->usage = lineart_usage_check(scene->master_collection, ob, is_render);
  obi->override_intersection_mask = lineart_intersection_mask_check(scene->master_collection, ob);
  obi->intersection_priority = lineart_intersection_priority_check(scene->master_collection, ob);
  Mesh *use_mesh;

  if (obi->usage == OBJECT_LRT_EXCLUDE) {
    return;
  }

  obi->obindex = obindex << LRT_OBINDEX_SHIFT;

  /* Prepare the matrix used for transforming this specific object (instance). This has to be
   * done before mesh bound-box check because the function needs that. */
  mul_m4db_m4db_m4fl(obi->model_view_proj, ld->conf.view_projection, use_mat);
  mul_m4db_m4db_m4fl(obi->model_view, ld->conf.view, use_mat);

  if (!ELEM(ob->type, OB_MESH, OB_MBALL, OB_CURVES_LEGACY, OB_SURF, OB_FONT, OB_CURVES)) {
    return;
  }
  if (ob->type == OB_MESH) {
    use_mesh = BKE_object_get_evaluated_mesh(ob);
    if ((!use_mesh) || use_mesh->runtime->edit_mesh) {
      /* If the object is being edited, then the mesh is not evaluated fully into the final
       * result, do not load them. This could be caused by incorrect evaluation order due to
       * the way line art uses depsgraph.See #102612 for explanation of this workaround. */
      return;
    }
  }
  else {
    use_mesh = BKE_mesh_new_from_object(depsgraph, ob, true, true, true);
  }

  /* In case we still can not get any mesh geometry data from the object, same as above. */
  if (!use_mesh) {
    return;
  }

  if (!lineart_geometry_check_visible(
          obi->model_view_proj, ld->conf.shift_x, ld->conf.shift_y, use_mesh))
  {
    return;
  }

  if (ob->type != OB_MESH) {
    obi->free_use_mesh = true;
  }

  /* Make normal matrix. */
  float imat[4][4];
  invert_m4_m4(imat, use_mat);
  transpose_m4(imat);
  copy_m4d_m4(obi->normal, imat);

  obi->original_me = use_mesh;
  obi->original_ob = (ref_ob->id.orig_id ? (Object *)ref_ob->id.orig_id : ref_ob);
  obi->original_ob_eval = DEG_get_evaluated(depsgraph, obi->original_ob);
  lineart_geometry_load_assign_thread(olti, obi, thread_count, use_mesh->faces_num);
}

void lineart_main_load_geometries(Depsgraph *depsgraph,
                                  Scene *scene,
                                  Object *camera /* Still use camera arg for convenience. */,
                                  LineartData *ld,
                                  bool allow_duplicates,
                                  bool do_shadow_casting,
                                  ListBase *shadow_elns,
                                  blender::Set<const Object *> *included_objects)
{
  double proj[4][4], view[4][4], result[4][4];
  float inv[4][4];

  if (!do_shadow_casting) {
    Camera *cam = static_cast<Camera *>(camera->data);
    float sensor = BKE_camera_sensor_size(cam->sensor_fit, cam->sensor_x, cam->sensor_y);
    int fit = BKE_camera_sensor_fit(cam->sensor_fit, ld->w, ld->h);
    double asp = (double(ld->w) / double(ld->h));
    if (ELEM(cam->type, CAM_PERSP, CAM_PANO, CAM_CUSTOM)) {
      if (fit == CAMERA_SENSOR_FIT_VERT && asp > 1) {
        sensor *= asp;
      }
      if (fit == CAMERA_SENSOR_FIT_HOR && asp < 1) {
        sensor /= asp;
      }
      const double fov = focallength_to_fov(cam->lens / (1 + ld->conf.overscan), sensor);
      lineart_matrix_perspective_44d(proj, fov, asp, cam->clip_start, cam->clip_end);
    }
    else if (cam->type == CAM_ORTHO) {
      const double w = cam->ortho_scale / 2;
      lineart_matrix_ortho_44d(proj, -w, w, -w / asp, w / asp, cam->clip_start, cam->clip_end);
    }
    else {
      BLI_assert(!"Unsupported camera type in lineart_main_load_geometries");
      unit_m4_db(proj);
    }

    invert_m4_m4(inv, ld->conf.cam_obmat);
    mul_m4db_m4db_m4fl(result, proj, inv);
    copy_m4_m4_db(proj, result);
    copy_m4_m4_db(ld->conf.view_projection, proj);

    unit_m4_db(view);
    copy_m4_m4_db(ld->conf.view, view);
  }

  BLI_listbase_clear(&ld->geom.triangle_buffer_pointers);
  BLI_listbase_clear(&ld->geom.vertex_buffer_pointers);

  double t_start;
  if (G.debug_value == 4000) {
    t_start = BLI_time_now_seconds();
  }

  int thread_count = ld->thread_count;
  int bound_box_discard_count = 0;
  int obindex = 0;

  /* This memory is in render buffer memory pool. So we don't need to free those after loading. */
  LineartObjectLoadTaskInfo *olti = static_cast<LineartObjectLoadTaskInfo *>(lineart_mem_acquire(
      &ld->render_data_pool, sizeof(LineartObjectLoadTaskInfo) * thread_count));

  eEvaluationMode eval_mode = DEG_get_mode(depsgraph);
  bool is_render = eval_mode == DAG_EVAL_RENDER;

  int flags = DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY | DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET |
              DEG_ITER_OBJECT_FLAG_VISIBLE;

  /* Instance duplicated & particles. */
  if (allow_duplicates) {
    flags |= DEG_ITER_OBJECT_FLAG_DUPLI;
  }

  DEGObjectIterSettings deg_iter_settings = {nullptr};
  deg_iter_settings.depsgraph = depsgraph;
  deg_iter_settings.flags = flags;
  deg_iter_settings.included_objects = included_objects;

  DEG_OBJECT_ITER_BEGIN (&deg_iter_settings, ob) {

    obindex++;

    Object *eval_ob = DEG_get_evaluated(depsgraph, ob);

    if (!eval_ob) {
      continue;
    }

    /* DEG_OBJECT_ITER_BEGIN will include the instanced mesh of these curve object types, so don't
     * load them twice. */
    if (allow_duplicates && ELEM(ob->type, OB_CURVES_LEGACY, OB_FONT, OB_SURF, OB_CURVES)) {
      continue;
    }

    if (BKE_object_visibility(eval_ob, eval_mode) & OB_VISIBLE_SELF) {
      lineart_object_load_single_instance(ld,
                                          depsgraph,
                                          scene,
                                          eval_ob,
                                          eval_ob,
                                          eval_ob->object_to_world().ptr(),
                                          is_render,
                                          olti,
                                          thread_count,
                                          obindex);
    }
  }
  DEG_OBJECT_ITER_END;

  TaskPool *tp = BLI_task_pool_create(nullptr, TASK_PRIORITY_HIGH);

  if (G.debug_value == 4000) {
    printf("thread count: %d\n", thread_count);
  }
  for (int i = 0; i < thread_count; i++) {
    olti[i].ld = ld;
    olti[i].shadow_elns = shadow_elns;
    olti[i].thread_id = i;
    BLI_task_pool_push(tp, (TaskRunFunction)lineart_object_load_worker, &olti[i], false, nullptr);
  }
  BLI_task_pool_work_and_wait(tp);
  BLI_task_pool_free(tp);

  /* The step below is to serialize vertex index in the whole scene, so
   * lineart_triangle_share_edge() can work properly from the lack of triangle adjacent info. */
  int global_i = 0;

  int edge_count = 0;
  for (int i = 0; i < thread_count; i++) {
    for (LineartObjectInfo *obi = olti[i].pending; obi; obi = obi->next) {
      if (!obi->v_eln) {
        continue;
      }
      edge_count += obi->pending_edges.next;
    }
  }
  lineart_finalize_object_edge_array_reserve(&ld->pending_edges, edge_count);

  for (int i = 0; i < thread_count; i++) {
    for (LineartObjectInfo *obi = olti[i].pending; obi; obi = obi->next) {
      if (!obi->v_eln) {
        continue;
      }
      LineartVert *v = (LineartVert *)obi->v_eln->pointer;
      int v_count = obi->v_eln->element_count;
      obi->v_eln->global_index_offset = global_i;
      for (int vi = 0; vi < v_count; vi++) {
        v[vi].index += global_i;
      }
      /* Register a global index increment. See #lineart_triangle_share_edge() and
       * #lineart_main_load_geometries() for detailed. It's okay that global_vindex might
       * eventually overflow, in such large scene it's virtually impossible for two vertex of the
       * same numeric index to come close together. */
      obi->global_i_offset = global_i;
      global_i += v_count;
      lineart_finalize_object_edge_array(&ld->pending_edges, obi);
    }
  }

  if (G.debug_value == 4000) {
    double t_elapsed = BLI_time_now_seconds() - t_start;
    printf("Line art loading time: %lf\n", t_elapsed);
    printf("Discarded %d object from bound box check\n", bound_box_discard_count);
  }
}

/**
 * Returns the two other verts of the triangle given a vertex. Returns false if the given vertex
 * doesn't belong to this triangle.
 */
static bool lineart_triangle_get_other_verts(const LineartTriangle *tri,
                                             const LineartVert *vt,
                                             LineartVert **l,
                                             LineartVert **r)
{
  if (tri->v[0] == vt) {
    *l = tri->v[1];
    *r = tri->v[2];
    return true;
  }
  if (tri->v[1] == vt) {
    *l = tri->v[2];
    *r = tri->v[0];
    return true;
  }
  if (tri->v[2] == vt) {
    *l = tri->v[0];
    *r = tri->v[1];
    return true;
  }
  return false;
}

bool lineart_edge_from_triangle(const LineartTriangle *tri,
                                const LineartEdge *e,
                                bool allow_overlapping_edges)
{
  const LineartEdge *use_e = e;
  if (e->flags & MOD_LINEART_EDGE_FLAG_LIGHT_CONTOUR) {
    if (((e->target_reference & LRT_LIGHT_CONTOUR_TARGET) == tri->target_reference) ||
        (((e->target_reference >> 32) & LRT_LIGHT_CONTOUR_TARGET) == tri->target_reference))
    {
      return true;
    }
  }
  else {
    /* Normally we just determine from identifiers of adjacent triangles. */
    if ((use_e->t1 && use_e->t1->target_reference == tri->target_reference) ||
        (use_e->t2 && use_e->t2->target_reference == tri->target_reference))
    {
      return true;
    }
  }

  /* If allows overlapping, then we compare the vertex coordinates one by one to determine if one
   * edge is from specific triangle. This is slower but can handle edge split cases very well. */
  if (allow_overlapping_edges) {
#define LRT_TRI_SAME_POINT(tri, i, pt) \
  ((LRT_DOUBLE_CLOSE_ENOUGH(tri->v[i]->gloc[0], pt->gloc[0]) && \
    LRT_DOUBLE_CLOSE_ENOUGH(tri->v[i]->gloc[1], pt->gloc[1]) && \
    LRT_DOUBLE_CLOSE_ENOUGH(tri->v[i]->gloc[2], pt->gloc[2])) || \
   (LRT_DOUBLE_CLOSE_ENOUGH(tri->v[i]->gloc[0], pt->gloc[0]) && \
    LRT_DOUBLE_CLOSE_ENOUGH(tri->v[i]->gloc[1], pt->gloc[1]) && \
    LRT_DOUBLE_CLOSE_ENOUGH(tri->v[i]->gloc[2], pt->gloc[2])))
    if ((LRT_TRI_SAME_POINT(tri, 0, e->v1) || LRT_TRI_SAME_POINT(tri, 1, e->v1) ||
         LRT_TRI_SAME_POINT(tri, 2, e->v1)) &&
        (LRT_TRI_SAME_POINT(tri, 0, e->v2) || LRT_TRI_SAME_POINT(tri, 1, e->v2) ||
         LRT_TRI_SAME_POINT(tri, 2, e->v2)))
    {
      return true;
    }
#undef LRT_TRI_SAME_POINT
  }
  return false;
}

/* Sorting three intersection points from min to max,
 * the order for each intersection is set in `lst[0]` to `lst[2]`. */
#define INTERSECT_SORT_MIN_TO_MAX_3(ia, ib, ic, lst) \
  { \
    lst[0] = LRT_MIN3_INDEX(ia, ib, ic); \
    lst[1] = (((ia <= ib && ib <= ic) || (ic <= ib && ib <= ia)) ? \
                  1 : \
                  (((ic <= ia && ia <= ib) || (ib < ia && ia <= ic)) ? 0 : 2)); \
    lst[2] = LRT_MAX3_INDEX(ia, ib, ic); \
  }

/* `ia ib ic` are ordered. */
#define INTERSECT_JUST_GREATER(is, order, num, index) \
  { \
    index = (num < is[order[0]] ? \
                 order[0] : \
                 (num < is[order[1]] ? order[1] : (num < is[order[2]] ? order[2] : -1))); \
  }

/* `ia ib ic` are ordered. */
#define INTERSECT_JUST_SMALLER(is, order, num, index) \
  { \
    index = (num > is[order[2]] ? \
                 order[2] : \
                 (num > is[order[1]] ? order[1] : (num > is[order[0]] ? order[0] : -1))); \
  }

#define LRT_ISEC(index) (index == 0 ? isec_e1 : (index == 1 ? isec_e2 : isec_e3))
#define LRT_PARALLEL(index) (index == 0 ? para_e1 : (index == 1 ? para_e2 : para_e3))

/**
 * This is the main function to calculate
 * the occlusion status between 1(one) triangle and 1(one) line.
 * if returns true, then from/to will carry the occluded segments
 * in ratio from `e->v1` to `e->v2`. The line is later cut with these two values.
 *
 * TODO(@Yiming): This function uses a convoluted method that needs to be redesigned.
 *
 * 1) The #lineart_intersect_seg_seg() and #lineart_point_triangle_relation() are separate calls,
 * which would potentially return results that doesn't agree, especially when it's an edge
 * extruding from one of the triangle's point. To get the information using one math process can
 * solve this problem.
 *
 * 2) Currently using discrete a/b/c/para_e1/para_e2/para_e3/is[3] values for storing
 * intersection/edge_aligned/intersection_order info, which isn't optimal, needs a better
 * representation (likely a struct) for readability and clarity of code path.
 *
 * I keep this function as-is because it's still fast, and more importantly the output value
 * threshold is already in tune with the cutting function in the next stage.
 * While current "edge aligned" fix isn't ideal, it does solve most of the precision issue
 * especially in orthographic camera mode.
 */
static bool lineart_triangle_edge_image_space_occlusion(const LineartTriangle *tri,
                                                        const LineartEdge *e,
                                                        const double *override_camera_loc,
                                                        const bool override_cam_is_persp,
                                                        const bool allow_overlapping_edges,
                                                        const double m_view_projection[4][4],
                                                        const double camera_dir[3],
                                                        const float cam_shift_x,
                                                        const float cam_shift_y,
                                                        double *from,
                                                        double *to)
{
  double cross_ratios[3] = {0};
  int cross_order[3];
  int cross_v1 = -1, cross_v2 = -1;
  /* If the edge intersects with the triangle edges (including extensions). */
  int isec_e1, isec_e2, isec_e3;
  /* If edge is parallel to one of the edges in the triangle. */
  bool para_e1, para_e2, para_e3;
  enum LineartPointTri state_v1 = LRT_OUTSIDE_TRIANGLE, state_v2 = LRT_OUTSIDE_TRIANGLE;

  double dir_v1[3];
  double dir_v2[3];
  double view_vector[4];
  double dir_cam[3];
  double dot_v1, dot_v2, dot_v1a, dot_v2a;
  double dot_f;
  double gloc[4], trans[4];
  double cut = -1;

  double *LFBC = e->v1->fbcoord, *RFBC = e->v2->fbcoord, *FBC0 = tri->v[0]->fbcoord,
         *FBC1 = tri->v[1]->fbcoord, *FBC2 = tri->v[2]->fbcoord;

  /* Overlapping not possible, return early. */
  if ((std::max({FBC0[0], FBC1[0], FBC2[0]}) < std::min(LFBC[0], RFBC[0])) ||
      (std::min({FBC0[0], FBC1[0], FBC2[0]}) > std::max(LFBC[0], RFBC[0])) ||
      (std::max({FBC0[1], FBC1[1], FBC2[1]}) < std::min(LFBC[1], RFBC[1])) ||
      (std::min({FBC0[1], FBC1[1], FBC2[1]}) > std::max(LFBC[1], RFBC[1])) ||
      (std::min({FBC0[3], FBC1[3], FBC2[3]}) > std::max(LFBC[3], RFBC[3])))
  {
    return false;
  }

  /* If the line is one of the edge in the triangle, then it's not occluded. */
  if (lineart_edge_from_triangle(tri, e, allow_overlapping_edges)) {
    return false;
  }

  /* Check if the line visually crosses one of the edge in the triangle. */
  isec_e1 = lineart_intersect_seg_seg(LFBC, RFBC, FBC0, FBC1, &cross_ratios[0], &para_e1);
  isec_e2 = lineart_intersect_seg_seg(LFBC, RFBC, FBC1, FBC2, &cross_ratios[1], &para_e2);
  isec_e3 = lineart_intersect_seg_seg(LFBC, RFBC, FBC2, FBC0, &cross_ratios[2], &para_e3);

  /* Sort the intersection distance. */
  INTERSECT_SORT_MIN_TO_MAX_3(cross_ratios[0], cross_ratios[1], cross_ratios[2], cross_order);

  sub_v3_v3v3_db(dir_v1, e->v1->gloc, tri->v[0]->gloc);
  sub_v3_v3v3_db(dir_v2, e->v2->gloc, tri->v[0]->gloc);

  copy_v3_v3_db(dir_cam, camera_dir);
  copy_v3_v3_db(view_vector, override_camera_loc);
  if (override_cam_is_persp) {
    sub_v3_v3v3_db(dir_cam, view_vector, tri->v[0]->gloc);
  }

  dot_v1 = dot_v3v3_db(dir_v1, tri->gn);
  dot_v2 = dot_v3v3_db(dir_v2, tri->gn);
  dot_f = dot_v3v3_db(dir_cam, tri->gn);

  if ((e->flags & MOD_LINEART_EDGE_FLAG_PROJECTED_SHADOW) &&
      (e->target_reference == tri->target_reference))
  {
    if (((dot_f > 0) && (e->flags & MOD_LINEART_EDGE_FLAG_SHADOW_FACING_LIGHT)) ||
        ((dot_f < 0) && !(e->flags & MOD_LINEART_EDGE_FLAG_SHADOW_FACING_LIGHT)))
    {
      *from = 0.0f;
      *to = 1.0f;
      return true;
    }

    return false;
  }

  /* NOTE(Yiming): When we don't use `dot_f==0` here, it's theoretically possible that _some_
   * faces in perspective mode would get erroneously caught in this condition where they really
   * are legit faces that would produce occlusion, but haven't encountered those yet in my test
   * files.
   */
  if (fabs(dot_f) < FLT_EPSILON) {
    return false;
  }

  /* Whether two end points are inside/on_the_edge/outside of the triangle. */
  state_v1 = lineart_point_triangle_relation(LFBC, FBC0, FBC1, FBC2);
  state_v2 = lineart_point_triangle_relation(RFBC, FBC0, FBC1, FBC2);

  /* If the edge doesn't visually cross any edge of the triangle... */
  if (!isec_e1 && !isec_e2 && !isec_e3) {
    /* And if both end point from the edge is outside of the triangle... */
    if ((!state_v1) && (!state_v2)) {
      return false; /* We don't have any occlusion. */
    }
  }

  /* Determine the cut position. */

  dot_v1a = fabs(dot_v1);
  if (dot_v1a < DBL_EPSILON) {
    dot_v1a = 0;
    dot_v1 = 0;
  }
  dot_v2a = fabs(dot_v2);
  if (dot_v2a < DBL_EPSILON) {
    dot_v2a = 0;
    dot_v2 = 0;
  }
  if (dot_v1 - dot_v2 == 0) {
    cut = 100000;
  }
  else if (dot_v1 * dot_v2 <= 0) {
    cut = dot_v1a / fabs(dot_v1 - dot_v2);
  }
  else {
    cut = fabs(dot_v2 + dot_v1) / fabs(dot_v1 - dot_v2);
    cut = dot_v2a > dot_v1a ? 1 - cut : cut;
  }

  /* Transform the cut from geometry space to image space. */
  if (override_cam_is_persp) {
    interp_v3_v3v3_db(gloc, e->v1->gloc, e->v2->gloc, cut);
    mul_v4_m4v3_db(trans, m_view_projection, gloc);
    mul_v3db_db(trans, (1 / trans[3]));
    trans[0] -= cam_shift_x * 2;
    trans[1] -= cam_shift_y * 2;
    /* To accommodate `k=0` and `k=inf` (vertical) lines. here the cut is in image space. */
    if (fabs(e->v1->fbcoord[0] - e->v2->fbcoord[0]) > fabs(e->v1->fbcoord[1] - e->v2->fbcoord[1]))
    {
      cut = ratiod(e->v1->fbcoord[0], e->v2->fbcoord[0], trans[0]);
    }
    else {
      cut = ratiod(e->v1->fbcoord[1], e->v2->fbcoord[1], trans[1]);
    }
  }

#define LRT_GUARD_NOT_FOUND \
  if (cross_v1 < 0 || cross_v2 < 0) { \
    return false; \
  }

  /* Determine the pair of edges that the line has crossed. The "|" symbol in the comment
   * indicates triangle boundary. DBL_TRIANGLE_LIM is needed to for floating point precision
   * tolerance. */

  if (state_v1 == LRT_INSIDE_TRIANGLE) {
    /* Left side is in the triangle. */
    if (state_v2 == LRT_INSIDE_TRIANGLE) {
      /* |   l---r   | */
      INTERSECT_JUST_SMALLER(cross_ratios, cross_order, DBL_TRIANGLE_LIM, cross_v1);
      INTERSECT_JUST_GREATER(cross_ratios, cross_order, 1 - DBL_TRIANGLE_LIM, cross_v2);
    }
    else if (state_v2 == LRT_ON_TRIANGLE) {
      /* |   l------r| */
      INTERSECT_JUST_SMALLER(cross_ratios, cross_order, DBL_TRIANGLE_LIM, cross_v1);
      INTERSECT_JUST_GREATER(cross_ratios, cross_order, 1 - DBL_TRIANGLE_LIM, cross_v2);
    }
    else if (state_v2 == LRT_OUTSIDE_TRIANGLE) {
      /* |   l-------|------r */
      INTERSECT_JUST_SMALLER(cross_ratios, cross_order, DBL_TRIANGLE_LIM, cross_v1);
      INTERSECT_JUST_GREATER(cross_ratios, cross_order, 0, cross_v2);
    }
  }
  else if (state_v1 == LRT_ON_TRIANGLE) {
    /* Left side is on some edge of the triangle. */
    if (state_v2 == LRT_INSIDE_TRIANGLE) {
      /* |l------r   | */
      INTERSECT_JUST_SMALLER(cross_ratios, cross_order, DBL_TRIANGLE_LIM, cross_v1);
      INTERSECT_JUST_GREATER(cross_ratios, cross_order, 1 - DBL_TRIANGLE_LIM, cross_v2);
    }
    else if (state_v2 == LRT_ON_TRIANGLE) {
      /* |l---------r| */
      INTERSECT_JUST_SMALLER(cross_ratios, cross_order, DBL_TRIANGLE_LIM, cross_v1);
      INTERSECT_JUST_GREATER(cross_ratios, cross_order, 1 - DBL_TRIANGLE_LIM, cross_v2);
    }
    else if (state_v2 == LRT_OUTSIDE_TRIANGLE) {
      /*           |l----------|-------r (crossing the triangle) [OR]
       * r---------|l          |         (not crossing the triangle) */
      INTERSECT_JUST_GREATER(cross_ratios, cross_order, DBL_TRIANGLE_LIM, cross_v2);
      if (cross_v2 >= 0 && LRT_ISEC(cross_v2) && cross_ratios[cross_v2] > (DBL_TRIANGLE_LIM)) {
        INTERSECT_JUST_SMALLER(cross_ratios, cross_order, DBL_TRIANGLE_LIM, cross_v1);
      }
      else {
        INTERSECT_JUST_SMALLER(cross_ratios, cross_order, DBL_TRIANGLE_LIM, cross_v2);
        if (cross_v2 > 0) {
          INTERSECT_JUST_SMALLER(cross_ratios, cross_order, cross_ratios[cross_v2], cross_v1);
        }
      }
      LRT_GUARD_NOT_FOUND
      /* We could have the edge being completely parallel to the triangle where there isn't a
       * viable occlusion result. */
      if ((LRT_PARALLEL(cross_v1) && !LRT_ISEC(cross_v1)) ||
          (LRT_PARALLEL(cross_v2) && !LRT_ISEC(cross_v2)))
      {
        return false;
      }
    }
  }
  else if (state_v1 == LRT_OUTSIDE_TRIANGLE) {
    /* Left side is outside of the triangle. */
    if (state_v2 == LRT_INSIDE_TRIANGLE) {
      /* l---|---r   | */
      INTERSECT_JUST_SMALLER(cross_ratios, cross_order, 1 - DBL_TRIANGLE_LIM, cross_v1);
      INTERSECT_JUST_GREATER(cross_ratios, cross_order, 1 - DBL_TRIANGLE_LIM, cross_v2);
    }
    else if (state_v2 == LRT_ON_TRIANGLE) {
      /*           |r----------|-------l (crossing the triangle) [OR]
       * l---------|r          |         (not crossing the triangle) */
      INTERSECT_JUST_SMALLER(cross_ratios, cross_order, 1 - DBL_TRIANGLE_LIM, cross_v1);
      if (cross_v1 >= 0 && LRT_ISEC(cross_v1) && cross_ratios[cross_v1] < (1 - DBL_TRIANGLE_LIM)) {
        INTERSECT_JUST_GREATER(cross_ratios, cross_order, 1 - DBL_TRIANGLE_LIM, cross_v2);
      }
      else {
        INTERSECT_JUST_GREATER(cross_ratios, cross_order, 1 - DBL_TRIANGLE_LIM, cross_v1);
        if (cross_v1 > 0) {
          INTERSECT_JUST_GREATER(cross_ratios, cross_order, cross_ratios[cross_v1], cross_v2);
        }
      }
      LRT_GUARD_NOT_FOUND
      /* The same logic applies as above case. */
      if ((LRT_PARALLEL(cross_v1) && !LRT_ISEC(cross_v1)) ||
          (LRT_PARALLEL(cross_v2) && !LRT_ISEC(cross_v2)))
      {
        return false;
      }
    }
    else if (state_v2 == LRT_OUTSIDE_TRIANGLE) {
      /*      l---|----|----r (crossing the triangle) [OR]
       * l----r   |    |      (not crossing the triangle) */
      INTERSECT_JUST_GREATER(cross_ratios, cross_order, -DBL_TRIANGLE_LIM, cross_v1);
      if (cross_v1 >= 0 && LRT_ISEC(cross_v1)) {
        INTERSECT_JUST_GREATER(cross_ratios, cross_order, cross_ratios[cross_v1], cross_v2);
      }
      else {
        if (cross_v1 >= 0) {
          INTERSECT_JUST_GREATER(cross_ratios, cross_order, cross_ratios[cross_v1], cross_v1);
          if (cross_v1 >= 0) {
            INTERSECT_JUST_GREATER(cross_ratios, cross_order, cross_ratios[cross_v1], cross_v2);
          }
        }
      }
    }
  }

  LRT_GUARD_NOT_FOUND

  double dot_1f = dot_v1 * dot_f, dot_2f = dot_v2 * dot_f;

  /* Determine the start and end point of image space cut on a line. */
  if (dot_1f <= 0 && dot_2f <= 0 && (dot_v1 || dot_v2)) {
    *from = std::max(0.0, cross_ratios[cross_v1]);
    *to = std::min(1.0, cross_ratios[cross_v2]);
    if (*from >= *to) {
      return false;
    }
    return true;
  }
  if (dot_1f >= 0 && dot_2f <= 0 && (dot_v1 || dot_v2)) {
    *from = std::max(cut, cross_ratios[cross_v1]);
    *to = std::min(1.0, cross_ratios[cross_v2]);
    if (*from >= *to) {
      return false;
    }
    return true;
  }
  if (dot_1f <= 0 && dot_2f >= 0 && (dot_v1 || dot_v2)) {
    *from = std::max(0.0, cross_ratios[cross_v1]);
    *to = std::min(cut, cross_ratios[cross_v2]);
    if (*from >= *to) {
      return false;
    }
    return true;
  }

  /* Unlikely, but here's the default failed value if anything fall through. */
  return false;
}

#undef INTERSECT_SORT_MIN_TO_MAX_3
#undef INTERSECT_JUST_GREATER
#undef INTERSECT_JUST_SMALLER
#undef LRT_ISEC
#undef LRT_PARALLEL

/**
 * At this stage of the computation we don't have triangle adjacent info anymore,
 * so we can only compare the global vert index.
 */
static bool lineart_triangle_share_edge(const LineartTriangle *l, const LineartTriangle *r)
{
  if (l->v[0]->index == r->v[0]->index) {
    if (l->v[1]->index == r->v[1]->index || l->v[1]->index == r->v[2]->index ||
        l->v[2]->index == r->v[2]->index || l->v[2]->index == r->v[1]->index)
    {
      return true;
    }
  }
  if (l->v[0]->index == r->v[1]->index) {
    if (l->v[1]->index == r->v[0]->index || l->v[1]->index == r->v[2]->index ||
        l->v[2]->index == r->v[2]->index || l->v[2]->index == r->v[0]->index)
    {
      return true;
    }
  }
  if (l->v[0]->index == r->v[2]->index) {
    if (l->v[1]->index == r->v[1]->index || l->v[1]->index == r->v[0]->index ||
        l->v[2]->index == r->v[0]->index || l->v[2]->index == r->v[1]->index)
    {
      return true;
    }
  }
  if (l->v[1]->index == r->v[0]->index) {
    if (l->v[2]->index == r->v[1]->index || l->v[2]->index == r->v[2]->index ||
        l->v[0]->index == r->v[2]->index || l->v[0]->index == r->v[1]->index)
    {
      return true;
    }
  }
  if (l->v[1]->index == r->v[1]->index) {
    if (l->v[2]->index == r->v[0]->index || l->v[2]->index == r->v[2]->index ||
        l->v[0]->index == r->v[2]->index || l->v[0]->index == r->v[0]->index)
    {
      return true;
    }
  }
  if (l->v[1]->index == r->v[2]->index) {
    if (l->v[2]->index == r->v[1]->index || l->v[2]->index == r->v[0]->index ||
        l->v[0]->index == r->v[0]->index || l->v[0]->index == r->v[1]->index)
    {
      return true;
    }
  }

  /* Otherwise not possible. */
  return false;
}

static LineartVert *lineart_triangle_share_point(const LineartTriangle *l,
                                                 const LineartTriangle *r)
{
  if (l->v[0] == r->v[0]) {
    return r->v[0];
  }
  if (l->v[0] == r->v[1]) {
    return r->v[1];
  }
  if (l->v[0] == r->v[2]) {
    return r->v[2];
  }
  if (l->v[1] == r->v[0]) {
    return r->v[0];
  }
  if (l->v[1] == r->v[1]) {
    return r->v[1];
  }
  if (l->v[1] == r->v[2]) {
    return r->v[2];
  }
  if (l->v[2] == r->v[0]) {
    return r->v[0];
  }
  if (l->v[2] == r->v[1]) {
    return r->v[1];
  }
  if (l->v[2] == r->v[2]) {
    return r->v[2];
  }
  return nullptr;
}

static bool lineart_triangle_2v_intersection_math(
    LineartVert *v1, LineartVert *v2, LineartTriangle *tri, const double *last, double *rv)
{
  /* Direction vectors for the edge verts. We will check if the verts are on the same side of the
   * triangle or not. */
  double dir_v1[3], dir_v2[3];
  double dot_v1, dot_v2;
  double gloc[3];

  sub_v3_v3v3_db(dir_v1, v1->gloc, tri->v[0]->gloc);
  sub_v3_v3v3_db(dir_v2, v2->gloc, tri->v[0]->gloc);

  dot_v1 = dot_v3v3_db(dir_v1, tri->gn);
  dot_v2 = dot_v3v3_db(dir_v2, tri->gn);

  if (dot_v1 * dot_v2 > 0 || (!dot_v1 && !dot_v2)) {
    return false;
  }

  dot_v1 = fabs(dot_v1);
  dot_v2 = fabs(dot_v2);

  interp_v3_v3v3_db(gloc, v1->gloc, v2->gloc, dot_v1 / (dot_v1 + dot_v2));

  /* Due to precision issue, we might end up with the same point as the one we already detected. */
  if (last && LRT_DOUBLE_CLOSE_ENOUGH(last[0], gloc[0]) &&
      LRT_DOUBLE_CLOSE_ENOUGH(last[1], gloc[1]) && LRT_DOUBLE_CLOSE_ENOUGH(last[2], gloc[2]))
  {
    return false;
  }

  if (!lineart_point_inside_triangle3d(gloc, tri->v[0]->gloc, tri->v[1]->gloc, tri->v[2]->gloc)) {
    return false;
  }

  copy_v3_v3_db(rv, gloc);

  return true;
}

static bool lineart_triangle_intersect_math(LineartTriangle *tri,
                                            LineartTriangle *t2,
                                            double *v1,
                                            double *v2)
{
  double *next = v1, *last = nullptr;
  LineartVert *sv1, *sv2;

  LineartVert *share = lineart_triangle_share_point(t2, tri);

  if (share) {
    /* If triangles have sharing points like `abc` and `acd`, then we only need to detect `bc`
     * against `acd` or `cd` against `abc`. */

    lineart_triangle_get_other_verts(tri, share, &sv1, &sv2);

    copy_v3_v3_db(v1, share->gloc);

    if (!lineart_triangle_2v_intersection_math(sv1, sv2, t2, nullptr, v2)) {
      lineart_triangle_get_other_verts(t2, share, &sv1, &sv2);
      if (lineart_triangle_2v_intersection_math(sv1, sv2, tri, nullptr, v2)) {
        return true;
      }
    }
  }
  else {
    /* If not sharing any points, then we need to try all the possibilities. */

    if (lineart_triangle_2v_intersection_math(tri->v[0], tri->v[1], t2, nullptr, v1)) {
      next = v2;
      last = v1;
    }

    if (lineart_triangle_2v_intersection_math(tri->v[1], tri->v[2], t2, last, next)) {
      if (last) {
        return true;
      }
      next = v2;
      last = v1;
    }
    if (lineart_triangle_2v_intersection_math(tri->v[2], tri->v[0], t2, last, next)) {
      if (last) {
        return true;
      }
      next = v2;
      last = v1;
    }

    if (lineart_triangle_2v_intersection_math(t2->v[0], t2->v[1], tri, last, next)) {
      if (last) {
        return true;
      }
      next = v2;
      last = v1;
    }
    if (lineart_triangle_2v_intersection_math(t2->v[1], t2->v[2], tri, last, next)) {
      if (last) {
        return true;
      }
      next = v2;
      last = v1;
    }
    if (lineart_triangle_2v_intersection_math(t2->v[2], t2->v[0], tri, last, next)) {
      if (last) {
        return true;
      }
      next = v2;
      last = v1;
    }
  }
  return false;
}

static void lineart_add_isec_thread(LineartIsecThread *th,
                                    const double *v1,
                                    const double *v2,
                                    LineartTriangle *tri1,
                                    LineartTriangle *tri2)
{
  if (th->current == th->max) {

    LineartIsecSingle *new_array = MEM_malloc_arrayN<LineartIsecSingle>(size_t(th->max) * 2,
                                                                        "LineartIsecSingle");
    memcpy(new_array, th->array, sizeof(LineartIsecSingle) * th->max);
    th->max *= 2;
    MEM_freeN(th->array);
    th->array = new_array;
  }
  LineartIsecSingle *isec_single = &th->array[th->current];
  copy_v3_v3_db(isec_single->v1, v1);
  copy_v3_v3_db(isec_single->v2, v2);
  isec_single->tri1 = tri1;
  isec_single->tri2 = tri2;
  if (tri1->target_reference > tri2->target_reference) {
    std::swap(isec_single->tri1, isec_single->tri2);
  }
  th->current++;
}

#define LRT_ISECT_TRIANGLE_PER_THREAD 4096

static bool lineart_schedule_new_triangle_task(LineartIsecThread *th)
{
  LineartData *ld = th->ld;
  int remaining = LRT_ISECT_TRIANGLE_PER_THREAD;

  BLI_spin_lock(&ld->lock_task);
  LineartElementLinkNode *eln = ld->isect_scheduled_up_to;

  if (!eln) {
    BLI_spin_unlock(&ld->lock_task);
    return false;
  }

  th->pending_from = eln;
  th->index_from = ld->isect_scheduled_up_to_index;

  while (remaining > 0 && eln) {
    int remaining_this_eln = eln->element_count - ld->isect_scheduled_up_to_index;
    int added_count = std::min(remaining, remaining_this_eln);
    remaining -= added_count;
    if (remaining || added_count == remaining_this_eln) {
      eln = eln->next;
      ld->isect_scheduled_up_to = eln;
      ld->isect_scheduled_up_to_index = 0;
    }
    else {
      ld->isect_scheduled_up_to_index += added_count;
    }
  }

  th->pending_to = eln ? eln :
                         static_cast<LineartElementLinkNode *>(
                             ld->geom.triangle_buffer_pointers.last);
  th->index_to = ld->isect_scheduled_up_to_index;

  BLI_spin_unlock(&ld->lock_task);

  return true;
}

/* This function initializes two things:
 * 1) Triangle array scheduling info, for each worker thread to get its chunk from the scheduler.
 * 2) Per-thread intersection result array. Does not store actual #LineartEdge, these results will
 * be finalized by #lineart_create_edges_from_isec_data
 */
static void lineart_init_isec_thread(LineartIsecData *d, LineartData *ld, int thread_count)
{
  d->threads = MEM_calloc_arrayN<LineartIsecThread>(thread_count, "LineartIsecThread arr");
  d->ld = ld;
  d->thread_count = thread_count;

  ld->isect_scheduled_up_to = static_cast<LineartElementLinkNode *>(
      ld->geom.triangle_buffer_pointers.first);
  ld->isect_scheduled_up_to_index = 0;

  for (int i = 0; i < thread_count; i++) {
    LineartIsecThread *it = &d->threads[i];
    it->array = MEM_malloc_arrayN<LineartIsecSingle>(100, "LineartIsecSingle arr");
    it->max = 100;
    it->current = 0;
    it->thread_id = i;
    it->ld = ld;
  }
}

static void lineart_destroy_isec_thread(LineartIsecData *d)
{
  for (int i = 0; i < d->thread_count; i++) {
    LineartIsecThread *it = &d->threads[i];
    MEM_freeN(it->array);
  }
  MEM_freeN(d->threads);
}

static void lineart_triangle_intersect_in_bounding_area(LineartTriangle *tri,
                                                        LineartBoundingArea *ba,
                                                        LineartIsecThread *th,
                                                        int up_to)
{
  BLI_assert(th != nullptr);

  if (!th) {
    return;
  }

  double *G0 = tri->v[0]->gloc, *G1 = tri->v[1]->gloc, *G2 = tri->v[2]->gloc;

  /* If this _is_ the smallest subdivision bounding area, then do the intersections there. */
  for (int i = 0; i < up_to; i++) {
    /* Testing_triangle->testing[0] is used to store pairing triangle reference.
     * See definition of LineartTriangleThread for more info. */
    LineartTriangle *testing_triangle = ba->linked_triangles[i];
    LineartTriangleThread *tt = (LineartTriangleThread *)testing_triangle;

    if (testing_triangle == tri || tt->testing_e[th->thread_id] == (LineartEdge *)tri) {
      continue;
    }
    tt->testing_e[th->thread_id] = (LineartEdge *)tri;

    if (!((testing_triangle->flags | tri->flags) & LRT_TRIANGLE_FORCE_INTERSECTION)) {
      if (((testing_triangle->flags | tri->flags) & LRT_TRIANGLE_NO_INTERSECTION) ||
          (testing_triangle->flags & tri->flags & LRT_TRIANGLE_INTERSECTION_ONLY))
      {
        continue;
      }
    }

    double *RG0 = testing_triangle->v[0]->gloc, *RG1 = testing_triangle->v[1]->gloc,
           *RG2 = testing_triangle->v[2]->gloc;

    /* Bounding box not overlapping or triangles share edges, not potential of intersecting. */
    if ((std::min({G0[2], G1[2], G2[2]}) > std::max({RG0[2], RG1[2], RG2[2]})) ||
        (std::max({G0[2], G1[2], G2[2]}) < std::min({RG0[2], RG1[2], RG2[2]})) ||
        (std::min({G0[0], G1[0], G2[0]}) > std::max({RG0[0], RG1[0], RG2[0]})) ||
        (std::max({G0[0], G1[0], G2[0]}) < std::min({RG0[0], RG1[0], RG2[0]})) ||
        (std::min({G0[1], G1[1], G2[1]}) > std::max({RG0[1], RG1[1], RG2[1]})) ||
        (std::max({G0[1], G1[1], G2[1]}) < std::min({RG0[1], RG1[1], RG2[1]})) ||
        lineart_triangle_share_edge(tri, testing_triangle))
    {
      continue;
    }

    /* If we do need to compute intersection, then finally do it. */

    double iv1[3], iv2[3];
    if (lineart_triangle_intersect_math(tri, testing_triangle, iv1, iv2)) {
      lineart_add_isec_thread(th, iv1, iv2, tri, testing_triangle);
    }
  }
}

void lineart_main_get_view_vector(LineartData *ld)
{
  float direction[3] = {0, 0, 1};
  float trans[3];
  float inv[4][4];
  float obmat_no_scale[4][4];

  copy_m4_m4(obmat_no_scale, ld->conf.cam_obmat);
  normalize_v3(obmat_no_scale[0]);
  normalize_v3(obmat_no_scale[1]);
  normalize_v3(obmat_no_scale[2]);
  invert_m4_m4(inv, obmat_no_scale);
  transpose_m4(inv);
  mul_v3_mat3_m4v3(trans, inv, direction);
  copy_m4_m4(ld->conf.cam_obmat, obmat_no_scale);
  copy_v3db_v3fl(ld->conf.view_vector, trans);

  if (ld->conf.light_reference_available) {
    copy_m4_m4(obmat_no_scale, ld->conf.cam_obmat_secondary);
    normalize_v3(obmat_no_scale[0]);
    normalize_v3(obmat_no_scale[1]);
    normalize_v3(obmat_no_scale[2]);
    invert_m4_m4(inv, obmat_no_scale);
    transpose_m4(inv);
    mul_v3_mat3_m4v3(trans, inv, direction);
    copy_m4_m4(ld->conf.cam_obmat_secondary, obmat_no_scale);
    copy_v3db_v3fl(ld->conf.view_vector_secondary, trans);
  }
}

static void lineart_end_bounding_area_recursive(LineartBoundingArea *ba)
{
  BLI_spin_end(&ba->lock);
  if (ba->child) {
    for (int i = 0; i < 4; i++) {
      lineart_end_bounding_area_recursive(&ba->child[i]);
    }
  }
}

void lineart_destroy_render_data_keep_init(LineartData *ld)
{
  if (ld == nullptr) {
    return;
  }

  BLI_listbase_clear(&ld->chains);
  BLI_listbase_clear(&ld->wasted_cuts);

  BLI_listbase_clear(&ld->geom.vertex_buffer_pointers);
  BLI_listbase_clear(&ld->geom.line_buffer_pointers);
  BLI_listbase_clear(&ld->geom.triangle_buffer_pointers);

  if (ld->pending_edges.array) {
    MEM_freeN(ld->pending_edges.array);
  }

  for (int i = 0; i < ld->qtree.initial_tile_count; i++) {
    lineart_end_bounding_area_recursive(&ld->qtree.initials[i]);
  }
  lineart_free_bounding_area_memories(ld);

  lineart_mem_destroy(&ld->render_data_pool);
}

static void lineart_destroy_render_data(LineartData *ld)
{
  if (ld == nullptr) {
    return;
  }

  BLI_spin_end(&ld->lock_task);
  BLI_spin_end(&ld->lock_cuts);
  BLI_spin_end(&ld->render_data_pool.lock_mem);

  lineart_destroy_render_data_keep_init(ld);

  lineart_mem_destroy(&ld->render_data_pool);
}

void MOD_lineart_destroy_render_data_v3(GreasePencilLineartModifierData *lmd)
{
  LineartData *ld = lmd->la_data_ptr;

  lineart_destroy_render_data(ld);

  if (ld) {
    MEM_freeN(ld);
    lmd->la_data_ptr = nullptr;
  }

  if (G.debug_value == 4000) {
    printf("LRT: Destroyed render data.\n");
  }
}

LineartCache *MOD_lineart_init_cache()
{
  LineartCache *lc = MEM_callocN<LineartCache>("Lineart Cache");
  return lc;
}

void MOD_lineart_clear_cache(LineartCache **lc)
{
  if (!(*lc)) {
    return;
  }
  lineart_mem_destroy(&((*lc)->chain_data_pool));
  MEM_freeN(*lc);
  (*lc) = nullptr;
}

static LineartData *lineart_create_render_buffer_v3(Scene *scene,
                                                    GreasePencilLineartModifierData *lmd,
                                                    Object *camera,
                                                    Object *active_camera,
                                                    LineartCache *lc)
{
  LineartData *ld = MEM_callocN<LineartData>("Line Art render buffer");
  lmd->cache = lc;
  lmd->la_data_ptr = ld;
  lc->all_enabled_edge_types = lmd->edge_types_override;

  if (!scene || !camera || !lc) {
    return nullptr;
  }
  const Camera *c = static_cast<Camera *>(camera->data);
  double clipping_offset = 0;

  if (lmd->calculation_flags & MOD_LINEART_ALLOW_CLIPPING_BOUNDARIES) {
    /* This way the clipped lines are "stably visible" by prevents depth buffer artifacts. */
    clipping_offset = 0.0001;
  }

  copy_v3db_v3fl(ld->conf.camera_pos, camera->object_to_world().location());
  if (active_camera) {
    copy_v3db_v3fl(ld->conf.active_camera_pos, active_camera->object_to_world().location());
  }
  copy_m4_m4(ld->conf.cam_obmat, camera->object_to_world().ptr());
  /* Make sure none of the scaling factor makes in, line art expects no scaling on cameras and
   * lights. */
  normalize_v3(ld->conf.cam_obmat[0]);
  normalize_v3(ld->conf.cam_obmat[1]);
  normalize_v3(ld->conf.cam_obmat[2]);

  ld->conf.cam_is_persp = (c->type == CAM_PERSP);
  ld->conf.near_clip = c->clip_start + clipping_offset;
  ld->conf.far_clip = c->clip_end - clipping_offset;
  ld->w = scene->r.xsch;
  ld->h = scene->r.ysch;

  if (ld->conf.cam_is_persp) {
    ld->qtree.recursive_level = LRT_TILE_RECURSIVE_PERSPECTIVE;
  }
  else {
    ld->qtree.recursive_level = LRT_TILE_RECURSIVE_ORTHO;
  }

  double asp = double(ld->w) / double(ld->h);
  int fit = BKE_camera_sensor_fit(c->sensor_fit, ld->w, ld->h);
  ld->conf.shift_x = fit == CAMERA_SENSOR_FIT_HOR ? c->shiftx : c->shiftx / asp;
  ld->conf.shift_y = fit == CAMERA_SENSOR_FIT_VERT ? c->shifty : c->shifty * asp;

  ld->conf.overscan = lmd->overscan;

  ld->conf.shift_x /= (1 + ld->conf.overscan);
  ld->conf.shift_y /= (1 + ld->conf.overscan);

  if (lmd->light_contour_object) {
    Object *light_obj = lmd->light_contour_object;
    copy_v3db_v3fl(ld->conf.camera_pos_secondary, light_obj->object_to_world().location());
    copy_m4_m4(ld->conf.cam_obmat_secondary, light_obj->object_to_world().ptr());
    /* Make sure none of the scaling factor makes in, line art expects no scaling on cameras and
     * lights. */
    normalize_v3(ld->conf.cam_obmat_secondary[0]);
    normalize_v3(ld->conf.cam_obmat_secondary[1]);
    normalize_v3(ld->conf.cam_obmat_secondary[2]);
    ld->conf.light_reference_available = true;
    if (light_obj->type == OB_LAMP) {
      ld->conf.cam_is_persp_secondary = ((Light *)light_obj->data)->type != LA_SUN;
    }
  }

  ld->conf.crease_threshold = cos(M_PI - lmd->crease_threshold);
  ld->conf.chaining_image_threshold = lmd->chaining_image_threshold;
  ld->conf.angle_splitting_threshold = lmd->angle_splitting_threshold;
  ld->conf.chain_smooth_tolerance = lmd->chain_smooth_tolerance;

  ld->conf.fuzzy_intersections = (lmd->calculation_flags & MOD_LINEART_INTERSECTION_AS_CONTOUR) !=
                                 0;
  ld->conf.fuzzy_everything = (lmd->calculation_flags & MOD_LINEART_EVERYTHING_AS_CONTOUR) != 0;
  ld->conf.allow_boundaries = (lmd->calculation_flags & MOD_LINEART_ALLOW_CLIPPING_BOUNDARIES) !=
                              0;
  ld->conf.use_loose_as_contour = (lmd->calculation_flags & MOD_LINEART_LOOSE_AS_CONTOUR) != 0;
  ld->conf.use_loose_edge_chain = (lmd->calculation_flags & MOD_LINEART_CHAIN_LOOSE_EDGES) != 0;
  ld->conf.use_geometry_space_chain = (lmd->calculation_flags &
                                       MOD_LINEART_CHAIN_GEOMETRY_SPACE) != 0;
  ld->conf.use_image_boundary_trimming = (lmd->calculation_flags &
                                          MOD_LINEART_USE_IMAGE_BOUNDARY_TRIMMING) != 0;

  /* See lineart_edge_from_triangle() for how this option may impact performance. */
  ld->conf.allow_overlapping_edges = (lmd->calculation_flags &
                                      MOD_LINEART_ALLOW_OVERLAPPING_EDGES) != 0;

  ld->conf.allow_duplicated_types = (lmd->calculation_flags &
                                     MOD_LINEART_ALLOW_OVERLAP_EDGE_TYPES) != 0;

  ld->conf.force_crease = (lmd->calculation_flags & MOD_LINEART_USE_CREASE_ON_SMOOTH_SURFACES) !=
                          0;
  ld->conf.sharp_as_crease = (lmd->calculation_flags & MOD_LINEART_USE_CREASE_ON_SHARP_EDGES) != 0;

  ld->conf.chain_preserve_details = (lmd->calculation_flags &
                                     MOD_LINEART_CHAIN_PRESERVE_DETAILS) != 0;

  /* This is used to limit calculation to a certain level to save time, lines who have higher
   * occlusion levels will get ignored. */
  ld->conf.max_occlusion_level = lmd->level_end_override;

  int16_t edge_types = lmd->edge_types_override;

  /* lmd->edge_types_override contains all used flags in the modifier stack. */
  ld->conf.use_contour = (edge_types & MOD_LINEART_EDGE_FLAG_CONTOUR) != 0;
  ld->conf.use_crease = (edge_types & MOD_LINEART_EDGE_FLAG_CREASE) != 0;
  ld->conf.use_material = (edge_types & MOD_LINEART_EDGE_FLAG_MATERIAL) != 0;
  ld->conf.use_edge_marks = (edge_types & MOD_LINEART_EDGE_FLAG_EDGE_MARK) != 0;
  ld->conf.use_intersections = (edge_types & MOD_LINEART_EDGE_FLAG_INTERSECTION) != 0;
  ld->conf.use_loose = (edge_types & MOD_LINEART_EDGE_FLAG_LOOSE) != 0;
  ld->conf.use_light_contour = ((edge_types & MOD_LINEART_EDGE_FLAG_LIGHT_CONTOUR) != 0 &&
                                (lmd->light_contour_object != nullptr));
  ld->conf.use_shadow = ((edge_types & MOD_LINEART_EDGE_FLAG_PROJECTED_SHADOW) != 0 &&
                         (lmd->light_contour_object != nullptr));

  ld->conf.shadow_selection = lmd->shadow_selection_override;
  ld->conf.shadow_enclose_shapes = lmd->shadow_selection_override ==
                                   LINEART_SHADOW_FILTER_ILLUMINATED_ENCLOSED_SHAPES;
  ld->conf.shadow_use_silhouette = lmd->shadow_use_silhouette_override != 0;

  ld->conf.use_back_face_culling = (lmd->calculation_flags & MOD_LINEART_USE_BACK_FACE_CULLING) !=
                                   0;

  ld->conf.filter_face_mark_invert = (lmd->calculation_flags &
                                      MOD_LINEART_FILTER_FACE_MARK_INVERT) != 0;
  ld->conf.filter_face_mark = (lmd->calculation_flags & MOD_LINEART_FILTER_FACE_MARK) != 0;
  ld->conf.filter_face_mark_boundaries = (lmd->calculation_flags &
                                          MOD_LINEART_FILTER_FACE_MARK_BOUNDARIES) != 0;
  ld->conf.filter_face_mark_keep_contour = (lmd->calculation_flags &
                                            MOD_LINEART_FILTER_FACE_MARK_KEEP_CONTOUR) != 0;

  ld->chain_data_pool = &lc->chain_data_pool;

  /* See #LineartData::edge_data_pool for explanation. */
  ld->edge_data_pool = &ld->render_data_pool;

  BLI_spin_init(&ld->lock_task);
  BLI_spin_init(&ld->lock_cuts);
  BLI_spin_init(&ld->render_data_pool.lock_mem);

  ld->thread_count = BKE_render_num_threads(&scene->r);

  return ld;
}

static int lineart_triangle_size_get(LineartData *ld)
{
  return sizeof(LineartTriangle) + (sizeof(LineartEdge *) * (ld->thread_count));
}

void lineart_main_bounding_area_make_initial(LineartData *ld)
{
  /* Initial tile split is defined as 4 (subdivided as 4*4), increasing the value allows the
   * algorithm to build the acceleration structure for bigger scenes a little faster but not as
   * efficient at handling medium to small scenes. */
  int sp_w = LRT_BA_ROWS;
  int sp_h = LRT_BA_ROWS;
  int row, col;
  LineartBoundingArea *ba;

  /* Always make sure the shortest side has at least LRT_BA_ROWS tiles. */
  if (ld->w > ld->h) {
    sp_w = sp_h * ld->w / ld->h;
  }
  else {
    sp_h = sp_w * ld->h / ld->w;
  }

  /* Because NDC (Normalized Device Coordinates) range is (-1,1),
   * so the span for each initial tile is double of that in the (0,1) range. */
  double span_w = 1.0 / sp_w * 2.0;
  double span_h = 1.0 / sp_h * 2.0;

  ld->qtree.count_x = sp_w;
  ld->qtree.count_y = sp_h;
  ld->qtree.tile_width = span_w;
  ld->qtree.tile_height = span_h;

  ld->qtree.initial_tile_count = sp_w * sp_h;
  ld->qtree.initials = static_cast<LineartBoundingArea *>(lineart_mem_acquire(
      &ld->render_data_pool, sizeof(LineartBoundingArea) * ld->qtree.initial_tile_count));
  for (int i = 0; i < ld->qtree.initial_tile_count; i++) {
    BLI_spin_init(&ld->qtree.initials[i].lock);
  }

  /* Initialize tiles. */
  for (row = 0; row < sp_h; row++) {
    for (col = 0; col < sp_w; col++) {
      ba = &ld->qtree.initials[row * ld->qtree.count_x + col];

      /* Set the four direction limits. */
      ba->l = span_w * col - 1.0;
      ba->r = (col == sp_w - 1) ? 1.0 : (span_w * (col + 1) - 1.0);
      ba->u = 1.0 - span_h * row;
      ba->b = (row == sp_h - 1) ? -1.0 : (1.0 - span_h * (row + 1));

      ba->cx = (ba->l + ba->r) / 2;
      ba->cy = (ba->u + ba->b) / 2;

      /* Init linked_triangles array. */
      ba->max_triangle_count = LRT_TILE_SPLITTING_TRIANGLE_LIMIT;
      ba->max_line_count = LRT_TILE_EDGE_COUNT_INITIAL;
      ba->linked_triangles = MEM_calloc_arrayN<LineartTriangle *>(ba->max_triangle_count,
                                                                  "ba_linked_triangles");
      ba->linked_lines = MEM_calloc_arrayN<LineartEdge *>(ba->max_line_count, "ba_linked_lines");

      BLI_spin_init(&ba->lock);
    }
  }
}

/**
 * Re-link adjacent tiles after one gets subdivided.
 */
static void lineart_bounding_areas_connect_new(LineartData *ld, LineartBoundingArea *root)
{
  LineartBoundingArea *ba = root->child, *tba;
  LinkData *lip2, *next_lip;
  LineartStaticMemPool *mph = &ld->render_data_pool;

  /* Inter-connection with newly created 4 child bounding areas. */
  lineart_list_append_pointer_pool(&ba[1].rp, mph, &ba[0]);
  lineart_list_append_pointer_pool(&ba[0].lp, mph, &ba[1]);
  lineart_list_append_pointer_pool(&ba[1].bp, mph, &ba[2]);
  lineart_list_append_pointer_pool(&ba[2].up, mph, &ba[1]);
  lineart_list_append_pointer_pool(&ba[2].rp, mph, &ba[3]);
  lineart_list_append_pointer_pool(&ba[3].lp, mph, &ba[2]);
  lineart_list_append_pointer_pool(&ba[3].up, mph, &ba[0]);
  lineart_list_append_pointer_pool(&ba[0].bp, mph, &ba[3]);

  /* Connect 4 child bounding areas to other areas that are
   * adjacent to their original parents. */
  LISTBASE_FOREACH (LinkData *, lip, &root->lp) {

    /* For example, we are dealing with parent's left side
     * "tba" represents each adjacent neighbor of the parent. */
    tba = static_cast<LineartBoundingArea *>(lip->data);

    /* if this neighbor is adjacent to
     * the two new areas on the left side of the parent,
     * then add them to the adjacent list as well. */
    if (ba[1].u > tba->b && ba[1].b < tba->u) {
      lineart_list_append_pointer_pool(&ba[1].lp, mph, tba);
      lineart_list_append_pointer_pool(&tba->rp, mph, &ba[1]);
    }
    if (ba[2].u > tba->b && ba[2].b < tba->u) {
      lineart_list_append_pointer_pool(&ba[2].lp, mph, tba);
      lineart_list_append_pointer_pool(&tba->rp, mph, &ba[2]);
    }
  }
  LISTBASE_FOREACH (LinkData *, lip, &root->rp) {
    tba = static_cast<LineartBoundingArea *>(lip->data);
    if (ba[0].u > tba->b && ba[0].b < tba->u) {
      lineart_list_append_pointer_pool(&ba[0].rp, mph, tba);
      lineart_list_append_pointer_pool(&tba->lp, mph, &ba[0]);
    }
    if (ba[3].u > tba->b && ba[3].b < tba->u) {
      lineart_list_append_pointer_pool(&ba[3].rp, mph, tba);
      lineart_list_append_pointer_pool(&tba->lp, mph, &ba[3]);
    }
  }
  LISTBASE_FOREACH (LinkData *, lip, &root->up) {
    tba = static_cast<LineartBoundingArea *>(lip->data);
    if (ba[0].r > tba->l && ba[0].l < tba->r) {
      lineart_list_append_pointer_pool(&ba[0].up, mph, tba);
      lineart_list_append_pointer_pool(&tba->bp, mph, &ba[0]);
    }
    if (ba[1].r > tba->l && ba[1].l < tba->r) {
      lineart_list_append_pointer_pool(&ba[1].up, mph, tba);
      lineart_list_append_pointer_pool(&tba->bp, mph, &ba[1]);
    }
  }
  LISTBASE_FOREACH (LinkData *, lip, &root->bp) {
    tba = static_cast<LineartBoundingArea *>(lip->data);
    if (ba[2].r > tba->l && ba[2].l < tba->r) {
      lineart_list_append_pointer_pool(&ba[2].bp, mph, tba);
      lineart_list_append_pointer_pool(&tba->up, mph, &ba[2]);
    }
    if (ba[3].r > tba->l && ba[3].l < tba->r) {
      lineart_list_append_pointer_pool(&ba[3].bp, mph, tba);
      lineart_list_append_pointer_pool(&tba->up, mph, &ba[3]);
    }
  }

  /* Then remove the parent bounding areas from
   * their original adjacent areas. */
  LISTBASE_FOREACH (LinkData *, lip, &root->lp) {
    for (lip2 = static_cast<LinkData *>(((LineartBoundingArea *)lip->data)->rp.first); lip2;
         lip2 = next_lip)
    {
      next_lip = lip2->next;
      tba = static_cast<LineartBoundingArea *>(lip2->data);
      if (tba == root) {
        lineart_list_remove_pointer_item_no_free(&((LineartBoundingArea *)lip->data)->rp, lip2);
        if (ba[1].u > tba->b && ba[1].b < tba->u) {
          lineart_list_append_pointer_pool(&tba->rp, mph, &ba[1]);
        }
        if (ba[2].u > tba->b && ba[2].b < tba->u) {
          lineart_list_append_pointer_pool(&tba->rp, mph, &ba[2]);
        }
      }
    }
  }
  LISTBASE_FOREACH (LinkData *, lip, &root->rp) {
    for (lip2 = static_cast<LinkData *>(((LineartBoundingArea *)lip->data)->lp.first); lip2;
         lip2 = next_lip)
    {
      next_lip = lip2->next;
      tba = static_cast<LineartBoundingArea *>(lip2->data);
      if (tba == root) {
        lineart_list_remove_pointer_item_no_free(&((LineartBoundingArea *)lip->data)->lp, lip2);
        if (ba[0].u > tba->b && ba[0].b < tba->u) {
          lineart_list_append_pointer_pool(&tba->lp, mph, &ba[0]);
        }
        if (ba[3].u > tba->b && ba[3].b < tba->u) {
          lineart_list_append_pointer_pool(&tba->lp, mph, &ba[3]);
        }
      }
    }
  }
  LISTBASE_FOREACH (LinkData *, lip, &root->up) {
    for (lip2 = static_cast<LinkData *>(((LineartBoundingArea *)lip->data)->bp.first); lip2;
         lip2 = next_lip)
    {
      next_lip = lip2->next;
      tba = static_cast<LineartBoundingArea *>(lip2->data);
      if (tba == root) {
        lineart_list_remove_pointer_item_no_free(&((LineartBoundingArea *)lip->data)->bp, lip2);
        if (ba[0].r > tba->l && ba[0].l < tba->r) {
          lineart_list_append_pointer_pool(&tba->up, mph, &ba[0]);
        }
        if (ba[1].r > tba->l && ba[1].l < tba->r) {
          lineart_list_append_pointer_pool(&tba->up, mph, &ba[1]);
        }
      }
    }
  }
  LISTBASE_FOREACH (LinkData *, lip, &root->bp) {
    for (lip2 = static_cast<LinkData *>(((LineartBoundingArea *)lip->data)->up.first); lip2;
         lip2 = next_lip)
    {
      next_lip = lip2->next;
      tba = static_cast<LineartBoundingArea *>(lip2->data);
      if (tba == root) {
        lineart_list_remove_pointer_item_no_free(&((LineartBoundingArea *)lip->data)->up, lip2);
        if (ba[2].r > tba->l && ba[2].l < tba->r) {
          lineart_list_append_pointer_pool(&tba->bp, mph, &ba[2]);
        }
        if (ba[3].r > tba->l && ba[3].l < tba->r) {
          lineart_list_append_pointer_pool(&tba->bp, mph, &ba[3]);
        }
      }
    }
  }

  /* Finally clear parent's adjacent list. */
  BLI_listbase_clear(&root->lp);
  BLI_listbase_clear(&root->rp);
  BLI_listbase_clear(&root->up);
  BLI_listbase_clear(&root->bp);
}

static void lineart_bounding_areas_connect_recursive(LineartData *ld, LineartBoundingArea *root)
{
  if (root->child) {
    lineart_bounding_areas_connect_new(ld, root);
    for (int i = 0; i < 4; i++) {
      lineart_bounding_areas_connect_recursive(ld, &root->child[i]);
    }
  }
}

void lineart_main_bounding_areas_connect_post(LineartData *ld)
{
  int total_tile_initial = ld->qtree.count_x * ld->qtree.count_y;
  int tiles_per_row = ld->qtree.count_x;

  for (int row = 0; row < ld->qtree.count_y; row++) {
    for (int col = 0; col < ld->qtree.count_x; col++) {
      LineartBoundingArea *ba = &ld->qtree.initials[row * tiles_per_row + col];
      /* Link adjacent ones. */
      if (row) {
        lineart_list_append_pointer_pool(
            &ba->up, &ld->render_data_pool, &ld->qtree.initials[(row - 1) * tiles_per_row + col]);
      }
      if (col) {
        lineart_list_append_pointer_pool(
            &ba->lp, &ld->render_data_pool, &ld->qtree.initials[row * tiles_per_row + col - 1]);
      }
      if (row != ld->qtree.count_y - 1) {
        lineart_list_append_pointer_pool(
            &ba->bp, &ld->render_data_pool, &ld->qtree.initials[(row + 1) * tiles_per_row + col]);
      }
      if (col != ld->qtree.count_x - 1) {
        lineart_list_append_pointer_pool(
            &ba->rp, &ld->render_data_pool, &ld->qtree.initials[row * tiles_per_row + col + 1]);
      }
    }
  }
  for (int i = 0; i < total_tile_initial; i++) {
    lineart_bounding_areas_connect_recursive(ld, &ld->qtree.initials[i]);
  }
}

/**
 * Subdivide a tile after one tile contains too many triangles, then re-link triangles into all the
 * child tiles.
 */
static void lineart_bounding_area_split(LineartData *ld,
                                        LineartBoundingArea *root,
                                        int recursive_level)
{
  LineartBoundingArea *ba = static_cast<LineartBoundingArea *>(
      lineart_mem_acquire_thread(&ld->render_data_pool, sizeof(LineartBoundingArea) * 4));
  ba[0].l = root->cx;
  ba[0].r = root->r;
  ba[0].u = root->u;
  ba[0].b = root->cy;
  ba[0].cx = (ba[0].l + ba[0].r) / 2;
  ba[0].cy = (ba[0].u + ba[0].b) / 2;

  ba[1].l = root->l;
  ba[1].r = root->cx;
  ba[1].u = root->u;
  ba[1].b = root->cy;
  ba[1].cx = (ba[1].l + ba[1].r) / 2;
  ba[1].cy = (ba[1].u + ba[1].b) / 2;

  ba[2].l = root->l;
  ba[2].r = root->cx;
  ba[2].u = root->cy;
  ba[2].b = root->b;
  ba[2].cx = (ba[2].l + ba[2].r) / 2;
  ba[2].cy = (ba[2].u + ba[2].b) / 2;

  ba[3].l = root->cx;
  ba[3].r = root->r;
  ba[3].u = root->cy;
  ba[3].b = root->b;
  ba[3].cx = (ba[3].l + ba[3].r) / 2;
  ba[3].cy = (ba[3].u + ba[3].b) / 2;

  /* Init linked_triangles array and locks. */
  for (int i = 0; i < 4; i++) {
    ba[i].max_triangle_count = LRT_TILE_SPLITTING_TRIANGLE_LIMIT;
    ba[i].max_line_count = LRT_TILE_EDGE_COUNT_INITIAL;
    ba[i].linked_triangles = MEM_calloc_arrayN<LineartTriangle *>(ba[i].max_triangle_count,
                                                                  "ba_linked_triangles");
    ba[i].linked_lines = MEM_calloc_arrayN<LineartEdge *>(ba[i].max_line_count, "ba_linked_lines");
    BLI_spin_init(&ba[i].lock);
  }

  for (uint32_t i = 0; i < root->triangle_count; i++) {
    LineartTriangle *tri = root->linked_triangles[i];

    double b[4];
    b[0] = std::min({tri->v[0]->fbcoord[0], tri->v[1]->fbcoord[0], tri->v[2]->fbcoord[0]});
    b[1] = std::max({tri->v[0]->fbcoord[0], tri->v[1]->fbcoord[0], tri->v[2]->fbcoord[0]});
    b[2] = std::max({tri->v[0]->fbcoord[1], tri->v[1]->fbcoord[1], tri->v[2]->fbcoord[1]});
    b[3] = std::min({tri->v[0]->fbcoord[1], tri->v[1]->fbcoord[1], tri->v[2]->fbcoord[1]});

    /* Re-link triangles into child tiles, not doing intersection lines during this because this
     * batch of triangles are all tested with each other for intersections. */
    if (LRT_BOUND_AREA_CROSSES(b, &ba[0].l)) {
      lineart_bounding_area_link_triangle(
          ld, &ba[0], tri, b, 0, recursive_level + 1, false, nullptr);
    }
    if (LRT_BOUND_AREA_CROSSES(b, &ba[1].l)) {
      lineart_bounding_area_link_triangle(
          ld, &ba[1], tri, b, 0, recursive_level + 1, false, nullptr);
    }
    if (LRT_BOUND_AREA_CROSSES(b, &ba[2].l)) {
      lineart_bounding_area_link_triangle(
          ld, &ba[2], tri, b, 0, recursive_level + 1, false, nullptr);
    }
    if (LRT_BOUND_AREA_CROSSES(b, &ba[3].l)) {
      lineart_bounding_area_link_triangle(
          ld, &ba[3], tri, b, 0, recursive_level + 1, false, nullptr);
    }
  }

  /* At this point the child tiles are fully initialized and it's safe for new triangles to be
   * inserted, so assign root->child for #lineart_bounding_area_link_triangle to use. */
  root->child = ba;
}

static bool lineart_bounding_area_edge_intersect(LineartData * /*fb*/,
                                                 const double l[2],
                                                 const double r[2],
                                                 LineartBoundingArea *ba)
{
  double dx, dy;
  double converted[4];
  double c1, c;

  if (((converted[0] = ba->l) > std::max(l[0], r[0])) ||
      ((converted[1] = ba->r) < std::min(l[0], r[0])) ||
      ((converted[2] = ba->b) > std::max(l[1], r[1])) ||
      ((converted[3] = ba->u) < std::min(l[1], r[1])))
  {
    return false;
  }

  dx = l[0] - r[0];
  dy = l[1] - r[1];

  c1 = dx * (converted[2] - l[1]) - dy * (converted[0] - l[0]);
  c = c1;

  c1 = dx * (converted[2] - l[1]) - dy * (converted[1] - l[0]);
  if (c1 * c <= 0) {
    return true;
  }
  c = c1;

  c1 = dx * (converted[3] - l[1]) - dy * (converted[0] - l[0]);
  if (c1 * c <= 0) {
    return true;
  }
  c = c1;

  c1 = dx * (converted[3] - l[1]) - dy * (converted[1] - l[0]);
  if (c1 * c <= 0) {
    return true;
  }
  c = c1;

  return false;
}

static bool lineart_bounding_area_triangle_intersect(LineartData *fb,
                                                     LineartTriangle *tri,
                                                     LineartBoundingArea *ba,
                                                     bool *r_triangle_vert_inside)
{
  double p1[2], p2[2], p3[2], p4[2];
  double *FBC1 = tri->v[0]->fbcoord, *FBC2 = tri->v[1]->fbcoord, *FBC3 = tri->v[2]->fbcoord;

  p3[0] = p1[0] = ba->l;
  p2[1] = p1[1] = ba->b;
  p2[0] = p4[0] = ba->r;
  p3[1] = p4[1] = ba->u;

  if ((FBC1[0] >= p1[0] && FBC1[0] <= p2[0] && FBC1[1] >= p1[1] && FBC1[1] <= p3[1]) ||
      (FBC2[0] >= p1[0] && FBC2[0] <= p2[0] && FBC2[1] >= p1[1] && FBC2[1] <= p3[1]) ||
      (FBC3[0] >= p1[0] && FBC3[0] <= p2[0] && FBC3[1] >= p1[1] && FBC3[1] <= p3[1]))
  {
    *r_triangle_vert_inside = true;
    return true;
  }

  *r_triangle_vert_inside = false;

  if (lineart_point_inside_triangle(p1, FBC1, FBC2, FBC3) ||
      lineart_point_inside_triangle(p2, FBC1, FBC2, FBC3) ||
      lineart_point_inside_triangle(p3, FBC1, FBC2, FBC3) ||
      lineart_point_inside_triangle(p4, FBC1, FBC2, FBC3))
  {
    return true;
  }

  if (lineart_bounding_area_edge_intersect(fb, FBC1, FBC2, ba) ||
      lineart_bounding_area_edge_intersect(fb, FBC2, FBC3, ba) ||
      lineart_bounding_area_edge_intersect(fb, FBC3, FBC1, ba))
  {
    return true;
  }

  return false;
}

/**
 * This function does two things:
 *
 * 1) Builds a quad-tree under ld->InitialBoundingAreas to achieve good geometry separation for
 * fast overlapping test between triangles and lines. This acceleration structure makes the
 * occlusion stage much faster.
 *
 * 2) Test triangles with other triangles that are previously linked into each tile
 * (#LineartBoundingArea) for intersection lines. When splitting the tile into 4 children and
 * re-linking triangles into the child tiles, intersections are inhibited so we don't get
 * duplicated intersection lines.
 */
static void lineart_bounding_area_link_triangle(LineartData *ld,
                                                LineartBoundingArea *root_ba,
                                                LineartTriangle *tri,
                                                double l_r_u_b[4],
                                                int recursive,
                                                int recursive_level,
                                                bool do_intersection,
                                                LineartIsecThread *th)
{
  bool triangle_vert_inside;
  if (!lineart_bounding_area_triangle_intersect(ld, tri, root_ba, &triangle_vert_inside)) {
    return;
  }

  LineartBoundingArea *old_ba = root_ba;

  if (old_ba->child) {
    /* If old_ba->child is not nullptr, then tile splitting is fully finished, safe to directly
     * insert into child tiles. */
    double *B1 = l_r_u_b;
    double b[4];
    if (!l_r_u_b) {
      b[0] = std::min({tri->v[0]->fbcoord[0], tri->v[1]->fbcoord[0], tri->v[2]->fbcoord[0]});
      b[1] = std::max({tri->v[0]->fbcoord[0], tri->v[1]->fbcoord[0], tri->v[2]->fbcoord[0]});
      b[2] = std::max({tri->v[0]->fbcoord[1], tri->v[1]->fbcoord[1], tri->v[2]->fbcoord[1]});
      b[3] = std::min({tri->v[0]->fbcoord[1], tri->v[1]->fbcoord[1], tri->v[2]->fbcoord[1]});
      B1 = b;
    }
    for (int iba = 0; iba < 4; iba++) {
      if (LRT_BOUND_AREA_CROSSES(B1, &old_ba->child[iba].l)) {
        lineart_bounding_area_link_triangle(
            ld, &old_ba->child[iba], tri, B1, recursive, recursive_level + 1, do_intersection, th);
      }
    }
    return;
  }

  /* When splitting tiles, triangles are relinked into new tiles by a single thread, #th is nullptr
   * in that situation. */
  if (th) {
    BLI_spin_lock(&old_ba->lock);
  }

  /* If there are still space left in this tile for insertion. */
  if (old_ba->triangle_count < old_ba->max_triangle_count) {
    const uint32_t old_tri_count = old_ba->triangle_count;

    old_ba->linked_triangles[old_tri_count] = tri;

    if (triangle_vert_inside) {
      old_ba->insider_triangle_count++;
    }
    old_ba->triangle_count++;

    /* Do intersections in place. */
    if (do_intersection && ld->conf.use_intersections) {
      lineart_triangle_intersect_in_bounding_area(tri, old_ba, th, old_tri_count);
    }

    if (th) {
      BLI_spin_unlock(&old_ba->lock);
    }
  }
  else { /* We need to wait for either splitting or array extension to be done. */

    if (recursive_level < ld->qtree.recursive_level &&
        old_ba->insider_triangle_count >= LRT_TILE_SPLITTING_TRIANGLE_LIMIT)
    {
      if (!old_ba->child) {
        /* old_ba->child==nullptr, means we are the thread that's doing the splitting. */
        lineart_bounding_area_split(ld, old_ba, recursive_level);
      } /* Otherwise other thread has completed the splitting process. */
    }
    else {
      if (old_ba->triangle_count == old_ba->max_triangle_count) {
        /* Means we are the thread that's doing the extension. */
        lineart_bounding_area_triangle_reallocate(old_ba);
      } /* Otherwise other thread has completed the extending the array. */
    }

    /* Unlock before going into recursive call. */
    if (th) {
      BLI_spin_unlock(&old_ba->lock);
    }

    /* Of course we still have our own triangle needs to be added. */
    lineart_bounding_area_link_triangle(
        ld, root_ba, tri, l_r_u_b, recursive, recursive_level, do_intersection, th);
  }
}

static void lineart_free_bounding_area_memory(LineartBoundingArea *ba, bool recursive)
{
  BLI_spin_end(&ba->lock);
  if (ba->linked_lines) {
    MEM_freeN(ba->linked_lines);
  }
  if (ba->linked_triangles) {
    MEM_freeN(ba->linked_triangles);
  }
  if (recursive && ba->child) {
    for (int i = 0; i < 4; i++) {
      lineart_free_bounding_area_memory(&ba->child[i], recursive);
    }
  }
}
static void lineart_free_bounding_area_memories(LineartData *ld)
{
  for (int i = 0; i < ld->qtree.count_y; i++) {
    for (int j = 0; j < ld->qtree.count_x; j++) {
      lineart_free_bounding_area_memory(&ld->qtree.initials[i * ld->qtree.count_x + j], true);
    }
  }
}

static void lineart_bounding_area_link_edge(LineartData *ld,
                                            LineartBoundingArea *root_ba,
                                            LineartEdge *e)
{
  if (root_ba->child == nullptr) {
    lineart_bounding_area_line_add(root_ba, e);
  }
  else {
    if (lineart_bounding_area_edge_intersect(
            ld, e->v1->fbcoord, e->v2->fbcoord, &root_ba->child[0]))
    {
      lineart_bounding_area_link_edge(ld, &root_ba->child[0], e);
    }
    if (lineart_bounding_area_edge_intersect(
            ld, e->v1->fbcoord, e->v2->fbcoord, &root_ba->child[1]))
    {
      lineart_bounding_area_link_edge(ld, &root_ba->child[1], e);
    }
    if (lineart_bounding_area_edge_intersect(
            ld, e->v1->fbcoord, e->v2->fbcoord, &root_ba->child[2]))
    {
      lineart_bounding_area_link_edge(ld, &root_ba->child[2], e);
    }
    if (lineart_bounding_area_edge_intersect(
            ld, e->v1->fbcoord, e->v2->fbcoord, &root_ba->child[3]))
    {
      lineart_bounding_area_link_edge(ld, &root_ba->child[3], e);
    }
  }
}

static void lineart_clear_linked_edges_recursive(LineartData *ld, LineartBoundingArea *root_ba)
{
  if (root_ba->child) {
    for (int i = 0; i < 4; i++) {
      lineart_clear_linked_edges_recursive(ld, &root_ba->child[i]);
    }
  }
  if (root_ba->linked_lines) {
    MEM_freeN(root_ba->linked_lines);
  }
  root_ba->line_count = 0;
  root_ba->max_line_count = 128;
  root_ba->linked_lines = MEM_calloc_arrayN<LineartEdge *>(root_ba->max_line_count,
                                                           "cleared lineart edges");
}
void lineart_main_clear_linked_edges(LineartData *ld)
{
  LineartBoundingArea *ba = ld->qtree.initials;
  for (int i = 0; i < ld->qtree.count_y; i++) {
    for (int j = 0; j < ld->qtree.count_x; j++) {
      lineart_clear_linked_edges_recursive(ld, &ba[i * ld->qtree.count_x + j]);
    }
  }
}

void lineart_main_link_lines(LineartData *ld)
{
  LRT_ITER_ALL_LINES_BEGIN
  {
    int r1, r2, c1, c2, row, col;
    if (lineart_get_edge_bounding_areas(ld, e, &r1, &r2, &c1, &c2)) {
      for (row = r1; row != r2 + 1; row++) {
        for (col = c1; col != c2 + 1; col++) {
          lineart_bounding_area_link_edge(
              ld, &ld->qtree.initials[row * ld->qtree.count_x + col], e);
        }
      }
    }
  }
  LRT_ITER_ALL_LINES_END
}

static void lineart_main_remove_unused_lines_recursive(LineartBoundingArea *ba,
                                                       uint8_t max_occlusion)
{
  if (ba->child) {
    for (int i = 0; i < 4; i++) {
      lineart_main_remove_unused_lines_recursive(&ba->child[i], max_occlusion);
    }
    return;
  }

  if (!ba->line_count) {
    return;
  }

  int usable_count = 0;
  for (int i = 0; i < ba->line_count; i++) {
    LineartEdge *e = ba->linked_lines[i];
    if (e->min_occ > max_occlusion) {
      continue;
    }
    usable_count++;
  }

  if (!usable_count) {
    ba->line_count = 0;
    return;
  }

  LineartEdge **new_array = MEM_calloc_arrayN<LineartEdge *>(usable_count,
                                                             "cleaned lineart edge array");

  int new_i = 0;
  for (int i = 0; i < ba->line_count; i++) {
    LineartEdge *e = ba->linked_lines[i];
    if (e->min_occ > max_occlusion) {
      continue;
    }
    new_array[new_i] = e;
    new_i++;
  }

  MEM_freeN(ba->linked_lines);
  ba->linked_lines = new_array;
  ba->max_line_count = ba->line_count = usable_count;
}

static void lineart_main_remove_unused_lines_from_tiles(LineartData *ld)
{
  for (int row = 0; row < ld->qtree.count_y; row++) {
    for (int col = 0; col < ld->qtree.count_x; col++) {
      lineart_main_remove_unused_lines_recursive(
          &ld->qtree.initials[row * ld->qtree.count_x + col], ld->conf.max_occlusion_level);
    }
  }
}

static bool lineart_get_triangle_bounding_areas(
    LineartData *ld, LineartTriangle *tri, int *rowbegin, int *rowend, int *colbegin, int *colend)
{
  double sp_w = ld->qtree.tile_width, sp_h = ld->qtree.tile_height;
  double b[4];

  if (!tri->v[0] || !tri->v[1] || !tri->v[2]) {
    return false;
  }

  b[0] = std::min({tri->v[0]->fbcoord[0], tri->v[1]->fbcoord[0], tri->v[2]->fbcoord[0]});
  b[1] = std::max({tri->v[0]->fbcoord[0], tri->v[1]->fbcoord[0], tri->v[2]->fbcoord[0]});
  b[2] = std::min({tri->v[0]->fbcoord[1], tri->v[1]->fbcoord[1], tri->v[2]->fbcoord[1]});
  b[3] = std::max({tri->v[0]->fbcoord[1], tri->v[1]->fbcoord[1], tri->v[2]->fbcoord[1]});

  if (b[0] > 1 || b[1] < -1 || b[2] > 1 || b[3] < -1) {
    return false;
  }

  (*colbegin) = int((b[0] + 1.0) / sp_w);
  (*colend) = int((b[1] + 1.0) / sp_w);
  (*rowend) = ld->qtree.count_y - int((b[2] + 1.0) / sp_h) - 1;
  (*rowbegin) = ld->qtree.count_y - int((b[3] + 1.0) / sp_h) - 1;

  if ((*colend) >= ld->qtree.count_x) {
    (*colend) = ld->qtree.count_x - 1;
  }
  if ((*rowend) >= ld->qtree.count_y) {
    (*rowend) = ld->qtree.count_y - 1;
  }
  *colbegin = std::max(*colbegin, 0);
  *rowbegin = std::max(*rowbegin, 0);

  return true;
}

static bool lineart_get_edge_bounding_areas(
    LineartData *ld, LineartEdge *e, int *rowbegin, int *rowend, int *colbegin, int *colend)
{
  double sp_w = ld->qtree.tile_width, sp_h = ld->qtree.tile_height;
  double b[4];

  if (!e->v1 || !e->v2) {
    return false;
  }

  if (e->v1->fbcoord[0] != e->v1->fbcoord[0] || e->v2->fbcoord[0] != e->v2->fbcoord[0]) {
    return false;
  }

  b[0] = std::min(e->v1->fbcoord[0], e->v2->fbcoord[0]);
  b[1] = std::max(e->v1->fbcoord[0], e->v2->fbcoord[0]);
  b[2] = std::min(e->v1->fbcoord[1], e->v2->fbcoord[1]);
  b[3] = std::max(e->v1->fbcoord[1], e->v2->fbcoord[1]);

  if (b[0] > 1 || b[1] < -1 || b[2] > 1 || b[3] < -1) {
    return false;
  }

  (*colbegin) = int((b[0] + 1.0) / sp_w);
  (*colend) = int((b[1] + 1.0) / sp_w);
  (*rowend) = ld->qtree.count_y - int((b[2] + 1.0) / sp_h) - 1;
  (*rowbegin) = ld->qtree.count_y - int((b[3] + 1.0) / sp_h) - 1;

  /* It's possible that the line stretches too much out to the side, resulting negative value. */
  if ((*rowend) < (*rowbegin)) {
    (*rowend) = ld->qtree.count_y - 1;
  }

  if ((*colend) < (*colbegin)) {
    (*colend) = ld->qtree.count_x - 1;
  }

  CLAMP((*colbegin), 0, ld->qtree.count_x - 1);
  CLAMP((*rowbegin), 0, ld->qtree.count_y - 1);
  CLAMP((*colend), 0, ld->qtree.count_x - 1);
  CLAMP((*rowend), 0, ld->qtree.count_y - 1);

  return true;
}

LineartBoundingArea *MOD_lineart_get_parent_bounding_area(LineartData *ld, double x, double y)
{
  double sp_w = ld->qtree.tile_width, sp_h = ld->qtree.tile_height;
  int col, row;

  if (x > 1 || x < -1 || y > 1 || y < -1) {
    return nullptr;
  }

  col = int((x + 1.0) / sp_w);
  row = ld->qtree.count_y - int((y + 1.0) / sp_h) - 1;

  if (col >= ld->qtree.count_x) {
    col = ld->qtree.count_x - 1;
  }
  if (row >= ld->qtree.count_y) {
    row = ld->qtree.count_y - 1;
  }
  col = std::max(col, 0);
  row = std::max(row, 0);

  return &ld->qtree.initials[row * ld->qtree.count_x + col];
}

static LineartBoundingArea *lineart_get_bounding_area(LineartData *ld, double x, double y)
{
  LineartBoundingArea *iba;
  double sp_w = ld->qtree.tile_width, sp_h = ld->qtree.tile_height;
  int c = int((x + 1.0) / sp_w);
  int r = ld->qtree.count_y - int((y + 1.0) / sp_h) - 1;
  r = std::max(r, 0);
  c = std::max(c, 0);
  if (r >= ld->qtree.count_y) {
    r = ld->qtree.count_y - 1;
  }
  if (c >= ld->qtree.count_x) {
    c = ld->qtree.count_x - 1;
  }

  iba = &ld->qtree.initials[r * ld->qtree.count_x + c];
  while (iba->child) {
    if (x > iba->cx) {
      if (y > iba->cy) {
        iba = &iba->child[0];
      }
      else {
        iba = &iba->child[3];
      }
    }
    else {
      if (y > iba->cy) {
        iba = &iba->child[1];
      }
      else {
        iba = &iba->child[2];
      }
    }
  }
  return iba;
}

LineartBoundingArea *MOD_lineart_get_bounding_area(LineartData *ld, double x, double y)
{
  LineartBoundingArea *ba;
  if ((ba = MOD_lineart_get_parent_bounding_area(ld, x, y)) != nullptr) {
    return lineart_get_bounding_area(ld, x, y);
  }
  return nullptr;
}

static void lineart_add_triangles_worker(TaskPool *__restrict /*pool*/, LineartIsecThread *th)
{
  LineartData *ld = th->ld;
  // int _dir_control = 0; /* UNUSED */
  while (lineart_schedule_new_triangle_task(th)) {
    for (LineartElementLinkNode *eln = th->pending_from; eln != th->pending_to->next;
         eln = eln->next)
    {
      int index_start = eln == th->pending_from ? th->index_from : 0;
      int index_end = eln == th->pending_to ? th->index_to : eln->element_count;
      LineartTriangle *tri = static_cast<LineartTriangle *>(
          (void *)(((uchar *)eln->pointer) + ld->sizeof_triangle * index_start));
      for (int ei = index_start; ei < index_end; ei++) {
        int x1, x2, y1, y2;
        int r, co;
        if ((tri->flags & LRT_CULL_USED) || (tri->flags & LRT_CULL_DISCARD)) {
          tri = static_cast<LineartTriangle *>((void *)(((uchar *)tri) + ld->sizeof_triangle));
          continue;
        }
        if (lineart_get_triangle_bounding_areas(ld, tri, &y1, &y2, &x1, &x2)) {
          // _dir_control++;
          for (co = x1; co <= x2; co++) {
            for (r = y1; r <= y2; r++) {
              lineart_bounding_area_link_triangle(ld,
                                                  &ld->qtree.initials[r * ld->qtree.count_x + co],
                                                  tri,
                                                  nullptr,
                                                  1,
                                                  0,
                                                  true,
                                                  th);
            }
          }
        } /* Else throw away. */
        tri = static_cast<LineartTriangle *>((void *)(((uchar *)tri) + ld->sizeof_triangle));
      }
    }
  }
}

static void lineart_create_edges_from_isec_data(LineartIsecData *d)
{
  LineartData *ld = d->ld;
  double ZMax = ld->conf.far_clip;
  double ZMin = ld->conf.near_clip;
  int total_lines = 0;

  for (int i = 0; i < d->thread_count; i++) {
    LineartIsecThread *th = &d->threads[i];
    if (G.debug_value == 4000) {
      printf("Thread %d isec generated %d lines.\n", i, th->current);
    }
    if (!th->current) {
      continue;
    }
    total_lines += th->current;
  }

  if (!total_lines) {
    return;
  }

  /* We don't care about removing duplicated vert in this method, chaining can handle that,
   * and it saves us from using locks and look up tables. */
  LineartVert *v = static_cast<LineartVert *>(
      lineart_mem_acquire(ld->edge_data_pool, sizeof(LineartVert) * total_lines * 2));
  LineartEdge *e = static_cast<LineartEdge *>(
      lineart_mem_acquire(ld->edge_data_pool, sizeof(LineartEdge) * total_lines));
  LineartEdgeSegment *es = static_cast<LineartEdgeSegment *>(
      lineart_mem_acquire(ld->edge_data_pool, sizeof(LineartEdgeSegment) * total_lines));

  LineartElementLinkNode *eln = static_cast<LineartElementLinkNode *>(
      lineart_mem_acquire(ld->edge_data_pool, sizeof(LineartElementLinkNode)));
  eln->element_count = total_lines;
  eln->pointer = e;
  eln->flags |= LRT_ELEMENT_INTERSECTION_DATA;
  BLI_addhead(&ld->geom.line_buffer_pointers, eln);

  for (int i = 0; i < d->thread_count; i++) {
    LineartIsecThread *th = &d->threads[i];
    if (!th->current) {
      continue;
    }

    for (int j = 0; j < th->current; j++) {
      LineartIsecSingle *is = &th->array[j];
      LineartVert *v1 = v;
      LineartVert *v2 = v + 1;
      copy_v3_v3_db(v1->gloc, is->v1);
      copy_v3_v3_db(v2->gloc, is->v2);
      /* The intersection line has been generated only in geometry space, so we need to transform
       * them as well. */
      mul_v4_m4v3_db(v1->fbcoord, ld->conf.view_projection, v1->gloc);
      mul_v4_m4v3_db(v2->fbcoord, ld->conf.view_projection, v2->gloc);
      mul_v3db_db(v1->fbcoord, (1 / v1->fbcoord[3]));
      mul_v3db_db(v2->fbcoord, (1 / v2->fbcoord[3]));

      v1->fbcoord[0] -= ld->conf.shift_x * 2;
      v1->fbcoord[1] -= ld->conf.shift_y * 2;
      v2->fbcoord[0] -= ld->conf.shift_x * 2;
      v2->fbcoord[1] -= ld->conf.shift_y * 2;

      /* This z transformation is not the same as the rest of the part, because the data don't go
       * through normal perspective division calls in the pipeline, but this way the 3D result and
       * occlusion on the generated line is correct, and we don't really use 2D for viewport stroke
       * generation anyway. */
      v1->fbcoord[2] = ZMin * ZMax / (ZMax - fabs(v1->fbcoord[2]) * (ZMax - ZMin));
      v2->fbcoord[2] = ZMin * ZMax / (ZMax - fabs(v2->fbcoord[2]) * (ZMax - ZMin));
      e->v1 = v1;
      e->v2 = v2;
      e->t1 = is->tri1;
      e->t2 = is->tri2;
      /* This is so we can also match intersection edges from shadow to later viewing stage. */
      e->edge_identifier = (uint64_t(e->t1->target_reference) << 32) | e->t2->target_reference;
      e->flags = MOD_LINEART_EDGE_FLAG_INTERSECTION;
      e->intersection_mask = (is->tri1->intersection_mask | is->tri2->intersection_mask);
      BLI_addtail(&e->segments, es);

      int obi1 = (e->t1->target_reference & LRT_OBINDEX_HIGHER);
      int obi2 = (e->t2->target_reference & LRT_OBINDEX_HIGHER);
      LineartElementLinkNode *eln1 = lineart_find_matching_eln(&ld->geom.line_buffer_pointers,
                                                               obi1);
      LineartElementLinkNode *eln2 = obi1 == obi2 ? eln1 :
                                                    lineart_find_matching_eln(
                                                        &ld->geom.line_buffer_pointers, obi2);
      Object *ob1 = eln1 ? static_cast<Object *>(eln1->object_ref) : nullptr;
      Object *ob2 = eln2 ? static_cast<Object *>(eln2->object_ref) : nullptr;
      if (e->t1->intersection_priority > e->t2->intersection_priority) {
        e->object_ref = ob1;
      }
      else if (e->t1->intersection_priority < e->t2->intersection_priority) {
        e->object_ref = ob2;
      }
      else { /* equal priority */
        if (ob1 == ob2) {
          /* object_ref should be ambiguous if intersection lines comes from different objects. */
          e->object_ref = ob1;
        }
      }

      lineart_add_edge_to_array(&ld->pending_edges, e);

      v += 2;
      e++;
      es++;
    }
  }
}

void lineart_main_add_triangles(LineartData *ld)
{
  double t_start;
  if (G.debug_value == 4000) {
    t_start = BLI_time_now_seconds();
  }

  /* Initialize per-thread data for thread task scheduling information and storing intersection
   * results. */
  LineartIsecData d = {nullptr};
  lineart_init_isec_thread(&d, ld, ld->thread_count);

  TaskPool *tp = BLI_task_pool_create(nullptr, TASK_PRIORITY_HIGH);
  for (int i = 0; i < ld->thread_count; i++) {
    BLI_task_pool_push(
        tp, (TaskRunFunction)lineart_add_triangles_worker, &d.threads[i], false, nullptr);
  }
  BLI_task_pool_work_and_wait(tp);
  BLI_task_pool_free(tp);

  if (ld->conf.use_intersections) {
    lineart_create_edges_from_isec_data(&d);
  }

  lineart_destroy_isec_thread(&d);

  if (G.debug_value == 4000) {
    double t_elapsed = BLI_time_now_seconds() - t_start;
    printf("Line art intersection time: %f\n", t_elapsed);
  }
}

LineartBoundingArea *lineart_edge_first_bounding_area(LineartData *ld,
                                                      double *fbcoord1,
                                                      double *fbcoord2)
{
  double data[2] = {fbcoord1[0], fbcoord1[1]};
  double LU[2] = {-1, 1}, RU[2] = {1, 1}, LB[2] = {-1, -1}, RB[2] = {1, -1};
  double r = 1, sr = 1;
  bool p_unused;

  if (data[0] > -1 && data[0] < 1 && data[1] > -1 && data[1] < 1) {
    return lineart_get_bounding_area(ld, data[0], data[1]);
  }

  if (lineart_intersect_seg_seg(fbcoord1, fbcoord2, LU, RU, &sr, &p_unused) && sr < r && sr > 0) {
    r = sr;
  }
  if (lineart_intersect_seg_seg(fbcoord1, fbcoord2, LB, RB, &sr, &p_unused) && sr < r && sr > 0) {
    r = sr;
  }
  if (lineart_intersect_seg_seg(fbcoord1, fbcoord2, LB, LU, &sr, &p_unused) && sr < r && sr > 0) {
    r = sr;
  }
  if (lineart_intersect_seg_seg(fbcoord1, fbcoord2, RB, RU, &sr, &p_unused) && sr < r && sr > 0) {
    r = sr;
  }
  interp_v2_v2v2_db(data, fbcoord1, fbcoord2, r);

  return lineart_get_bounding_area(ld, data[0], data[1]);
}

LineartBoundingArea *lineart_bounding_area_next(LineartBoundingArea *self,
                                                double *fbcoord1,
                                                double *fbcoord2,
                                                double x,
                                                double y,
                                                double k,
                                                int positive_x,
                                                int positive_y,
                                                double *next_x,
                                                double *next_y)
{
  double rx, ry, ux, uy, lx, ly, bx, by;
  double r1, r2;
  LineartBoundingArea *ba;

  /* If we are marching towards the right. */
  if (positive_x > 0) {
    rx = self->r;
    ry = y + k * (rx - x);

    /* If we are marching towards the top. */
    if (positive_y > 0) {
      uy = self->u;
      ux = x + (uy - y) / k;
      r1 = ratiod(fbcoord1[0], fbcoord2[0], rx);
      r2 = ratiod(fbcoord1[0], fbcoord2[0], ux);
      if (std::min(r1, r2) > 1) {
        return nullptr;
      }

      /* We reached the right side before the top side. */
      if (r1 <= r2) {
        LISTBASE_FOREACH (LinkData *, lip, &self->rp) {
          ba = static_cast<LineartBoundingArea *>(lip->data);
          if (ba->u >= ry && ba->b < ry) {
            *next_x = rx;
            *next_y = ry;
            return ba;
          }
        }
      }
      /* We reached the top side before the right side. */
      else {
        LISTBASE_FOREACH (LinkData *, lip, &self->up) {
          ba = static_cast<LineartBoundingArea *>(lip->data);
          if (ba->r >= ux && ba->l < ux) {
            *next_x = ux;
            *next_y = uy;
            return ba;
          }
        }
      }
    }
    /* If we are marching towards the bottom. */
    else if (positive_y < 0) {
      by = self->b;
      bx = x + (by - y) / k;
      r1 = ratiod(fbcoord1[0], fbcoord2[0], rx);
      r2 = ratiod(fbcoord1[0], fbcoord2[0], bx);
      if (std::min(r1, r2) > 1) {
        return nullptr;
      }
      if (r1 <= r2) {
        LISTBASE_FOREACH (LinkData *, lip, &self->rp) {
          ba = static_cast<LineartBoundingArea *>(lip->data);
          if (ba->u >= ry && ba->b < ry) {
            *next_x = rx;
            *next_y = ry;
            return ba;
          }
        }
      }
      else {
        LISTBASE_FOREACH (LinkData *, lip, &self->bp) {
          ba = static_cast<LineartBoundingArea *>(lip->data);
          if (ba->r >= bx && ba->l < bx) {
            *next_x = bx;
            *next_y = by;
            return ba;
          }
        }
      }
    }
    /* If the line is completely horizontal, in which Y difference == 0. */
    else {
      r1 = ratiod(fbcoord1[0], fbcoord2[0], self->r);
      if (r1 > 1) {
        return nullptr;
      }
      LISTBASE_FOREACH (LinkData *, lip, &self->rp) {
        ba = static_cast<LineartBoundingArea *>(lip->data);
        if (ba->u >= y && ba->b < y) {
          *next_x = self->r;
          *next_y = y;
          return ba;
        }
      }
    }
  }

  /* If we are marching towards the left. */
  else if (positive_x < 0) {
    lx = self->l;
    ly = y + k * (lx - x);

    /* If we are marching towards the top. */
    if (positive_y > 0) {
      uy = self->u;
      ux = x + (uy - y) / k;
      r1 = ratiod(fbcoord1[0], fbcoord2[0], lx);
      r2 = ratiod(fbcoord1[0], fbcoord2[0], ux);
      if (std::min(r1, r2) > 1) {
        return nullptr;
      }
      if (r1 <= r2) {
        LISTBASE_FOREACH (LinkData *, lip, &self->lp) {
          ba = static_cast<LineartBoundingArea *>(lip->data);
          if (ba->u >= ly && ba->b < ly) {
            *next_x = lx;
            *next_y = ly;
            return ba;
          }
        }
      }
      else {
        LISTBASE_FOREACH (LinkData *, lip, &self->up) {
          ba = static_cast<LineartBoundingArea *>(lip->data);
          if (ba->r >= ux && ba->l < ux) {
            *next_x = ux;
            *next_y = uy;
            return ba;
          }
        }
      }
    }

    /* If we are marching towards the bottom. */
    else if (positive_y < 0) {
      by = self->b;
      bx = x + (by - y) / k;
      r1 = ratiod(fbcoord1[0], fbcoord2[0], lx);
      r2 = ratiod(fbcoord1[0], fbcoord2[0], bx);
      if (std::min(r1, r2) > 1) {
        return nullptr;
      }
      if (r1 <= r2) {
        LISTBASE_FOREACH (LinkData *, lip, &self->lp) {
          ba = static_cast<LineartBoundingArea *>(lip->data);
          if (ba->u >= ly && ba->b < ly) {
            *next_x = lx;
            *next_y = ly;
            return ba;
          }
        }
      }
      else {
        LISTBASE_FOREACH (LinkData *, lip, &self->bp) {
          ba = static_cast<LineartBoundingArea *>(lip->data);
          if (ba->r >= bx && ba->l < bx) {
            *next_x = bx;
            *next_y = by;
            return ba;
          }
        }
      }
    }
    /* Again, horizontal. */
    else {
      r1 = ratiod(fbcoord1[0], fbcoord2[0], self->l);
      if (r1 > 1) {
        return nullptr;
      }
      LISTBASE_FOREACH (LinkData *, lip, &self->lp) {
        ba = static_cast<LineartBoundingArea *>(lip->data);
        if (ba->u >= y && ba->b < y) {
          *next_x = self->l;
          *next_y = y;
          return ba;
        }
      }
    }
  }
  /* If the line is completely vertical, hence X difference == 0. */
  else {
    if (positive_y > 0) {
      r1 = ratiod(fbcoord1[1], fbcoord2[1], self->u);
      if (r1 > 1) {
        return nullptr;
      }
      LISTBASE_FOREACH (LinkData *, lip, &self->up) {
        ba = static_cast<LineartBoundingArea *>(lip->data);
        if (ba->r > x && ba->l <= x) {
          *next_x = x;
          *next_y = self->u;
          return ba;
        }
      }
    }
    else if (positive_y < 0) {
      r1 = ratiod(fbcoord1[1], fbcoord2[1], self->b);
      if (r1 > 1) {
        return nullptr;
      }
      LISTBASE_FOREACH (LinkData *, lip, &self->bp) {
        ba = static_cast<LineartBoundingArea *>(lip->data);
        if (ba->r > x && ba->l <= x) {
          *next_x = x;
          *next_y = self->b;
          return ba;
        }
      }
    }
    else {
      /* Segment has no length. */
      return nullptr;
    }
  }
  return nullptr;
}

bool MOD_lineart_compute_feature_lines_v3(Depsgraph *depsgraph,
                                          GreasePencilLineartModifierData &lmd,
                                          LineartCache **cached_result,
                                          bool enable_stroke_depth_offset)
{
  LineartData *ld;
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  int intersections_only = 0; /* Not used right now, but preserve for future. */
  Object *lineart_camera = nullptr;

  double t_start;
  if (G.debug_value == 4000) {
    t_start = BLI_time_now_seconds();
  }

  bool use_render_camera_override = false;
  if (lmd.calculation_flags & MOD_LINEART_USE_CUSTOM_CAMERA) {
    if (!lmd.source_camera ||
        (lineart_camera = DEG_get_evaluated(depsgraph, lmd.source_camera))->type != OB_CAMERA)
    {
      return false;
    }
  }
  else {
    Render *render = RE_GetSceneRender(scene);
    if (render && render->camera_override) {
      lineart_camera = DEG_get_evaluated(depsgraph, render->camera_override);
      use_render_camera_override = true;
    }
    if (!lineart_camera) {
      BKE_scene_camera_switch_update(scene);
      if (!scene->camera) {
        return false;
      }
      lineart_camera = scene->camera;
    }
  }

  LineartCache *lc = *cached_result;
  if (!lc) {
    lc = MOD_lineart_init_cache();
    *cached_result = lc;
  }

  ld = lineart_create_render_buffer_v3(scene,
                                       &lmd,
                                       lineart_camera,
                                       use_render_camera_override ? lineart_camera : scene->camera,
                                       lc);

  /* Triangle thread testing data size varies depending on the thread count.
   * See definition of LineartTriangleThread for details. */
  ld->sizeof_triangle = lineart_triangle_size_get(ld);

  LineartData *shadow_rb = nullptr;
  LineartElementLinkNode *shadow_veln, *shadow_eeln;
  ListBase *shadow_elns = ld->conf.shadow_selection ? &lc->shadow_elns : nullptr;
  bool shadow_generated = lineart_main_try_generate_shadow_v3(depsgraph,
                                                              scene,
                                                              ld,
                                                              &lmd,
                                                              &lc->shadow_data_pool,
                                                              &shadow_veln,
                                                              &shadow_eeln,
                                                              shadow_elns,
                                                              &shadow_rb);

  /* Get view vector before loading geometries, because we detect feature lines there. */
  lineart_main_get_view_vector(ld);

  LineartModifierRuntime *runtime = reinterpret_cast<LineartModifierRuntime *>(lmd.runtime);
  blender::Set<const Object *> *included_objects = runtime ? &runtime->object_dependencies :
                                                             nullptr;

  lineart_main_load_geometries(depsgraph,
                               scene,
                               lineart_camera,
                               ld,
                               lmd.calculation_flags & MOD_LINEART_ALLOW_DUPLI_OBJECTS,
                               false,
                               shadow_elns,
                               included_objects);

  if (shadow_generated) {
    lineart_main_transform_and_add_shadow(ld, shadow_veln, shadow_eeln);
  }

  if (!ld->geom.vertex_buffer_pointers.first) {
    /* No geometry loaded, return early. */
    return true;
  }

  /* Initialize the bounding box acceleration structure, it's a lot like BVH in 3D. */
  lineart_main_bounding_area_make_initial(ld);

  /* We need to get cut into triangles that are crossing near/far plans, only this way can we get
   * correct coordinates of those clipped lines. Done in two steps,
   * setting clip_far==false for near plane. */
  lineart_main_cull_triangles(ld, false);
  /* `clip_far == true` for far plane. */
  lineart_main_cull_triangles(ld, true);

  /* At this point triangle adjacent info pointers is no longer needed, free them. */
  lineart_main_free_adjacent_data(ld);

  /* Do the perspective division after clipping is done. */
  lineart_main_perspective_division(ld);

  lineart_main_discard_out_of_frame_edges(ld);

  /* Triangle intersections are done here during sequential adding of them. Only after this,
   * triangles and lines are all linked with acceleration structure, and the 2D occlusion stage
   * can do its job. */
  lineart_main_add_triangles(ld);

  /* Add shadow cuts to intersection lines as well. */
  lineart_register_intersection_shadow_cuts(ld, shadow_elns);

  /* Re-link bounding areas because they have been subdivided by worker threads and we need
   * adjacent info. */
  lineart_main_bounding_areas_connect_post(ld);

  /* Link lines to acceleration structure, this can only be done after perspective division, if
   * we do it after triangles being added, the acceleration structure has already been
   * subdivided, this way we do less list manipulations. */
  lineart_main_link_lines(ld);

  /* "intersection_only" is preserved for being called in a standalone fashion.
   * If so the data will already be available at the stage. Otherwise we do the occlusion and
   * chaining etc. */

  if (!intersections_only) {

    /* Occlusion is work-and-wait. This call will not return before work is completed. */
    lineart_main_occlusion_begin(ld);

    lineart_main_make_enclosed_shapes(ld, shadow_rb);

    lineart_main_remove_unused_lines_from_tiles(ld);

    /* Chaining is all single threaded. See `lineart_chain.cc`.
     * In this particular call, only lines that are geometrically connected (share the _exact_
     * same end point) will be chained together. */
    MOD_lineart_chain_feature_lines(ld);

    /* We are unable to take care of occlusion if we only connect end points, so here we do a
     * spit, where the splitting point could be any cut in e->segments. */
    MOD_lineart_chain_split_for_fixed_occlusion(ld);

    /* Then we connect chains based on the _proximity_ of their end points in image space, here's
     * the place threshold value gets involved. */
    MOD_lineart_chain_connect(ld);

    if (ld->conf.chain_smooth_tolerance > FLT_EPSILON) {
      /* Keeping UI range of 0-1 for ease of read while scaling down the actual value for best
       * effective range in image-space (Coordinate only goes from -1 to 1). This value is
       * somewhat arbitrary, but works best for the moment. */
      MOD_lineart_smooth_chains(ld, ld->conf.chain_smooth_tolerance / 50);
    }

    if (ld->conf.use_image_boundary_trimming) {
      MOD_lineart_chain_clip_at_border(ld);
    }

    if (ld->conf.angle_splitting_threshold > FLT_EPSILON) {
      MOD_lineart_chain_split_angle(ld, ld->conf.angle_splitting_threshold);
    }

    if (enable_stroke_depth_offset && lmd.stroke_depth_offset > FLT_EPSILON) {
      MOD_lineart_chain_offset_towards_camera(
          ld, lmd.stroke_depth_offset, lmd.flags & MOD_LINEART_OFFSET_TOWARDS_CUSTOM_CAMERA);
    }

    if (ld->conf.shadow_use_silhouette) {
      MOD_lineart_chain_find_silhouette_backdrop_objects(ld);
    }

    /* Finally transfer the result list into cache. */
    memcpy(&lc->chains, &ld->chains, sizeof(ListBase));

    /* At last, we need to clear flags so we don't confuse GPencil generation calls. */
    MOD_lineart_chain_clear_picked_flag(lc);

    MOD_lineart_finalize_chains(ld);
  }

  lineart_mem_destroy(&lc->shadow_data_pool);

  if (ld->conf.shadow_enclose_shapes && shadow_rb) {
    lineart_destroy_render_data_keep_init(shadow_rb);
    MEM_freeN(shadow_rb);
  }

  if (G.debug_value == 4000) {
    lineart_count_and_print_render_buffer_memory(ld);

    double t_elapsed = BLI_time_now_seconds() - t_start;
    printf("Line art total time: %lf\n", t_elapsed);
  }

  return true;
}

struct LineartChainWriteInfo {
  LineartEdgeChain *chain;
  int point_count;
};

void MOD_lineart_gpencil_generate_v3(const LineartCache *cache,
                                     const blender::float4x4 &inverse_mat,
                                     Depsgraph *depsgraph,
                                     blender::bke::greasepencil::Drawing &drawing,
                                     const int8_t source_type,
                                     Object *source_object,
                                     Collection *source_collection,
                                     const int level_start,
                                     const int level_end,
                                     const int mat_nr,
                                     const int16_t edge_types,
                                     const uchar mask_switches,
                                     const uchar material_mask_bits,
                                     const uchar intersection_mask,
                                     const float thickness,
                                     const float opacity,
                                     const uchar shadow_selection,
                                     const uchar silhouette_mode,
                                     const char *source_vgname,
                                     const char *vgname,
                                     const int modifier_flags,
                                     const int modifier_calculation_flags)
{
  if (G.debug_value == 4000) {
    printf("Line Art v3: Generating...\n");
  }

  if (cache == nullptr) {
    if (G.debug_value == 4000) {
      printf("nullptr Lineart cache!\n");
    }
    return;
  }

  Object *orig_ob = nullptr;
  Collection *orig_col = nullptr;

  if (source_type == LINEART_SOURCE_OBJECT) {
    if (!source_object) {
      return;
    }
    orig_ob = source_object->id.orig_id ? (Object *)source_object->id.orig_id : source_object;
    orig_col = nullptr;
  }
  else if (source_type == LINEART_SOURCE_COLLECTION) {
    if (!source_collection) {
      return;
    }
    orig_col = source_collection->id.orig_id ? (Collection *)source_collection->id.orig_id :
                                               source_collection;
    orig_ob = nullptr;
  }
  /* Otherwise the whole scene is selected. */

  int enabled_types = cache->all_enabled_edge_types;

  bool invert_input = modifier_calculation_flags & MOD_LINEART_INVERT_SOURCE_VGROUP;

  bool inverse_silhouette = modifier_flags & MOD_LINEART_INVERT_SILHOUETTE_FILTER;

  blender::Vector<LineartChainWriteInfo> writer;
  writer.reserve(128);
  int total_point_count = 0;
  int stroke_count = 0;
  LISTBASE_FOREACH (LineartEdgeChain *, ec, &cache->chains) {

    if (ec->picked) {
      continue;
    }
    if (!(ec->type & (edge_types & enabled_types))) {
      continue;
    }
    if (ec->level > level_end || ec->level < level_start) {
      continue;
    }
    if (orig_ob && orig_ob != ec->object_ref) {
      continue;
    }
    if (orig_col && ec->object_ref) {
      if (BKE_collection_has_object_recursive_instanced(orig_col, ec->object_ref)) {
        if (modifier_flags & MOD_LINEART_INVERT_COLLECTION) {
          continue;
        }
      }
      else {
        if (!(modifier_flags & MOD_LINEART_INVERT_COLLECTION)) {
          continue;
        }
      }
    }
    if (mask_switches & MOD_LINEART_MATERIAL_MASK_ENABLE) {
      if (mask_switches & MOD_LINEART_MATERIAL_MASK_MATCH) {
        if (ec->material_mask_bits != material_mask_bits) {
          continue;
        }
      }
      else {
        if (!(ec->material_mask_bits & material_mask_bits)) {
          continue;
        }
      }
    }
    if (ec->type & MOD_LINEART_EDGE_FLAG_INTERSECTION) {
      if (mask_switches & MOD_LINEART_INTERSECTION_MATCH) {
        if (ec->intersection_mask != intersection_mask) {
          continue;
        }
      }
      else {
        if ((intersection_mask) && !(ec->intersection_mask & intersection_mask)) {
          continue;
        }
      }
    }
    if (shadow_selection) {
      if (ec->shadow_mask_bits != LRT_SHADOW_MASK_UNDEFINED) {
        /* TODO(@Yiming): Give a behavior option for how to display undefined shadow info. */
        if (shadow_selection == LINEART_SHADOW_FILTER_ILLUMINATED &&
            !(ec->shadow_mask_bits & LRT_SHADOW_MASK_ILLUMINATED))
        {
          continue;
        }
        if (shadow_selection == LINEART_SHADOW_FILTER_SHADED &&
            !(ec->shadow_mask_bits & LRT_SHADOW_MASK_SHADED))
        {
          continue;
        }
        if (shadow_selection == LINEART_SHADOW_FILTER_ILLUMINATED_ENCLOSED_SHAPES) {
          uint32_t test_bits = ec->shadow_mask_bits & LRT_SHADOW_TEST_SHAPE_BITS;
          if ((test_bits != LRT_SHADOW_MASK_ILLUMINATED) &&
              (test_bits != (LRT_SHADOW_MASK_SHADED | LRT_SHADOW_MASK_ILLUMINATED_SHAPE)))
          {
            continue;
          }
        }
      }
    }
    if (silhouette_mode && (ec->type & (MOD_LINEART_EDGE_FLAG_CONTOUR))) {
      bool is_silhouette = false;
      if (orig_col) {
        if (!ec->silhouette_backdrop) {
          is_silhouette = true;
        }
        else if (!BKE_collection_has_object_recursive_instanced(orig_col, ec->silhouette_backdrop))
        {
          is_silhouette = true;
        }
      }
      else {
        if ((!orig_ob) && (!ec->silhouette_backdrop)) {
          is_silhouette = true;
        }
      }

      if ((silhouette_mode == LINEART_SILHOUETTE_FILTER_INDIVIDUAL || orig_ob) &&
          ec->silhouette_backdrop != ec->object_ref)
      {
        is_silhouette = true;
      }

      if (inverse_silhouette) {
        is_silhouette = !is_silhouette;
      }
      if (!is_silhouette) {
        continue;
      }
    }

    /* Preserved: If we ever do asynchronous generation, this picked flag should be set here. */
    // ec->picked = 1;

    const int count = MOD_lineart_chain_count(ec);
    if (count < 2) {
      continue;
    }

    total_point_count += count;
    writer.append({ec, count});

    stroke_count++;
  }

  if (!total_point_count || !stroke_count) {
    return;
  }

  blender::bke::CurvesGeometry new_curves(total_point_count, stroke_count);
  new_curves.fill_curve_types(CURVE_TYPE_POLY);

  MutableAttributeAccessor attributes = new_curves.attributes_for_write();
  MutableSpan<float3> point_positions = new_curves.positions_for_write();

  SpanAttributeWriter<float> point_radii = attributes.lookup_or_add_for_write_only_span<float>(
      "radius", AttrDomain::Point);

  SpanAttributeWriter<float> point_opacities = attributes.lookup_or_add_for_write_span<float>(
      "opacity", AttrDomain::Point);

  SpanAttributeWriter<int> stroke_materials = attributes.lookup_or_add_for_write_span<int>(
      "material_index", AttrDomain::Curve);

  MutableSpan<int> offsets = new_curves.offsets_for_write();

  const bool weight_transfer_match_output = modifier_calculation_flags &
                                            MOD_LINEART_MATCH_OUTPUT_VGROUP;

  using blender::StringRef;
  using blender::Vector;

  auto ensure_target_defgroup = [&](StringRef group_name) {
    int group_index = 0;
    LISTBASE_FOREACH_INDEX (bDeformGroup *, group, &new_curves.vertex_group_names, group_index) {
      if (group_name == StringRef(group->name)) {
        return group_index;
      }
    }
    bDeformGroup *defgroup = MEM_callocN<bDeformGroup>(__func__);
    group_name.copy_utf8_truncated(defgroup->name);
    BLI_addtail(&new_curves.vertex_group_names, defgroup);
    return group_index;
  };

  int up_to_point = 0;
  for (int chain_i : writer.index_range()) {
    LineartChainWriteInfo &cwi = writer[chain_i];

    Vector<int> src_to_dst_defgroup;

    blender::Span<MDeformVert> src_dvert;
    Mesh *src_mesh = nullptr;
    MutableSpan<MDeformVert> dv = new_curves.deform_verts_for_write();
    int target_defgroup = ensure_target_defgroup(vgname);
    if (source_vgname) {
      Object *eval_ob = DEG_get_evaluated(depsgraph, cwi.chain->object_ref);
      if (eval_ob && eval_ob->type == OB_MESH) {
        src_mesh = BKE_object_get_evaluated_mesh(eval_ob);
        src_dvert = src_mesh->deform_verts();
      }
    }

    if (!src_dvert.is_empty()) {
      const ListBase *deflist = &src_mesh->vertex_group_names;
      int group_index = 0;
      LISTBASE_FOREACH_INDEX (bDeformGroup *, defgroup, deflist, group_index) {
        if (StringRef(defgroup->name).startswith(source_vgname)) {
          const int target_group_index = weight_transfer_match_output ?
                                             ensure_target_defgroup(defgroup->name) :
                                             target_defgroup;
          src_to_dst_defgroup.append(target_group_index);
        }
        else {
          src_to_dst_defgroup.append(-1);
        }
      }
    }

    auto transfer_to_matching_groups = [&](const int64_t source_index, const int target_index) {
      for (const int from_group : src_to_dst_defgroup.index_range()) {
        if (from_group < 0) {
          continue;
        }
        const MDeformWeight *mdw_from = BKE_defvert_find_index(&src_dvert[source_index],
                                                               from_group);
        MDeformWeight *mdw_to = BKE_defvert_ensure_index(&dv[target_index],
                                                         src_to_dst_defgroup[from_group]);
        const float source_weight = mdw_from ? mdw_from->weight : 0.0f;
        mdw_to->weight = invert_input ? (1 - source_weight) : source_weight;
      }
    };

    auto transfer_to_singular_group = [&](const int64_t source_index, const int target_index) {
      float highest_weight = 0.0f;
      for (const int from_group : src_to_dst_defgroup.index_range()) {
        if (from_group < 0) {
          continue;
        }
        const MDeformWeight *mdw_from = BKE_defvert_find_index(&src_dvert[source_index],
                                                               from_group);
        const float source_weight = mdw_from ? mdw_from->weight : 0.0f;
        highest_weight = std::max(highest_weight, source_weight);
      }
      MDeformWeight *mdw_to = BKE_defvert_ensure_index(&dv[target_index], target_defgroup);
      mdw_to->weight = invert_input ? (1 - highest_weight) : highest_weight;
    };

    int i;
    LISTBASE_FOREACH_INDEX (LineartEdgeChainItem *, eci, &cwi.chain->chain, i) {
      int point_i = i + up_to_point;
      point_positions[point_i] = blender::math::transform_point(inverse_mat, float3(eci->gpos));
      point_radii.span[point_i] = thickness / 2.0f;
      if (point_opacities) {
        point_opacities.span[point_i] = opacity;
      }

      const int64_t vindex = eci->index - cwi.chain->index_offset;

      if (!src_to_dst_defgroup.is_empty()) {
        if (weight_transfer_match_output) {
          transfer_to_matching_groups(vindex, point_i);
        }
        else {
          transfer_to_singular_group(vindex, point_i);
        }
      }
    }

    offsets[chain_i] = up_to_point;
    stroke_materials.span[chain_i] = max_ii(mat_nr, 0);
    up_to_point += cwi.point_count;
  }

  offsets[writer.index_range().last() + 1] = up_to_point;

  SpanAttributeWriter<bool> stroke_cyclic = attributes.lookup_or_add_for_write_span<bool>(
      "cyclic", AttrDomain::Curve);
  stroke_cyclic.span.fill(false);
  stroke_cyclic.finish();

  point_radii.finish();
  point_opacities.finish();
  stroke_materials.finish();

  Curves *original_curves = blender::bke::curves_new_nomain(drawing.strokes());
  Curves *created_curves = blender::bke::curves_new_nomain(std::move(new_curves));
  std::array<blender::bke::GeometrySet, 2> geometry_sets{
      blender::bke::GeometrySet::from_curves(original_curves),
      blender::bke::GeometrySet::from_curves(created_curves)};
  blender::bke::GeometrySet joined = blender::geometry::join_geometries(geometry_sets, {});

  drawing.strokes_for_write() = std::move(joined.get_curves_for_write()->geometry.wrap());
  drawing.tag_topology_changed();

  if (G.debug_value == 4000) {
    printf("LRT: Generated %d strokes.\n", stroke_count);
  }
}
