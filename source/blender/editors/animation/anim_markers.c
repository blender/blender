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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edanimation
 */

#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_fcurve.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_unit.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_view2d.h"
#include "UI_resources.h"

#include "ED_anim_api.h"
#include "ED_markers.h"
#include "ED_screen.h"
#include "ED_select_utils.h"
#include "ED_util.h"
#include "ED_numinput.h"
#include "ED_object.h"
#include "ED_transform.h"
#include "ED_types.h"

#include "DEG_depsgraph.h"

/* ************* Marker API **************** */

/* helper function for getting the list of markers to work on */
static ListBase *context_get_markers(Scene *scene, ScrArea *sa)
{
  /* local marker sets... */
  if (sa) {
    if (sa->spacetype == SPACE_ACTION) {
      SpaceAction *saction = (SpaceAction *)sa->spacedata.first;

      /* local markers can only be shown when there's only a single active action to grab them from
       * - flag only takes effect when there's an action, otherwise it can get too confusing?
       */
      if (ELEM(saction->mode, SACTCONT_ACTION, SACTCONT_SHAPEKEY) && (saction->action)) {
        if (saction->flag & SACTION_POSEMARKERS_SHOW) {
          return &saction->action->markers;
        }
      }
    }
  }

  /* default to using the scene's markers */
  return &scene->markers;
}

/* ............. */

/* public API for getting markers from context */
ListBase *ED_context_get_markers(const bContext *C)
{
  return context_get_markers(CTX_data_scene(C), CTX_wm_area(C));
}

/* public API for getting markers from "animation" context */
ListBase *ED_animcontext_get_markers(const bAnimContext *ac)
{
  if (ac) {
    return context_get_markers(ac->scene, ac->sa);
  }
  else {
    return NULL;
  }
}

/* --------------------------------- */

/**
 * Apply some transformation to markers after the fact
 *
 * \param markers: List of markers to affect - this may or may not be the scene markers list,
 * so don't assume anything.
 * \param scene: Current scene (for getting current frame)
 * \param mode: (TfmMode) transform mode that this transform is for
 * \param value: From the transform code, this is ``t->vec[0]``
 * (which is delta transform for grab/extend, and scale factor for scale)
 * \param side: (B/L/R) for 'extend' functionality, which side of current frame to use
 */
int ED_markers_post_apply_transform(
    ListBase *markers, Scene *scene, int mode, float value, char side)
{
  TimeMarker *marker;
  float cfra = (float)CFRA;
  int changed_tot = 0;

  /* sanity check - no markers, or locked markers */
  if ((scene->toolsettings->lock_markers) || (markers == NULL)) {
    return changed_tot;
  }

  /* affect selected markers - it's unlikely that we will want to affect all in this way? */
  for (marker = markers->first; marker; marker = marker->next) {
    if (marker->flag & SELECT) {
      switch (mode) {
        case TFM_TIME_TRANSLATE:
        case TFM_TIME_EXTEND: {
          /* apply delta if marker is on the right side of the current frame */
          if ((side == 'B') || (side == 'L' && marker->frame < cfra) ||
              (side == 'R' && marker->frame >= cfra)) {
            marker->frame += round_fl_to_int(value);
            changed_tot++;
          }
          break;
        }
        case TFM_TIME_SCALE: {
          /* rescale the distance between the marker and the current frame */
          marker->frame = cfra + round_fl_to_int((float)(marker->frame - cfra) * value);
          changed_tot++;
          break;
        }
      }
    }
  }

  return changed_tot;
}

/* --------------------------------- */

/* Get the marker that is closest to this point */
/* XXX for select, the min_dist should be small */
TimeMarker *ED_markers_find_nearest_marker(ListBase *markers, float x)
{
  TimeMarker *marker, *nearest = NULL;
  float dist, min_dist = 1000000;

  if (markers) {
    for (marker = markers->first; marker; marker = marker->next) {
      dist = fabsf((float)marker->frame - x);

      if (dist < min_dist) {
        min_dist = dist;
        nearest = marker;
      }
    }
  }

  return nearest;
}

/* Return the time of the marker that occurs on a frame closest to the given time */
int ED_markers_find_nearest_marker_time(ListBase *markers, float x)
{
  TimeMarker *nearest = ED_markers_find_nearest_marker(markers, x);
  return (nearest) ? (nearest->frame) : round_fl_to_int(x);
}

void ED_markers_get_minmax(ListBase *markers, short sel, float *first, float *last)
{
  TimeMarker *marker;
  float min, max;

  /* sanity check */
  // printf("markers = %p -  %p, %p\n", markers, markers->first, markers->last);
  if (ELEM(NULL, markers, markers->first, markers->last)) {
    *first = 0.0f;
    *last = 0.0f;
    return;
  }

  min = FLT_MAX;
  max = -FLT_MAX;
  for (marker = markers->first; marker; marker = marker->next) {
    if (!sel || (marker->flag & SELECT)) {
      if (marker->frame < min) {
        min = (float)marker->frame;
      }
      if (marker->frame > max) {
        max = (float)marker->frame;
      }
    }
  }

  /* set the min/max values */
  *first = min;
  *last = max;
}

static bool region_position_is_over_marker(View2D *v2d, ListBase *markers, float region_x)
{
  if (markers == NULL || BLI_listbase_is_empty(markers)) {
    return false;
  }

  float frame_at_position = UI_view2d_region_to_view_x(v2d, region_x);
  TimeMarker *nearest_marker = ED_markers_find_nearest_marker(markers, frame_at_position);
  float pixel_distance = UI_view2d_scale_get_x(v2d) *
                         fabsf(nearest_marker->frame - frame_at_position);

  return pixel_distance <= UI_DPI_ICON_SIZE;
}

/* --------------------------------- */

