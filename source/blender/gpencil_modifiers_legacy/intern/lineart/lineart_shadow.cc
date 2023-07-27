/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "MOD_gpencil_legacy_lineart.h"
#include "MOD_lineart.h"

#include "lineart_intern.h"

#include "BKE_global.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_lib_id.h"
#include "BKE_material.h"
#include "BKE_object.h"
#include "BKE_scene.h"
#include "DEG_depsgraph_query.h"
#include "DNA_collection_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"
#include "MEM_guardedalloc.h"

#include "BLI_task.h"
#include "PIL_time.h"

/* Shadow loading etc. ================== */

LineartElementLinkNode *lineart_find_matching_eln(ListBase *shadow_elns, int obindex)
{
  LISTBASE_FOREACH (LineartElementLinkNode *, eln, shadow_elns) {
    if (eln->obindex == obindex) {
      return eln;
    }
  }
  return nullptr;
}

LineartEdge *lineart_find_matching_edge(LineartElementLinkNode *shadow_eln,
                                        uint64_t edge_identifier)
{
  LineartEdge *elist = (LineartEdge *)shadow_eln->pointer;
  for (int i = 0; i < shadow_eln->element_count; i++) {
    if (elist[i].edge_identifier == edge_identifier) {
      return &elist[i];
    }
  }
  return nullptr;
}

static bool lineart_contour_viewed_from_dark_side(LineartData *ld, LineartEdge *e)
{

  if (!(e->flags & (LRT_EDGE_FLAG_CONTOUR | LRT_EDGE_FLAG_CONTOUR_SECONDARY))) {
    return false;
  }
  double view_vector[3];
  double light_vector[3];
  bool side_1_facing_light = false;
  bool side_2_facing_light = false;
  bool side_1_facing_camera = false;
  if (ld->conf.cam_is_persp_secondary) {
    sub_v3_v3v3_db(light_vector, ld->conf.camera_pos_secondary, e->v1->gloc);
  }
  else {
    copy_v3_v3_db(light_vector, ld->conf.view_vector_secondary);
  }
  double dot_light_1 = dot_v3v3_db(light_vector, e->t1->gn);
  side_1_facing_light = (dot_light_1 > 0);
  if (e->t2) {
    double dot_light_2 = dot_v3v3_db(light_vector, e->t2->gn);
    side_2_facing_light = (dot_light_2 > 0);
  }
  else {
    side_2_facing_light = !side_1_facing_light;
  }

  if (ld->conf.cam_is_persp) {
    sub_v3_v3v3_db(view_vector, ld->conf.camera_pos, e->v1->gloc);
  }
  else {
    copy_v3_v3_db(view_vector, ld->conf.view_vector);
  }
  double dot_view_1 = dot_v3v3_db(view_vector, e->t1->gn);
  side_1_facing_camera = (dot_view_1 > 0);

  if ((side_1_facing_camera && (!side_1_facing_light) && side_2_facing_light) ||
      ((!side_1_facing_camera) && side_1_facing_light && (!side_2_facing_light)))
  {
    return true;
  }
  return false;
}

void lineart_register_shadow_cuts(LineartData *ld, LineartEdge *e, LineartEdge *shadow_edge)
{
  LISTBASE_FOREACH (LineartEdgeSegment *, es, &shadow_edge->segments) {
    /* Convert to view space cutting points. */
    double la1 = es->ratio;
    double la2 = es->next ? es->next->ratio : 1.0f;
    la1 = la1 * e->v2->fbcoord[3] /
          (e->v1->fbcoord[3] - la1 * (e->v1->fbcoord[3] - e->v2->fbcoord[3]));
    la2 = la2 * e->v2->fbcoord[3] /
          (e->v1->fbcoord[3] - la2 * (e->v1->fbcoord[3] - e->v2->fbcoord[3]));
    uchar shadow_bits = (es->occlusion != 0) ? LRT_SHADOW_MASK_SHADED :
                                               LRT_SHADOW_MASK_ILLUMINATED;

    if (lineart_contour_viewed_from_dark_side(ld, e) && shadow_bits == LRT_SHADOW_MASK_ILLUMINATED)
    {
      shadow_bits = LRT_SHADOW_MASK_SHADED;
    }

    lineart_edge_cut(ld, e, la1, la2, 0, 0, shadow_bits);
  }
}

void lineart_register_intersection_shadow_cuts(LineartData *ld, ListBase *shadow_elns)
{
  if (!shadow_elns) {
    return;
  }

  LineartElementLinkNode *eln_isect_shadow = nullptr;
  LineartElementLinkNode *eln_isect_original = nullptr;

  LISTBASE_FOREACH (LineartElementLinkNode *, eln, shadow_elns) {
    if (eln->flags & LRT_ELEMENT_INTERSECTION_DATA) {
      eln_isect_shadow = eln;
      break;
    }
  }
  LISTBASE_FOREACH (LineartElementLinkNode *, eln, &ld->geom.line_buffer_pointers) {
    if (eln->flags & LRT_ELEMENT_INTERSECTION_DATA) {
      eln_isect_original = eln;
      break;
    }
  }
  if (!eln_isect_shadow || !eln_isect_original) {
    return;
  }

  /* Keeping it single threaded for now because a simple parallel_for could end up getting the same
   * #shadow_e in different threads. */
  for (int i = 0; i < eln_isect_original->element_count; i++) {
    LineartEdge *e = &((LineartEdge *)eln_isect_original->pointer)[i];
    LineartEdge *shadow_e = lineart_find_matching_edge(eln_isect_shadow,
                                                       uint64_t(e->edge_identifier));
    if (shadow_e) {
      lineart_register_shadow_cuts(ld, e, shadow_e);
    }
  }
}

/* Shadow computation part ================== */

static LineartShadowSegment *lineart_give_shadow_segment(LineartData *ld)
{
  BLI_spin_lock(&ld->lock_cuts);

  /* See if there is any already allocated memory we can reuse. */
  if (ld->wasted_shadow_cuts.first) {
    LineartShadowSegment *es = (LineartShadowSegment *)BLI_pophead(&ld->wasted_shadow_cuts);
    BLI_spin_unlock(&ld->lock_cuts);
    memset(es, 0, sizeof(LineartShadowSegment));
    return (LineartShadowSegment *)es;
  }
  BLI_spin_unlock(&ld->lock_cuts);

  /* Otherwise allocate some new memory. */
  return (LineartShadowSegment *)lineart_mem_acquire_thread(&ld->render_data_pool,
                                                            sizeof(LineartShadowSegment));
}

static void lineart_shadow_segment_slice_get(double *fb_co_1,
                                             double *fb_co_2,
                                             double *gloc_1,
                                             double *gloc_2,
                                             double ratio,
                                             double at_1,
                                             double at_2,
                                             double *r_fb_co,
                                             double *r_gloc)
{
  double real_at = ((at_2 - at_1) == 0) ? 0 : ((ratio - at_1) / (at_2 - at_1));
  double ga = fb_co_1[3] * real_at / (fb_co_2[3] * (1.0f - real_at) + fb_co_1[3] * real_at);
  interp_v3_v3v3_db(r_fb_co, fb_co_1, fb_co_2, real_at);
  r_fb_co[3] = interpd(fb_co_2[3], fb_co_1[3], ga);
  interp_v3_v3v3_db(r_gloc, gloc_1, gloc_2, ga);
}

