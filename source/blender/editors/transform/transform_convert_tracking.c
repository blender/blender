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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup edtransform
 */

#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_movieclip.h"
#include "BKE_node.h"
#include "BKE_tracking.h"

#include "ED_clip.h"

#include "WM_api.h"
#include "WM_types.h"

#include "transform.h"
#include "transform_convert.h"

typedef struct TransDataTracking {
  int mode, flag;

  /* tracks transformation from main window */
  int area;
  const float *relative, *loc;
  float soffset[2], srelative[2];
  float offset[2];

  float (*smarkers)[2];
  int markersnr;
  int framenr;
  MovieTrackingMarker *markers;

  /* marker transformation from curves editor */
  float *prev_pos, scale;
  short coord;

  MovieTrackingTrack *track;
  MovieTrackingPlaneTrack *plane_track;
} TransDataTracking;

enum transDataTracking_Mode {
  transDataTracking_ModeTracks = 0,
  transDataTracking_ModeCurves = 1,
  transDataTracking_ModePlaneTracks = 2,
};

/* -------------------------------------------------------------------- */
/** \name Clip Editor Motion Tracking Transform Creation
 * \{ */

typedef struct TransformInitContext {
  SpaceClip *space_clip;

  TransInfo *t;
  TransDataContainer *tc;

  /* NOTE: These pointers will be `nullptr` during counting step.
   * This means, that the transformation data initialization functions are to increment
   * `tc->data_len` instead of filling in the transformation data when these pointers are
   * `nullptr`. For simplicity, check the `current.td` against `nullptr`.
   * Do not `tc->data_len` when filling in the transformation data. */
  struct {
    TransData *td;
    TransData2D *td2d;
    TransDataTracking *tdt;
  } current;
} TransformInitContext;

static void markerToTransDataInit(TransformInitContext *init_context,
                                  MovieTrackingTrack *track,
                                  MovieTrackingMarker *marker,
                                  int area,
                                  float loc[2],
                                  const float rel[2],
                                  const float off[2],
                                  const float aspect[2])
{
  TransData *td = init_context->current.td;
  TransData2D *td2d = init_context->current.td2d;
  TransDataTracking *tdt = init_context->current.tdt;

  if (td == NULL) {
    init_context->tc->data_len++;
    return;
  }

  int anchor = area == TRACK_AREA_POINT && off;

  tdt->flag = marker->flag;
  tdt->framenr = marker->framenr;
  tdt->mode = transDataTracking_ModeTracks;

  if (anchor) {
    td2d->loc[0] = rel[0] * aspect[0]; /* hold original location */
    td2d->loc[1] = rel[1] * aspect[1];

    tdt->loc = loc;
    td2d->loc2d = loc; /* current location */
  }
  else {
    td2d->loc[0] = loc[0] * aspect[0]; /* hold original location */
    td2d->loc[1] = loc[1] * aspect[1];

    td2d->loc2d = loc; /* current location */
  }
  td2d->loc[2] = 0.0f;

  tdt->relative = rel;
  tdt->area = area;

  tdt->markersnr = track->markersnr;
  tdt->markers = track->markers;
  tdt->track = track;

  if (rel) {
    if (!anchor) {
      td2d->loc[0] += rel[0] * aspect[0];
      td2d->loc[1] += rel[1] * aspect[1];
    }

    copy_v2_v2(tdt->srelative, rel);
  }

  if (off) {
    copy_v2_v2(tdt->soffset, off);
  }

  td->flag = 0;
  td->loc = td2d->loc;
  copy_v3_v3(td->iloc, td->loc);

  // copy_v3_v3(td->center, td->loc);
  td->flag |= TD_INDIVIDUAL_SCALE;
  td->center[0] = marker->pos[0] * aspect[0];
  td->center[1] = marker->pos[1] * aspect[1];

  memset(td->axismtx, 0, sizeof(td->axismtx));
  td->axismtx[2][2] = 1.0f;

  td->ext = NULL;
  td->val = NULL;

  td->flag |= TD_SELECTED;
  td->dist = 0.0;

  unit_m3(td->mtx);
  unit_m3(td->smtx);

  init_context->current.td++;
  init_context->current.td2d++;
  init_context->current.tdt++;
}

