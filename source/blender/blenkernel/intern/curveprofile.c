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
 * Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_curve_types.h"
#include "DNA_curveprofile_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_task.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BKE_curve.h"
#include "BKE_curveprofile.h"
#include "BKE_fcurve.h"

#include "BLO_read_write.h"

void BKE_curveprofile_free_data(CurveProfile *profile)
{
  MEM_SAFE_FREE(profile->path);
  MEM_SAFE_FREE(profile->table);
  MEM_SAFE_FREE(profile->segments);
}

void BKE_curveprofile_free(CurveProfile *profile)
{
  if (profile) {
    BKE_curveprofile_free_data(profile);
    MEM_freeN(profile);
  }
}

void BKE_curveprofile_copy_data(CurveProfile *target, const CurveProfile *profile)
{
  *target = *profile;

  target->path = MEM_dupallocN(profile->path);
  target->table = MEM_dupallocN(profile->table);
  target->segments = MEM_dupallocN(profile->segments);

  /* Update the reference the points have to the profile. */
  for (int i = 0; i < target->path_len; i++) {
    target->path[i].profile = target;
  }
}

CurveProfile *BKE_curveprofile_copy(const CurveProfile *profile)
{
  if (profile) {
    CurveProfile *new_prdgt = MEM_dupallocN(profile);
    BKE_curveprofile_copy_data(new_prdgt, profile);
    return new_prdgt;
  }
  return NULL;
}

/**
 * Move a point's handle, accounting for the alignment of handles with the HD_ALIGN type.
 *
 * \param handle_1 Whether to move the 1st or 2nd control point.
 * \param new_location The *relative* change in the handle's position.
 * \note Requires #BKE_curveprofile_update call after.
 * \return Whether the handle moved from its start position.
 */
bool BKE_curveprofile_move_handle(struct CurveProfilePoint *point,
                                  const bool handle_1,
                                  const bool snap,
                                  const float delta[2])
{
  short handle_type = (handle_1) ? point->h1 : point->h2;
  float *handle_location = (handle_1) ? &point->h1_loc[0] : &point->h2_loc[0];

  float start_position[2];
  copy_v2_v2(start_position, handle_location);

  /* Don't move the handle if it's not a free handle type. */
  if (!ELEM(handle_type, HD_FREE, HD_ALIGN)) {
    return false;
  }

  /* Move the handle. */
  handle_location[0] += delta ? delta[0] : 0.0f;
  handle_location[1] += delta ? delta[1] : 0.0f;
  if (snap) {
    handle_location[0] = 0.125f * roundf(8.0f * handle_location[0]);
    handle_location[1] = 0.125f * roundf(8.0f * handle_location[1]);
  }

  /* Move the other handle if they are aligned. */
  if (handle_type == HD_ALIGN) {
    short other_handle_type = (handle_1) ? point->h2 : point->h1;
    if (other_handle_type == HD_ALIGN) {
      float *other_handle_location = (handle_1) ? &point->h2_loc[0] : &point->h1_loc[0];
      other_handle_location[0] = 2.0f * point->x - handle_location[0];
      other_handle_location[1] = 2.0f * point->y - handle_location[1];
    }
  }

  if (!equals_v2v2(handle_location, start_position)) {
    return true;
  }
  return false;
}

/**
 * Moves a control point, accounting for clipping and snapping, and moving free handles.
 *
 * \param snap Whether to snap the point to the grid
 * \param new_location The *relative* change of the point's location.
 * \return Whether the point moved from its start position.
 * \note Requires #BKE_curveprofile_update call after.
 */
bool BKE_curveprofile_move_point(struct CurveProfile *profile,
                                 struct CurveProfilePoint *point,
                                 const bool snap,
                                 const float delta[2])
{
  float origx = point->x;
  float origy = point->y;

  point->x += delta[0];
  point->y += delta[1];
  if (snap) {
    point->x = 0.125f * roundf(8.0f * point->x);
    point->y = 0.125f * roundf(8.0f * point->y);
  }

  /* Clip here instead to test clipping here to stop handles from moving too. */
  if (profile->flag & PROF_USE_CLIP) {
    point->x = max_ff(point->x, profile->clip_rect.xmin);
    point->x = min_ff(point->x, profile->clip_rect.xmax);
    point->y = max_ff(point->y, profile->clip_rect.ymin);
    point->y = min_ff(point->y, profile->clip_rect.ymax);
  }

  /* Also move free handles even when they aren't selected. */
  if (ELEM(point->h1, HD_FREE, HD_ALIGN)) {
    point->h1_loc[0] += point->x - origx;
    point->h1_loc[1] += point->y - origy;
  }
  if (ELEM(point->h2, HD_FREE, HD_ALIGN)) {
    point->h2_loc[0] += point->x - origx;
    point->h2_loc[1] += point->y - origy;
  }

  if (point->x != origx || point->y != origy) {
    return true;
  }
  return false;
}

/**
 * Removes a specific point from the path of control points.
 * \note Requires #BKE_curveprofile_update call after.
 */
bool BKE_curveprofile_remove_point(CurveProfile *profile, CurveProfilePoint *point)
{
  CurveProfilePoint *pts;

  /* Must have 2 points minimum. */
  if (profile->path_len <= 2) {
    return false;
  }

  /* Input point must be within the array. */
  if (!(point > profile->path && point < profile->path + profile->path_len)) {
    return false;
  }

  pts = MEM_mallocN(sizeof(CurveProfilePoint) * profile->path_len, "path points");

  uint i_delete = (uint)(point - profile->path);

  /* Copy the before and after the deleted point. */
  memcpy(pts, profile->path, sizeof(CurveProfilePoint) * i_delete);
  memcpy(pts + i_delete,
         profile->path + i_delete + 1,
         sizeof(CurveProfilePoint) * (profile->path_len - i_delete - 1));

  MEM_freeN(profile->path);
  profile->path = pts;
  profile->path_len -= 1;
  return true;
}

