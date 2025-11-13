/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edanimation
 */

#include <cmath>

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_idprop.hh"
#include "BKE_layer.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"
#include "BKE_unit.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "GPU_immediate.hh"
#include "GPU_matrix.hh"
#include "GPU_state.hh"

#include "UI_interface.hh"
#include "UI_interface_icons.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "ED_anim_api.hh"
#include "ED_keyframes_edit.hh"
#include "ED_markers.hh"
#include "ED_numinput.hh"
#include "ED_object.hh"
#include "ED_screen.hh"
#include "ED_select_utils.hh"
#include "ED_transform.hh"
#include "ED_util.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

/* -------------------------------------------------------------------- */
/** \name Marker API
 * \{ */

ListBase *ED_scene_markers_get(const bContext *C, Scene *scene)
{
  if (!scene) {
    return nullptr;
  }

  bAnimContext ac;
  if (!ANIM_animdata_get_context(C, &ac)) {
    return &scene->markers;
  }
  return ac.markers;
}

ListBase *ED_scene_markers_get_from_area(Scene *scene, ViewLayer *view_layer, const ScrArea *area)
{
  if (!scene) {
    return nullptr;
  }

  /* If the area is the dopesheet, AND it is configured to show scene markers (instead of
   * pose/action markers), directly go for the scene markers. */
  if (area->spacetype == SPACE_ACTION) {
    const SpaceAction *saction = static_cast<SpaceAction *>(area->spacedata.first);
    if (!(saction->flag & SACTION_POSEMARKERS_SHOW)) {
      return &scene->markers;
    }
  }

  bAction *active_action = ANIM_active_action_from_area(scene, view_layer, area);
  if (active_action) {
    return &active_action->markers;
  }
  return &scene->markers;
}

/* ............. */

ListBase *ED_context_get_markers(const bContext *C)
{
  return ED_scene_markers_get(C, CTX_data_scene(C));
}

ListBase *ED_sequencer_context_get_markers(const bContext *C)
{
  return ED_scene_markers_get(C, CTX_data_sequencer_scene(C));
}

/* --------------------------------- */

int ED_markers_post_apply_transform(
    ListBase *markers, Scene *scene, int mode, float value, char side)
{
  float cfra = float(scene->r.cfra);
  int changed_tot = 0;

  /* sanity check - no markers, or locked markers */
  if ((scene->toolsettings->lock_markers) || (markers == nullptr)) {
    return changed_tot;
  }

  /* affect selected markers - it's unlikely that we will want to affect all in this way? */
  LISTBASE_FOREACH (TimeMarker *, marker, markers) {
    if (marker->flag & SELECT) {
      switch (mode) {
        case blender::ed::transform::TFM_TIME_TRANSLATE:
        case blender::ed::transform::TFM_TIME_EXTEND: {
          /* apply delta if marker is on the right side of the current frame */
          if ((side == 'B') || (side == 'L' && marker->frame < cfra) ||
              (side == 'R' && marker->frame >= cfra))
          {
            marker->frame += round_fl_to_int(value);
            changed_tot++;
          }
          break;
        }
        case blender::ed::transform::TFM_TIME_SCALE: {
          /* rescale the distance between the marker and the current frame */
          marker->frame = cfra + round_fl_to_int(float(marker->frame - cfra) * value);
          changed_tot++;
          break;
        }
      }
    }
  }

  return changed_tot;
}

/* --------------------------------- */

TimeMarker *ED_markers_find_nearest_marker(ListBase *markers, const float frame)
{
  if (markers == nullptr || BLI_listbase_is_empty(markers)) {
    return nullptr;
  }

  /* Always initialize the first so it's guaranteed to return a marker
   * even if `frame` is NAN or the deltas are not finite. see: #136059. */
  TimeMarker *marker = static_cast<TimeMarker *>(markers->first);
  TimeMarker *nearest = marker;
  float min_dist = fabsf(float(marker->frame) - frame);
  for (marker = marker->next; marker; marker = marker->next) {
    const float dist = fabsf(float(marker->frame) - frame);
    if (dist < min_dist) {
      min_dist = dist;
      nearest = marker;
    }
  }

  return nearest;
}

int ED_markers_find_nearest_marker_time(ListBase *markers, float x)
{
  TimeMarker *nearest = ED_markers_find_nearest_marker(markers, x);
  return (nearest) ? (nearest->frame) : round_fl_to_int(x);
}

void ED_markers_get_minmax(ListBase *markers, short sel, float *r_first, float *r_last)
{
  float min, max;

  /* sanity check */
  // printf("markers = %p -  %p, %p\n", markers, markers->first, markers->last);
  if (ELEM(nullptr, markers, markers->first, markers->last)) {
    *r_first = 0.0f;
    *r_last = 0.0f;
    return;
  }

  min = FLT_MAX;
  max = -FLT_MAX;
  LISTBASE_FOREACH (TimeMarker *, marker, markers) {
    if (!sel || (marker->flag & SELECT)) {
      if (marker->frame < min) {
        min = float(marker->frame);
      }
      if (marker->frame > max) {
        max = float(marker->frame);
      }
    }
  }

  /* set the min/max values */
  *r_first = min;
  *r_last = max;
}

/**
 * Function used in operator polls, checks whether the markers region is currently drawn in the
 * editor in which the operator is called.
 */
static bool operator_markers_region_active(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  if (area == nullptr) {
    return false;
  }

  switch (area->spacetype) {
    case SPACE_ACTION: {
      SpaceAction *saction = static_cast<SpaceAction *>(area->spacedata.first);
      if (saction->flag & SACTION_SHOW_MARKERS) {
        return true;
      }
      break;
    }
    case SPACE_GRAPH: {
      SpaceGraph *sipo = static_cast<SpaceGraph *>(area->spacedata.first);
      if (sipo->mode != SIPO_MODE_DRIVERS && sipo->flag & SIPO_SHOW_MARKERS) {
        return true;
      }
      break;
    }
    case SPACE_NLA: {
      SpaceNla *snla = static_cast<SpaceNla *>(area->spacedata.first);
      if (snla->flag & SNLA_SHOW_MARKERS) {
        return true;
      }
      break;
    }
    case SPACE_SEQ: {
      SpaceSeq *seq = static_cast<SpaceSeq *>(area->spacedata.first);
      if (seq->flag & SEQ_SHOW_MARKERS) {
        return true;
      }
      break;
    }
  }
  return false;
}

static TimeMarker *region_position_is_over_marker(const View2D *v2d,
                                                  ListBase *markers,
                                                  float region_x)
{
  if (markers == nullptr || BLI_listbase_is_empty(markers)) {
    return nullptr;
  }

  float frame_at_position = UI_view2d_region_to_view_x(v2d, region_x);
  TimeMarker *nearest_marker = ED_markers_find_nearest_marker(markers, frame_at_position);
  float pixel_distance = UI_view2d_scale_get_x(v2d) *
                         fabsf(nearest_marker->frame - frame_at_position);

  if (pixel_distance <= UI_ICON_SIZE) {
    return nearest_marker;
  }
  return nullptr;
}

/* --------------------------------- */