static void trackToTransData(TransformInitContext *init_context,
                             const int framenr,
                             MovieTrackingTrack *track,
                             const float aspect[2])
{
  MovieTrackingMarker *marker = BKE_tracking_marker_ensure(track, framenr);

  markerToTransDataInit(init_context,
                        track,
                        marker,
                        TRACK_AREA_POINT,
                        track->offset,
                        marker->pos,
                        track->offset,
                        aspect);

  if (track->flag & SELECT) {
    markerToTransDataInit(
        init_context, track, marker, TRACK_AREA_POINT, marker->pos, NULL, NULL, aspect);
  }

  if (track->pat_flag & SELECT) {
    int a;

    for (a = 0; a < 4; a++) {
      markerToTransDataInit(init_context,
                            track,
                            marker,
                            TRACK_AREA_PAT,
                            marker->pattern_corners[a],
                            marker->pos,
                            NULL,
                            aspect);
    }
  }

  if (track->search_flag & SELECT) {
    markerToTransDataInit(init_context,
                          track,
                          marker,
                          TRACK_AREA_SEARCH,
                          marker->search_min,
                          marker->pos,
                          NULL,
                          aspect);

    markerToTransDataInit(init_context,
                          track,
                          marker,
                          TRACK_AREA_SEARCH,
                          marker->search_max,
                          marker->pos,
                          NULL,
                          aspect);
  }

  if (init_context->current.td != NULL) {
    marker->flag &= ~(MARKER_DISABLED | MARKER_TRACKED);
  }
}

static void trackToTransDataIfNeeded(TransformInitContext *init_context,
                                     const int framenr,
                                     MovieTrackingTrack *track,
                                     const float aspect[2])
{
  if (!TRACK_VIEW_SELECTED(init_context->space_clip, track)) {
    return;
  }
  if (track->flag & TRACK_LOCKED) {
    return;
  }
  trackToTransData(init_context, framenr, track, aspect);
}

static void planeMarkerToTransDataInit(TransformInitContext *init_context,
                                       MovieTrackingPlaneTrack *plane_track,
                                       MovieTrackingPlaneMarker *plane_marker,
                                       float corner[2],
                                       const float aspect[2])
{
  TransData *td = init_context->current.td;
  TransData2D *td2d = init_context->current.td2d;
  TransDataTracking *tdt = init_context->current.tdt;

  if (td == NULL) {
    init_context->tc->data_len++;
    return;
  }

  tdt->flag = plane_marker->flag;
  tdt->framenr = plane_marker->framenr;
  tdt->mode = transDataTracking_ModePlaneTracks;
  tdt->plane_track = plane_track;

  td2d->loc[0] = corner[0] * aspect[0]; /* hold original location */
  td2d->loc[1] = corner[1] * aspect[1];

  td2d->loc2d = corner; /* current location */
  td2d->loc[2] = 0.0f;

  td->flag = 0;
  td->loc = td2d->loc;
  copy_v3_v3(td->iloc, td->loc);
  copy_v3_v3(td->center, td->loc);

  memset(td->axismtx, 0, sizeof(td->axismtx));
  td->axismtx[2][2] = 1.0f;

  td->ext = NULL;
  td->val = NULL;

  td->flag |= TD_SELECTED;
  td->dist = 0.0;

  unit_m3(td->mtx);
  unit_m3(td->smtx);

  init_context->current.td++;
  init_context->current.td2d++;
  init_context->current.tdt++;
}

static void planeTrackToTransData(TransformInitContext *init_context,
                                  const int framenr,
                                  MovieTrackingPlaneTrack *plane_track,
                                  const float aspect[2])
{
  MovieTrackingPlaneMarker *plane_marker = BKE_tracking_plane_marker_ensure(plane_track, framenr);

  for (int i = 0; i < 4; i++) {
    planeMarkerToTransDataInit(
        init_context, plane_track, plane_marker, plane_marker->corners[i], aspect);
  }

  if (init_context->current.td != NULL) {
    plane_marker->flag &= ~PLANE_MARKER_TRACKED;
  }
}