/**
 * This function tries to get the closest projected segments along two end points.
 * The x,y of s1, s2 are aligned in frame-buffer coordinates, only z,w are different.
 * We will get the closest z/w as well as the corresponding global coordinates.
 *
 * \code{.unparsed}
 *             (far side)
 * l-------r [s1]  ^
 *       _-r [s2]  |    In this situation it will essentially return the coordinates of s2.
 *    _-`          |
 * l-`             |
 *
 *                    (far side)
 *             _-r [s2]   ^
 *          _-`           |   In this case the return coordinates would be `s2l` and `s1r`,
 * l-----_c`-----r [s1]   |   and `r_new` will be assigned coordinates of `c`.
 *    _-`                 |
 * l-`                    |
 * \endcode
 *
 * Returns true when a new cut (`c`) is needed in the middle, otherwise returns false, and
 * `*r_new_xxx` are not touched.
 */
static bool lineart_do_closest_segment(bool is_persp,
                                       double *s1_fb_co_1,
                                       double *s1_fb_co_2,
                                       double *s2_fb_co_1,
                                       double *s2_fb_co_2,
                                       double *s1_gloc_1,
                                       double *s1_gloc_2,
                                       double *s2_gloc_1,
                                       double *s2_gloc_2,
                                       double *r_fb_co_1,
                                       double *r_fb_co_2,
                                       double *r_gloc_1,
                                       double *r_gloc_2,
                                       double *r_new_in_the_middle,
                                       double *r_new_in_the_middle_global,
                                       double *r_new_at,
                                       bool *is_side_2r,
                                       bool *use_new_ref)
{
  int side = 0;
  int z_index = is_persp ? 3 : 2;
  /* Always use the closest point to the light camera. */
  if (s1_fb_co_1[z_index] >= s2_fb_co_1[z_index]) {
    copy_v4_v4_db(r_fb_co_1, s2_fb_co_1);
    copy_v3_v3_db(r_gloc_1, s2_gloc_1);
    side++;
  }
  if (s1_fb_co_2[z_index] >= s2_fb_co_2[z_index]) {
    copy_v4_v4_db(r_fb_co_2, s2_fb_co_2);
    copy_v3_v3_db(r_gloc_2, s2_gloc_2);
    *is_side_2r = true;
    side++;
  }
  if (s1_fb_co_1[z_index] <= s2_fb_co_1[z_index]) {
    copy_v4_v4_db(r_fb_co_1, s1_fb_co_1);
    copy_v3_v3_db(r_gloc_1, s1_gloc_1);
    side--;
  }
  if (s1_fb_co_2[z_index] <= s2_fb_co_2[z_index]) {
    copy_v4_v4_db(r_fb_co_2, s1_fb_co_2);
    copy_v3_v3_db(r_gloc_2, s1_gloc_2);
    *is_side_2r = false;
    side--;
  }

  /* No need to cut in the middle, because one segment completely overlaps the other. */
  if (side) {
    if (side > 0) {
      *is_side_2r = true;
      *use_new_ref = true;
    }
    else if (side < 0) {
      *is_side_2r = false;
      *use_new_ref = false;
    }
    return false;
  }

  /* Else there must be an intersection point in the middle. Use "w" value to linearly plot the
   * position and get image space "ratio" position. */
  double dl = s1_fb_co_1[z_index] - s2_fb_co_1[z_index];
  double dr = s1_fb_co_2[z_index] - s2_fb_co_2[z_index];
  double ga = ratiod(dl, dr, 0);
  *r_new_at = is_persp ? s2_fb_co_2[3] * ga / (s2_fb_co_1[3] * (1.0f - ga) + s2_fb_co_2[3] * ga) :
                         ga;
  interp_v3_v3v3_db(r_new_in_the_middle, s2_fb_co_1, s2_fb_co_2, *r_new_at);
  r_new_in_the_middle[3] = interpd(s2_fb_co_2[3], s2_fb_co_1[3], ga);
  interp_v3_v3v3_db(r_new_in_the_middle_global, s1_gloc_1, s1_gloc_2, ga);
  *use_new_ref = true;

  return true;
}

/* For each visible [segment] of the edge, create 1 shadow edge. Note if the original edge has
 * multiple visible cuts, multiple shadow edges should be generated. */
static void lineart_shadow_create_shadow_edge_array(LineartData *ld,
                                                    bool transform_edge_cuts,
                                                    bool do_light_contour)
{
/* If the segment is short enough, we ignore them because it's not prominently visible anyway. */
#define DISCARD_NONSENSE_SEGMENTS \
  if (es->occlusion != 0 || \
      (es->next && LRT_DOUBLE_CLOSE_ENOUGH(es->ratio, ((LineartEdgeSegment *)es->next)->ratio))) \
  { \
    LRT_ITER_ALL_LINES_NEXT; \
    continue; \
  }

  /* Count and allocate at once to save time. */
  int segment_count = 0;
  uint16_t accept_types = (LRT_EDGE_FLAG_CONTOUR | LRT_EDGE_FLAG_LOOSE);
  if (do_light_contour) {
    accept_types |= LRT_EDGE_FLAG_LIGHT_CONTOUR;
  }
  LRT_ITER_ALL_LINES_BEGIN
  {
    /* Only contour and loose edges can actually cast shadows. We allow light contour here because
     * we want to see if it also doubles as a view contour, in that case we also need to project
     * them. */
    if (!(e->flags & accept_types)) {
      continue;
    }
    if (e->flags == LRT_EDGE_FLAG_LIGHT_CONTOUR) {
      /* Check if the light contour also doubles as a view contour. */
      LineartEdge *orig_e = (LineartEdge *)e->t1;
      if (!orig_e->t2) {
        e->flags |= LRT_EDGE_FLAG_CONTOUR;
      }
      else {
        double vv[3];
        double *view_vector = vv;
        double dot_1 = 0, dot_2 = 0;
        double result;
        if (ld->conf.cam_is_persp) {
          sub_v3_v3v3_db(view_vector, orig_e->v1->gloc, ld->conf.camera_pos);
        }
        else {
          view_vector = ld->conf.view_vector;
        }

        dot_1 = dot_v3v3_db(view_vector, orig_e->t1->gn);
        dot_2 = dot_v3v3_db(view_vector, orig_e->t2->gn);

        if ((result = dot_1 * dot_2) <= 0 && (dot_1 + dot_2)) {
          /* If this edge is both a light contour and a view contour, mark it for the convenience
           * of generating it in the next iteration. */
          e->flags |= LRT_EDGE_FLAG_CONTOUR;
        }
      }
      if (!(e->flags & LRT_EDGE_FLAG_CONTOUR)) {
        continue;
      }
    }
    LISTBASE_FOREACH (LineartEdgeSegment *, es, &e->segments) {
      DISCARD_NONSENSE_SEGMENTS
      segment_count++;
    }
  }
  LRT_ITER_ALL_LINES_END

  LineartShadowEdge *sedge = static_cast<LineartShadowEdge *>(
      lineart_mem_acquire(&ld->render_data_pool, sizeof(LineartShadowEdge) * segment_count));
  LineartShadowSegment *sseg = static_cast<LineartShadowSegment *>(lineart_mem_acquire(
      &ld->render_data_pool, sizeof(LineartShadowSegment) * segment_count * 2));

  ld->shadow_edges = sedge;
  ld->shadow_edges_count = segment_count;

  int i = 0;
  LRT_ITER_ALL_LINES_BEGIN
  {
    if (!(e->flags & (LRT_EDGE_FLAG_CONTOUR | LRT_EDGE_FLAG_LOOSE))) {
      continue;
    }
    LISTBASE_FOREACH (LineartEdgeSegment *, es, &e->segments) {
      DISCARD_NONSENSE_SEGMENTS

      double next_at = es->next ? ((LineartEdgeSegment *)es->next)->ratio : 1.0f;
      /* Get correct XYZ and W coordinates. */
      interp_v3_v3v3_db(sedge[i].fbc1, e->v1->fbcoord, e->v2->fbcoord, es->ratio);
      interp_v3_v3v3_db(sedge[i].fbc2, e->v1->fbcoord, e->v2->fbcoord, next_at);

      /* Global coord for light-shadow separation line (occlusion-corrected light contour). */
      double ga1 = e->v1->fbcoord[3] * es->ratio /
                   (es->ratio * e->v1->fbcoord[3] + (1 - es->ratio) * e->v2->fbcoord[3]);
      double ga2 = e->v1->fbcoord[3] * next_at /
                   (next_at * e->v1->fbcoord[3] + (1 - next_at) * e->v2->fbcoord[3]);
      interp_v3_v3v3_db(sedge[i].g1, e->v1->gloc, e->v2->gloc, ga1);
      interp_v3_v3v3_db(sedge[i].g2, e->v1->gloc, e->v2->gloc, ga2);

      /* Assign an absurdly big W for initial distance so when triangles show up to catch the
       * shadow, their w must certainly be smaller than this value so the shadow catches
       * successfully. */
      sedge[i].fbc1[3] = 1e30;
      sedge[i].fbc2[3] = 1e30;
      sedge[i].fbc1[2] = 1e30;
      sedge[i].fbc2[2] = 1e30;

      /* Assign to the first segment's right and the last segment's left position */
      copy_v4_v4_db(sseg[i * 2].fbc2, sedge[i].fbc1);
      copy_v4_v4_db(sseg[i * 2 + 1].fbc1, sedge[i].fbc2);
      sseg[i * 2].ratio = 0.0f;
      sseg[i * 2 + 1].ratio = 1.0f;
      BLI_addtail(&sedge[i].shadow_segments, &sseg[i * 2]);
      BLI_addtail(&sedge[i].shadow_segments, &sseg[i * 2 + 1]);

      if (e->flags & LRT_EDGE_FLAG_LIGHT_CONTOUR) {
        sedge[i].e_ref = (LineartEdge *)e->t1;
        sedge[i].e_ref_light_contour = e;
        /* Restore original edge flag for edges "who is both view and light contour" so we still
         * have correct edge flags. */
        e->flags &= (~LRT_EDGE_FLAG_CONTOUR);
      }
      else {
        sedge[i].e_ref = e;
      }

      sedge[i].es_ref = es;

      i++;
    }
  }
  LRT_ITER_ALL_LINES_END

  /* Transform the cutting position to global space for regular feature lines. This is for
   * convenience of reusing the shadow cast function for both shadow line generation and silhouette
   * registration, which the latter one needs view-space coordinates, while cast shadow needs
   * global-space coordinates. */
  if (transform_edge_cuts) {
    LRT_ITER_ALL_LINES_BEGIN
    {
      LISTBASE_FOREACH (LineartEdgeSegment *, es, &e->segments) {
        es->ratio = e->v1->fbcoord[3] * es->ratio /
                    (es->ratio * e->v1->fbcoord[3] + (1 - es->ratio) * e->v2->fbcoord[3]);
      }
    }
    LRT_ITER_ALL_LINES_END
  }

  if (G.debug_value == 4000) {
    printf("Shadow: Added %d raw shadow_edges\n", segment_count);
  }
}

