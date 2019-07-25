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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edmask
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_mask.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_mask_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_select_utils.h"
#include "ED_mask.h" /* own include */
#include "ED_screen.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "mask_intern.h" /* own include */

bool ED_mask_find_nearest_diff_point(const bContext *C,
                                     struct Mask *mask_orig,
                                     const float normal_co[2],
                                     int threshold,
                                     bool feather,
                                     float tangent[2],
                                     const bool use_deform,
                                     const bool use_project,
                                     MaskLayer **masklay_r,
                                     MaskSpline **spline_r,
                                     MaskSplinePoint **point_r,
                                     float *u_r,
                                     float *score_r)
{
  ScrArea *sa = CTX_wm_area(C);
  ARegion *ar = CTX_wm_region(C);

  MaskLayer *point_masklay;
  MaskSpline *point_spline;
  MaskSplinePoint *point = NULL;
  float dist_best_sq = FLT_MAX, co[2];
  int width, height;
  float u = 0.0f;
  float scalex, scaley;

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Mask *mask_eval = (Mask *)DEG_get_evaluated_id(depsgraph, &mask_orig->id);

  ED_mask_get_size(sa, &width, &height);
  ED_mask_pixelspace_factor(sa, ar, &scalex, &scaley);

  co[0] = normal_co[0] * scalex;
  co[1] = normal_co[1] * scaley;

  for (MaskLayer *masklay_orig = mask_orig->masklayers.first,
                 *masklay_eval = mask_eval->masklayers.first;
       masklay_orig != NULL;
       masklay_orig = masklay_orig->next, masklay_eval = masklay_eval->next) {
    if (masklay_orig->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
      continue;
    }

    for (MaskSpline *spline_orig = masklay_orig->splines.first,
                    *spline_eval = masklay_eval->splines.first;
         spline_orig != NULL;
         spline_orig = spline_orig->next, spline_eval = spline_eval->next) {
      int i;
      MaskSplinePoint *cur_point_eval;

      for (i = 0, cur_point_eval = use_deform ? spline_eval->points_deform : spline_eval->points;
           i < spline_eval->tot_point;
           i++, cur_point_eval++) {
        unsigned int tot_diff_point;
        float *diff_points = BKE_mask_point_segment_diff(
            spline_eval, cur_point_eval, width, height, &tot_diff_point);

        if (diff_points) {
          int j, tot_point;
          unsigned int tot_feather_point;
          float *feather_points = NULL, *points;

          if (feather) {
            feather_points = BKE_mask_point_segment_feather_diff(
                spline_eval, cur_point_eval, width, height, &tot_feather_point);

            points = feather_points;
            tot_point = tot_feather_point;
          }
          else {
            points = diff_points;
            tot_point = tot_diff_point;
          }

          for (j = 0; j < tot_point - 1; j++) {
            float dist_sq, a[2], b[2];

            a[0] = points[2 * j] * scalex;
            a[1] = points[2 * j + 1] * scaley;

            b[0] = points[2 * j + 2] * scalex;
            b[1] = points[2 * j + 3] * scaley;

            dist_sq = dist_squared_to_line_segment_v2(co, a, b);

            if (dist_sq < dist_best_sq) {
              if (tangent) {
                sub_v2_v2v2(tangent, &diff_points[2 * j + 2], &diff_points[2 * j]);
              }

              point_masklay = masklay_orig;
              point_spline = spline_orig;
              point = use_deform ?
                          &spline_orig->points[(cur_point_eval - spline_eval->points_deform)] :
                          &spline_orig->points[(cur_point_eval - spline_eval->points)];
              dist_best_sq = dist_sq;
              u = (float)j / tot_point;
            }
          }

          if (feather_points != NULL) {
            MEM_freeN(feather_points);
          }
          MEM_freeN(diff_points);
        }
      }
    }
  }

  if (point && dist_best_sq < threshold) {
    if (masklay_r) {
      *masklay_r = point_masklay;
    }

    if (spline_r) {
      *spline_r = point_spline;
    }

    if (point_r) {
      *point_r = point;
    }

    if (u_r) {
      /* TODO(sergey): Projection fails in some weirdo cases.. */
      if (use_project) {
        u = BKE_mask_spline_project_co(point_spline, point, u, normal_co, MASK_PROJ_ANY);
      }

      *u_r = u;
    }

    if (score_r) {
      *score_r = dist_best_sq;
    }

    return true;
  }

  if (masklay_r) {
    *masklay_r = NULL;
  }

  if (spline_r) {
    *spline_r = NULL;
  }

  if (point_r) {
    *point_r = NULL;
  }

  return false;
}