/**
 * Removes every point in the widget with the supplied flag set, except for the first and last.
 *
 * \param flag: #CurveProfilePoint.flag.
 *
 * \note Requires #BKE_curveprofile_update call after.
 */
void BKE_curveprofile_remove_by_flag(CurveProfile *profile, const short flag)
{
  int i_old, i_new, n_removed = 0;

  /* Copy every point without the flag into the new path. */
  CurveProfilePoint *new_pts = MEM_mallocN(sizeof(CurveProfilePoint) * profile->path_len,
                                           "profile path");

  /* Build the new list without any of the points with the flag. Keep the first and last points. */
  new_pts[0] = profile->path[0];
  for (i_old = 1, i_new = 1; i_old < profile->path_len - 1; i_old++) {
    if (!(profile->path[i_old].flag & flag)) {
      new_pts[i_new] = profile->path[i_old];
      i_new++;
    }
    else {
      n_removed++;
    }
  }
  new_pts[i_new] = profile->path[i_old];

  MEM_freeN(profile->path);
  profile->path = new_pts;
  profile->path_len -= n_removed;
}

/**
 * Adds a new point at the specified location. The choice for which points to place the new vertex
 * between is made by checking which control point line segment is closest to the new point and
 * placing the new vertex in between that segment's points.
 *
 * \note Requires #BKE_curveprofile_update call after.
 */
CurveProfilePoint *BKE_curveprofile_insert(CurveProfile *profile, float x, float y)
{
  CurveProfilePoint *new_pt = NULL;
  float new_loc[2] = {x, y};

  /* Don't add more control points  than the maximum size of the higher resolution table. */
  if (profile->path_len == PROF_TABLE_MAX - 1) {
    return NULL;
  }

  /* Find the index at the line segment that's closest to the new position. */
  float distance;
  float min_distance = FLT_MAX;
  int i_insert = 0;
  for (int i = 0; i < profile->path_len - 1; i++) {
    float loc1[2] = {profile->path[i].x, profile->path[i].y};
    float loc2[2] = {profile->path[i + 1].x, profile->path[i + 1].y};

    distance = dist_squared_to_line_segment_v2(new_loc, loc1, loc2);
    if (distance < min_distance) {
      min_distance = distance;
      i_insert = i + 1;
    }
  }

  /* Insert the new point at the location we found and copy all of the old points in as well. */
  profile->path_len++;
  CurveProfilePoint *new_pts = MEM_mallocN(sizeof(CurveProfilePoint) * profile->path_len,
                                           "profile path");
  for (int i_new = 0, i_old = 0; i_new < profile->path_len; i_new++) {
    if (i_new != i_insert) {
      /* Insert old points. */
      memcpy(&new_pts[i_new], &profile->path[i_old], sizeof(CurveProfilePoint));
      new_pts[i_new].flag &= ~PROF_SELECT; /* Deselect old points. */
      i_old++;
    }
    else {
      /* Insert new point. */
      new_pts[i_new].x = x;
      new_pts[i_new].y = y;
      new_pts[i_new].flag = PROF_SELECT;
      new_pt = &new_pts[i_new];
      /* Set handles of new point based on its neighbors. */
      if (new_pts[i_new - 1].h2 == HD_VECT && profile->path[i_insert].h1 == HD_VECT) {
        new_pt->h1 = new_pt->h2 = HD_VECT;
      }
      else {
        new_pt->h1 = new_pt->h2 = HD_AUTO;
      }
      /* Give new point a reference to the profile. */
      new_pt->profile = profile;
    }
  }

  /* Free the old path and use the new one. */
  MEM_freeN(profile->path);
  profile->path = new_pts;
  return new_pt;
}

/**
 * Sets the handle type of the selected control points.
 * \param type_* Handle type for the first handle. HD_VECT, HD_AUTO, HD_FREE, or HD_ALIGN.
 * \note Requires #BKE_curveprofile_update call after.
 */
void BKE_curveprofile_selected_handle_set(CurveProfile *profile, int type_1, int type_2)
{
  for (int i = 0; i < profile->path_len; i++) {
    if (ELEM(profile->path[i].flag, PROF_SELECT, PROF_H1_SELECT, PROF_H2_SELECT)) {
      profile->path[i].h1 = type_1;
      profile->path[i].h2 = type_2;

      if (type_1 == HD_ALIGN && type_2 == HD_ALIGN) {
        /* Align the handles. */
        BKE_curveprofile_move_handle(&profile->path[i], true, false, NULL);
      }
    }
  }
}

/**
 * Flips the profile across the diagonal so that its orientation is reversed.
 *
 * \note Requires #BKE_curveprofile_update call after.
 */
void BKE_curveprofile_reverse(CurveProfile *profile)
{
  /* When there are only two points, reversing shouldn't do anything. */
  if (profile->path_len == 2) {
    return;
  }
  CurveProfilePoint *new_pts = MEM_mallocN(sizeof(CurveProfilePoint) * profile->path_len,
                                           "profile path");
  /* Mirror the new points across the y = x line */
  for (int i = 0; i < profile->path_len; i++) {
    int i_reversed = profile->path_len - i - 1;
    BLI_assert(i_reversed >= 0);
    new_pts[i_reversed].x = profile->path[i].y;
    new_pts[i_reversed].y = profile->path[i].x;
    new_pts[i_reversed].flag = profile->path[i].flag;
    new_pts[i_reversed].h1 = profile->path[i].h2;
    new_pts[i_reversed].h2 = profile->path[i].h1;
    new_pts[i_reversed].profile = profile;

    /* Mirror free handles, they can't be recalculated. */
    if (ELEM(profile->path[i].h1, HD_FREE, HD_ALIGN)) {
      new_pts[i_reversed].h1_loc[0] = profile->path[i].h2_loc[1];
      new_pts[i_reversed].h1_loc[1] = profile->path[i].h2_loc[0];
    }
    if (ELEM(profile->path[i].h2, HD_FREE, HD_ALIGN)) {
      new_pts[i_reversed].h2_loc[0] = profile->path[i].h1_loc[1];
      new_pts[i_reversed].h2_loc[1] = profile->path[i].h1_loc[0];
    }
  }

  /* Free the old points and use the new ones */
  MEM_freeN(profile->path);
  profile->path = new_pts;
}

