/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * This file contains implementation of plane tracker.
 */

#include "MEM_guardedalloc.h"

#include "DNA_movieclip_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_tracking.h"

#include "libmv-capi.h"

typedef double Vec2[2];

static int point_markers_correspondences_on_both_image(
    MovieTrackingPlaneTrack *plane_track, int frame1, int frame2, Vec2 **r_x1, Vec2 **r_x2)
{
  Vec2 *x1, *x2;

  *r_x1 = x1 = MEM_cnew_array<Vec2>(plane_track->point_tracksnr, "point correspondences x1");
  *r_x2 = x2 = MEM_cnew_array<Vec2>(plane_track->point_tracksnr, "point correspondences x2");

  int correspondence_index = 0;
  for (int i = 0; i < plane_track->point_tracksnr; i++) {
    MovieTrackingTrack *point_track = plane_track->point_tracks[i];
    MovieTrackingMarker *point_marker1, *point_marker2;

    point_marker1 = BKE_tracking_marker_get_exact(point_track, frame1);
    point_marker2 = BKE_tracking_marker_get_exact(point_track, frame2);

    if (point_marker1 != nullptr && point_marker2 != nullptr) {
      /* Here conversion from float to double happens. */
      x1[correspondence_index][0] = point_marker1->pos[0];
      x1[correspondence_index][1] = point_marker1->pos[1];

      x2[correspondence_index][0] = point_marker2->pos[0];
      x2[correspondence_index][1] = point_marker2->pos[1];

      correspondence_index++;
    }
  }

  return correspondence_index;
}

/* NOTE: frame number should be in clip space, not scene space */
static void track_plane_from_existing_motion(MovieTrackingPlaneTrack *plane_track,
                                             int start_frame,
                                             int direction,
                                             bool retrack)
{
  MovieTrackingPlaneMarker *start_plane_marker = BKE_tracking_plane_marker_get(plane_track,
                                                                               start_frame);
  MovieTrackingPlaneMarker *keyframe_plane_marker = nullptr;
  MovieTrackingPlaneMarker new_plane_marker;
  int frame_delta = direction > 0 ? 1 : -1;

  if (plane_track->flag & PLANE_TRACK_AUTOKEY) {
    /* Find a keyframe in given direction. */
    for (int current_frame = start_frame;; current_frame += frame_delta) {
      MovieTrackingPlaneMarker *next_plane_marker = BKE_tracking_plane_marker_get_exact(
          plane_track, current_frame + frame_delta);

      if (next_plane_marker == nullptr) {
        break;
      }

      if ((next_plane_marker->flag & PLANE_MARKER_TRACKED) == 0) {
        keyframe_plane_marker = next_plane_marker;
        break;
      }
    }
  }
  else {
    start_plane_marker->flag |= PLANE_MARKER_TRACKED;
  }

  new_plane_marker = *start_plane_marker;
  new_plane_marker.flag |= PLANE_MARKER_TRACKED;

  for (int current_frame = start_frame;; current_frame += frame_delta) {
    MovieTrackingPlaneMarker *next_plane_marker = BKE_tracking_plane_marker_get_exact(
        plane_track, current_frame + frame_delta);
    Vec2 *x1, *x2;
    double H_double[3][3];
    float H[3][3];

    /* As soon as we meet keyframed plane, we stop updating the sequence. */
    if (next_plane_marker && (next_plane_marker->flag & PLANE_MARKER_TRACKED) == 0) {
      /* Don't override keyframes if track is in auto-keyframe mode */
      if (plane_track->flag & PLANE_TRACK_AUTOKEY) {
        break;
      }
    }

    const int num_correspondences = point_markers_correspondences_on_both_image(
        plane_track, current_frame, current_frame + frame_delta, &x1, &x2);
    if (num_correspondences < 4) {
      MEM_freeN(x1);
      MEM_freeN(x2);
      break;
    }

    libmv_homography2DFromCorrespondencesEuc(x1, x2, num_correspondences, H_double);

    copy_m3_m3d(H, H_double);

    for (int i = 0; i < 4; i++) {
      float vec[3] = {0.0f, 0.0f, 1.0f}, vec2[3];
      copy_v2_v2(vec, new_plane_marker.corners[i]);

      /* Apply homography */
      mul_v3_m3v3(vec2, H, vec);

      /* Normalize. */
      vec2[0] /= vec2[2];
      vec2[1] /= vec2[2];

      copy_v2_v2(new_plane_marker.corners[i], vec2);
    }

    new_plane_marker.framenr = current_frame + frame_delta;

    if (!retrack && keyframe_plane_marker && next_plane_marker &&
        (plane_track->flag & PLANE_TRACK_AUTOKEY))
    {
      float fac = (float(next_plane_marker->framenr) - start_plane_marker->framenr) /
                  (float(keyframe_plane_marker->framenr) - start_plane_marker->framenr);

      fac = 3 * fac * fac - 2 * fac * fac * fac;

      for (int i = 0; i < 4; i++) {
        interp_v2_v2v2(new_plane_marker.corners[i],
                       new_plane_marker.corners[i],
                       next_plane_marker->corners[i],
                       fac);
      }
    }

    BKE_tracking_plane_marker_insert(plane_track, &new_plane_marker);

    MEM_freeN(x1);
    MEM_freeN(x2);
  }
}