/******************** add vertex *********************/

static void setup_vertex_point(Mask *mask,
                               MaskSpline *spline,
                               MaskSplinePoint *new_point,
                               const float point_co[2],
                               const float u,
                               const float ctime,
                               const MaskSplinePoint *reference_point,
                               const bool reference_adjacent)
{
  const MaskSplinePoint *reference_parent_point = NULL;
  BezTriple *bezt;
  float co[3];

  copy_v2_v2(co, point_co);
  co[2] = 0.0f;

  /* point coordinate */
  bezt = &new_point->bezt;

  bezt->h1 = bezt->h2 = HD_ALIGN;

  if (reference_point) {
    if (reference_point->bezt.h1 == HD_VECT && reference_point->bezt.h2 == HD_VECT) {
      /* If the reference point is sharp try using some smooth point as reference
       * for handles.
       */
      int point_index = reference_point - spline->points;
      int delta = new_point == spline->points ? 1 : -1;
      int i = 0;
      for (i = 0; i < spline->tot_point - 1; ++i) {
        MaskSplinePoint *current_point;

        point_index += delta;
        if (point_index == -1 || point_index >= spline->tot_point) {
          if (spline->flag & MASK_SPLINE_CYCLIC) {
            if (point_index == -1) {
              point_index = spline->tot_point - 1;
            }
            else if (point_index >= spline->tot_point) {
              point_index = 0;
            }
          }
          else {
            break;
          }
        }

        current_point = &spline->points[point_index];
        if (current_point->bezt.h1 != HD_VECT || current_point->bezt.h2 != HD_VECT) {
          bezt->h1 = bezt->h2 = MAX2(current_point->bezt.h2, current_point->bezt.h1);
          break;
        }
      }
    }
    else {
      bezt->h1 = bezt->h2 = MAX2(reference_point->bezt.h2, reference_point->bezt.h1);
    }

    reference_parent_point = reference_point;
  }
  else if (reference_adjacent) {
    if (spline->tot_point != 1) {
      MaskSplinePoint *prev_point, *next_point, *close_point;

      const int index = (int)(new_point - spline->points);
      if (spline->flag & MASK_SPLINE_CYCLIC) {
        prev_point = &spline->points[mod_i(index - 1, spline->tot_point)];
        next_point = &spline->points[mod_i(index + 1, spline->tot_point)];
      }
      else {
        prev_point = (index != 0) ? &spline->points[index - 1] : NULL;
        next_point = (index != spline->tot_point - 1) ? &spline->points[index + 1] : NULL;
      }

      if (prev_point && next_point) {
        close_point = (len_squared_v2v2(new_point->bezt.vec[1], prev_point->bezt.vec[1]) <
                       len_squared_v2v2(new_point->bezt.vec[1], next_point->bezt.vec[1])) ?
                          prev_point :
                          next_point;
      }
      else {
        close_point = prev_point ? prev_point : next_point;
      }

      /* handle type */
      char handle_type = 0;
      if (prev_point) {
        handle_type = prev_point->bezt.h2;
      }
      if (next_point) {
        handle_type = MAX2(next_point->bezt.h2, handle_type);
      }
      bezt->h1 = bezt->h2 = handle_type;

      /* parent */
      reference_parent_point = close_point;

      /* note, we may want to copy other attributes later, radius? pressure? color? */
    }
  }

  copy_v3_v3(bezt->vec[0], co);
  copy_v3_v3(bezt->vec[1], co);
  copy_v3_v3(bezt->vec[2], co);

  if (reference_parent_point) {
    new_point->parent = reference_parent_point->parent;

    if (new_point->parent.id) {
      float parent_matrix[3][3];
      BKE_mask_point_parent_matrix_get(new_point, ctime, parent_matrix);
      invert_m3(parent_matrix);
      mul_m3_v2(parent_matrix, new_point->bezt.vec[1]);
    }
  }
  else {
    BKE_mask_parent_init(&new_point->parent);
  }

  if (spline->tot_point != 1) {
    BKE_mask_calc_handle_adjacent_interp(spline, new_point, u);
  }

  /* select new point */
  MASKPOINT_SEL_ALL(new_point);
  ED_mask_select_flush_all(mask);
}