/* This function does the actual cutting on a given "shadow edge".
 * #start / #end determines the view(from light camera) space cutting ratio.
 * #start/end_gloc/fbc are the respective start/end coordinates.
 * #facing_light is set from the caller which determines if this edge landed on a triangle's light
 * facing side or not.
 *
 * Visually this function does this: (Top is the far side of the camera)
 *                      _-end
 *                   _-`
 * l[-------------_-`--------------]r [e]    1) Calls for cut on top of #e.
 *             _-`
 *          _-`
 *    start-`
 *
 *                      _-end
 *                   _-`
 * l[-----][------_-`----][--------]r [e]    2) Add cutting points on #e at #start/#end.
 *             _-`
 *          _-`
 *    start-`
 *
 *                      _-end
 *                   _-`
 *         [------_-`----]                   3) Call lineart_shadow_segment_slice_get() to
 *             _-`                              get coordinates of a visually aligned segment on
 *          _-`                                 #e with the incoming segment.
 *    start-`
 *
 *                _c-----]                   4) Call lineart_do_closest_segment() to find out the
 *             _-`                              actual geometry after cut, add a new cut if needed.
 *          _-`
 *        [`
 *
 * l[-----]       _][----][--------]r [e]    5) Write coordinates on cuts.
 *             _-`
 *          _-`
 *        [`
 *
 * This process is repeated on each existing segments of the shadow edge (#e), which ensures they
 * all have been tested for closest segments after cutting. And in the diagram it's clear that the
 * left/right side of cuts are likely to be discontinuous, each cut's left side designates the
 * right side of the last segment, and vice-versa. */
