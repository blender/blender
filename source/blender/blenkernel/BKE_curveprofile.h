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

#ifndef __BKE_CURVEPROFILE_H__
#define __BKE_CURVEPROFILE_H__

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct BlendWriter;
struct BlendDataReader;
struct CurveProfile;
struct CurveProfilePoint;

void BKE_curveprofile_set_defaults(struct CurveProfile *profile);

struct CurveProfile *BKE_curveprofile_add(int preset);

void BKE_curveprofile_free_data(struct CurveProfile *profile);

void BKE_curveprofile_free(struct CurveProfile *profile);

void BKE_curveprofile_copy_data(struct CurveProfile *target, const struct CurveProfile *profile);

struct CurveProfile *BKE_curveprofile_copy(const struct CurveProfile *profile);

bool BKE_curveprofile_remove_point(struct CurveProfile *profile, struct CurveProfilePoint *point);

void BKE_curveprofile_remove_by_flag(struct CurveProfile *profile, const short flag);

struct CurveProfilePoint *BKE_curveprofile_insert(struct CurveProfile *profile, float x, float y);

void BKE_curveprofile_selected_handle_set(struct CurveProfile *profile, int type_1, int type_2);

void BKE_curveprofile_reverse(struct CurveProfile *profile);

void BKE_curveprofile_reset(struct CurveProfile *profile);

void BKE_curveprofile_create_samples(struct CurveProfile *profile,
                                     int segments_len,
                                     bool sample_straight_edges,
                                     struct CurveProfilePoint *r_samples);

void BKE_curveprofile_initialize(struct CurveProfile *profile, short segments_len);

/* Called for a complete update of the widget after modifications */
void BKE_curveprofile_update(struct CurveProfile *profile, const bool rem_doubles);

/* Need to find the total length of the curve to sample a portion of it */
float BKE_curveprofile_total_length(const struct CurveProfile *profile);

void BKE_curveprofile_create_samples_even_spacing(struct CurveProfile *profile,
                                                  int segments_len,
                                                  struct CurveProfilePoint *r_samples);

/* Length portion is the fraction of the total path length where we want the location */
void BKE_curveprofile_evaluate_length_portion(const struct CurveProfile *profile,
                                              float length_portion,
                                              float *x_out,
                                              float *y_out);

void BKE_curveprofile_blend_write(struct BlendWriter *writer, const struct CurveProfile *profile);
void BKE_curveprofile_blend_read(struct BlendDataReader *reader, struct CurveProfile *profile);

#ifdef __cplusplus
}
#endif

#endif
