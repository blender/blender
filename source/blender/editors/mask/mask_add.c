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

#include "DNA_mask_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mask.h" /* own include */
#include "ED_screen.h"
#include "ED_select_utils.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "mask_intern.h" /* own include */

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
      for (int i = 0; i < spline->tot_point - 1; i++) {
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

static void finSelectedSplinePoint(MaskLayer *mask_layer,
                                   MaskSpline **spline,
                                   MaskSplinePoint **point,
                                   bool check_active)
{
  MaskSpline *cur_spline = mask_layer->splines.first;

  *spline = NULL;
  *point = NULL;

  if (check_active) {
    /* TODO, having an active point but no active spline is possible, why? */
    if (mask_layer->act_spline && mask_layer->act_point &&
        MASKPOINT_ISSEL_ANY(mask_layer->act_point)) {
      *spline = mask_layer->act_spline;
      *point = mask_layer->act_point;
      return;
    }
  }

  while (cur_spline) {
    for (int i = 0; i < cur_spline->tot_point; i++) {
      MaskSplinePoint *cur_point = &cur_spline->points[i];

      if (MASKPOINT_ISSEL_ANY(cur_point)) {
        if (!ELEM(*spline, NULL, cur_spline)) {
          *spline = NULL;
          *point = NULL;
          return;
        }
        if (*point) {
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
  MaskLayer *mask_layer;
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
                                      &mask_layer,
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
    BKE_mask_layer_shape_changed_add(mask_layer,
                                     BKE_mask_layer_shape_spline_to_index(mask_layer, spline) +
                                         point_index + 1,
                                     true,
                                     true);

    mask_layer->act_spline = spline;
    mask_layer->act_point = new_point;

    WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);

    return true;
  }

  return false;
}

static bool add_vertex_extrude(const bContext *C,
                               Mask *mask,
                               MaskLayer *mask_layer,
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
  bool do_prev; /* use prev point rather than next?? */

  if (!mask_layer) {
    return false;
  }
  finSelectedSplinePoint(mask_layer, &spline, &point, true);

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

  mask_layer->act_point = new_point;

  setup_vertex_point(mask, spline, new_point, co, 0.5f, ctime, ref_point, false);

  if (mask_layer->splines_shapes.first) {
    point_index = (((int)(new_point - spline->points) + 0) % spline->tot_point);
    BKE_mask_layer_shape_changed_add(mask_layer,
                                     BKE_mask_layer_shape_spline_to_index(mask_layer, spline) +
                                         point_index,
                                     true,
                                     true);
  }

  WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);

  return true;
}

static bool add_vertex_new(const bContext *C, Mask *mask, MaskLayer *mask_layer, const float co[2])
{
  Scene *scene = CTX_data_scene(C);
  const float ctime = CFRA;

  MaskSpline *spline;
  MaskSplinePoint *new_point = NULL, *ref_point = NULL;

  if (!mask_layer) {
    /* if there's no mask layer currently operationg on, create new one */
    mask_layer = BKE_mask_layer_new(mask, "");
    mask->masklay_act = mask->masklay_tot - 1;
  }

  ED_mask_select_toggle_all(mask, SEL_DESELECT);

  spline = BKE_mask_spline_add(mask_layer);

  mask_layer->act_spline = spline;
  new_point = spline->points;

  mask_layer->act_point = new_point;

  setup_vertex_point(mask, spline, new_point, co, 0.5f, ctime, ref_point, false);

  {
    int point_index = (((int)(new_point - spline->points) + 0) % spline->tot_point);
    BKE_mask_layer_shape_changed_add(mask_layer,
                                     BKE_mask_layer_shape_spline_to_index(mask_layer, spline) +
                                         point_index,
                                     true,
                                     true);
  }

  WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);

  return true;
}

/* Convert coordinate from normalized space to pixel one.
 * TODO(sergey): Make the function more generally available. */
static void mask_point_make_pixel_space(bContext *C,
                                        const float point_normalized[2],
                                        float point_pixel[2])
{
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);

  float scalex, scaley;
  ED_mask_pixelspace_factor(area, region, &scalex, &scaley);

  point_pixel[0] = point_normalized[0] * scalex;
  point_pixel[1] = point_normalized[1] * scaley;
}

static int add_vertex_handle_cyclic_at_point(bContext *C,
                                             Mask *mask,
                                             MaskSpline *spline,
                                             MaskSplinePoint *active_point,
                                             MaskSplinePoint *other_point,
                                             float co[2])
{
  const float tolerance_in_pixels_squared = 4 * 4;

  if (spline->flag & MASK_SPLINE_CYCLIC) {
    /* The spline is already cyclic, so there is no need to handle anything here.
     * Return PASS_THROUGH so that it's possible to add vertices close to the endpoints of the
     * cyclic spline. */
    return OPERATOR_PASS_THROUGH;
  }

  float co_pixel[2];
  mask_point_make_pixel_space(C, co, co_pixel);

  float point_pixel[2];
  mask_point_make_pixel_space(C, other_point->bezt.vec[1], point_pixel);

  const float dist_squared = len_squared_v2v2(co_pixel, point_pixel);
  if (dist_squared > tolerance_in_pixels_squared) {
    return OPERATOR_PASS_THROUGH;
  }

  spline->flag |= MASK_SPLINE_CYCLIC;

  /* TODO, update keyframes in time. */
  BKE_mask_calc_handle_point_auto(spline, active_point, false);
  BKE_mask_calc_handle_point_auto(spline, other_point, false);

  DEG_id_tag_update(&mask->id, ID_RECALC_GEOMETRY);

  WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);

  return OPERATOR_FINISHED;
}

