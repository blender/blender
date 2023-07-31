/* SPDX-FileCopyrightText: 2019 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <algorithm>

#include "MEM_guardedalloc.h"

#include "DNA_curve_types.h"
#include "DNA_curveprofile_types.h"

#include "BLI_math_vector.h"
#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "BKE_curve.h"
#include "BKE_curveprofile.h"

#include "BLO_read_write.h"

/** Number of points in high resolution table is dynamic up to a maximum. */
#define PROF_TABLE_MAX 512

/* -------------------------------------------------------------------- */
/** \name Data Handling
 * \{ */

CurveProfile *BKE_curveprofile_add(eCurveProfilePresets preset)
{
  CurveProfile *profile = MEM_cnew<CurveProfile>(__func__);

  BKE_curveprofile_set_defaults(profile);
  profile->preset = preset;
  BKE_curveprofile_reset(profile);
  BKE_curveprofile_update(profile, 0);

  return profile;
}

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

  target->path = (CurveProfilePoint *)MEM_dupallocN(profile->path);
  target->table = (CurveProfilePoint *)MEM_dupallocN(profile->table);
  target->segments = (CurveProfilePoint *)MEM_dupallocN(profile->segments);

  /* Update the reference the points have to the profile. */
  for (int i = 0; i < target->path_len; i++) {
    target->path[i].profile = target;
  }
}

CurveProfile *BKE_curveprofile_copy(const CurveProfile *profile)
{
  if (profile) {
    CurveProfile *new_prdgt = (CurveProfile *)MEM_dupallocN(profile);
    BKE_curveprofile_copy_data(new_prdgt, profile);
    return new_prdgt;
  }
  return nullptr;
}

void BKE_curveprofile_blend_write(BlendWriter *writer, const CurveProfile *profile)
{
  BLO_write_struct(writer, CurveProfile, profile);
  BLO_write_struct_array(writer, CurveProfilePoint, profile->path_len, profile->path);
}