/* **** add extrude vertex **** */

static void finSelectedSplinePoint(MaskLayer *masklay,
                                   MaskSpline **spline,
                                   MaskSplinePoint **point,
                                   bool check_active)
{
  MaskSpline *cur_spline = masklay->splines.first;

  *spline = NULL;
  *point = NULL;

  if (check_active) {
    /* TODO, having an active point but no active spline is possible, why? */
    if (masklay->act_spline && masklay->act_point && MASKPOINT_ISSEL_ANY(masklay->act_point)) {
      *spline = masklay->act_spline;
      *point = masklay->act_point;
      return;
    }
  }

  while (cur_spline) {
    int i;

    for (i = 0; i < cur_spline->tot_point; i++) {
      MaskSplinePoint *cur_point = &cur_spline->points[i];

      if (MASKPOINT_ISSEL_ANY(cur_point)) {
        if (*spline != NULL && *spline != cur_spline) {
          *spline = NULL;
          *point = NULL;
          return;
        }
        else if (*point) {
          *point = NULL;
        }
        else {
          *spline = cur_spline;
          *point = cur_point;
        }
      }
    }

    cur_spline = cur_spline->next;
  }
}

/* **** add subdivide vertex **** */

static void mask_spline_add_point_at_index(MaskSpline *spline, int point_index)
{
  MaskSplinePoint *new_point_array;

  new_point_array = MEM_callocN(sizeof(MaskSplinePoint) * (spline->tot_point + 1),
                                "add mask vert points");

  memcpy(new_point_array, spline->points, sizeof(MaskSplinePoint) * (point_index + 1));
  memcpy(new_point_array + point_index + 2,
         spline->points + point_index + 1,
         sizeof(MaskSplinePoint) * (spline->tot_point - point_index - 1));

  MEM_freeN(spline->points);
  spline->points = new_point_array;
  spline->tot_point++;
}

static bool add_vertex_subdivide(const bContext *C, Mask *mask, const float co[2])
{
  MaskLayer *masklay;
  MaskSpline *spline;
  MaskSplinePoint *point = NULL;
  const float threshold = 9;
  float tangent[2];
  float u;

  if (ED_mask_find_nearest_diff_point(C,
                                      mask,
                                      co,
                                      threshold,
                                      false,
                                      tangent,
                                      true,
                                      true,
                                      &masklay,
                                      &spline,
                                      &point,
                                      &u,
                                      NULL)) {
    Scene *scene = CTX_data_scene(C);
    const float ctime = CFRA;

    MaskSplinePoint *new_point;
    int point_index = point - spline->points;

    ED_mask_select_toggle_all(mask, SEL_DESELECT);

    mask_spline_add_point_at_index(spline, point_index);

    new_point = &spline->points[point_index + 1];

    setup_vertex_point(mask, spline, new_point, co, u, ctime, NULL, true);

    /* TODO - we could pass the spline! */
    BKE_mask_layer_shape_changed_add(masklay,
                                     BKE_mask_layer_shape_spline_to_index(masklay, spline) +
                                         point_index + 1,
                                     true,
                                     true);

    masklay->act_spline = spline;
    masklay->act_point = new_point;

    WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);

    return true;
  }

  return false;
}