void BKE_tracking_track_plane_from_existing_motion(MovieTrackingPlaneTrack *plane_track,
                                                   int start_frame)
{
  track_plane_from_existing_motion(plane_track, start_frame, 1, false);
  track_plane_from_existing_motion(plane_track, start_frame, -1, false);
}

static MovieTrackingPlaneMarker *find_plane_keyframe(MovieTrackingPlaneTrack *plane_track,
                                                     int start_frame,
                                                     int direction)
{
  MovieTrackingPlaneMarker *plane_marker = BKE_tracking_plane_marker_get(plane_track, start_frame);
  int index = plane_marker - plane_track->markers;
  int frame_delta = direction > 0 ? 1 : -1;

  while (index >= 0 && index < plane_track->markersnr) {
    if ((plane_marker->flag & PLANE_MARKER_TRACKED) == 0) {
      return plane_marker;
    }
    plane_marker += frame_delta;
  }

  return nullptr;
}

void BKE_tracking_retrack_plane_from_existing_motion_at_segment(
    MovieTrackingPlaneTrack *plane_track, int start_frame)
{
  MovieTrackingPlaneMarker *prev_plane_keyframe, *next_plane_keyframe;

  prev_plane_keyframe = find_plane_keyframe(plane_track, start_frame, -1);
  next_plane_keyframe = find_plane_keyframe(plane_track, start_frame, 1);

  if (prev_plane_keyframe != nullptr && next_plane_keyframe != nullptr) {
    /* First we track from left keyframe to the right one without any blending. */
    track_plane_from_existing_motion(plane_track, prev_plane_keyframe->framenr, 1, true);

    /* And then we track from the right keyframe to the left one, so shape blends in nicely */
    track_plane_from_existing_motion(plane_track, next_plane_keyframe->framenr, -1, false);
  }
  else if (prev_plane_keyframe != nullptr) {
    track_plane_from_existing_motion(plane_track, prev_plane_keyframe->framenr, 1, true);
  }
  else if (next_plane_keyframe != nullptr) {
    track_plane_from_existing_motion(plane_track, next_plane_keyframe->framenr, -1, true);
  }
}

BLI_INLINE void float_corners_to_double(/*const*/ float corners[4][2], double double_corners[4][2])
{
  copy_v2db_v2fl(double_corners[0], corners[0]);
  copy_v2db_v2fl(double_corners[1], corners[1]);
  copy_v2db_v2fl(double_corners[2], corners[2]);
  copy_v2db_v2fl(double_corners[3], corners[3]);
}

void BKE_tracking_homography_between_two_quads(/*const*/ float reference_corners[4][2],
                                               /*const*/ float corners[4][2],
                                               float H[3][3])
{
  Vec2 x1[4], x2[4];
  double H_double[3][3];

  float_corners_to_double(reference_corners, x1);
  float_corners_to_double(corners, x2);

  libmv_homography2DFromCorrespondencesEuc(x1, x2, 4, H_double);

  copy_m3_m3d(H, H_double);
}
