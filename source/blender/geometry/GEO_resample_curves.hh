/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "FN_field.hh"

#include "BKE_geometry_set.hh"

struct Curves;

namespace blender::geometry {

/**
 * Create new curves where the selected curves have been resampled with a number of uniform-length
 * samples defined by the count field. Interpolate attributes to the result, with an accuracy that
 * depends on the curve's resolution parameter.
 *
 * \note The values provided by the #count_field are clamped to 1 or greater.
 */
Curves *resample_to_count(const CurveComponent &src_component,
                          const fn::Field<bool> &selection_field,
                          const fn::Field<int> &count_field);

/**
 * Create new curves resampled to make each segment have the length specified by the
 * #segment_length field input, rounded to make the length of each segment the same.
 * The accuracy will depend on the curve's resolution parameter.
 */
Curves *resample_to_length(const CurveComponent &src_component,
                           const fn::Field<bool> &selection_field,
                           const fn::Field<float> &segment_length_field);

/**
 * Evaluate each selected curve to its implicit evaluated points.
 */
Curves *resample_to_evaluated(const CurveComponent &src_component,
                              const fn::Field<bool> &selection_field);

}  // namespace blender::geometry