static bool add_vertex_extrude(const bContext *C,
                               Mask *mask,
                               MaskLayer *masklay,
                               const float co[2])
{
  Scene *scene = CTX_data_scene(C);
  const float ctime = CFRA;

  MaskSpline *spline;
  MaskSplinePoint *point;
  MaskSplinePoint *new_point = NULL, *ref_point = NULL;

  /* check on which side we want to add the point */
  int point_index;
  float tangent_point[2];
  float tangent_co[2];
  bool do_cyclic_correct = false;
  bool do_prev; /* use prev point rather then next?? */

  if (!masklay) {
    return false;
  }
  else {
    finSelectedSplinePoint(masklay, &spline, &point, true);
  }

  ED_mask_select_toggle_all(mask, SEL_DESELECT);

  point_index = (point - spline->points);

  MASKPOINT_DESEL_ALL(point);

  if ((spline->flag & MASK_SPLINE_CYCLIC) ||
      (point_index > 0 && point_index != spline->tot_point - 1)) {
    BKE_mask_calc_tangent_polyline(spline, point, tangent_point);
    sub_v2_v2v2(tangent_co, co, point->bezt.vec[1]);

    if (dot_v2v2(tangent_point, tangent_co) < 0.0f) {
      do_prev = true;
    }
    else {
      do_prev = false;
    }
  }
  else if (((spline->flag & MASK_SPLINE_CYCLIC) == 0) && (point_index == 0)) {
    do_prev = true;
  }
  else if (((spline->flag & MASK_SPLINE_CYCLIC) == 0) && (point_index == spline->tot_point - 1)) {
    do_prev = false;
  }
  else {
    do_prev = false; /* quiet warning */
    /* should never get here */
    BLI_assert(0);
  }

  /* use the point before the active one */
  if (do_prev) {
    point_index--;
    if (point_index < 0) {
      point_index += spline->tot_point; /* wrap index */
      if ((spline->flag & MASK_SPLINE_CYCLIC) == 0) {
        do_cyclic_correct = true;
        point_index = 0;
      }
    }
  }

  //      print_v2("", tangent_point);
  //      printf("%d\n", point_index);

  mask_spline_add_point_at_index(spline, point_index);

  if (do_cyclic_correct) {
    ref_point = &spline->points[point_index + 1];
    new_point = &spline->points[point_index];
    *ref_point = *new_point;
    memset(new_point, 0, sizeof(*new_point));
  }
  else {
    ref_point = &spline->points[point_index];
    new_point = &spline->points[point_index + 1];
  }

  masklay->act_point = new_point;

  setup_vertex_point(mask, spline, new_point, co, 0.5f, ctime, ref_point, false);

  if (masklay->splines_shapes.first) {
    point_index = (((int)(new_point - spline->points) + 0) % spline->tot_point);
    BKE_mask_layer_shape_changed_add(
        masklay, BKE_mask_layer_shape_spline_to_index(masklay, spline) + point_index, true, true);
  }

  WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);

  return true;
}

static bool add_vertex_new(const bContext *C, Mask *mask, MaskLayer *masklay, const float co[2])
{
  Scene *scene = CTX_data_scene(C);
  const float ctime = CFRA;

  MaskSpline *spline;
  MaskSplinePoint *new_point = NULL, *ref_point = NULL;

  if (!masklay) {
    /* if there's no masklay currently operationg on, create new one */
    masklay = BKE_mask_layer_new(mask, "");
    mask->masklay_act = mask->masklay_tot - 1;
  }

  ED_mask_select_toggle_all(mask, SEL_DESELECT);

  spline = BKE_mask_spline_add(masklay);

  masklay->act_spline = spline;
  new_point = spline->points;

  masklay->act_point = new_point;

  setup_vertex_point(mask, spline, new_point, co, 0.5f, ctime, ref_point, false);

  {
    int point_index = (((int)(new_point - spline->points) + 0) % spline->tot_point);
    BKE_mask_layer_shape_changed_add(
        masklay, BKE_mask_layer_shape_spline_to_index(masklay, spline) + point_index, true, true);
  }

  WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);

  return true;
}

static int add_vertex_exec(bContext *C, wmOperator *op)
{
  Mask *mask = CTX_data_edit_mask(C);
  MaskLayer *masklay;

  float co[2];

  if (mask == NULL) {
    /* if there's no active mask, create one */
    mask = ED_mask_new(C, NULL);
  }

  masklay = BKE_mask_layer_active(mask);

  if (masklay && masklay->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
    masklay = NULL;
  }

  RNA_float_get_array(op->ptr, "location", co);

  /* TODO, having an active point but no active spline is possible, why? */
  if (masklay && masklay->act_spline && masklay->act_point &&
      MASKPOINT_ISSEL_ANY(masklay->act_point)) {

    /* cheap trick - double click for cyclic */
    MaskSpline *spline = masklay->act_spline;
    MaskSplinePoint *point = masklay->act_point;

    const bool is_sta = (point == spline->points);
    const bool is_end = (point == &spline->points[spline->tot_point - 1]);

    /* then check are we overlapping the mouse */
    if ((is_sta || is_end) && equals_v2v2(co, point->bezt.vec[1])) {
      if (spline->flag & MASK_SPLINE_CYCLIC) {
        /* nothing to do */
        return OPERATOR_CANCELLED;
      }
      else {
        /* recalc the connecting point as well to make a nice even curve */
        MaskSplinePoint *point_other = is_end ? spline->points :
                                                &spline->points[spline->tot_point - 1];
        spline->flag |= MASK_SPLINE_CYCLIC;

        /* TODO, update keyframes in time */
        BKE_mask_calc_handle_point_auto(spline, point, false);
        BKE_mask_calc_handle_point_auto(spline, point_other, false);

        DEG_id_tag_update(&mask->id, ID_RECALC_GEOMETRY);

        WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);
        return OPERATOR_FINISHED;
      }
    }

    if (!add_vertex_subdivide(C, mask, co)) {
      if (!add_vertex_extrude(C, mask, masklay, co)) {
        return OPERATOR_CANCELLED;
      }
    }
  }
  else {
    if (!add_vertex_subdivide(C, mask, co)) {
      if (!add_vertex_new(C, mask, masklay, co)) {
        return OPERATOR_CANCELLED;
      }
    }
  }

  DEG_id_tag_update(&mask->id, ID_RECALC_GEOMETRY);

  return OPERATOR_FINISHED;
}