/* Adds a marker to list of cfra elems */
static void add_marker_to_cfra_elem(ListBase *lb, TimeMarker *marker, short only_sel)
{
  CfraElem *ce, *cen;

  /* should this one only be considered if it is selected? */
  if ((only_sel) && ((marker->flag & SELECT) == 0)) {
    return;
  }

  /* insertion sort - try to find a previous cfra elem */
  for (ce = lb->first; ce; ce = ce->next) {
    if (ce->cfra == marker->frame) {
      /* do because of double keys */
      if (marker->flag & SELECT) {
        ce->sel = marker->flag;
      }
      return;
    }
    else if (ce->cfra > marker->frame) {
      break;
    }
  }

  cen = MEM_callocN(sizeof(CfraElem), "add_to_cfra_elem");
  if (ce) {
    BLI_insertlinkbefore(lb, ce, cen);
  }
  else {
    BLI_addtail(lb, cen);
  }

  cen->cfra = marker->frame;
  cen->sel = marker->flag;
}

/* This function makes a list of all the markers. The only_sel
 * argument is used to specify whether only the selected markers
 * are added.
 */
void ED_markers_make_cfra_list(ListBase *markers, ListBase *lb, short only_sel)
{
  TimeMarker *marker;

  if (lb) {
    /* Clear the list first, since callers have no way of knowing
     * whether this terminated early otherwise. This may lead
     * to crashes if the user didn't clear the memory first.
     */
    lb->first = lb->last = NULL;
  }
  else {
    return;
  }

  if (markers == NULL) {
    return;
  }

  for (marker = markers->first; marker; marker = marker->next) {
    add_marker_to_cfra_elem(lb, marker, only_sel);
  }
}

void ED_markers_deselect_all(ListBase *markers, int action)
{
  if (action == SEL_TOGGLE) {
    action = ED_markers_get_first_selected(markers) ? SEL_DESELECT : SEL_SELECT;
  }

  for (TimeMarker *marker = markers->first; marker; marker = marker->next) {
    if (action == SEL_SELECT) {
      marker->flag |= SELECT;
    }
    else if (action == SEL_DESELECT) {
      marker->flag &= ~SELECT;
    }
    else if (action == SEL_INVERT) {
      marker->flag ^= SELECT;
    }
    else {
      BLI_assert(0);
    }
  }
}

/* --------------------------------- */

/* Get the first selected marker */
TimeMarker *ED_markers_get_first_selected(ListBase *markers)
{
  TimeMarker *marker;

  if (markers) {
    for (marker = markers->first; marker; marker = marker->next) {
      if (marker->flag & SELECT) {
        return marker;
      }
    }
  }

  return NULL;
}

/* --------------------------------- */

/* Print debugging prints of list of markers
 * BSI's: do NOT make static or put in if-defs as "unused code".
 * That's too much trouble when we need to use for quick debugging!
 */
void debug_markers_print_list(ListBase *markers)
{
  TimeMarker *marker;

  if (markers == NULL) {
    printf("No markers list to print debug for\n");
    return;
  }

  printf("List of markers follows: -----\n");

  for (marker = markers->first; marker; marker = marker->next) {
    printf(
        "\t'%s' on %d at %p with %u\n", marker->name, marker->frame, (void *)marker, marker->flag);
  }

  printf("End of list ------------------\n");
}

/* ************* Marker Drawing ************ */

static void marker_color_get(TimeMarker *marker, unsigned char *color)
{
  if (marker->flag & SELECT) {
    UI_GetThemeColor4ubv(TH_TEXT_HI, color);
  }
  else {
    UI_GetThemeColor4ubv(TH_TEXT, color);
  }
}

static void draw_marker_name(const uiFontStyle *fstyle,
                             TimeMarker *marker,
                             float marker_x,
                             float text_y)
{
  unsigned char text_color[4];
  marker_color_get(marker, text_color);

  const char *name = marker->name;

#ifdef DURIAN_CAMERA_SWITCH
  if (marker->camera) {
    Object *camera = marker->camera;
    name = camera->id.name + 2;
    if (camera->restrictflag & OB_RESTRICT_RENDER) {
      text_color[3] = 100;
    }
  }
#endif

  int name_x = marker_x + UI_DPI_ICON_SIZE * 0.6;
  UI_fontstyle_draw_simple(fstyle, name_x, text_y, name, text_color);
}

static void draw_marker_line(const float color[4], float x, float ymin, float ymax)
{
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);
  immUniform2f("viewport_size", viewport_size[2] / UI_DPI_FAC, viewport_size[3] / UI_DPI_FAC);

  immUniformColor4fv(color);
  immUniform1i("colors_len", 0); /* "simple" mode */
  immUniform1f("dash_width", 6.0f);
  immUniform1f("dash_factor", 0.5f);

  immBegin(GPU_PRIM_LINES, 2);
  immVertex2f(pos, x, ymin);
  immVertex2f(pos, x, ymax);
  immEnd();

  immUnbindProgram();
}

static int marker_get_icon_id(TimeMarker *marker, int flag)
{
  if (flag & DRAW_MARKERS_LOCAL) {
    return (marker->flag & ACTIVE) ? ICON_PMARKER_ACT :
                                     (marker->flag & SELECT) ? ICON_PMARKER_SEL : ICON_PMARKER;
  }
#ifdef DURIAN_CAMERA_SWITCH
  else if (marker->camera) {
    return (marker->flag & SELECT) ? ICON_OUTLINER_OB_CAMERA : ICON_CAMERA_DATA;
  }
#endif
  else {
    return (marker->flag & SELECT) ? ICON_MARKER_HLT : ICON_MARKER;
  }
}

static void draw_marker_line_if_necessary(TimeMarker *marker, int flag, int xpos, int height)
{
#ifdef DURIAN_CAMERA_SWITCH
  if ((marker->camera) || (flag & DRAW_MARKERS_LINES))
#else
  if (flag & DRAW_MARKERS_LINES)
#endif
  {
    float color[4];
    if (marker->flag & SELECT) {
      copy_v4_fl4(color, 1.0f, 1.0f, 1.0f, 0.38f);
    }
    else {
      copy_v4_fl4(color, 0.0f, 0.0f, 0.0f, 0.38f);
    }

    draw_marker_line(color, xpos, UI_DPI_FAC * 20, height);
  }
}