void BKE_curveprofile_blend_read(BlendDataReader *reader, CurveProfile *profile)
{
  BLO_read_data_address(reader, &profile->path);
  profile->table = nullptr;
  profile->segments = nullptr;

  /* Reset the points' pointers to the profile. */
  for (int i = 0; i < profile->path_len; i++) {
    profile->path[i].profile = profile;
  }

  BKE_curveprofile_init(profile, profile->segments_len);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Editing
 * \{ */

bool BKE_curveprofile_move_handle(CurveProfilePoint *point,
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

bool BKE_curveprofile_move_point(CurveProfile *profile,
                                 CurveProfilePoint *point,
                                 const bool snap,
                                 const float delta[2])
{
  /* Don't move the final point. */
  if (point == &profile->path[profile->path_len - 1]) {
    return false;
  }
  /* Don't move the first point. */
  if (point == profile->path) {
    return false;
  }
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

bool BKE_curveprofile_remove_point(CurveProfile *profile, CurveProfilePoint *point)
{
  /* Must have 2 points minimum. */
  if (profile->path_len <= 2) {
    return false;
  }

  /* Input point must be within the array. */
  if (!(point > profile->path && point < profile->path + profile->path_len)) {
    return false;
  }

  CurveProfilePoint *new_path = (CurveProfilePoint *)MEM_mallocN(
      sizeof(CurveProfilePoint) * profile->path_len, __func__);

  int i_delete = int(point - profile->path);
  BLI_assert(i_delete > 0);

  /* Copy the before and after the deleted point. */
  memcpy(new_path, profile->path, sizeof(CurveProfilePoint) * i_delete);
  memcpy(new_path + i_delete,
         profile->path + i_delete + 1,
         sizeof(CurveProfilePoint) * (profile->path_len - i_delete - 1));

  MEM_freeN(profile->path);
  profile->path = new_path;
  profile->path_len -= 1;
  return true;
}

void BKE_curveprofile_remove_by_flag(CurveProfile *profile, const short flag)
{
  /* Copy every point without the flag into the new path. */
  CurveProfilePoint *new_path = (CurveProfilePoint *)MEM_mallocN(
      sizeof(CurveProfilePoint) * profile->path_len, __func__);

  /* Build the new list without any of the points with the flag. Keep the first and last points. */
  int i_new = 1;
  int i_old = 1;
  int n_removed = 0;
  new_path[0] = profile->path[0];
  for (; i_old < profile->path_len - 1; i_old++) {
    if (!(profile->path[i_old].flag & flag)) {
      new_path[i_new] = profile->path[i_old];
      i_new++;
    }
    else {
      n_removed++;
    }
  }
  new_path[i_new] = profile->path[i_old];

  MEM_freeN(profile->path);
  profile->path = new_path;
  profile->path_len -= n_removed;
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

CurveProfilePoint *BKE_curveprofile_insert(CurveProfile *profile, float x, float y)
{
  const float new_loc[2] = {x, y};

  /* Don't add more control points  than the maximum size of the higher resolution table. */
  if (profile->path_len == PROF_TABLE_MAX - 1) {
    return nullptr;
  }

  /* Find the index at the line segment that's closest to the new position. */
  float min_distance = FLT_MAX;
  int i_insert = 0;
  for (int i = 0; i < profile->path_len - 1; i++) {
    const float loc1[2] = {profile->path[i].x, profile->path[i].y};
    const float loc2[2] = {profile->path[i + 1].x, profile->path[i + 1].y};

    float distance = dist_squared_to_line_segment_v2(new_loc, loc1, loc2);
    if (distance < min_distance) {
      min_distance = distance;
      i_insert = i + 1;
    }
  }

  /* Insert the new point at the location we found and copy all of the old points in as well. */
  profile->path_len++;
  CurveProfilePoint *new_path = (CurveProfilePoint *)MEM_mallocN(
      sizeof(CurveProfilePoint) * profile->path_len, __func__);
  CurveProfilePoint *new_pt = nullptr;
  for (int i_new = 0, i_old = 0; i_new < profile->path_len; i_new++) {
    if (i_new != i_insert) {
      /* Insert old points. */
      new_path[i_new] = profile->path[i_old];
      new_path[i_new].flag &= ~PROF_SELECT; /* Deselect old points. */
      i_old++;
    }
    else {
      /* Insert new point. */
      /* Set handles of new point based on its neighbors. */
      char new_handle_type = (new_path[i_new - 1].h2 == HD_VECT &&
                              profile->path[i_insert].h1 == HD_VECT) ?
                                 HD_VECT :
                                 HD_AUTO;
      point_init(&new_path[i_new], x, y, PROF_SELECT, new_handle_type, new_handle_type);
      new_pt = &new_path[i_new];
      /* Give new point a reference to the profile. */
      new_pt->profile = profile;
    }
  }

  /* Free the old path and use the new one. */
  MEM_freeN(profile->path);
  profile->path = new_path;
  return new_pt;
}

void BKE_curveprofile_selected_handle_set(CurveProfile *profile, int type_1, int type_2)
{
  for (int i = 0; i < profile->path_len; i++) {
    if (ELEM(profile->path[i].flag, PROF_SELECT, PROF_H1_SELECT, PROF_H2_SELECT)) {
      profile->path[i].h1 = type_1;
      profile->path[i].h2 = type_2;

      if (type_1 == HD_ALIGN && type_2 == HD_ALIGN) {
        /* Align the handles. */
        BKE_curveprofile_move_handle(&profile->path[i], true, false, nullptr);
      }
    }
  }
}

static CurveProfilePoint mirror_point(const CurveProfilePoint *point)
{
  CurveProfilePoint new_point = *point;
  point_init(&new_point, point->y, point->x, point->flag, point->h2, point->h1);
  return new_point;
}

void BKE_curveprofile_reverse(CurveProfile *profile)
{
  /* When there are only two points, reversing shouldn't do anything. */
  if (profile->path_len == 2) {
    return;
  }
  CurveProfilePoint *new_path = (CurveProfilePoint *)MEM_mallocN(
      sizeof(CurveProfilePoint) * profile->path_len, __func__);
  /* Mirror the new points across the y = x line */
  for (int i = 0; i < profile->path_len; i++) {
    int i_reversed = profile->path_len - i - 1;
    BLI_assert(i_reversed >= 0);
    new_path[i_reversed] = mirror_point(&profile->path[i]);
    new_path[i_reversed].profile = profile;

    /* Mirror free handles, they can't be recalculated. */
    if (ELEM(profile->path[i].h1, HD_FREE, HD_ALIGN)) {
      new_path[i_reversed].h1_loc[0] = profile->path[i].h2_loc[1];
      new_path[i_reversed].h1_loc[1] = profile->path[i].h2_loc[0];
    }
    if (ELEM(profile->path[i].h2, HD_FREE, HD_ALIGN)) {
      new_path[i_reversed].h2_loc[0] = profile->path[i].h1_loc[1];
      new_path[i_reversed].h2_loc[1] = profile->path[i].h1_loc[0];
    }
  }

  /* Free the old points and use the new ones */
  MEM_freeN(profile->path);
  profile->path = new_path;
}

/**
 * Builds a quarter circle profile with space on each side for 'support loops.'
 */
static void curveprofile_build_supports(CurveProfile *profile)
{
  int n = profile->path_len;

  point_init(&profile->path[0], 1.0f, 0.0f, 0, HD_VECT, HD_VECT);
  point_init(&profile->path[1], 1.0f, 0.5f, 0, HD_VECT, HD_VECT);
  for (int i = 1; i < n - 2; i++) {
    const float x = 1.0f - (0.5f * (1.0f - cosf(float(i / float(n - 3)) * M_PI_2)));
    const float y = 0.5f + 0.5f * sinf(float((i / float(n - 3)) * M_PI_2));
    point_init(&profile->path[i], x, y, 0, HD_AUTO, HD_AUTO);
  }
  point_init(&profile->path[n - 2], 0.5f, 1.0f, 0, HD_VECT, HD_VECT);
  point_init(&profile->path[n - 1], 0.0f, 1.0f, 0, HD_VECT, HD_VECT);
}

/**
 * Puts the widgets control points in a step pattern.
 * Uses vector handles for each point.
 */
static void curveprofile_build_steps(CurveProfile *profile)
{
  int n = profile->path_len;

  /* Special case for two points to avoid dividing by zero later. */
  if (n == 2) {
    point_init(&profile->path[0], 1.0f, 0.0f, 0, HD_VECT, HD_VECT);
    point_init(&profile->path[0], 0.0f, 1.0f, 0, HD_VECT, HD_VECT);
    return;
  }

  float n_steps_x = (n % 2 == 0) ? n : (n - 1);
  float n_steps_y = (n % 2 == 0) ? (n - 2) : (n - 1);

  for (int i = 0; i < n; i++) {
    int step_x = (i + 1) / 2;
    int step_y = i / 2;
    const float x = 1.0f - (float(2 * step_x) / n_steps_x);
    const float y = float(2 * step_y) / n_steps_y;
    point_init(&profile->path[i], x, y, 0, HD_VECT, HD_VECT);
  }
}

void BKE_curveprofile_reset_view(CurveProfile *profile)
{
  profile->view_rect = profile->clip_rect;
}

void BKE_curveprofile_reset(CurveProfile *profile)
{
  MEM_SAFE_FREE(profile->path);

  eCurveProfilePresets preset = static_cast<eCurveProfilePresets>(profile->preset);
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

  profile->path = (CurveProfilePoint *)MEM_callocN(sizeof(CurveProfilePoint) * profile->path_len,
                                                   __func__);

  switch (preset) {
    case PROF_PRESET_LINE:
      point_init(&profile->path[0], 1.0f, 0.0f, 0, HD_AUTO, HD_AUTO);
      point_init(&profile->path[1], 0.0f, 1.0f, 0, HD_AUTO, HD_AUTO);
      break;
    case PROF_PRESET_SUPPORTS:
      curveprofile_build_supports(profile);
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
      curveprofile_build_steps(profile);
      break;
  }

  profile->flag &= ~PROF_DIRTY_PRESET;

  /* Ensure each point has a reference to the profile. */
  for (int i = 0; i < profile->path_len; i++) {
    profile->path[i].profile = profile;
  }

  MEM_SAFE_FREE(profile->table);
  profile->table = nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sampling and Evaluation
 * \{ */

int BKE_curveprofile_table_size(const CurveProfile *profile)
{
  /** Number of table points per control point. */
  const int resolution = 16;

  /* Make sure there is always one sample, even if there are no control points. */
  return std::clamp((profile->path_len - 1) * resolution + 1, 1, PROF_TABLE_MAX);
}

/**
 * Helper for 'curve_profile_create' samples.
 * Returns whether both handles that make up the edge are vector handles.
 */
static bool is_curved_edge(CurveProfilePoint *path, int i)
{
  return (path[i].h2 != HD_VECT || path[i + 1].h1 != HD_VECT);
}

/**
 * Used to set bezier handle locations in the sample creation process. Reduced copy of
 * #calchandleNurb_intern code in `curve.cc`, mostly changed by removing the third dimension.
 */
static void point_calculate_handle(CurveProfilePoint *point,
                                   const CurveProfilePoint *prev,
                                   const CurveProfilePoint *next)
{
  if (point->h1 == HD_FREE && point->h2 == HD_FREE) {
    return;
  }

  float *point_loc = &point->x;

  float pt[2];
  const float *prev_loc, *next_loc;
  if (prev == nullptr) {
    next_loc = &next->x;
    pt[0] = 2.0f * point_loc[0] - next_loc[0];
    pt[1] = 2.0f * point_loc[1] - next_loc[1];
    prev_loc = pt;
  }
  else {
    prev_loc = &prev->x;
  }

  if (next == nullptr) {
    prev_loc = &prev->x;
    pt[0] = 2.0f * point_loc[0] - prev_loc[0];
    pt[1] = 2.0f * point_loc[1] - prev_loc[1];
    next_loc = pt;
  }
  else {
    next_loc = &next->x;
  }

  float dvec_a[2], dvec_b[2];
  sub_v2_v2v2(dvec_a, point_loc, prev_loc);
  sub_v2_v2v2(dvec_b, next_loc, point_loc);

  float len_a = len_v2(dvec_a);
  float len_b = len_v2(dvec_b);
  if (len_a == 0.0f) {
    len_a = 1.0f;
  }
  if (len_b == 0.0f) {
    len_b = 1.0f;
  }

  if (point->h1 == HD_AUTO || point->h2 == HD_AUTO) {
    float tvec[2];
    tvec[0] = dvec_b[0] / len_b + dvec_a[0] / len_a;
    tvec[1] = dvec_b[1] / len_b + dvec_a[1] / len_a;

    float len = len_v2(tvec) * 2.5614f;
    if (len != 0.0f) {
      if (point->h1 == HD_AUTO) {
        len_a /= len;
        madd_v2_v2v2fl(point->h1_loc, point_loc, tvec, -len_a);
      }
      if (point->h2 == HD_AUTO) {
        len_b /= len;
        madd_v2_v2v2fl(point->h2_loc, point_loc, tvec, len_b);
      }
    }
  }

  if (point->h1 == HD_VECT) {
    madd_v2_v2v2fl(point->h1_loc, point_loc, dvec_a, -1.0f / 3.0f);
  }
  if (point->h2 == HD_VECT) {
    madd_v2_v2v2fl(point->h2_loc, point_loc, dvec_b, 1.0f / 3.0f);
  }
}

static void calculate_path_handles(CurveProfilePoint *path, int path_len)
{
  point_calculate_handle(&path[0], nullptr, &path[1]);
  for (int i = 1; i < path_len - 1; i++) {
    point_calculate_handle(&path[i], &path[i - 1], &path[i + 1]);
  }
  point_calculate_handle(&path[path_len - 1], &path[path_len - 2], nullptr);
}

/**
 * Helper function for #create_samples. Calculates the angle between the
 * handles on the inside of the edge starting at index i. A larger angle means the edge is
 * more curved.
 * \param i_edge: The start index of the edge to calculate the angle for.
 */
static float bezt_edge_handle_angle(const CurveProfilePoint *path, int i_edge)
{
  /* Find the direction of the handles that define this edge along the direction of the path. */
  float start_handle_direction[2], end_handle_direction[2];
  /* Handle 2 - point location. */
  sub_v2_v2v2(start_handle_direction, path[i_edge].h2_loc, &path[i_edge].x);
  /* Point location - handle 1. */
  sub_v2_v2v2(end_handle_direction, &path[i_edge + 1].x, path[i_edge + 1].h1_loc);

  return angle_v2v2(start_handle_direction, end_handle_direction);
}

/** Struct to sort curvature of control point edges. */
struct CurvatureSortPoint {
  /** The index of the corresponding profile point. */
  int point_index;
  /** The curvature of the edge with the above index. */
  float point_curvature;
};

/**
 * Helper function for #create_samples for sorting edges based on curvature.
 */
static int sort_points_curvature(const void *in_a, const void *in_b)
{
  const CurvatureSortPoint *a = (const CurvatureSortPoint *)in_a;
  const CurvatureSortPoint *b = (const CurvatureSortPoint *)in_b;

  if (a->point_curvature > b->point_curvature) {
    return 0;
  }

  return 1;
}

/**
 * Used for sampling curves along the profile's path. Any points more than the number of
 * user-defined points will be evenly distributed among the curved edges.
 * Then the remainders will be distributed to the most curved edges.
 *
 * \param n_segments: The number of segments to sample along the path. Ideally it is higher than
 * the number of points used to define the profile (profile->path_len).
 * \param sample_straight_edges: Whether to sample points between vector handle control points.
 * If this is true and there are only vector edges the straight edges will still be sampled.
 * \param r_samples: Return array of points to put the sampled positions. Must have length
 * n_segments. Fill the array with the sampled locations and if the point corresponds to a
 * control point, its handle type.
 */
static void create_samples(CurveProfile *profile,
                           int n_segments,
                           bool sample_straight_edges,
                           CurveProfilePoint *r_samples)
{
  CurveProfilePoint *path = profile->path;
  int totpoints = profile->path_len;
  BLI_assert(n_segments > 0);

  int totedges = totpoints - 1;

  calculate_path_handles(path, totpoints);

  /* Create a list of edge indices with the most curved at the start, least curved at the end. */
  CurvatureSortPoint *curve_sorted = (CurvatureSortPoint *)MEM_callocN(
      sizeof(CurvatureSortPoint) * totedges, __func__);
  for (int i = 0; i < totedges; i++) {
    curve_sorted[i].point_index = i;
    /* Calculate the curvature of each edge once for use when sorting for curvature. */
    curve_sorted[i].point_curvature = bezt_edge_handle_angle(path, i);
  }
  qsort(curve_sorted, totedges, sizeof(CurvatureSortPoint), sort_points_curvature);

  /* Assign the number of sampled points for each edge. */
  int16_t *n_samples = (int16_t *)MEM_callocN(sizeof(int16_t) * totedges, "samples numbers");
  int n_added = 0;
  int n_left;
  if (n_segments >= totedges) {
    if (sample_straight_edges) {
      /* Assign an even number to each edge if it’s possible, then add the remainder of sampled
       * points starting with the most curved edges. */
      int n_common = n_segments / totedges;
      n_left = n_segments % totedges;

      /* Assign the points that fill fit evenly to the edges. */
      if (n_common > 0) {
        BLI_assert(n_common < INT16_MAX);
        for (int i = 0; i < totedges; i++) {
          n_samples[i] = n_common;
          n_added += n_common;
        }
      }
    }
    else {
      /* Count the number of curved edges */
      int n_curved_edges = 0;
      for (int i = 0; i < totedges; i++) {
        if (is_curved_edge(path, i)) {
          n_curved_edges++;
        }
      }
      /* Just sample all of the edges if there are no curved edges. */
      n_curved_edges = (n_curved_edges == 0) ? totedges : n_curved_edges;

      /* Give all of the curved edges the same number of points and straight edges one point. */
      n_left = n_segments - (totedges - n_curved_edges); /* Left after 1 for each straight edge. */
      int n_common = n_left / n_curved_edges;            /* Number assigned to all curved edges */
      if (n_common > 0) {
        for (int i = 0; i < totedges; i++) {
          /* Add the common number if it's a curved edge or if edges are curved. */
          if (is_curved_edge(path, i) || n_curved_edges == totedges) {
            BLI_assert(n_common + n_samples[i] < INT16_MAX);
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
  for (int i = 0; i < n_left; i++) {
    BLI_assert(n_samples[curve_sorted[i].point_index] < INT16_MAX);
    n_samples[curve_sorted[i].point_index]++;
    n_added++;
  }

  BLI_assert(n_added == n_segments); /* n_added is just used for this assert, could remove it. */
  UNUSED_VARS_NDEBUG(n_added);

  /* Sample the points and add them to the locations table. */
  for (int i_sample = 0, i = 0; i < totedges; i++) {
    if (n_samples[i] > 0) {
      /* Carry over the handle types from the control point to its first corresponding sample. */
      r_samples[i_sample].h1 = path[i].h1;
      r_samples[i_sample].h2 = path[i].h2;
      /* All extra sample points for this control point get "auto" handles. */
      for (int j = i_sample + 1; j < i_sample + n_samples[i]; j++) {
        r_samples[j].flag = 0;
        r_samples[j].h1 = HD_AUTO;
        r_samples[j].h2 = HD_AUTO;
        BLI_assert(j < n_segments);
      }

      /* Sample from the bezier points. X then Y values. */
      BKE_curve_forward_diff_bezier(path[i].x,
                                    path[i].h2_loc[0],
                                    path[i + 1].h1_loc[0],
                                    path[i + 1].x,
                                    &r_samples[i_sample].x,
                                    n_samples[i],
                                    sizeof(CurveProfilePoint));
      BKE_curve_forward_diff_bezier(path[i].y,
                                    path[i].h2_loc[1],
                                    path[i + 1].h1_loc[1],
                                    path[i + 1].y,
                                    &r_samples[i_sample].y,
                                    n_samples[i],
                                    sizeof(CurveProfilePoint));
    }
    i_sample += n_samples[i]; /* Add the next set of points after the ones we just added. */
    BLI_assert(i_sample <= n_segments);
  }

  MEM_freeN(curve_sorted);
  MEM_freeN(n_samples);
}

void BKE_curveprofile_set_defaults(CurveProfile *profile)
{
  profile->flag = PROF_USE_CLIP;

  BLI_rctf_init(&profile->view_rect, 0.0f, 1.0f, 0.0f, 1.0f);
  profile->clip_rect = profile->view_rect;

  profile->path_len = 2;
  profile->path = (CurveProfilePoint *)MEM_callocN(2 * sizeof(CurveProfilePoint), __func__);

  profile->path[0].x = 1.0f;
  profile->path[0].y = 0.0f;
  profile->path[0].profile = profile;
  profile->path[1].x = 1.0f;
  profile->path[1].y = 1.0f;
  profile->path[1].profile = profile;

  profile->changed_timestamp = 0;
}

void BKE_curveprofile_init(CurveProfile *profile, short segments_len)
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
 * \note Requires #BKE_curveprofile_init or #BKE_curveprofile_update call before to fill table.
 */
static float curveprofile_distance_to_next_table_point(const CurveProfile *profile, int i)
{
  BLI_assert(i < BKE_curveprofile_table_size(profile));

  return len_v2v2(&profile->table[i].x, &profile->table[i + 1].x);
}

/**
 * Calculates the total length of the profile from the curves sampled in the table.
 *
 * \note Requires #BKE_curveprofile_init or #BKE_curveprofile_update call before to fill table.
 */
static float curveprofile_total_length(const CurveProfile *profile)
{
  float total_length = 0;
  for (int i = 0; i < BKE_curveprofile_table_size(profile) - 1; i++) {
    total_length += len_v2v2(&profile->table[i].x, &profile->table[i + 1].x);
  }
  return total_length;
}

/**
 * Samples evenly spaced positions along the curve profile's table (generated from path). Fills
 * an entire table at once for a speedup if all of the results are going to be used anyway.
 *
 * \note Requires #BKE_curveprofile_init or #BKE_curveprofile_update call before to fill table.
 * \note Working, but would conflict with "Sample Straight Edges" option, so this is unused for
 * now.
 */
static void create_samples_even_spacing(CurveProfile *profile,
                                        int n_segments,
                                        CurveProfilePoint *r_samples)
{
  const float total_length = curveprofile_total_length(profile);
  const float segment_length = total_length / n_segments;
  float distance_to_next_table_point = curveprofile_distance_to_next_table_point(profile, 0);
  float distance_to_previous_table_point = 0.0f;
  int i_table = 0;

  /* Set the location for the first point. */
  r_samples[0].x = profile->table[0].x;
  r_samples[0].y = profile->table[0].y;

  /* Travel along the path, recording the locations of segments as we pass them. */
  float segment_left = segment_length;
  for (int i = 1; i < n_segments; i++) {
    /* Travel over all of the points that fit inside this segment. */
    while (distance_to_next_table_point < segment_left) {
      segment_left -= distance_to_next_table_point;
      i_table++;
      distance_to_next_table_point = curveprofile_distance_to_next_table_point(profile, i_table);
      distance_to_previous_table_point = 0.0f;
    }
    /* We're at the last table point that fits inside the current segment, use interpolation. */
    float factor = (distance_to_previous_table_point + segment_left) /
                   (distance_to_previous_table_point + distance_to_next_table_point);
    r_samples[i].x = interpf(profile->table[i_table + 1].x, profile->table[i_table].x, factor);
    r_samples[i].y = interpf(profile->table[i_table + 1].y, profile->table[i_table].y, factor);
    BLI_assert(factor <= 1.0f && factor >= 0.0f);
#ifdef DEBUG_CURVEPROFILE_EVALUATE
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
    segment_left = segment_length;
  }
}

/**
 * Creates a higher resolution table by sampling the curved points.
 * This table is used for display and evenly spaced evaluation.
 */
static void curveprofile_make_table(CurveProfile *profile)
{
  int n_samples = BKE_curveprofile_table_size(profile);
  CurveProfilePoint *new_table = (CurveProfilePoint *)MEM_callocN(
      sizeof(CurveProfilePoint) * (n_samples + 1), __func__);

  if (n_samples > 1) {
    create_samples(profile, n_samples - 1, false, new_table);
  }

  /* Manually add last point at the end of the profile */
  new_table[n_samples - 1].x = 0.0f;
  new_table[n_samples - 1].y = 1.0f;

  MEM_SAFE_FREE(profile->table);
  profile->table = new_table;
}

/**
 * Creates the table of points used for displaying a preview of the sampled segment locations on
 * the widget itself.
 */
static void curveprofile_make_segments_table(CurveProfile *profile)
{
  int n_samples = profile->segments_len;
  if (n_samples <= 0) {
    return;
  }
  CurveProfilePoint *new_table = (CurveProfilePoint *)MEM_callocN(
      sizeof(CurveProfilePoint) * (n_samples + 1), __func__);

  if (profile->flag & PROF_SAMPLE_EVEN_LENGTHS) {
    /* Even length sampling incompatible with only straight edge sampling for now. */
    create_samples_even_spacing(profile, n_samples, new_table);
  }
  else {
    create_samples(profile, n_samples, profile->flag & PROF_SAMPLE_STRAIGHT_EDGES, new_table);
  }

  MEM_SAFE_FREE(profile->segments);
  profile->segments = new_table;
}

void BKE_curveprofile_update(CurveProfile *profile, const int update_flags)
{
  CurveProfilePoint *points = profile->path;
  rctf *clipr = &profile->clip_rect;

  profile->changed_timestamp++;

  /* Clamp with the clipping rect in case something got past. */
  if (profile->flag & PROF_USE_CLIP) {
    /* Move points inside the clip rectangle. */
    if (update_flags & PROF_UPDATE_CLIP) {
      for (int i = 0; i < profile->path_len; i++) {
        points[i].x = clamp_f(points[i].x, clipr->xmin, clipr->xmax);
        points[i].y = clamp_f(points[i].y, clipr->ymin, clipr->ymax);

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
  float thresh = pow2f(0.01f * BLI_rctf_size_x(clipr));
  if (update_flags & PROF_UPDATE_REMOVE_DOUBLES && profile->path_len > 2) {
    for (int i = 0; i < profile->path_len - 1; i++) {
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
    curveprofile_make_segments_table(profile);
  }
}

void BKE_curveprofile_evaluate_length_portion(const CurveProfile *profile,
                                              float length_portion,
                                              float *x_out,
                                              float *y_out)
{
  const float total_length = curveprofile_total_length(profile);
  const float requested_length = length_portion * total_length;

  /* Find the last point along the path with a lower length portion than the input. */
  int i = 0;
  float length_traveled = 0.0f;
  while (length_traveled < requested_length) {
    /* Check if we reached the last point before the final one. */
    if (i == BKE_curveprofile_table_size(profile) - 2) {
      break;
    }
    float new_length = curveprofile_distance_to_next_table_point(profile, i);
    if (length_traveled + new_length >= requested_length) {
      break;
    }
    length_traveled += new_length;
    i++;
  }

  /* Now travel the remaining distance of length portion down the path to the next point and
   * find the location where we stop. */
  float distance_to_next_point = curveprofile_distance_to_next_table_point(profile, i);
  float lerp_factor = (requested_length - length_traveled) / distance_to_next_point;

#ifdef DEBUG_CURVEPROFILE_EVALUATE
  printf("CURVEPROFILE EVALUATE\n");
  printf("  length portion input: %f\n", double(length_portion));
  printf("  requested path length: %f\n", double(requested_length));
  printf("  distance to next point: %f\n", double(distance_to_next_point));
  printf("  length traveled: %f\n", double(length_traveled));
  printf("  lerp-factor: %f\n", double(lerp_factor));
  printf("  ith point (%f, %f)\n", double(profile->path[i].x), double(profile->path[i].y));
  printf("  next point(%f, %f)\n", double(profile->path[i + 1].x), double(profile->path[i + 1].y));
#endif

  *x_out = interpf(profile->table[i].x, profile->table[i + 1].x, lerp_factor);
  *y_out = interpf(profile->table[i].y, profile->table[i + 1].y, lerp_factor);
}

/** \} */
