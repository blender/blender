/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "BLI_lasso_2d.hh"
#include "BLI_rect.h"
#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math_geom.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"
#include "BLI_set.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "BKE_context.hh"
#include "BKE_report.hh"

#include "BLT_translation.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_define.hh"

#include "SEQ_channels.hh"
#include "SEQ_connect.hh"
#include "SEQ_effects.hh"
#include "SEQ_iterator.hh"
#include "SEQ_relations.hh"
#include "SEQ_retiming.hh"
#include "SEQ_select.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"
#include "SEQ_transform.hh"

/* For menu, popup, icons, etc. */

#include "ED_outliner.hh"
#include "ED_screen.hh"
#include "ED_select_utils.hh"
#include "ED_sequencer.hh"

#include "UI_view2d.hh"

/* Own include. */
#include "sequencer_intern.hh"

namespace blender::ed::vse {

/* -------------------------------------------------------------------- */
/** \name Selection Utilities
 * \{ */

class MouseCoords {
 public:
  int2 region;
  float2 view;

  MouseCoords(const View2D *v2d, int x, int y)
  {
    region[0] = x;
    region[1] = y;
    UI_view2d_region_to_view(v2d, x, y, &view[0], &view[1]);
  }
};

bool deselect_all_strips(const Scene *scene)
{
  Editing *ed = seq::editing_get(scene);
  bool changed = false;

  if (ed == nullptr) {
    return changed;
  }

  VectorSet<Strip *> strips = seq::query_all_strips(seq::active_seqbase_get(ed));
  for (Strip *strip : strips) {
    if (strip->flag & STRIP_ALLSEL) {
      strip->flag &= ~STRIP_ALLSEL;
      changed = true;
    }
  }
  return changed;
}

Strip *strip_under_mouse_get(const Scene *scene, const View2D *v2d, const int mval[2])
{
  float mouse_co[2];
  UI_view2d_region_to_view(v2d, mval[0], mval[1], &mouse_co[0], &mouse_co[1]);

  Vector<Strip *> visible = sequencer_visible_strips_get(scene, v2d);
  int mouse_channel = int(mouse_co[1]);
  for (Strip *strip : visible) {
    if (strip->channel != mouse_channel) {
      continue;
    }
    rctf body;
    strip_rectf(scene, strip, &body);
    if (BLI_rctf_isect_pt_v(&body, mouse_co)) {
      return strip;
    }
  }

  return nullptr;
}

VectorSet<Strip *> all_strips_from_context(bContext *C)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);
  ListBase *seqbase = seq::active_seqbase_get(ed);
  ListBase *channels = seq::channels_displayed_get(ed);

  const bool is_preview = sequencer_view_has_preview_poll(C);
  if (is_preview) {
    return seq::query_rendered_strips(scene, channels, seqbase, scene->r.cfra, 0);
  }

  return seq::query_all_strips(seqbase);
}

VectorSet<Strip *> selected_strips_from_context(bContext *C)
{
  const Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);
  ListBase *seqbase = seq::active_seqbase_get(ed);
  ListBase *channels = seq::channels_displayed_get(ed);

  const bool is_preview = sequencer_view_has_preview_poll(C);

  if (is_preview) {
    VectorSet strips = seq::query_rendered_strips(scene, channels, seqbase, scene->r.cfra, 0);
    strips.remove_if([&](Strip *strip) { return (strip->flag & SELECT) == 0; });
    return strips;
  }

  return seq::query_selected_strips(seqbase);
}

static void select_surrounding_handles(Scene *scene, Strip *test) /* XXX BRING BACK */
{
  Strip *neighbor;

  neighbor = find_neighboring_strip(scene, test, seq::SIDE_LEFT, -1);
  if (neighbor) {
    /* Only select neighbor handle if matching handle from test strip is also selected,
     * or if neighbor was not selected at all up till now.
     * Otherwise, we get odd mismatch when shift-alt-rmb selecting neighbor strips... */
    if (!(neighbor->flag & SELECT) || (test->flag & SEQ_LEFTSEL)) {
      neighbor->flag |= SEQ_RIGHTSEL;
    }
    neighbor->flag |= SELECT;
    recurs_sel_strip(neighbor);
  }
  neighbor = find_neighboring_strip(scene, test, seq::SIDE_RIGHT, -1);
  if (neighbor) {
    if (!(neighbor->flag & SELECT) || (test->flag & SEQ_RIGHTSEL)) { /* See comment above. */
      neighbor->flag |= SEQ_LEFTSEL;
    }
    neighbor->flag |= SELECT;
    recurs_sel_strip(neighbor);
  }
}

/* Used for mouse selection in #SEQUENCER_OT_select. */
static void select_active_side(
    const Scene *scene, ListBase *seqbase, int sel_side, int channel, int frame)
{

  LISTBASE_FOREACH (Strip *, strip, seqbase) {
    if (channel == strip->channel) {
      switch (sel_side) {
        case seq::SIDE_LEFT:
          if (frame > seq::time_left_handle_frame_get(scene, strip)) {
            strip->flag &= ~(SEQ_RIGHTSEL | SEQ_LEFTSEL);
            strip->flag |= SELECT;
          }
          break;
        case seq::SIDE_RIGHT:
          if (frame < seq::time_left_handle_frame_get(scene, strip)) {
            strip->flag &= ~(SEQ_RIGHTSEL | SEQ_LEFTSEL);
            strip->flag |= SELECT;
          }
          break;
        case seq::SIDE_BOTH:
          strip->flag &= ~(SEQ_RIGHTSEL | SEQ_LEFTSEL);
          strip->flag |= SELECT;
          break;
      }
    }
  }
}

/* Used for mouse selection in #SEQUENCER_OT_select_side. */
static void select_active_side_range(const Scene *scene,
                                     ListBase *seqbase,
                                     const int sel_side,
                                     const int frame_ranges[seq::MAX_CHANNELS],
                                     const int frame_ignore)
{
  LISTBASE_FOREACH (Strip *, strip, seqbase) {
    if (strip->channel < seq::MAX_CHANNELS) {
      const int frame = frame_ranges[strip->channel];
      if (frame == frame_ignore) {
        continue;
      }
      switch (sel_side) {
        case seq::SIDE_LEFT:
          if (frame > seq::time_left_handle_frame_get(scene, strip)) {
            strip->flag &= ~(SEQ_RIGHTSEL | SEQ_LEFTSEL);
            strip->flag |= SELECT;
          }
          break;
        case seq::SIDE_RIGHT:
          if (frame < seq::time_left_handle_frame_get(scene, strip)) {
            strip->flag &= ~(SEQ_RIGHTSEL | SEQ_LEFTSEL);
            strip->flag |= SELECT;
          }
          break;
        case seq::SIDE_BOTH:
          strip->flag &= ~(SEQ_RIGHTSEL | SEQ_LEFTSEL);
          strip->flag |= SELECT;
          break;
      }
    }
  }
}

/* Used alongside `select_linked_time` helper function in #SEQUENCER_OT_select. */
static void select_linked_time_strip(const Scene *scene,
                                     const Strip *strip_source,
                                     const eStripHandle handle_clicked)
{
  ListBase *seqbase = seq::active_seqbase_get(scene->ed);
  int source_left = seq::time_left_handle_frame_get(scene, strip_source);
  int source_right = seq::time_right_handle_frame_get(scene, strip_source);

  LISTBASE_FOREACH (Strip *, strip_dest, seqbase) {
    if (strip_source->channel != strip_dest->channel) {
      const bool left_match = (seq::time_left_handle_frame_get(scene, strip_dest) == source_left);
      const bool right_match = (seq::time_right_handle_frame_get(scene, strip_dest) ==
                                source_right);

      if (left_match && right_match) {
        /* Direct match, copy all selection settings. */
        strip_dest->flag &= ~STRIP_ALLSEL;
        strip_dest->flag |= strip_source->flag & (STRIP_ALLSEL);
        recurs_sel_strip(strip_dest);
      }
      else if (left_match && handle_clicked == STRIP_HANDLE_LEFT) {
        strip_dest->flag &= ~(SELECT | SEQ_LEFTSEL);
        strip_dest->flag |= strip_source->flag & (SELECT | SEQ_LEFTSEL);
        recurs_sel_strip(strip_dest);
      }
      else if (right_match && handle_clicked == STRIP_HANDLE_RIGHT) {
        strip_dest->flag &= ~(SELECT | SEQ_RIGHTSEL);
        strip_dest->flag |= strip_source->flag & (SELECT | SEQ_RIGHTSEL);
        recurs_sel_strip(strip_dest);
      }
    }
  }
}

#if 0 /* BRING BACK */
void select_surround_from_last(Scene *scene)
{
  Strip *strip = get_last_seq(scene);

  if (strip == nullptr) {
    return;
  }

  select_surrounding_handles(scene, strip);
}
#endif

void select_strip_single(Scene *scene, Strip *strip, bool deselect_all)
{
  if (deselect_all) {
    deselect_all_strips(scene);
  }

  seq::select_active_set(scene, strip);

  strip->flag |= SELECT;
  recurs_sel_strip(strip);
}

void strip_rectf(const Scene *scene, const Strip *strip, rctf *r_rect)
{
  r_rect->xmin = seq::time_left_handle_frame_get(scene, strip);
  r_rect->xmax = seq::time_right_handle_frame_get(scene, strip);
  r_rect->ymin = strip->channel + STRIP_OFSBOTTOM;
  r_rect->ymax = strip->channel + STRIP_OFSTOP;
}

Strip *find_neighboring_strip(const Scene *scene, const Strip *test, const int lr, int sel)
{
  /* sel: 0==unselected, 1==selected, -1==don't care. */
  Editing *ed = seq::editing_get(scene);

  if (ed == nullptr) {
    return nullptr;
  }

  if (sel > 0) {
    sel = SELECT;
  }
  LISTBASE_FOREACH (Strip *, strip, ed->current_strips()) {
    if ((strip != test) && (test->channel == strip->channel) &&
        ((sel == -1) || (sel && (strip->flag & SELECT)) ||
         (sel == 0 && (strip->flag & SELECT) == 0)))
    {
      switch (lr) {
        case seq::SIDE_LEFT:
          if (seq::time_left_handle_frame_get(scene, test) ==
              seq::time_right_handle_frame_get(scene, strip))
          {
            return strip;
          }
          break;
        case seq::SIDE_RIGHT:
          if (seq::time_right_handle_frame_get(scene, test) ==
              seq::time_left_handle_frame_get(scene, strip))
          {
            return strip;
          }
          break;
      }
    }
  }
  return nullptr;
}

#if 0
static void select_neighbor_from_last(Scene *scene, int lr)
{
  Strip *strip = seq::SEQ_select_active_get(scene);
  Strip *neighbor;
  bool changed = false;
  if (strip) {
    neighbor = find_neighboring_strip(scene, strip, lr, -1);
    if (neighbor) {
      switch (lr) {
        case seq::SIDE_LEFT:
          neighbor->flag |= SELECT;
          recurs_sel_strip(neighbor);
          neighbor->flag |= SEQ_RIGHTSEL;
          strip->flag |= SEQ_LEFTSEL;
          break;
        case seq::SIDE_RIGHT:
          neighbor->flag |= SELECT;
          recurs_sel_strip(neighbor);
          neighbor->flag |= SEQ_LEFTSEL;
          strip->flag |= SEQ_RIGHTSEL;
          break;
      }
      strip->flag |= SELECT;
      changed = true;
    }
  }
  if (changed) {
    /* Pass. */
  }
}
#endif

void recurs_sel_strip(Strip *strip_meta)
{
  Strip *strip;
  strip = static_cast<Strip *>(strip_meta->seqbase.first);

  while (strip) {

    if (strip_meta->flag & (SEQ_LEFTSEL + SEQ_RIGHTSEL)) {
      strip->flag &= ~STRIP_ALLSEL;
    }
    else if (strip_meta->flag & SELECT) {
      strip->flag |= SELECT;
    }
    else {
      strip->flag &= ~STRIP_ALLSEL;
    }

    if (strip->seqbase.first) {
      recurs_sel_strip(strip);
    }

    strip = static_cast<Strip *>(strip->next);
  }
}