static void draw_marker(
    const uiFontStyle *fstyle, TimeMarker *marker, int xpos, int flag, int region_height)
{
  GPU_blend(true);
  GPU_blend_set_func_separate(
      GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

  draw_marker_line_if_necessary(marker, flag, xpos, region_height);

  int icon_id = marker_get_icon_id(marker, flag);
  UI_icon_draw(xpos - 0.55f * UI_DPI_ICON_SIZE, UI_DPI_FAC * 18, icon_id);

  GPU_blend(false);

  float name_y = UI_DPI_FAC * 18;
  if (marker->flag & SELECT) {
    name_y += UI_DPI_FAC * 10;
  }
  draw_marker_name(fstyle, marker, xpos, name_y);
}

static void draw_markers_background(rctf *rect)
{
  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  unsigned char shade[4];
  UI_GetThemeColor4ubv(TH_TIME_SCRUB_BACKGROUND, shade);

  immUniformColor4ubv(shade);

  GPU_blend(true);
  GPU_blend_set_func_separate(
      GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

  immRectf(pos, rect->xmin, rect->ymin, rect->xmax, rect->ymax);

  GPU_blend(false);

  immUnbindProgram();
}

static bool marker_is_in_frame_range(TimeMarker *marker, int frame_range[2])
{
  if (marker->frame < frame_range[0]) {
    return false;
  }
  if (marker->frame > frame_range[1]) {
    return false;
  }
  return true;
}

static void get_marker_region_rect(View2D *v2d, rctf *rect)
{
  rect->xmin = v2d->cur.xmin;
  rect->xmax = v2d->cur.xmax;
  rect->ymin = 0;
  rect->ymax = UI_MARKER_MARGIN_Y;
}

static void get_marker_clip_frame_range(View2D *v2d, float xscale, int r_range[2])
{
  float font_width_max = (10 * UI_DPI_FAC) / xscale;
  r_range[0] = v2d->cur.xmin - sizeof(((TimeMarker *)NULL)->name) * font_width_max;
  r_range[1] = v2d->cur.xmax + font_width_max;
}

/* Draw Scene-Markers in time window */
void ED_markers_draw(const bContext *C, int flag)
{
  ListBase *markers = ED_context_get_markers(C);
  if (markers == NULL || BLI_listbase_is_empty(markers)) {
    return;
  }

  ARegion *ar = CTX_wm_region(C);
  View2D *v2d = UI_view2d_fromcontext(C);

  rctf markers_region_rect;
  get_marker_region_rect(v2d, &markers_region_rect);

  draw_markers_background(&markers_region_rect);

  /* no time correction for framelen! space is drawn with old values */
  float xscale, dummy;
  UI_view2d_scale_get(v2d, &xscale, &dummy);
  GPU_matrix_push();
  GPU_matrix_scale_2f(1.0f / xscale, 1.0f);

  int clip_frame_range[2];
  get_marker_clip_frame_range(v2d, xscale, clip_frame_range);

  const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;

  for (TimeMarker *marker = markers->first; marker; marker = marker->next) {
    if ((marker->flag & SELECT) == 0) {
      if (marker_is_in_frame_range(marker, clip_frame_range)) {
        draw_marker(fstyle, marker, marker->frame * xscale, flag, ar->winy);
      }
    }
  }
  for (TimeMarker *marker = markers->first; marker; marker = marker->next) {
    if (marker->flag & SELECT) {
      if (marker_is_in_frame_range(marker, clip_frame_range)) {
        draw_marker(fstyle, marker, marker->frame * xscale, flag, ar->winy);
      }
    }
  }

  GPU_matrix_pop();
}

/* ************************ Marker Wrappers API ********************* */
/* These wrappers allow marker operators to function within the confines
 * of standard animation editors, such that they can coexist with the
 * primary operations of those editors.
 */

/* ------------------------ */

/* special poll() which checks if there are selected markers first */
static bool ed_markers_poll_selected_markers(bContext *C)
{
  ListBase *markers = ED_context_get_markers(C);

  /* first things first: markers can only exist in timeline views */
  if (ED_operator_animview_active(C) == 0) {
    return 0;
  }

  /* check if some marker is selected */
  return ED_markers_get_first_selected(markers) != NULL;
}

static bool ed_markers_poll_selected_no_locked_markers(bContext *C)
{
  ListBase *markers = ED_context_get_markers(C);
  ToolSettings *ts = CTX_data_tool_settings(C);

  if (ts->lock_markers) {
    return 0;
  }

  /* first things first: markers can only exist in timeline views */
  if (ED_operator_animview_active(C) == 0) {
    return 0;
  }

  /* check if some marker is selected */
  return ED_markers_get_first_selected(markers) != NULL;
}

/* special poll() which checks if there are any markers at all first */
static bool ed_markers_poll_markers_exist(bContext *C)
{
  ListBase *markers = ED_context_get_markers(C);
  ToolSettings *ts = CTX_data_tool_settings(C);

  if (ts->lock_markers) {
    return 0;
  }

  /* first things first: markers can only exist in timeline views */
  if (ED_operator_animview_active(C) == 0) {
    return 0;
  }

  /* list of markers must exist, as well as some markers in it! */
  return (markers && markers->first);
}

/* ************************** add markers *************************** */

/* add TimeMarker at current frame */
static int ed_marker_add_exec(bContext *C, wmOperator *UNUSED(op))
{
  ListBase *markers = ED_context_get_markers(C);
  TimeMarker *marker;
  int frame = CTX_data_scene(C)->r.cfra;

  if (markers == NULL) {
    return OPERATOR_CANCELLED;
  }

  /* prefer not having 2 markers at the same place,
   * though the user can move them to overlap once added */
  for (marker = markers->first; marker; marker = marker->next) {
    if (marker->frame == frame) {
      return OPERATOR_CANCELLED;
    }
  }

  /* deselect all */
  for (marker = markers->first; marker; marker = marker->next) {
    marker->flag &= ~SELECT;
  }

  marker = MEM_callocN(sizeof(TimeMarker), "TimeMarker");
  marker->flag = SELECT;
  marker->frame = frame;
  BLI_snprintf(marker->name, sizeof(marker->name), "F_%02d", frame);  // XXX - temp code only
  BLI_addtail(markers, marker);

  WM_event_add_notifier(C, NC_SCENE | ND_MARKERS, NULL);
  WM_event_add_notifier(C, NC_ANIMATION | ND_MARKERS, NULL);

  return OPERATOR_FINISHED;
}

static void MARKER_OT_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Time Marker";
  ot->description = "Add a new time marker";
  ot->idname = "MARKER_OT_add";

  /* api callbacks */
  ot->exec = ed_marker_add_exec;
  ot->poll = ED_operator_animview_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ************************** transform markers *************************** */

/* operator state vars used:
 *     frs: delta movement
 *
 * functions:
 *
 *     init()   check selection, add customdata with old values and some lookups
 *
 *     apply()  do the actual movement
 *
 *     exit()    cleanup, send notifier
 *
 *     cancel() to escape from modal
 *
 * callbacks:
 *
 *     exec()    calls init, apply, exit
 *
 *     invoke() calls init, adds modal handler
 *
 *     modal()    accept modal events while doing it, ends with apply and exit, or cancel
 */

typedef struct MarkerMove {
  SpaceLink *slink;
  ListBase *markers;
  int event_type; /* store invoke-event, to verify */
  int *oldframe, evtx, firstx;
  NumInput num;
} MarkerMove;

static bool ed_marker_move_use_time(MarkerMove *mm)
{
  if (((mm->slink->spacetype == SPACE_SEQ) && !(((SpaceSeq *)mm->slink)->flag & SEQ_DRAWFRAMES)) ||
      ((mm->slink->spacetype == SPACE_ACTION) &&
       (((SpaceAction *)mm->slink)->flag & SACTION_DRAWTIME)) ||
      ((mm->slink->spacetype == SPACE_GRAPH) &&
       !(((SpaceGraph *)mm->slink)->flag & SIPO_DRAWTIME)) ||
      ((mm->slink->spacetype == SPACE_NLA) && !(((SpaceNla *)mm->slink)->flag & SNLA_DRAWTIME))) {
    return true;
  }

  return false;
}

static void ed_marker_move_update_header(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  MarkerMove *mm = op->customdata;
  TimeMarker *marker, *selmarker = NULL;
  const int offs = RNA_int_get(op->ptr, "frames");
  char str[UI_MAX_DRAW_STR];
  char str_offs[NUM_STR_REP_LEN];
  int totmark;
  const bool use_time = ed_marker_move_use_time(mm);

  for (totmark = 0, marker = mm->markers->first; marker; marker = marker->next) {
    if (marker->flag & SELECT) {
      selmarker = marker;
      totmark++;
    }
  }

  if (hasNumInput(&mm->num)) {
    outputNumInput(&mm->num, str_offs, &scene->unit);
  }
  else if (use_time) {
    BLI_snprintf(str_offs, sizeof(str_offs), "%.2f", FRA2TIME(offs));
  }
  else {
    BLI_snprintf(str_offs, sizeof(str_offs), "%d", offs);
  }

  if (totmark == 1 && selmarker) {
    /* we print current marker value */
    if (use_time) {
      BLI_snprintf(
          str, sizeof(str), TIP_("Marker %.2f offset %s"), FRA2TIME(selmarker->frame), str_offs);
    }
    else {
      BLI_snprintf(str, sizeof(str), TIP_("Marker %d offset %s"), selmarker->frame, str_offs);
    }
  }
  else {
    BLI_snprintf(str, sizeof(str), TIP_("Marker offset %s"), str_offs);
  }

  ED_area_status_text(CTX_wm_area(C), str);
}

/* copy selection to temp buffer */
/* return 0 if not OK */
static bool ed_marker_move_init(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ListBase *markers = ED_context_get_markers(C);
  MarkerMove *mm;
  TimeMarker *marker;
  int a, totmark;

  if (markers == NULL) {
    return false;
  }

  for (totmark = 0, marker = markers->first; marker; marker = marker->next) {
    if (marker->flag & SELECT) {
      totmark++;
    }
  }

  if (totmark == 0) {
    return false;
  }

  op->customdata = mm = MEM_callocN(sizeof(MarkerMove), "Markermove");
  mm->slink = CTX_wm_space_data(C);
  mm->markers = markers;
  mm->oldframe = MEM_callocN(totmark * sizeof(int), "MarkerMove oldframe");

  initNumInput(&mm->num);
  mm->num.idx_max = 0; /* one axis */
  mm->num.val_flag[0] |= NUM_NO_FRACTION;
  mm->num.unit_sys = scene->unit.system;
  /* No time unit supporting frames currently... */
  mm->num.unit_type[0] = ed_marker_move_use_time(mm) ? B_UNIT_TIME : B_UNIT_NONE;

  for (a = 0, marker = markers->first; marker; marker = marker->next) {
    if (marker->flag & SELECT) {
      mm->oldframe[a] = marker->frame;
      a++;
    }
  }

  return true;
}

/* free stuff */
static void ed_marker_move_exit(bContext *C, wmOperator *op)
{
  MarkerMove *mm = op->customdata;

  /* free data */
  MEM_freeN(mm->oldframe);
  MEM_freeN(op->customdata);
  op->customdata = NULL;

  /* clear custom header prints */
  ED_area_status_text(CTX_wm_area(C), NULL);
}

static int ed_marker_move_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  bool tweak = RNA_boolean_get(op->ptr, "tweak");
  if (tweak) {
    ARegion *ar = CTX_wm_region(C);
    View2D *v2d = &ar->v2d;
    ListBase *markers = ED_context_get_markers(C);
    if (!region_position_is_over_marker(v2d, markers, event->x - ar->winrct.xmin)) {
      return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
    }
  }

  if (ed_marker_move_init(C, op)) {
    MarkerMove *mm = op->customdata;

    mm->evtx = event->x;
    mm->firstx = event->x;
    mm->event_type = event->type;

    /* add temp handler */
    WM_event_add_modal_handler(C, op);

    /* reset frs delta */
    RNA_int_set(op->ptr, "frames", 0);

    ed_marker_move_update_header(C, op);

    return OPERATOR_RUNNING_MODAL;
  }

  return OPERATOR_CANCELLED;
}

/* note, init has to be called successfully */
static void ed_marker_move_apply(bContext *C, wmOperator *op)
{
#ifdef DURIAN_CAMERA_SWITCH
  bScreen *sc = CTX_wm_screen(C);
  Scene *scene = CTX_data_scene(C);
  Object *camera = scene->camera;
#endif
  MarkerMove *mm = op->customdata;
  TimeMarker *marker;
  int a, offs;

  offs = RNA_int_get(op->ptr, "frames");
  for (a = 0, marker = mm->markers->first; marker; marker = marker->next) {
    if (marker->flag & SELECT) {
      marker->frame = mm->oldframe[a] + offs;
      a++;
    }
  }

  WM_event_add_notifier(C, NC_SCENE | ND_MARKERS, NULL);
  WM_event_add_notifier(C, NC_ANIMATION | ND_MARKERS, NULL);

#ifdef DURIAN_CAMERA_SWITCH
  /* so we get view3d redraws */
  BKE_scene_camera_switch_update(scene);

  if (camera != scene->camera) {
    BKE_screen_view3d_scene_sync(sc, scene);
    WM_event_add_notifier(C, NC_SCENE | NA_EDITED, scene);
  }
#endif
}

/* only for modal */
static void ed_marker_move_cancel(bContext *C, wmOperator *op)
{
  RNA_int_set(op->ptr, "frames", 0);
  ed_marker_move_apply(C, op);
  ed_marker_move_exit(C, op);
}

static int ed_marker_move_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Scene *scene = CTX_data_scene(C);
  MarkerMove *mm = op->customdata;
  View2D *v2d = UI_view2d_fromcontext(C);
  const bool has_numinput = hasNumInput(&mm->num);
  const bool use_time = ed_marker_move_use_time(mm);

  /* Modal numinput active, try to handle numeric inputs first... */
  if (event->val == KM_PRESS && has_numinput && handleNumInput(C, &mm->num, event)) {
    float value = (float)RNA_int_get(op->ptr, "frames");

    applyNumInput(&mm->num, &value);
    if (use_time) {
      value = TIME2FRA(value);
    }

    RNA_int_set(op->ptr, "frames", (int)value);
    ed_marker_move_apply(C, op);
    ed_marker_move_update_header(C, op);
  }
  else {
    bool handled = false;
    switch (event->type) {
      case ESCKEY:
        ed_marker_move_cancel(C, op);
        return OPERATOR_CANCELLED;
      case RIGHTMOUSE:
        /* press = user manually demands transform to be canceled */
        if (event->val == KM_PRESS) {
          ed_marker_move_cancel(C, op);
          return OPERATOR_CANCELLED;
        }
        /* else continue; <--- see if release event should be caught for tweak-end */
        ATTR_FALLTHROUGH;

      case RETKEY:
      case PADENTER:
      case LEFTMOUSE:
      case MIDDLEMOUSE:
        if (WM_event_is_modal_tweak_exit(event, mm->event_type)) {
          ed_marker_move_exit(C, op);
          WM_event_add_notifier(C, NC_SCENE | ND_MARKERS, NULL);
          WM_event_add_notifier(C, NC_ANIMATION | ND_MARKERS, NULL);
          return OPERATOR_FINISHED;
        }
        break;
      case MOUSEMOVE:
        if (!has_numinput) {
          float dx;

          dx = BLI_rctf_size_x(&v2d->cur) / BLI_rcti_size_x(&v2d->mask);

          if (event->x != mm->evtx) { /* XXX maybe init for first time */
            float fac;

            mm->evtx = event->x;
            fac = ((float)(event->x - mm->firstx) * dx);

            apply_keyb_grid(event->shift,
                            event->ctrl,
                            &fac,
                            0.0,
                            1.0,
                            0.1,
                            0 /*was: U.flag & USER_AUTOGRABGRID*/);

            RNA_int_set(op->ptr, "frames", (int)fac);
            ed_marker_move_apply(C, op);
            ed_marker_move_update_header(C, op);
          }
        }
        break;
    }

    if (!handled && event->val == KM_PRESS && handleNumInput(C, &mm->num, event)) {
      float value = (float)RNA_int_get(op->ptr, "frames");

      applyNumInput(&mm->num, &value);
      if (use_time) {
        value = TIME2FRA(value);
      }

      RNA_int_set(op->ptr, "frames", (int)value);
      ed_marker_move_apply(C, op);
      ed_marker_move_update_header(C, op);
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

static int ed_marker_move_exec(bContext *C, wmOperator *op)
{
  if (ed_marker_move_init(C, op)) {
    ed_marker_move_apply(C, op);
    ed_marker_move_exit(C, op);
    return OPERATOR_FINISHED;
  }
  return OPERATOR_PASS_THROUGH;
}

static void MARKER_OT_move(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Move Time Marker";
  ot->description = "Move selected time marker(s)";
  ot->idname = "MARKER_OT_move";

  /* api callbacks */
  ot->exec = ed_marker_move_exec;
  ot->invoke = ed_marker_move_invoke;
  ot->modal = ed_marker_move_modal;
  ot->poll = ed_markers_poll_selected_no_locked_markers;
  ot->cancel = ed_marker_move_cancel;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_XY;

  /* rna storage */
  RNA_def_int(ot->srna, "frames", 0, INT_MIN, INT_MAX, "Frames", "", INT_MIN, INT_MAX);
  PropertyRNA *prop = RNA_def_boolean(
      ot->srna, "tweak", 0, "Tweak", "Operator has been activated using a tweak event");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* ************************** duplicate markers *************************** */

/* operator state vars used:
 *     frs: delta movement
 *
 * functions:
 *
 *     apply()  do the actual duplicate
 *
 * callbacks:
 *
 *     exec()    calls apply, move_exec
 *
 *     invoke() calls apply, move_invoke
 *
 *     modal()    uses move_modal
 */

/* duplicate selected TimeMarkers */
static void ed_marker_duplicate_apply(bContext *C)
{
  ListBase *markers = ED_context_get_markers(C);
  TimeMarker *marker, *newmarker;

  if (markers == NULL) {
    return;
  }

  /* go through the list of markers, duplicate selected markers and add duplicated copies
   * to the beginning of the list (unselect original markers)
   */
  for (marker = markers->first; marker; marker = marker->next) {
    if (marker->flag & SELECT) {
      /* unselect selected marker */
      marker->flag &= ~SELECT;

      /* create and set up new marker */
      newmarker = MEM_callocN(sizeof(TimeMarker), "TimeMarker");
      newmarker->flag = SELECT;
      newmarker->frame = marker->frame;
      BLI_strncpy(newmarker->name, marker->name, sizeof(marker->name));

#ifdef DURIAN_CAMERA_SWITCH
      newmarker->camera = marker->camera;
#endif

      /* new marker is added to the beginning of list */
      // FIXME: bad ordering!
      BLI_addhead(markers, newmarker);
    }
  }
}

static int ed_marker_duplicate_exec(bContext *C, wmOperator *op)
{
  ed_marker_duplicate_apply(C);
  ed_marker_move_exec(C, op); /* assumes frs delta set */

  return OPERATOR_FINISHED;
}

static int ed_marker_duplicate_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ed_marker_duplicate_apply(C);
  return ed_marker_move_invoke(C, op, event);
}

static void MARKER_OT_duplicate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Duplicate Time Marker";
  ot->description = "Duplicate selected time marker(s)";
  ot->idname = "MARKER_OT_duplicate";

  /* api callbacks */
  ot->exec = ed_marker_duplicate_exec;
  ot->invoke = ed_marker_duplicate_invoke;
  ot->modal = ed_marker_move_modal;
  ot->poll = ed_markers_poll_selected_no_locked_markers;
  ot->cancel = ed_marker_move_cancel;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* rna storage */
  RNA_def_int(ot->srna, "frames", 0, INT_MIN, INT_MAX, "Frames", "", INT_MIN, INT_MAX);
}

/* ************************** selection ************************************/

static void deselect_markers(ListBase *markers)
{
  for (TimeMarker *marker = markers->first; marker; marker = marker->next) {
    marker->flag &= ~SELECT;
  }
}

/* select/deselect TimeMarker at current frame */
static void select_timeline_marker_frame(ListBase *markers, int frame, bool extend)
{
  TimeMarker *marker, *marker_first = NULL;

  /* support for selection cycling */
  for (marker = markers->first; marker; marker = marker->next) {
    if (marker->frame == frame) {
      if (marker->flag & SELECT) {
        marker_first = marker->next;
        break;
      }
    }
  }

  /* if extend is not set, then deselect markers */
  if (extend == false) {
    deselect_markers(markers);
  }

  LISTBASE_CIRCULAR_FORWARD_BEGIN (markers, marker, marker_first) {
    /* this way a not-extend select will always give 1 selected marker */
    if (marker->frame == frame) {
      marker->flag ^= SELECT;
      break;
    }
  }
  LISTBASE_CIRCULAR_FORWARD_END(markers, marker, marker_first);
}

static void select_marker_camera_switch(
    bContext *C, bool camera, bool extend, ListBase *markers, int cfra)
{
#ifdef DURIAN_CAMERA_SWITCH
  if (camera) {
    Scene *scene = CTX_data_scene(C);
    ViewLayer *view_layer = CTX_data_view_layer(C);
    Base *base;
    TimeMarker *marker;
    int sel = 0;

    if (!extend) {
      BKE_view_layer_base_deselect_all(view_layer);
    }

    for (marker = markers->first; marker; marker = marker->next) {
      if (marker->frame == cfra) {
        sel = (marker->flag & SELECT);
        break;
      }
    }

    for (marker = markers->first; marker; marker = marker->next) {
      if (marker->camera) {
        if (marker->frame == cfra) {
          base = BKE_view_layer_base_find(view_layer, marker->camera);
          if (base) {
            ED_object_base_select(base, sel);
            if (sel) {
              ED_object_base_activate(C, base);
            }
          }
        }
      }
    }

    DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
  }
#else
  (void)camera;
#endif
}

static int ed_marker_select(bContext *C, const wmEvent *event, bool extend, bool camera)
{
  ListBase *markers = ED_context_get_markers(C);
  ARegion *ar = CTX_wm_region(C);
  View2D *v2d = UI_view2d_fromcontext(C);

  float mouse_region_x = event->x - ar->winrct.xmin;
  if (region_position_is_over_marker(v2d, markers, mouse_region_x)) {
    float frame_at_mouse_position = UI_view2d_region_to_view_x(v2d, mouse_region_x);
    int cfra = ED_markers_find_nearest_marker_time(markers, frame_at_mouse_position);
    select_timeline_marker_frame(markers, cfra, extend);

    select_marker_camera_switch(C, camera, extend, markers, cfra);
  }
  else {
    deselect_markers(markers);
  }

  WM_event_add_notifier(C, NC_SCENE | ND_MARKERS, NULL);
  WM_event_add_notifier(C, NC_ANIMATION | ND_MARKERS, NULL);

  /* allowing tweaks, but needs OPERATOR_FINISHED, otherwise renaming fails... [#25987] */
  return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
}

static int ed_marker_select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const bool extend = RNA_boolean_get(op->ptr, "extend");
  bool camera = false;
#ifdef DURIAN_CAMERA_SWITCH
  camera = RNA_boolean_get(op->ptr, "camera");
#endif
  return ed_marker_select(C, event, extend, camera);
}

static void MARKER_OT_select(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Select Time Marker";
  ot->description = "Select time marker(s)";
  ot->idname = "MARKER_OT_select";

  /* api callbacks */
  ot->invoke = ed_marker_select_invoke;
  ot->poll = ed_markers_poll_markers_exist;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  prop = RNA_def_boolean(ot->srna, "extend", 0, "Extend", "Extend the selection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
#ifdef DURIAN_CAMERA_SWITCH
  prop = RNA_def_boolean(ot->srna, "camera", 0, "Camera", "Select the camera");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
#endif
}

/* *************************** box select markers **************** */

/* operator state vars used: (added by default WM callbacks)
 * xmin, ymin
 * xmax, ymax
 *
 * customdata: the wmGesture pointer, with subwindow
 *
 * callbacks:
 *
 *  exec()  has to be filled in by user
 *
 *  invoke() default WM function
 *          adds modal handler
 *
 *  modal() default WM function
 *          accept modal events while doing it, calls exec(), handles ESC and border drawing
 *
 *  poll()  has to be filled in by user for context
 */

static int ed_marker_box_select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *ar = CTX_wm_region(C);
  View2D *v2d = &ar->v2d;

  ListBase *markers = ED_context_get_markers(C);
  bool over_marker = region_position_is_over_marker(v2d, markers, event->x - ar->winrct.xmin);

  bool tweak = RNA_boolean_get(op->ptr, "tweak");
  if (tweak && over_marker) {
    return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
  }

  return WM_gesture_box_invoke(C, op, event);
}

static int ed_marker_box_select_exec(bContext *C, wmOperator *op)
{
  View2D *v2d = UI_view2d_fromcontext(C);
  ListBase *markers = ED_context_get_markers(C);
  rctf rect;

  WM_operator_properties_border_to_rctf(op, &rect);
  UI_view2d_region_to_view_rctf(v2d, &rect, &rect);

  if (markers == NULL) {
    return 0;
  }

  const eSelectOp sel_op = RNA_enum_get(op->ptr, "mode");
  const bool select = (sel_op != SEL_OP_SUB);
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    ED_markers_deselect_all(markers, SEL_DESELECT);
  }

  for (TimeMarker *marker = markers->first; marker; marker = marker->next) {
    if (BLI_rctf_isect_x(&rect, marker->frame)) {
      SET_FLAG_FROM_TEST(marker->flag, select, SELECT);
    }
  }

  WM_event_add_notifier(C, NC_SCENE | ND_MARKERS, NULL);
  WM_event_add_notifier(C, NC_ANIMATION | ND_MARKERS, NULL);

  return 1;
}

