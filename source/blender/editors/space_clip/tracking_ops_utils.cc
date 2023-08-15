/* SPDX-FileCopyrightText: 2016 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spclip
 */

#include "DNA_space_types.h"

#include "BKE_context.h"
#include "BKE_tracking.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "tracking_ops_intern.h" /* own include */

void clip_tracking_clear_invisible_track_selection(SpaceClip *sc, MovieClip *clip)
{
  int hidden = 0;
  if ((sc->flag & SC_SHOW_MARKER_PATTERN) == 0) {
    hidden |= TRACK_AREA_PAT;
  }
  if ((sc->flag & SC_SHOW_MARKER_SEARCH) == 0) {
    hidden |= TRACK_AREA_SEARCH;
  }
  if (!hidden) {
    return;
  }

  const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);
  LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
    if ((track->flag & TRACK_HIDDEN) == 0) {
      BKE_tracking_track_flag_clear(track, hidden, SELECT);
    }
  }
}

void clip_tracking_hide_cursor(bContext *C)
{
  wmWindow *win = CTX_wm_window(C);
  WM_cursor_set(win, WM_CURSOR_NONE);
}

void clip_tracking_show_cursor(bContext *C)
{
  wmWindow *win = CTX_wm_window(C);
  WM_cursor_set(win, WM_CURSOR_DEFAULT);
}