static void planeTrackToTransDataIfNeeded(TransformInitContext *init_context,
                                          const int framenr,
                                          MovieTrackingPlaneTrack *plane_track,
                                          const float aspect[2])
{
  if (!PLANE_TRACK_VIEW_SELECTED(plane_track)) {
    return;
  }
  planeTrackToTransData(init_context, framenr, plane_track, aspect);
}

static void transDataTrackingFree(TransInfo *UNUSED(t),
                                  TransDataContainer *UNUSED(tc),
                                  TransCustomData *custom_data)
{
  if (custom_data->data) {
    TransDataTracking *tdt = custom_data->data;
    if (tdt->smarkers) {
      MEM_freeN(tdt->smarkers);
    }

    MEM_freeN(tdt);
    custom_data->data = NULL;
  }
}

static void createTransTrackingTracksData(bContext *C, TransInfo *t)
{
  SpaceClip *space_clip = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(space_clip);
  const ListBase *tracksbase = BKE_tracking_get_active_tracks(&clip->tracking);
  const ListBase *plane_tracks_base = BKE_tracking_get_active_plane_tracks(&clip->tracking);
  const int framenr = ED_space_clip_get_clip_frame_number(space_clip);

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  TransformInitContext init_context = {NULL};
  init_context.space_clip = space_clip;
  init_context.t = t;
  init_context.tc = tc;

  /* Count required transformation data. */

  tc->data_len = 0;

  LISTBASE_FOREACH (MovieTrackingTrack *, track, tracksbase) {
    trackToTransDataIfNeeded(&init_context, framenr, track, t->aspect);
  }

  LISTBASE_FOREACH (MovieTrackingPlaneTrack *, plane_track, plane_tracks_base) {
    planeTrackToTransDataIfNeeded(&init_context, framenr, plane_track, t->aspect);
  }

  if (tc->data_len == 0) {
    return;
  }

  tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransTracking TransData");
  tc->data_2d = MEM_callocN(tc->data_len * sizeof(TransData2D), "TransTracking TransData2D");
  tc->custom.type.data = MEM_callocN(tc->data_len * sizeof(TransDataTracking),
                                     "TransTracking TransDataTracking");
  tc->custom.type.free_cb = transDataTrackingFree;

  init_context.current.td = tc->data;
  init_context.current.td2d = tc->data_2d;
  init_context.current.tdt = tc->custom.type.data;

  /* Create actual transformation data. */

  LISTBASE_FOREACH (MovieTrackingTrack *, track, tracksbase) {
    trackToTransDataIfNeeded(&init_context, framenr, track, t->aspect);
  }

  LISTBASE_FOREACH (MovieTrackingPlaneTrack *, plane_track, plane_tracks_base) {
    planeTrackToTransDataIfNeeded(&init_context, framenr, plane_track, t->aspect);
  }
}

static void markerToTransCurveDataInit(TransData *td,
                                       TransData2D *td2d,
                                       TransDataTracking *tdt,
                                       MovieTrackingTrack *track,
                                       MovieTrackingMarker *marker,
                                       MovieTrackingMarker *prev_marker,
                                       short coord,
                                       float size)
{
  float frames_delta = (marker->framenr - prev_marker->framenr);

  tdt->flag = marker->flag;
  marker->flag &= ~MARKER_TRACKED;

  tdt->mode = transDataTracking_ModeCurves;
  tdt->coord = coord;
  tdt->scale = 1.0f / size * frames_delta;
  tdt->prev_pos = prev_marker->pos;
  tdt->track = track;

  /* calculate values depending on marker's speed */
  td2d->loc[0] = marker->framenr;
  td2d->loc[1] = (marker->pos[coord] - prev_marker->pos[coord]) * size / frames_delta;
  td2d->loc[2] = 0.0f;

  td2d->loc2d = marker->pos; /* current location */

  td->flag = 0;
  td->loc = td2d->loc;
  copy_v3_v3(td->center, td->loc);
  copy_v3_v3(td->iloc, td->loc);

  memset(td->axismtx, 0, sizeof(td->axismtx));
  td->axismtx[2][2] = 1.0f;

  td->ext = NULL;
  td->val = NULL;

  td->flag |= TD_SELECTED;
  td->dist = 0.0;

  unit_m3(td->mtx);
  unit_m3(td->smtx);
}