static int add_vertex_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ScrArea *sa = CTX_wm_area(C);
  ARegion *ar = CTX_wm_region(C);

  float co[2];

  ED_mask_mouse_pos(sa, ar, event->mval, co);

  RNA_float_set_array(op->ptr, "location", co);

  return add_vertex_exec(C, op);
}

void MASK_OT_add_vertex(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Vertex";
  ot->description = "Add vertex to active spline";
  ot->idname = "MASK_OT_add_vertex";

  /* api callbacks */
  ot->exec = add_vertex_exec;
  ot->invoke = add_vertex_invoke;
  ot->poll = ED_operator_mask;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_float_vector(ot->srna,
                       "location",
                       2,
                       NULL,
                       -FLT_MAX,
                       FLT_MAX,
                       "Location",
                       "Location of vertex in normalized space",
                       -1.0f,
                       1.0f);
}

/******************** add feather vertex *********************/

static int add_feather_vertex_exec(bContext *C, wmOperator *op)
{
  Mask *mask = CTX_data_edit_mask(C);
  MaskLayer *masklay;
  MaskSpline *spline;
  MaskSplinePoint *point = NULL;
  const float threshold = 9;
  float co[2], u;

  RNA_float_get_array(op->ptr, "location", co);

  point = ED_mask_point_find_nearest(C, mask, co, threshold, NULL, NULL, NULL, NULL);
  if (point) {
    return OPERATOR_FINISHED;
  }

  if (ED_mask_find_nearest_diff_point(
          C, mask, co, threshold, true, NULL, true, true, &masklay, &spline, &point, &u, NULL)) {
    float w = BKE_mask_point_weight(spline, point, u);
    float weight_scalar = BKE_mask_point_weight_scalar(spline, point, u);

    if (weight_scalar != 0.0f) {
      w = w / weight_scalar;
    }

    BKE_mask_point_add_uw(point, u, w);

    DEG_id_tag_update(&mask->id, ID_RECALC_GEOMETRY);

    WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);

    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

static int add_feather_vertex_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ScrArea *sa = CTX_wm_area(C);
  ARegion *ar = CTX_wm_region(C);

  float co[2];

  ED_mask_mouse_pos(sa, ar, event->mval, co);

  RNA_float_set_array(op->ptr, "location", co);

  return add_feather_vertex_exec(C, op);
}

void MASK_OT_add_feather_vertex(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Feather Vertex";
  ot->description = "Add vertex to feather";
  ot->idname = "MASK_OT_add_feather_vertex";

  /* api callbacks */
  ot->exec = add_feather_vertex_exec;
  ot->invoke = add_feather_vertex_invoke;
  ot->poll = ED_maskedit_mask_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_float_vector(ot->srna,
                       "location",
                       2,
                       NULL,
                       -FLT_MAX,
                       FLT_MAX,
                       "Location",
                       "Location of vertex in normalized space",
                       -1.0f,
                       1.0f);
}

/******************** common primitive functions *********************/

