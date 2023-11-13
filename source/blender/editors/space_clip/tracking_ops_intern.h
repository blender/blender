/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spclip
 */

#pragma once

#include "BLI_utildefines.h"

#include "BKE_tracking.h"

struct ListBase;
struct MovieClip;
struct SpaceClip;
struct bContext;
struct MovieTrackingTrack;
struct MovieTrackingMarker;
struct MovieTrackingPlaneTrack;
struct MovieTrackingPlaneMarker;

/* tracking_utils.c */

void clip_tracking_clear_invisible_track_selection(struct SpaceClip *sc, struct MovieClip *clip);

void clip_tracking_show_cursor(struct bContext *C);
void clip_tracking_hide_cursor(struct bContext *C);

/* tracking_select.h */

void ed_tracking_deselect_all_tracks(struct ListBase *tracks_base);
void ed_tracking_deselect_all_plane_tracks(struct ListBase *plane_tracks_base);

typedef struct TrackPickOptions {
  /* Ignore tracks which are not selected */
  bool selected_only;

  /* Ignore tracks which are locked. */
  bool unlocked_only;

  /* Ignore markers which are disabled. */
  bool enabled_only;
} TrackPickOptions;

BLI_INLINE TrackPickOptions ed_tracking_pick_options_defaults(void)
{
  TrackPickOptions options = {false};
  return options;
}

typedef enum eTrackPickAreaDetail {
  TRACK_PICK_AREA_DETAIL_NONE,

  /* Position of the marker (when area is TRACK_AREA_POINT).
   * Position of the pattern corner when area is TRACK_AREA_PAT and corner != -1. */
  TRACK_PICK_AREA_DETAIL_POSITION,

  /* Size and offset of the search area. */
  TRACK_PICK_AREA_DETAIL_SIZE,
  TRACK_PICK_AREA_DETAIL_OFFSET,

  /* "Widget" used to define pattern rotation and scale. */
  TRACK_PICK_AREA_DETAIL_TILT_SIZE,

  /* Edge of pattern or search area. */
  TRACK_PICK_AREA_DETAIL_EDGE,
} eTrackPickAreaDetail;

typedef struct PointTrackPick {
  struct MovieTrackingTrack *track;
  struct MovieTrackingMarker *marker;

  /* Picked area of the track. Is a single element from eTrackArea (no multiple choices are
   * possible). */
  eTrackArea area;
  eTrackPickAreaDetail area_detail;

  /* When a pattern corner is picked is 0-based index of the corner.
   * Otherwise is -1. */
  int corner_index;

  /* Distance to the pick measured in squared pixels. */
  float distance_px_squared;
} PointTrackPick;

/* Pick point track which is closest to the given coordinate.
 * Operates in the original non-stabilized and non-un-distorted coordinates. */
PointTrackPick ed_tracking_pick_point_track(const TrackPickOptions *options,
                                            struct bContext *C,
                                            const float co[2]);

/* Returns true when the pick did not pick anything. */
BLI_INLINE bool ed_tracking_point_track_pick_empty(const PointTrackPick *pick)
{
  return pick->track == NULL;
}

/* Check whether the point track pick corresponds to a part of the marker which can be sued
 * for slide operation. */
bool ed_tracking_point_track_pick_can_slide(const SpaceClip *space_clip,
                                            const PointTrackPick *pick);

typedef struct PlaneTrackPick {
  struct MovieTrackingPlaneTrack *plane_track;
  struct MovieTrackingPlaneMarker *plane_marker;

  /* When not equal to -1 denotes the index of the corner which was the closest to the requested
   * coordinate. */
  int corner_index;

  /* Distance to the pick measured in squared pixels. */
  float distance_px_squared;
} PlaneTrackPick;

/* Pick plane track which is closest to the given coordinate.
 * Operates in the original non-stabilized and non-un-distorted coordinates. */
PlaneTrackPick ed_tracking_pick_plane_track(const TrackPickOptions *options,
                                            struct bContext *C,
                                            const float co[2]);

/* Returns true when the pick did not pick anything. */
BLI_INLINE bool ed_tracking_plane_track_pick_empty(const PlaneTrackPick *pick)
{
  return pick->plane_track == NULL;
}

/* Check whether the plane track pick corresponds to a part of the marker which can be sued
 * for slide operation. */
bool ed_tracking_plane_track_pick_can_slide(const PlaneTrackPick *pick);

typedef struct TrackingPick {
  /* NOTE: At maximum one of these picks will have a track. */
  PointTrackPick point_track_pick;
  PlaneTrackPick plane_track_pick;
} TrackingPick;

/* Pick closest point or plane track (whichever is the closest to the given coordinate).
 * Operates in the original non-stabilized and non-un-distorted coordinates. */
TrackingPick ed_tracking_pick_closest(const TrackPickOptions *options,
                                      bContext *C,
                                      const float co[2]);

/* Returns true when the pick did not pick anything. */
BLI_INLINE bool ed_tracking_pick_empty(const TrackingPick *pick)
{
  return ed_tracking_point_track_pick_empty(&pick->point_track_pick) &&
         ed_tracking_plane_track_pick_empty(&pick->plane_track_pick);
}

/* Check whether any of the pick can be used for the marker slide operation. */
BLI_INLINE bool ed_tracking_pick_can_slide(const struct SpaceClip *space_clip,
                                           const struct TrackingPick *pick)
{
  return ed_tracking_point_track_pick_can_slide(space_clip, &pick->point_track_pick) ||
         ed_tracking_plane_track_pick_can_slide(&pick->plane_track_pick);
}