static void createTransTrackingCurvesData(bContext *C, TransInfo *t)
{
  TransData *td;
  TransData2D *td2d;
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  ListBase *tracksbase = BKE_tracking_get_active_tracks(&clip->tracking);
  MovieTrackingTrack *track;
  MovieTrackingMarker *marker, *prev_marker;
  TransDataTracking *tdt;
  int i, width, height;

  BKE_movieclip_get_size(clip, &sc->user, &width, &height);

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  /* count */
  tc->data_len = 0;

  if ((sc->flag & SC_SHOW_GRAPH_TRACKS_MOTION) == 0) {
    return;
  }

  track = tracksbase->first;
  while (track) {
    if (TRACK_VIEW_SELECTED(sc, track) && (track->flag & TRACK_LOCKED) == 0) {
      for (i = 1; i < track->markersnr; i++) {
        marker = &track->markers[i];
        prev_marker = &track->markers[i - 1];

        if ((marker->flag & MARKER_DISABLED) || (prev_marker->flag & MARKER_DISABLED)) {
          continue;
        }

        if (marker->flag & MARKER_GRAPH_SEL_X) {
          tc->data_len += 1;
        }

        if (marker->flag & MARKER_GRAPH_SEL_Y) {
          tc->data_len += 1;
        }
      }
    }

    track = track->next;
  }

  if (tc->data_len == 0) {
    return;
  }

  td = tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransTracking TransData");
  td2d = tc->data_2d = MEM_callocN(tc->data_len * sizeof(TransData2D),
                                   "TransTracking TransData2D");
  tc->custom.type.data = tdt = MEM_callocN(tc->data_len * sizeof(TransDataTracking),
                                           "TransTracking TransDataTracking");
  tc->custom.type.free_cb = transDataTrackingFree;

  /* create actual data */
  track = tracksbase->first;
  while (track) {
    if (TRACK_VIEW_SELECTED(sc, track) && (track->flag & TRACK_LOCKED) == 0) {
      for (i = 1; i < track->markersnr; i++) {
        marker = &track->markers[i];
        prev_marker = &track->markers[i - 1];

        if ((marker->flag & MARKER_DISABLED) || (prev_marker->flag & MARKER_DISABLED)) {
          continue;
        }

        if (marker->flag & MARKER_GRAPH_SEL_X) {
          markerToTransCurveDataInit(
              td, td2d, tdt, track, marker, &track->markers[i - 1], 0, width);
          td += 1;
          td2d += 1;
          tdt += 1;
        }

        if (marker->flag & MARKER_GRAPH_SEL_Y) {
          markerToTransCurveDataInit(
              td, td2d, tdt, track, marker, &track->markers[i - 1], 1, height);

          td += 1;
          td2d += 1;
          tdt += 1;
        }
      }
    }

    track = track->next;
  }
}