/** Adds a marker to list of `cfra` elements. */
static void add_marker_to_cfra_elem(ListBase *lb, TimeMarker *marker, const bool only_selected)
{
  CfraElem *ce, *cen;

  /* should this one only be considered if it is selected? */
  if (only_selected && ((marker->flag & SELECT) == 0)) {
    return;
  }

  /* insertion sort - try to find a previous cfra elem */
  for (ce = static_cast<CfraElem *>(lb->first); ce; ce = ce->next) {
    if (ce->cfra == marker->frame) {
      /* do because of double keys */
      if (marker->flag & SELECT) {
        ce->sel = marker->flag;
      }
      return;
    }
    if (ce->cfra > marker->frame) {
      break;
    }
  }

  cen = MEM_callocN<CfraElem>("add_to_cfra_elem");
  if (ce) {
    BLI_insertlinkbefore(lb, ce, cen);
  }
  else {
    BLI_addtail(lb, cen);
  }

  cen->cfra = marker->frame;
  cen->sel = marker->flag;
}

void ED_markers_make_cfra_list(ListBase *markers, ListBase *lb, const bool only_selected)
{
  if (lb) {
    /* Clear the list first, since callers have no way of knowing
     * whether this terminated early otherwise. This may lead
     * to crashes if the user didn't clear the memory first.
     */
    lb->first = lb->last = nullptr;
  }
  else {
    return;
  }

  if (markers == nullptr) {
    return;
  }

  LISTBASE_FOREACH (TimeMarker *, marker, markers) {
    add_marker_to_cfra_elem(lb, marker, only_selected);
  }
}

