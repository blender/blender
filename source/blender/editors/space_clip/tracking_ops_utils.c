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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spclip
 */

#include "DNA_space_types.h"

#include "BKE_context.h"
#include "BKE_tracking.h"

#include "WM_api.h"
#include "WM_types.h"

#include "tracking_ops_intern.h" /* own include */

void clip_tracking_clear_invisible_track_selection(SpaceClip *sc, MovieClip *clip)
{
  ListBase *tracksbase = BKE_tracking_get_active_tracks(&clip->tracking);
  int hidden = 0;
  if ((sc->flag & SC_SHOW_MARKER_PATTERN) == 0) {
    hidden |= TRACK_AREA_PAT;
  }
  if ((sc->flag & SC_SHOW_MARKER_SEARCH) == 0) {
    hidden |= TRACK_AREA_SEARCH;
  }
  if (hidden) {
    for (MovieTrackingTrack *track = tracksbase->first; track != NULL; track = track->next) {
      if ((track->flag & TRACK_HIDDEN) == 0) {
        BKE_tracking_track_flag_clear(track, hidden, SELECT);
      }
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