void createTransTrackingData(bContext *C, TransInfo *t)
{
  ARegion *region = CTX_wm_region(C);
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  int width, height;

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  tc->data_len = 0;

  if (!clip) {
    return;
  }

  BKE_movieclip_get_size(clip, &sc->user, &width, &height);

  if (width == 0 || height == 0) {
    return;
  }

  if (region->regiontype == RGN_TYPE_PREVIEW) {
    /* transformation was called from graph editor */
    createTransTrackingCurvesData(C, t);
  }
  else {
    createTransTrackingTracksData(C, t);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name recalc Motion Tracking TransData
 * \{ */

static void cancelTransTracking(TransInfo *t)
{
  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);
  TransDataTracking *tdt_array = tc->custom.type.data;

  int i = 0;
  while (i < tc->data_len) {
    TransDataTracking *tdt = &tdt_array[i];

    if (tdt->mode == transDataTracking_ModeTracks) {
      MovieTrackingTrack *track = tdt->track;
      MovieTrackingMarker *marker = BKE_tracking_marker_get_exact(track, tdt->framenr);

      BLI_assert(marker != NULL);

      marker->flag = tdt->flag;

      if (track->flag & SELECT) {
        i++;
      }

      if (track->pat_flag & SELECT) {
        i += 4;
      }

      if (track->search_flag & SELECT) {
        i += 2;
      }
    }
    else if (tdt->mode == transDataTracking_ModeCurves) {
      MovieTrackingTrack *track = tdt->track;
      MovieTrackingMarker *marker, *prev_marker;
      int a;

      for (a = 1; a < track->markersnr; a++) {
        marker = &track->markers[a];
        prev_marker = &track->markers[a - 1];

        if ((marker->flag & MARKER_DISABLED) || (prev_marker->flag & MARKER_DISABLED)) {
          continue;
        }

        if (marker->flag & (MARKER_GRAPH_SEL_X | MARKER_GRAPH_SEL_Y)) {
          marker->flag = tdt->flag;
        }
      }
    }
    else if (tdt->mode == transDataTracking_ModePlaneTracks) {
      MovieTrackingPlaneTrack *plane_track = tdt->plane_track;
      MovieTrackingPlaneMarker *plane_marker = BKE_tracking_plane_marker_get_exact(plane_track,
                                                                                   tdt->framenr);

      BLI_assert(plane_marker != NULL);

      plane_marker->flag = tdt->flag;
      i += 3;
    }

    i++;
  }
}

static void flushTransTracking(TransInfo *t)
{
  TransData *td;
  TransData2D *td2d;
  TransDataTracking *tdt;
  int a;

  if (t->state == TRANS_CANCEL) {
    cancelTransTracking(t);
  }

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  /* flush to 2d vector from internally used 3d vector */
  for (a = 0, td = tc->data, td2d = tc->data_2d, tdt = tc->custom.type.data; a < tc->data_len;
       a++, td2d++, td++, tdt++) {
    if (tdt->mode == transDataTracking_ModeTracks) {
      float loc2d[2];

      if (t->mode == TFM_ROTATION && tdt->area == TRACK_AREA_SEARCH) {
        continue;
      }

      loc2d[0] = td2d->loc[0] / t->aspect[0];
      loc2d[1] = td2d->loc[1] / t->aspect[1];

      if (t->flag & T_ALT_TRANSFORM) {
        if (t->mode == TFM_RESIZE) {
          if (tdt->area != TRACK_AREA_PAT) {
            continue;
          }
        }
        else if (t->mode == TFM_TRANSLATION) {
          if (tdt->area == TRACK_AREA_POINT && tdt->relative) {
            float d[2], d2[2];

            if (!tdt->smarkers) {
              tdt->smarkers = MEM_callocN(sizeof(*tdt->smarkers) * tdt->markersnr,
                                          "flushTransTracking markers");
              for (a = 0; a < tdt->markersnr; a++) {
                copy_v2_v2(tdt->smarkers[a], tdt->markers[a].pos);
              }
            }

            sub_v2_v2v2(d, loc2d, tdt->soffset);
            sub_v2_v2(d, tdt->srelative);

            sub_v2_v2v2(d2, loc2d, tdt->srelative);

            for (a = 0; a < tdt->markersnr; a++) {
              add_v2_v2v2(tdt->markers[a].pos, tdt->smarkers[a], d2);
            }

            negate_v2_v2(td2d->loc2d, d);
          }
        }
      }

      if (tdt->area != TRACK_AREA_POINT || tdt->relative == NULL) {
        td2d->loc2d[0] = loc2d[0];
        td2d->loc2d[1] = loc2d[1];

        if (tdt->relative) {
          sub_v2_v2(td2d->loc2d, tdt->relative);
        }
      }
    }
    else if (tdt->mode == transDataTracking_ModeCurves) {
      td2d->loc2d[tdt->coord] = tdt->prev_pos[tdt->coord] + td2d->loc[1] * tdt->scale;
    }
    else if (tdt->mode == transDataTracking_ModePlaneTracks) {
      td2d->loc2d[0] = td2d->loc[0] / t->aspect[0];
      td2d->loc2d[1] = td2d->loc[1] / t->aspect[1];
    }
  }
}

/* helper for recalcData() - for Movie Clip transforms */
void recalcData_tracking(TransInfo *t)
{
  SpaceClip *sc = t->area->spacedata.first;

  if (ED_space_clip_check_show_trackedit(sc)) {
    MovieClip *clip = ED_space_clip_get_clip(sc);
    ListBase *tracksbase = BKE_tracking_get_active_tracks(&clip->tracking);
    MovieTrackingTrack *track;
    int framenr = ED_space_clip_get_clip_frame_number(sc);

    flushTransTracking(t);

    track = tracksbase->first;
    while (track) {
      if (TRACK_VIEW_SELECTED(sc, track) && (track->flag & TRACK_LOCKED) == 0) {
        MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);

        if (t->mode == TFM_TRANSLATION) {
          if (TRACK_AREA_SELECTED(track, TRACK_AREA_PAT)) {
            BKE_tracking_marker_clamp(marker, CLAMP_PAT_POS);
          }
          if (TRACK_AREA_SELECTED(track, TRACK_AREA_SEARCH)) {
            BKE_tracking_marker_clamp(marker, CLAMP_SEARCH_POS);
          }
        }
        else if (t->mode == TFM_RESIZE) {
          if (TRACK_AREA_SELECTED(track, TRACK_AREA_PAT)) {
            BKE_tracking_marker_clamp(marker, CLAMP_PAT_DIM);
          }
          if (TRACK_AREA_SELECTED(track, TRACK_AREA_SEARCH)) {
            BKE_tracking_marker_clamp(marker, CLAMP_SEARCH_DIM);
          }
        }
        else if (t->mode == TFM_ROTATION) {
          if (TRACK_AREA_SELECTED(track, TRACK_AREA_PAT)) {
            BKE_tracking_marker_clamp(marker, CLAMP_PAT_POS);
          }
        }
      }

      track = track->next;
    }

    DEG_id_tag_update(&clip->id, 0);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Special After Transform Tracking
 * \{ */

void special_aftertrans_update__movieclip(bContext *C, TransInfo *t)
{
  SpaceClip *sc = t->area->spacedata.first;
  MovieClip *clip = ED_space_clip_get_clip(sc);
  ListBase *plane_tracks_base = BKE_tracking_get_active_plane_tracks(&clip->tracking);
  const int framenr = ED_space_clip_get_clip_frame_number(sc);
  /* Update coordinates of modified plane tracks. */
  LISTBASE_FOREACH (MovieTrackingPlaneTrack *, plane_track, plane_tracks_base) {
    bool do_update = false;
    if (plane_track->flag & PLANE_TRACK_HIDDEN) {
      continue;
    }
    do_update |= PLANE_TRACK_VIEW_SELECTED(plane_track) != 0;
    if (do_update == false) {
      if ((plane_track->flag & PLANE_TRACK_AUTOKEY) == 0) {
        int i;
        for (i = 0; i < plane_track->point_tracksnr; i++) {
          MovieTrackingTrack *track = plane_track->point_tracks[i];
          if (TRACK_VIEW_SELECTED(sc, track)) {
            do_update = true;
            break;
          }
        }
      }
    }
    if (do_update) {
      BKE_tracking_track_plane_from_existing_motion(plane_track, framenr);
    }
  }
  if (t->scene->nodetree != NULL) {
    /* Tracks can be used for stabilization nodes,
     * flush update for such nodes.
     */
    nodeUpdateID(t->scene->nodetree, &clip->id);
    WM_event_add_notifier(C, NC_SCENE | ND_NODES, NULL);
  }
}

/** \} */