/**
 * Builds a quarter circle profile with space on each side for 'support loops.'
 */
static void CurveProfile_build_supports(CurveProfile *profile)
{
  int n = profile->path_len;

  profile->path[0].x = 1.0;
  profile->path[0].y = 0.0;
  profile->path[0].flag = 0;
  profile->path[0].h1 = HD_VECT;
  profile->path[0].h2 = HD_VECT;
  profile->path[1].x = 1.0;
  profile->path[1].y = 0.5;
  profile->path[1].flag = 0;
  profile->path[1].h1 = HD_VECT;
  profile->path[1].h2 = HD_VECT;
  for (int i = 1; i < n - 2; i++) {
    profile->path[i + 1].x = 1.0f - (0.5f * (1.0f - cosf((float)((i / (float)(n - 3))) * M_PI_2)));
    profile->path[i + 1].y = 0.5f + 0.5f * sinf((float)((i / (float)(n - 3)) * M_PI_2));
    profile->path[i + 1].flag = 0;
    profile->path[i + 1].h1 = HD_AUTO;
    profile->path[i + 1].h2 = HD_AUTO;
  }
  profile->path[n - 2].x = 0.5;
  profile->path[n - 2].y = 1.0;
  profile->path[n - 2].flag = 0;
  profile->path[n - 2].h1 = HD_VECT;
  profile->path[n - 2].h2 = HD_VECT;
  profile->path[n - 1].x = 0.0;
  profile->path[n - 1].y = 1.0;
  profile->path[n - 1].flag = 0;
  profile->path[n - 1].h1 = HD_VECT;
  profile->path[n - 1].h2 = HD_VECT;
}

/**
 * Puts the widgets control points in a step pattern.
 * Uses vector handles for each point.
 */
static void CurveProfile_build_steps(CurveProfile *profile)
{
  int n, step_x, step_y;
  float n_steps_x, n_steps_y;

  n = profile->path_len;

  /* Special case for two points to avoid dividing by zero later. */
  if (n == 2) {
    profile->path[0].x = 1.0f;
    profile->path[0].y = 0.0f;
    profile->path[0].flag = 0;
    profile->path[0].h1 = HD_VECT;
    profile->path[0].h2 = HD_VECT;
    profile->path[1].x = 0.0f;
    profile->path[1].y = 1.0f;
    profile->path[1].flag = 0;
    profile->path[1].h1 = HD_VECT;
    profile->path[1].h2 = HD_VECT;
    return;
  }

  n_steps_x = (n % 2 == 0) ? n : (n - 1);
  n_steps_y = (n % 2 == 0) ? (n - 2) : (n - 1);

  for (int i = 0; i < n; i++) {
    step_x = (i + 1) / 2;
    step_y = i / 2;
    profile->path[i].x = 1.0f - ((float)(2 * step_x) / n_steps_x);
    profile->path[i].y = (float)(2 * step_y) / n_steps_y;
    profile->path[i].flag = 0;
    profile->path[i].h1 = HD_VECT;
    profile->path[i].h2 = HD_VECT;
  }
}

/**
 * Shorthand helper function for setting location and interpolation of a point.
 */
static void point_init(CurveProfilePoint *point, float x, float y, short flag, char h1, char h2)
{
  point->x = x;
  point->y = y;
  point->flag = flag;
  point->h1 = h1;
  point->h2 = h2;
}

/**
 * Resets the profile to the current preset.
 *
 * \note Requires #BKE_curveprofile_update call after.
 */