static void MARKER_OT_select_box(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Marker Box Select";
  ot->description = "Select all time markers using box selection";
  ot->idname = "MARKER_OT_select_box";

  /* api callbacks */
  ot->exec = ed_marker_box_select_exec;
  ot->invoke = ed_marker_box_select_invoke;
  ot->modal = WM_gesture_box_modal;
  ot->cancel = WM_gesture_box_cancel;

  ot->poll = ed_markers_poll_markers_exist;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_gesture_box(ot);
  WM_operator_properties_select_operation_simple(ot);

  PropertyRNA *prop = RNA_def_boolean(
      ot->srna, "tweak", 0, "Tweak", "Operator has been activated using a tweak event");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* *********************** (de)select all ***************** */

static int ed_marker_select_all_exec(bContext *C, wmOperator *op)
{
  ListBase *markers = ED_context_get_markers(C);
  if (markers == NULL) {
    return OPERATOR_CANCELLED;
  }

  int action = RNA_enum_get(op->ptr, "action");
  ED_markers_deselect_all(markers, action);

  WM_event_add_notifier(C, NC_SCENE | ND_MARKERS, NULL);
  WM_event_add_notifier(C, NC_ANIMATION | ND_MARKERS, NULL);

  return OPERATOR_FINISHED;
}

static void MARKER_OT_select_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "(De)select all Markers";
  ot->description = "Change selection of all time markers";
  ot->idname = "MARKER_OT_select_all";

  /* api callbacks */
  ot->exec = ed_marker_select_all_exec;
  ot->poll = ed_markers_poll_markers_exist;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* rna */
  WM_operator_properties_select_all(ot);
}