void ED_markers_deselect_all(ListBase *markers, int action)
{
  if (action == SEL_TOGGLE) {
    action = ED_markers_get_first_selected(markers) ? SEL_DESELECT : SEL_SELECT;
  }

  LISTBASE_FOREACH (TimeMarker *, marker, markers) {
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

TimeMarker *ED_markers_get_first_selected(ListBase *markers)
{
  if (markers) {
    LISTBASE_FOREACH (TimeMarker *, marker, markers) {
      if (marker->flag & SELECT) {
        return marker;
      }
    }
  }

  return nullptr;
}

bool ED_markers_region_visible(const ScrArea *area, const ARegion *region)
{
  if (region->winy <= (UI_ANIM_MINY + UI_MARKER_MARGIN_Y)) {
    return false;
  }

  switch (area->spacetype) {
    case SPACE_ACTION: {
      const SpaceAction *saction = static_cast<SpaceAction *>(area->spacedata.first);
      if ((saction->flag & SACTION_SHOW_MARKERS) == 0) {
        return false;
      }
      break;
    }
    case SPACE_GRAPH: {
      const SpaceGraph *sgraph = static_cast<SpaceGraph *>(area->spacedata.first);
      if (sgraph->mode == SIPO_MODE_DRIVERS) {
        return false;
      }
      if ((sgraph->flag & SIPO_SHOW_MARKERS) == 0) {
        return false;
      }
      break;
    }
    case SPACE_NLA: {
      const SpaceNla *snla = static_cast<SpaceNla *>(area->spacedata.first);
      if ((snla->flag & SNLA_SHOW_MARKERS) == 0) {
        return false;
      }
      break;
    }
    case SPACE_SEQ: {
      const SpaceSeq *seq = static_cast<SpaceSeq *>(area->spacedata.first);
      if ((seq->flag & SEQ_SHOW_MARKERS) == 0) {
        return false;
      }
      break;
    }
    default:
      /* Unexpected editor type that shows no markers. */
      BLI_assert_unreachable();
      return false;
  }

  return true;
}

/* --------------------------------- */

void debug_markers_print_list(ListBase *markers)
{
  /* NOTE: do NOT make static or use `ifdef`'s as "unused code".
   * That's too much trouble when we need to use for quick debugging! */
  if (markers == nullptr) {
    printf("No markers list to print debug for\n");
    return;
  }

  printf("List of markers follows: -----\n");

  LISTBASE_FOREACH (TimeMarker *, marker, markers) {
    printf(
        "\t'%s' on %d at %p with %u\n", marker->name, marker->frame, (void *)marker, marker->flag);
  }

  printf("End of list ------------------\n");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Marker Drawing
 * \{ */

static void marker_color_get(const TimeMarker *marker, uchar *r_text_color, uchar *r_line_color)
{
  if (marker->flag & SELECT) {
    UI_GetThemeColor4ubv(TH_TIME_MARKER_LINE_SELECTED, r_text_color);
    UI_GetThemeColor4ubv(TH_TIME_MARKER_LINE_SELECTED, r_line_color);
  }
  else {
    UI_GetThemeColor4ubv(TH_TIME_MARKER_LINE, r_text_color);
    UI_GetThemeColor4ubv(TH_TIME_MARKER_LINE, r_line_color);
  }
}

static void draw_marker_name(const uchar *text_color,
                             const uiFontStyle *fstyle,
                             TimeMarker *marker,
                             float marker_x,
                             float xmax,
                             float text_y)
{
  const char *name = marker->name;
  uchar final_text_color[4];

  copy_v4_v4_uchar(final_text_color, text_color);

  if (marker->camera) {
    Object *camera = marker->camera;
    name = camera->id.name + 2;
    if (camera->visibility_flag & OB_HIDE_RENDER) {
      final_text_color[3] = 100;
    }
  }

  const int icon_half_width = UI_ICON_SIZE * 0.6;
  uiFontStyleDraw_Params fs_params{};
  fs_params.align = UI_STYLE_TEXT_LEFT;
  fs_params.word_wrap = 0;

  rcti rect{};
  rect.xmin = marker_x + icon_half_width;
  rect.xmax = xmax - icon_half_width;
  rect.ymin = text_y;
  rect.ymax = text_y;

  UI_fontstyle_draw(fstyle, &rect, name, strlen(name), final_text_color, &fs_params);
}

static void draw_marker_line(const uchar *color, int xpos, int ymin, int ymax)
{
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

  immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);
  immUniform2f("viewport_size", viewport_size[2] / UI_SCALE_FAC, viewport_size[3] / UI_SCALE_FAC);

  immUniformColor4ubv(color);
  immUniform1i("colors_len", 0); /* "simple" mode */
  immUniform1f("dash_width", 6.0f);
  immUniform1f("udash_factor", 0.5f);

  immBegin(GPU_PRIM_LINES, 2);
  immVertex2f(pos, xpos, ymin);
  immVertex2f(pos, xpos, ymax);
  immEnd();

  immUnbindProgram();
}

static int marker_get_icon_id(TimeMarker *marker, int flag)
{
  if (flag & DRAW_MARKERS_LOCAL) {
    return (marker->flag & SELECT) ? ICON_PMARKER_SEL : ICON_PMARKER;
  }
  if (marker->camera) {
    return (marker->flag & SELECT) ? ICON_OUTLINER_OB_CAMERA : ICON_CAMERA_DATA;
  }
  return (marker->flag & SELECT) ? ICON_MARKER_HLT : ICON_MARKER;
}

static void draw_marker(const uiFontStyle *fstyle,
                        TimeMarker *marker,
                        int xpos,
                        int xmax,
                        int flag,
                        int region_height,
                        bool is_elevated)
{
  uchar line_color[4], text_color[4];

  marker_color_get(marker, text_color, line_color);

  GPU_blend(GPU_BLEND_ALPHA);

  draw_marker_line(line_color, xpos, UI_SCALE_FAC * 28, region_height);

  int icon_id = marker_get_icon_id(marker, flag);

  uchar marker_color[4];
  if (marker->flag & SELECT) {
    UI_GetThemeColor4ubv(TH_TIME_MARKER_LINE_SELECTED, marker_color);
  }
  else {
    UI_GetThemeColor4ubv(TH_TIME_MARKER_LINE, marker_color);
  }

  UI_icon_draw_ex(xpos - (0.5f * UI_ICON_SIZE) - (0.5f * U.pixelsize),
                  UI_SCALE_FAC * 18,
                  icon_id,
                  UI_INV_SCALE_FAC,
                  1.0f,
                  0.0f,
                  marker_color,
                  false,
                  UI_NO_ICON_OVERLAY_TEXT);

  GPU_blend(GPU_BLEND_NONE);

  float name_y = UI_SCALE_FAC * 18;
  /* Give an offset to the marker that is elevated. */
  if (is_elevated) {
    name_y += UI_SCALE_FAC * 10;
  }
  draw_marker_name(text_color, fstyle, marker, xpos, xmax, name_y);
}

static void draw_markers_background(const rctf *rect)
{
  uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  uchar shade[4];
  UI_GetThemeColor4ubv(TH_TIME_SCRUB_BACKGROUND, shade);

  immUniformColor4ubv(shade);

  GPU_blend(GPU_BLEND_ALPHA);

  immRectf(pos, rect->xmin, rect->ymin, rect->xmax, rect->ymax);

  GPU_blend(GPU_BLEND_NONE);

  immUnbindProgram();
}

static bool marker_is_in_frame_range(TimeMarker *marker, const int frame_range[2])
{
  if (marker->frame < frame_range[0]) {
    return false;
  }
  if (marker->frame > frame_range[1]) {
    return false;
  }
  return true;
}

static void get_marker_region_rect(View2D *v2d, rctf *r_rect)
{
  r_rect->xmin = v2d->cur.xmin;
  r_rect->xmax = v2d->cur.xmax;
  r_rect->ymin = 0;
  r_rect->ymax = UI_MARKER_MARGIN_Y;
}

static void get_marker_clip_frame_range(View2D *v2d, float xscale, int r_range[2])
{
  float font_width_max = (10 * UI_SCALE_FAC) / xscale;
  r_range[0] = v2d->cur.xmin - sizeof(TimeMarker::name) * font_width_max;
  r_range[1] = v2d->cur.xmax + font_width_max;
}

static int markers_frame_sort(const void *a, const void *b)
{
  const TimeMarker *marker_a = static_cast<const TimeMarker *>(a);
  const TimeMarker *marker_b = static_cast<const TimeMarker *>(b);

  return marker_a->frame > marker_b->frame;
}

void ED_markers_draw(const bContext *C, int flag)
{
  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  ListBase *markers = is_sequencer ? ED_sequencer_context_get_markers(C) :
                                     ED_context_get_markers(C);
  if (markers == nullptr || BLI_listbase_is_empty(markers)) {
    return;
  }

  ARegion *region = CTX_wm_region(C);
  View2D *v2d = UI_view2d_fromcontext(C);
  int cfra = CTX_data_scene(C)->r.cfra;

  GPU_line_width(1.0f);

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

  /* Markers are not stored by frame order, so we need to sort it here. */
  ListBase sorted_markers;

  BLI_duplicatelist(&sorted_markers, markers);
  BLI_listbase_sort(&sorted_markers, markers_frame_sort);

  /**
   * Set a temporary bit in the marker's flag to indicate that it should be elevated.
   * This bit will be flipped back at the end of this function.
   */
  const int ELEVATED = 0x10;
  LISTBASE_FOREACH (TimeMarker *, marker, &sorted_markers) {
    const bool is_elevated = (marker->flag & SELECT) ||
                             (cfra >= marker->frame &&
                              (marker->next == nullptr || cfra < marker->next->frame));
    SET_FLAG_FROM_TEST(marker->flag, is_elevated, ELEVATED);
  }

  /* Separate loops in order to draw selected markers on top. */

  /**
   * Draw non-elevated markers first.
   * Note that unlike the elevated markers, these marker names will always be clipped by the
   * proceeding marker. This is done because otherwise, the text overlaps with the icon of the
   * marker itself.
   */
  LISTBASE_FOREACH (TimeMarker *, marker, &sorted_markers) {
    if ((marker->flag & ELEVATED) == 0 && marker_is_in_frame_range(marker, clip_frame_range)) {
      const int xmax = marker->next ? marker->next->frame : clip_frame_range[1] + 1;
      draw_marker(
          fstyle, marker, marker->frame * xscale, xmax * xscale, flag, region->winy, false);
    }
  }

  /* Now draw the elevated markers */
  for (TimeMarker *marker = static_cast<TimeMarker *>(sorted_markers.first); marker != nullptr;) {

    /* Skip this marker if it is elevated or out of the frame range. */
    if ((marker->flag & ELEVATED) == 0 || !marker_is_in_frame_range(marker, clip_frame_range)) {
      marker = marker->next;
      continue;
    }

    /* Find the next elevated marker. */
    /* We use the next marker to determine how wide our text should be */
    TimeMarker *next_marker = marker->next;
    while (next_marker != nullptr && (next_marker->flag & ELEVATED) == 0) {
      next_marker = next_marker->next;
    }

    const int xmax = next_marker ? next_marker->frame : clip_frame_range[1] + 1;
    draw_marker(fstyle, marker, marker->frame * xscale, xmax * xscale, flag, region->winy, true);

    marker = next_marker;
  }

  /* Reset the elevated flag. */
  LISTBASE_FOREACH (TimeMarker *, marker, &sorted_markers) {
    marker->flag &= ~ELEVATED;
  }

  BLI_freelistN(&sorted_markers);

  GPU_matrix_pop();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Marker Wrappers API
 *
 * These wrappers allow marker operators to function within the confines
 * of standard animation editors, such that they can coexist with the
 * primary operations of those editors.
 * \{ */

/* ------------------------ */

/* special poll() which checks if there are selected markers first */
static bool ed_markers_poll_selected_markers(bContext *C)
{
  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  ListBase *markers = is_sequencer ? ED_sequencer_context_get_markers(C) :
                                     ED_context_get_markers(C);

  if (!operator_markers_region_active(C)) {
    return false;
  }

  /* check if some marker is selected */
  if (ED_markers_get_first_selected(markers) == nullptr) {
    CTX_wm_operator_poll_msg_set(C, "No markers are selected");
    return false;
  }

  return true;
}

static bool ed_markers_poll_selected_no_locked_markers(bContext *C)
{
  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  ListBase *markers = is_sequencer ? ED_sequencer_context_get_markers(C) :
                                     ED_context_get_markers(C);
  ToolSettings *ts = CTX_data_tool_settings(C);

  if (!operator_markers_region_active(C)) {
    return false;
  }

  if (ts->lock_markers) {
    CTX_wm_operator_poll_msg_set(C, "Markers are locked");
    return false;
  }

  /* check if some marker is selected */
  if (ED_markers_get_first_selected(markers) == nullptr) {
    CTX_wm_operator_poll_msg_set(C, "No markers are selected");
    return false;
  }

  return true;
}

/* special poll() which checks if there are any markers at all first */
static bool ed_markers_poll_markers_exist(bContext *C)
{
  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  ListBase *markers = is_sequencer ? ED_sequencer_context_get_markers(C) :
                                     ED_context_get_markers(C);
  ToolSettings *ts = CTX_data_tool_settings(C);

  if (ts->lock_markers || !operator_markers_region_active(C)) {
    return false;
  }

  /* list of markers must exist, as well as some markers in it! */
  return (markers && markers->first);
}

static bool ed_markers_poll_markers_exist_visible(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  if (area == nullptr) {
    return false;
  }

  /* Minimum vertical size to select markers, while still scrubbing frames. */
  ARegion *region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
  if (region && region->winy < UI_MARKERS_MINY) {
    return false;
  }

  return ed_markers_poll_markers_exist(C);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Markers
 * \{ */

/* add TimeMarker at current frame */
static wmOperatorStatus ed_marker_add_exec(bContext *C, wmOperator * /*op*/)
{
  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  Scene *scene = is_sequencer ? CTX_data_sequencer_scene(C) : CTX_data_scene(C);
  ListBase *markers = is_sequencer ? ED_sequencer_context_get_markers(C) :
                                     ED_context_get_markers(C);

  if (markers == nullptr || scene == nullptr) {
    return OPERATOR_CANCELLED;
  }

  const int frame = scene->r.cfra;

  /* prefer not having 2 markers at the same place,
   * though the user can move them to overlap once added */
  LISTBASE_FOREACH (TimeMarker *, marker, markers) {
    if (marker->frame == frame) {
      return OPERATOR_CANCELLED;
    }
  }

  /* deselect all */
  LISTBASE_FOREACH (TimeMarker *, marker, markers) {
    marker->flag &= ~SELECT;
  }

  TimeMarker *marker = MEM_callocN<TimeMarker>("TimeMarker");
  marker->flag = SELECT;
  marker->frame = frame;
  SNPRINTF_UTF8(marker->name, "F_%02d", frame);
  BLI_addtail(markers, marker);

  WM_event_add_notifier(C, NC_SCENE | ND_MARKERS, nullptr);
  WM_event_add_notifier(C, NC_ANIMATION | ND_MARKERS, nullptr);

  return OPERATOR_FINISHED;
}

static void MARKER_OT_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Time Marker";
  ot->description = "Add a new time marker";
  ot->idname = "MARKER_OT_add";

  /* API callbacks. */
  ot->exec = ed_marker_add_exec;
  ot->poll = operator_markers_region_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Markers
 * \{ */

/* operator state vars used:
 *    frames: delta movement
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

struct MarkerMove {
  SpaceLink *slink;
  ListBase *markers;
  short event_type, event_val; /* store invoke-event, to verify */
  int *oldframe, evtx, firstx;
  NumInput num;
};

static bool ed_marker_move_use_time(MarkerMove *mm)
{
  if (((mm->slink->spacetype == SPACE_SEQ) &&
       !(reinterpret_cast<SpaceSeq *>(mm->slink)->flag & SEQ_DRAWFRAMES)) ||
      ((mm->slink->spacetype == SPACE_ACTION) &&
       (reinterpret_cast<SpaceAction *>(mm->slink)->flag & SACTION_DRAWTIME)) ||
      ((mm->slink->spacetype == SPACE_GRAPH) &&
       (reinterpret_cast<SpaceGraph *>(mm->slink)->flag & SIPO_DRAWTIME)) ||
      ((mm->slink->spacetype == SPACE_NLA) &&
       (reinterpret_cast<SpaceNla *>(mm->slink)->flag & SNLA_DRAWTIME)))
  {
    return true;
  }

  return false;
}

static void ed_marker_move_update_header(bContext *C, wmOperator *op)
{
  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  Scene *scene = is_sequencer ? CTX_data_sequencer_scene(C) : CTX_data_scene(C);
  MarkerMove *mm = static_cast<MarkerMove *>(op->customdata);
  TimeMarker *marker, *selmarker = nullptr;
  const int ofs = RNA_int_get(op->ptr, "frames");
  char str[UI_MAX_DRAW_STR];
  char str_ofs[NUM_STR_REP_LEN];
  int totmark;
  const bool use_time = ed_marker_move_use_time(mm);

  for (totmark = 0, marker = static_cast<TimeMarker *>(mm->markers->first); marker;
       marker = marker->next)
  {
    if (marker->flag & SELECT) {
      selmarker = marker;
      totmark++;
    }
  }

  if (hasNumInput(&mm->num)) {
    outputNumInput(&mm->num, str_ofs, scene->unit);
  }
  else if (use_time) {
    SNPRINTF_UTF8(str_ofs, "%.2f", FRA2TIME(ofs));
  }
  else {
    SNPRINTF_UTF8(str_ofs, "%d", ofs);
  }

  if (totmark == 1 && selmarker) {
    /* we print current marker value */
    if (use_time) {
      SNPRINTF_UTF8(str, IFACE_("Marker %.2f offset %s"), FRA2TIME(selmarker->frame), str_ofs);
    }
    else {
      SNPRINTF_UTF8(str, IFACE_("Marker %d offset %s"), selmarker->frame, str_ofs);
    }
  }
  else {
    SNPRINTF_UTF8(str, IFACE_("Marker offset %s"), str_ofs);
  }

  ED_area_status_text(CTX_wm_area(C), str);
}

/* copy selection to temp buffer */
/* return 0 if not OK */
static bool ed_marker_move_init(bContext *C, wmOperator *op)
{
  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  Scene *scene = is_sequencer ? CTX_data_sequencer_scene(C) : CTX_data_scene(C);
  ListBase *markers = is_sequencer ? ED_sequencer_context_get_markers(C) :
                                     ED_context_get_markers(C);
  MarkerMove *mm;
  TimeMarker *marker;
  int a, totmark;

  if (markers == nullptr) {
    return false;
  }

  for (totmark = 0, marker = static_cast<TimeMarker *>(markers->first); marker;
       marker = marker->next)
  {
    if (marker->flag & SELECT) {
      totmark++;
    }
  }

  if (totmark == 0) {
    return false;
  }

  op->customdata = mm = MEM_callocN<MarkerMove>("Markermove");
  mm->slink = CTX_wm_space_data(C);
  mm->markers = markers;
  mm->oldframe = MEM_calloc_arrayN<int>(totmark, "MarkerMove oldframe");

  initNumInput(&mm->num);
  mm->num.idx_max = 0; /* one axis */
  mm->num.val_flag[0] |= NUM_NO_FRACTION;
  mm->num.unit_sys = scene->unit.system;
  /* No time unit supporting frames currently... */
  mm->num.unit_type[0] = ed_marker_move_use_time(mm) ? B_UNIT_TIME : B_UNIT_NONE;

  for (a = 0, marker = static_cast<TimeMarker *>(markers->first); marker; marker = marker->next) {
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
  MarkerMove *mm = static_cast<MarkerMove *>(op->customdata);

  /* free data */
  MEM_freeN(mm->oldframe);
  MEM_freeN(mm);
  op->customdata = nullptr;

  /* clear custom header prints */
  ED_area_status_text(CTX_wm_area(C), nullptr);
}

static wmOperatorStatus ed_marker_move_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const bool tweak = RNA_struct_find_property(op->ptr, "tweak") &&
                     RNA_boolean_get(op->ptr, "tweak");

  if (tweak) {
    ARegion *region = CTX_wm_region(C);
    View2D *v2d = &region->v2d;
    const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
    ListBase *markers = is_sequencer ? ED_sequencer_context_get_markers(C) :
                                       ED_context_get_markers(C);
    if (!region_position_is_over_marker(v2d, markers, event->xy[0] - region->winrct.xmin)) {
      return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
    }
  }

  if (ed_marker_move_init(C, op)) {
    MarkerMove *mm = static_cast<MarkerMove *>(op->customdata);

    mm->evtx = event->xy[0];
    mm->firstx = event->xy[0];
    mm->event_type = event->type;
    mm->event_val = event->val;

    /* add temp handler */
    WM_event_add_modal_handler(C, op);

    /* Reset frames delta. */
    RNA_int_set(op->ptr, "frames", 0);

    ed_marker_move_update_header(C, op);

    return OPERATOR_RUNNING_MODAL;
  }

  return OPERATOR_CANCELLED;
}

/* NOTE: init has to be called successfully. */
static void ed_marker_move_apply(bContext *C, wmOperator *op)
{
  bScreen *screen = CTX_wm_screen(C);
  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  Scene *scene = is_sequencer ? CTX_data_sequencer_scene(C) : CTX_data_scene(C);
  Object *camera = scene->camera;
  MarkerMove *mm = static_cast<MarkerMove *>(op->customdata);
  TimeMarker *marker;
  int a, ofs;

  ofs = RNA_int_get(op->ptr, "frames");
  for (a = 0, marker = static_cast<TimeMarker *>(mm->markers->first); marker;
       marker = marker->next)
  {
    if (marker->flag & SELECT) {
      marker->frame = mm->oldframe[a] + ofs;
      a++;
    }
  }

  WM_event_add_notifier(C, NC_SCENE | ND_MARKERS, nullptr);
  WM_event_add_notifier(C, NC_ANIMATION | ND_MARKERS, nullptr);

  /* so we get view3d redraws */
  BKE_scene_camera_switch_update(scene);

  if (camera != scene->camera) {
    BKE_screen_view3d_scene_sync(screen, scene);
    WM_event_add_notifier(C, NC_SCENE | NA_EDITED, scene);
  }
}

/* only for modal */
static void ed_marker_move_cancel(bContext *C, wmOperator *op)
{
  RNA_int_set(op->ptr, "frames", 0);
  ed_marker_move_apply(C, op);
  ed_marker_move_exit(C, op);
}

static wmOperatorStatus ed_marker_move_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  Scene *scene = is_sequencer ? CTX_data_sequencer_scene(C) : CTX_data_scene(C);
  MarkerMove *mm = static_cast<MarkerMove *>(op->customdata);
  View2D *v2d = UI_view2d_fromcontext(C);
  const bool has_numinput = hasNumInput(&mm->num);
  const bool use_time = ed_marker_move_use_time(mm);

  /* Modal numinput active, try to handle numeric inputs first... */
  if (event->val == KM_PRESS && has_numinput && handleNumInput(C, &mm->num, event)) {
    float value = float(RNA_int_get(op->ptr, "frames"));

    applyNumInput(&mm->num, &value);
    if (use_time) {
      value = TIME2FRA(value);
    }

    RNA_int_set(op->ptr, "frames", int(value));
    ed_marker_move_apply(C, op);
    ed_marker_move_update_header(C, op);
  }
  else {
    bool handled = false;
    switch (event->type) {
      case EVT_ESCKEY:
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

      case EVT_RETKEY:
      case EVT_PADENTER:
      case LEFTMOUSE:
      case MIDDLEMOUSE:
        if (WM_event_is_modal_drag_exit(event, mm->event_type, mm->event_val)) {
          ed_marker_move_exit(C, op);
          WM_event_add_notifier(C, NC_SCENE | ND_MARKERS, nullptr);
          WM_event_add_notifier(C, NC_ANIMATION | ND_MARKERS, nullptr);
          return OPERATOR_FINISHED;
        }
        break;
      case MOUSEMOVE:
        if (!has_numinput) {
          float dx;

          dx = BLI_rctf_size_x(&v2d->cur) / BLI_rcti_size_x(&v2d->mask);

          if (event->xy[0] != mm->evtx) { /* XXX maybe init for first time */
            float fac;

            mm->evtx = event->xy[0];
            fac = (float(event->xy[0] - mm->firstx) * dx);

            apply_keyb_grid((event->modifier & KM_SHIFT) != 0,
                            (event->modifier & KM_CTRL) != 0,
                            &fac,
                            0.0,
                            scene->frames_per_second(),
                            0.1 * scene->frames_per_second(),
                            0);

            RNA_int_set(op->ptr, "frames", int(fac));
            ed_marker_move_apply(C, op);
            ed_marker_move_update_header(C, op);
          }
        }
        break;
      default: {
        break;
      }
    }

    if (!handled && event->val == KM_PRESS && handleNumInput(C, &mm->num, event)) {
      float value = float(RNA_int_get(op->ptr, "frames"));

      applyNumInput(&mm->num, &value);
      if (use_time) {
        value = TIME2FRA(value);
      }

      RNA_int_set(op->ptr, "frames", int(value));
      ed_marker_move_apply(C, op);
      ed_marker_move_update_header(C, op);
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus ed_marker_move_exec(bContext *C, wmOperator *op)
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

  /* API callbacks. */
  ot->exec = ed_marker_move_exec;
  ot->invoke = ed_marker_move_invoke;
  ot->modal = ed_marker_move_modal;
  ot->poll = ed_markers_poll_selected_no_locked_markers;
  ot->cancel = ed_marker_move_cancel;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_X;

  /* rna storage */
  RNA_def_int(ot->srna, "frames", 0, INT_MIN, INT_MAX, "Frames", "", INT_MIN, INT_MAX);
  PropertyRNA *prop = RNA_def_boolean(
      ot->srna, "tweak", false, "Tweak", "Operator has been activated using a click-drag event");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Duplicate Markers
 * \{ */

/* operator state vars used:
 *    frames: delta movement
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
  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  ListBase *markers = is_sequencer ? ED_sequencer_context_get_markers(C) :
                                     ED_context_get_markers(C);
  if (markers == nullptr) {
    return;
  }

  /* go through the list of markers, duplicate selected markers and add duplicated copies
   * to the beginning of the list (unselect original markers)
   */
  LISTBASE_FOREACH (TimeMarker *, marker, markers) {
    if (marker->flag & SELECT) {
      /* unselect selected marker */
      marker->flag &= ~SELECT;

      /* create and set up new marker */
      TimeMarker *newmarker = MEM_callocN<TimeMarker>("TimeMarker");
      newmarker->flag = SELECT;
      newmarker->frame = marker->frame;
      STRNCPY_UTF8(newmarker->name, marker->name);
      newmarker->camera = marker->camera;

      if (marker->prop != nullptr) {
        newmarker->prop = IDP_CopyProperty(marker->prop);
      }

      /* new marker is added to the beginning of list */
      /* FIXME: bad ordering! */
      BLI_addhead(markers, newmarker);
    }
  }
}

static wmOperatorStatus ed_marker_duplicate_exec(bContext *C, wmOperator *op)
{
  ed_marker_duplicate_apply(C);
  ed_marker_move_exec(C, op); /* Assumes frame delta set. */

  return OPERATOR_FINISHED;
}

static wmOperatorStatus ed_marker_duplicate_invoke(bContext *C,
                                                   wmOperator *op,
                                                   const wmEvent *event)
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

  /* API callbacks. */
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pick Select Markers
 *
 * Select/de-select time-marker at the current frame.
 * \{ */

static void deselect_markers(ListBase *markers)
{
  LISTBASE_FOREACH (TimeMarker *, marker, markers) {
    marker->flag &= ~SELECT;
  }
}

static void select_marker_camera_switch(
    bContext *C, bool camera, bool extend, ListBase *markers, int cfra)
{
  using namespace blender::ed;
  if (camera) {
    BLI_assert(CTX_data_mode_enum(C) == CTX_MODE_OBJECT);

    const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
    Scene *scene = is_sequencer ? CTX_data_sequencer_scene(C) : CTX_data_scene(C);

    ViewLayer *view_layer = CTX_data_view_layer(C);
    Base *base;
    int sel = 0;

    if (!extend) {
      BKE_view_layer_base_deselect_all(scene, view_layer);
    }

    LISTBASE_FOREACH (TimeMarker *, marker, markers) {
      if (marker->frame == cfra && marker->camera) {
        sel = (marker->flag & SELECT);
        break;
      }
    }

    BKE_view_layer_synced_ensure(scene, view_layer);

    LISTBASE_FOREACH (TimeMarker *, marker, markers) {
      if (marker->camera) {
        if (marker->frame == cfra) {
          base = BKE_view_layer_base_find(view_layer, marker->camera);
          if (base) {
            object::base_select(base, object::eObjectSelect_Mode(sel));
            if (!extend) {
              object::base_activate(C, base);
            }
          }
        }
      }
    }

    DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
  }
}

static wmOperatorStatus ed_marker_select(bContext *C,
                                         const int mval[2],
                                         bool extend,
                                         bool deselect_all,
                                         bool camera,
                                         bool wait_to_deselect_others)
{
  /* NOTE: keep this functionality in sync with #ACTION_OT_clickselect.
   * The logic here closely matches its internals.
   * From a user perspective the functions should also behave in much the same way.
   * The main difference with marker selection is support for selecting the camera.
   *
   * The variables (`sel_op` & `deselect_all`) have been included so marker
   * selection can use identical checks to dope-sheet selection. */

  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  ListBase *markers = is_sequencer ? ED_sequencer_context_get_markers(C) :
                                     ED_context_get_markers(C);

  const View2D *v2d = UI_view2d_fromcontext(C);
  wmOperatorStatus ret_val = OPERATOR_FINISHED;
  TimeMarker *nearest_marker = region_position_is_over_marker(v2d, markers, mval[0]);
  const float frame_at_mouse_position = UI_view2d_region_to_view_x(v2d, mval[0]);
  const int cfra = ED_markers_find_nearest_marker_time(markers, frame_at_mouse_position);
  const bool found = (nearest_marker != nullptr);
  const bool is_selected = (nearest_marker && nearest_marker->flag & SELECT);

  eSelectOp sel_op = (extend) ? (is_selected ? SEL_OP_SUB : SEL_OP_ADD) : SEL_OP_SET;

  if ((sel_op == SEL_OP_SET && found) || (!found && deselect_all)) {
    sel_op = SEL_OP_ADD;

    /* Rather than deselecting others, users may want to drag to box-select (drag from empty space)
     * or tweak-translate an already selected item. If these cases may apply, delay deselection. */
    if (wait_to_deselect_others && (!found || is_selected)) {
      ret_val = OPERATOR_RUNNING_MODAL;
    }
    else {
      /* Deselect all markers. */
      deselect_markers(markers);
    }
  }

  if (found) {
    TimeMarker *marker, *marker_cycle_selected = nullptr;
    TimeMarker *marker_found = nullptr;

    /* Support for selection cycling. */
    LISTBASE_FOREACH (TimeMarker *, marker, markers) {
      if (marker->frame == cfra) {
        if (marker->flag & SELECT) {
          marker_cycle_selected = static_cast<TimeMarker *>(marker->next ? marker->next :
                                                                           markers->first);
          break;
        }
      }
    }

    /* If extend is not set, then deselect markers. */
    LISTBASE_CIRCULAR_FORWARD_BEGIN (TimeMarker *, markers, marker, marker_cycle_selected) {
      /* This way a not-extend select will always give 1 selected marker. */
      if (marker->frame == cfra) {
        marker_found = marker;
        break;
      }
    }
    LISTBASE_CIRCULAR_FORWARD_END(TimeMarker *, markers, marker, marker_cycle_selected);

    if (marker_found) {
      if (sel_op == SEL_OP_SUB) {
        marker_found->flag &= ~SELECT;
      }
      else {
        marker_found->flag |= SELECT;
      }
    }
  }
  /* If extend is set (by holding Shift), then add the camera to the selection too. */
  if (found && camera) {
    select_marker_camera_switch(C, true, extend, markers, nearest_marker->frame);
  }

  WM_event_add_notifier(C, NC_SCENE | ND_MARKERS, nullptr);
  WM_event_add_notifier(C, NC_ANIMATION | ND_MARKERS, nullptr);

  /* Allowing tweaks, but needs OPERATOR_FINISHED, otherwise renaming fails, see #25987. */
  return ret_val;
}

static wmOperatorStatus ed_marker_select_exec(bContext *C, wmOperator *op)
{
  const bool extend = RNA_boolean_get(op->ptr, "extend");
  const bool wait_to_deselect_others = RNA_boolean_get(op->ptr, "wait_to_deselect_others");
  bool camera = false;
  camera = RNA_boolean_get(op->ptr, "camera");
  if (camera) {
    /* Supporting mode switching from this operator doesn't seem so useful.
     * So only allow setting the active camera in object-mode. */
    if (CTX_data_mode_enum(C) != CTX_MODE_OBJECT) {
      BKE_report(
          op->reports, RPT_WARNING, "Selecting the camera is only supported in object mode");
      camera = false;
    }
  }
  int mval[2];
  mval[0] = RNA_int_get(op->ptr, "mouse_x");
  mval[1] = RNA_int_get(op->ptr, "mouse_y");
  bool deselect_all = true;

  wmOperatorStatus ret_value = ed_marker_select(
      C, mval, extend, deselect_all, camera, wait_to_deselect_others);

  return ret_value | OPERATOR_PASS_THROUGH;
}

static void MARKER_OT_select(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Select Time Marker";
  ot->description = "Select time marker(s)";
  ot->idname = "MARKER_OT_select";

  /* API callbacks. */
  ot->poll = ed_markers_poll_markers_exist_visible;
  ot->exec = ed_marker_select_exec;
  ot->invoke = WM_generic_select_invoke;
  ot->modal = WM_generic_select_modal;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  WM_operator_properties_generic_select(ot);
  prop = RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the selection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna, "camera", false, "Camera", "Select the camera");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Box Select Markers
 * \{ */

/* operator state vars used: (added by default WM callbacks)
 * xmin, ymin
 * xmax, ymax
 *
 * customdata: the wmGesture pointer, with sub-window.
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

static wmOperatorStatus ed_marker_box_select_invoke(bContext *C,
                                                    wmOperator *op,
                                                    const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  View2D *v2d = &region->v2d;

  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  ListBase *markers = is_sequencer ? ED_sequencer_context_get_markers(C) :
                                     ED_context_get_markers(C);
  bool over_marker = region_position_is_over_marker(
                         v2d, markers, event->xy[0] - region->winrct.xmin) != nullptr;

  bool tweak = RNA_boolean_get(op->ptr, "tweak");
  if (tweak && over_marker) {
    return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
  }

  return WM_gesture_box_invoke(C, op, event);
}

static wmOperatorStatus ed_marker_box_select_exec(bContext *C, wmOperator *op)
{
  View2D *v2d = UI_view2d_fromcontext(C);
  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  ListBase *markers = is_sequencer ? ED_sequencer_context_get_markers(C) :
                                     ED_context_get_markers(C);
  rctf rect;

  WM_operator_properties_border_to_rctf(op, &rect);
  UI_view2d_region_to_view_rctf(v2d, &rect, &rect);

  if (markers == nullptr) {
    return OPERATOR_CANCELLED;
  }

  const eSelectOp sel_op = eSelectOp(RNA_enum_get(op->ptr, "mode"));
  const bool select = (sel_op != SEL_OP_SUB);
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    ED_markers_deselect_all(markers, SEL_DESELECT);
  }

  LISTBASE_FOREACH (TimeMarker *, marker, markers) {
    if (BLI_rctf_isect_x(&rect, marker->frame)) {
      SET_FLAG_FROM_TEST(marker->flag, select, SELECT);
    }
  }

  WM_event_add_notifier(C, NC_SCENE | ND_MARKERS, nullptr);
  WM_event_add_notifier(C, NC_ANIMATION | ND_MARKERS, nullptr);

  return OPERATOR_FINISHED;
}

static void MARKER_OT_select_box(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Marker Box Select";
  ot->description = "Select all time markers using box selection";
  ot->idname = "MARKER_OT_select_box";

  /* API callbacks. */
  ot->exec = ed_marker_box_select_exec;
  ot->invoke = ed_marker_box_select_invoke;
  ot->modal = WM_gesture_box_modal;
  ot->cancel = WM_gesture_box_cancel;

  ot->poll = ed_markers_poll_markers_exist;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_gesture_box(ot);
  WM_operator_properties_select_operation_simple(ot);

  PropertyRNA *prop = RNA_def_boolean(
      ot->srna, "tweak", false, "Tweak", "Operator has been activated using a click-drag event");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name (de)select all
 * \{ */

static wmOperatorStatus ed_marker_select_all_exec(bContext *C, wmOperator *op)
{
  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  ListBase *markers = is_sequencer ? ED_sequencer_context_get_markers(C) :
                                     ED_context_get_markers(C);
  if (markers == nullptr) {
    return OPERATOR_CANCELLED;
  }

  int action = RNA_enum_get(op->ptr, "action");
  ED_markers_deselect_all(markers, action);

  WM_event_add_notifier(C, NC_SCENE | ND_MARKERS, nullptr);
  WM_event_add_notifier(C, NC_ANIMATION | ND_MARKERS, nullptr);

  return OPERATOR_FINISHED;
}

static void MARKER_OT_select_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "(De)select all Markers";
  ot->description = "Change selection of all time markers";
  ot->idname = "MARKER_OT_select_all";

  /* API callbacks. */
  ot->exec = ed_marker_select_all_exec;
  ot->poll = ed_markers_poll_markers_exist;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* rna */
  WM_operator_properties_select_all(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Left/Right of Frame
 * \{ */

enum eMarkers_LeftRightSelect_Mode {
  MARKERS_LRSEL_LEFT = 0,
  MARKERS_LRSEL_RIGHT,
};

static const EnumPropertyItem prop_markers_select_leftright_modes[] = {
    {MARKERS_LRSEL_LEFT, "LEFT", 0, "Before Current Frame", ""},
    {MARKERS_LRSEL_RIGHT, "RIGHT", 0, "After Current Frame", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static void markers_select_leftright(bAnimContext *ac,
                                     const eMarkers_LeftRightSelect_Mode mode,
                                     const bool extend)
{
  ListBase *markers = ac->markers;
  Scene *scene = ac->scene;

  if (markers == nullptr) {
    return;
  }

  if (!extend) {
    deselect_markers(markers);
  }

  LISTBASE_FOREACH (TimeMarker *, marker, markers) {
    if ((mode == MARKERS_LRSEL_LEFT && marker->frame <= scene->r.cfra) ||
        (mode == MARKERS_LRSEL_RIGHT && marker->frame >= scene->r.cfra))
    {
      marker->flag |= SELECT;
    }
  }
}

static wmOperatorStatus ed_marker_select_leftright_exec(bContext *C, wmOperator *op)
{
  const eMarkers_LeftRightSelect_Mode mode = eMarkers_LeftRightSelect_Mode(
      RNA_enum_get(op->ptr, "mode"));
  const bool extend = RNA_boolean_get(op->ptr, "extend");

  bAnimContext ac;
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  markers_select_leftright(&ac, mode, extend);

  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, nullptr);

  return OPERATOR_FINISHED;
}

static void MARKER_OT_select_leftright(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Markers Before/After Current Frame";
  ot->description = "Select markers on and left/right of the current frame";
  ot->idname = "MARKER_OT_select_leftright";

  /* API callbacks. */
  ot->exec = ed_marker_select_leftright_exec;
  ot->poll = ed_markers_poll_markers_exist;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* rna storage */
  RNA_def_enum(
      ot->srna, "mode", prop_markers_select_leftright_modes, MARKERS_LRSEL_LEFT, "Mode", "");
  RNA_def_boolean(ot->srna, "extend", false, "Extend Select", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remove Marker
 *
 * Remove selected time-markers.
 * \{ */

static wmOperatorStatus ed_marker_delete_exec(bContext *C, wmOperator * /*op*/)

{
  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  ListBase *markers = is_sequencer ? ED_sequencer_context_get_markers(C) :
                                     ED_context_get_markers(C);
  TimeMarker *marker, *nmarker;
  bool changed = false;

  if (markers == nullptr) {
    return OPERATOR_CANCELLED;
  }

  for (marker = static_cast<TimeMarker *>(markers->first); marker; marker = nmarker) {
    nmarker = marker->next;
    if (marker->flag & SELECT) {
      if (marker->prop != nullptr) {
        IDP_FreePropertyContent(marker->prop);
        MEM_freeN(marker->prop);
      }
      BLI_freelinkN(markers, marker);
      changed = true;
    }
  }

  if (changed) {
    WM_event_add_notifier(C, NC_SCENE | ND_MARKERS, nullptr);
    WM_event_add_notifier(C, NC_ANIMATION | ND_MARKERS, nullptr);
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus ed_marker_delete_invoke(bContext *C,
                                                wmOperator *op,
                                                const wmEvent * /*event*/)
{
  if (RNA_boolean_get(op->ptr, "confirm")) {
    return WM_operator_confirm_ex(C,
                                  op,
                                  IFACE_("Delete selected markers?"),
                                  nullptr,
                                  IFACE_("Delete"),
                                  ALERT_ICON_NONE,
                                  false);
  }
  return ed_marker_delete_exec(C, op);
}

static void MARKER_OT_delete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Markers";
  ot->description = "Delete selected time marker(s)";
  ot->idname = "MARKER_OT_delete";

  /* API callbacks. */
  ot->invoke = ed_marker_delete_invoke;
  ot->exec = ed_marker_delete_exec;
  ot->poll = ed_markers_poll_selected_no_locked_markers;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  WM_operator_properties_confirm_or_exec(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Rename Marker
 *
 * Rename first selected time-marker.
 * \{ */

static wmOperatorStatus ed_marker_rename_exec(bContext *C, wmOperator *op)
{
  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  ListBase *markers = is_sequencer ? ED_sequencer_context_get_markers(C) :
                                     ED_context_get_markers(C);
  TimeMarker *marker = ED_markers_get_first_selected(markers);

  if (marker) {
    RNA_string_get(op->ptr, "name", marker->name);

    WM_event_add_notifier(C, NC_SCENE | ND_MARKERS, nullptr);
    WM_event_add_notifier(C, NC_ANIMATION | ND_MARKERS, nullptr);

    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

static wmOperatorStatus ed_marker_rename_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  ListBase *markers = is_sequencer ? ED_sequencer_context_get_markers(C) :
                                     ED_context_get_markers(C);
  /* must initialize the marker name first if there is a marker selected */
  TimeMarker *marker = ED_markers_get_first_selected(markers);
  if (marker) {
    RNA_string_set(op->ptr, "name", marker->name);
  }

  return WM_operator_props_popup_confirm_ex(
      C, op, event, IFACE_("Rename Selected Time Marker"), IFACE_("Rename"));
}

static void MARKER_OT_rename(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Rename Marker";
  ot->description = "Rename first selected time marker";
  ot->idname = "MARKER_OT_rename";

  /* API callbacks. */
  ot->invoke = ed_marker_rename_invoke;
  ot->exec = ed_marker_rename_exec;
  ot->poll = ed_markers_poll_selected_no_locked_markers;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_string(
      ot->srna, "name", "RenamedMarker", sizeof(TimeMarker::name), "Name", "New name for marker");
#if 0
  RNA_def_boolean(ot->srna,
                  "ensure_unique",
                  0,
                  "Ensure Unique",
                  "Ensure that new name is unique within collection of markers");
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Make Links to Scene
 * \{ */

static wmOperatorStatus ed_marker_make_links_scene_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  ListBase *markers = is_sequencer ? ED_sequencer_context_get_markers(C) :
                                     ED_context_get_markers(C);
  Scene *scene_to = static_cast<Scene *>(
      BLI_findlink(&bmain->scenes, RNA_enum_get(op->ptr, "scene")));
  TimeMarker *marker_new;

  if (scene_to == nullptr) {
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
  LISTBASE_FOREACH (TimeMarker *, marker, markers) {
    if (marker->flag & SELECT) {
      marker_new = static_cast<TimeMarker *>(MEM_dupallocN(marker));
      marker_new->prev = marker_new->next = nullptr;

      BLI_addtail(&scene_to->markers, marker_new);
    }
  }

  return OPERATOR_FINISHED;
}

static void MARKER_OT_make_links_scene(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Copy Markers to Scene";
  ot->description = "Copy selected markers to another scene";
  ot->idname = "MARKER_OT_make_links_scene";

  /* API callbacks. */
  ot->exec = ed_marker_make_links_scene_exec;
  ot->invoke = WM_menu_invoke;
  ot->poll = ed_markers_poll_selected_markers;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_enum(ot->srna, "scene", rna_enum_dummy_NULL_items, 0, "Scene", "");
  RNA_def_enum_funcs(prop, RNA_scene_itemf);
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  ot->prop = prop;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Camera Bind Marker
 * \{ */

static wmOperatorStatus ed_marker_camera_bind_exec(bContext *C, wmOperator *op)
{
  bScreen *screen = CTX_wm_screen(C);
  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  Scene *scene = is_sequencer ? CTX_data_sequencer_scene(C) : CTX_data_scene(C);
  if (!scene) {
    return OPERATOR_CANCELLED;
  }
  ListBase *markers = is_sequencer ? ED_sequencer_context_get_markers(C) :
                                     ED_context_get_markers(C);
  Object *ob = CTX_data_active_object(C);
  TimeMarker *marker;

  /* Don't do anything if we don't have a camera selected */
  if (ob == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Select a camera to bind to a marker on this frame");
    return OPERATOR_CANCELLED;
  }

  /* add new marker, unless we already have one on this frame, in which case, replace it */
  if (markers == nullptr) {
    return OPERATOR_CANCELLED;
  }

  marker = ED_markers_find_nearest_marker(markers, scene->r.cfra);
  if ((marker == nullptr) || (marker->frame != scene->r.cfra)) {
    marker = MEM_callocN<TimeMarker>("Camera TimeMarker");
    /* This marker's name is only displayed in the viewport statistics, animation editors use the
     * camera's name when bound to a marker. */
    SNPRINTF_UTF8(marker->name, "F_%02d", scene->r.cfra);
    marker->flag = SELECT;
    marker->frame = scene->r.cfra;
    BLI_addtail(markers, marker);

    /* deselect all others, so that the user can then move it without problems */
    LISTBASE_FOREACH (TimeMarker *, m, markers) {
      if (m != marker) {
        m->flag &= ~SELECT;
      }
    }
  }

  /* bind to the nominated camera (as set in operator props) */
  marker->camera = ob;

  /* camera may have changes */
  BKE_scene_camera_switch_update(scene);
  BKE_screen_view3d_scene_sync(screen, scene);
  DEG_relations_tag_update(CTX_data_main(C));

  WM_event_add_notifier(C, NC_SCENE | ND_MARKERS, nullptr);
  WM_event_add_notifier(C, NC_ANIMATION | ND_MARKERS, nullptr);
  WM_event_add_notifier(C, NC_SCENE | NA_EDITED, scene); /* so we get view3d redraws */

  return OPERATOR_FINISHED;
}

static void MARKER_OT_camera_bind(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Bind Camera to Markers";
  ot->description = "Bind the selected camera to a marker on the current frame";
  ot->idname = "MARKER_OT_camera_bind";

  /* API callbacks. */
  ot->exec = ed_marker_camera_bind_exec;
  ot->poll = operator_markers_region_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registration
 * \{ */

void ED_operatortypes_marker()
{
  WM_operatortype_append(MARKER_OT_add);
  WM_operatortype_append(MARKER_OT_move);
  WM_operatortype_append(MARKER_OT_duplicate);
  WM_operatortype_append(MARKER_OT_select);
  WM_operatortype_append(MARKER_OT_select_box);
  WM_operatortype_append(MARKER_OT_select_all);
  WM_operatortype_append(MARKER_OT_select_leftright);
  WM_operatortype_append(MARKER_OT_delete);
  WM_operatortype_append(MARKER_OT_rename);
  WM_operatortype_append(MARKER_OT_make_links_scene);
  WM_operatortype_append(MARKER_OT_camera_bind);
}

void ED_keymap_marker(wmKeyConfig *keyconf)
{
  WM_keymap_ensure(keyconf, "Markers", SPACE_EMPTY, RGN_TYPE_WINDOW);
}

/** \} */
