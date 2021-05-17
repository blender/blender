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
 *
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */

/* \file
 * \ingroup editors
 */

#include "MOD_lineart.h"

#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BKE_camera.h"
#include "BKE_collection.h"
#include "BKE_customdata.h"
#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_geom.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_scene.h"
#include "DEG_depsgraph_query.h"
#include "DNA_camera_types.h"
#include "DNA_collection_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"
#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "bmesh.h"
#include "bmesh_class.h"
#include "bmesh_tools.h"

#include "lineart_intern.h"

static LineartBoundingArea *lineart_edge_first_bounding_area(LineartRenderBuffer *rb,
                                                             LineartEdge *e);

static void lineart_bounding_area_link_line(LineartRenderBuffer *rb,
                                            LineartBoundingArea *root_ba,
                                            LineartEdge *e);

static LineartBoundingArea *lineart_bounding_area_next(LineartBoundingArea *this,
                                                       LineartEdge *e,
                                                       double x,
                                                       double y,
                                                       double k,
                                                       int positive_x,
                                                       int positive_y,
                                                       double *next_x,
                                                       double *next_y);

static bool lineart_get_edge_bounding_areas(LineartRenderBuffer *rb,
                                            LineartEdge *e,
                                            int *rowbegin,
                                            int *rowend,
                                            int *colbegin,
                                            int *colend);

static void lineart_bounding_area_link_triangle(LineartRenderBuffer *rb,
                                                LineartBoundingArea *root_ba,
                                                LineartTriangle *rt,
                                                double *LRUB,
                                                int recursive,
                                                int recursive_level,
                                                bool do_intersection);

static bool lineart_triangle_edge_image_space_occlusion(SpinLock *spl,
                                                        const LineartTriangle *rt,
                                                        const LineartEdge *e,
                                                        const double *override_camera_loc,
                                                        const bool override_cam_is_persp,
                                                        const bool allow_overlapping_edges,
                                                        const double vp[4][4],
                                                        const double *camera_dir,
                                                        const float cam_shift_x,
                                                        const float cam_shift_y,
                                                        double *from,
                                                        double *to);

static void lineart_add_edge_to_list(LineartRenderBuffer *rb, LineartEdge *e);

static void lineart_discard_segment(LineartRenderBuffer *rb, LineartLineSegment *rls)
{
  BLI_spin_lock(&rb->lock_cuts);

  memset(rls, 0, sizeof(LineartLineSegment));

  /* Storing the node for potentially reuse the memory for new segment data.
   * Line Art data is not freed after all calculations are done. */
  BLI_addtail(&rb->wasted_cuts, rls);

  BLI_spin_unlock(&rb->lock_cuts);
}

static LineartLineSegment *lineart_give_segment(LineartRenderBuffer *rb)
{
  BLI_spin_lock(&rb->lock_cuts);

  /* See if there is any already allocated memory we can reuse. */
  if (rb->wasted_cuts.first) {
    LineartLineSegment *rls = (LineartLineSegment *)BLI_pophead(&rb->wasted_cuts);
    BLI_spin_unlock(&rb->lock_cuts);
    memset(rls, 0, sizeof(LineartLineSegment));
    return rls;
  }
  BLI_spin_unlock(&rb->lock_cuts);

  /* Otherwise allocate some new memory. */
  return (LineartLineSegment *)lineart_mem_aquire_thread(&rb->render_data_pool,
                                                         sizeof(LineartLineSegment));
}

/**
 * Cuts the edge in image space and mark occlusion level for each segment.
 */
static void lineart_edge_cut(
    LineartRenderBuffer *rb, LineartEdge *e, double start, double end, uchar transparency_mask)
{
  LineartLineSegment *rls, *irls, *next_rls, *prev_rls;
  LineartLineSegment *cut_start_before = 0, *cut_end_before = 0;
  LineartLineSegment *ns = 0, *ns2 = 0;
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
    start = 0;
  }
  if (UNLIKELY(end != end)) {
    end = 0;
  }

  if (start > end) {
    double t = start;
    start = end;
    end = t;
  }

  /* Begin looking for starting position of the segment. */
  /* Not using a list iteration macro because of it more clear when using for loops to iterate
   * through the segments. */
  for (rls = e->segments.first; rls; rls = rls->next) {
    if (LRT_DOUBLE_CLOSE_ENOUGH(rls->at, start)) {
      cut_start_before = rls;
      ns = cut_start_before;
      break;
    }
    if (rls->next == NULL) {
      break;
    }
    irls = rls->next;
    if (irls->at > start + 1e-09 && start > rls->at) {
      cut_start_before = irls;
      ns = lineart_give_segment(rb);
      break;
    }
  }
  if (!cut_start_before && LRT_DOUBLE_CLOSE_ENOUGH(1, end)) {
    untouched = 1;
  }
  for (rls = cut_start_before; rls; rls = rls->next) {
    /* We tried to cut at existing cutting point (e.g. where the line's occluded by a triangle
     * strip). */
    if (LRT_DOUBLE_CLOSE_ENOUGH(rls->at, end)) {
      cut_end_before = rls;
      ns2 = cut_end_before;
      break;
    }
    /* This check is to prevent `rls->at == 1.0` (where we don't need to cut because we are at the
     * end point). */
    if (!rls->next && LRT_DOUBLE_CLOSE_ENOUGH(1, end)) {
      cut_end_before = rls;
      ns2 = cut_end_before;
      untouched = 1;
      break;
    }
    /* When an actual cut is needed in the line. */
    if (rls->at > end) {
      cut_end_before = rls;
      ns2 = lineart_give_segment(rb);
      break;
    }
  }

  /* When we still can't find any existing cut in the line, we allocate new ones. */
  if (ns == NULL) {
    ns = lineart_give_segment(rb);
  }
  if (ns2 == NULL) {
    if (untouched) {
      ns2 = ns;
      cut_end_before = ns2;
    }
    else {
      ns2 = lineart_give_segment(rb);
    }
  }

  if (cut_start_before) {
    if (cut_start_before != ns) {
      /* Insert cutting points for when a new cut is needed. */
      irls = cut_start_before->prev ? cut_start_before->prev : NULL;
      ns->occlusion = irls ? irls->occlusion : 0;
      ns->transparency_mask = irls->transparency_mask;
      BLI_insertlinkbefore(&e->segments, cut_start_before, ns);
    }
    /* Otherwise we already found a existing cutting point, no need to insert a new one. */
  }
  else {
    /* We have yet to reach a existing cutting point even after we searched the whole line, so we
     * append the new cut to the end. */
    irls = e->segments.last;
    ns->occlusion = irls->occlusion;
    ns->transparency_mask = irls->transparency_mask;
    BLI_addtail(&e->segments, ns);
  }
  if (cut_end_before) {
    /* The same manipulation as on "cut_start_before". */
    if (cut_end_before != ns2) {
      irls = cut_end_before->prev ? cut_end_before->prev : NULL;
      ns2->occlusion = irls ? irls->occlusion : 0;
      ns2->transparency_mask = irls ? irls->transparency_mask : 0;
      BLI_insertlinkbefore(&e->segments, cut_end_before, ns2);
    }
  }
  else {
    irls = e->segments.last;
    ns2->occlusion = irls->occlusion;
    ns2->transparency_mask = irls->transparency_mask;
    BLI_addtail(&e->segments, ns2);
  }

  /* If we touched the cut list, we assign the new cut position based on new cut position,
   * this way we accommodate precision lost due to multiple cut inserts. */
  ns->at = start;
  if (!untouched) {
    ns2->at = end;
  }
  else {
    /* For the convenience of the loop below. */
    ns2 = ns2->next;
  }

  /* Register 1 level of occlusion for all touched segments. */
  for (rls = ns; rls && rls != ns2; rls = rls->next) {
    rls->occlusion++;
    rls->transparency_mask |= transparency_mask;
  }

  /* Reduce adjacent cutting points of the same level, which saves memory. */
  char min_occ = 127;
  prev_rls = NULL;
  for (rls = e->segments.first; rls; rls = next_rls) {
    next_rls = rls->next;

    if (prev_rls && prev_rls->occlusion == rls->occlusion &&
        prev_rls->transparency_mask == rls->transparency_mask) {
      BLI_remlink(&e->segments, rls);
      /* This puts the node back to the render buffer, if more cut happens, these unused nodes get
       * picked first. */
      lineart_discard_segment(rb, rls);
      continue;
    }

    min_occ = MIN2(min_occ, rls->occlusion);

    prev_rls = rls;
  }
  e->min_occ = min_occ;
}

/**
 * To see if given line is connected to an adjacent intersection line.
 */
BLI_INLINE bool lineart_occlusion_is_adjacent_intersection(LineartEdge *e, LineartTriangle *rt)
{
  LineartVertIntersection *v1 = (void *)e->v1;
  LineartVertIntersection *v2 = (void *)e->v2;
  return ((v1->base.flag && v1->intersecting_with == rt) ||
          (v2->base.flag && v2->intersecting_with == rt));
}

static void lineart_occlusion_single_line(LineartRenderBuffer *rb, LineartEdge *e, int thread_id)
{
  double x = e->v1->fbcoord[0], y = e->v1->fbcoord[1];
  LineartBoundingArea *ba = lineart_edge_first_bounding_area(rb, e);
  LineartBoundingArea *nba = ba;
  LineartTriangleThread *rt;

  /* These values are used for marching along the line. */
  double l, r;
  double k = (e->v2->fbcoord[1] - e->v1->fbcoord[1]) /
             (e->v2->fbcoord[0] - e->v1->fbcoord[0] + 1e-30);
  int positive_x = (e->v2->fbcoord[0] - e->v1->fbcoord[0]) > 0 ?
                       1 :
                       (e->v2->fbcoord[0] == e->v1->fbcoord[0] ? 0 : -1);
  int positive_y = (e->v2->fbcoord[1] - e->v1->fbcoord[1]) > 0 ?
                       1 :
                       (e->v2->fbcoord[1] == e->v1->fbcoord[1] ? 0 : -1);

  while (nba) {

    LISTBASE_FOREACH (LinkData *, lip, &nba->linked_triangles) {
      rt = lip->data;
      /* If we are already testing the line in this thread, then don't do it. */
      if (rt->testing_e[thread_id] == e || (rt->base.flags & LRT_TRIANGLE_INTERSECTION_ONLY) ||
          lineart_occlusion_is_adjacent_intersection(e, (LineartTriangle *)rt)) {
        continue;
      }
      rt->testing_e[thread_id] = e;
      if (lineart_triangle_edge_image_space_occlusion(&rb->lock_task,
                                                      (const LineartTriangle *)rt,
                                                      e,
                                                      rb->camera_pos,
                                                      rb->cam_is_persp,
                                                      rb->allow_overlapping_edges,
                                                      rb->view_projection,
                                                      rb->view_vector,
                                                      rb->shift_x,
                                                      rb->shift_y,
                                                      &l,
                                                      &r)) {
        lineart_edge_cut(rb, e, l, r, rt->base.transparency_mask);
        if (e->min_occ > rb->max_occlusion_level) {
          /* No need to calculate any longer on this line because no level more than set value is
           * going to show up in the rendered result. */
          return;
        }
      }
    }
    /* Marching along `e->v1` to `e->v2`, searching each possible bounding areas it may touch. */
    nba = lineart_bounding_area_next(nba, e, x, y, k, positive_x, positive_y, &x, &y);
  }
}

static int lineart_occlusion_make_task_info(LineartRenderBuffer *rb, LineartRenderTaskInfo *rti)
{
  LineartEdge *data;
  int i;
  int res = 0;

  BLI_spin_lock(&rb->lock_task);

#define LRT_ASSIGN_OCCLUSION_TASK(name) \
  if (rb->name##_managed) { \
    data = rb->name##_managed; \
    rti->name = (void *)data; \
    for (i = 0; i < LRT_THREAD_EDGE_COUNT && data; i++) { \
      data = data->next; \
    } \
    rti->name##_end = data; \
    rb->name##_managed = data; \
    res = 1; \
  } \
  else { \
    rti->name = NULL; \
  }

  LRT_ASSIGN_OCCLUSION_TASK(contour);
  LRT_ASSIGN_OCCLUSION_TASK(intersection);
  LRT_ASSIGN_OCCLUSION_TASK(crease);
  LRT_ASSIGN_OCCLUSION_TASK(material);
  LRT_ASSIGN_OCCLUSION_TASK(edge_mark);

#undef LRT_ASSIGN_OCCLUSION_TASK

  BLI_spin_unlock(&rb->lock_task);

  return res;
}

static void lineart_occlusion_worker(TaskPool *__restrict UNUSED(pool), LineartRenderTaskInfo *rti)
{
  LineartRenderBuffer *rb = rti->rb;
  LineartEdge *eip;

  while (lineart_occlusion_make_task_info(rb, rti)) {

    for (eip = rti->contour; eip && eip != rti->contour_end; eip = eip->next) {
      lineart_occlusion_single_line(rb, eip, rti->thread_id);
    }

    for (eip = rti->crease; eip && eip != rti->crease_end; eip = eip->next) {
      lineart_occlusion_single_line(rb, eip, rti->thread_id);
    }

    for (eip = rti->intersection; eip && eip != rti->intersection_end; eip = eip->next) {
      lineart_occlusion_single_line(rb, eip, rti->thread_id);
    }

    for (eip = rti->material; eip && eip != rti->material_end; eip = eip->next) {
      lineart_occlusion_single_line(rb, eip, rti->thread_id);
    }

    for (eip = rti->edge_mark; eip && eip != rti->edge_mark_end; eip = eip->next) {
      lineart_occlusion_single_line(rb, eip, rti->thread_id);
    }
  }
}

/**
 * All internal functions starting with lineart_main_ is called inside
 * #MOD_lineart_compute_feature_lines function.
 * This function handles all occlusion calculation.
 */