bool strip_point_image_isect(const Scene *scene, const Strip *strip, float point_view[2])
{
  const Array<float2> strip_image_quad = seq::image_transform_final_quad_get(scene, strip);
  return isect_point_quad_v2(point_view,
                             strip_image_quad[0],
                             strip_image_quad[1],
                             strip_image_quad[2],
                             strip_image_quad[3]);
}

void sequencer_select_do_updates(const bContext *C, Scene *scene)
{
  ED_outliner_select_sync_from_sequence_tag(C);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name (De)select All Operator
 * \{ */

static wmOperatorStatus sequencer_de_select_all_exec(bContext *C, wmOperator *op)
{
  int action = RNA_enum_get(op->ptr, "action");
  Scene *scene = CTX_data_sequencer_scene(C);

  if (sequencer_view_has_preview_poll(C) && !sequencer_view_preview_only_poll(C)) {
    return OPERATOR_CANCELLED;
  }

  if (sequencer_retiming_mode_is_active(C) && retiming_keys_can_be_displayed(CTX_wm_space_seq(C)))
  {
    return sequencer_retiming_select_all_exec(C, op);
  }

  VectorSet strips = all_strips_from_context(C);

  if (action == SEL_TOGGLE) {
    action = SEL_SELECT;
    for (Strip *strip : strips) {
      if (strip->flag & STRIP_ALLSEL) {
        action = SEL_DESELECT;
        break;
      }
    }
  }
  if (ELEM(action, SEL_INVERT, SEL_SELECT)) {
    if (action == SEL_INVERT) {
      for (Strip *strip : strips) {
        if (strip->flag & STRIP_ALLSEL) {
          strips.remove(strip);
        }
      }
    }
    deselect_all_strips(scene);
  }
  for (Strip *strip : strips) {
    switch (action) {
      case SEL_SELECT:
        strip->flag |= SELECT;
        break;
      case SEL_DESELECT:
        strip->flag &= ~STRIP_ALLSEL;
        break;
      case SEL_INVERT:
        strip->flag |= SELECT;
        break;
    }
  }
  ED_outliner_select_sync_from_sequence_tag(C);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_select_all(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "(De)select All";
  ot->idname = "SEQUENCER_OT_select_all";
  ot->description = "Select or deselect all strips";

  /* API callbacks. */
  ot->exec = sequencer_de_select_all_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;

  WM_operator_properties_select_all(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Inverse Operator
 * \{ */

static wmOperatorStatus sequencer_select_inverse_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_sequencer_scene(C);

  if (sequencer_view_has_preview_poll(C) && !sequencer_view_preview_only_poll(C)) {
    return OPERATOR_CANCELLED;
  }

  VectorSet strips = all_strips_from_context(C);

  for (Strip *strip : strips) {
    if (strip->flag & SELECT) {
      strip->flag &= ~STRIP_ALLSEL;
    }
    else {
      strip->flag &= ~(SEQ_LEFTSEL + SEQ_RIGHTSEL);
      strip->flag |= SELECT;
    }
  }

  ED_outliner_select_sync_from_sequence_tag(C);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_select_inverse(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Select Inverse";
  ot->idname = "SEQUENCER_OT_select_inverse";
  ot->description = "Select unselected strips";

  /* API callbacks. */
  ot->exec = sequencer_select_inverse_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Operator
 * \{ */

static void sequencer_select_set_active(Scene *scene, Strip *strip)
{
  seq::select_active_set(scene, strip);
  recurs_sel_strip(strip);
}

static void sequencer_select_side_of_frame(const bContext *C,
                                           const View2D *v2d,
                                           const int mval[2],
                                           Scene *scene)
{
  Editing *ed = seq::editing_get(scene);

  const float x = UI_view2d_region_to_view_x(v2d, mval[0]);
  LISTBASE_FOREACH (Strip *, strip_iter, seq::active_seqbase_get(ed)) {
    if (((x < scene->r.cfra) &&
         (seq::time_right_handle_frame_get(scene, strip_iter) <= scene->r.cfra)) ||
        ((x >= scene->r.cfra) &&
         (seq::time_left_handle_frame_get(scene, strip_iter) >= scene->r.cfra)))
    {
      /* Select left or right. */
      strip_iter->flag |= SELECT;
      recurs_sel_strip(strip_iter);
    }
  }

  {
    SpaceSeq *sseq = CTX_wm_space_seq(C);
    if (sseq && sseq->flag & SEQ_MARKER_TRANS) {

      LISTBASE_FOREACH (TimeMarker *, tmarker, &scene->markers) {
        if (((x < scene->r.cfra) && (tmarker->frame <= scene->r.cfra)) ||
            ((x >= scene->r.cfra) && (tmarker->frame >= scene->r.cfra)))
        {
          tmarker->flag |= SELECT;
        }
        else {
          tmarker->flag &= ~SELECT;
        }
      }
    }
  }
}

static void sequencer_select_linked_handle(const bContext *C,
                                           Strip *strip,
                                           const eStripHandle handle_clicked)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);
  if (!ELEM(handle_clicked, STRIP_HANDLE_LEFT, STRIP_HANDLE_RIGHT)) {
    /* First click selects the strip and its adjacent handles (if valid).
     * Second click selects the strip,
     * both of its handles and its adjacent handles (if valid). */
    const bool is_striponly_selected = ((strip->flag & STRIP_ALLSEL) == SELECT);
    strip->flag &= ~STRIP_ALLSEL;
    strip->flag |= is_striponly_selected ? STRIP_ALLSEL : SELECT;
    select_surrounding_handles(scene, strip);
  }
  else {
    /* Always select the strip under the cursor. */
    strip->flag |= SELECT;

    /* First click selects adjacent handles on that side.
     * Second click selects all strips in that direction.
     * If there are no adjacent strips, it just selects all in that direction.
     */
    const int sel_side = (handle_clicked == STRIP_HANDLE_LEFT) ? seq::SIDE_LEFT : seq::SIDE_RIGHT;

    Strip *neighbor = find_neighboring_strip(scene, strip, sel_side, -1);
    if (neighbor) {
      switch (sel_side) {
        case seq::SIDE_LEFT:
          if ((strip->flag & SEQ_LEFTSEL) && (neighbor->flag & SEQ_RIGHTSEL)) {
            strip->flag |= SELECT;
            select_active_side(scene,
                               ed->current_strips(),
                               seq::SIDE_LEFT,
                               strip->channel,
                               seq::time_left_handle_frame_get(scene, strip));
          }
          else {
            strip->flag |= SELECT;
            neighbor->flag |= SELECT;
            recurs_sel_strip(neighbor);
            neighbor->flag |= SEQ_RIGHTSEL;
            strip->flag |= SEQ_LEFTSEL;
          }
          break;
        case seq::SIDE_RIGHT:
          if ((strip->flag & SEQ_RIGHTSEL) && (neighbor->flag & SEQ_LEFTSEL)) {
            strip->flag |= SELECT;
            select_active_side(scene,
                               ed->current_strips(),
                               seq::SIDE_RIGHT,
                               strip->channel,
                               seq::time_left_handle_frame_get(scene, strip));
          }
          else {
            strip->flag |= SELECT;
            neighbor->flag |= SELECT;
            recurs_sel_strip(neighbor);
            neighbor->flag |= SEQ_LEFTSEL;
            strip->flag |= SEQ_RIGHTSEL;
          }
          break;
      }
    }
    else {

      select_active_side(scene,
                         ed->current_strips(),
                         sel_side,
                         strip->channel,
                         seq::time_left_handle_frame_get(scene, strip));
    }
  }
}

/** Collect sequencer that are candidates for being selected. */
struct SeqSelect_Link {
  SeqSelect_Link *next, *prev;
  Strip *strip;
  /** Only use for center selection. */
  float center_dist_sq;
};

static int strip_sort_for_depth_select(const void *a, const void *b)
{
  const SeqSelect_Link *slink_a = static_cast<const SeqSelect_Link *>(a);
  const SeqSelect_Link *slink_b = static_cast<const SeqSelect_Link *>(b);

  /* Exactly overlapping strips, sort by channel (so the top-most is first). */
  if (slink_a->strip->channel < slink_b->strip->channel) {
    return 1;
  }
  if (slink_a->strip->channel > slink_b->strip->channel) {
    return -1;
  }
  return 0;
}

static int strip_sort_for_center_select(const void *a, const void *b)
{
  const SeqSelect_Link *slink_a = static_cast<const SeqSelect_Link *>(a);
  const SeqSelect_Link *slink_b = static_cast<const SeqSelect_Link *>(b);
  if (slink_a->center_dist_sq > slink_b->center_dist_sq) {
    return 1;
  }
  if (slink_a->center_dist_sq < slink_b->center_dist_sq) {
    return -1;
  }

  /* Exactly overlapping strips, use depth. */
  return strip_sort_for_depth_select(a, b);
}

/**
 * Check if click happened on image which belongs to strip.
 * If multiple strips are found, loop through them in order
 * (depth (top-most first) or closest to mouse when `center` is true).
 */
static Strip *strip_select_from_preview(
    const bContext *C, const int mval[2], const bool toggle, const bool extend, const bool center)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);
  ListBase *seqbase = seq::active_seqbase_get(ed);
  ListBase *channels = seq::channels_displayed_get(ed);
  SpaceSeq *sseq = CTX_wm_space_seq(C);
  View2D *v2d = UI_view2d_fromcontext(C);

  float mouseco_view[2];
  UI_view2d_region_to_view(v2d, mval[0], mval[1], &mouseco_view[0], &mouseco_view[1]);

  /* Always update the coordinates (check extended after). */
  const bool use_cycle = (!WM_cursor_test_motion_and_update(mval) || extend || toggle);

  /* Allow strips this far from the closest center to be included.
   * This allows cycling over center points which are near enough
   * to overlapping from the users perspective. */
  const float center_dist_sq_max = square_f(75.0f * U.pixelsize);
  const float center_scale_px[2] = {
      UI_view2d_scale_get_x(v2d),
      UI_view2d_scale_get_y(v2d),
  };

  VectorSet strips = seq::query_rendered_strips(
      scene, channels, seqbase, scene->r.cfra, sseq->chanshown);

  SeqSelect_Link *slink_active = nullptr;
  Strip *strip_active = seq::select_active_get(scene);
  ListBase strips_ordered = {nullptr};
  for (Strip *strip : strips) {
    bool isect = false;
    float center_dist_sq_test = 0.0f;
    if (center) {
      /* Detect overlapping center points (scaled by the zoom level). */
      float2 co = seq::image_transform_origin_offset_pixelspace_get(scene, strip);
      sub_v2_v2(co, mouseco_view);
      mul_v2_v2(co, center_scale_px);
      center_dist_sq_test = len_squared_v2(co);
      isect = center_dist_sq_test <= center_dist_sq_max;
      if (isect) {
        /* Use an active strip penalty for "center" selection when cycle is enabled. */
        if (use_cycle && (strip == strip_active) && (strip_active->flag & SELECT)) {
          center_dist_sq_test = square_f(sqrtf(center_dist_sq_test) + (3.0f * U.pixelsize));
        }
      }
    }
    else {
      isect = strip_point_image_isect(scene, strip, mouseco_view);
    }

    if (isect) {
      SeqSelect_Link *slink = MEM_callocN<SeqSelect_Link>(__func__);
      slink->strip = strip;
      slink->center_dist_sq = center_dist_sq_test;
      BLI_addtail(&strips_ordered, slink);

      if (strip == strip_active) {
        slink_active = slink;
      }
    }
  }

  BLI_listbase_sort(&strips_ordered,
                    center ? strip_sort_for_center_select : strip_sort_for_depth_select);

  SeqSelect_Link *slink_select = static_cast<SeqSelect_Link *>(strips_ordered.first);
  Strip *strip_select = nullptr;
  if (slink_select != nullptr) {
    /* Only use special behavior for the active strip when it's selected. */
    if ((center == false) && slink_active && (strip_active->flag & SELECT)) {
      if (use_cycle) {
        if (slink_active->next) {
          slink_select = slink_active->next;
        }
      }
      else {
        /* Match object selection behavior: keep the current active item unless cycle is enabled.
         * Clicking again in the same location will cycle away from the active object. */
        slink_select = slink_active;
      }
    }
    strip_select = slink_select->strip;
  }

  BLI_freelistN(&strips_ordered);

  return strip_select;
}

bool handle_is_selected(const Strip *strip, const eStripHandle handle)
{
  return ((strip->flag & SEQ_LEFTSEL) && (handle == STRIP_HANDLE_LEFT)) ||
         ((strip->flag & SEQ_RIGHTSEL) && (handle == STRIP_HANDLE_RIGHT));
}

/**
 * Test to see if the desired strip `selection` already matches the underlying strips' state.
 * If so, `sequencer_select` functions will keep the rest of the current timeline selection intact
 * on press, only selecting the given strip on release if no tweak occurs.
 */
static bool element_already_selected(const StripSelection &selection)
{
  if (selection.strip1 == nullptr) {
    return false;
  }

  const bool strip1_already_selected = ((selection.strip1->flag & SELECT) != 0);
  if (selection.strip2 == nullptr) {
    if (selection.handle == STRIP_HANDLE_NONE) {
      return strip1_already_selected && !(selection.strip1->flag & (SEQ_LEFTSEL | SEQ_RIGHTSEL));
    }
    return strip1_already_selected && handle_is_selected(selection.strip1, selection.handle);
  }

  /* If we are here, the strip selection is dual handle. */
  const bool strip2_already_selected = ((selection.strip2->flag & SELECT) != 0);
  const int strip1_handle = selection.strip1->flag & (SEQ_RIGHTSEL | SEQ_LEFTSEL);
  const int strip2_handle = selection.strip2->flag & (SEQ_RIGHTSEL | SEQ_LEFTSEL);
  /* Handles must be selected in XOR fashion, with `strip1` matching `handle_clicked`. */
  const bool both_handles_selected = strip1_handle == selection.handle && strip2_handle != 0 &&
                                     strip1_handle != strip2_handle;
  return strip1_already_selected && strip2_already_selected && both_handles_selected;
}

static void sequencer_select_connected_strips(const StripSelection &selection)
{
  VectorSet<Strip *> sources;
  sources.add(selection.strip1);
  if (selection.strip2) {
    sources.add(selection.strip2);
  }

  for (Strip *source : sources) {
    VectorSet<Strip *> connections = seq::connected_strips_get(source);
    for (Strip *connection : connections) {
      /* Copy selection settings exactly for connected strips. */
      connection->flag &= ~STRIP_ALLSEL;
      connection->flag |= source->flag & (STRIP_ALLSEL);
    }
  }
}

static void sequencer_copy_handles_to_selected_strips(const Scene *scene,
                                                      const StripSelection &selection,
                                                      VectorSet<Strip *> copy_to)
{
  /* TODO(john): Dual handle propagation is not supported for now due to its complexity,
   * but once we simplify selection assumptions in 5.0 we can add support for it. */
  if (selection.strip2) {
    return;
  }

  Strip *source = selection.strip1;
  /* Test for neighboring strips in the `copy_to` list. If any border one another, remove them,
   * since we don't want to mess with dual handles. */
  VectorSet<Strip *> test(copy_to);
  test.add(source);
  for (Strip *test_strip : test) {
    /* Don't copy left handle over to a `test_strip` that has a strip directly on its left. */
    if ((source->flag & SEQ_LEFTSEL) &&
        find_neighboring_strip(scene, test_strip, seq::SIDE_LEFT, -1))
    {
      /* If this was the source strip, do not copy handles at all and prematurely return. */
      if (test_strip == source) {
        return;
      }
      copy_to.remove(test_strip);
    }

    /* Don't copy right handle over to a `test_strip` that has a strip directly on its right. */
    if ((source->flag & SEQ_RIGHTSEL) &&
        find_neighboring_strip(scene, test_strip, seq::SIDE_RIGHT, -1))
    {
      /* If this was the source strip, do not copy handles at all and prematurely return. */
      if (test_strip == source) {
        return;
      }
      copy_to.remove(test_strip);
    }
  }

  for (Strip *strip : copy_to) {
    /* NOTE that this can be `ALLSEL` since `prev_selection` was deselected earlier. */
    strip->flag &= ~STRIP_ALLSEL;
    strip->flag |= source->flag & STRIP_ALLSEL;
  }
}

static void sequencer_select_strip_impl(const Editing *ed,
                                        Strip *strip,
                                        const eStripHandle handle_clicked,
                                        const bool extend,
                                        const bool deselect,
                                        const bool toggle)
{
  const bool is_active = (ed->act_strip == strip);

  /* Exception for active strip handles. */
  if ((handle_clicked != STRIP_HANDLE_NONE) && (strip->flag & SELECT) && is_active && toggle) {
    if (handle_clicked == STRIP_HANDLE_LEFT) {
      strip->flag ^= SEQ_LEFTSEL;
    }
    else if (handle_clicked == STRIP_HANDLE_RIGHT) {
      strip->flag ^= SEQ_RIGHTSEL;
    }
    return;
  }

  /* Select strip. */
  /* Match object selection behavior. */
  int action = -1;
  if (extend) {
    action = 1;
  }
  else if (deselect) {
    action = 0;
  }
  else {
    if (!((strip->flag & SELECT) && is_active)) {
      action = 1;
    }
    else if (toggle) {
      action = 0;
    }
  }

  if (action == 1) {
    strip->flag |= SELECT;
    if (handle_clicked == STRIP_HANDLE_LEFT) {
      strip->flag |= SEQ_LEFTSEL;
    }
    if (handle_clicked == STRIP_HANDLE_RIGHT) {
      strip->flag |= SEQ_RIGHTSEL;
    }
  }
  else if (action == 0) {
    strip->flag &= ~STRIP_ALLSEL;
  }
}

static void select_linked_time(const Scene *scene,
                               const StripSelection &selection,
                               const bool extend,
                               const bool deselect,
                               const bool toggle)
{
  Editing *ed = seq::editing_get(scene);

  sequencer_select_strip_impl(ed, selection.strip1, selection.handle, extend, deselect, toggle);
  select_linked_time_strip(scene, selection.strip1, selection.handle);

  if (selection.strip2 != nullptr) {
    eStripHandle strip2_handle_clicked = (selection.handle == STRIP_HANDLE_LEFT) ?
                                             STRIP_HANDLE_RIGHT :
                                             STRIP_HANDLE_LEFT;
    sequencer_select_strip_impl(
        ed, selection.strip2, strip2_handle_clicked, extend, deselect, toggle);
    select_linked_time_strip(scene, selection.strip2, strip2_handle_clicked);
  }
}

/**
 * Similar to `strip_handle_draw_size_get()`, but returns a larger clickable area that is
 * the same for a given zoom level no matter whether "simplified tweaking" is turned off or on.
 * `strip_clickable_areas_get` will pad this past strip bounds by 1/3 of the inner handle size,
 * making the full size 15 + 5 = 20px in frames for large strips, 1/4 + 1/12 = 1/3 of the strip
 * size for small ones. */
static float inner_clickable_handle_size_get(const Scene *scene,
                                             const Strip *strip,
                                             const View2D *v2d)
{
  const float pixelx = 1 / UI_view2d_scale_get_x(v2d);
  const float strip_len = seq::time_right_handle_frame_get(scene, strip) -
                          seq::time_left_handle_frame_get(scene, strip);
  return min_ff(15.0f * pixelx * U.pixelsize, strip_len / 4);
}

bool can_select_handle(const Scene *scene, const Strip *strip, const View2D *v2d)
{
  if (seq::effect_get_num_inputs(strip->type) > 0) {
    return false;
  }

  Editing *ed = seq::editing_get(scene);
  ListBase *channels = seq::channels_displayed_get(ed);
  if (seq::transform_is_locked(channels, strip)) {
    return false;
  }

  /* This ensures clickable handles are deactivated when the strip gets too small
   * (25 pixels). Since the full handle size for a small strip is 1/3 of the strip size (see
   * `inner_clickable_handle_size_get`), this means handles cannot be smaller than 25/3 = 8px. */
  int min_len = 25 * U.pixelsize;

  const float pixelx = 1 / UI_view2d_scale_get_x(v2d);
  const int strip_len = seq::time_right_handle_frame_get(scene, strip) -
                        seq::time_left_handle_frame_get(scene, strip);
  if (strip_len / pixelx < min_len) {
    return false;
  }

  if (UI_view2d_scale_get_y(v2d) < 16 * U.pixelsize) {
    return false;
  }

  return true;
}

static void strip_clickable_areas_get(const Scene *scene,
                                      const Strip *strip,
                                      const View2D *v2d,
                                      rctf *r_body,
                                      rctf *r_left_handle,
                                      rctf *r_right_handle)
{
  strip_rectf(scene, strip, r_body);
  *r_left_handle = *r_body;
  *r_right_handle = *r_body;

  const float handsize = inner_clickable_handle_size_get(scene, strip, v2d);
  r_left_handle->xmax = r_body->xmin + handsize;
  r_right_handle->xmin = r_body->xmax - handsize;
  BLI_rctf_pad(r_left_handle, handsize / 3, 0.0f);
  BLI_rctf_pad(r_right_handle, handsize / 3, 0.0f);
  BLI_rctf_pad(r_body, -handsize, 0.0f);
}

static rctf strip_clickable_area_get(const Scene *scene, const View2D *v2d, const Strip *strip)
{
  rctf body, left, right;
  strip_clickable_areas_get(scene, strip, v2d, &body, &left, &right);
  BLI_rctf_union(&body, &left);
  BLI_rctf_union(&body, &right);
  return body;
}

static float strip_to_frame_distance(const Scene *scene,
                                     const View2D *v2d,
                                     const Strip *strip,
                                     float timeline_frame)
{
  rctf body, left, right;
  strip_clickable_areas_get(scene, strip, v2d, &body, &left, &right);
  return BLI_rctf_length_x(&body, timeline_frame);
}

/**
 * Get strips that can be selected by a click from `mouse_co` in view-space.
 * The area considered includes padded handles past strip bounds, so multiple strips may be
 * returned.
 */
static Vector<Strip *> padded_strips_under_mouse_get(const Scene *scene,
                                                     const View2D *v2d,
                                                     float mouse_co[2])
{
  Editing *ed = seq::editing_get(scene);

  if (ed == nullptr) {
    return {};
  }

  Vector<Strip *> strips;
  LISTBASE_FOREACH (Strip *, strip, ed->current_strips()) {
    if (strip->channel != int(mouse_co[1])) {
      continue;
    }
    if (seq::time_left_handle_frame_get(scene, strip) > v2d->cur.xmax) {
      continue;
    }
    if (seq::time_right_handle_frame_get(scene, strip) < v2d->cur.xmin) {
      continue;
    }
    const rctf body = strip_clickable_area_get(scene, v2d, strip);
    if (!BLI_rctf_isect_pt_v(&body, mouse_co)) {
      continue;
    }
    strips.append(strip);
  }

  std::sort(strips.begin(), strips.end(), [&](const Strip *strip1, const Strip *strip2) {
    return strip_to_frame_distance(scene, v2d, strip1, mouse_co[0]) <
           strip_to_frame_distance(scene, v2d, strip2, mouse_co[0]);
  });

  return strips;
}

static bool strips_are_adjacent(const Scene *scene, const Strip *strip1, const Strip *strip2)
{
  const int s1_left = seq::time_left_handle_frame_get(scene, strip1);
  const int s1_right = seq::time_right_handle_frame_get(scene, strip1);
  const int s2_left = seq::time_left_handle_frame_get(scene, strip2);
  const int s2_right = seq::time_right_handle_frame_get(scene, strip2);

  return s1_right == s2_left || s1_left == s2_right;
}

static eStripHandle strip_handle_under_cursor_get(const Scene *scene,
                                                  const Strip *strip,
                                                  const View2D *v2d,
                                                  float mouse_co[2])
{
  if (!can_select_handle(scene, strip, v2d)) {
    return STRIP_HANDLE_NONE;
  }

  rctf body, left, right;
  strip_clickable_areas_get(scene, strip, v2d, &body, &left, &right);
  if (BLI_rctf_isect_pt_v(&left, mouse_co)) {
    return STRIP_HANDLE_LEFT;
  }
  if (BLI_rctf_isect_pt_v(&right, mouse_co)) {
    return STRIP_HANDLE_RIGHT;
  }

  return STRIP_HANDLE_NONE;
}

static bool is_mouse_over_both_handles_of_adjacent_strips(const Scene *scene,
                                                          Vector<Strip *> strips,
                                                          const View2D *v2d,
                                                          float mouse_co[2])
{
  const eStripHandle strip1_handle = strip_handle_under_cursor_get(
      scene, strips[0], v2d, mouse_co);

  if (strip1_handle == STRIP_HANDLE_NONE) {
    return false;
  }
  if (!strips_are_adjacent(scene, strips[0], strips[1])) {
    return false;
  }
  const eStripHandle strip2_handle = strip_handle_under_cursor_get(
      scene, strips[1], v2d, mouse_co);
  if (strip1_handle == STRIP_HANDLE_RIGHT && strip2_handle != STRIP_HANDLE_LEFT) {
    return false;
  }
  if (strip1_handle == STRIP_HANDLE_LEFT && strip2_handle != STRIP_HANDLE_RIGHT) {
    return false;
  }

  return true;
}

StripSelection pick_strip_and_handle(const Scene *scene, const View2D *v2d, float mouse_co[2])
{
  StripSelection selection;
  /* Do not pick strips when clicking inside time scrub region. */
  float time_scrub_y = v2d->cur.ymax - UI_TIME_SCRUB_MARGIN_Y / UI_view2d_scale_get_y(v2d);
  if (mouse_co[1] > time_scrub_y) {
    return selection;
  }

  Vector<Strip *> strips = padded_strips_under_mouse_get(scene, v2d, mouse_co);

  if (strips.size() == 0) {
    return selection;
  }

  selection.strip1 = strips[0];
  selection.handle = strip_handle_under_cursor_get(scene, selection.strip1, v2d, mouse_co);

  if (strips.size() == 2 &&
      is_mouse_over_both_handles_of_adjacent_strips(scene, strips, v2d, mouse_co))
  {
    selection.strip2 = strips[1];
  }

  return selection;
}

wmOperatorStatus sequencer_select_exec(bContext *C, wmOperator *op)
{
  const View2D *v2d = UI_view2d_fromcontext(C);
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);
  ARegion *region = CTX_wm_region(C);

  if (ed == nullptr) {
    return OPERATOR_CANCELLED;
  }

  if (region->regiontype == RGN_TYPE_PREVIEW) {
    if (!sequencer_view_preview_only_poll(C)) {
      return OPERATOR_CANCELLED;
    }
    const SpaceSeq *sseq = CTX_wm_space_seq(C);
    if (sseq->mainb != SEQ_DRAW_IMG_IMBUF) {
      return OPERATOR_CANCELLED;
    }
  }

  const bool was_retiming = sequencer_retiming_mode_is_active(C);

  MouseCoords mouse_co(v2d, RNA_int_get(op->ptr, "mouse_x"), RNA_int_get(op->ptr, "mouse_y"));

  /* Check to see if the mouse cursor intersects with the retiming box; if so, `strip_key_owner` is
   * set. If the cursor intersects with a retiming key, `key` will be set too. */
  Strip *strip_key_owner = nullptr;
  SeqRetimingKey *key = retiming_mouseover_key_get(C, mouse_co.region, &strip_key_owner);

  /* If no key was found, the mouse cursor may still intersect with a "fake key" that has not been
   * realized yet. */
  if (strip_key_owner != nullptr && key == nullptr &&
      retiming_keys_can_be_displayed(CTX_wm_space_seq(C)) &&
      seq::retiming_data_is_editable(strip_key_owner))
  {
    key = try_to_realize_fake_keys(C, strip_key_owner, mouse_co.region);
  }

  if (key != nullptr) {
    if (!was_retiming) {
      deselect_all_strips(scene);
      sequencer_select_do_updates(C, scene);
    }
    /* Attempt to realize any other connected strips' fake keys. */
    if (seq::is_strip_connected(strip_key_owner)) {
      const int key_frame = seq::retiming_key_timeline_frame_get(scene, strip_key_owner, key);
      VectorSet<Strip *> connections = seq::connected_strips_get(strip_key_owner);
      for (Strip *connection : connections) {
        if (key_frame == left_fake_key_frame_get(C, connection) ||
            key_frame == right_fake_key_frame_get(C, connection))
        {
          realize_fake_keys(scene, connection);
        }
      }
    }
    return sequencer_retiming_key_select_exec(C, op, key, strip_key_owner);
  }

  /* We should only reach here if no retiming selection is happening. */
  if (was_retiming) {
    seq::retiming_selection_clear(ed);
    WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
  }

  const bool extend = RNA_boolean_get(op->ptr, "extend");
  const bool deselect = RNA_boolean_get(op->ptr, "deselect");
  const bool deselect_all = RNA_boolean_get(op->ptr, "deselect_all");
  const bool toggle = RNA_boolean_get(op->ptr, "toggle");
  const bool center = RNA_boolean_get(op->ptr, "center");

  StripSelection selection;
  if (region->regiontype == RGN_TYPE_PREVIEW) {
    selection.strip1 = strip_select_from_preview(C, mouse_co.region, toggle, extend, center);
  }
  else {
    selection = pick_strip_and_handle(scene, v2d, mouse_co.view);
  }

  /* NOTE: `side_of_frame` and `linked_time` functionality is designed to be shared on one
   * keymap, therefore both properties can be true at the same time. */
  if (selection.strip1 && RNA_boolean_get(op->ptr, "linked_time")) {
    if (!extend && !toggle) {
      deselect_all_strips(scene);
    }
    select_linked_time(scene, selection, extend, deselect, toggle);
    sequencer_select_do_updates(C, scene);
    sequencer_select_set_active(scene, selection.strip1);
    return OPERATOR_FINISHED;
  }

  /* Select left, right or overlapping the current frame. */
  if (RNA_boolean_get(op->ptr, "side_of_frame")) {
    if (!extend && !toggle) {
      deselect_all_strips(scene);
    }
    sequencer_select_side_of_frame(C, v2d, mouse_co.region, scene);
    sequencer_select_do_updates(C, scene);
    return OPERATOR_FINISHED;
  }

  /* On Alt selection, select the strip and bordering handles. */
  if (selection.strip1 && RNA_boolean_get(op->ptr, "linked_handle")) {
    if (!extend && !toggle) {
      deselect_all_strips(scene);
    }
    sequencer_select_linked_handle(C, selection.strip1, selection.handle);
    sequencer_select_do_updates(C, scene);
    sequencer_select_set_active(scene, selection.strip1);
    return OPERATOR_FINISHED;
  }

  const bool wait_to_deselect_others = RNA_boolean_get(op->ptr, "wait_to_deselect_others");
  const bool already_selected = element_already_selected(selection);

  SpaceSeq *sseq = CTX_wm_space_seq(C);
  if (selection.handle != STRIP_HANDLE_NONE && already_selected) {
    sseq->flag &= ~SPACE_SEQ_DESELECT_STRIP_HANDLE;
  }
  else {
    sseq->flag |= SPACE_SEQ_DESELECT_STRIP_HANDLE;
  }
  const bool ignore_connections = RNA_boolean_get(op->ptr, "ignore_connections");

  /* Clicking on already selected element falls on modal operation.
   * All strips are deselected on mouse button release unless extend mode is used. */
  if (already_selected && wait_to_deselect_others && !toggle && !ignore_connections) {
    return OPERATOR_RUNNING_MODAL;
  }

  VectorSet<Strip *> copy_to;
  /* True if the user selects either handle of a strip that is already selected, meaning that
   * handles should be propagated to all currently selected strips. */
  bool copy_handles_to_sel = (selection.handle != STRIP_HANDLE_NONE) &&
                             (selection.strip1->flag & SELECT);

  /* TODO(john): Dual handle propagation is not supported for now due to its complexity,
   * but once we simplify selection assumptions in 5.0 we can add support for it. */
  copy_handles_to_sel &= (selection.strip2 == nullptr);

  if (copy_handles_to_sel) {
    copy_to = seq::query_selected_strips(seq::active_seqbase_get(scene->ed));
    copy_to.remove(selection.strip1);
    copy_to.remove_if([](Strip *strip) { return strip->is_effect(); });
  }

  bool changed = false;
  /* Deselect everything for now. NOTE that this condition runs for almost every click with no
   * modifiers. `sequencer_select_strip_impl` expects this and will re-select any strips in
   * `selection`. */
  if (deselect_all ||
      (selection.strip1 && (extend == false && deselect == false && toggle == false)))
  {
    changed |= deselect_all_strips(scene);
  }

  /* Nothing to select, but strips might have been deselected, in which case we should update. */
  if (!selection.strip1) {
    if (changed) {
      sequencer_select_do_updates(C, scene);
    }
    return changed ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
  }

  /* Do actual selection. */
  sequencer_select_strip_impl(ed, selection.strip1, selection.handle, extend, deselect, toggle);
  if (selection.strip2 != nullptr) {
    /* Invert handle selection for second strip. */
    eStripHandle strip2_handle_clicked = (selection.handle == STRIP_HANDLE_LEFT) ?
                                             STRIP_HANDLE_RIGHT :
                                             STRIP_HANDLE_LEFT;
    sequencer_select_strip_impl(
        ed, selection.strip2, strip2_handle_clicked, extend, deselect, toggle);
  }

  if (!ignore_connections) {
    if (copy_handles_to_sel) {
      sequencer_copy_handles_to_selected_strips(scene, selection, copy_to);
    }

    sequencer_select_connected_strips(selection);
  }

  sequencer_select_do_updates(C, scene);
  sequencer_select_set_active(scene, selection.strip1);
  return OPERATOR_FINISHED;
}

static wmOperatorStatus sequencer_select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const wmOperatorStatus retval = WM_generic_select_invoke(C, op, event);
  ARegion *region = CTX_wm_region(C);
  if (region && (region->regiontype == RGN_TYPE_PREVIEW)) {
    return WM_operator_flag_only_pass_through_on_press(retval, event);
  }
  return retval;
}

static std::string sequencer_select_get_name(wmOperatorType *ot, PointerRNA *ptr)
{
  if (RNA_boolean_get(ptr, "ignore_connections")) {
    return CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Select (Unconnected)");
  }
  if (RNA_boolean_get(ptr, "linked_time")) {
    return CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Select (Linked Time)");
  }
  if (RNA_boolean_get(ptr, "linked_handle")) {
    return CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Select (Linked Handle)");
  }
  if (RNA_boolean_get(ptr, "side_of_frame")) {
    return CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Select (Side of Frame)");
  }

  return ED_select_pick_get_name(ot, ptr);
}

void SEQUENCER_OT_select(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers. */
  ot->name = "Select";
  ot->idname = "SEQUENCER_OT_select";
  ot->description = "Select a strip (last selected becomes the \"active strip\")";

  /* API callbacks. */
  ot->exec = sequencer_select_exec;
  ot->invoke = sequencer_select_invoke;
  ot->modal = WM_generic_select_modal;
  ot->poll = ED_operator_sequencer_active;
  ot->get_name = sequencer_select_get_name;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;

  /* Properties. */
  WM_operator_properties_generic_select(ot);

  WM_operator_properties_mouse_select(ot);

  prop = RNA_def_boolean(
      ot->srna,
      "center",
      false,
      "Center",
      "Use the object center when selecting, in edit mode used to extend object selection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna,
                         "linked_handle",
                         false,
                         "Linked Handle",
                         "Select handles next to the active strip");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna,
                         "linked_time",
                         false,
                         "Linked Time",
                         "Select other strips or handles at the same time, or all retiming keys "
                         "after the current in retiming mode");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(
      ot->srna,
      "side_of_frame",
      false,
      "Side of Frame",
      "Select all strips on same side of the current frame as the mouse cursor");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna,
                         "ignore_connections",
                         false,
                         "Ignore Connections",
                         "Select strips individually whether or not they are connected");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Handle Operator
 * \{ */

/** This operator is only used in the RCS keymap by default and is not exposed in any menus. */
static wmOperatorStatus sequencer_select_handle_exec(bContext *C, wmOperator *op)
{
  const View2D *v2d = UI_view2d_fromcontext(C);
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);

  if (ed == nullptr) {
    return OPERATOR_CANCELLED;
  }

  MouseCoords mouse_co(v2d, RNA_int_get(op->ptr, "mouse_x"), RNA_int_get(op->ptr, "mouse_y"));

  StripSelection selection = pick_strip_and_handle(scene, v2d, mouse_co.view);
  if (selection.strip1 == nullptr || selection.handle == STRIP_HANDLE_NONE) {
    return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
  }

  /* Ignore clicks on retiming keys. */
  Strip *strip_key_test = nullptr;
  SeqRetimingKey *key = retiming_mouseover_key_get(C, mouse_co.region, &strip_key_test);
  if (key != nullptr) {
    return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
  }

  SpaceSeq *sseq = CTX_wm_space_seq(C);
  if (element_already_selected(selection)) {
    sseq->flag &= ~SPACE_SEQ_DESELECT_STRIP_HANDLE;
    return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
  }
  sseq->flag |= SPACE_SEQ_DESELECT_STRIP_HANDLE;
  deselect_all_strips(scene);

  /* Do actual selection. */
  sequencer_select_strip_impl(ed, selection.strip1, selection.handle, false, false, false);
  if (selection.strip2 != nullptr) {
    /* Invert handle selection for second strip */
    eStripHandle strip2_handle_clicked = (selection.handle == STRIP_HANDLE_LEFT) ?
                                             STRIP_HANDLE_RIGHT :
                                             STRIP_HANDLE_LEFT;
    sequencer_select_strip_impl(ed, selection.strip2, strip2_handle_clicked, false, false, false);
  }

  const bool ignore_connections = RNA_boolean_get(op->ptr, "ignore_connections");
  if (!ignore_connections) {
    sequencer_select_connected_strips(selection);
  }

  seq::retiming_selection_clear(ed);
  sequencer_select_do_updates(C, scene);
  sequencer_select_set_active(scene, selection.strip1);
  return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
}

static wmOperatorStatus sequencer_select_handle_invoke(bContext *C,
                                                       wmOperator *op,
                                                       const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);

  int mval[2];
  WM_event_drag_start_mval(event, region, mval);

  RNA_int_set(op->ptr, "mouse_x", mval[0]);
  RNA_int_set(op->ptr, "mouse_y", mval[1]);

  return sequencer_select_handle_exec(C, op);
}