static void lineart_shadow_edge_cut(LineartData *ld,
                                    LineartShadowEdge *e,
                                    double start,
                                    double end,
                                    double *start_gloc,
                                    double *end_gloc,
                                    double *start_fb_co,
                                    double *end_fb_co,
                                    bool facing_light,
                                    uint32_t target_reference)
{
  LineartShadowSegment *seg, *i_seg;
  LineartShadowSegment *cut_start_after = static_cast<LineartShadowSegment *>(
                           e->shadow_segments.first),
                       *cut_end_before = static_cast<LineartShadowSegment *>(
                           e->shadow_segments.last);
  LineartShadowSegment *new_seg_1 = nullptr, *new_seg_2 = nullptr, *seg_1 = nullptr,
                       *seg_2 = nullptr;
  int untouched = 0;

  /* If for some reason the occlusion function may give a result that has zero length, or
   * reversed in direction, or NAN, we take care of them here. */
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
  for (seg = static_cast<LineartShadowSegment *>(e->shadow_segments.first); seg; seg = seg->next) {
    if (LRT_DOUBLE_CLOSE_ENOUGH(seg->ratio, start)) {
      cut_start_after = seg;
      new_seg_1 = cut_start_after;
      break;
    }
    if (seg->next == nullptr) {
      break;
    }
    i_seg = seg->next;
    if (i_seg->ratio > start + 1e-09 && start > seg->ratio) {
      cut_start_after = seg;
      new_seg_1 = lineart_give_shadow_segment(ld);
      break;
    }
  }
  if (!cut_start_after && LRT_DOUBLE_CLOSE_ENOUGH(1, end)) {
    untouched = 1;
  }
  for (seg = cut_start_after->next; seg; seg = seg->next) {
    /* We tried to cut at existing cutting point (e.g. where the line's occluded by a triangle
     * strip). */
    if (LRT_DOUBLE_CLOSE_ENOUGH(seg->ratio, end)) {
      cut_end_before = seg;
      new_seg_2 = cut_end_before;
      break;
    }
    /* This check is to prevent `es->ratio == 1.0` (where we don't need to cut because we are ratio
     * the end point). */
    if (!seg->next && LRT_DOUBLE_CLOSE_ENOUGH(1, end)) {
      cut_end_before = seg;
      new_seg_2 = cut_end_before;
      untouched = 1;
      break;
    }
    /* When an actual cut is needed in the line. */
    if (seg->ratio > end) {
      cut_end_before = seg;
      new_seg_2 = lineart_give_shadow_segment(ld);
      break;
    }
  }

  /* When we still can't find any existing cut in the line, we allocate new ones. */
  if (new_seg_1 == nullptr) {
    new_seg_1 = lineart_give_shadow_segment(ld);
  }
  if (new_seg_2 == nullptr) {
    if (untouched) {
      new_seg_2 = new_seg_1;
      cut_end_before = new_seg_2;
    }
    else {
      new_seg_2 = lineart_give_shadow_segment(ld);
    }
  }

  /* If we touched the cut list, we assign the new cut position based on new cut position,
   * this way we accommodate precision lost due to multiple cut inserts. */
  new_seg_1->ratio = start;
  if (!untouched) {
    new_seg_2->ratio = end;
  }

  double r_fb_co_1[4] = {0}, r_fb_co_2[4] = {0}, r_gloc_1[3] = {0}, r_gloc_2[3] = {0};
  double r_new_in_the_middle[4], r_new_in_the_middle_global[3], r_new_at;
  double *s1_fb_co_1, *s1_fb_co_2, *s1_gloc_1, *s1_gloc_2;

  /* Temporary coordinate records and "middle" records. */
  double t_g1[3], t_g2[3], t_fbc1[4], t_fbc2[4], m_g1[3], m_fbc1[4], m_g2[3], m_fbc2[4];
  bool is_side_2r, has_middle = false, use_new_ref;
  copy_v4_v4_db(t_fbc1, start_fb_co);
  copy_v3_v3_db(t_g1, start_gloc);

  /* Do max stuff before insert. */
  LineartShadowSegment *nes;
  for (seg = cut_start_after; seg != cut_end_before; seg = nes) {
    nes = seg->next;

    s1_fb_co_1 = seg->fbc2;
    s1_fb_co_2 = nes->fbc1;

    s1_gloc_1 = seg->g2;
    s1_gloc_2 = nes->g1;

    seg_1 = seg;
    seg_2 = nes;

    if (seg == cut_start_after) {
      lineart_shadow_segment_slice_get(seg->fbc2,
                                       nes->fbc1,
                                       seg->g2,
                                       nes->g1,
                                       new_seg_1->ratio,
                                       seg->ratio,
                                       nes->ratio,
                                       m_fbc1,
                                       m_g1);
      s1_fb_co_1 = m_fbc1;
      s1_gloc_1 = m_g1;

      seg_1 = new_seg_1;
      if (cut_start_after != new_seg_1) {
        BLI_insertlinkafter(&e->shadow_segments, cut_start_after, new_seg_1);
        copy_v4_v4_db(new_seg_1->fbc1, m_fbc1);
        copy_v3_v3_db(new_seg_1->g1, m_g1);
      }
    }
    if (nes == cut_end_before) {
      lineart_shadow_segment_slice_get(seg->fbc2,
                                       nes->fbc1,
                                       seg->g2,
                                       nes->g1,
                                       new_seg_2->ratio,
                                       seg->ratio,
                                       nes->ratio,
                                       m_fbc2,
                                       m_g2);
      s1_fb_co_2 = m_fbc2;
      s1_gloc_2 = m_g2;

      seg_2 = new_seg_2;
      if (cut_end_before != new_seg_2) {
        BLI_insertlinkbefore(&e->shadow_segments, cut_end_before, new_seg_2);
        copy_v4_v4_db(new_seg_2->fbc2, m_fbc2);
        copy_v3_v3_db(new_seg_2->g2, m_g2);
        /* Need to restore the flag for next segment's reference. */
        seg_2->flag = seg->flag;
        seg_2->target_reference = seg->target_reference;
      }
    }

    lineart_shadow_segment_slice_get(
        start_fb_co, end_fb_co, start_gloc, end_gloc, seg_2->ratio, start, end, t_fbc2, t_g2);

    if ((has_middle = lineart_do_closest_segment(ld->conf.cam_is_persp,
                                                 s1_fb_co_1,
                                                 s1_fb_co_2,
                                                 t_fbc1,
                                                 t_fbc2,
                                                 s1_gloc_1,
                                                 s1_gloc_2,
                                                 t_g1,
                                                 t_g2,
                                                 r_fb_co_1,
                                                 r_fb_co_2,
                                                 r_gloc_1,
                                                 r_gloc_2,
                                                 r_new_in_the_middle,
                                                 r_new_in_the_middle_global,
                                                 &r_new_at,
                                                 &is_side_2r,
                                                 &use_new_ref)))
    {
      LineartShadowSegment *ss_middle = lineart_give_shadow_segment(ld);
      ss_middle->ratio = interpf(seg_2->ratio, seg_1->ratio, r_new_at);
      ss_middle->flag = LRT_SHADOW_CASTED |
                        (use_new_ref ? (facing_light ? LRT_SHADOW_FACING_LIGHT : 0) : seg_1->flag);
      ss_middle->target_reference = (use_new_ref ? (target_reference) : seg_1->target_reference);
      copy_v3_v3_db(ss_middle->g1, r_new_in_the_middle_global);
      copy_v3_v3_db(ss_middle->g2, r_new_in_the_middle_global);
      copy_v4_v4_db(ss_middle->fbc1, r_new_in_the_middle);
      copy_v4_v4_db(ss_middle->fbc2, r_new_in_the_middle);
      BLI_insertlinkafter(&e->shadow_segments, seg_1, ss_middle);
    }
    /* Always assign the "closest" value to the segment. */
    copy_v4_v4_db(seg_1->fbc2, r_fb_co_1);
    copy_v3_v3_db(seg_1->g2, r_gloc_1);
    copy_v4_v4_db(seg_2->fbc1, r_fb_co_2);
    copy_v3_v3_db(seg_2->g1, r_gloc_2);

    if (has_middle) {
      seg_1->flag = LRT_SHADOW_CASTED |
                    (is_side_2r ? seg->flag : (facing_light ? LRT_SHADOW_FACING_LIGHT : 0));
      seg_1->target_reference = is_side_2r ? seg->target_reference : target_reference;
    }
    else {
      seg_1->flag = LRT_SHADOW_CASTED |
                    (use_new_ref ? (facing_light ? LRT_SHADOW_FACING_LIGHT : 0) : seg->flag);
      seg_1->target_reference = use_new_ref ? target_reference : seg->target_reference;
    }

    copy_v4_v4_db(t_fbc1, t_fbc2);
    copy_v3_v3_db(t_g1, t_g2);
  }
}

