/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spclip
 */

#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_rect.h"

#include "BKE_context.hh"
#include "BKE_tracking.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_clip.hh"
#include "ED_screen.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "UI_view2d.hh"

#include "clip_intern.hh" /* own include */

static bool space_clip_dopesheet_poll(bContext *C)
{
  if (ED_space_clip_tracking_poll(C)) {
    SpaceClip *sc = CTX_wm_space_clip(C);

    if (sc->view == SC_VIEW_DOPESHEET) {
      ARegion *region = CTX_wm_region(C);

      return region->regiontype == RGN_TYPE_PREVIEW;
    }
  }

  return false;
}

/********************** select channel operator *********************/

static bool dopesheet_select_channel_poll(bContext *C)
{
  SpaceClip *sc = CTX_wm_space_clip(C);

  if (sc && sc->clip) {
    return sc->view == SC_VIEW_DOPESHEET;
  }

  return false;
}

static wmOperatorStatus dopesheet_select_channel_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);
  MovieTrackingDopesheet *dopesheet = &tracking->dopesheet;
  float location[2];
  const bool extend = RNA_boolean_get(op->ptr, "extend");
  int current_channel_index = 0, channel_index;
  const bool show_selected_only = (dopesheet->flag & TRACKING_DOPE_SELECTED_ONLY) != 0;

  RNA_float_get_array(op->ptr, "location", location);
  channel_index = -(location[1] - (CHANNEL_FIRST + CHANNEL_HEIGHT_HALF)) / CHANNEL_STEP;

  LISTBASE_FOREACH (MovieTrackingDopesheetChannel *, channel, &dopesheet->channels) {
    MovieTrackingTrack *track = channel->track;

    if (current_channel_index == channel_index) {
      if (extend) {
        track->flag ^= TRACK_DOPE_SEL;
      }
      else {
        track->flag |= TRACK_DOPE_SEL;
      }

      if (track->flag & TRACK_DOPE_SEL) {
        tracking_object->active_track = track;
        BKE_tracking_track_select(&tracking_object->tracks, track, TRACK_AREA_ALL, true);
      }
      else if (show_selected_only == false) {
        BKE_tracking_track_deselect(track, TRACK_AREA_ALL);
      }
    }
    else if (!extend) {
      track->flag &= ~TRACK_DOPE_SEL;
    }

    current_channel_index++;
  }

  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, nullptr);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus dopesheet_select_channel_invoke(bContext *C,
                                                        wmOperator *op,
                                                        const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  float location[2];

  UI_view2d_region_to_view(
      &region->v2d, event->mval[0], event->mval[1], &location[0], &location[1]);
  RNA_float_set_array(op->ptr, "location", location);

  return dopesheet_select_channel_exec(C, op);
}

void CLIP_OT_dopesheet_select_channel(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Channel";
  ot->description = "Select movie tracking channel";
  ot->idname = "CLIP_OT_dopesheet_select_channel";

  /* API callbacks. */
  ot->invoke = dopesheet_select_channel_invoke;
  ot->exec = dopesheet_select_channel_exec;
  ot->poll = dopesheet_select_channel_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_float_vector(ot->srna,
                       "location",
                       2,
                       nullptr,
                       -FLT_MAX,
                       FLT_MAX,
                       "Location",
                       "Mouse location to select channel",
                       -100.0f,
                       100.0f);
  RNA_def_boolean(ot->srna,
                  "extend",
                  false,
                  "Extend",
                  "Extend selection rather than clearing the existing selection");
}

/********************** View All operator *********************/

static wmOperatorStatus dopesheet_view_all_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  ARegion *region = CTX_wm_region(C);
  View2D *v2d = &region->v2d;
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingDopesheet *dopesheet = &tracking->dopesheet;
  int frame_min = INT_MAX, frame_max = INT_MIN;

  LISTBASE_FOREACH (MovieTrackingDopesheetChannel *, channel, &dopesheet->channels) {
    if (channel->segments) {
      frame_min = min_ii(frame_min, channel->segments[0]);
      frame_max = max_ii(frame_max, channel->segments[channel->tot_segment]);
    }
  }

  if (frame_min < frame_max) {
    float extra;

    v2d->cur.xmin = frame_min;
    v2d->cur.xmax = frame_max;

    /* we need an extra "buffer" factor on either side so that the endpoints are visible */
    extra = 0.01f * BLI_rctf_size_x(&v2d->cur);
    v2d->cur.xmin -= extra;
    v2d->cur.xmax += extra;

    ED_region_tag_redraw(region);
  }

  return OPERATOR_FINISHED;
}

void CLIP_OT_dopesheet_view_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Frame All";
  ot->description = "Reset viewable area to show full keyframe range";
  ot->idname = "CLIP_OT_dopesheet_view_all";

  /* API callbacks. */
  ot->exec = dopesheet_view_all_exec;
  ot->poll = space_clip_dopesheet_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