void SEQUENCER_OT_select_handle(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers. */
  ot->name = "Select Handle";
  ot->idname = "SEQUENCER_OT_select_handle";
  ot->description = "Select strip handle";

  /* API callbacks. */
  ot->exec = sequencer_select_handle_exec;
  ot->invoke = sequencer_select_handle_invoke;
  ot->poll = ED_operator_sequencer_active;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;

  /* Properties. */
  WM_operator_properties_generic_select(ot);

  prop = RNA_def_boolean(ot->srna,
                         "ignore_connections",
                         false,
                         "Ignore Connections",
                         "Select strips individually whether or not they are connected");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select More Operator
 * \{ */

/* Run recursively to select linked. */
static bool select_linked_internal(Scene *scene)
{
  Editing *ed = seq::editing_get(scene);

  if (ed == nullptr) {
    return false;
  }

  bool changed = false;

  LISTBASE_FOREACH (Strip *, strip, seq::active_seqbase_get(ed)) {
    if ((strip->flag & SELECT) == 0) {
      continue;
    }
    /* Only get unselected neighbors. */
    Strip *neighbor = find_neighboring_strip(scene, strip, seq::SIDE_LEFT, 0);
    if (neighbor) {
      neighbor->flag |= SELECT;
      recurs_sel_strip(neighbor);
      changed = true;
    }
    neighbor = find_neighboring_strip(scene, strip, seq::SIDE_RIGHT, 0);
    if (neighbor) {
      neighbor->flag |= SELECT;
      recurs_sel_strip(neighbor);
      changed = true;
    }
  }

  return changed;
}

/* Select only one linked strip on each side. */
static bool select_more_less_impl(Scene *scene, bool select_more)
{
  Editing *ed = seq::editing_get(scene);

  if (ed == nullptr) {
    return false;
  }

  GSet *neighbors = BLI_gset_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "Linked strips");
  const int neighbor_selection_filter = select_more ? 0 : SELECT;
  const int selection_filter = select_more ? SELECT : 0;

  LISTBASE_FOREACH (Strip *, strip, seq::active_seqbase_get(ed)) {
    if ((strip->flag & SELECT) != selection_filter) {
      continue;
    }
    Strip *neighbor = find_neighboring_strip(
        scene, strip, seq::SIDE_LEFT, neighbor_selection_filter);
    if (neighbor) {
      BLI_gset_add(neighbors, neighbor);
    }
    neighbor = find_neighboring_strip(scene, strip, seq::SIDE_RIGHT, neighbor_selection_filter);
    if (neighbor) {
      BLI_gset_add(neighbors, neighbor);
    }
  }

  bool changed = false;
  GSetIterator gsi;
  BLI_gsetIterator_init(&gsi, neighbors);
  while (!BLI_gsetIterator_done(&gsi)) {
    Strip *neighbor = static_cast<Strip *>(BLI_gsetIterator_getKey(&gsi));
    if (select_more) {
      neighbor->flag |= SELECT;
      recurs_sel_strip(neighbor);
    }
    else {
      neighbor->flag &= ~SELECT;
    }
    changed = true;
    BLI_gsetIterator_step(&gsi);
  }

  BLI_gset_free(neighbors, nullptr);
  return changed;
}

static wmOperatorStatus sequencer_select_more_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_sequencer_scene(C);

  if (!select_more_less_impl(scene, true)) {
    return OPERATOR_CANCELLED;
  }

  ED_outliner_select_sync_from_sequence_tag(C);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_select_more(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Select More";
  ot->idname = "SEQUENCER_OT_select_more";
  ot->description = "Select more strips adjacent to the current selection";

  /* API callbacks. */
  ot->exec = sequencer_select_more_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Less Operator
 * \{ */

static wmOperatorStatus sequencer_select_less_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_sequencer_scene(C);

  if (!select_more_less_impl(scene, false)) {
    return OPERATOR_CANCELLED;
  }

  ED_outliner_select_sync_from_sequence_tag(C);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_select_less(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Select Less";
  ot->idname = "SEQUENCER_OT_select_less";
  ot->description = "Shrink the current selection of adjacent selected strips";

  /* API callbacks. */
  ot->exec = sequencer_select_less_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Pick Linked Operator
 * \{ */

static wmOperatorStatus sequencer_select_linked_pick_invoke(bContext *C,
                                                            wmOperator *op,
                                                            const wmEvent *event)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  const View2D *v2d = UI_view2d_fromcontext(C);

  bool extend = RNA_boolean_get(op->ptr, "extend");

  float mouse_co[2];
  UI_view2d_region_to_view(v2d, event->mval[0], event->mval[1], &mouse_co[0], &mouse_co[1]);

  /* This works like UV, not mesh. */
  StripSelection mouse_selection = pick_strip_and_handle(scene, v2d, mouse_co);
  if (!mouse_selection.strip1) {
    return OPERATOR_FINISHED; /* User error as with mesh?? */
  }

  if (extend == 0) {
    deselect_all_strips(scene);
  }

  mouse_selection.strip1->flag |= SELECT;
  recurs_sel_strip(mouse_selection.strip1);

  bool selected = true;
  while (selected) {
    selected = select_linked_internal(scene);
  }

  ED_outliner_select_sync_from_sequence_tag(C);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_select_linked_pick(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Select Pick Linked";
  ot->idname = "SEQUENCER_OT_select_linked_pick";
  ot->description = "Select a chain of linked strips nearest to the mouse pointer";

  /* API callbacks. */
  ot->invoke = sequencer_select_linked_pick_invoke;
  ot->poll = ED_operator_sequencer_active;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties. */
  PropertyRNA *prop;
  prop = RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the selection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Linked Operator
 * \{ */

static wmOperatorStatus sequencer_select_linked_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  bool selected;

  selected = true;
  while (selected) {
    selected = select_linked_internal(scene);
  }

  ED_outliner_select_sync_from_sequence_tag(C);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_select_linked(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Select Linked";
  ot->idname = "SEQUENCER_OT_select_linked";
  ot->description = "Select all strips adjacent to the current selection";

  /* API callbacks. */
  ot->exec = sequencer_select_linked_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Handles Operator
 * \{ */

enum {
  SEQ_SELECT_HANDLES_SIDE_LEFT,
  SEQ_SELECT_HANDLES_SIDE_RIGHT,
  SEQ_SELECT_HANDLES_SIDE_BOTH,
  SEQ_SELECT_HANDLES_SIDE_LEFT_NEIGHBOR,
  SEQ_SELECT_HANDLES_SIDE_RIGHT_NEIGHBOR,
  SEQ_SELECT_HANDLES_SIDE_BOTH_NEIGHBORS,
};

static const EnumPropertyItem prop_select_handles_side_types[] = {
    {SEQ_SELECT_HANDLES_SIDE_LEFT, "LEFT", 0, "Left", ""},
    {SEQ_SELECT_HANDLES_SIDE_RIGHT, "RIGHT", 0, "Right", ""},
    {SEQ_SELECT_HANDLES_SIDE_BOTH, "BOTH", 0, "Both", ""},
    {SEQ_SELECT_HANDLES_SIDE_LEFT_NEIGHBOR, "LEFT_NEIGHBOR", 0, "Left Neighbor", ""},
    {SEQ_SELECT_HANDLES_SIDE_RIGHT_NEIGHBOR, "RIGHT_NEIGHBOR", 0, "Right Neighbor", ""},
    {SEQ_SELECT_HANDLES_SIDE_BOTH_NEIGHBORS, "BOTH_NEIGHBORS", 0, "Both Neighbors", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static wmOperatorStatus sequencer_select_handles_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);
  int sel_side = RNA_enum_get(op->ptr, "side");
  LISTBASE_FOREACH (Strip *, strip, ed->current_strips()) {
    if (strip->flag & SELECT) {
      Strip *l_neighbor = find_neighboring_strip(scene, strip, seq::SIDE_LEFT, -1);
      Strip *r_neighbor = find_neighboring_strip(scene, strip, seq::SIDE_RIGHT, -1);

      switch (sel_side) {
        case SEQ_SELECT_HANDLES_SIDE_LEFT:
          strip->flag &= ~SEQ_RIGHTSEL;
          strip->flag |= SEQ_LEFTSEL;
          break;
        case SEQ_SELECT_HANDLES_SIDE_RIGHT:
          strip->flag &= ~SEQ_LEFTSEL;
          strip->flag |= SEQ_RIGHTSEL;
          break;
        case SEQ_SELECT_HANDLES_SIDE_BOTH:
          strip->flag |= SEQ_LEFTSEL | SEQ_RIGHTSEL;
          break;
        case SEQ_SELECT_HANDLES_SIDE_LEFT_NEIGHBOR:
          if (l_neighbor) {
            if (!(l_neighbor->flag & SELECT)) {
              l_neighbor->flag |= SEQ_RIGHTSEL;
            }
          }
          break;
        case SEQ_SELECT_HANDLES_SIDE_RIGHT_NEIGHBOR:
          if (r_neighbor) {
            if (!(r_neighbor->flag & SELECT)) {
              r_neighbor->flag |= SEQ_LEFTSEL;
            }
          }
          break;
        case SEQ_SELECT_HANDLES_SIDE_BOTH_NEIGHBORS:
          if (l_neighbor) {
            if (!(l_neighbor->flag & SELECT)) {
              l_neighbor->flag |= SEQ_RIGHTSEL;
            }
          }
          if (r_neighbor) {
            if (!(r_neighbor->flag & SELECT)) {
              r_neighbor->flag |= SEQ_LEFTSEL;
            }
            break;
          }
      }
    }
  }
  /* Select strips. */
  LISTBASE_FOREACH (Strip *, strip, ed->current_strips()) {
    if ((strip->flag & SEQ_LEFTSEL) || (strip->flag & SEQ_RIGHTSEL)) {
      if (!(strip->flag & SELECT)) {
        strip->flag |= SELECT;
        recurs_sel_strip(strip);
      }
    }
  }

  ED_outliner_select_sync_from_sequence_tag(C);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_select_handles(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Select Handles";
  ot->idname = "SEQUENCER_OT_select_handles";
  ot->description = "Select gizmo handles on the sides of the selected strip";

  /* API callbacks. */
  ot->exec = sequencer_select_handles_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties. */
  RNA_def_enum(ot->srna,
               "side",
               prop_select_handles_side_types,
               SEQ_SELECT_HANDLES_SIDE_BOTH,
               "Side",
               "The side of the handle that is selected");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Side of Frame Operator
 * \{ */

static wmOperatorStatus sequencer_select_side_of_frame_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);
  const bool extend = RNA_boolean_get(op->ptr, "extend");
  const int side = RNA_enum_get(op->ptr, "side");

  if (ed == nullptr) {
    return OPERATOR_CANCELLED;
  }
  if (extend == false) {
    deselect_all_strips(scene);
  }
  const int timeline_frame = scene->r.cfra;
  LISTBASE_FOREACH (Strip *, strip, seq::active_seqbase_get(ed)) {
    bool test = false;
    switch (side) {
      case -1:
        test = (timeline_frame >= seq::time_right_handle_frame_get(scene, strip));
        break;
      case 1:
        test = (timeline_frame <= seq::time_left_handle_frame_get(scene, strip));
        break;
      case 2:
        test = seq::time_strip_intersects_frame(scene, strip, timeline_frame);
        break;
    }

    if (test) {
      strip->flag |= SELECT;
      recurs_sel_strip(strip);
    }
  }

  ED_outliner_select_sync_from_sequence_tag(C);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_select_side_of_frame(wmOperatorType *ot)
{
  static const EnumPropertyItem sequencer_select_left_right_types[] = {
      {-1, "LEFT", 0, "Left", "Select to the left of the current frame"},
      {1, "RIGHT", 0, "Right", "Select to the right of the current frame"},
      {2, "CURRENT", 0, "Current Frame", "Select intersecting with the current frame"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* Identifiers. */
  ot->name = "Select Side of Frame";
  ot->idname = "SEQUENCER_OT_select_side_of_frame";
  ot->description = "Select strips relative to the current frame";

  /* API callbacks. */
  ot->exec = sequencer_select_side_of_frame_exec;
  ot->poll = ED_operator_sequencer_active;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;

  /* Properties. */
  PropertyRNA *prop;
  prop = RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the selection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  ot->prop = RNA_def_enum(ot->srna, "side", sequencer_select_left_right_types, 0, "Side", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Side Operator
 * \{ */

static wmOperatorStatus sequencer_select_side_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);

  const int sel_side = RNA_enum_get(op->ptr, "side");
  const int frame_init = sel_side == seq::SIDE_LEFT ? INT_MIN : INT_MAX;
  int frame_ranges[seq::MAX_CHANNELS];
  bool selected = false;

  copy_vn_i(frame_ranges, ARRAY_SIZE(frame_ranges), frame_init);

  LISTBASE_FOREACH (Strip *, strip, ed->current_strips()) {
    if (UNLIKELY(strip->channel >= seq::MAX_CHANNELS)) {
      continue;
    }
    int *frame_limit_p = &frame_ranges[strip->channel];
    if (strip->flag & SELECT) {
      selected = true;
      if (sel_side == seq::SIDE_LEFT) {
        *frame_limit_p = max_ii(*frame_limit_p, seq::time_left_handle_frame_get(scene, strip));
      }
      else {
        *frame_limit_p = min_ii(*frame_limit_p, seq::time_left_handle_frame_get(scene, strip));
      }
    }
  }

  if (selected == false) {
    return OPERATOR_CANCELLED;
  }

  select_active_side_range(scene, ed->current_strips(), sel_side, frame_ranges, frame_init);

  ED_outliner_select_sync_from_sequence_tag(C);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_select_side(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Select Side";
  ot->idname = "SEQUENCER_OT_select_side";
  ot->description = "Select strips on the nominated side of the selected strips";

  /* API callbacks. */
  ot->exec = sequencer_select_side_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties. */
  RNA_def_enum(ot->srna,
               "side",
               prop_side_types,
               seq::SIDE_BOTH,
               "Side",
               "The side to which the selection is applied");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Box Select Operator
 * \{ */

static bool strip_box_select_rect_image_isect(const Scene *scene,
                                              const Strip *strip,
                                              const rctf *rect)
{
  const Array<float2> strip_image_quad = seq::image_transform_final_quad_get(scene, strip);
  float rect_quad[4][2] = {{rect->xmax, rect->ymax},
                           {rect->xmax, rect->ymin},
                           {rect->xmin, rect->ymin},
                           {rect->xmin, rect->ymax}};

  return strip_point_image_isect(scene, strip, rect_quad[0]) ||
         strip_point_image_isect(scene, strip, rect_quad[1]) ||
         strip_point_image_isect(scene, strip, rect_quad[2]) ||
         strip_point_image_isect(scene, strip, rect_quad[3]) ||
         isect_point_quad_v2(
             strip_image_quad[0], rect_quad[0], rect_quad[1], rect_quad[2], rect_quad[3]) ||
         isect_point_quad_v2(
             strip_image_quad[1], rect_quad[0], rect_quad[1], rect_quad[2], rect_quad[3]) ||
         isect_point_quad_v2(
             strip_image_quad[2], rect_quad[0], rect_quad[1], rect_quad[2], rect_quad[3]) ||
         isect_point_quad_v2(
             strip_image_quad[3], rect_quad[0], rect_quad[1], rect_quad[2], rect_quad[3]);
}

static void seq_box_select_strip_from_preview(const bContext *C,
                                              const rctf *rect,
                                              const eSelectOp mode)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);
  ListBase *seqbase = seq::active_seqbase_get(ed);
  ListBase *channels = seq::channels_displayed_get(ed);
  SpaceSeq *sseq = CTX_wm_space_seq(C);

  VectorSet strips = seq::query_rendered_strips(
      scene, channels, seqbase, scene->r.cfra, sseq->chanshown);
  for (Strip *strip : strips) {
    if (!strip_box_select_rect_image_isect(scene, strip, rect)) {
      continue;
    }

    if (ELEM(mode, SEL_OP_ADD, SEL_OP_SET)) {
      strip->flag |= SELECT;
    }
    else {
      BLI_assert(mode == SEL_OP_SUB);
      strip->flag &= ~SELECT;
    }
  }
}

static wmOperatorStatus sequencer_box_select_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  View2D *v2d = UI_view2d_fromcontext(C);
  Editing *ed = seq::editing_get(scene);

  if (ed == nullptr) {
    return OPERATOR_CANCELLED;
  }

  if (sequencer_retiming_mode_is_active(C) && retiming_keys_can_be_displayed(CTX_wm_space_seq(C)))
  {
    return sequencer_retiming_box_select_exec(C, op);
  }

  const eSelectOp sel_op = eSelectOp(RNA_enum_get(op->ptr, "mode"));
  const bool handles = RNA_boolean_get(op->ptr, "include_handles");
  const bool select = (sel_op != SEL_OP_SUB);

  bool changed = false;

  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    changed |= deselect_all_strips(scene);
  }

  rctf rectf;
  WM_operator_properties_border_to_rctf(op, &rectf);
  UI_view2d_region_to_view_rctf(v2d, &rectf, &rectf);

  ARegion *region = CTX_wm_region(C);
  if (region->regiontype == RGN_TYPE_PREVIEW) {
    if (!sequencer_view_preview_only_poll(C)) {
      return OPERATOR_CANCELLED;
    }
    seq_box_select_strip_from_preview(C, &rectf, sel_op);
    sequencer_select_do_updates(C, scene);
    return OPERATOR_FINISHED;
  }

  LISTBASE_FOREACH (Strip *, strip, ed->current_strips()) {
    rctf rq;
    strip_rectf(scene, strip, &rq);
    if (BLI_rctf_isect(&rq, &rectf, nullptr)) {
      if (handles) {
        /* Get the clickable handle size, ignoring padding. */
        float handsize = inner_clickable_handle_size_get(scene, strip, v2d) * 4;

        /* Right handle. */
        if (rectf.xmax > (seq::time_right_handle_frame_get(scene, strip) - handsize)) {
          if (select) {
            strip->flag |= SELECT | SEQ_RIGHTSEL;
          }
          else {
            /* Deselect the strip if it's left with no handles selected. */
            if ((strip->flag & SEQ_RIGHTSEL) && ((strip->flag & SEQ_LEFTSEL) == 0)) {
              strip->flag &= ~SELECT;
            }
            strip->flag &= ~SEQ_RIGHTSEL;
          }

          changed = true;
        }
        /* Left handle. */
        if (rectf.xmin < (seq::time_left_handle_frame_get(scene, strip) + handsize)) {
          if (select) {
            strip->flag |= SELECT | SEQ_LEFTSEL;
          }
          else {
            /* Deselect the strip if it's left with no handles selected. */
            if ((strip->flag & SEQ_LEFTSEL) && ((strip->flag & SEQ_RIGHTSEL) == 0)) {
              strip->flag &= ~SELECT;
            }
            strip->flag &= ~SEQ_LEFTSEL;
          }
        }

        changed = true;
      }

      /* Regular box selection. */
      else {
        SET_FLAG_FROM_TEST(strip->flag, select, SELECT);
        strip->flag &= ~(SEQ_LEFTSEL | SEQ_RIGHTSEL);
        changed = true;
      }

      const bool ignore_connections = RNA_boolean_get(op->ptr, "ignore_connections");
      if (!ignore_connections) {
        /* Propagate selection to connected strips. */
        StripSelection selection;
        selection.strip1 = strip;
        sequencer_select_connected_strips(selection);
      }
    }
  }

  if (!changed) {
    return OPERATOR_CANCELLED;
  }

  sequencer_select_do_updates(C, scene);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus sequencer_box_select_invoke(bContext *C,
                                                    wmOperator *op,
                                                    const wmEvent *event)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  const View2D *v2d = UI_view2d_fromcontext(C);
  ARegion *region = CTX_wm_region(C);

  if (region->regiontype == RGN_TYPE_PREVIEW && !sequencer_view_preview_only_poll(C)) {
    return OPERATOR_CANCELLED;
  }

  const bool tweak = RNA_boolean_get(op->ptr, "tweak");

  if (tweak) {
    int mval[2];
    float mouse_co[2];
    WM_event_drag_start_mval(event, region, mval);
    UI_view2d_region_to_view(v2d, mval[0], mval[1], &mouse_co[0], &mouse_co[1]);

    StripSelection selection = pick_strip_and_handle(scene, v2d, mouse_co);

    if (selection.strip1 != nullptr) {
      return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
    }
  }

  return WM_gesture_box_invoke(C, op, event);
}

void SEQUENCER_OT_select_box(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers. */
  ot->name = "Box Select";
  ot->idname = "SEQUENCER_OT_select_box";
  ot->description = "Select strips using box selection";

  /* API callbacks. */
  ot->invoke = sequencer_box_select_invoke;
  ot->exec = sequencer_box_select_exec;
  ot->modal = WM_gesture_box_modal;
  ot->cancel = WM_gesture_box_cancel;

  ot->poll = ED_operator_sequencer_active;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;

  /* Properties. */
  WM_operator_properties_gesture_box(ot);
  WM_operator_properties_select_operation_simple(ot);

  prop = RNA_def_boolean(
      ot->srna,
      "tweak",
      false,
      "Tweak",
      "Make box select pass through to sequence slide when the cursor is hovering on a strip");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(
      ot->srna, "include_handles", false, "Select Handles", "Select the strips and their handles");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna,
                         "ignore_connections",
                         false,
                         "Ignore Connections",
                         "Select strips individually whether or not they are connected");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Lasso Select Operator
 * \{ */
static bool do_lasso_select_is_origin_inside(const ARegion *region,
                                             const rcti *clip_rect,
                                             const Span<int2> mcoords,
                                             const float co_test[2])
{
  int co_screen[2];
  if (UI_view2d_view_to_region_clip(
          &region->v2d, co_test[0], co_test[1], &co_screen[0], &co_screen[1]) &&
      BLI_rcti_isect_pt_v(clip_rect, co_screen) &&
      BLI_lasso_is_point_inside(mcoords, co_screen[0], co_screen[1], V2D_IS_CLIPPED))
  {
    return true;
  }
  return false;
}

static bool rcti_in_lasso(const rcti rect, const Span<int2> mcoords)
{
  rcti lasso_rect;
  BLI_lasso_boundbox(&lasso_rect, mcoords);
  /* Check if edge of strip is in the lasso. */
  if (BLI_lasso_is_edge_inside(
          mcoords, rect.xmin, rect.ymin, rect.xmax, rect.ymin, V2D_IS_CLIPPED) ||
      BLI_lasso_is_edge_inside(
          mcoords, rect.xmax, rect.ymin, rect.xmax, rect.ymax, V2D_IS_CLIPPED) ||
      BLI_lasso_is_edge_inside(
          mcoords, rect.xmax, rect.ymax, rect.xmin, rect.ymax, V2D_IS_CLIPPED) ||
      BLI_lasso_is_edge_inside(
          mcoords, rect.xmin, rect.ymax, rect.xmin, rect.ymin, V2D_IS_CLIPPED))
  {
    return true;
  }

  /* Check if lasso is in the strip rect. Used when the lasso is only inside one strip. */
  if (BLI_rcti_inside_rcti(&rect, &lasso_rect)) {
    return true;
  }
  return false;
}

static bool do_lasso_select_timeline(bContext *C,
                                     const Span<int2> mcoords,
                                     ARegion *region,
                                     const eSelectOp sel_op)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = seq::editing_get(scene);

  bool changed = false;
  const bool select = (sel_op != SEL_OP_SUB);

  LISTBASE_FOREACH (Strip *, strip, &ed->seqbase) {
    rctf strip_rct;
    rcti region_rct;
    strip_rectf(scene, strip, &strip_rct);
    UI_view2d_view_to_region_clip(
        &region->v2d, strip_rct.xmin, strip_rct.ymin, &region_rct.xmin, &region_rct.ymin);
    UI_view2d_view_to_region_clip(
        &region->v2d, strip_rct.xmax, strip_rct.ymax, &region_rct.xmax, &region_rct.ymax);

    if (rcti_in_lasso(region_rct, mcoords)) {
      SET_FLAG_FROM_TEST(strip->flag, select, SELECT);
      changed = true;
    }
  }
  return changed;
}

static bool do_lasso_select_preview(bContext *C,
                                    Editing *ed,
                                    const Span<int2> mcoords,
                                    const eSelectOp sel_op)
{
  Scene *scene = CTX_data_scene(C);
  const ARegion *region = CTX_wm_region(C);

  bool changed = false;
  rcti rect;
  BLI_lasso_boundbox(&rect, mcoords);

  ListBase *seqbase = seq::active_seqbase_get(ed);
  ListBase *channels = seq::channels_displayed_get(ed);
  SpaceSeq *sseq = CTX_wm_space_seq(C);

  VectorSet strips = seq::query_rendered_strips(
      scene, channels, seqbase, scene->r.cfra, sseq->chanshown);
  for (Strip *strip : strips) {
    float2 origin = seq::image_transform_origin_offset_pixelspace_get(scene, strip);
    if (do_lasso_select_is_origin_inside(region, &rect, mcoords, origin)) {
      changed = true;
      if (ELEM(sel_op, SEL_OP_ADD, SEL_OP_SET)) {
        strip->flag |= SELECT;
      }
      else {
        BLI_assert(sel_op == SEL_OP_SUB);
        strip->flag &= ~SELECT;
      }
    }
  }

  return changed;
}

static wmOperatorStatus vse_lasso_select_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ARegion *region = CTX_wm_region(C);
  Array<int2> mcoords = WM_gesture_lasso_path_to_array(C, op);
  Editing *ed = seq::editing_get(scene);

  if (ed == nullptr) {
    return OPERATOR_CANCELLED;
  }

  if (mcoords.is_empty()) {
    return OPERATOR_PASS_THROUGH;
  }

  const eSelectOp sel_op = eSelectOp(RNA_enum_get(op->ptr, "mode"));
  const bool use_pre_deselect = SEL_OP_USE_PRE_DESELECT(sel_op);
  bool changed = false;

  if (use_pre_deselect) {
    changed |= deselect_all_strips(scene);
  }

  if (region->regiontype == RGN_TYPE_PREVIEW) {
    changed = do_lasso_select_preview(C, ed, mcoords, sel_op);
  }
  else {
    changed = do_lasso_select_timeline(C, mcoords, region, sel_op);
  }

  if (changed) {
    sequencer_select_do_updates(C, scene);
    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

void SEQUENCER_OT_select_lasso(wmOperatorType *ot)
{
  ot->name = "Lasso Select";
  ot->description = "Select strips using lasso selection";
  ot->idname = "SEQUENCER_OT_select_lasso";

  ot->invoke = WM_gesture_lasso_invoke;
  ot->modal = WM_gesture_lasso_modal;
  ot->exec = vse_lasso_select_exec;
  ot->poll = ED_operator_sequencer_active;
  ot->cancel = WM_gesture_lasso_cancel;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_DEPENDS_ON_CURSOR;

  /* properties */
  WM_operator_properties_gesture_lasso(ot);
  WM_operator_properties_select_operation_simple(ot);
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Circle Select Operator
 * \{ */
static bool strip_circle_select_radius_image_isect(const Scene *scene,
                                                   const Strip *strip,
                                                   const int *radius,
                                                   const float2 mval)
{
  float2 origin = seq::image_transform_origin_offset_pixelspace_get(scene, strip);

  float dx = origin.x - float(mval[0]);
  float dy = origin.y - float(mval[1]);
  float dist_sq = sqrt(dx * dx + dy * dy);

  return dist_sq <= *radius;
}

static void seq_circle_select_strip_from_preview(bContext *C,
                                                 int radius,
                                                 const float2 mval,
                                                 const eSelectOp mode)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = seq::editing_get(scene);
  ListBase *seqbase = seq::active_seqbase_get(ed);
  ListBase *channels = seq::channels_displayed_get(ed);
  SpaceSeq *sseq = CTX_wm_space_seq(C);

  VectorSet strips = seq::query_rendered_strips(
      scene, channels, seqbase, scene->r.cfra, sseq->chanshown);
  for (Strip *strip : strips) {
    if (!strip_circle_select_radius_image_isect(scene, strip, &radius, mval)) {
      continue;
    }

    if (ELEM(mode, SEL_OP_ADD, SEL_OP_SET)) {
      strip->flag |= SELECT;
    }
    else {
      BLI_assert(mode == SEL_OP_SUB);
      strip->flag &= ~SELECT;
    }
  }
}

static bool check_circle_intersection_in_timeline(const rctf *rect,
                                                  const float xy[2],
                                                  const float x_radius,
                                                  const float y_radius)
{
  float dx, dy;

  if (xy[0] >= rect->xmin && xy[0] <= rect->xmax) {
    dx = 0;
  }
  else {
    dx = (xy[0] < rect->xmin) ? (rect->xmin - xy[0]) : (xy[0] - rect->xmax);
  }

  if (xy[1] >= rect->ymin && xy[1] <= rect->ymax) {
    dy = 0;
  }
  else {
    dy = (xy[1] < rect->ymin) ? (rect->ymin - xy[1]) : (xy[1] - rect->ymax);
  }

  return ((dx * dx) / (x_radius * x_radius) + (dy * dy) / (y_radius * y_radius) <= 1.0f);
}
static wmOperatorStatus vse_circle_select_exec(bContext *C, wmOperator *op)
{
  const int radius = RNA_int_get(op->ptr, "radius");
  const int mval[2] = {RNA_int_get(op->ptr, "x"), RNA_int_get(op->ptr, "y")};
  wmGesture *gesture = static_cast<wmGesture *>(op->customdata);
  const eSelectOp sel_op = eSelectOp(RNA_enum_get(op->ptr, "mode"));

  Scene *scene = CTX_data_scene(C);
  View2D *v2d = UI_view2d_fromcontext(C);
  Editing *ed = seq::editing_get(scene);
  ARegion *region = CTX_wm_region(C);

  const bool use_pre_deselect = SEL_OP_USE_PRE_DESELECT(sel_op);

  if (use_pre_deselect && WM_gesture_is_modal_first(gesture)) {
    deselect_all_strips(scene);
    sequencer_select_do_updates(C, scene);
  }

  if (ed == nullptr) {
    return OPERATOR_CANCELLED;
  }

  float2 view_mval;
  UI_view2d_region_to_view(v2d, mval[0], mval[1], &view_mval[0], &view_mval[1]);
  float pixel_radius = radius / UI_view2d_scale_get_x(v2d);

  if (region->regiontype == RGN_TYPE_PREVIEW) {
    seq_circle_select_strip_from_preview(C, pixel_radius, view_mval, sel_op);
    sequencer_select_do_updates(C, scene);
    return OPERATOR_FINISHED;
  }

  float x_radius = radius / UI_view2d_scale_get_x(v2d);
  float y_radius = radius / UI_view2d_scale_get_y(v2d);
  bool changed = false;
  LISTBASE_FOREACH (Strip *, strip, ed->current_strips()) {
    rctf rq;
    strip_rectf(scene, strip, &rq);
    /* Use custom function to check the distance because in timeline the circle is a ellipse. */
    if (check_circle_intersection_in_timeline(&rq, view_mval, x_radius, y_radius)) {
      if (ELEM(sel_op, SEL_OP_ADD, SEL_OP_SET)) {
        strip->flag |= SELECT;
      }
      else {
        BLI_assert(sel_op == SEL_OP_SUB);
        strip->flag &= ~SELECT;
      }
      changed = true;

      const bool ignore_connections = RNA_boolean_get(op->ptr, "ignore_connections");
      if (!ignore_connections) {
        /* Propagate selection to connected strips. */
        StripSelection selection;
        selection.strip1 = strip;
        sequencer_select_connected_strips(selection);
      }
    }
  }
  if (changed) {
    sequencer_select_do_updates(C, scene);
  }
  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_select_circle(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Circle Select";
  ot->description = "Select strips using circle selection";
  ot->idname = "SEQUENCER_OT_select_circle";

  ot->invoke = WM_gesture_circle_invoke;
  ot->modal = WM_gesture_circle_modal;
  ot->exec = vse_circle_select_exec;

  ot->poll = ED_operator_sequencer_active;

  ot->get_name = ED_select_circle_get_name;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_gesture_circle(ot);
  WM_operator_properties_select_operation_simple(ot);

  prop = RNA_def_boolean(ot->srna,
                         "ignore_connections",
                         false,
                         "Ignore Connections",
                         "Select strips individually whether or not they are connected");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Grouped Operator
 * \{ */

enum {
  SEQ_SELECT_GROUP_TYPE,
  SEQ_SELECT_GROUP_TYPE_BASIC,
  SEQ_SELECT_GROUP_TYPE_EFFECT,
  SEQ_SELECT_GROUP_DATA,
  SEQ_SELECT_GROUP_EFFECT,
  SEQ_SELECT_GROUP_EFFECT_LINK,
  SEQ_SELECT_GROUP_OVERLAP,
};

static const EnumPropertyItem sequencer_prop_select_grouped_types[] = {
    {SEQ_SELECT_GROUP_TYPE, "TYPE", 0, "Type", "Shared strip type"},
    {SEQ_SELECT_GROUP_TYPE_BASIC,
     "TYPE_BASIC",
     0,
     "Global Type",
     "All strips of same basic type (graphical or sound)"},
    {SEQ_SELECT_GROUP_TYPE_EFFECT,
     "TYPE_EFFECT",
     0,
     "Effect Type",
     "Shared strip effect type (if active strip is not an effect one, select all non-effect "
     "strips)"},
    {SEQ_SELECT_GROUP_DATA, "DATA", 0, "Data", "Shared data (scene, image, sound, etc.)"},
    {SEQ_SELECT_GROUP_EFFECT, "EFFECT", 0, "Effect", "Shared effects"},
    {SEQ_SELECT_GROUP_EFFECT_LINK,
     "EFFECT_LINK",
     0,
     "Effect/Linked",
     "Other strips affected by the active one (sharing some time, and below or "
     "effect-assigned)"},
    {SEQ_SELECT_GROUP_OVERLAP, "OVERLAP", 0, "Overlap", "Overlapping time"},
    {0, nullptr, 0, nullptr, nullptr},
};

#define STRIP_IS_SOUND(_strip) (_strip->type == STRIP_TYPE_SOUND_RAM)

#define STRIP_USE_DATA(_strip) \
  (ELEM(_strip->type, STRIP_TYPE_SCENE, STRIP_TYPE_MOVIECLIP, STRIP_TYPE_MASK) || \
   STRIP_HAS_PATH(_strip))

#define STRIP_CHANNEL_CHECK(_strip, _chan) ELEM((_chan), 0, (_strip)->channel)

static bool select_grouped_type(Span<Strip *> strips,
                                ListBase * /*seqbase*/,
                                Strip *act_strip,
                                const int channel)
{
  bool changed = false;

  for (Strip *strip : strips) {
    if (STRIP_CHANNEL_CHECK(strip, channel) && strip->type == act_strip->type) {
      strip->flag |= SELECT;
      changed = true;
    }
  }

  return changed;
}

static bool select_grouped_type_basic(Span<Strip *> strips,
                                      ListBase * /*seqbase*/,
                                      Strip *act_strip,
                                      const int channel)
{
  bool changed = false;
  const bool is_sound = STRIP_IS_SOUND(act_strip);

  for (Strip *strip : strips) {
    if (STRIP_CHANNEL_CHECK(strip, channel) &&
        (is_sound ? STRIP_IS_SOUND(strip) : !STRIP_IS_SOUND(strip)))
    {
      strip->flag |= SELECT;
      changed = true;
    }
  }

  return changed;
}

static bool select_grouped_type_effect(Span<Strip *> strips,
                                       ListBase * /*seqbase*/,
                                       Strip *act_strip,
                                       const int channel)
{
  bool changed = false;
  const bool is_effect = act_strip->is_effect();

  for (Strip *strip : strips) {
    if (STRIP_CHANNEL_CHECK(strip, channel) &&
        (is_effect ? strip->is_effect() : !strip->is_effect()))
    {
      strip->flag |= SELECT;
      changed = true;
    }
  }

  return changed;
}

static bool select_grouped_data(Span<Strip *> strips,
                                ListBase * /*seqbase*/,
                                Strip *act_strip,
                                const int channel)
{
  bool changed = false;
  const char *dirpath = act_strip->data ? act_strip->data->dirpath : nullptr;

  if (!STRIP_USE_DATA(act_strip)) {
    return changed;
  }

  if (STRIP_HAS_PATH(act_strip) && dirpath) {
    for (Strip *strip : strips) {
      if (STRIP_CHANNEL_CHECK(strip, channel) && STRIP_HAS_PATH(strip) && strip->data &&
          STREQ(strip->data->dirpath, dirpath))
      {
        strip->flag |= SELECT;
        changed = true;
      }
    }
  }
  else if (act_strip->type == STRIP_TYPE_SCENE) {
    Scene *sce = act_strip->scene;
    for (Strip *strip : strips) {
      if (STRIP_CHANNEL_CHECK(strip, channel) && strip->type == STRIP_TYPE_SCENE &&
          strip->scene == sce)
      {
        strip->flag |= SELECT;
        changed = true;
      }
    }
  }
  else if (act_strip->type == STRIP_TYPE_MOVIECLIP) {
    MovieClip *clip = act_strip->clip;
    for (Strip *strip : strips) {
      if (STRIP_CHANNEL_CHECK(strip, channel) && strip->type == STRIP_TYPE_MOVIECLIP &&
          strip->clip == clip)
      {
        strip->flag |= SELECT;
        changed = true;
      }
    }
  }
  else if (act_strip->type == STRIP_TYPE_MASK) {
    Mask *mask = act_strip->mask;
    for (Strip *strip : strips) {
      if (STRIP_CHANNEL_CHECK(strip, channel) && strip->type == STRIP_TYPE_MASK &&
          strip->mask == mask)
      {
        strip->flag |= SELECT;
        changed = true;
      }
    }
  }

  return changed;
}

static bool select_grouped_effect(Span<Strip *> strips,
                                  ListBase * /*seqbase*/,
                                  Strip *act_strip,
                                  const int channel)
{
  bool changed = false;
  Set<StripType> effects;

  for (const Strip *strip : strips) {
    if (STRIP_CHANNEL_CHECK(strip, channel) && strip->is_effect() &&
        seq::relation_is_effect_of_strip(strip, act_strip))
    {
      effects.add(StripType(strip->type));
    }
  }

  for (Strip *strip : strips) {
    if (STRIP_CHANNEL_CHECK(strip, channel) && effects.contains(StripType(strip->type))) {
      if (strip->input1) {
        strip->input1->flag |= SELECT;
      }
      if (strip->input2) {
        strip->input2->flag |= SELECT;
      }
      changed = true;
    }
  }

  return changed;
}

static bool select_grouped_time_overlap(const Scene *scene,
                                        Span<Strip *> strips,
                                        ListBase * /*seqbase*/,
                                        Strip *act_strip)
{
  bool changed = false;

  for (Strip *strip : strips) {
    if (seq::time_left_handle_frame_get(scene, strip) <
            seq::time_right_handle_frame_get(scene, act_strip) &&
        seq::time_right_handle_frame_get(scene, strip) >
            seq::time_left_handle_frame_get(scene, act_strip))
    {
      strip->flag |= SELECT;
      changed = true;
    }
  }

  return changed;
}

/* Query strips that are in lower channel and intersect in time with strip_reference. */
static void query_lower_channel_strips(const Scene *scene,
                                       Strip *strip_reference,
                                       ListBase *seqbase,
                                       VectorSet<Strip *> &strips)
{
  LISTBASE_FOREACH (Strip *, strip_test, seqbase) {
    if (strip_test->channel > strip_reference->channel) {
      continue; /* Not lower channel. */
    }
    if (seq::time_right_handle_frame_get(scene, strip_test) <=
            seq::time_left_handle_frame_get(scene, strip_reference) ||
        seq::time_left_handle_frame_get(scene, strip_test) >=
            seq::time_right_handle_frame_get(scene, strip_reference))
    {
      continue; /* Not intersecting in time. */
    }
    strips.add(strip_test);
  }
}

/* Select all strips within time range and with lower channel of initial selection. Then select
 * effect chains of these strips. */
static bool select_grouped_effect_link(const Scene *scene,
                                       VectorSet<Strip *> strips,
                                       ListBase *seqbase,
                                       Strip * /*act_strip*/,
                                       const int /*channel*/)
{
  /* Get collection of strips. */
  strips.remove_if([&](Strip *strip) { return (strip->flag & SELECT) == 0; });
  const int selected_strip_count = strips.size();
  /* XXX: this uses scene as arg, so it does not work with iterator :( I had thought about this,
   * but expand function is just so useful... I can just add scene and inject it I guess. */
  seq::iterator_set_expand(scene, seqbase, strips, query_lower_channel_strips);
  seq::iterator_set_expand(scene, seqbase, strips, seq::query_strip_effect_chain);

  /* Check if other strips will be affected. */
  const bool changed = strips.size() > selected_strip_count;

  /* Actual logic. */
  for (Strip *strip : strips) {
    strip->flag |= SELECT;
  }

  return changed;
}

#undef STRIP_IS_SOUND
#undef STRIP_USE_DATA

static wmOperatorStatus sequencer_select_grouped_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  ListBase *seqbase = seq::active_seqbase_get(seq::editing_get(scene));
  Strip *act_strip = seq::select_active_get(scene);

  const bool is_preview = sequencer_view_has_preview_poll(C);
  if (is_preview && !sequencer_view_preview_only_poll(C)) {
    return OPERATOR_CANCELLED;
  }

  VectorSet strips = all_strips_from_context(C);

  if (act_strip == nullptr || (is_preview && !strips.contains(act_strip))) {
    BKE_report(op->reports, RPT_ERROR, "No active strip!");
    return OPERATOR_CANCELLED;
  }

  const int type = RNA_enum_get(op->ptr, "type");
  const int channel = RNA_boolean_get(op->ptr, "use_active_channel") ? act_strip->channel : 0;
  const bool extend = RNA_boolean_get(op->ptr, "extend");

  bool changed = false;

  if (!extend) {
    LISTBASE_FOREACH (Strip *, strip, seqbase) {
      strip->flag &= ~SELECT;
      changed = true;
    }
  }

  switch (type) {
    case SEQ_SELECT_GROUP_TYPE:
      changed |= select_grouped_type(strips, seqbase, act_strip, channel);
      break;
    case SEQ_SELECT_GROUP_TYPE_BASIC:
      changed |= select_grouped_type_basic(strips, seqbase, act_strip, channel);
      break;
    case SEQ_SELECT_GROUP_TYPE_EFFECT:
      changed |= select_grouped_type_effect(strips, seqbase, act_strip, channel);
      break;
    case SEQ_SELECT_GROUP_DATA:
      changed |= select_grouped_data(strips, seqbase, act_strip, channel);
      break;
    case SEQ_SELECT_GROUP_EFFECT:
      changed |= select_grouped_effect(strips, seqbase, act_strip, channel);
      break;
    case SEQ_SELECT_GROUP_EFFECT_LINK:
      changed |= select_grouped_effect_link(scene, strips, seqbase, act_strip, channel);
      break;
    case SEQ_SELECT_GROUP_OVERLAP:
      changed |= select_grouped_time_overlap(scene, strips, seqbase, act_strip);
      break;
    default:
      BLI_assert(0);
      break;
  }

  if (changed) {
    ED_outliner_select_sync_from_sequence_tag(C);
    WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);
    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

void SEQUENCER_OT_select_grouped(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Select Grouped";
  ot->idname = "SEQUENCER_OT_select_grouped";
  ot->description = "Select all strips grouped by various properties";

  /* API callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = sequencer_select_grouped_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties. */
  ot->prop = RNA_def_enum(ot->srna, "type", sequencer_prop_select_grouped_types, 0, "Type", "");
  RNA_def_boolean(ot->srna,
                  "extend",
                  false,
                  "Extend",
                  "Extend selection instead of deselecting everything first");
  RNA_def_boolean(ot->srna,
                  "use_active_channel",
                  false,
                  "Same Channel",
                  "Only consider strips on the same channel as the active one");
}

/** \} */

}  // namespace blender::ed::vse