void BKE_curveprofile_reset(CurveProfile *profile)
{
  if (profile->path) {
    MEM_freeN(profile->path);
  }

  int preset = profile->preset;
  switch (preset) {
    case PROF_PRESET_LINE:
      profile->path_len = 2;
      break;
    case PROF_PRESET_SUPPORTS:
      /* Use a dynamic number of control points for the widget's profile. */
      if (profile->segments_len < 4) {
        /* But always use enough points to at least build the support points. */
        profile->path_len = 5;
      }
      else {
        profile->path_len = profile->segments_len + 1;
      }
      break;
    case PROF_PRESET_CORNICE:
      profile->path_len = 13;
      break;
    case PROF_PRESET_CROWN:
      profile->path_len = 11;
      break;
    case PROF_PRESET_STEPS:
      /* Also use dynamic number of control points based on the set number of segments. */
      if (profile->segments_len == 0) {
        /* totsegments hasn't been set-- use the number of control points for 8 steps. */
        profile->path_len = 17;
      }
      else {
        profile->path_len = profile->segments_len + 1;
      }
      break;
  }

  profile->path = MEM_callocN(sizeof(CurveProfilePoint) * profile->path_len, "profile path");

  switch (preset) {
    case PROF_PRESET_LINE:
      point_init(&profile->path[0], 1.0f, 0.0f, 0, HD_AUTO, HD_AUTO);
      point_init(&profile->path[1], 0.0f, 1.0f, 0, HD_AUTO, HD_AUTO);
      break;
    case PROF_PRESET_SUPPORTS:
      CurveProfile_build_supports(profile);
      break;
    case PROF_PRESET_CORNICE:
      point_init(&profile->path[0], 1.0f, 0.0f, 0, HD_VECT, HD_VECT);
      point_init(&profile->path[1], 1.0f, 0.125f, 0, HD_VECT, HD_VECT);
      point_init(&profile->path[2], 0.92f, 0.16f, 0, HD_AUTO, HD_AUTO);
      point_init(&profile->path[3], 0.875f, 0.25f, 0, HD_VECT, HD_VECT);
      point_init(&profile->path[4], 0.8f, 0.25f, 0, HD_VECT, HD_VECT);
      point_init(&profile->path[5], 0.733f, 0.433f, 0, HD_AUTO, HD_AUTO);
      point_init(&profile->path[6], 0.582f, 0.522f, 0, HD_AUTO, HD_AUTO);
      point_init(&profile->path[7], 0.4f, 0.6f, 0, HD_AUTO, HD_AUTO);
      point_init(&profile->path[8], 0.289f, 0.727f, 0, HD_AUTO, HD_AUTO);
      point_init(&profile->path[9], 0.25f, 0.925f, 0, HD_VECT, HD_VECT);
      point_init(&profile->path[10], 0.175f, 0.925f, 0, HD_VECT, HD_VECT);
      point_init(&profile->path[11], 0.175f, 1.0f, 0, HD_VECT, HD_VECT);
      point_init(&profile->path[12], 0.0f, 1.0f, 0, HD_VECT, HD_VECT);
      break;
    case PROF_PRESET_CROWN:
      point_init(&profile->path[0], 1.0f, 0.0f, 0, HD_VECT, HD_VECT);
      point_init(&profile->path[1], 1.0f, 0.25f, 0, HD_VECT, HD_VECT);
      point_init(&profile->path[2], 0.75f, 0.25f, 0, HD_VECT, HD_VECT);
      point_init(&profile->path[3], 0.75f, 0.325f, 0, HD_VECT, HD_VECT);
      point_init(&profile->path[4], 0.925f, 0.4f, 0, HD_AUTO, HD_AUTO);
      point_init(&profile->path[5], 0.975f, 0.5f, 0, HD_AUTO, HD_AUTO);
      point_init(&profile->path[6], 0.94f, 0.65f, 0, HD_AUTO, HD_AUTO);
      point_init(&profile->path[7], 0.85f, 0.75f, 0, HD_AUTO, HD_AUTO);
      point_init(&profile->path[8], 0.75f, 0.875f, 0, HD_AUTO, HD_AUTO);
      point_init(&profile->path[9], 0.7f, 1.0f, 0, HD_VECT, HD_VECT);
      point_init(&profile->path[10], 0.0f, 1.0f, 0, HD_VECT, HD_VECT);
      break;
    case PROF_PRESET_STEPS:
      CurveProfile_build_steps(profile);
      break;
  }

  profile->flag &= ~PROF_DIRTY_PRESET;

  /* Ensure each point has a reference to the profile. */
  for (int i = 0; i < profile->path_len; i++) {
    profile->path[i].profile = profile;
  }

  if (profile->table) {
    MEM_freeN(profile->table);
    profile->table = NULL;
  }
}

/**
 * Helper for 'curve_profile_create' samples.
 * Returns whether both handles that make up the edge are vector handles.
 */
static bool is_curved_edge(BezTriple *bezt, int i)
{
  return (bezt[i].h2 != HD_VECT || bezt[i + 1].h1 != HD_VECT);
}

/**
 * Used to set bezier handle locations in the sample creation process. Reduced copy of
 * #calchandleNurb_intern code in curve.c, mostly changed by removing the third dimension.
 */
static void calchandle_profile(BezTriple *bezt, const BezTriple *prev, const BezTriple *next)
{
#define point_handle1 ((point_loc)-3)
#define point_handle2 ((point_loc) + 3)

  const float *prev_loc, *next_loc;
  float *point_loc;
  float pt[3];
  float len, len_a, len_b;
  float dvec_a[2], dvec_b[2];

  if (bezt->h1 == 0 && bezt->h2 == 0) {
    return;
  }

  point_loc = bezt->vec[1];

  if (prev == NULL) {
    next_loc = next->vec[1];
    pt[0] = 2.0f * point_loc[0] - next_loc[0];
    pt[1] = 2.0f * point_loc[1] - next_loc[1];
    prev_loc = pt;
  }
  else {
    prev_loc = prev->vec[1];
  }

  if (next == NULL) {
    prev_loc = prev->vec[1];
    pt[0] = 2.0f * point_loc[0] - prev_loc[0];
    pt[1] = 2.0f * point_loc[1] - prev_loc[1];
    next_loc = pt;
  }
  else {
    next_loc = next->vec[1];
  }

  sub_v2_v2v2(dvec_a, point_loc, prev_loc);
  sub_v2_v2v2(dvec_b, next_loc, point_loc);

  len_a = len_v2(dvec_a);
  len_b = len_v2(dvec_b);

  if (len_a == 0.0f) {
    len_a = 1.0f;
  }
  if (len_b == 0.0f) {
    len_b = 1.0f;
  }

  if (bezt->h1 == HD_AUTO || bezt->h2 == HD_AUTO) { /* auto */
    float tvec[2];
    tvec[0] = dvec_b[0] / len_b + dvec_a[0] / len_a;
    tvec[1] = dvec_b[1] / len_b + dvec_a[1] / len_a;

    len = len_v2(tvec) * 2.5614f;
    if (len != 0.0f) {

      if (bezt->h1 == HD_AUTO) {
        len_a /= len;
        madd_v2_v2v2fl(point_handle1, point_loc, tvec, -len_a);
      }
      if (bezt->h2 == HD_AUTO) {
        len_b /= len;
        madd_v2_v2v2fl(point_handle2, point_loc, tvec, len_b);
      }
    }
  }

  if (bezt->h1 == HD_VECT) { /* vector */
    madd_v2_v2v2fl(point_handle1, point_loc, dvec_a, -1.0f / 3.0f);
  }
  if (bezt->h2 == HD_VECT) {
    madd_v2_v2v2fl(point_handle2, point_loc, dvec_b, 1.0f / 3.0f);
  }
#undef point_handle1
#undef point_handle2
}