static void lineart_main_occlusion_begin(LineartRenderBuffer *rb)
{
  int thread_count = rb->thread_count;
  LineartRenderTaskInfo *rti = MEM_callocN(sizeof(LineartRenderTaskInfo) * thread_count,
                                           "Task Pool");
  int i;

  rb->contour_managed = rb->contours;
  rb->crease_managed = rb->crease_lines;
  rb->intersection_managed = rb->intersection_lines;
  rb->material_managed = rb->material_lines;
  rb->edge_mark_managed = rb->edge_marks;

  TaskPool *tp = BLI_task_pool_create(NULL, TASK_PRIORITY_HIGH);

  for (i = 0; i < thread_count; i++) {
    rti[i].thread_id = i;
    rti[i].rb = rb;
    BLI_task_pool_push(tp, (TaskRunFunction)lineart_occlusion_worker, &rti[i], 0, NULL);
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
  double cl, c;

  cl = (v0[0] - v[0]) * (v1[1] - v[1]) - (v0[1] - v[1]) * (v1[0] - v[0]);
  c = cl;

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

  cl = (v0[0] - v[0]) * (v1[1] - v[1]) - (v0[1] - v[1]) * (v1[0] - v[0]);
  if (c * cl <= 0) {
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

  if (v1[0] - v0[0]) {
    c1 = ratiod(v0[0], v1[0], v[0]);
  }
  else if (v[0] == v1[0]) {
    c2 = ratiod(v0[1], v1[1], v[1]);
    return (c2 >= 0 && c2 <= 1);
  }

  if (v1[1] - v0[1]) {
    c2 = ratiod(v0[1], v1[1], v[1]);
  }
  else if (v[1] == v1[1]) {
    c1 = ratiod(v0[0], v1[0], v[0]);
    return (c1 >= 0 && c1 <= 1);
  }

  if (LRT_DOUBLE_CLOSE_ENOUGH(c1, c2) && c1 >= 0 && c1 <= 1) {
    return 1;
  }

  return 0;
}

/**
 * Same algorithm as lineart_point_inside_triangle(), but returns differently:
 * 0-outside 1-on the edge 2-inside.
 */
static int lineart_point_triangle_relation(double v[2], double v0[2], double v1[2], double v2[2])
{
  double cl, c;
  double r;
  if (lineart_point_on_line_segment(v, v0, v1) || lineart_point_on_line_segment(v, v1, v2) ||
      lineart_point_on_line_segment(v, v2, v0)) {
    return 1;
  }

  cl = (v0[0] - v[0]) * (v1[1] - v[1]) - (v0[1] - v[1]) * (v1[0] - v[0]);
  c = cl;

  cl = (v1[0] - v[0]) * (v2[1] - v[1]) - (v1[1] - v[1]) * (v2[0] - v[0]);
  if ((r = c * cl) < 0) {
    return 0;
  }

  c = cl;

  cl = (v2[0] - v[0]) * (v0[1] - v[1]) - (v2[1] - v[1]) * (v0[0] - v[0]);
  if ((r = c * cl) < 0) {
    return 0;
  }

  c = cl;

  cl = (v0[0] - v[0]) * (v1[1] - v[1]) - (v0[1] - v[1]) * (v1[0] - v[0]);
  if ((r = c * cl) < 0) {
    return 0;
  }

  if (r == 0) {
    return 1;
  }

  return 2;
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
static LineartElementLinkNode *lineart_memory_get_triangle_space(LineartRenderBuffer *rb)
{
  LineartElementLinkNode *reln;

  /* We don't need to allocate a whole bunch of triangles because the amount of clipped triangles
   * are relatively small. */
  LineartTriangle *render_triangles = lineart_mem_aquire(&rb->render_data_pool,
                                                         64 * rb->triangle_size);

  reln = lineart_list_append_pointer_pool_sized(&rb->triangle_buffer_pointers,
                                                &rb->render_data_pool,
                                                render_triangles,
                                                sizeof(LineartElementLinkNode));
  reln->element_count = 64;
  reln->flags |= LRT_ELEMENT_IS_ADDITIONAL;

  return reln;
}

static LineartElementLinkNode *lineart_memory_get_vert_space(LineartRenderBuffer *rb)
{
  LineartElementLinkNode *reln;

  LineartVert *render_vertices = lineart_mem_aquire(&rb->render_data_pool,
                                                    sizeof(LineartVert) * 64);

  reln = lineart_list_append_pointer_pool_sized(&rb->vertex_buffer_pointers,
                                                &rb->render_data_pool,
                                                render_vertices,
                                                sizeof(LineartElementLinkNode));
  reln->element_count = 64;
  reln->flags |= LRT_ELEMENT_IS_ADDITIONAL;

  return reln;
}

static LineartElementLinkNode *lineart_memory_get_edge_space(LineartRenderBuffer *rb)
{
  LineartElementLinkNode *reln;

  LineartEdge *render_edges = lineart_mem_aquire(&rb->render_data_pool, sizeof(LineartEdge) * 64);

  reln = lineart_list_append_pointer_pool_sized(&rb->line_buffer_pointers,
                                                &rb->render_data_pool,
                                                render_edges,
                                                sizeof(LineartElementLinkNode));
  reln->element_count = 64;
  reln->crease_threshold = rb->crease_threshold;
  reln->flags |= LRT_ELEMENT_IS_ADDITIONAL;

  return reln;
}

static void lineart_triangle_post(LineartTriangle *rt, LineartTriangle *orig)
{
  /* Just re-assign normal and set cull flag. */
  copy_v3_v3_db(rt->gn, orig->gn);
  rt->flags = LRT_CULL_GENERATED;
}

static void lineart_triangle_set_cull_flag(LineartTriangle *rt, uchar flag)
{
  uchar intersection_only = (rt->flags & LRT_TRIANGLE_INTERSECTION_ONLY);
  rt->flags = flag;
  rt->flags |= intersection_only;
}

static bool lineart_edge_match(LineartTriangle *rt, LineartEdge *e, int v1, int v2)
{
  return ((rt->v[v1] == e->v1 && rt->v[v2] == e->v2) ||
          (rt->v[v2] == e->v1 && rt->v[v1] == e->v2));
}

/**
 * Does near-plane cut on 1 triangle only. When cutting with far-plane, the camera vectors gets
 * reversed by the caller so don't need to implement one in a different direction.
 */
static void lineart_triangle_cull_single(LineartRenderBuffer *rb,
                                         LineartTriangle *rt,
                                         int in0,
                                         int in1,
                                         int in2,
                                         double *cam_pos,
                                         double *view_dir,
                                         bool allow_boundaries,
                                         double (*vp)[4],
                                         Object *ob,
                                         int *r_v_count,
                                         int *r_e_count,
                                         int *r_t_count,
                                         LineartElementLinkNode *v_eln,
                                         LineartElementLinkNode *e_eln,
                                         LineartElementLinkNode *t_eln)
{
  double vv1[3], vv2[3], dot1, dot2;
  double a;
  int v_count = *r_v_count;
  int e_count = *r_e_count;
  int t_count = *r_t_count;
  int v1_obi, v2_obi;
  char new_flag = 0;

  LineartEdge *new_e, *e, *old_e;
  LineartLineSegment *rls;
  LineartTriangleAdjacent *rta;

  if (rt->flags & (LRT_CULL_USED | LRT_CULL_GENERATED | LRT_CULL_DISCARD)) {
    return;
  }

  /* See definition of rt->intersecting_verts and the usage in
   * lineart_geometry_object_load() for details. */
  rta = (void *)rt->intersecting_verts;

  LineartVert *rv = &((LineartVert *)v_eln->pointer)[v_count];
  LineartTriangle *rt1 = (void *)(((uchar *)t_eln->pointer) + rb->triangle_size * t_count);
  LineartTriangle *rt2 = (void *)(((uchar *)t_eln->pointer) + rb->triangle_size * (t_count + 1));

  new_e = &((LineartEdge *)e_eln->pointer)[e_count];
  /* Init `rl` to the last `rl` entry. */
  e = new_e;

#define INCREASE_RL \
  e_count++; \
  v1_obi = e->v1_obindex; \
  v2_obi = e->v2_obindex; \
  new_e = &((LineartEdge *)e_eln->pointer)[e_count]; \
  e = new_e; \
  e->v1_obindex = v1_obi; \
  e->v2_obindex = v2_obi; \
  rls = lineart_mem_aquire(&rb->render_data_pool, sizeof(LineartLineSegment)); \
  BLI_addtail(&e->segments, rls);

#define SELECT_RL(e_num, v1_link, v2_link, newrt) \
  if (rta->e[e_num]) { \
    old_e = rta->e[e_num]; \
    new_flag = old_e->flags; \
    old_e->flags = LRT_EDGE_FLAG_CHAIN_PICKED; \
    INCREASE_RL \
    e->v1 = (v1_link); \
    e->v2 = (v2_link); \
    e->flags = new_flag; \
    e->object_ref = ob; \
    e->t1 = ((old_e->t1 == rt) ? (newrt) : (old_e->t1)); \
    e->t2 = ((old_e->t2 == rt) ? (newrt) : (old_e->t2)); \
    lineart_add_edge_to_list(rb, e); \
  }

#define RELINK_RL(e_num, newrt) \
  if (rta->e[e_num]) { \
    old_e = rta->e[e_num]; \
    old_e->t1 = ((old_e->t1 == rt) ? (newrt) : (old_e->t1)); \
    old_e->t2 = ((old_e->t2 == rt) ? (newrt) : (old_e->t2)); \
  }

#define REMOVE_TRIANGLE_RL \
  if (rta->e[0]) { \
    rta->e[0]->flags = LRT_EDGE_FLAG_CHAIN_PICKED; \
  } \
  if (rta->e[1]) { \
    rta->e[1]->flags = LRT_EDGE_FLAG_CHAIN_PICKED; \
  } \
  if (rta->e[2]) { \
    rta->e[2]->flags = LRT_EDGE_FLAG_CHAIN_PICKED; \
  }

  switch (in0 + in1 + in2) {
    case 0: /* Triangle is visible. Ignore this triangle. */
      return;
    case 3:
      /* Triangle completely behind near plane, throw it away
       * also remove render lines form being computed. */
      lineart_triangle_set_cull_flag(rt, LRT_CULL_DISCARD);
      REMOVE_TRIANGLE_RL
      return;
    case 2:
      /* Two points behind near plane, cut those and
       * generate 2 new points, 3 lines and 1 triangle. */
      lineart_triangle_set_cull_flag(rt, LRT_CULL_USED);

      /**
       * (!in0) means "when point 0 is visible".
       * conditions for point 1, 2 are the same idea.
       *
       * \code{.txt}
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
        sub_v3_v3v3_db(vv1, rt->v[0]->gloc, cam_pos);
        sub_v3_v3v3_db(vv2, cam_pos, rt->v[2]->gloc);
        dot1 = dot_v3v3_db(vv1, view_dir);
        dot2 = dot_v3v3_db(vv2, view_dir);
        a = dot1 / (dot1 + dot2);
        /* Assign it to a new point. */
        interp_v3_v3v3_db(rv[0].gloc, rt->v[0]->gloc, rt->v[2]->gloc, a);
        mul_v4_m4v3_db(rv[0].fbcoord, vp, rv[0].gloc);
        rv[0].index = rt->v[2]->index;

        /* Cut point for line 1---|-----0. */
        sub_v3_v3v3_db(vv1, rt->v[0]->gloc, cam_pos);
        sub_v3_v3v3_db(vv2, cam_pos, rt->v[1]->gloc);
        dot1 = dot_v3v3_db(vv1, view_dir);
        dot2 = dot_v3v3_db(vv2, view_dir);
        a = dot1 / (dot1 + dot2);
        /* Assign it to another new point. */
        interp_v3_v3v3_db(rv[1].gloc, rt->v[0]->gloc, rt->v[1]->gloc, a);
        mul_v4_m4v3_db(rv[1].fbcoord, vp, rv[1].gloc);
        rv[1].index = rt->v[1]->index;

        /* New line connecting two new points. */
        INCREASE_RL
        if (allow_boundaries) {
          e->flags = LRT_EDGE_FLAG_CONTOUR;
          lineart_prepend_edge_direct(&rb->contours, e);
        }
        /* NOTE: inverting `e->v1/v2` (left/right point) doesn't matter as long as
         * `rt->rl` and `rt->v` has the same sequence. and the winding direction
         * can be either CW or CCW but needs to be consistent throughout the calculation. */
        e->v1 = &rv[1];
        e->v2 = &rv[0];
        /* Only one adjacent triangle, because the other side is the near plane. */
        /* Use `tl` or `tr` doesn't matter. */
        e->t1 = rt1;
        e->object_ref = ob;

        /* New line connecting original point 0 and a new point, only when it's a selected line. */
        SELECT_RL(2, rt->v[0], &rv[0], rt1)
        /* New line connecting original point 0 and another new point. */
        SELECT_RL(0, rt->v[0], &rv[1], rt1)

        /* Re-assign triangle point array to two new points. */
        rt1->v[0] = rt->v[0];
        rt1->v[1] = &rv[1];
        rt1->v[2] = &rv[0];

        lineart_triangle_post(rt1, rt);

        v_count += 2;
        t_count += 1;
      }
      else if (!in2) {
        sub_v3_v3v3_db(vv1, rt->v[2]->gloc, cam_pos);
        sub_v3_v3v3_db(vv2, cam_pos, rt->v[0]->gloc);
        dot1 = dot_v3v3_db(vv1, view_dir);
        dot2 = dot_v3v3_db(vv2, view_dir);
        a = dot1 / (dot1 + dot2);
        interp_v3_v3v3_db(rv[0].gloc, rt->v[2]->gloc, rt->v[0]->gloc, a);
        mul_v4_m4v3_db(rv[0].fbcoord, vp, rv[0].gloc);
        rv[0].index = rt->v[0]->index;

        sub_v3_v3v3_db(vv1, rt->v[2]->gloc, cam_pos);
        sub_v3_v3v3_db(vv2, cam_pos, rt->v[1]->gloc);
        dot1 = dot_v3v3_db(vv1, view_dir);
        dot2 = dot_v3v3_db(vv2, view_dir);
        a = dot1 / (dot1 + dot2);
        interp_v3_v3v3_db(rv[1].gloc, rt->v[2]->gloc, rt->v[1]->gloc, a);
        mul_v4_m4v3_db(rv[1].fbcoord, vp, rv[1].gloc);
        rv[1].index = rt->v[1]->index;

        INCREASE_RL
        if (allow_boundaries) {
          e->flags = LRT_EDGE_FLAG_CONTOUR;
          lineart_prepend_edge_direct(&rb->contours, e);
        }
        e->v1 = &rv[0];
        e->v2 = &rv[1];
        e->t1 = rt1;
        e->object_ref = ob;

        SELECT_RL(2, rt->v[2], &rv[0], rt1)
        SELECT_RL(1, rt->v[2], &rv[1], rt1)

        rt1->v[0] = &rv[0];
        rt1->v[1] = &rv[1];
        rt1->v[2] = rt->v[2];

        lineart_triangle_post(rt1, rt);

        v_count += 2;
        t_count += 1;
      }
      else if (!in1) {
        sub_v3_v3v3_db(vv1, rt->v[1]->gloc, cam_pos);
        sub_v3_v3v3_db(vv2, cam_pos, rt->v[2]->gloc);
        dot1 = dot_v3v3_db(vv1, view_dir);
        dot2 = dot_v3v3_db(vv2, view_dir);
        a = dot1 / (dot1 + dot2);
        interp_v3_v3v3_db(rv[0].gloc, rt->v[1]->gloc, rt->v[2]->gloc, a);
        mul_v4_m4v3_db(rv[0].fbcoord, vp, rv[0].gloc);
        rv[0].index = rt->v[2]->index;

        sub_v3_v3v3_db(vv1, rt->v[1]->gloc, cam_pos);
        sub_v3_v3v3_db(vv2, cam_pos, rt->v[0]->gloc);
        dot1 = dot_v3v3_db(vv1, view_dir);
        dot2 = dot_v3v3_db(vv2, view_dir);
        a = dot1 / (dot1 + dot2);
        interp_v3_v3v3_db(rv[1].gloc, rt->v[1]->gloc, rt->v[0]->gloc, a);
        mul_v4_m4v3_db(rv[1].fbcoord, vp, rv[1].gloc);
        rv[1].index = rt->v[0]->index;

        INCREASE_RL
        if (allow_boundaries) {
          e->flags = LRT_EDGE_FLAG_CONTOUR;
          lineart_prepend_edge_direct(&rb->contours, e);
        }
        e->v1 = &rv[1];
        e->v2 = &rv[0];
        e->t1 = rt1;
        e->object_ref = ob;

        SELECT_RL(1, rt->v[1], &rv[0], rt1)
        SELECT_RL(0, rt->v[1], &rv[1], rt1)

        rt1->v[0] = &rv[0];
        rt1->v[1] = rt->v[1];
        rt1->v[2] = &rv[1];

        lineart_triangle_post(rt1, rt);

        v_count += 2;
        t_count += 1;
      }
      break;
    case 1:
      /* One point behind near plane, cut those and
       * generate 2 new points, 4 lines and 2 triangles. */
      lineart_triangle_set_cull_flag(rt, LRT_CULL_USED);

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
        sub_v3_v3v3_db(vv1, rt->v[1]->gloc, cam_pos);
        sub_v3_v3v3_db(vv2, cam_pos, rt->v[0]->gloc);
        dot1 = dot_v3v3_db(vv1, view_dir);
        dot2 = dot_v3v3_db(vv2, view_dir);
        a = dot2 / (dot1 + dot2);
        /* Assign to a new point. */
        interp_v3_v3v3_db(rv[0].gloc, rt->v[0]->gloc, rt->v[1]->gloc, a);
        mul_v4_m4v3_db(rv[0].fbcoord, vp, rv[0].gloc);
        rv[0].index = rt->v[0]->index;

        /* Cut point for line 0---|------2. */
        sub_v3_v3v3_db(vv1, rt->v[2]->gloc, cam_pos);
        sub_v3_v3v3_db(vv2, cam_pos, rt->v[0]->gloc);
        dot1 = dot_v3v3_db(vv1, view_dir);
        dot2 = dot_v3v3_db(vv2, view_dir);
        a = dot2 / (dot1 + dot2);
        /* Assign to other new point. */
        interp_v3_v3v3_db(rv[1].gloc, rt->v[0]->gloc, rt->v[2]->gloc, a);
        mul_v4_m4v3_db(rv[1].fbcoord, vp, rv[1].gloc);
        rv[1].index = rt->v[0]->index;

        /* New line connects two new points. */
        INCREASE_RL
        if (allow_boundaries) {
          e->flags = LRT_EDGE_FLAG_CONTOUR;
          lineart_prepend_edge_direct(&rb->contours, e);
        }
        e->v1 = &rv[1];
        e->v2 = &rv[0];
        e->t1 = rt1;
        e->object_ref = ob;

        /* New line connects new point 0 and old point 1,
         * this is a border line. */

        SELECT_RL(0, rt->v[1], &rv[0], rt1)
        SELECT_RL(2, rt->v[2], &rv[1], rt2)
        RELINK_RL(1, rt2)

        /* We now have one triangle closed. */
        rt1->v[0] = rt->v[1];
        rt1->v[1] = &rv[1];
        rt1->v[2] = &rv[0];
        /* Close the second triangle. */
        rt2->v[0] = &rv[1];
        rt2->v[1] = rt->v[1];
        rt2->v[2] = rt->v[2];

        lineart_triangle_post(rt1, rt);
        lineart_triangle_post(rt2, rt);

        v_count += 2;
        t_count += 2;
      }
      else if (in1) {

        sub_v3_v3v3_db(vv1, rt->v[1]->gloc, cam_pos);
        sub_v3_v3v3_db(vv2, cam_pos, rt->v[2]->gloc);
        dot1 = dot_v3v3_db(vv1, view_dir);
        dot2 = dot_v3v3_db(vv2, view_dir);
        a = dot1 / (dot1 + dot2);
        interp_v3_v3v3_db(rv[0].gloc, rt->v[1]->gloc, rt->v[2]->gloc, a);
        mul_v4_m4v3_db(rv[0].fbcoord, vp, rv[0].gloc);
        rv[0].index = rt->v[1]->index;

        sub_v3_v3v3_db(vv1, rt->v[1]->gloc, cam_pos);
        sub_v3_v3v3_db(vv2, cam_pos, rt->v[0]->gloc);
        dot1 = dot_v3v3_db(vv1, view_dir);
        dot2 = dot_v3v3_db(vv2, view_dir);
        a = dot1 / (dot1 + dot2);
        interp_v3_v3v3_db(rv[1].gloc, rt->v[1]->gloc, rt->v[0]->gloc, a);
        mul_v4_m4v3_db(rv[1].fbcoord, vp, rv[1].gloc);
        rv[1].index = rt->v[1]->index;

        INCREASE_RL
        if (allow_boundaries) {
          e->flags = LRT_EDGE_FLAG_CONTOUR;
          lineart_prepend_edge_direct(&rb->contours, e);
        }
        e->v1 = &rv[1];
        e->v2 = &rv[0];
        e->t1 = rt1;
        e->object_ref = ob;

        SELECT_RL(1, rt->v[2], &rv[0], rt1)
        SELECT_RL(0, rt->v[0], &rv[1], rt2)
        RELINK_RL(2, rt2)

        rt1->v[0] = rt->v[2];
        rt1->v[1] = &rv[1];
        rt1->v[2] = &rv[0];

        rt2->v[0] = &rv[1];
        rt2->v[1] = rt->v[2];
        rt2->v[2] = rt->v[0];

        lineart_triangle_post(rt1, rt);
        lineart_triangle_post(rt2, rt);

        v_count += 2;
        t_count += 2;
      }
      else if (in2) {

        sub_v3_v3v3_db(vv1, rt->v[2]->gloc, cam_pos);
        sub_v3_v3v3_db(vv2, cam_pos, rt->v[0]->gloc);
        dot1 = dot_v3v3_db(vv1, view_dir);
        dot2 = dot_v3v3_db(vv2, view_dir);
        a = dot1 / (dot1 + dot2);
        interp_v3_v3v3_db(rv[0].gloc, rt->v[2]->gloc, rt->v[0]->gloc, a);
        mul_v4_m4v3_db(rv[0].fbcoord, vp, rv[0].gloc);
        rv[0].index = rt->v[2]->index;

        sub_v3_v3v3_db(vv1, rt->v[2]->gloc, cam_pos);
        sub_v3_v3v3_db(vv2, cam_pos, rt->v[1]->gloc);
        dot1 = dot_v3v3_db(vv1, view_dir);
        dot2 = dot_v3v3_db(vv2, view_dir);
        a = dot1 / (dot1 + dot2);
        interp_v3_v3v3_db(rv[1].gloc, rt->v[2]->gloc, rt->v[1]->gloc, a);
        mul_v4_m4v3_db(rv[1].fbcoord, vp, rv[1].gloc);
        rv[1].index = rt->v[2]->index;

        INCREASE_RL
        if (allow_boundaries) {
          e->flags = LRT_EDGE_FLAG_CONTOUR;
          lineart_prepend_edge_direct(&rb->contours, e);
        }
        e->v1 = &rv[1];
        e->v2 = &rv[0];
        e->t1 = rt1;
        e->object_ref = ob;

        SELECT_RL(2, rt->v[0], &rv[0], rt1)
        SELECT_RL(1, rt->v[1], &rv[1], rt2)
        RELINK_RL(0, rt2)

        rt1->v[0] = rt->v[0];
        rt1->v[1] = &rv[1];
        rt1->v[2] = &rv[0];

        rt2->v[0] = &rv[1];
        rt2->v[1] = rt->v[0];
        rt2->v[2] = rt->v[1];

        lineart_triangle_post(rt1, rt);
        lineart_triangle_post(rt2, rt);

        v_count += 2;
        t_count += 2;
      }
      break;
  }
  *r_v_count = v_count;
  *r_e_count = e_count;
  *r_t_count = t_count;

#undef INCREASE_RL
#undef SELECT_RL
#undef RELINK_RL
#undef REMOVE_TRIANGLE_RL
}

/**
 * This function cuts triangles with near- or far-plane. Setting clip_far = true for cutting with
 * far-plane. For triangles that's crossing the plane, it will generate new 1 or 2 triangles with
 * new topology that represents the trimmed triangle. (which then became a triangle or a square
 * formed by two triangles)
 */
static void lineart_main_cull_triangles(LineartRenderBuffer *rb, bool clip_far)
{
  LineartTriangle *rt;
  LineartElementLinkNode *v_eln, *t_eln, *e_eln;
  double(*vp)[4] = rb->view_projection;
  int i;
  int v_count = 0, t_count = 0, e_count = 0;
  Object *ob;
  bool allow_boundaries = rb->allow_boundaries;
  double cam_pos[3];
  double clip_start = rb->near_clip, clip_end = rb->far_clip;
  double view_dir[3], clip_advance[3];

  copy_v3_v3_db(view_dir, rb->view_vector);
  copy_v3_v3_db(clip_advance, rb->view_vector);
  copy_v3_v3_db(cam_pos, rb->camera_pos);

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

  v_eln = lineart_memory_get_vert_space(rb);
  t_eln = lineart_memory_get_triangle_space(rb);
  e_eln = lineart_memory_get_edge_space(rb);

  /* Additional memory space for storing generated points and triangles. */
#define LRT_CULL_ENSURE_MEMORY \
  if (v_count > 60) { \
    v_eln->element_count = v_count; \
    v_eln = lineart_memory_get_vert_space(rb); \
    v_count = 0; \
  } \
  if (t_count > 60) { \
    t_eln->element_count = t_count; \
    t_eln = lineart_memory_get_triangle_space(rb); \
    t_count = 0; \
  } \
  if (e_count > 60) { \
    e_eln->element_count = e_count; \
    e_eln = lineart_memory_get_edge_space(rb); \
    e_count = 0; \
  }

#define LRT_CULL_DECIDE_INSIDE \
  /* These three represents points that are in the clipping range or not*/ \
  in0 = 0, in1 = 0, in2 = 0; \
  if (clip_far) { \
    /* Point outside far plane. */ \
    if (rt->v[0]->fbcoord[use_w] > clip_end) { \
      in0 = 1; \
    } \
    if (rt->v[1]->fbcoord[use_w] > clip_end) { \
      in1 = 1; \
    } \
    if (rt->v[2]->fbcoord[use_w] > clip_end) { \
      in2 = 1; \
    } \
  } \
  else { \
    /* Point inside near plane. */ \
    if (rt->v[0]->fbcoord[use_w] < clip_start) { \
      in0 = 1; \
    } \
    if (rt->v[1]->fbcoord[use_w] < clip_start) { \
      in1 = 1; \
    } \
    if (rt->v[2]->fbcoord[use_w] < clip_start) { \
      in2 = 1; \
    } \
  }

  int use_w = 3;
  int in0 = 0, in1 = 0, in2 = 0;

  if (!rb->cam_is_persp) {
    clip_start = -1;
    clip_end = 1;
    use_w = 2;
  }

  /* Then go through all the other triangles. */
  LISTBASE_FOREACH (LineartElementLinkNode *, reln, &rb->triangle_buffer_pointers) {
    if (reln->flags & LRT_ELEMENT_IS_ADDITIONAL) {
      continue;
    }
    ob = reln->object_ref;
    for (i = 0; i < reln->element_count; i++) {
      /* Select the triangle in the array. */
      rt = (void *)(((uchar *)reln->pointer) + rb->triangle_size * i);

      LRT_CULL_DECIDE_INSIDE
      LRT_CULL_ENSURE_MEMORY
      lineart_triangle_cull_single(rb,
                                   rt,
                                   in0,
                                   in1,
                                   in2,
                                   cam_pos,
                                   view_dir,
                                   allow_boundaries,
                                   vp,
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

/**
 * Adjacent data is only used during the initial stages of computing.
 * So we can free it using this function when it is not needed anymore.
 */
static void lineart_main_free_adjacent_data(LineartRenderBuffer *rb)
{
  LinkData *ld;
  while ((ld = BLI_pophead(&rb->triangle_adjacent_pointers)) != NULL) {
    MEM_freeN(ld->data);
  }
  LISTBASE_FOREACH (LineartElementLinkNode *, reln, &rb->triangle_buffer_pointers) {
    LineartTriangle *rt = reln->pointer;
    int i;
    for (i = 0; i < reln->element_count; i++) {
      /* See definition of rt->intersecting_verts and the usage in
       * lineart_geometry_object_load() for detailed. */
      rt->intersecting_verts = NULL;
      rt = (LineartTriangle *)(((uchar *)rt) + rb->triangle_size);
    }
  }
}

static void lineart_main_perspective_division(LineartRenderBuffer *rb)
{
  LineartVert *rv;
  int i;

  if (!rb->cam_is_persp) {
    return;
  }

  LISTBASE_FOREACH (LineartElementLinkNode *, reln, &rb->vertex_buffer_pointers) {
    rv = reln->pointer;
    for (i = 0; i < reln->element_count; i++) {
      /* Do not divide Z, we use Z to back transform cut points in later chaining process. */
      rv[i].fbcoord[0] /= rv[i].fbcoord[3];
      rv[i].fbcoord[1] /= rv[i].fbcoord[3];
      /* Re-map z into (0-1) range, because we no longer need NDC (Normalized Device Coordinates)
       * at the moment.
       * The algorithm currently doesn't need Z for operation, we use W instead. If Z is needed in
       * the future, the line below correctly transforms it to view space coordinates. */
      // `rv[i].fbcoord[2] = -2 * rv[i].fbcoord[2] / (far - near) - (far + near) / (far - near);
      rv[i].fbcoord[0] -= rb->shift_x * 2;
      rv[i].fbcoord[1] -= rb->shift_y * 2;
    }
  }
}

/**
 * Transform a single vert to it's viewing position.
 */
static void lineart_vert_transform(
    BMVert *v, int index, LineartVert *RvBuf, double (*mv_mat)[4], double (*mvp_mat)[4])
{
  double co[4];
  LineartVert *rv = &RvBuf[index];
  copy_v3db_v3fl(co, v->co);
  mul_v3_m4v3_db(rv->gloc, mv_mat, co);
  mul_v4_m4v3_db(rv->fbcoord, mvp_mat, co);
}

/**
 * Because we have a variable size for #LineartTriangle, we need an access helper.
 * See #LineartTriangleThread for more info.
 */
static LineartTriangle *lineart_triangle_from_index(LineartRenderBuffer *rb,
                                                    LineartTriangle *rt_array,
                                                    int index)
{
  char *b = (char *)rt_array;
  b += (index * rb->triangle_size);
  return (LineartTriangle *)b;
}

static char lineart_identify_feature_line(LineartRenderBuffer *rb,
                                          BMEdge *e,
                                          LineartTriangle *rt_array,
                                          LineartVert *rv_array,
                                          float crease_threshold,
                                          bool no_crease,
                                          bool count_freestyle,
                                          BMesh *bm_if_freestyle)
{
  BMLoop *ll, *lr = NULL;
  ll = e->l;
  if (ll) {
    lr = e->l->radial_next;
  }

  if (ll == lr || !lr) {
    return LRT_EDGE_FLAG_CONTOUR;
  }

  LineartTriangle *rt1, *rt2;
  LineartVert *l;

  /* The mesh should already be triangulated now, so we can assume each face is a triangle. */
  rt1 = lineart_triangle_from_index(rb, rt_array, BM_elem_index_get(ll->f));
  rt2 = lineart_triangle_from_index(rb, rt_array, BM_elem_index_get(lr->f));

  l = &rv_array[BM_elem_index_get(e->v1)];

  double vv[3];
  double *view_vector = vv;
  double dot_1 = 0, dot_2 = 0;
  double result;
  FreestyleEdge *fe;

  if (rb->cam_is_persp) {
    sub_v3_v3v3_db(view_vector, l->gloc, rb->camera_pos);
  }
  else {
    view_vector = rb->view_vector;
  }

  dot_1 = dot_v3v3_db(view_vector, rt1->gn);
  dot_2 = dot_v3v3_db(view_vector, rt2->gn);

  if ((result = dot_1 * dot_2) <= 0 && (dot_1 + dot_2)) {
    return LRT_EDGE_FLAG_CONTOUR;
  }

  if (rb->use_crease && (dot_v3v3_db(rt1->gn, rt2->gn) < crease_threshold)) {
    if (!no_crease) {
      return LRT_EDGE_FLAG_CREASE;
    }
  }
  else if (rb->use_material && (ll->f->mat_nr != lr->f->mat_nr)) {
    return LRT_EDGE_FLAG_MATERIAL;
  }
  else if (count_freestyle && rb->use_edge_marks) {
    fe = CustomData_bmesh_get(&bm_if_freestyle->edata, e->head.data, CD_FREESTYLE_EDGE);
    if (fe->flag & FREESTYLE_EDGE_MARK) {
      return LRT_EDGE_FLAG_EDGE_MARK;
    }
  }
  return 0;
}

static void lineart_add_edge_to_list(LineartRenderBuffer *rb, LineartEdge *e)
{
  switch (e->flags) {
    case LRT_EDGE_FLAG_CONTOUR:
      lineart_prepend_edge_direct(&rb->contours, e);
      break;
    case LRT_EDGE_FLAG_CREASE:
      lineart_prepend_edge_direct(&rb->crease_lines, e);
      break;
    case LRT_EDGE_FLAG_MATERIAL:
      lineart_prepend_edge_direct(&rb->material_lines, e);
      break;
    case LRT_EDGE_FLAG_EDGE_MARK:
      lineart_prepend_edge_direct(&rb->edge_marks, e);
      break;
    case LRT_EDGE_FLAG_INTERSECTION:
      lineart_prepend_edge_direct(&rb->intersection_lines, e);
      break;
  }
}

static void lineart_triangle_adjacent_assign(LineartTriangle *rt,
                                             LineartTriangleAdjacent *rta,
                                             LineartEdge *e)
{
  if (lineart_edge_match(rt, e, 0, 1)) {
    rta->e[0] = e;
  }
  else if (lineart_edge_match(rt, e, 1, 2)) {
    rta->e[1] = e;
  }
  else if (lineart_edge_match(rt, e, 2, 0)) {
    rta->e[2] = e;
  }
}

static void lineart_geometry_object_load(Depsgraph *dg,
                                         Object *ob,
                                         double (*mv_mat)[4],
                                         double (*mvp_mat)[4],
                                         LineartRenderBuffer *rb,
                                         int override_usage,
                                         int *global_vindex)
{
  BMesh *bm;
  BMVert *v;
  BMFace *f;
  BMEdge *e;
  BMLoop *loop;
  LineartEdge *la_e;
  LineartTriangle *rt;
  LineartTriangleAdjacent *orta;
  double new_mvp[4][4], new_mv[4][4], normal[4][4];
  float imat[4][4];
  LineartElementLinkNode *reln;
  LineartVert *orv;
  LineartEdge *o_la_e;
  LineartTriangle *ort;
  Object *orig_ob;
  int CanFindFreestyle = 0;
  int i, global_i = (*global_vindex);
  Mesh *use_mesh;
  float use_crease = 0;

  int usage = override_usage ? override_usage : ob->lineart.usage;

#define LRT_MESH_FINISH \
  BM_mesh_free(bm); \
  if (ob->type != OB_MESH) { \
    BKE_mesh_free(use_mesh); \
    MEM_freeN(use_mesh); \
  }

  if (usage == OBJECT_LRT_EXCLUDE) {
    return;
  }

  if (ELEM(ob->type, OB_MESH, OB_MBALL, OB_CURVE, OB_SURF, OB_FONT)) {

    if (ob->type == OB_MESH) {
      use_mesh = DEG_get_evaluated_object(dg, ob)->data;
    }
    else {
      use_mesh = BKE_mesh_new_from_object(NULL, ob, false, false);
    }

    /* In case we can not get any mesh geometry data from the object */
    if (!use_mesh) {
      return;
    }

    /* First we need to prepare the matrix used for transforming this specific object.  */
    mul_m4db_m4db_m4fl_uniq(new_mvp, mvp_mat, ob->obmat);
    mul_m4db_m4db_m4fl_uniq(new_mv, mv_mat, ob->obmat);

    invert_m4_m4(imat, ob->obmat);
    transpose_m4(imat);
    copy_m4d_m4(normal, imat);

    if (use_mesh->edit_mesh) {
      /* Do not use edit_mesh directly because we will modify it, so create a copy. */
      bm = BM_mesh_copy(use_mesh->edit_mesh->bm);
    }
    else {
      const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(((Mesh *)(use_mesh)));
      bm = BM_mesh_create(&allocsize,
                          &((struct BMeshCreateParams){
                              .use_toolflags = true,
                          }));
      BM_mesh_bm_from_me(bm,
                         use_mesh,
                         &((struct BMeshFromMeshParams){
                             .calc_face_normal = true,
                         }));
    }

    if (rb->remove_doubles) {
      BMEditMesh *em = BKE_editmesh_create(bm, false);
      BMOperator findop, weldop;

      /* See bmesh_opdefines.c and bmesh_operators.c for op names and argument formatting. */
      BMO_op_initf(bm, &findop, BMO_FLAG_DEFAULTS, "find_doubles verts=%av dist=%f", 0.0001);

      BMO_op_exec(bm, &findop);

      /* Weld the vertices. */
      BMO_op_init(bm, &weldop, BMO_FLAG_DEFAULTS, "weld_verts");
      BMO_slot_copy(&findop, slots_out, "targetmap.out", &weldop, slots_in, "targetmap");
      BMO_op_exec(bm, &weldop);

      BMO_op_finish(bm, &findop);
      BMO_op_finish(bm, &weldop);

      MEM_freeN(em);
    }

    BM_mesh_elem_hflag_disable_all(bm, BM_FACE | BM_EDGE, BM_ELEM_TAG, false);
    BM_mesh_triangulate(
        bm, MOD_TRIANGULATE_QUAD_FIXED, MOD_TRIANGULATE_NGON_BEAUTY, 4, false, NULL, NULL, NULL);
    BM_mesh_normals_update(bm);
    BM_mesh_elem_table_ensure(bm, BM_VERT | BM_EDGE | BM_FACE);
    BM_mesh_elem_index_ensure(bm, BM_VERT | BM_EDGE | BM_FACE);

    if (CustomData_has_layer(&bm->edata, CD_FREESTYLE_EDGE)) {
      CanFindFreestyle = 1;
    }

    /* Only allocate memory for verts and tris as we don't know how many lines we will generate
     * yet. */
    orv = lineart_mem_aquire(&rb->render_data_pool, sizeof(LineartVert) * bm->totvert);
    ort = lineart_mem_aquire(&rb->render_data_pool, bm->totface * rb->triangle_size);

    orig_ob = ob->id.orig_id ? (Object *)ob->id.orig_id : ob;

    reln = lineart_list_append_pointer_pool_sized(
        &rb->vertex_buffer_pointers, &rb->render_data_pool, orv, sizeof(LineartElementLinkNode));
    reln->element_count = bm->totvert;
    reln->object_ref = orig_ob;

    if (ob->lineart.flags & OBJECT_LRT_OWN_CREASE) {
      use_crease = cosf(M_PI - ob->lineart.crease_threshold);
    }
    else {
      use_crease = rb->crease_threshold;
    }

    /* FIXME(Yiming): Hack for getting clean 3D text, the seam that extruded text object creates
     * erroneous detection on creases. Future configuration should allow options. */
    if (ob->type == OB_FONT) {
      reln->flags |= LRT_ELEMENT_BORDER_ONLY;
    }

    reln = lineart_list_append_pointer_pool_sized(
        &rb->triangle_buffer_pointers, &rb->render_data_pool, ort, sizeof(LineartElementLinkNode));
    reln->element_count = bm->totface;
    reln->object_ref = orig_ob;
    reln->flags |= (usage == OBJECT_LRT_NO_INTERSECTION ? LRT_ELEMENT_NO_INTERSECTION : 0);

    /* Note this memory is not from pool, will be deleted after culling. */
    orta = MEM_callocN(sizeof(LineartTriangleAdjacent) * bm->totface, "LineartTriangleAdjacent");
    /* Link is minimal so we use pool anyway. */
    lineart_list_append_pointer_pool(&rb->triangle_adjacent_pointers, &rb->render_data_pool, orta);

    for (i = 0; i < bm->totvert; i++) {
      v = BM_vert_at_index(bm, i);
      lineart_vert_transform(v, i, orv, new_mv, new_mvp);
      orv[i].index = i + global_i;
    }
    /* Register a global index increment. See #lineart_triangle_share_edge() and
     * #lineart_main_load_geometries() for detailed. It's okay that global_vindex might eventually
     * overflow, in such large scene it's virtually impossible for two vertex of the same numeric
     * index to come close together. */
    (*global_vindex) += bm->totvert;

    rt = ort;
    for (i = 0; i < bm->totface; i++) {
      f = BM_face_at_index(bm, i);

      loop = f->l_first;
      rt->v[0] = &orv[BM_elem_index_get(loop->v)];
      loop = loop->next;
      rt->v[1] = &orv[BM_elem_index_get(loop->v)];
      loop = loop->next;
      rt->v[2] = &orv[BM_elem_index_get(loop->v)];

      /* Transparency bit assignment. */
      Material *mat = BKE_object_material_get(ob, f->mat_nr + 1);
      rt->transparency_mask = ((mat && (mat->lineart.flags & LRT_MATERIAL_TRANSPARENCY_ENABLED)) ?
                                   mat->lineart.transparency_mask :
                                   0);

      double gn[3];
      copy_v3db_v3fl(gn, f->no);
      mul_v3_mat3_m4v3_db(rt->gn, normal, gn);
      normalize_v3_db(rt->gn);

      if (usage == OBJECT_LRT_INTERSECTION_ONLY) {
        rt->flags |= LRT_TRIANGLE_INTERSECTION_ONLY;
      }
      else if (ELEM(usage, OBJECT_LRT_NO_INTERSECTION, OBJECT_LRT_OCCLUSION_ONLY)) {
        rt->flags |= LRT_TRIANGLE_NO_INTERSECTION;
      }

      /* Re-use this field to refer to adjacent info, will be cleared after culling stage. */
      rt->intersecting_verts = (void *)&orta[i];

      rt = (LineartTriangle *)(((uchar *)rt) + rb->triangle_size);
    }

    /* Use BM_ELEM_TAG in f->head.hflag to store needed faces in the first iteration. */

    int allocate_la_e = 0;
    for (i = 0; i < bm->totedge; i++) {
      e = BM_edge_at_index(bm, i);

      /* Because e->head.hflag is char, so line type flags should not exceed positive 7 bits. */
      char eflag = lineart_identify_feature_line(
          rb, e, ort, orv, use_crease, ob->type == OB_FONT, CanFindFreestyle, bm);
      if (eflag) {
        /* Only allocate for feature lines (instead of all lines) to save memory. */
        allocate_la_e++;
      }
      /* Here we just use bm's flag for when loading actual lines, then we don't need to call
       * lineart_identify_feature_line() again, e->head.hflag deleted after loading anyway. Always
       * set the flag, so hflag stays 0 for lines that are not feature lines. */
      e->head.hflag = eflag;
    }

    o_la_e = lineart_mem_aquire(&rb->render_data_pool, sizeof(LineartEdge) * allocate_la_e);
    reln = lineart_list_append_pointer_pool_sized(
        &rb->line_buffer_pointers, &rb->render_data_pool, o_la_e, sizeof(LineartElementLinkNode));
    reln->element_count = allocate_la_e;
    reln->object_ref = orig_ob;

    la_e = o_la_e;
    for (i = 0; i < bm->totedge; i++) {
      e = BM_edge_at_index(bm, i);

      /* Not a feature line, so we skip. */
      if (!e->head.hflag) {
        continue;
      }

      la_e->v1 = &orv[BM_elem_index_get(e->v1)];
      la_e->v2 = &orv[BM_elem_index_get(e->v2)];
      la_e->v1_obindex = la_e->v1->index - global_i;
      la_e->v2_obindex = la_e->v2->index - global_i;
      if (e->l) {
        int findex = BM_elem_index_get(e->l->f);
        la_e->t1 = lineart_triangle_from_index(rb, ort, findex);
        lineart_triangle_adjacent_assign(la_e->t1, &orta[findex], la_e);
        if (e->l->radial_next && e->l->radial_next != e->l) {
          findex = BM_elem_index_get(e->l->radial_next->f);
          la_e->t2 = lineart_triangle_from_index(rb, ort, findex);
          lineart_triangle_adjacent_assign(la_e->t2, &orta[findex], la_e);
        }
      }
      la_e->flags = e->head.hflag;
      la_e->object_ref = orig_ob;

      LineartLineSegment *rls = lineart_mem_aquire(&rb->render_data_pool,
                                                   sizeof(LineartLineSegment));
      BLI_addtail(&la_e->segments, rls);
      if (ELEM(usage, OBJECT_LRT_INHERIT, OBJECT_LRT_INCLUDE, OBJECT_LRT_NO_INTERSECTION)) {
        lineart_add_edge_to_list(rb, la_e);
      }

      la_e++;
    }

    LRT_MESH_FINISH
  }

#undef LRT_MESH_FINISH
}

static bool _lineart_object_not_in_source_collection(Collection *source, Object *ob)
{
  CollectionChild *cc;
  Collection *c = source->id.orig_id ? (Collection *)source->id.orig_id : source;
  if (BKE_collection_has_object_recursive_instanced(c, (Object *)(ob->id.orig_id))) {
    return false;
  }
  for (cc = source->children.first; cc; cc = cc->next) {
    if (!_lineart_object_not_in_source_collection(cc->collection, ob)) {
      return false;
    }
  }
  return true;
}

/**
 * See if this object in such collection is used for generating line art,
 * Disabling a collection for line art will doable all objects inside.
 * `_rb` is used to provide source selection info.
 * See the definition of `rb->_source_type` for details.
 */
static int lineart_usage_check(Collection *c, Object *ob, LineartRenderBuffer *_rb)
{

  if (!c) {
    return OBJECT_LRT_INHERIT;
  }

  int object_has_special_usage = (ob->lineart.usage != OBJECT_LRT_INHERIT);

  if (object_has_special_usage) {
    return ob->lineart.usage;
  }

  if (c->children.first == NULL) {
    if (BKE_collection_has_object(c, (Object *)(ob->id.orig_id))) {
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
        }
        return OBJECT_LRT_INHERIT;
      }
      return ob->lineart.usage;
    }
    return OBJECT_LRT_INHERIT;
  }

  LISTBASE_FOREACH (CollectionChild *, cc, &c->children) {
    int result = lineart_usage_check(cc->collection, ob, _rb);
    if (result > OBJECT_LRT_INHERIT) {
      return result;
    }
  }

  /* Temp solution to speed up calculation in the modifier without cache. See the definition of
   * rb->_source_type for details. */
  if (_rb->_source_type == LRT_SOURCE_OBJECT) {
    if (ob != _rb->_source_object && ob->id.orig_id != (ID *)_rb->_source_object) {
      return OBJECT_LRT_OCCLUSION_ONLY;
    }
  }
  else if (_rb->_source_type == LRT_SOURCE_COLLECTION) {
    if (_lineart_object_not_in_source_collection(_rb->_source_collection, ob)) {
      return OBJECT_LRT_OCCLUSION_ONLY;
    }
  }

  return OBJECT_LRT_INHERIT;
}

static void lineart_main_load_geometries(
    Depsgraph *depsgraph,
    Scene *scene,
    Object *camera /* Still use camera arg for convenience. */,
    LineartRenderBuffer *rb,
    bool allow_duplicates)
{
  double proj[4][4], view[4][4], result[4][4];
  float inv[4][4];

  Camera *cam = camera->data;
  float sensor = BKE_camera_sensor_size(cam->sensor_fit, cam->sensor_x, cam->sensor_y);
  double fov = focallength_to_fov(cam->lens, sensor);

  double asp = ((double)rb->w / (double)rb->h);

  if (cam->type == CAM_PERSP) {
    if (cam->sensor_fit == CAMERA_SENSOR_FIT_AUTO) {
      if (asp < 1) {
        fov /= asp;
      }
      else {
        fov *= asp;
      }
    }
    else if (cam->sensor_fit == CAMERA_SENSOR_FIT_HOR) {
      if (asp < 1) {
        fov /= asp;
      }
    }
    else if (cam->sensor_fit == CAMERA_SENSOR_FIT_VERT) {
      if (asp > 1) {
        fov *= asp;
      }
    }
    lineart_matrix_perspective_44d(proj, fov, asp, cam->clip_start, cam->clip_end);
  }
  else if (cam->type == CAM_ORTHO) {
    double w = cam->ortho_scale / 2;
    lineart_matrix_ortho_44d(proj, -w, w, -w / asp, w / asp, cam->clip_start, cam->clip_end);
  }
  invert_m4_m4(inv, rb->cam_obmat);
  mul_m4db_m4db_m4fl_uniq(result, proj, inv);
  copy_m4_m4_db(proj, result);
  copy_m4_m4_db(rb->view_projection, proj);

  unit_m4_db(view);

  BLI_listbase_clear(&rb->triangle_buffer_pointers);
  BLI_listbase_clear(&rb->vertex_buffer_pointers);

  int flags = DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY | DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET |
              DEG_ITER_OBJECT_FLAG_VISIBLE;

  /* Instance duplicated & particles. */
  if (allow_duplicates) {
    flags |= DEG_ITER_OBJECT_FLAG_DUPLI;
  }

  /* This is to serialize vertex index in the whole scene, so lineart_triangle_share_edge() can
   * work properly from the lack of triangle adjacent info. */
  int global_i = 0;

  DEG_OBJECT_ITER_BEGIN (depsgraph, ob, flags) {
    int usage = lineart_usage_check(scene->master_collection, ob, rb);

    lineart_geometry_object_load(depsgraph, ob, view, proj, rb, usage, &global_i);
  }
  DEG_OBJECT_ITER_END;
}

/**
 * Returns the two other verts of the triangle given a vertex. Returns false if the given vertex
 * doesn't belong to this triangle.
 */
static bool lineart_triangle_get_other_verts(const LineartTriangle *rt,
                                             const LineartVert *rv,
                                             LineartVert **l,
                                             LineartVert **r)
{
  if (rt->v[0] == rv) {
    *l = rt->v[1];
    *r = rt->v[2];
    return true;
  }
  if (rt->v[1] == rv) {
    *l = rt->v[2];
    *r = rt->v[0];
    return true;
  }
  if (rt->v[2] == rv) {
    *l = rt->v[0];
    *r = rt->v[1];
    return true;
  }
  return false;
}

static bool lineart_edge_from_triangle(const LineartTriangle *rt,
                                       const LineartEdge *e,
                                       bool allow_overlapping_edges)
{
  /* Normally we just determine from the pointer address. */
  if (e->t1 == rt || e->t2 == rt) {
    return true;
  }
  /* If allows overlapping, then we compare the vertex coordinates one by one to determine if one
   * edge is from specific triangle. This is slower but can handle edge split cases very well. */
  if (allow_overlapping_edges) {
#define LRT_TRI_SAME_POINT(rt, i, pt) \
  ((LRT_DOUBLE_CLOSE_ENOUGH(rt->v[i]->gloc[0], pt->gloc[0]) && \
    LRT_DOUBLE_CLOSE_ENOUGH(rt->v[i]->gloc[1], pt->gloc[1]) && \
    LRT_DOUBLE_CLOSE_ENOUGH(rt->v[i]->gloc[2], pt->gloc[2])) || \
   (LRT_DOUBLE_CLOSE_ENOUGH(rt->v[i]->gloc[0], pt->gloc[0]) && \
    LRT_DOUBLE_CLOSE_ENOUGH(rt->v[i]->gloc[1], pt->gloc[1]) && \
    LRT_DOUBLE_CLOSE_ENOUGH(rt->v[i]->gloc[2], pt->gloc[2])))
    if ((LRT_TRI_SAME_POINT(rt, 0, e->v1) || LRT_TRI_SAME_POINT(rt, 1, e->v1) ||
         LRT_TRI_SAME_POINT(rt, 2, e->v1)) &&
        (LRT_TRI_SAME_POINT(rt, 0, e->v2) || LRT_TRI_SAME_POINT(rt, 1, e->v2) ||
         LRT_TRI_SAME_POINT(rt, 2, e->v2))) {
      return true;
    }
#undef LRT_TRI_SAME_POINT
  }
  return false;
}

/* Sorting three intersection points from min to max,
 * the order for each intersection is set in lst[0] to lst[2].*/
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
                 (num < is[order[1]] ? order[1] : (num < is[order[2]] ? order[2] : order[2]))); \
  }

/* `ia ib ic` are ordered. */
#define INTERSECT_JUST_SMALLER(is, order, num, index) \
  { \
    index = (num > is[order[2]] ? \
                 order[2] : \
                 (num > is[order[1]] ? order[1] : (num > is[order[0]] ? order[0] : order[0]))); \
  }

/**
 * This is the main function to calculate
 * the occlusion status between 1(one) triangle and 1(one) line.
 * if returns true, then from/to will carry the occluded segments
 * in ratio from `e->v1` to `e->v2`. The line is later cut with these two values.
 */
static bool lineart_triangle_edge_image_space_occlusion(SpinLock *UNUSED(spl),
                                                        const LineartTriangle *rt,
                                                        const LineartEdge *e,
                                                        const double *override_camera_loc,
                                                        const bool override_cam_is_persp,
                                                        const bool allow_overlapping_edges,
                                                        const double vp[4][4],
                                                        const double *camera_dir,
                                                        const float cam_shift_x,
                                                        const float cam_shift_y,
                                                        double *from,
                                                        double *to)
{
  double is[3] = {0};
  int order[3];
  int LCross = -1, RCross = -1;
  int a, b, c;
  int st_l = 0, st_r = 0;

  double Lv[3];
  double Rv[3];
  double vd4[4];
  double Cv[3];
  double dot_l, dot_r, dot_la, dot_ra;
  double dot_f;
  double gloc[4], trans[4];
  double cut = -1;

  double *LFBC = e->v1->fbcoord, *RFBC = e->v2->fbcoord, *FBC0 = rt->v[0]->fbcoord,
         *FBC1 = rt->v[1]->fbcoord, *FBC2 = rt->v[2]->fbcoord;

  /* Overlapping not possible, return early. */
  if ((MAX3(FBC0[0], FBC1[0], FBC2[0]) < MIN2(LFBC[0], RFBC[0])) ||
      (MIN3(FBC0[0], FBC1[0], FBC2[0]) > MAX2(LFBC[0], RFBC[0])) ||
      (MAX3(FBC0[1], FBC1[1], FBC2[1]) < MIN2(LFBC[1], RFBC[1])) ||
      (MIN3(FBC0[1], FBC1[1], FBC2[1]) > MAX2(LFBC[1], RFBC[1])) ||
      (MIN3(FBC0[3], FBC1[3], FBC2[3]) > MAX2(LFBC[3], RFBC[3]))) {
    return false;
  }

  /* If the the line is one of the edge in the triangle, then it's not occluded. */
  if (lineart_edge_from_triangle(rt, e, allow_overlapping_edges)) {
    return false;
  }

  /* Check if the line visually crosses one of the edge in the triangle. */
  a = lineart_LineIntersectTest2d(LFBC, RFBC, FBC0, FBC1, &is[0]);
  b = lineart_LineIntersectTest2d(LFBC, RFBC, FBC1, FBC2, &is[1]);
  c = lineart_LineIntersectTest2d(LFBC, RFBC, FBC2, FBC0, &is[2]);

  /* Sort the intersection distance. */
  INTERSECT_SORT_MIN_TO_MAX_3(is[0], is[1], is[2], order);

  sub_v3_v3v3_db(Lv, e->v1->gloc, rt->v[0]->gloc);
  sub_v3_v3v3_db(Rv, e->v2->gloc, rt->v[0]->gloc);

  copy_v3_v3_db(Cv, camera_dir);

  if (override_cam_is_persp) {
    copy_v3_v3_db(vd4, override_camera_loc);
  }
  else {
    copy_v4_v4_db(vd4, override_camera_loc);
  }
  if (override_cam_is_persp) {
    sub_v3_v3v3_db(Cv, vd4, rt->v[0]->gloc);
  }

  dot_l = dot_v3v3_db(Lv, rt->gn);
  dot_r = dot_v3v3_db(Rv, rt->gn);
  dot_f = dot_v3v3_db(Cv, rt->gn);

  if (!dot_f) {
    return false;
  }

  if (!a && !b && !c) {
    if (!(st_l = lineart_point_triangle_relation(LFBC, FBC0, FBC1, FBC2)) &&
        !(st_r = lineart_point_triangle_relation(RFBC, FBC0, FBC1, FBC2))) {
      return 0; /* Intersection point is not inside triangle. */
    }
  }

  st_l = lineart_point_triangle_relation(LFBC, FBC0, FBC1, FBC2);
  st_r = lineart_point_triangle_relation(RFBC, FBC0, FBC1, FBC2);

  /* Determine the cut position. */

  dot_la = fabs(dot_l);
  if (dot_la < DBL_EPSILON) {
    dot_la = 0;
    dot_l = 0;
  }
  dot_ra = fabs(dot_r);
  if (dot_ra < DBL_EPSILON) {
    dot_ra = 0;
    dot_r = 0;
  }
  if (dot_l - dot_r == 0) {
    cut = 100000;
  }
  else if (dot_l * dot_r <= 0) {
    cut = dot_la / fabs(dot_l - dot_r);
  }
  else {
    cut = fabs(dot_r + dot_l) / fabs(dot_l - dot_r);
    cut = dot_ra > dot_la ? 1 - cut : cut;
  }

  /* Transform the cut from geometry space to image space. */
  if (override_cam_is_persp) {
    interp_v3_v3v3_db(gloc, e->v1->gloc, e->v2->gloc, cut);
    mul_v4_m4v3_db(trans, vp, gloc);
    mul_v3db_db(trans, (1 / trans[3]));
  }
  else {
    interp_v3_v3v3_db(trans, e->v1->fbcoord, e->v2->fbcoord, cut);
  }
  trans[0] -= cam_shift_x * 2;
  trans[1] -= cam_shift_y * 2;

  /* To accommodate `k=0` and `k=inf` (vertical) lines. here the cut is in image space. */
  if (fabs(e->v1->fbcoord[0] - e->v2->fbcoord[0]) > fabs(e->v1->fbcoord[1] - e->v2->fbcoord[1])) {
    cut = ratiod(e->v1->fbcoord[0], e->v2->fbcoord[0], trans[0]);
  }
  else {
    cut = ratiod(e->v1->fbcoord[1], e->v2->fbcoord[1], trans[1]);
  }

  /* Determine the pair of edges that the line has crossed. */

  if (st_l == 2) {
    if (st_r == 2) {
      INTERSECT_JUST_SMALLER(is, order, DBL_TRIANGLE_LIM, LCross);
      INTERSECT_JUST_GREATER(is, order, 1 - DBL_TRIANGLE_LIM, RCross);
    }
    else if (st_r == 1) {
      INTERSECT_JUST_SMALLER(is, order, DBL_TRIANGLE_LIM, LCross);
      INTERSECT_JUST_GREATER(is, order, 1 - DBL_TRIANGLE_LIM, RCross);
    }
    else if (st_r == 0) {
      INTERSECT_JUST_SMALLER(is, order, DBL_TRIANGLE_LIM, LCross);
      INTERSECT_JUST_GREATER(is, order, 0, RCross);
    }
  }
  else if (st_l == 1) {
    if (st_r == 2) {
      INTERSECT_JUST_SMALLER(is, order, DBL_TRIANGLE_LIM, LCross);
      INTERSECT_JUST_GREATER(is, order, 1 - DBL_TRIANGLE_LIM, RCross);
    }
    else if (st_r == 1) {
      INTERSECT_JUST_SMALLER(is, order, DBL_TRIANGLE_LIM, LCross);
      INTERSECT_JUST_GREATER(is, order, 1 - DBL_TRIANGLE_LIM, RCross);
    }
    else if (st_r == 0) {
      INTERSECT_JUST_GREATER(is, order, DBL_TRIANGLE_LIM, RCross);
      if (LRT_ABC(RCross) && is[RCross] > (DBL_TRIANGLE_LIM)) {
        INTERSECT_JUST_SMALLER(is, order, DBL_TRIANGLE_LIM, LCross);
      }
      else {
        INTERSECT_JUST_SMALLER(is, order, -DBL_TRIANGLE_LIM, LCross);
        INTERSECT_JUST_GREATER(is, order, -DBL_TRIANGLE_LIM, RCross);
      }
    }
  }
  else if (st_l == 0) {
    if (st_r == 2) {
      INTERSECT_JUST_SMALLER(is, order, 1 - DBL_TRIANGLE_LIM, LCross);
      INTERSECT_JUST_GREATER(is, order, 1 - DBL_TRIANGLE_LIM, RCross);
    }
    else if (st_r == 1) {
      INTERSECT_JUST_SMALLER(is, order, 1 - DBL_TRIANGLE_LIM, LCross);
      if (LRT_ABC(LCross) && is[LCross] < (1 - DBL_TRIANGLE_LIM)) {
        INTERSECT_JUST_GREATER(is, order, 1 - DBL_TRIANGLE_LIM, RCross);
      }
      else {
        INTERSECT_JUST_SMALLER(is, order, 1 + DBL_TRIANGLE_LIM, LCross);
        INTERSECT_JUST_GREATER(is, order, 1 + DBL_TRIANGLE_LIM, RCross);
      }
    }
    else if (st_r == 0) {
      INTERSECT_JUST_GREATER(is, order, 0, LCross);
      if (LRT_ABC(LCross) && is[LCross] > 0) {
        INTERSECT_JUST_GREATER(is, order, is[LCross], RCross);
      }
      else {
        INTERSECT_JUST_GREATER(is, order, is[LCross], LCross);
        INTERSECT_JUST_GREATER(is, order, is[LCross], RCross);
      }
    }
  }

  double LF = dot_l * dot_f, RF = dot_r * dot_f;

  /* Determine the start and end point of image space cut on a line. */
  if (LF <= 0 && RF <= 0 && (dot_l || dot_r)) {
    *from = MAX2(0, is[LCross]);
    *to = MIN2(1, is[RCross]);
    if (*from >= *to) {
      return false;
    }
    return true;
  }
  if (LF >= 0 && RF <= 0 && (dot_l || dot_r)) {
    *from = MAX2(cut, is[LCross]);
    *to = MIN2(1, is[RCross]);
    if (*from >= *to) {
      return false;
    }
    return true;
  }
  if (LF <= 0 && RF >= 0 && (dot_l || dot_r)) {
    *from = MAX2(0, is[LCross]);
    *to = MIN2(cut, is[RCross]);
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

/**
 * At this stage of the computation we don't have triangle adjacent info anymore,
 * so we can only compare the global vert index.
 */
static bool lineart_triangle_share_edge(const LineartTriangle *l, const LineartTriangle *r)
{
  if (l->v[0]->index == r->v[0]->index) {
    if (l->v[1]->index == r->v[1]->index || l->v[1]->index == r->v[2]->index ||
        l->v[2]->index == r->v[2]->index || l->v[2]->index == r->v[1]->index) {
      return true;
    }
  }
  if (l->v[0]->index == r->v[1]->index) {
    if (l->v[1]->index == r->v[0]->index || l->v[1]->index == r->v[2]->index ||
        l->v[2]->index == r->v[2]->index || l->v[2]->index == r->v[0]->index) {
      return true;
    }
  }
  if (l->v[0]->index == r->v[2]->index) {
    if (l->v[1]->index == r->v[1]->index || l->v[1]->index == r->v[0]->index ||
        l->v[2]->index == r->v[0]->index || l->v[2]->index == r->v[1]->index) {
      return true;
    }
  }
  if (l->v[1]->index == r->v[0]->index) {
    if (l->v[2]->index == r->v[1]->index || l->v[2]->index == r->v[2]->index ||
        l->v[0]->index == r->v[2]->index || l->v[0]->index == r->v[1]->index) {
      return true;
    }
  }
  if (l->v[1]->index == r->v[1]->index) {
    if (l->v[2]->index == r->v[0]->index || l->v[2]->index == r->v[2]->index ||
        l->v[0]->index == r->v[2]->index || l->v[0]->index == r->v[0]->index) {
      return true;
    }
  }
  if (l->v[1]->index == r->v[2]->index) {
    if (l->v[2]->index == r->v[1]->index || l->v[2]->index == r->v[0]->index ||
        l->v[0]->index == r->v[0]->index || l->v[0]->index == r->v[1]->index) {
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
  return NULL;
}

/**
 * To save time and prevent overlapping lines when computing intersection lines.
 */
static bool lineart_vert_already_intersected_2v(LineartVertIntersection *rv,
                                                LineartVertIntersection *v1,
                                                LineartVertIntersection *v2)
{
  return ((rv->isec1 == v1->base.index && rv->isec2 == v2->base.index) ||
          (rv->isec2 == v2->base.index && rv->isec1 == v1->base.index));
}

static void lineart_vert_set_intersection_2v(LineartVert *rv, LineartVert *v1, LineartVert *v2)
{
  LineartVertIntersection *irv = (LineartVertIntersection *)rv;
  irv->isec1 = v1->index;
  irv->isec2 = v2->index;
}

/**
 * This tests a triangle against a virtual line represented by `v1---v2`.
 * The vertices returned after repeated calls to this function
 * is then used to create a triangle/triangle intersection line.
 */
static LineartVert *lineart_triangle_2v_intersection_test(LineartRenderBuffer *rb,
                                                          LineartVert *v1,
                                                          LineartVert *v2,
                                                          LineartTriangle *rt,
                                                          LineartTriangle *testing,
                                                          LineartVert *last)
{
  double Lv[3];
  double Rv[3];
  double dot_l, dot_r;
  LineartVert *result;
  double gloc[3];
  LineartVert *l = v1, *r = v2;

  for (LinkNode *ln = (void *)testing->intersecting_verts; ln; ln = ln->next) {
    LineartVertIntersection *rv = ln->link;
    if (rv->intersecting_with == rt &&
        lineart_vert_already_intersected_2v(
            rv, (LineartVertIntersection *)l, (LineartVertIntersection *)r)) {
      return (LineartVert *)rv;
    }
  }

  sub_v3_v3v3_db(Lv, l->gloc, testing->v[0]->gloc);
  sub_v3_v3v3_db(Rv, r->gloc, testing->v[0]->gloc);

  dot_l = dot_v3v3_db(Lv, testing->gn);
  dot_r = dot_v3v3_db(Rv, testing->gn);

  if (dot_l * dot_r > 0 || (!dot_l && !dot_r)) {
    return 0;
  }

  dot_l = fabs(dot_l);
  dot_r = fabs(dot_r);

  interp_v3_v3v3_db(gloc, l->gloc, r->gloc, dot_l / (dot_l + dot_r));

  /* Due to precision issue, we might end up with the same point as the one we already detected.
   */
  if (last && LRT_DOUBLE_CLOSE_ENOUGH(last->gloc[0], gloc[0]) &&
      LRT_DOUBLE_CLOSE_ENOUGH(last->gloc[1], gloc[1]) &&
      LRT_DOUBLE_CLOSE_ENOUGH(last->gloc[2], gloc[2])) {
    return NULL;
  }

  if (!(lineart_point_inside_triangle3d(
          gloc, testing->v[0]->gloc, testing->v[1]->gloc, testing->v[2]->gloc))) {
    return NULL;
  }

  /* This is an intersection vert, the size is bigger than LineartVert,
   * allocated separately. */
  result = lineart_mem_aquire(&rb->render_data_pool, sizeof(LineartVertIntersection));

  /* Indicate the data structure difference. */
  result->flag = LRT_VERT_HAS_INTERSECTION_DATA;

  copy_v3_v3_db(result->gloc, gloc);

  lineart_prepend_pool(&testing->intersecting_verts, &rb->render_data_pool, result);

  return result;
}

/**
 * Test if two triangles intersect. Generates one intersection line if the check succeeds.
 */
static LineartEdge *lineart_triangle_intersect(LineartRenderBuffer *rb,
                                               LineartTriangle *rt,
                                               LineartTriangle *testing)
{
  LineartVert *v1 = 0, *v2 = 0;
  LineartVert **next = &v1;
  LineartEdge *result;
  LineartVert *E0T = 0;
  LineartVert *E1T = 0;
  LineartVert *E2T = 0;
  LineartVert *TE0 = 0;
  LineartVert *TE1 = 0;
  LineartVert *TE2 = 0;
  LineartVert *sv1, *sv2;
  double cl[3];

  double ZMin, ZMax;
  ZMax = rb->far_clip;
  ZMin = rb->near_clip;
  copy_v3_v3_db(cl, rb->camera_pos);
  LineartVert *share = lineart_triangle_share_point(testing, rt);

  if (share) {
    /* If triangles have sharing points like `abc` and `acd`, then we only need to detect `bc`
     * against `acd` or `cd` against `abc`. */

    LineartVert *new_share;
    lineart_triangle_get_other_verts(rt, share, &sv1, &sv2);

    v1 = new_share = lineart_mem_aquire(&rb->render_data_pool, (sizeof(LineartVertIntersection)));

    new_share->flag = LRT_VERT_HAS_INTERSECTION_DATA;

    copy_v3_v3_db(new_share->gloc, share->gloc);

    v2 = lineart_triangle_2v_intersection_test(rb, sv1, sv2, rt, testing, 0);

    if (v2 == NULL) {
      lineart_triangle_get_other_verts(testing, share, &sv1, &sv2);
      v2 = lineart_triangle_2v_intersection_test(rb, sv1, sv2, testing, rt, 0);
      if (v2 == NULL) {
        return 0;
      }
      lineart_prepend_pool(&testing->intersecting_verts, &rb->render_data_pool, new_share);
    }
    else {
      lineart_prepend_pool(&rt->intersecting_verts, &rb->render_data_pool, new_share);
    }
  }
  else {
    /* If not sharing any points, then we need to try all the possibilities. */

    E0T = lineart_triangle_2v_intersection_test(rb, rt->v[0], rt->v[1], rt, testing, 0);
    if (E0T && (!(*next))) {
      (*next) = E0T;
      lineart_vert_set_intersection_2v((*next), rt->v[0], rt->v[1]);
      next = &v2;
    }
    E1T = lineart_triangle_2v_intersection_test(rb, rt->v[1], rt->v[2], rt, testing, v1);
    if (E1T && (!(*next))) {
      (*next) = E1T;
      lineart_vert_set_intersection_2v((*next), rt->v[1], rt->v[2]);
      next = &v2;
    }
    if (!(*next)) {
      E2T = lineart_triangle_2v_intersection_test(rb, rt->v[2], rt->v[0], rt, testing, v1);
    }
    if (E2T && (!(*next))) {
      (*next) = E2T;
      lineart_vert_set_intersection_2v((*next), rt->v[2], rt->v[0]);
      next = &v2;
    }

    if (!(*next)) {
      TE0 = lineart_triangle_2v_intersection_test(
          rb, testing->v[0], testing->v[1], testing, rt, v1);
    }
    if (TE0 && (!(*next))) {
      (*next) = TE0;
      lineart_vert_set_intersection_2v((*next), testing->v[0], testing->v[1]);
      next = &v2;
    }
    if (!(*next)) {
      TE1 = lineart_triangle_2v_intersection_test(
          rb, testing->v[1], testing->v[2], testing, rt, v1);
    }
    if (TE1 && (!(*next))) {
      (*next) = TE1;
      lineart_vert_set_intersection_2v((*next), testing->v[1], testing->v[2]);
      next = &v2;
    }
    if (!(*next)) {
      TE2 = lineart_triangle_2v_intersection_test(
          rb, testing->v[2], testing->v[0], testing, rt, v1);
    }
    if (TE2 && (!(*next))) {
      (*next) = TE2;
      lineart_vert_set_intersection_2v((*next), testing->v[2], testing->v[0]);
      next = &v2;
    }

    if (!(*next)) {
      return 0;
    }
  }

  /* The intersection line has been generated only in geometry space, so we need to transform
   * them as well. */
  mul_v4_m4v3_db(v1->fbcoord, rb->view_projection, v1->gloc);
  mul_v4_m4v3_db(v2->fbcoord, rb->view_projection, v2->gloc);
  mul_v3db_db(v1->fbcoord, (1 / v1->fbcoord[3]));
  mul_v3db_db(v2->fbcoord, (1 / v2->fbcoord[3]));

  v1->fbcoord[0] -= rb->shift_x * 2;
  v1->fbcoord[1] -= rb->shift_y * 2;
  v2->fbcoord[0] -= rb->shift_x * 2;
  v2->fbcoord[1] -= rb->shift_y * 2;

  /* This z transformation is not the same as the rest of the part, because the data don't go
   * through normal perspective division calls in the pipeline, but this way the 3D result and
   * occlusion on the generated line is correct, and we don't really use 2D for viewport stroke
   * generation anyway. */
  v1->fbcoord[2] = ZMin * ZMax / (ZMax - fabs(v1->fbcoord[2]) * (ZMax - ZMin));
  v2->fbcoord[2] = ZMin * ZMax / (ZMax - fabs(v2->fbcoord[2]) * (ZMax - ZMin));

  ((LineartVertIntersection *)v1)->intersecting_with = rt;
  ((LineartVertIntersection *)v2)->intersecting_with = testing;

  result = lineart_mem_aquire(&rb->render_data_pool, sizeof(LineartEdge));
  result->v1 = v1;
  result->v2 = v2;
  result->t1 = rt;
  result->t2 = testing;

  LineartLineSegment *rls = lineart_mem_aquire(&rb->render_data_pool, sizeof(LineartLineSegment));
  BLI_addtail(&result->segments, rls);
  /* Don't need to OR flags right now, just a type mark. */
  result->flags = LRT_EDGE_FLAG_INTERSECTION;
  lineart_prepend_edge_direct(&rb->intersection_lines, result);
  int r1, r2, c1, c2, row, col;
  if (lineart_get_edge_bounding_areas(rb, result, &r1, &r2, &c1, &c2)) {
    for (row = r1; row != r2 + 1; row++) {
      for (col = c1; col != c2 + 1; col++) {
        lineart_bounding_area_link_line(
            rb, &rb->initial_bounding_areas[row * LRT_BA_ROWS + col], result);
      }
    }
  }

  rb->intersection_count++;

  return result;
}

static void lineart_triangle_intersect_in_bounding_area(LineartRenderBuffer *rb,
                                                        LineartTriangle *rt,
                                                        LineartBoundingArea *ba)
{
  /* Testing_triangle->testing[0] is used to store pairing triangle reference.
   * See definition of LineartTriangleThread for more info. */
  LineartTriangle *testing_triangle;
  LineartTriangleThread *rtt;
  LinkData *lip, *next_lip;

  double *G0 = rt->v[0]->gloc, *G1 = rt->v[1]->gloc, *G2 = rt->v[2]->gloc;

  /* If this is not the smallest subdiv bounding area.*/
  if (ba->child) {
    lineart_triangle_intersect_in_bounding_area(rb, rt, &ba->child[0]);
    lineart_triangle_intersect_in_bounding_area(rb, rt, &ba->child[1]);
    lineart_triangle_intersect_in_bounding_area(rb, rt, &ba->child[2]);
    lineart_triangle_intersect_in_bounding_area(rb, rt, &ba->child[3]);
    return;
  }

  /* If this _is_ the smallest subdiv bounding area, then do the intersections there. */
  for (lip = ba->linked_triangles.first; lip; lip = next_lip) {
    next_lip = lip->next;
    testing_triangle = lip->data;
    rtt = (LineartTriangleThread *)testing_triangle;

    if (testing_triangle == rt || rtt->testing_e[0] == (LineartEdge *)rt) {
      continue;
    }
    rtt->testing_e[0] = (LineartEdge *)rt;

    if ((testing_triangle->flags & LRT_TRIANGLE_NO_INTERSECTION) ||
        ((testing_triangle->flags & LRT_TRIANGLE_INTERSECTION_ONLY) &&
         (rt->flags & LRT_TRIANGLE_INTERSECTION_ONLY))) {
      continue;
    }

    double *RG0 = testing_triangle->v[0]->gloc, *RG1 = testing_triangle->v[1]->gloc,
           *RG2 = testing_triangle->v[2]->gloc;

    /* Bounding box not overlapping or triangles share edges, not potential of intersecting. */
    if ((MIN3(G0[2], G1[2], G2[2]) > MAX3(RG0[2], RG1[2], RG2[2])) ||
        (MAX3(G0[2], G1[2], G2[2]) < MIN3(RG0[2], RG1[2], RG2[2])) ||
        (MIN3(G0[0], G1[0], G2[0]) > MAX3(RG0[0], RG1[0], RG2[0])) ||
        (MAX3(G0[0], G1[0], G2[0]) < MIN3(RG0[0], RG1[0], RG2[0])) ||
        (MIN3(G0[1], G1[1], G2[1]) > MAX3(RG0[1], RG1[1], RG2[1])) ||
        (MAX3(G0[1], G1[1], G2[1]) < MIN3(RG0[1], RG1[1], RG2[1])) ||
        lineart_triangle_share_edge(rt, testing_triangle)) {
      continue;
    }

    /* If we do need to compute intersection, then finally do it. */
    lineart_triangle_intersect(rb, rt, testing_triangle);
  }
}

/**
 * The calculated view vector will point towards the far-plane from the camera position.
 */
static void lineart_main_get_view_vector(LineartRenderBuffer *rb)
{
  float direction[3] = {0, 0, 1};
  float trans[3];
  float inv[4][4];
  float obmat_no_scale[4][4];

  copy_m4_m4(obmat_no_scale, rb->cam_obmat);

  normalize_v3(obmat_no_scale[0]);
  normalize_v3(obmat_no_scale[1]);
  normalize_v3(obmat_no_scale[2]);
  invert_m4_m4(inv, obmat_no_scale);
  transpose_m4(inv);
  mul_v3_mat3_m4v3(trans, inv, direction);
  copy_m4_m4(rb->cam_obmat, obmat_no_scale);
  copy_v3db_v3fl(rb->view_vector, trans);
}

static void lineart_destroy_render_data(LineartRenderBuffer *rb)
{
  if (rb == NULL) {
    return;
  }

  rb->contour_count = 0;
  rb->contour_managed = NULL;
  rb->intersection_count = 0;
  rb->intersection_managed = NULL;
  rb->material_line_count = 0;
  rb->material_managed = NULL;
  rb->crease_count = 0;
  rb->crease_managed = NULL;
  rb->edge_mark_count = 0;
  rb->edge_mark_managed = NULL;

  rb->contours = NULL;
  rb->intersection_lines = NULL;
  rb->crease_lines = NULL;
  rb->material_lines = NULL;
  rb->edge_marks = NULL;

  BLI_listbase_clear(&rb->chains);
  BLI_listbase_clear(&rb->wasted_cuts);

  BLI_listbase_clear(&rb->vertex_buffer_pointers);
  BLI_listbase_clear(&rb->line_buffer_pointers);
  BLI_listbase_clear(&rb->triangle_buffer_pointers);

  BLI_spin_end(&rb->lock_task);
  BLI_spin_end(&rb->lock_cuts);
  BLI_spin_end(&rb->render_data_pool.lock_mem);

  lineart_mem_destroy(&rb->render_data_pool);
}

void MOD_lineart_destroy_render_data(LineartGpencilModifierData *lmd)
{
  LineartRenderBuffer *rb = lmd->render_buffer;

  lineart_destroy_render_data(rb);

  if (rb) {
    MEM_freeN(rb);
    lmd->render_buffer = NULL;
  }

  if (G.debug_value == 4000) {
    printf("LRT: Destroyed render data.\n");
  }
}

static LineartRenderBuffer *lineart_create_render_buffer(Scene *scene,
                                                         LineartGpencilModifierData *lmd)
{
  LineartRenderBuffer *rb = MEM_callocN(sizeof(LineartRenderBuffer), "Line Art render buffer");

  lmd->render_buffer = rb;

  if (!scene || !scene->camera) {
    return NULL;
  }
  Camera *c = scene->camera->data;
  double clipping_offset = 0;

  if (lmd->calculation_flags & LRT_ALLOW_CLIPPING_BOUNDARIES) {
    /* This way the clipped lines are "stably visible" by prevents depth buffer artifacts. */
    clipping_offset = 0.0001;
  }

  copy_v3db_v3fl(rb->camera_pos, scene->camera->obmat[3]);
  copy_m4_m4(rb->cam_obmat, scene->camera->obmat);
  rb->cam_is_persp = (c->type == CAM_PERSP);
  rb->near_clip = c->clip_start + clipping_offset;
  rb->far_clip = c->clip_end - clipping_offset;
  rb->w = scene->r.xsch;
  rb->h = scene->r.ysch;

  double asp = ((double)rb->w / (double)rb->h);
  rb->shift_x = (asp >= 1) ? c->shiftx : c->shiftx * asp;
  rb->shift_y = (asp <= 1) ? c->shifty : c->shifty * asp;

  rb->crease_threshold = cos(M_PI - lmd->crease_threshold);
  rb->angle_splitting_threshold = lmd->angle_splitting_threshold;
  rb->chaining_image_threshold = lmd->chaining_image_threshold;

  rb->fuzzy_intersections = (lmd->calculation_flags & LRT_INTERSECTION_AS_CONTOUR) != 0;
  rb->fuzzy_everything = (lmd->calculation_flags & LRT_EVERYTHING_AS_CONTOUR) != 0;
  rb->allow_boundaries = (lmd->calculation_flags & LRT_ALLOW_CLIPPING_BOUNDARIES) != 0;
  rb->remove_doubles = (lmd->calculation_flags & LRT_REMOVE_DOUBLES) != 0;

  /* See lineart_edge_from_triangle() for how this option may impact performance. */
  rb->allow_overlapping_edges = (lmd->calculation_flags & LRT_ALLOW_OVERLAPPING_EDGES) != 0;

  rb->use_contour = (lmd->edge_types & LRT_EDGE_FLAG_CONTOUR) != 0;
  rb->use_crease = (lmd->edge_types & LRT_EDGE_FLAG_CREASE) != 0;
  rb->use_material = (lmd->edge_types & LRT_EDGE_FLAG_MATERIAL) != 0;
  rb->use_edge_marks = (lmd->edge_types & LRT_EDGE_FLAG_EDGE_MARK) != 0;
  rb->use_intersections = (lmd->edge_types & LRT_EDGE_FLAG_INTERSECTION) != 0;

  BLI_spin_init(&rb->lock_task);
  BLI_spin_init(&rb->lock_cuts);
  BLI_spin_init(&rb->render_data_pool.lock_mem);

  return rb;
}

static int lineart_triangle_size_get(const Scene *scene, LineartRenderBuffer *rb)
{
  if (rb->thread_count == 0) {
    rb->thread_count = BKE_render_num_threads(&scene->r);
  }
  return sizeof(LineartTriangle) + (sizeof(LineartEdge *) * (rb->thread_count));
}

static void lineart_main_bounding_area_make_initial(LineartRenderBuffer *rb)
{
  /* Initial tile split is defined as 4 (subdivided as 4*4), increasing the value allows the
   * algorithm to build the acceleration structure for bigger scenes a little faster but not as
   * efficient at handling medium to small scenes. */
  int sp_w = LRT_BA_ROWS;
  int sp_h = LRT_BA_ROWS;
  int row, col;
  LineartBoundingArea *ba;

  /* Because NDC (Normalized Device Coordinates) range is (-1,1),
   * so the span for each initial tile is double of that in the (0,1) range. */
  double span_w = (double)1 / sp_w * 2.0;
  double span_h = (double)1 / sp_h * 2.0;

  rb->tile_count_x = sp_w;
  rb->tile_count_y = sp_h;
  rb->width_per_tile = span_w;
  rb->height_per_tile = span_h;

  rb->bounding_area_count = sp_w * sp_h;
  rb->initial_bounding_areas = lineart_mem_aquire(
      &rb->render_data_pool, sizeof(LineartBoundingArea) * rb->bounding_area_count);

  /* Initialize tiles. */
  for (row = 0; row < sp_h; row++) {
    for (col = 0; col < sp_w; col++) {
      ba = &rb->initial_bounding_areas[row * LRT_BA_ROWS + col];

      /* Set the four direction limits. */
      ba->l = span_w * col - 1.0;
      ba->r = (col == sp_w - 1) ? 1.0 : (span_w * (col + 1) - 1.0);
      ba->u = 1.0 - span_h * row;
      ba->b = (row == sp_h - 1) ? -1.0 : (1.0 - span_h * (row + 1));

      ba->cx = (ba->l + ba->r) / 2;
      ba->cy = (ba->u + ba->b) / 2;

      /* Link adjacent ones. */
      if (row) {
        lineart_list_append_pointer_pool(
            &ba->up,
            &rb->render_data_pool,
            &rb->initial_bounding_areas[(row - 1) * LRT_BA_ROWS + col]);
      }
      if (col) {
        lineart_list_append_pointer_pool(&ba->lp,
                                         &rb->render_data_pool,
                                         &rb->initial_bounding_areas[row * LRT_BA_ROWS + col - 1]);
      }
      if (row != sp_h - 1) {
        lineart_list_append_pointer_pool(
            &ba->bp,
            &rb->render_data_pool,
            &rb->initial_bounding_areas[(row + 1) * LRT_BA_ROWS + col]);
      }
      if (col != sp_w - 1) {
        lineart_list_append_pointer_pool(&ba->rp,
                                         &rb->render_data_pool,
                                         &rb->initial_bounding_areas[row * LRT_BA_ROWS + col + 1]);
      }
    }
  }
}

/**
 * Re-link adjacent tiles after one gets subdivided.
 */
static void lineart_bounding_areas_connect_new(LineartRenderBuffer *rb, LineartBoundingArea *root)
{
  LineartBoundingArea *ba = root->child, *tba;
  LinkData *lip2, *next_lip;
  LineartStaticMemPool *mph = &rb->render_data_pool;

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
    tba = lip->data;

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
    tba = lip->data;
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
    tba = lip->data;
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
    tba = lip->data;
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
    for (lip2 = ((LineartBoundingArea *)lip->data)->rp.first; lip2; lip2 = next_lip) {
      next_lip = lip2->next;
      tba = lip2->data;
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
    for (lip2 = ((LineartBoundingArea *)lip->data)->lp.first; lip2; lip2 = next_lip) {
      next_lip = lip2->next;
      tba = lip2->data;
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
    for (lip2 = ((LineartBoundingArea *)lip->data)->bp.first; lip2; lip2 = next_lip) {
      next_lip = lip2->next;
      tba = lip2->data;
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
    for (lip2 = ((LineartBoundingArea *)lip->data)->up.first; lip2; lip2 = next_lip) {
      next_lip = lip2->next;
      tba = lip2->data;
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

/**
 * Subdivide a tile after one tile contains too many triangles.
 */
static void lineart_bounding_area_split(LineartRenderBuffer *rb,
                                        LineartBoundingArea *root,
                                        int recursive_level)
{
  LineartBoundingArea *ba = lineart_mem_aquire(&rb->render_data_pool,
                                               sizeof(LineartBoundingArea) * 4);
  LineartTriangle *rt;
  LineartEdge *e;

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

  root->child = ba;

  lineart_bounding_areas_connect_new(rb, root);

  while ((rt = lineart_list_pop_pointer_no_free(&root->linked_triangles)) != NULL) {
    LineartBoundingArea *cba = root->child;
    double b[4];
    b[0] = MIN3(rt->v[0]->fbcoord[0], rt->v[1]->fbcoord[0], rt->v[2]->fbcoord[0]);
    b[1] = MAX3(rt->v[0]->fbcoord[0], rt->v[1]->fbcoord[0], rt->v[2]->fbcoord[0]);
    b[2] = MAX3(rt->v[0]->fbcoord[1], rt->v[1]->fbcoord[1], rt->v[2]->fbcoord[1]);
    b[3] = MIN3(rt->v[0]->fbcoord[1], rt->v[1]->fbcoord[1], rt->v[2]->fbcoord[1]);
    if (LRT_BOUND_AREA_CROSSES(b, &cba[0].l)) {
      lineart_bounding_area_link_triangle(rb, &cba[0], rt, b, 0, recursive_level + 1, false);
    }
    if (LRT_BOUND_AREA_CROSSES(b, &cba[1].l)) {
      lineart_bounding_area_link_triangle(rb, &cba[1], rt, b, 0, recursive_level + 1, false);
    }
    if (LRT_BOUND_AREA_CROSSES(b, &cba[2].l)) {
      lineart_bounding_area_link_triangle(rb, &cba[2], rt, b, 0, recursive_level + 1, false);
    }
    if (LRT_BOUND_AREA_CROSSES(b, &cba[3].l)) {
      lineart_bounding_area_link_triangle(rb, &cba[3], rt, b, 0, recursive_level + 1, false);
    }
  }

  while ((e = lineart_list_pop_pointer_no_free(&root->linked_lines)) != NULL) {
    lineart_bounding_area_link_line(rb, root, e);
  }

  rb->bounding_area_count += 3;
}

static bool lineart_bounding_area_line_intersect(LineartRenderBuffer *UNUSED(fb),
                                                 const double l[2],
                                                 const double r[2],
                                                 LineartBoundingArea *ba)
{
  double vx, vy;
  double converted[4];
  double c1, c;

  if (((converted[0] = (double)ba->l) > MAX2(l[0], r[0])) ||
      ((converted[1] = (double)ba->r) < MIN2(l[0], r[0])) ||
      ((converted[2] = (double)ba->b) > MAX2(l[1], r[1])) ||
      ((converted[3] = (double)ba->u) < MIN2(l[1], r[1]))) {
    return false;
  }

  vx = l[0] - r[0];
  vy = l[1] - r[1];

  c1 = vx * (converted[2] - l[1]) - vy * (converted[0] - l[0]);
  c = c1;

  c1 = vx * (converted[2] - l[1]) - vy * (converted[1] - l[0]);
  if (c1 * c <= 0) {
    return true;
  }
  c = c1;

  c1 = vx * (converted[3] - l[1]) - vy * (converted[0] - l[0]);
  if (c1 * c <= 0) {
    return true;
  }
  c = c1;

  c1 = vx * (converted[3] - l[1]) - vy * (converted[1] - l[0]);
  if (c1 * c <= 0) {
    return true;
  }
  c = c1;

  return false;
}

static bool lineart_bounding_area_triangle_intersect(LineartRenderBuffer *fb,
                                                     LineartTriangle *rt,
                                                     LineartBoundingArea *ba)
{
  double p1[2], p2[2], p3[2], p4[2];
  double *FBC1 = rt->v[0]->fbcoord, *FBC2 = rt->v[1]->fbcoord, *FBC3 = rt->v[2]->fbcoord;

  p3[0] = p1[0] = (double)ba->l;
  p2[1] = p1[1] = (double)ba->b;
  p2[0] = p4[0] = (double)ba->r;
  p3[1] = p4[1] = (double)ba->u;

  if ((FBC1[0] >= p1[0] && FBC1[0] <= p2[0] && FBC1[1] >= p1[1] && FBC1[1] <= p3[1]) ||
      (FBC2[0] >= p1[0] && FBC2[0] <= p2[0] && FBC2[1] >= p1[1] && FBC2[1] <= p3[1]) ||
      (FBC3[0] >= p1[0] && FBC3[0] <= p2[0] && FBC3[1] >= p1[1] && FBC3[1] <= p3[1])) {
    return true;
  }

  if (lineart_point_inside_triangle(p1, FBC1, FBC2, FBC3) ||
      lineart_point_inside_triangle(p2, FBC1, FBC2, FBC3) ||
      lineart_point_inside_triangle(p3, FBC1, FBC2, FBC3) ||
      lineart_point_inside_triangle(p4, FBC1, FBC2, FBC3)) {
    return true;
  }

  if ((lineart_bounding_area_line_intersect(fb, FBC1, FBC2, ba)) ||
      (lineart_bounding_area_line_intersect(fb, FBC2, FBC3, ba)) ||
      (lineart_bounding_area_line_intersect(fb, FBC3, FBC1, ba))) {
    return true;
  }

  return false;
}

/**
 * 1) Link triangles with bounding areas for later occlusion test.
 * 2) Test triangles with existing(added previously) triangles for intersection lines.
 */
static void lineart_bounding_area_link_triangle(LineartRenderBuffer *rb,
                                                LineartBoundingArea *root_ba,
                                                LineartTriangle *rt,
                                                double *LRUB,
                                                int recursive,
                                                int recursive_level,
                                                bool do_intersection)
{
  if (!lineart_bounding_area_triangle_intersect(rb, rt, root_ba)) {
    return;
  }
  if (root_ba->child == NULL) {
    lineart_list_append_pointer_pool(&root_ba->linked_triangles, &rb->render_data_pool, rt);
    root_ba->triangle_count++;
    /* If splitting doesn't improve triangle separation, then shouldn't allow splitting anymore.
     * Here we use recursive limit. This is especially useful in orthographic render,
     * where a lot of faces could easily line up perfectly in image space,
     * which can not be separated by simply slicing the image tile. */
    if (root_ba->triangle_count > 200 && recursive && recursive_level < 10) {
      lineart_bounding_area_split(rb, root_ba, recursive_level);
    }
    if (recursive && do_intersection && rb->use_intersections) {
      lineart_triangle_intersect_in_bounding_area(rb, rt, root_ba);
    }
  }
  else {
    LineartBoundingArea *ba = root_ba->child;
    double *B1 = LRUB;
    double b[4];
    if (!LRUB) {
      b[0] = MIN3(rt->v[0]->fbcoord[0], rt->v[1]->fbcoord[0], rt->v[2]->fbcoord[0]);
      b[1] = MAX3(rt->v[0]->fbcoord[0], rt->v[1]->fbcoord[0], rt->v[2]->fbcoord[0]);
      b[2] = MAX3(rt->v[0]->fbcoord[1], rt->v[1]->fbcoord[1], rt->v[2]->fbcoord[1]);
      b[3] = MIN3(rt->v[0]->fbcoord[1], rt->v[1]->fbcoord[1], rt->v[2]->fbcoord[1]);
      B1 = b;
    }
    if (LRT_BOUND_AREA_CROSSES(B1, &ba[0].l)) {
      lineart_bounding_area_link_triangle(
          rb, &ba[0], rt, B1, recursive, recursive_level + 1, do_intersection);
    }
    if (LRT_BOUND_AREA_CROSSES(B1, &ba[1].l)) {
      lineart_bounding_area_link_triangle(
          rb, &ba[1], rt, B1, recursive, recursive_level + 1, do_intersection);
    }
    if (LRT_BOUND_AREA_CROSSES(B1, &ba[2].l)) {
      lineart_bounding_area_link_triangle(
          rb, &ba[2], rt, B1, recursive, recursive_level + 1, do_intersection);
    }
    if (LRT_BOUND_AREA_CROSSES(B1, &ba[3].l)) {
      lineart_bounding_area_link_triangle(
          rb, &ba[3], rt, B1, recursive, recursive_level + 1, do_intersection);
    }
  }
}

static void lineart_bounding_area_link_line(LineartRenderBuffer *rb,
                                            LineartBoundingArea *root_ba,
                                            LineartEdge *e)
{
  if (root_ba->child == NULL) {
    lineart_list_append_pointer_pool(&root_ba->linked_lines, &rb->render_data_pool, e);
  }
  else {
    if (lineart_bounding_area_line_intersect(
            rb, e->v1->fbcoord, e->v2->fbcoord, &root_ba->child[0])) {
      lineart_bounding_area_link_line(rb, &root_ba->child[0], e);
    }
    if (lineart_bounding_area_line_intersect(
            rb, e->v1->fbcoord, e->v2->fbcoord, &root_ba->child[1])) {
      lineart_bounding_area_link_line(rb, &root_ba->child[1], e);
    }
    if (lineart_bounding_area_line_intersect(
            rb, e->v1->fbcoord, e->v2->fbcoord, &root_ba->child[2])) {
      lineart_bounding_area_link_line(rb, &root_ba->child[2], e);
    }
    if (lineart_bounding_area_line_intersect(
            rb, e->v1->fbcoord, e->v2->fbcoord, &root_ba->child[3])) {
      lineart_bounding_area_link_line(rb, &root_ba->child[3], e);
    }
  }
}

/**
 * Link lines to their respective bounding areas.
 */
static void lineart_main_link_lines(LineartRenderBuffer *rb)
{
  LRT_ITER_ALL_LINES_BEGIN
  {
    int r1, r2, c1, c2, row, col;
    if (lineart_get_edge_bounding_areas(rb, e, &r1, &r2, &c1, &c2)) {
      for (row = r1; row != r2 + 1; row++) {
        for (col = c1; col != c2 + 1; col++) {
          lineart_bounding_area_link_line(
              rb, &rb->initial_bounding_areas[row * LRT_BA_ROWS + col], e);
        }
      }
    }
  }
  LRT_ITER_ALL_LINES_END
}

static bool lineart_get_triangle_bounding_areas(LineartRenderBuffer *rb,
                                                LineartTriangle *rt,
                                                int *rowbegin,
                                                int *rowend,
                                                int *colbegin,
                                                int *colend)
{
  double sp_w = rb->width_per_tile, sp_h = rb->height_per_tile;
  double b[4];

  if (!rt->v[0] || !rt->v[1] || !rt->v[2]) {
    return false;
  }

  b[0] = MIN3(rt->v[0]->fbcoord[0], rt->v[1]->fbcoord[0], rt->v[2]->fbcoord[0]);
  b[1] = MAX3(rt->v[0]->fbcoord[0], rt->v[1]->fbcoord[0], rt->v[2]->fbcoord[0]);
  b[2] = MIN3(rt->v[0]->fbcoord[1], rt->v[1]->fbcoord[1], rt->v[2]->fbcoord[1]);
  b[3] = MAX3(rt->v[0]->fbcoord[1], rt->v[1]->fbcoord[1], rt->v[2]->fbcoord[1]);

  if (b[0] > 1 || b[1] < -1 || b[2] > 1 || b[3] < -1) {
    return false;
  }

  (*colbegin) = (int)((b[0] + 1.0) / sp_w);
  (*colend) = (int)((b[1] + 1.0) / sp_w);
  (*rowend) = rb->tile_count_y - (int)((b[2] + 1.0) / sp_h) - 1;
  (*rowbegin) = rb->tile_count_y - (int)((b[3] + 1.0) / sp_h) - 1;

  if ((*colend) >= rb->tile_count_x) {
    (*colend) = rb->tile_count_x - 1;
  }
  if ((*rowend) >= rb->tile_count_y) {
    (*rowend) = rb->tile_count_y - 1;
  }
  if ((*colbegin) < 0) {
    (*colbegin) = 0;
  }
  if ((*rowbegin) < 0) {
    (*rowbegin) = 0;
  }

  return true;
}

static bool lineart_get_edge_bounding_areas(LineartRenderBuffer *rb,
                                            LineartEdge *e,
                                            int *rowbegin,
                                            int *rowend,
                                            int *colbegin,
                                            int *colend)
{
  double sp_w = rb->width_per_tile, sp_h = rb->height_per_tile;
  double b[4];

  if (!e->v1 || !e->v2) {
    return false;
  }

  if (e->v1->fbcoord[0] != e->v1->fbcoord[0] || e->v2->fbcoord[0] != e->v2->fbcoord[0]) {
    return false;
  }

  b[0] = MIN2(e->v1->fbcoord[0], e->v2->fbcoord[0]);
  b[1] = MAX2(e->v1->fbcoord[0], e->v2->fbcoord[0]);
  b[2] = MIN2(e->v1->fbcoord[1], e->v2->fbcoord[1]);
  b[3] = MAX2(e->v1->fbcoord[1], e->v2->fbcoord[1]);

  if (b[0] > 1 || b[1] < -1 || b[2] > 1 || b[3] < -1) {
    return false;
  }

  (*colbegin) = (int)((b[0] + 1.0) / sp_w);
  (*colend) = (int)((b[1] + 1.0) / sp_w);
  (*rowend) = rb->tile_count_y - (int)((b[2] + 1.0) / sp_h) - 1;
  (*rowbegin) = rb->tile_count_y - (int)((b[3] + 1.0) / sp_h) - 1;

  /* It's possible that the line stretches too much out to the side, resulting negative value. */
  if ((*rowend) < (*rowbegin)) {
    (*rowend) = rb->tile_count_y - 1;
  }

  if ((*colend) < (*colbegin)) {
    (*colend) = rb->tile_count_x - 1;
  }

  CLAMP((*colbegin), 0, rb->tile_count_x - 1);
  CLAMP((*rowbegin), 0, rb->tile_count_y - 1);
  CLAMP((*colend), 0, rb->tile_count_x - 1);
  CLAMP((*rowend), 0, rb->tile_count_y - 1);

  return true;
}

/**
 * This only gets initial "biggest" tile.
 */
LineartBoundingArea *MOD_lineart_get_parent_bounding_area(LineartRenderBuffer *rb,
                                                          double x,
                                                          double y)
{
  double sp_w = rb->width_per_tile, sp_h = rb->height_per_tile;
  int col, row;

  if (x > 1 || x < -1 || y > 1 || y < -1) {
    return 0;
  }

  col = (int)((x + 1.0) / sp_w);
  row = rb->tile_count_y - (int)((y + 1.0) / sp_h) - 1;

  if (col >= rb->tile_count_x) {
    col = rb->tile_count_x - 1;
  }
  if (row >= rb->tile_count_y) {
    row = rb->tile_count_y - 1;
  }
  if (col < 0) {
    col = 0;
  }
  if (row < 0) {
    row = 0;
  }

  return &rb->initial_bounding_areas[row * LRT_BA_ROWS + col];
}

static LineartBoundingArea *lineart_get_bounding_area(LineartRenderBuffer *rb, double x, double y)
{
  LineartBoundingArea *iba;
  double sp_w = rb->width_per_tile, sp_h = rb->height_per_tile;
  int c = (int)((x + 1.0) / sp_w);
  int r = rb->tile_count_y - (int)((y + 1.0) / sp_h) - 1;
  if (r < 0) {
    r = 0;
  }
  if (c < 0) {
    c = 0;
  }
  if (r >= rb->tile_count_y) {
    r = rb->tile_count_y - 1;
  }
  if (c >= rb->tile_count_x) {
    c = rb->tile_count_x - 1;
  }

  iba = &rb->initial_bounding_areas[r * LRT_BA_ROWS + c];
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

/**
 * Wrapper for more convenience.
 */
LineartBoundingArea *MOD_lineart_get_bounding_area(LineartRenderBuffer *rb, double x, double y)
{
  LineartBoundingArea *ba;
  if ((ba = MOD_lineart_get_parent_bounding_area(rb, x, y)) != NULL) {
    return lineart_get_bounding_area(rb, x, y);
  }
  return NULL;
}

/**
 * Sequentially add triangles into render buffer. This also does intersection along the way.
 */
static void lineart_main_add_triangles(LineartRenderBuffer *rb)
{
  LineartTriangle *rt;
  int i, lim;
  int x1, x2, y1, y2;
  int r, co;

  LISTBASE_FOREACH (LineartElementLinkNode *, reln, &rb->triangle_buffer_pointers) {
    rt = reln->pointer;
    lim = reln->element_count;
    for (i = 0; i < lim; i++) {
      if ((rt->flags & LRT_CULL_USED) || (rt->flags & LRT_CULL_DISCARD)) {
        rt = (void *)(((uchar *)rt) + rb->triangle_size);
        continue;
      }
      if (lineart_get_triangle_bounding_areas(rb, rt, &y1, &y2, &x1, &x2)) {
        for (co = x1; co <= x2; co++) {
          for (r = y1; r <= y2; r++) {
            lineart_bounding_area_link_triangle(rb,
                                                &rb->initial_bounding_areas[r * LRT_BA_ROWS + co],
                                                rt,
                                                0,
                                                1,
                                                0,
                                                (!(rt->flags & LRT_TRIANGLE_NO_INTERSECTION)));
          }
        }
      } /* Else throw away. */
      rt = (void *)(((uchar *)rt) + rb->triangle_size);
    }
  }
}

/**
 * This function gets the tile for the point `e->v1`, and later use #lineart_bounding_area_next()
 * to get next along the way.
 */
static LineartBoundingArea *lineart_edge_first_bounding_area(LineartRenderBuffer *rb,
                                                             LineartEdge *e)
{
  double data[2] = {e->v1->fbcoord[0], e->v1->fbcoord[1]};
  double LU[2] = {-1, 1}, RU[2] = {1, 1}, LB[2] = {-1, -1}, RB[2] = {1, -1};
  double r = 1, sr = 1;

  if (data[0] > -1 && data[0] < 1 && data[1] > -1 && data[1] < 1) {
    return lineart_get_bounding_area(rb, data[0], data[1]);
  }

  if (lineart_LineIntersectTest2d(e->v1->fbcoord, e->v2->fbcoord, LU, RU, &sr) && sr < r &&
      sr > 0) {
    r = sr;
  }
  if (lineart_LineIntersectTest2d(e->v1->fbcoord, e->v2->fbcoord, LB, RB, &sr) && sr < r &&
      sr > 0) {
    r = sr;
  }
  if (lineart_LineIntersectTest2d(e->v1->fbcoord, e->v2->fbcoord, LB, LU, &sr) && sr < r &&
      sr > 0) {
    r = sr;
  }
  if (lineart_LineIntersectTest2d(e->v1->fbcoord, e->v2->fbcoord, RB, RU, &sr) && sr < r &&
      sr > 0) {
    r = sr;
  }
  interp_v2_v2v2_db(data, e->v1->fbcoord, e->v2->fbcoord, r);

  return lineart_get_bounding_area(rb, data[0], data[1]);
}

/**
 * This march along one render line in image space and
 * get the next bounding area the line is crossing.
 */
static LineartBoundingArea *lineart_bounding_area_next(LineartBoundingArea *this,
                                                       LineartEdge *e,
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
    rx = this->r;
    ry = y + k * (rx - x);

    /* If we are marching towards the top. */
    if (positive_y > 0) {
      uy = this->u;
      ux = x + (uy - y) / k;
      r1 = ratiod(e->v1->fbcoord[0], e->v2->fbcoord[0], rx);
      r2 = ratiod(e->v1->fbcoord[0], e->v2->fbcoord[0], ux);
      if (MIN2(r1, r2) > 1) {
        return 0;
      }

      /* We reached the right side before the top side. */
      if (r1 <= r2) {
        LISTBASE_FOREACH (LinkData *, lip, &this->rp) {
          ba = lip->data;
          if (ba->u >= ry && ba->b < ry) {
            *next_x = rx;
            *next_y = ry;
            return ba;
          }
        }
      }
      /* We reached the top side before the right side. */
      else {
        LISTBASE_FOREACH (LinkData *, lip, &this->up) {
          ba = lip->data;
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
      by = this->b;
      bx = x + (by - y) / k;
      r1 = ratiod(e->v1->fbcoord[0], e->v2->fbcoord[0], rx);
      r2 = ratiod(e->v1->fbcoord[0], e->v2->fbcoord[0], bx);
      if (MIN2(r1, r2) > 1) {
        return 0;
      }
      if (r1 <= r2) {
        LISTBASE_FOREACH (LinkData *, lip, &this->rp) {
          ba = lip->data;
          if (ba->u >= ry && ba->b < ry) {
            *next_x = rx;
            *next_y = ry;
            return ba;
          }
        }
      }
      else {
        LISTBASE_FOREACH (LinkData *, lip, &this->bp) {
          ba = lip->data;
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
      r1 = ratiod(e->v1->fbcoord[0], e->v2->fbcoord[0], this->r);
      if (r1 > 1) {
        return 0;
      }
      LISTBASE_FOREACH (LinkData *, lip, &this->rp) {
        ba = lip->data;
        if (ba->u >= y && ba->b < y) {
          *next_x = this->r;
          *next_y = y;
          return ba;
        }
      }
    }
  }

  /* If we are marching towards the left. */
  else if (positive_x < 0) {
    lx = this->l;
    ly = y + k * (lx - x);

    /* If we are marching towards the top. */
    if (positive_y > 0) {
      uy = this->u;
      ux = x + (uy - y) / k;
      r1 = ratiod(e->v1->fbcoord[0], e->v2->fbcoord[0], lx);
      r2 = ratiod(e->v1->fbcoord[0], e->v2->fbcoord[0], ux);
      if (MIN2(r1, r2) > 1) {
        return 0;
      }
      if (r1 <= r2) {
        LISTBASE_FOREACH (LinkData *, lip, &this->lp) {
          ba = lip->data;
          if (ba->u >= ly && ba->b < ly) {
            *next_x = lx;
            *next_y = ly;
            return ba;
          }
        }
      }
      else {
        LISTBASE_FOREACH (LinkData *, lip, &this->up) {
          ba = lip->data;
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
      by = this->b;
      bx = x + (by - y) / k;
      r1 = ratiod(e->v1->fbcoord[0], e->v2->fbcoord[0], lx);
      r2 = ratiod(e->v1->fbcoord[0], e->v2->fbcoord[0], bx);
      if (MIN2(r1, r2) > 1) {
        return 0;
      }
      if (r1 <= r2) {
        LISTBASE_FOREACH (LinkData *, lip, &this->lp) {
          ba = lip->data;
          if (ba->u >= ly && ba->b < ly) {
            *next_x = lx;
            *next_y = ly;
            return ba;
          }
        }
      }
      else {
        LISTBASE_FOREACH (LinkData *, lip, &this->bp) {
          ba = lip->data;
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
      r1 = ratiod(e->v1->fbcoord[0], e->v2->fbcoord[0], this->l);
      if (r1 > 1) {
        return 0;
      }
      LISTBASE_FOREACH (LinkData *, lip, &this->lp) {
        ba = lip->data;
        if (ba->u >= y && ba->b < y) {
          *next_x = this->l;
          *next_y = y;
          return ba;
        }
      }
    }
  }
  /* If the line is completely vertical, hence X difference == 0. */
  else {
    if (positive_y > 0) {
      r1 = ratiod(e->v1->fbcoord[1], e->v2->fbcoord[1], this->u);
      if (r1 > 1) {
        return 0;
      }
      LISTBASE_FOREACH (LinkData *, lip, &this->up) {
        ba = lip->data;
        if (ba->r > x && ba->l <= x) {
          *next_x = x;
          *next_y = this->u;
          return ba;
        }
      }
    }
    else if (positive_y < 0) {
      r1 = ratiod(e->v1->fbcoord[1], e->v2->fbcoord[1], this->b);
      if (r1 > 1) {
        return 0;
      }
      LISTBASE_FOREACH (LinkData *, lip, &this->bp) {
        ba = lip->data;
        if (ba->r > x && ba->l <= x) {
          *next_x = x;
          *next_y = this->b;
          return ba;
        }
      }
    }
    else {
      /* Segment has no length. */
      return 0;
    }
  }
  return 0;
}

/**
 * This is the entry point of all line art calculations.
 *
 * \return True when a change is made.
 */
bool MOD_lineart_compute_feature_lines(Depsgraph *depsgraph, LineartGpencilModifierData *lmd)
{
  LineartRenderBuffer *rb;
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  int intersections_only = 0; /* Not used right now, but preserve for future. */

  if (!scene->camera) {
    return false;
  }

  rb = lineart_create_render_buffer(scene, lmd);

  /* Triangle thread testing data size varies depending on the thread count.
   * See definition of LineartTriangleThread for details. */
  rb->triangle_size = lineart_triangle_size_get(scene, rb);

  /* This is used to limit calculation to a certain level to save time, lines who have higher
   * occlusion levels will get ignored. */
  rb->max_occlusion_level = MAX2(lmd->level_start, lmd->level_end);

  /* FIXME(Yiming): See definition of int #LineartRenderBuffer::_source_type for detailed. */
  rb->_source_type = lmd->source_type;
  rb->_source_collection = lmd->source_collection;
  rb->_source_object = lmd->source_object;

  /* Get view vector before loading geometries, because we detect feature lines there. */
  lineart_main_get_view_vector(rb);
  lineart_main_load_geometries(
      depsgraph, scene, scene->camera, rb, lmd->calculation_flags & LRT_ALLOW_DUPLI_OBJECTS);

  if (!rb->vertex_buffer_pointers.first) {
    /* No geometry loaded, return early. */
    return true;
  }

  /* Initialize the bounding box acceleration structure, it's a lot like BVH in 3D. */
  lineart_main_bounding_area_make_initial(rb);

  /* We need to get cut into triangles that are crossing near/far plans, only this way can we get
   * correct coordinates of those clipped lines. Done in two steps,
   * setting clip_far==false for near plane. */
  lineart_main_cull_triangles(rb, false);
  /* `clip_far == true` for far plane. */
  lineart_main_cull_triangles(rb, true);

  /* At this point triangle adjacent info pointers is no longer needed, free them. */
  lineart_main_free_adjacent_data(rb);

  /* Do the perspective division after clipping is done. */
  lineart_main_perspective_division(rb);

  /* Triangle intersections are done here during sequential adding of them. Only after this,
   * triangles and lines are all linked with acceleration structure, and the 2D occlusion stage
   * can do its job. */
  lineart_main_add_triangles(rb);

  /* Link lines to acceleration structure, this can only be done after perspective division, if
   * we do it after triangles being added, the acceleration structure has already been
   * subdivided, this way we do less list manipulations. */
  lineart_main_link_lines(rb);

  /* "intersection_only" is preserved for being called in a standalone fashion.
   * If so the data will already be available at the stage. Otherwise we do the occlusion and
   * chaining etc.*/

  if (!intersections_only) {

    /* Occlusion is work-and-wait. This call will not return before work is completed. */
    lineart_main_occlusion_begin(rb);

    /* Chaining is all single threaded. See lineart_chain.c
     * In this particular call, only lines that are geometrically connected (share the _exact_
     * same end point) will be chained together. */
    MOD_lineart_chain_feature_lines(rb);

    /* We are unable to take care of occlusion if we only connect end points, so here we do a
     * spit, where the splitting point could be any cut in e->segments. */
    MOD_lineart_chain_split_for_fixed_occlusion(rb);

    /* Then we connect chains based on the _proximity_ of their end points in image space, here's
     * the place threshold value gets involved. */

    /* do_geometry_space = true. */
    MOD_lineart_chain_connect(rb);

    /* After chaining, we need to clear flags so we don't confuse GPencil generation calls. */
    MOD_lineart_chain_clear_picked_flag(rb);

    float *t_image = &lmd->chaining_image_threshold;
    /* This configuration ensures there won't be accidental lost of short unchained segments. */
    MOD_lineart_chain_discard_short(rb, MIN2(*t_image, 0.001f) - FLT_EPSILON);

    if (rb->angle_splitting_threshold > FLT_EPSILON) {
      MOD_lineart_chain_split_angle(rb, rb->angle_splitting_threshold);
    }
  }

  if (G.debug_value == 4000) {
    lineart_count_and_print_render_buffer_memory(rb);
  }

  return true;
}

static int lineart_rb_edge_types(LineartRenderBuffer *rb)
{
  int types = 0;
  types |= rb->use_contour ? LRT_EDGE_FLAG_CONTOUR : 0;
  types |= rb->use_crease ? LRT_EDGE_FLAG_CREASE : 0;
  types |= rb->use_material ? LRT_EDGE_FLAG_MATERIAL : 0;
  types |= rb->use_edge_marks ? LRT_EDGE_FLAG_EDGE_MARK : 0;
  types |= rb->use_intersections ? LRT_EDGE_FLAG_INTERSECTION : 0;
  return types;
}

static void lineart_gpencil_generate(LineartRenderBuffer *rb,
                                     Depsgraph *depsgraph,
                                     Object *gpencil_object,
                                     float (*gp_obmat_inverse)[4],
                                     bGPDlayer *UNUSED(gpl),
                                     bGPDframe *gpf,
                                     int level_start,
                                     int level_end,
                                     int material_nr,
                                     Object *source_object,
                                     Collection *source_collection,
                                     int types,
                                     uchar transparency_flags,
                                     uchar transparency_mask,
                                     short thickness,
                                     float opacity,
                                     const char *source_vgname,
                                     const char *vgname,
                                     int modifier_flags)
{
  if (rb == NULL) {
    if (G.debug_value == 4000) {
      printf("NULL Lineart rb!\n");
    }
    return;
  }

  int stroke_count = 0;
  int color_idx = 0;

  Object *orig_ob = NULL;
  if (source_object) {
    orig_ob = source_object->id.orig_id ? (Object *)source_object->id.orig_id : source_object;
  }

  Collection *orig_col = NULL;
  if (source_collection) {
    orig_col = source_collection->id.orig_id ? (Collection *)source_collection->id.orig_id :
                                               source_collection;
  }

  /* (!orig_col && !orig_ob) means the whole scene is selected. */

  float mat[4][4];
  unit_m4(mat);

  int enabled_types = lineart_rb_edge_types(rb);
  bool invert_input = modifier_flags & LRT_GPENCIL_INVERT_SOURCE_VGROUP;
  bool match_output = modifier_flags & LRT_GPENCIL_MATCH_OUTPUT_VGROUP;

  LISTBASE_FOREACH (LineartLineChain *, rlc, &rb->chains) {

    if (rlc->picked) {
      continue;
    }
    if (!(rlc->type & (types & enabled_types))) {
      continue;
    }
    if (rlc->level > level_end || rlc->level < level_start) {
      continue;
    }
    if (orig_ob && orig_ob != rlc->object_ref) {
      continue;
    }
    if (orig_col && rlc->object_ref) {
      if (!BKE_collection_has_object_recursive_instanced(orig_col, (Object *)rlc->object_ref)) {
        continue;
      }
    }
    if (transparency_flags & LRT_GPENCIL_TRANSPARENCY_ENABLE) {
      if (transparency_flags & LRT_GPENCIL_TRANSPARENCY_MATCH) {
        if (rlc->transparency_mask != transparency_mask) {
          continue;
        }
      }
      else {
        if (!(rlc->transparency_mask & transparency_mask)) {
          continue;
        }
      }
    }

    /* Preserved: If we ever do asynchronous generation, this picked flag should be set here. */
    // rlc->picked = 1;

    int array_idx = 0;
    int count = MOD_lineart_chain_count(rlc);
    bGPDstroke *gps = BKE_gpencil_stroke_add(gpf, color_idx, count, thickness, false);

    float *stroke_data = MEM_callocN(sizeof(float) * count * GP_PRIM_DATABUF_SIZE,
                                     "line art add stroke");

    LISTBASE_FOREACH (LineartLineChainItem *, rlci, &rlc->chain) {
      stroke_data[array_idx] = rlci->gpos[0];
      stroke_data[array_idx + 1] = rlci->gpos[1];
      stroke_data[array_idx + 2] = rlci->gpos[2];
      mul_m4_v3(gp_obmat_inverse, &stroke_data[array_idx]);
      stroke_data[array_idx + 3] = 1;       /* thickness. */
      stroke_data[array_idx + 4] = opacity; /* hardness?. */
      array_idx += 5;
    }

    BKE_gpencil_stroke_add_points(gps, stroke_data, count, mat);
    BKE_gpencil_dvert_ensure(gps);
    gps->mat_nr = max_ii(material_nr, 0);

    MEM_freeN(stroke_data);

    if (source_vgname && vgname) {
      Object *eval_ob = DEG_get_evaluated_object(depsgraph, rlc->object_ref);
      int gpdg = -1;
      if ((match_output || (gpdg = BKE_object_defgroup_name_index(gpencil_object, vgname)) >= 0)) {
        if (eval_ob && eval_ob->type == OB_MESH) {
          int dindex = 0;
          Mesh *me = (Mesh *)eval_ob->data;
          if (me->dvert) {
            LISTBASE_FOREACH (bDeformGroup *, db, &eval_ob->defbase) {
              if ((!source_vgname) || strstr(db->name, source_vgname) == db->name) {
                if (match_output) {
                  gpdg = BKE_object_defgroup_name_index(gpencil_object, db->name);
                  if (gpdg < 0) {
                    continue;
                  }
                }
                int sindex = 0, vindex;
                LISTBASE_FOREACH (LineartLineChainItem *, rlci, &rlc->chain) {
                  vindex = rlci->index;
                  if (vindex >= me->totvert) {
                    break;
                  }
                  MDeformWeight *mdw = BKE_defvert_ensure_index(&me->dvert[vindex], dindex);
                  MDeformWeight *gdw = BKE_defvert_ensure_index(&gps->dvert[sindex], gpdg);

                  float use_weight = mdw->weight;
                  if (invert_input) {
                    use_weight = 1 - use_weight;
                  }
                  gdw->weight = MAX2(use_weight, gdw->weight);

                  sindex++;
                }
              }
              dindex++;
            }
          }
        }
      }
    }

    if (G.debug_value == 4000) {
      BKE_gpencil_stroke_set_random_color(gps);
    }
    BKE_gpencil_stroke_geometry_update(gpencil_object->data, gps);
    stroke_count++;
  }

  if (G.debug_value == 4000) {
    printf("LRT: Generated %d strokes.\n", stroke_count);
  }
}

/**
 * Wrapper for external calls.
 */
void MOD_lineart_gpencil_generate(LineartRenderBuffer *rb,
                                  Depsgraph *depsgraph,
                                  Object *ob,
                                  bGPDlayer *gpl,
                                  bGPDframe *gpf,
                                  char source_type,
                                  void *source_reference,
                                  int level_start,
                                  int level_end,
                                  int mat_nr,
                                  short edge_types,
                                  uchar transparency_flags,
                                  uchar transparency_mask,
                                  short thickness,
                                  float opacity,
                                  const char *source_vgname,
                                  const char *vgname,
                                  int modifier_flags)
{

  if (!gpl || !gpf || !ob) {
    return;
  }

  Object *source_object = NULL;
  Collection *source_collection = NULL;
  short use_types = 0;
  if (source_type == LRT_SOURCE_OBJECT) {
    if (!source_reference) {
      return;
    }
    source_object = (Object *)source_reference;
    /* Note that intersection lines will only be in collection. */
    use_types = edge_types & (~LRT_EDGE_FLAG_INTERSECTION);
  }
  else if (source_type == LRT_SOURCE_COLLECTION) {
    if (!source_reference) {
      return;
    }
    source_collection = (Collection *)source_reference;
    use_types = edge_types;
  }
  else {
    /* Whole scene. */
    use_types = edge_types;
  }
  float gp_obmat_inverse[4][4];
  invert_m4_m4(gp_obmat_inverse, ob->obmat);
  lineart_gpencil_generate(rb,
                           depsgraph,
                           ob,
                           gp_obmat_inverse,
                           gpl,
                           gpf,
                           level_start,
                           level_end,
                           mat_nr,
                           source_object,
                           source_collection,
                           use_types,
                           transparency_flags,
                           transparency_mask,
                           thickness,
                           opacity,
                           source_vgname,
                           vgname,
                           modifier_flags);
}