/* ***************** remove marker *********************** */

/* remove selected TimeMarkers */
static int ed_marker_delete_exec(bContext *C, wmOperator *UNUSED(op))
{
  ListBase *markers = ED_context_get_markers(C);
  TimeMarker *marker, *nmarker;
  bool changed = false;

  if (markers == NULL) {
    return OPERATOR_CANCELLED;
  }

  for (marker = markers->first; marker; marker = nmarker) {
    nmarker = marker->next;
    if (marker->flag & SELECT) {
      BLI_freelinkN(markers, marker);
      changed = true;
    }
  }

  if (changed) {
    WM_event_add_notifier(C, NC_SCENE | ND_MARKERS, NULL);
    WM_event_add_notifier(C, NC_ANIMATION | ND_MARKERS, NULL);
  }

  return OPERATOR_FINISHED;
}

static void MARKER_OT_delete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Markers";
  ot->description = "Delete selected time marker(s)";
  ot->idname = "MARKER_OT_delete";

  /* api callbacks */
  ot->invoke = WM_operator_confirm;
  ot->exec = ed_marker_delete_exec;
  ot->poll = ed_markers_poll_selected_no_locked_markers;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* **************** rename marker ***************** */

/* rename first selected TimeMarker */
static int ed_marker_rename_exec(bContext *C, wmOperator *op)
{
  TimeMarker *marker = ED_markers_get_first_selected(ED_context_get_markers(C));

  if (marker) {
    RNA_string_get(op->ptr, "name", marker->name);

    WM_event_add_notifier(C, NC_SCENE | ND_MARKERS, NULL);
    WM_event_add_notifier(C, NC_ANIMATION | ND_MARKERS, NULL);

    return OPERATOR_FINISHED;
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

static int ed_marker_rename_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  /* must initialize the marker name first if there is a marker selected */
  TimeMarker *marker = ED_markers_get_first_selected(ED_context_get_markers(C));
  if (marker) {
    RNA_string_set(op->ptr, "name", marker->name);
  }

  return WM_operator_props_popup_confirm(C, op, event);
}

static void MARKER_OT_rename(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Rename Marker";
  ot->description = "Rename first selected time marker";
  ot->idname = "MARKER_OT_rename";

  /* api callbacks */
  ot->invoke = ed_marker_rename_invoke;
  ot->exec = ed_marker_rename_exec;
  ot->poll = ed_markers_poll_selected_no_locked_markers;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_string(ot->srna,
                            "name",
                            "RenamedMarker",
                            sizeof(((TimeMarker *)NULL)->name),
                            "Name",
                            "New name for marker");
#if 0
  RNA_def_boolean(ot->srna,
                  "ensure_unique",
                  0,
                  "Ensure Unique",
                  "Ensure that new name is unique within collection of markers");
#endif
}

/* **************** make links to scene ***************** */

static int ed_marker_make_links_scene_exec(bContext *C, wmOperator *op)
{
  ListBase *markers = ED_context_get_markers(C);
  Scene *scene_to = BLI_findlink(&CTX_data_main(C)->scenes, RNA_enum_get(op->ptr, "scene"));
  TimeMarker *marker, *marker_new;

  if (scene_to == NULL) {
    BKE_report(op->reports, RPT_ERROR, "Scene not found");
    return OPERATOR_CANCELLED;
  }

  if (scene_to == CTX_data_scene(C)) {
    BKE_report(op->reports, RPT_ERROR, "Cannot re-link markers into the same scene");
    return OPERATOR_CANCELLED;
  }

  if (scene_to->toolsettings->lock_markers) {
    BKE_report(op->reports, RPT_ERROR, "Target scene has locked markers");
    return OPERATOR_CANCELLED;
  }

  /* copy markers */
  for (marker = markers->first; marker; marker = marker->next) {
    if (marker->flag & SELECT) {
      marker_new = MEM_dupallocN(marker);
      marker_new->prev = marker_new->next = NULL;

      BLI_addtail(&scene_to->markers, marker_new);
    }
  }

  return OPERATOR_FINISHED;
}

static void MARKER_OT_make_links_scene(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Make Links to Scene";
  ot->description = "Copy selected markers to another scene";
  ot->idname = "MARKER_OT_make_links_scene";

  /* api callbacks */
  ot->exec = ed_marker_make_links_scene_exec;
  ot->invoke = WM_menu_invoke;
  ot->poll = ed_markers_poll_selected_markers;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_enum(ot->srna, "scene", DummyRNA_NULL_items, 0, "Scene", "");
  RNA_def_enum_funcs(prop, RNA_scene_itemf);
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  ot->prop = prop;
}

#ifdef DURIAN_CAMERA_SWITCH
/* ******************************* camera bind marker ***************** */

static int ed_marker_camera_bind_exec(bContext *C, wmOperator *op)
{
  bScreen *sc = CTX_wm_screen(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  ListBase *markers = ED_context_get_markers(C);
  TimeMarker *marker;

  /* Don't do anything if we don't have a camera selected */
  if (ob == NULL) {
    BKE_report(op->reports, RPT_ERROR, "Select a camera to bind to a marker on this frame");
    return OPERATOR_CANCELLED;
  }

  /* add new marker, unless we already have one on this frame, in which case, replace it */
  if (markers == NULL) {
    return OPERATOR_CANCELLED;
  }

  marker = ED_markers_find_nearest_marker(markers, CFRA);
  if ((marker == NULL) || (marker->frame != CFRA)) {
    marker = MEM_callocN(sizeof(TimeMarker), "Camera TimeMarker");
    marker->flag = SELECT;
    marker->frame = CFRA;
    BLI_addtail(markers, marker);

    /* deselect all others, so that the user can then move it without problems */
    for (TimeMarker *m = markers->first; m; m = m->next) {
      if (m != marker) {
        m->flag &= ~SELECT;
      }
    }
  }

  /* bind to the nominated camera (as set in operator props) */
  marker->camera = ob;

  /* camera may have changes */
  BKE_scene_camera_switch_update(scene);
  BKE_screen_view3d_scene_sync(sc, scene);

  WM_event_add_notifier(C, NC_SCENE | ND_MARKERS, NULL);
  WM_event_add_notifier(C, NC_ANIMATION | ND_MARKERS, NULL);
  WM_event_add_notifier(C, NC_SCENE | NA_EDITED, scene); /* so we get view3d redraws */

  return OPERATOR_FINISHED;
}

static void MARKER_OT_camera_bind(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Bind Camera to Markers";
  ot->description = "Bind the selected camera to a marker on the current frame";
  ot->idname = "MARKER_OT_camera_bind";

  /* api callbacks */
  ot->exec = ed_marker_camera_bind_exec;
  ot->poll = ED_operator_animview_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
#endif

/* ************************** registration **********************************/

/* called in screen_ops.c:ED_operatortypes_screen() */
void ED_operatortypes_marker(void)
{
  WM_operatortype_append(MARKER_OT_add);
  WM_operatortype_append(MARKER_OT_move);
  WM_operatortype_append(MARKER_OT_duplicate);
  WM_operatortype_append(MARKER_OT_select);
  WM_operatortype_append(MARKER_OT_select_box);
  WM_operatortype_append(MARKER_OT_select_all);
  WM_operatortype_append(MARKER_OT_delete);
  WM_operatortype_append(MARKER_OT_rename);
  WM_operatortype_append(MARKER_OT_make_links_scene);
#ifdef DURIAN_CAMERA_SWITCH
  WM_operatortype_append(MARKER_OT_camera_bind);
#endif
}

/* called in screen_ops.c:ED_keymap_screen() */
void ED_keymap_marker(wmKeyConfig *keyconf)
{
  WM_keymap_ensure(keyconf, "Markers", 0, 0);
}