static bool lineart_shadow_cast_onto_triangle(LineartData *ld,
                                              LineartTriangle *tri,
                                              LineartShadowEdge *sedge,
                                              double *r_at_1,
                                              double *r_at_2,
                                              double *r_fb_co_1,
                                              double *r_fb_co_2,
                                              double *r_gloc_1,
                                              double *r_gloc_2,
                                              bool *r_facing_light)
{

  double *LFBC = sedge->fbc1, *RFBC = sedge->fbc2, *FBC0 = tri->v[0]->fbcoord,
         *FBC1 = tri->v[1]->fbcoord, *FBC2 = tri->v[2]->fbcoord;

  /* Bound box check. Because we have already done occlusion in the shadow camera, so any visual
   * intersection found in this function must mean that the triangle is behind the given line so it
   * will always project a shadow, hence no need to do depth bound-box check. */
  if ((MAX3(FBC0[0], FBC1[0], FBC2[0]) < MIN2(LFBC[0], RFBC[0])) ||
      (MIN3(FBC0[0], FBC1[0], FBC2[0]) > MAX2(LFBC[0], RFBC[0])) ||
      (MAX3(FBC0[1], FBC1[1], FBC2[1]) < MIN2(LFBC[1], RFBC[1])) ||
      (MIN3(FBC0[1], FBC1[1], FBC2[1]) > MAX2(LFBC[1], RFBC[1])))
  {
    return false;
  }

  bool is_persp = ld->conf.cam_is_persp;
  double ratio[2];
  int trie[2];
  int pi = 0;
  if (lineart_line_isec_2d_ignore_line2pos(FBC0, FBC1, LFBC, RFBC, &ratio[pi])) {
    trie[pi] = 0;
    pi++;
  }
  if (lineart_line_isec_2d_ignore_line2pos(FBC1, FBC2, LFBC, RFBC, &ratio[pi])) {
    /* ratio[0] == 1 && ratio[1] == 0 means we found a intersection at the same point of the
     * edge (FBC1), ignore this one and try get the intersection point from the other side of
     * the edge
     */
    if (!(pi && LRT_DOUBLE_CLOSE_ENOUGH(ratio[0], 1.0f) &&
          LRT_DOUBLE_CLOSE_ENOUGH(ratio[1], 0.0f))) {
      trie[pi] = 1;
      pi++;
    }
  }
  if (!pi) {
    return false;
  }
  if (pi == 1 && lineart_line_isec_2d_ignore_line2pos(FBC2, FBC0, LFBC, RFBC, &ratio[pi])) {

    if ((trie[0] == 0 && LRT_DOUBLE_CLOSE_ENOUGH(ratio[0], 0.0f) &&
         LRT_DOUBLE_CLOSE_ENOUGH(ratio[1], 1.0f)) ||
        (trie[0] == 1 && LRT_DOUBLE_CLOSE_ENOUGH(ratio[0], 1.0f) &&
         LRT_DOUBLE_CLOSE_ENOUGH(ratio[1], 0.0f)))
    {
      return false;
    }
    trie[pi] = 2;
    pi++;
  }

  if (pi != 2) {
    return false;
  }

  /* Get projected global position. */

  double gpos1[3], gpos2[3];
  double *v1 = (trie[0] == 0 ? FBC0 : (trie[0] == 1 ? FBC1 : FBC2));
  double *v2 = (trie[0] == 0 ? FBC1 : (trie[0] == 1 ? FBC2 : FBC0));
  double *v3 = (trie[1] == 0 ? FBC0 : (trie[1] == 1 ? FBC1 : FBC2));
  double *v4 = (trie[1] == 0 ? FBC1 : (trie[1] == 1 ? FBC2 : FBC0));
  double *gv1 = (trie[0] == 0 ? tri->v[0]->gloc :
                                (trie[0] == 1 ? tri->v[1]->gloc : tri->v[2]->gloc));
  double *gv2 = (trie[0] == 0 ? tri->v[1]->gloc :
                                (trie[0] == 1 ? tri->v[2]->gloc : tri->v[0]->gloc));
  double *gv3 = (trie[1] == 0 ? tri->v[0]->gloc :
                                (trie[1] == 1 ? tri->v[1]->gloc : tri->v[2]->gloc));
  double *gv4 = (trie[1] == 0 ? tri->v[1]->gloc :
                                (trie[1] == 1 ? tri->v[2]->gloc : tri->v[0]->gloc));
  double gr1 = is_persp ? v1[3] * ratio[0] / (ratio[0] * v1[3] + (1 - ratio[0]) * v2[3]) :
                          ratio[0];
  double gr2 = is_persp ? v3[3] * ratio[1] / (ratio[1] * v3[3] + (1 - ratio[1]) * v4[3]) :
                          ratio[1];
  interp_v3_v3v3_db(gpos1, gv1, gv2, gr1);
  interp_v3_v3v3_db(gpos2, gv3, gv4, gr2);

  double fbc1[4], fbc2[4];

  mul_v4_m4v3_db(fbc1, ld->conf.view_projection, gpos1);
  mul_v4_m4v3_db(fbc2, ld->conf.view_projection, gpos2);
  if (is_persp) {
    mul_v3db_db(fbc1, 1.0f / fbc1[3]);
    mul_v3db_db(fbc2, 1.0f / fbc2[3]);
  }

  int use = (fabs(LFBC[0] - RFBC[0]) > fabs(LFBC[1] - RFBC[1])) ? 0 : 1;
  double at1 = ratiod(LFBC[use], RFBC[use], fbc1[use]);
  double at2 = ratiod(LFBC[use], RFBC[use], fbc2[use]);
  if (at1 > at2) {
    swap_v3_v3_db(gpos1, gpos2);
    swap_v4_v4_db(fbc1, fbc2);
    SWAP(double, at1, at2);
  }

  /* If not effectively projecting anything. */
  if (at1 > (1.0f - FLT_EPSILON) || at2 < FLT_EPSILON) {
    return false;
  }

  /* Trim to edge's end points. */

  double t_fbc1[4], t_fbc2[4], t_gpos1[3], t_gpos2[3];
  bool trimmed1 = false, trimmed2 = false;
  if (at1 < 0 || at2 > 1) {
    double rat1 = (-at1) / (at2 - at1);
    double rat2 = (1.0f - at1) / (at2 - at1);
    double gat1 = is_persp ? fbc1[3] * rat1 / (rat1 * fbc1[3] + (1 - rat1) * fbc2[3]) : rat1;
    double gat2 = is_persp ? fbc1[3] * rat2 / (rat2 * fbc1[3] + (1 - rat2) * fbc2[3]) : rat2;
    if (at1 < 0) {
      interp_v3_v3v3_db(t_gpos1, gpos1, gpos2, gat1);
      interp_v3_v3v3_db(t_fbc1, fbc1, fbc2, rat1);
      t_fbc1[3] = interpd(fbc2[3], fbc1[3], gat1);
      at1 = 0;
      trimmed1 = true;
    }
    if (at2 > 1) {
      interp_v3_v3v3_db(t_gpos2, gpos1, gpos2, gat2);
      interp_v3_v3v3_db(t_fbc2, fbc1, fbc2, rat2);
      t_fbc2[3] = interpd(fbc2[3], fbc1[3], gat2);
      at2 = 1;
      trimmed2 = true;
    }
  }
  if (trimmed1) {
    copy_v4_v4_db(fbc1, t_fbc1);
    copy_v3_v3_db(gpos1, t_gpos1);
  }
  if (trimmed2) {
    copy_v4_v4_db(fbc2, t_fbc2);
    copy_v3_v3_db(gpos2, t_gpos2);
  }

  *r_at_1 = at1;
  *r_at_2 = at2;
  copy_v4_v4_db(r_fb_co_1, fbc1);
  copy_v4_v4_db(r_fb_co_2, fbc2);
  copy_v3_v3_db(r_gloc_1, gpos1);
  copy_v3_v3_db(r_gloc_2, gpos2);

  double camera_vector[3];

  if (is_persp) {
    sub_v3_v3v3_db(camera_vector, ld->conf.camera_pos, tri->v[0]->gloc);
  }
  else {
    copy_v3_v3_db(camera_vector, ld->conf.view_vector);
  }

  double dot_f = dot_v3v3_db(camera_vector, tri->gn);
  *r_facing_light = (dot_f < 0);

  return true;
}

/* The one step all to cast all visible edges in light camera back to other geometries behind them,
 * the result of this step can then be generated as actual LineartEdge's for occlusion test in view
 * camera. */