/**
 * Helper function for 'BKE_CurveProfile_create_samples.' Calculates the angle between the
 * handles on the inside of the edge starting at index i. A larger angle means the edge is
 * more curved.
 * \param i_edge: The start index of the edge to calculate the angle for.
 */
static float bezt_edge_handle_angle(const BezTriple *bezt, int i_edge)
{
  /* Find the direction of the handles that define this edge along the direction of the path. */
  float start_handle_direction[2], end_handle_direction[2];
  /* Handle 2 - point location. */
  sub_v2_v2v2(start_handle_direction, bezt[i_edge].vec[2], bezt[i_edge].vec[1]);
  /* Point location - handle 1. */
  sub_v2_v2v2(end_handle_direction, bezt[i_edge + 1].vec[1], bezt[i_edge + 1].vec[0]);

  float angle = angle_v2v2(start_handle_direction, end_handle_direction);
  return angle;
}

/** Struct to sort curvature of control point edges. */
typedef struct {
  /** The index of the corresponding bezier point. */
  int bezt_index;
  /** The curvature of the edge with the above index. */
  float bezt_curvature;
} CurvatureSortPoint;

/**
 * Helper function for 'BKE_curveprofile_create_samples' for sorting edges based on curvature.
 */
static int sort_points_curvature(const void *in_a, const void *in_b)
{
  const CurvatureSortPoint *a = (const CurvatureSortPoint *)in_a;
  const CurvatureSortPoint *b = (const CurvatureSortPoint *)in_b;

  if (a->bezt_curvature > b->bezt_curvature) {
    return 0;
  }
  else {
    return 1;
  }
}

/**
 * Used for sampling curves along the profile's path. Any points more than the number of
 * user-defined points will be evenly distributed among the curved edges.
 * Then the remainders will be distributed to the most curved edges.
 *
 * \param n_segments: The number of segments to sample along the path. It must be higher than the
 * number of points used to define the profile (profile->path_len).
 * \param sample_straight_edges: Whether to sample points between vector handle control points. If
 * this is true and there are only vector edges the straight edges will still be sampled.
 * \param r_samples: An array of points to put the sampled positions. Must have length n_segments.
 * \return r_samples: Fill the array with the sampled locations and if the point corresponds
 * to a control point, its handle type.
 */