static int create_primitive_from_points(
    bContext *C, wmOperator *op, const float (*points)[2], int num_points, char handle_type)
{
  ScrArea *sa = CTX_wm_area(C);
  Mask *mask;
  MaskLayer *mask_layer;
  MaskSpline *new_spline;
  float scale, location[2], frame_size[2];
  int i, width, height;
  int size = RNA_float_get(op->ptr, "size");

  ED_mask_get_size(sa, &width, &height);
  scale = (float)size / max_ii(width, height);

  /* Get location in mask space. */
  frame_size[0] = width;
  frame_size[1] = height;
  RNA_float_get_array(op->ptr, "location", location);
  location[0] /= width;
  location[1] /= height;
  BKE_mask_coord_from_frame(location, location, frame_size);

  /* Make it so new primitive is centered to mouse location. */
  location[0] -= 0.5f * scale;
  location[1] -= 0.5f * scale;

  bool added_mask = false;
  mask_layer = ED_mask_layer_ensure(C, &added_mask);
  mask = CTX_data_edit_mask(C);

  ED_mask_select_toggle_all(mask, SEL_DESELECT);

  new_spline = BKE_mask_spline_add(mask_layer);
  new_spline->flag = MASK_SPLINE_CYCLIC | SELECT;
  new_spline->points = MEM_recallocN(new_spline->points, sizeof(MaskSplinePoint) * num_points);

  mask_layer->act_spline = new_spline;
  mask_layer->act_point = NULL;

  const int spline_index = BKE_mask_layer_shape_spline_to_index(mask_layer, new_spline);

  for (i = 0; i < num_points; i++) {
    new_spline->tot_point = i + 1;

    MaskSplinePoint *new_point = &new_spline->points[i];
    BKE_mask_parent_init(&new_point->parent);

    copy_v2_v2(new_point->bezt.vec[1], points[i]);
    mul_v2_fl(new_point->bezt.vec[1], scale);
    add_v2_v2(new_point->bezt.vec[1], location);

    new_point->bezt.h1 = handle_type;
    new_point->bezt.h2 = handle_type;
    BKE_mask_point_select_set(new_point, true);

    if (mask_layer->splines_shapes.first) {
      BKE_mask_layer_shape_changed_add(mask_layer, spline_index + i, true, true);
    }
  }

  if (added_mask) {
    WM_event_add_notifier(C, NC_MASK | NA_ADDED, NULL);
  }
  WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);

  DEG_id_tag_update(&mask->id, ID_RECALC_GEOMETRY);

  return OPERATOR_FINISHED;
}

static int primitive_add_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  ScrArea *sa = CTX_wm_area(C);
  float cursor[2];
  int width, height;

  ED_mask_get_size(sa, &width, &height);
  ED_mask_cursor_location_get(sa, cursor);

  cursor[0] *= width;
  cursor[1] *= height;

  RNA_float_set_array(op->ptr, "location", cursor);

  return op->type->exec(C, op);
}

static void define_primitive_add_properties(wmOperatorType *ot)
{
  RNA_def_float(
      ot->srna, "size", 100, -FLT_MAX, FLT_MAX, "Size", "Size of new circle", -FLT_MAX, FLT_MAX);
  RNA_def_float_vector(ot->srna,
                       "location",
                       2,
                       NULL,
                       -FLT_MAX,
                       FLT_MAX,
                       "Location",
                       "Location of new circle",
                       -FLT_MAX,
                       FLT_MAX);
}

/******************** primitive add circle *********************/

static int primitive_circle_add_exec(bContext *C, wmOperator *op)
{
  const float points[4][2] = {{0.0f, 0.5f}, {0.5f, 1.0f}, {1.0f, 0.5f}, {0.5f, 0.0f}};
  int num_points = sizeof(points) / (2 * sizeof(float));

  create_primitive_from_points(C, op, points, num_points, HD_AUTO);

  return OPERATOR_FINISHED;
}

void MASK_OT_primitive_circle_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Circle";
  ot->description = "Add new circle-shaped spline";
  ot->idname = "MASK_OT_primitive_circle_add";

  /* api callbacks */
  ot->exec = primitive_circle_add_exec;
  ot->invoke = primitive_add_invoke;
  ot->poll = ED_operator_mask;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  define_primitive_add_properties(ot);
}

/******************** primitive add suqare *********************/

static int primitive_square_add_exec(bContext *C, wmOperator *op)
{
  const float points[4][2] = {{0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}};
  int num_points = sizeof(points) / (2 * sizeof(float));

  create_primitive_from_points(C, op, points, num_points, HD_VECT);

  return OPERATOR_FINISHED;
}

void MASK_OT_primitive_square_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Square";
  ot->description = "Add new square-shaped spline";
  ot->idname = "MASK_OT_primitive_square_add";

  /* api callbacks */
  ot->exec = primitive_square_add_exec;
  ot->invoke = primitive_add_invoke;
  ot->poll = ED_operator_mask;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  define_primitive_add_properties(ot);
}
