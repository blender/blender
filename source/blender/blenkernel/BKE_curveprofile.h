/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation */

#pragma once

/** \file
 * \ingroup bke
 */

#include "DNA_curveprofile_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct BlendDataReader;
struct BlendWriter;
struct CurveProfile;
struct CurveProfilePoint;

/**
 * Sets the default settings and clip range for the profile widget.
 * Does not generate either table.
 */
void BKE_curveprofile_set_defaults(struct CurveProfile *profile);

/**
 * Returns a pointer to a newly allocated curve profile, using the given preset.
 */
struct CurveProfile *BKE_curveprofile_add(eCurveProfilePresets preset);

void BKE_curveprofile_free_data(struct CurveProfile *profile);

void BKE_curveprofile_free(struct CurveProfile *profile);

void BKE_curveprofile_copy_data(struct CurveProfile *target, const struct CurveProfile *profile);

struct CurveProfile *BKE_curveprofile_copy(const struct CurveProfile *profile);

/**
 * Move a point's handle, accounting for the alignment of handles with the #HD_ALIGN type.
 *
 * \param handle_1: Whether to move the 1st or 2nd control point.
 * \param delta: The *relative* change in the handle's position.
 * \note Requires #BKE_curveprofile_update call after.
 * \return Whether the handle moved from its start position.
 */
bool BKE_curveprofile_move_handle(struct CurveProfilePoint *point,
                                  bool handle_1,
                                  bool snap,
                                  const float delta[2]);

/**
 * Moves a control point, accounting for clipping and snapping, and moving free handles.
 *
 * \param snap: Whether to snap the point to the grid
 * \param delta: The *relative* change of the point's location.
 * \return Whether the point moved from its start position.
 * \note Requires #BKE_curveprofile_update call after.
 */
bool BKE_curveprofile_move_point(struct CurveProfile *profile,
                                 struct CurveProfilePoint *point,
                                 bool snap,
                                 const float delta[2]);

/**
 * Removes a specific point from the path of control points.
 * \note Requires #BKE_curveprofile_update call after.
 */
bool BKE_curveprofile_remove_point(struct CurveProfile *profile, struct CurveProfilePoint *point);

/**
 * Removes every point in the widget with the supplied flag set, except for the first and last.
 *
 * \param flag: #CurveProfilePoint.flag.
 *
 * \note Requires #BKE_curveprofile_update call after.
 */
void BKE_curveprofile_remove_by_flag(struct CurveProfile *profile, short flag);

/**
 * Adds a new point at the specified location. The choice for which points to place the new vertex
 * between is made by checking which control point line segment is closest to the new point and
 * placing the new vertex in between that segment's points.
 *
 * \note Requires #BKE_curveprofile_update call after.
 */
struct CurveProfilePoint *BKE_curveprofile_insert(struct CurveProfile *profile, float x, float y);

/**
 * Sets the handle type of the selected control points.
 * \param type_1, type_2: Handle type for the first handle. HD_VECT, HD_AUTO, HD_FREE, or HD_ALIGN.
 * \note Requires #BKE_curveprofile_update call after.
 */
void BKE_curveprofile_selected_handle_set(struct CurveProfile *profile, int type_1, int type_2);

/**
 * Flips the profile across the diagonal so that its orientation is reversed.
 *
 * \note Requires #BKE_curveprofile_update call after.
 */
void BKE_curveprofile_reverse(struct CurveProfile *profile);

/**
 * Reset the view to the clipping rectangle.
 */
void BKE_curveprofile_reset_view(struct CurveProfile *profile);

/**
 * Resets the profile to the current preset.
 *
 * \note Requires #BKE_curveprofile_update call after.
 */
void BKE_curveprofile_reset(struct CurveProfile *profile);

int BKE_curveprofile_table_size(const struct CurveProfile *profile);

/**
 * Refreshes the higher resolution table sampled from the input points. A call to this or
 * #BKE_curveprofile_update is needed before evaluation functions that use the table.
 * Also sets the number of segments used for the display preview of the locations
 * of the sampled points.
 */
void BKE_curveprofile_init(struct CurveProfile *profile, short segments_len);

/* Called for a complete update of the widget after modifications */
enum {
  PROF_UPDATE_NONE = 0,
  PROF_UPDATE_REMOVE_DOUBLES = (1 << 0),
  PROF_UPDATE_CLIP = (1 << 1),
};
/**
 * Should be called after the widget is changed. Does profile and remove double checks and more
 * importantly, recreates the display / evaluation and segments tables.
 * \param update_flags: Bit-field with fields defined in header file.
 * Controls removing doubles and clipping.
 */
void BKE_curveprofile_update(struct CurveProfile *profile, int update_flags);

/**
 * Does a single evaluation along the profile's path.
 * Travels down (length_portion * path) length and returns the position at that point.
 * Where length portion is the fraction of the total path length where we want the location.
 *
 * \param length_portion: The portion (0 to 1) of the path's full length to sample at.
 * \note Requires #BKE_curveprofile_init or #BKE_curveprofile_update call before to fill table.
 */
void BKE_curveprofile_evaluate_length_portion(const struct CurveProfile *profile,
                                              float length_portion,
                                              float *x_out,
                                              float *y_out);

void BKE_curveprofile_blend_write(struct BlendWriter *writer, const struct CurveProfile *profile);
/**
 * Expects that the curve profile itself has been read already.
 */
void BKE_curveprofile_blend_read(struct BlendDataReader *reader, struct CurveProfile *profile);

#ifdef __cplusplus
}
#endif