void BKE_curveprofile_create_samples(CurveProfile *profile,
                                     int n_segments,
                                     bool sample_straight_edges,
                                     CurveProfilePoint *r_samples)
{
  BezTriple *bezt;
  int i, n_left, n_common, i_sample, n_curved_edges;
  int *n_samples;
  CurvatureSortPoint *curve_sorted;
  int totpoints = profile->path_len;
  int totedges = totpoints - 1;

  BLI_assert(n_segments > 0);

  /* Create Bezier points for calculating the higher resolution path. */
  bezt = MEM_callocN(sizeof(BezTriple) * totpoints, "beztarr");
  for (i = 0; i < totpoints; i++) {
    bezt[i].vec[1][0] = profile->path[i].x;
    bezt[i].vec[1][1] = profile->path[i].y;
    bezt[i].h1 = profile->path[i].h1;
    bezt[i].h2 = profile->path[i].h2;
    /* Copy handle locations if the handle type is free. */
    if (ELEM(profile->path[i].h1, HD_FREE, HD_ALIGN)) {
      bezt[i].vec[0][0] = profile->path[i].h1_loc[0];
      bezt[i].vec[0][1] = profile->path[i].h1_loc[1];
    }
    if (ELEM(profile->path[i].h1, HD_FREE, HD_ALIGN)) {
      bezt[i].vec[2][0] = profile->path[i].h2_loc[0];
      bezt[i].vec[2][1] = profile->path[i].h2_loc[1];
    }
  }
  /* Get handle positions for the non-free bezier points. */
  calchandle_profile(&bezt[0], NULL, &bezt[1]);
  for (i = 1; i < totpoints - 1; i++) {
    calchandle_profile(&bezt[i], &bezt[i - 1], &bezt[i + 1]);
  }
  calchandle_profile(&bezt[totpoints - 1], &bezt[totpoints - 2], NULL);

  /* Copy the handle locations back to the control points. */
  for (i = 0; i < totpoints; i++) {
    profile->path[i].h1_loc[0] = bezt[i].vec[0][0];
    profile->path[i].h1_loc[1] = bezt[i].vec[0][1];
    profile->path[i].h2_loc[0] = bezt[i].vec[2][0];
    profile->path[i].h2_loc[1] = bezt[i].vec[2][1];
  }

  /* Create a list of edge indices with the most curved at the start, least curved at the end. */
  curve_sorted = MEM_callocN(sizeof(CurvatureSortPoint) * totedges, "curve sorted");
  for (i = 0; i < totedges; i++) {
    curve_sorted[i].bezt_index = i;
    /* Calculate the curvature of each edge once for use when sorting for curvature. */
    curve_sorted[i].bezt_curvature = bezt_edge_handle_angle(bezt, i);
  }
  qsort(curve_sorted, (size_t)totedges, sizeof(CurvatureSortPoint), sort_points_curvature);

  /* Assign the number of sampled points for each edge. */
  n_samples = MEM_callocN(sizeof(int) * totedges, "create samples numbers");
  int n_added = 0;
  if (n_segments >= totedges) {
    if (sample_straight_edges) {
      /* Assign an even number to each edge if it’s possible, then add the remainder of sampled
       * points starting with the most curved edges. */
      n_common = n_segments / totedges;
      n_left = n_segments % totedges;

      /* Assign the points that fill fit evenly to the edges. */
      if (n_common > 0) {
        for (i = 0; i < totedges; i++) {
          n_samples[i] = n_common;
          n_added += n_common;
        }
      }
    }
    else {
      /* Count the number of curved edges */
      n_curved_edges = 0;
      for (i = 0; i < totedges; i++) {
        if (is_curved_edge(bezt, i)) {
          n_curved_edges++;
        }
      }
      /* Just sample all of the edges if there are no curved edges. */
      n_curved_edges = (n_curved_edges == 0) ? totedges : n_curved_edges;

      /* Give all of the curved edges the same number of points and straight edges one point. */
      n_left = n_segments - (totedges - n_curved_edges); /* Left after 1 for each straight edge. */
      n_common = n_left / n_curved_edges;                /* Number assigned to all curved edges */
      if (n_common > 0) {
        for (i = 0; i < totedges; i++) {
          /* Add the common number if it's a curved edge or if edges are curved. */
          if (is_curved_edge(bezt, i) || n_curved_edges == totedges) {
            n_samples[i] += n_common;
            n_added += n_common;
          }
          else {
            n_samples[i] = 1;
            n_added++;
          }
        }
      }
      n_left -= n_common * n_curved_edges;
    }
  }
  else {
    /* Not enough segments to give one to each edge, so just give them to the most curved edges. */
    n_left = n_segments;
  }
  /* Assign the remainder of the points that couldn't be spread out evenly. */
  BLI_assert(n_left < totedges);
  for (i = 0; i < n_left; i++) {
    n_samples[curve_sorted[i].bezt_index]++;
    n_added++;
  }

  BLI_assert(n_added == n_segments); /* n_added is just used for this assert, could remove it. */

  /* Sample the points and add them to the locations table. */
  for (i_sample = 0, i = 0; i < totedges; i++) {
    if (n_samples[i] > 0) {
      /* Carry over the handle types from the control point to its first corresponding sample. */
      r_samples[i_sample].h1 = profile->path[i].h1;
      r_samples[i_sample].h2 = profile->path[i].h2;
      /* All extra sample points for this control point get "auto" handles. */
      for (int j = i_sample + 1; j < i_sample + n_samples[i]; j++) {
        r_samples[j].flag = 0;
        r_samples[j].h1 = HD_AUTO;
        r_samples[j].h2 = HD_AUTO;
        BLI_assert(j < n_segments);
      }

      /* Sample from the bezier points. X then Y values. */
      BKE_curve_forward_diff_bezier(bezt[i].vec[1][0],
                                    bezt[i].vec[2][0],
                                    bezt[i + 1].vec[0][0],
                                    bezt[i + 1].vec[1][0],
                                    &r_samples[i_sample].x,
                                    n_samples[i],
                                    sizeof(CurveProfilePoint));
      BKE_curve_forward_diff_bezier(bezt[i].vec[1][1],
                                    bezt[i].vec[2][1],
                                    bezt[i + 1].vec[0][1],
                                    bezt[i + 1].vec[1][1],
                                    &r_samples[i_sample].y,
                                    n_samples[i],
                                    sizeof(CurveProfilePoint));
    }
    i_sample += n_samples[i]; /* Add the next set of points after the ones we just added. */
    BLI_assert(i_sample <= n_segments);
  }

#ifdef DEBUG_PROFILE_TABLE
  printf("CURVEPROFILE CREATE SAMPLES\n");
  printf("n_segments: %d\n", n_segments);
  printf("totedges: %d\n", totedges);
  printf("n_common: %d\n", n_common);
  printf("n_left: %d\n", n_left);
  printf("n_samples: ");
  for (i = 0; i < totedges; i++) {
    printf("%d, ", n_samples[i]);
  }
  printf("\n");
  printf("i_curved_sorted: ");
  for (i = 0; i < totedges; i++) {
    printf("(%d %.2f), ", curve_sorted[i].bezt_index, curve_sorted[i].bezt_curvature);
  }
  printf("\n");
#endif

  MEM_freeN(bezt);
  MEM_freeN(curve_sorted);
  MEM_freeN(n_samples);
}

/**
 * Creates a higher resolution table by sampling the curved points.
 * This table is used for display and evenly spaced evaluation.
 */
static void curveprofile_make_table(CurveProfile *profile)
{
  int n_samples = PROF_N_TABLE(profile->path_len);
  CurveProfilePoint *new_table = MEM_callocN(sizeof(CurveProfilePoint) * (n_samples + 1),
                                             "high-res table");

  BKE_curveprofile_create_samples(profile, n_samples - 1, false, new_table);
  /* Manually add last point at the end of the profile */
  new_table[n_samples - 1].x = 0.0f;
  new_table[n_samples - 1].y = 1.0f;

  if (profile->table) {
    MEM_freeN(profile->table);
  }
  profile->table = new_table;
}

/**
 * Creates the table of points used for displaying a preview of the sampled segment locations on
 * the widget itself.
 */
static void CurveProfile_make_segments_table(CurveProfile *profile)
{
  int n_samples = profile->segments_len;
  if (n_samples <= 0) {
    return;
  }
  CurveProfilePoint *new_table = MEM_callocN(sizeof(CurveProfilePoint) * (n_samples + 1),
                                             "samples table");

  if (profile->flag & PROF_SAMPLE_EVEN_LENGTHS) {
    /* Even length sampling incompatible with only straight edge sampling for now. */
    BKE_curveprofile_create_samples_even_spacing(profile, n_samples, new_table);
  }
  else {
    BKE_curveprofile_create_samples(
        profile, n_samples, profile->flag & PROF_SAMPLE_STRAIGHT_EDGES, new_table);
  }

  if (profile->segments) {
    MEM_freeN(profile->segments);
  }
  profile->segments = new_table;
}