static void lineart_shadow_cast(LineartData *ld, bool transform_edge_cuts, bool do_light_contour)
{

  lineart_shadow_create_shadow_edge_array(ld, transform_edge_cuts, do_light_contour);

  /* Keep it single threaded for now because the loop will write "done" pointers to triangles. */
  for (int edge_i = 0; edge_i < ld->shadow_edges_count; edge_i++) {
    LineartShadowEdge *sedge = &ld->shadow_edges[edge_i];

    LineartTriangleThread *tri;
    double at_1, at_2;
    double fb_co_1[4], fb_co_2[4];
    double global_1[3], global_2[3];
    bool facing_light;

    LRT_EDGE_BA_MARCHING_BEGIN(sedge->fbc1, sedge->fbc2)
    {
      for (int i = 0; i < nba->triangle_count; i++) {
        tri = (LineartTriangleThread *)nba->linked_triangles[i];
        if (tri->testing_e[0] == (LineartEdge *)sedge || tri->base.mat_occlusion == 0 ||
            lineart_edge_from_triangle(
                (LineartTriangle *)tri, sedge->e_ref, ld->conf.allow_overlapping_edges))
        {
          continue;
        }
        tri->testing_e[0] = (LineartEdge *)sedge;

        if (lineart_shadow_cast_onto_triangle(ld,
                                              (LineartTriangle *)tri,
                                              sedge,
                                              &at_1,
                                              &at_2,
                                              fb_co_1,
                                              fb_co_2,
                                              global_1,
                                              global_2,
                                              &facing_light))
        {
          lineart_shadow_edge_cut(ld,
                                  sedge,
                                  at_1,
                                  at_2,
                                  global_1,
                                  global_2,
                                  fb_co_1,
                                  fb_co_2,
                                  facing_light,
                                  tri->base.target_reference);
        }
      }
      LRT_EDGE_BA_MARCHING_NEXT(sedge->fbc1, sedge->fbc2);
    }
    LRT_EDGE_BA_MARCHING_END;
  }
}

/* For each [segment] on a shadow shadow_edge, 1 LineartEdge will be generated with a cast shadow
 * edge flag (if that segment failed to cast onto anything then it's not generated). The original
 * shadow shadow_edge is optionally generated as a light contour. */
static bool lineart_shadow_cast_generate_edges(LineartData *ld,
                                               bool do_original_edges,
                                               LineartElementLinkNode **r_veln,
                                               LineartElementLinkNode **r_eeln)
{
  int tot_edges = 0;
  int tot_orig_edges = 0;
  for (int i = 0; i < ld->shadow_edges_count; i++) {
    LineartShadowEdge *sedge = &ld->shadow_edges[i];
    LISTBASE_FOREACH (LineartShadowSegment *, sseg, &sedge->shadow_segments) {
      if (!(sseg->flag & LRT_SHADOW_CASTED)) {
        continue;
      }
      if (!sseg->next) {
        break;
      }
      tot_edges++;
    }
    tot_orig_edges++;
  }

  int edge_alloc = tot_edges + (do_original_edges ? tot_orig_edges : 0);

  if (G.debug_value == 4000) {
    printf("Line art shadow segments total: %d\n", tot_edges);
  }

  if (!edge_alloc) {
    return false;
  }
  LineartElementLinkNode *veln = static_cast<LineartElementLinkNode *>(
      lineart_mem_acquire(ld->shadow_data_pool, sizeof(LineartElementLinkNode)));
  LineartElementLinkNode *eeln = static_cast<LineartElementLinkNode *>(
      lineart_mem_acquire(ld->shadow_data_pool, sizeof(LineartElementLinkNode)));
  veln->pointer = lineart_mem_acquire(ld->shadow_data_pool, sizeof(LineartVert) * edge_alloc * 2);
  eeln->pointer = lineart_mem_acquire(ld->shadow_data_pool, sizeof(LineartEdge) * edge_alloc);
  LineartEdgeSegment *es = static_cast<LineartEdgeSegment *>(
      lineart_mem_acquire(ld->shadow_data_pool, sizeof(LineartEdgeSegment) * edge_alloc));
  *r_veln = veln;
  *r_eeln = eeln;

  veln->element_count = edge_alloc * 2;
  eeln->element_count = edge_alloc;

  LineartVert *vlist = static_cast<LineartVert *>(veln->pointer);
  LineartEdge *elist = static_cast<LineartEdge *>(eeln->pointer);

  int ei = 0;
  for (int i = 0; i < ld->shadow_edges_count; i++) {
    LineartShadowEdge *sedge = &ld->shadow_edges[i];
    LISTBASE_FOREACH (LineartShadowSegment *, sseg, &sedge->shadow_segments) {
      if (!(sseg->flag & LRT_SHADOW_CASTED)) {
        continue;
      }
      if (!sseg->next) {
        break;
      }
      LineartEdge *e = &elist[ei];
      BLI_addtail(&e->segments, &es[ei]);
      LineartVert *v1 = &vlist[ei * 2], *v2 = &vlist[ei * 2 + 1];
      copy_v3_v3_db(v1->gloc, sseg->g2);
      copy_v3_v3_db(v2->gloc, ((LineartShadowSegment *)sseg->next)->g1);
      e->v1 = v1;
      e->v2 = v2;
      e->t1 = (LineartTriangle *)sedge->e_ref; /* See LineartEdge::t1 for usage. */
      e->t2 = (LineartTriangle *)(sedge->e_ref_light_contour ? sedge->e_ref_light_contour :
                                                               sedge->e_ref);
      e->target_reference = sseg->target_reference;
      e->edge_identifier = sedge->e_ref->edge_identifier;
      e->flags = (LRT_EDGE_FLAG_PROJECTED_SHADOW |
                  ((sseg->flag & LRT_SHADOW_FACING_LIGHT) ? LRT_EDGE_FLAG_SHADOW_FACING_LIGHT :
                                                            0));
      ei++;
    }
    if (do_original_edges) {
      /* Occlusion-corrected light contour. */
      LineartEdge *e = &elist[ei];
      BLI_addtail(&e->segments, &es[ei]);
      LineartVert *v1 = &vlist[ei * 2], *v2 = &vlist[ei * 2 + 1];
      v1->index = sedge->e_ref->v1->index;
      v2->index = sedge->e_ref->v2->index;
      copy_v3_v3_db(v1->gloc, sedge->g1);
      copy_v3_v3_db(v2->gloc, sedge->g2);
      uint64_t ref_1 = sedge->e_ref->t1 ? sedge->e_ref->t1->target_reference : 0;
      uint64_t ref_2 = sedge->e_ref->t2 ? sedge->e_ref->t2->target_reference : 0;
      e->edge_identifier = sedge->e_ref->edge_identifier;
      e->target_reference = ((ref_1 << 32) | ref_2);
      e->v1 = v1;
      e->v2 = v2;
      e->t1 = e->t2 = (LineartTriangle *)sedge->e_ref;
      e->flags = LRT_EDGE_FLAG_LIGHT_CONTOUR;
      if (lineart_contour_viewed_from_dark_side(ld, sedge->e_ref)) {
        lineart_edge_cut(ld, e, 0.0f, 1.0f, 0, 0, LRT_SHADOW_MASK_SHADED);
      }
      ei++;
    }
  }
  return true;
}

static void lineart_shadow_register_silhouette(LineartData *ld)
{
  /* Keeping it single threaded for now because a simple parallel_for could end up getting the same
   * #sedge->e_ref in different threads. */
  for (int i = 0; i < ld->shadow_edges_count; i++) {
    LineartShadowEdge *sedge = &ld->shadow_edges[i];

    LineartEdge *e = sedge->e_ref;
    LineartEdgeSegment *es = sedge->es_ref;
    double es_start = es->ratio, es_end = es->next ? es->next->ratio : 1.0f;
    LISTBASE_FOREACH (LineartShadowSegment *, sseg, &sedge->shadow_segments) {
      if (!(sseg->flag & LRT_SHADOW_CASTED)) {
        continue;
      }
      if (!sseg->next) {
        break;
      }

      uint32_t silhouette_flags = (sseg->target_reference & LRT_OBINDEX_HIGHER) |
                                  LRT_SHADOW_SILHOUETTE_ERASED_GROUP;

      double at_start = interpd(es_end, es_start, sseg->ratio);
      double at_end = interpd(es_end, es_start, sseg->next->ratio);
      lineart_edge_cut(ld, e, at_start, at_end, 0, 0, silhouette_flags);
    }
  }
}

