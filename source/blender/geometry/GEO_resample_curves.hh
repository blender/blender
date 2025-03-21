/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "FN_field.hh"

#include "BKE_curves.hh"

namespace blender::geometry {

using bke::CurvesGeometry;

struct ResampleCurvesOutputAttributeIDs {
  std::optional<std::string> tangent_id;
  std::optional<std::string> normal_id;
};

/**
 * Create new curves where the selected curves have been resampled with a number of uniform-length
 * samples defined by the count field. Interpolate attributes to the result, with an accuracy that
 * depends on the curve's resolution parameter.
 *
 * \note The values provided by the #count_field are clamped to 1 or greater.
 */
CurvesGeometry resample_to_count(const CurvesGeometry &src_curves,
                                 const IndexMask &selection,
                                 const VArray<int> &counts,
                                 const ResampleCurvesOutputAttributeIDs &output_ids = {});
CurvesGeometry resample_to_count(const CurvesGeometry &src_curves,
                                 const fn::FieldContext &field_context,
                                 const fn::Field<bool> &selection_field,
                                 const fn::Field<int> &count_field,
                                 const ResampleCurvesOutputAttributeIDs &output_ids = {});

/**
 * Create new curves resampled to make each segment have the length specified by the
 * #segment_length field input, rounded to make the length of each segment the same.
 * The accuracy will depend on the curve's resolution parameter.
 *
 * \param keep_last_segment: If false, curves that are too short are collapsed to a single point.
 *   If true, they will have at least one segment after resampling. This mainly exists for
 *   compatibility.
 */
CurvesGeometry resample_to_length(const CurvesGeometry &src_curves,
                                  const IndexMask &selection,
                                  const VArray<float> &sample_lengths,
                                  const ResampleCurvesOutputAttributeIDs &output_ids = {},
                                  bool keep_last_segment = false);
CurvesGeometry resample_to_length(const CurvesGeometry &src_curves,
                                  const fn::FieldContext &field_context,
                                  const fn::Field<bool> &selection_field,
                                  const fn::Field<float> &segment_length_field,
                                  const ResampleCurvesOutputAttributeIDs &output_ids = {},
                                  bool keep_last_segment = false);

/**
 * Evaluate each selected curve to its implicit evaluated points.
 */
CurvesGeometry resample_to_evaluated(const CurvesGeometry &src_curves,
                                     const IndexMask &selection,
                                     const ResampleCurvesOutputAttributeIDs &output_ids = {});
CurvesGeometry resample_to_evaluated(const CurvesGeometry &src_curves,
                                     const fn::FieldContext &field_context,
                                     const fn::Field<bool> &selection_field,
                                     const ResampleCurvesOutputAttributeIDs &output_ids = {});

}  // namespace blender::geometry