/**
 * Sets the default settings and clip range for the profile widget.
 * Does not generate either table.
 */
void BKE_curveprofile_set_defaults(CurveProfile *profile)
{
  profile->flag = PROF_USE_CLIP;

  BLI_rctf_init(&profile->view_rect, 0.0f, 1.0f, 0.0f, 1.0f);
  profile->clip_rect = profile->view_rect;

  profile->path_len = 2;
  profile->path = MEM_callocN(2 * sizeof(CurveProfilePoint), "path points");

  profile->path[0].x = 1.0f;
  profile->path[0].y = 0.0f;
  profile->path[0].profile = profile;
  profile->path[1].x = 1.0f;
  profile->path[1].y = 1.0f;
  profile->path[1].profile = profile;

  profile->changed_timestamp = 0;
}

/**
 * Returns a pointer to a newly allocated curve profile, using the given preset.
 * \param preset: Value in #eCurveProfilePresets.
 */
struct CurveProfile *BKE_curveprofile_add(int preset)
{
  CurveProfile *profile = MEM_callocN(sizeof(CurveProfile), "curve profile");

  BKE_curveprofile_set_defaults(profile);
  profile->preset = preset;
  BKE_curveprofile_reset(profile);
  curveprofile_make_table(profile);

  return profile;
}

/**
 * Should be called after the widget is changed. Does profile and remove double checks and more
 * importantly, recreates the display / evaluation and segments tables.
 * \param update_flags: Bitfield with fields defined in header file. Controls removing doubles and
 * clipping.
 */
void BKE_curveprofile_update(CurveProfile *profile, const int update_flags)
{
  CurveProfilePoint *points = profile->path;
  rctf *clipr = &profile->clip_rect;
  float thresh;
  int i;

  profile->changed_timestamp++;

  /* Clamp with the clipping rect in case something got past. */
  if (profile->flag & PROF_USE_CLIP) {
    /* Move points inside the clip rectangle. */
    if (update_flags & PROF_UPDATE_CLIP) {
      for (i = 0; i < profile->path_len; i++) {
        points[i].x = max_ff(points[i].x, clipr->xmin);
        points[i].x = min_ff(points[i].x, clipr->xmax);
        points[i].y = max_ff(points[i].y, clipr->ymin);
        points[i].y = min_ff(points[i].y, clipr->ymax);

        /* Extra sanity assert to make sure the points have the right profile pointer. */
        BLI_assert(points[i].profile == profile);
      }
    }
    /* Ensure zoom-level respects clipping. */
    if (BLI_rctf_size_x(&profile->view_rect) > BLI_rctf_size_x(&profile->clip_rect)) {
      profile->view_rect.xmin = profile->clip_rect.xmin;
      profile->view_rect.xmax = profile->clip_rect.xmax;
    }
    if (BLI_rctf_size_y(&profile->view_rect) > BLI_rctf_size_y(&profile->clip_rect)) {
      profile->view_rect.ymin = profile->clip_rect.ymin;
      profile->view_rect.ymax = profile->clip_rect.ymax;
    }
  }

  /* Remove doubles with a threshold set at 1% of default range. */
  thresh = pow2f(0.01f * BLI_rctf_size_x(clipr));
  if (update_flags & PROF_UPDATE_REMOVE_DOUBLES && profile->path_len > 2) {
    for (i = 0; i < profile->path_len - 1; i++) {
      if (len_squared_v2v2(&points[i].x, &points[i + 1].x) < thresh) {
        if (i == 0) {
          BKE_curveprofile_remove_point(profile, &points[1]);
        }
        else {
          BKE_curveprofile_remove_point(profile, &points[i]);
        }
        break; /* Assumes 1 deletion per update call is ok. */
      }
    }
  }

  /* Create the high resolution table for drawing and some evaluation functions. */
  curveprofile_make_table(profile);

  /* Store a table of samples for the segment locations for a preview and the table's user. */
  if (profile->segments_len > 0) {
    CurveProfile_make_segments_table(profile);
  }
}

/**
 * Refreshes the higher resolution table sampled from the input points. A call to this or
 * #BKE_curveprofile_update is needed before evaluation functions that use the table.
 * Also sets the number of segments used for the display preview of the locations
 * of the sampled points.
 */
void BKE_curveprofile_initialize(CurveProfile *profile, short segments_len)
{
  if (segments_len != profile->segments_len) {
    profile->flag |= PROF_DIRTY_PRESET;
  }
  profile->segments_len = segments_len;

  /* Calculate the higher resolution / segments tables for display and evaluation. */
  BKE_curveprofile_update(profile, PROF_UPDATE_NONE);
}

/**
 * Gives the distance to the next point in the widgets sampled table, in other words the length
 * of the \a 'i' edge of the table.
 *
 * \note Requires curveprofile_initialize or #BKE_curveprofile_update call before to fill table.
 */
static float curveprofile_distance_to_next_table_point(const CurveProfile *profile, int i)
{
  BLI_assert(i < PROF_N_TABLE(profile->path_len));

  return len_v2v2(&profile->table[i].x, &profile->table[i + 1].x);
}

/**
 * Calculates the total length of the profile from the curves sampled in the table.
 *
 * \note Requires curveprofile_initialize or #BKE_curveprofile_update call before to fill table.
 */
float BKE_curveprofile_total_length(const CurveProfile *profile)
{
  float total_length = 0;
  for (int i = 0; i < PROF_N_TABLE(profile->path_len) - 1; i++) {
    total_length += len_v2v2(&profile->table[i].x, &profile->table[i + 1].x);
  }
  return total_length;
}