/* To achieve enclosed shape effect, we need to:
 * 1) Show shaded segments against lit background.
 * 2) Erase lit segments against lit background. */
static void lineart_shadow_register_enclosed_shapes(LineartData *ld, LineartData *shadow_ld)
{
  LineartEdge *e;
  LineartEdgeSegment *es;
  for (int i = 0; i < shadow_ld->pending_edges.next; i++) {
    e = shadow_ld->pending_edges.array[i];

    /* Only care about shade-on-light and light-on-light situations, hence we only need
     * non-occluded segments in shadow buffer. */
    if (e->min_occ > 0) {
      continue;
    }
    for (es = static_cast<LineartEdgeSegment *>(e->segments.first); es; es = es->next) {
      if (es->occlusion > 0) {
        continue;
      }
      double next_at = es->next ? ((LineartEdgeSegment *)es->next)->ratio : 1.0f;
      LineartEdge *orig_e = (LineartEdge *)e->t2;

      /* Shadow view space to global. */
      double ga1 = e->v1->fbcoord[3] * es->ratio /
                   (es->ratio * e->v1->fbcoord[3] + (1 - es->ratio) * e->v2->fbcoord[3]);
      double ga2 = e->v1->fbcoord[3] * next_at /
                   (next_at * e->v1->fbcoord[3] + (1 - next_at) * e->v2->fbcoord[3]);
      double g1[3], g2[3], g1v[4], g2v[4];
      interp_v3_v3v3_db(g1, e->v1->gloc, e->v2->gloc, ga1);
      interp_v3_v3v3_db(g2, e->v1->gloc, e->v2->gloc, ga2);
      mul_v4_m4v3_db(g1v, ld->conf.view_projection, g1);
      mul_v4_m4v3_db(g2v, ld->conf.view_projection, g2);

      if (ld->conf.cam_is_persp) {
        mul_v3db_db(g1v, (1 / g1v[3]));
        mul_v3db_db(g2v, (1 / g2v[3]));
      }

      g1v[0] -= ld->conf.shift_x * 2;
      g1v[1] -= ld->conf.shift_y * 2;
      g2v[0] -= ld->conf.shift_x * 2;
      g2v[1] -= ld->conf.shift_y * 2;

#define GET_RATIO(n) \
  (fabs(orig_e->v2->fbcoord[0] - orig_e->v1->fbcoord[0]) > \
   fabs(orig_e->v2->fbcoord[1] - orig_e->v1->fbcoord[1])) ? \
      ((g##n##v[0] - orig_e->v1->fbcoord[0]) / \
       (orig_e->v2->fbcoord[0] - orig_e->v1->fbcoord[0])) : \
      ((g##n##v[1] - orig_e->v1->fbcoord[1]) / (orig_e->v2->fbcoord[1] - orig_e->v1->fbcoord[1]))
      double la1, la2;
      la1 = GET_RATIO(1);
      la2 = GET_RATIO(2);
#undef GET_RATIO

      lineart_edge_cut(ld, orig_e, la1, la2, 0, 0, LRT_SHADOW_MASK_ENCLOSED_SHAPE);
    }
  }
}

bool lineart_main_try_generate_shadow(Depsgraph *depsgraph,
                                      Scene *scene,
                                      LineartData *original_ld,
                                      LineartGpencilModifierData *lmd,
                                      LineartStaticMemPool *shadow_data_pool,
                                      LineartElementLinkNode **r_veln,
                                      LineartElementLinkNode **r_eeln,
                                      ListBase *r_calculated_edges_eln_list,
                                      LineartData **r_shadow_ld_if_reproject)
{
  if ((!original_ld->conf.use_shadow && !original_ld->conf.use_light_contour &&
       !original_ld->conf.shadow_selection) ||
      (!lmd->light_contour_object))
  {
    return false;
  }

  double t_start;
  if (G.debug_value == 4000) {
    t_start = PIL_check_seconds_timer();
  }

  bool is_persp = true;

  if (lmd->light_contour_object->type == OB_LAMP) {
    Light *la = (Light *)lmd->light_contour_object->data;
    if (la->type == LA_SUN) {
      is_persp = false;
    }
  }

  LineartData *ld = static_cast<LineartData *>(
      MEM_callocN(sizeof(LineartData), "LineArt render buffer copied"));
  memcpy(ld, original_ld, sizeof(LineartData));

  BLI_spin_init(&ld->lock_task);
  BLI_spin_init(&ld->lock_cuts);
  BLI_spin_init(&ld->render_data_pool.lock_mem);

  ld->conf.do_shadow_cast = true;
  ld->shadow_data_pool = shadow_data_pool;

  /* See LineartData::edge_data_pool for explanation. */
  if (ld->conf.shadow_selection) {
    ld->edge_data_pool = shadow_data_pool;
  }
  else {
    ld->edge_data_pool = &ld->render_data_pool;
  }

  copy_v3_v3_db(ld->conf.camera_pos_secondary, ld->conf.camera_pos);
  copy_m4_m4(ld->conf.cam_obmat_secondary, ld->conf.cam_obmat);

  copy_m4_m4(ld->conf.cam_obmat, lmd->light_contour_object->object_to_world);
  copy_v3db_v3fl(ld->conf.camera_pos, ld->conf.cam_obmat[3]);
  ld->conf.cam_is_persp_secondary = ld->conf.cam_is_persp;
  ld->conf.cam_is_persp = is_persp;
  ld->conf.near_clip = is_persp ? lmd->shadow_camera_near : -lmd->shadow_camera_far;
  ld->conf.far_clip = lmd->shadow_camera_far;
  ld->w = lmd->shadow_camera_size;
  ld->h = lmd->shadow_camera_size;
  /* Need to prevent wrong camera configuration so that shadow computation won't stall. */
  if (!ld->w || !ld->h) {
    ld->w = ld->h = 200;
  }
  if (!ld->conf.near_clip || !ld->conf.far_clip) {
    ld->conf.near_clip = 0.1f;
    ld->conf.far_clip = 200.0f;
  }
  ld->qtree.recursive_level = is_persp ? LRT_TILE_RECURSIVE_PERSPECTIVE : LRT_TILE_RECURSIVE_ORTHO;

  /* Contour and loose edge from light viewing direction will be cast as shadow, so only
   * force them on. If we need lit/shaded information for other line types, they are then
   * enabled as-is so that cutting positions can also be calculated through shadow projection.
   */
  if (!ld->conf.shadow_selection) {
    ld->conf.use_crease = ld->conf.use_material = ld->conf.use_edge_marks =
        ld->conf.use_intersections = ld->conf.use_light_contour = false;
  }
  else {
    ld->conf.use_contour_secondary = true;
    ld->conf.allow_duplicated_types = true;
  }
  ld->conf.use_loose = true;
  ld->conf.use_contour = true;

  ld->conf.max_occlusion_level = 0; /* No point getting see-through projections there. */
  ld->conf.use_back_face_culling = false;

  /* Override matrices to light "camera". */
  double proj[4][4], view[4][4], result[4][4];
  float inv[4][4];
  if (is_persp) {
    lineart_matrix_perspective_44d(proj, DEG2RAD(160), 1, ld->conf.near_clip, ld->conf.far_clip);
  }
  else {
    lineart_matrix_ortho_44d(
        proj, -ld->w, ld->w, -ld->h, ld->h, ld->conf.near_clip, ld->conf.far_clip);
  }
  invert_m4_m4(inv, ld->conf.cam_obmat);
  mul_m4db_m4db_m4fl(result, proj, inv);
  copy_m4_m4_db(proj, result);
  copy_m4_m4_db(ld->conf.view_projection, proj);
  unit_m4_db(view);
  copy_m4_m4_db(ld->conf.view, view);

  lineart_main_get_view_vector(ld);

  lineart_main_load_geometries(
      depsgraph, scene, nullptr, ld, lmd->flags & LRT_ALLOW_DUPLI_OBJECTS, true, nullptr);

  if (!ld->geom.vertex_buffer_pointers.first) {
    /* No geometry loaded, return early. */
    lineart_destroy_render_data_keep_init(ld);
    MEM_freeN(ld);
    return false;
  }

  /* The exact same process as in MOD_lineart_compute_feature_lines() until occlusion finishes.
   */

  lineart_main_bounding_area_make_initial(ld);
  lineart_main_cull_triangles(ld, false);
  lineart_main_cull_triangles(ld, true);
  lineart_main_free_adjacent_data(ld);
  lineart_main_perspective_division(ld);
  lineart_main_discard_out_of_frame_edges(ld);
  lineart_main_add_triangles(ld);
  lineart_main_bounding_areas_connect_post(ld);
  lineart_main_link_lines(ld);
  lineart_main_occlusion_begin(ld);

  /* Do shadow cast stuff then get generated vert/edge data. */
  lineart_shadow_cast(ld, true, false);
  bool any_generated = lineart_shadow_cast_generate_edges(ld, true, r_veln, r_eeln);

  if (ld->conf.shadow_selection) {
    memcpy(r_calculated_edges_eln_list, &ld->geom.line_buffer_pointers, sizeof(ListBase));
  }

  if (ld->conf.shadow_enclose_shapes) {
    /* Need loaded data for re-projecting the 3rd time to get shape boundary against lit/shaded
     * region. */
    (*r_shadow_ld_if_reproject) = ld;
  }
  else {
    lineart_destroy_render_data_keep_init(ld);
    MEM_freeN(ld);
  }

  if (G.debug_value == 4000) {
    double t_elapsed = PIL_check_seconds_timer() - t_start;
    printf("Line art shadow stage 1 time: %f\n", t_elapsed);
  }

  return any_generated;
}

struct LineartShadowFinalizeData {
  LineartData *ld;
  LineartVert *v;
  LineartEdge *e;
};

static void lineart_shadow_transform_task(void *__restrict userdata,
                                          const int element_index,
                                          const TaskParallelTLS *__restrict /*tls*/)
{
  LineartShadowFinalizeData *data = (LineartShadowFinalizeData *)userdata;
  LineartData *ld = data->ld;
  LineartVert *v = &data->v[element_index];
  mul_v4_m4v3_db(v->fbcoord, ld->conf.view_projection, v->gloc);
}

static void lineart_shadow_finalize_shadow_edges_task(void *__restrict userdata,
                                                      const int i,
                                                      const TaskParallelTLS *__restrict /*tls*/)
{
  LineartShadowFinalizeData *data = (LineartShadowFinalizeData *)userdata;
  LineartData *ld = data->ld;
  LineartEdge *e = data->e;

  if (e[i].flags & LRT_EDGE_FLAG_LIGHT_CONTOUR) {
    LineartElementLinkNode *eln = lineart_find_matching_eln(
        &ld->geom.vertex_buffer_pointers, e[i].edge_identifier & LRT_OBINDEX_HIGHER);
    if (eln) {
      int v1i = (((e[i].edge_identifier) >> 32) & LRT_OBINDEX_LOWER);
      int v2i = (e[i].edge_identifier & LRT_OBINDEX_LOWER);
      LineartVert *v = (LineartVert *)eln->pointer;
      /* If the global position is close enough, use the original vertex to prevent flickering
       * caused by very slim boundary condition in point_triangle_relation(). */
      if (LRT_CLOSE_LOOSER_v3(e[i].v1->gloc, v[v1i].gloc)) {
        e[i].v1 = &v[v1i];
      }
      if (LRT_CLOSE_LOOSER_v3(e[i].v2->gloc, v[v2i].gloc)) {
        e[i].v2 = &v[v2i];
      }
    }
  }
}

void lineart_main_transform_and_add_shadow(LineartData *ld,
                                           LineartElementLinkNode *veln,
                                           LineartElementLinkNode *eeln)
{

  TaskParallelSettings transform_settings;
  BLI_parallel_range_settings_defaults(&transform_settings);
  /* Set the minimum amount of edges a thread has to process. */
  transform_settings.min_iter_per_thread = 8192;

  LineartShadowFinalizeData data = {0};
  data.ld = ld;
  data.v = (LineartVert *)veln->pointer;
  data.e = (LineartEdge *)eeln->pointer;

  BLI_task_parallel_range(
      0, veln->element_count, &data, lineart_shadow_transform_task, &transform_settings);
  BLI_task_parallel_range(0,
                          eeln->element_count,
                          &data,
                          lineart_shadow_finalize_shadow_edges_task,
                          &transform_settings);
  for (int i = 0; i < eeln->element_count; i++) {
    lineart_add_edge_to_array(&ld->pending_edges, &data.e[i]);
  }

  BLI_addtail(&ld->geom.vertex_buffer_pointers, veln);
  BLI_addtail(&ld->geom.line_buffer_pointers, eeln);
}

void lineart_main_make_enclosed_shapes(LineartData *ld, LineartData *shadow_ld)
{
  double t_start;
  if (G.debug_value == 4000) {
    t_start = PIL_check_seconds_timer();
  }

  if (shadow_ld || ld->conf.shadow_use_silhouette) {
    lineart_shadow_cast(ld, false, shadow_ld ? true : false);
    if (ld->conf.shadow_use_silhouette) {
      lineart_shadow_register_silhouette(ld);
    }
  }

  if (G.debug_value == 4000) {
    double t_elapsed = PIL_check_seconds_timer() - t_start;
    printf("Line art shadow stage 2 cast and silhouette time: %f\n", t_elapsed);
  }

  if (!shadow_ld) {
    return;
  }

  ld->shadow_data_pool = &ld->render_data_pool;

  if (shadow_ld->pending_edges.array) {
    MEM_freeN(shadow_ld->pending_edges.array);
    shadow_ld->pending_edges.array = nullptr;
    shadow_ld->pending_edges.next = shadow_ld->pending_edges.max = 0;
  }

  LineartElementLinkNode *shadow_veln, *shadow_eeln;

  bool any_generated = lineart_shadow_cast_generate_edges(ld, false, &shadow_veln, &shadow_eeln);

  if (!any_generated) {
    return;
  }

  LineartVert *v = static_cast<LineartVert *>(shadow_veln->pointer);
  for (int i = 0; i < shadow_veln->element_count; i++) {
    mul_v4_m4v3_db(v[i].fbcoord, shadow_ld->conf.view_projection, v[i].gloc);
    if (shadow_ld->conf.cam_is_persp) {
      mul_v3db_db(v[i].fbcoord, (1 / v[i].fbcoord[3]));
    }
  }

  lineart_finalize_object_edge_array_reserve(&shadow_ld->pending_edges,
                                             shadow_eeln->element_count);

  LineartEdge *se = static_cast<LineartEdge *>(shadow_eeln->pointer);
  for (int i = 0; i < shadow_eeln->element_count; i++) {
    lineart_add_edge_to_array(&shadow_ld->pending_edges, &se[i]);
  }

  shadow_ld->scheduled_count = 0;

  lineart_main_clear_linked_edges(shadow_ld);
  lineart_main_link_lines(shadow_ld);
  lineart_main_occlusion_begin(shadow_ld);

  lineart_shadow_register_enclosed_shapes(ld, shadow_ld);

  if (G.debug_value == 4000) {
    double t_elapsed = PIL_check_seconds_timer() - t_start;
    printf("Line art shadow stage 2 total time: %f\n", t_elapsed);
  }
}