static int add_vertex_handle_cyclic(
    bContext *C, Mask *mask, MaskSpline *spline, MaskSplinePoint *active_point, float co[2])
{
  MaskSplinePoint *first_point = &spline->points[0];
  MaskSplinePoint *last_point = &spline->points[spline->tot_point - 1];
  const bool is_first_point_active = (active_point == first_point);
  const bool is_last_point_active = (active_point == last_point);
  if (is_last_point_active) {
    return add_vertex_handle_cyclic_at_point(C, mask, spline, active_point, first_point, co);
  }
  if (is_first_point_active) {
    return add_vertex_handle_cyclic_at_point(C, mask, spline, active_point, last_point, co);
  }
  return OPERATOR_PASS_THROUGH;
}

static int add_vertex_exec(bContext *C, wmOperator *op)
{
  MaskViewLockState lock_state;
  ED_mask_view_lock_state_store(C, &lock_state);

  Mask *mask = CTX_data_edit_mask(C);
  if (mask == NULL) {
    /* if there's no active mask, create one */
    mask = ED_mask_new(C, NULL);
  }

  MaskLayer *mask_layer = BKE_mask_layer_active(mask);

  if (mask_layer && mask_layer->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
    mask_layer = NULL;
  }

  float co[2];
  RNA_float_get_array(op->ptr, "location", co);

  /* TODO, having an active point but no active spline is possible, why? */
  if (mask_layer && mask_layer->act_spline && mask_layer->act_point &&
      MASKPOINT_ISSEL_ANY(mask_layer->act_point)) {
    MaskSpline *spline = mask_layer->act_spline;
    MaskSplinePoint *active_point = mask_layer->act_point;
    const int cyclic_result = add_vertex_handle_cyclic(C, mask, spline, active_point, co);
    if (cyclic_result != OPERATOR_PASS_THROUGH) {
      return cyclic_result;
    }

    if (!add_vertex_subdivide(C, mask, co)) {
      if (!add_vertex_extrude(C, mask, mask_layer, co)) {
        return OPERATOR_CANCELLED;
      }
    }
  }
  else {
    if (!add_vertex_subdivide(C, mask, co)) {
      if (!add_vertex_new(C, mask, mask_layer, co)) {
        return OPERATOR_CANCELLED;
      }
    }
  }

  DEG_id_tag_update(&mask->id, ID_RECALC_GEOMETRY);

  ED_mask_view_lock_state_restore_no_jump(C, &lock_state);

  return OPERATOR_FINISHED;
}

static int add_vertex_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);

  float co[2];

  ED_mask_mouse_pos(area, region, event->mval, co);

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
  MaskLayer *mask_layer;
  MaskSpline *spline;
  MaskSplinePoint *point = NULL;
  const float threshold = 9;
  float co[2], u;

  RNA_float_get_array(op->ptr, "location", co);

  point = ED_mask_point_find_nearest(C, mask, co, threshold, NULL, NULL, NULL, NULL);
  if (point) {
    return OPERATOR_FINISHED;
  }

  if (ED_mask_find_nearest_diff_point(C,
                                      mask,
                                      co,
                                      threshold,
                                      true,
                                      NULL,
                                      true,
                                      true,
                                      &mask_layer,
                                      &spline,
                                      &point,
                                      &u,
                                      NULL)) {
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
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);

  float co[2];

  ED_mask_mouse_pos(area, region, event->mval, co);

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
  MaskViewLockState lock_state;
  ED_mask_view_lock_state_store(C, &lock_state);

  ScrArea *area = CTX_wm_area(C);
  int size = RNA_float_get(op->ptr, "size");

  int width, height;
  ED_mask_get_size(area, &width, &height);
  float scale = (float)size / max_ii(width, height);

  /* Get location in mask space. */
  float frame_size[2];
  frame_size[0] = width;
  frame_size[1] = height;
  float location[2];
  RNA_float_get_array(op->ptr, "location", location);
  location[0] /= width;
  location[1] /= height;
  BKE_mask_coord_from_frame(location, location, frame_size);

  /* Make it so new primitive is centered to mouse location. */
  location[0] -= 0.5f * scale;
  location[1] -= 0.5f * scale;

  bool added_mask = false;
  MaskLayer *mask_layer = ED_mask_layer_ensure(C, &added_mask);
  Mask *mask = CTX_data_edit_mask(C);

  ED_mask_select_toggle_all(mask, SEL_DESELECT);

  MaskSpline *new_spline = BKE_mask_spline_add(mask_layer);
  new_spline->flag = MASK_SPLINE_CYCLIC | SELECT;
  new_spline->points = MEM_recallocN(new_spline->points, sizeof(MaskSplinePoint) * num_points);

  mask_layer->act_spline = new_spline;
  mask_layer->act_point = NULL;

  const int spline_index = BKE_mask_layer_shape_spline_to_index(mask_layer, new_spline);

  for (int i = 0; i < num_points; i++) {
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

  ED_mask_view_lock_state_restore_no_jump(C, &lock_state);

  return OPERATOR_FINISHED;
}

static int primitive_add_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  ScrArea *area = CTX_wm_area(C);
  float cursor[2];
  int width, height;

  ED_mask_get_size(area, &width, &height);
  ED_mask_cursor_location_get(area, cursor);

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
  int num_points = sizeof(points) / (sizeof(float[2]));

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
  int num_points = sizeof(points) / (sizeof(float[2]));

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