/**
 * Samples evenly spaced positions along the curve profile's table (generated from path). Fills
 * an entire table at once for a speedup if all of the results are going to be used anyway.
 *
 * \note Requires curveprofile_initialize or #BKE_curveprofile_update call before to fill table.
 * \note Working, but would conflict with "Sample Straight Edges" option, so this is unused for
 * now.
 */
void BKE_curveprofile_create_samples_even_spacing(CurveProfile *profile,
                                                  int n_segments,
                                                  CurveProfilePoint *r_samples)
{
  const float total_length = BKE_curveprofile_total_length(profile);
  const float segment_length = total_length / n_segments;
  float length_travelled = 0.0f;
  float distance_to_next_table_point = curveprofile_distance_to_next_table_point(profile, 0);
  float distance_to_previous_table_point = 0.0f;
  float segment_left, factor;
  int i_table = 0;

  /* Set the location for the first point. */
  r_samples[0].x = profile->table[0].x;
  r_samples[0].y = profile->table[0].y;

  /* Travel along the path, recording the locations of segments as we pass them. */
  segment_left = segment_length;
  for (int i = 1; i < n_segments; i++) {
    /* Travel over all of the points that fit inside this segment. */
    while (distance_to_next_table_point < segment_left) {
      length_travelled += distance_to_next_table_point;
      segment_left -= distance_to_next_table_point;
      i_table++;
      distance_to_next_table_point = curveprofile_distance_to_next_table_point(profile, i_table);
      distance_to_previous_table_point = 0.0f;
    }
    /* We're at the last table point that fits inside the current segment, use interpolation. */
    factor = (distance_to_previous_table_point + segment_left) /
             (distance_to_previous_table_point + distance_to_next_table_point);
    r_samples[i].x = interpf(profile->table[i_table + 1].x, profile->table[i_table].x, factor);
    r_samples[i].y = interpf(profile->table[i_table + 1].y, profile->table[i_table].y, factor);
#ifdef DEBUG_CURVEPROFILE_EVALUATE
    BLI_assert(factor <= 1.0f && factor >= 0.0f);
    printf("segment_left: %.3f\n", segment_left);
    printf("i_table: %d\n", i_table);
    printf("distance_to_previous_table_point: %.3f\n", distance_to_previous_table_point);
    printf("distance_to_next_table_point: %.3f\n", distance_to_next_table_point);
    printf("Interpolating with factor %.3f from (%.3f, %.3f) to (%.3f, %.3f)\n\n",
           factor,
           profile->table[i_table].x,
           profile->table[i_table].y,
           profile->table[i_table + 1].x,
           profile->table[i_table + 1].y);
#endif

    /* We sampled in between this table point and the next, so the next travel step is smaller. */
    distance_to_next_table_point -= segment_left;
    distance_to_previous_table_point += segment_left;
    length_travelled += segment_left;
    segment_left = segment_length;
  }
}

/**
 * Does a single evaluation along the profile's path.
 * Travels down (length_portion * path) length and returns the position at that point.
 *
 * \param length_portion: The portion (0 to 1) of the path's full length to sample at.
 * \note Requires curveprofile_initialize or #BKE_curveprofile_update call before to fill table.
 */
void BKE_curveprofile_evaluate_length_portion(const CurveProfile *profile,
                                              float length_portion,
                                              float *x_out,
                                              float *y_out)
{
  const float total_length = BKE_curveprofile_total_length(profile);
  const float requested_length = length_portion * total_length;

  /* Find the last point along the path with a lower length portion than the input. */
  int i = 0;
  float length_travelled = 0.0f;
  while (length_travelled < requested_length) {
    /* Check if we reached the last point before the final one. */
    if (i == PROF_N_TABLE(profile->path_len) - 2) {
      break;
    }
    float new_length = curveprofile_distance_to_next_table_point(profile, i);
    if (length_travelled + new_length >= requested_length) {
      break;
    }
    length_travelled += new_length;
    i++;
  }

  /* Now travel the remaining distance of length portion down the path to the next point and
   * find the location where we stop. */
  float distance_to_next_point = curveprofile_distance_to_next_table_point(profile, i);
  float lerp_factor = (requested_length - length_travelled) / distance_to_next_point;

#ifdef DEBUG_CURVEPROFILE_EVALUATE
  printf("CURVEPROFILE EVALUATE\n");
  printf("  length portion input: %f\n", (double)length_portion);
  printf("  requested path length: %f\n", (double)requested_length);
  printf("  distance to next point: %f\n", (double)distance_to_next_point);
  printf("  length travelled: %f\n", (double)length_travelled);
  printf("  lerp-factor: %f\n", (double)lerp_factor);
  printf("  ith point (%f, %f)\n", (double)profile->path[i].x, (double)profile->path[i].y);
  printf("  next point(%f, %f)\n", (double)profile->path[i + 1].x, (double)profile->path[i + 1].y);
#endif

  *x_out = interpf(profile->table[i].x, profile->table[i + 1].x, lerp_factor);
  *y_out = interpf(profile->table[i].y, profile->table[i + 1].y, lerp_factor);
}

void BKE_curveprofile_blend_write(struct BlendWriter *writer, const struct CurveProfile *profile)
{
  BLO_write_struct(writer, CurveProfile, profile);
  BLO_write_struct_array(writer, CurveProfilePoint, profile->path_len, profile->path);
}

/* Expects that the curve profile itself has been read already. */
void BKE_curveprofile_blend_read(struct BlendDataReader *reader, struct CurveProfile *profile)
{
  BLO_read_data_address(reader, &profile->path);
  profile->table = NULL;
  profile->segments = NULL;

  /* Reset the points' pointers to the profile. */
  for (int i = 0; i < profile->path_len; i++) {
    profile->path[i].profile = profile;
  }
}
