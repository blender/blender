/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_curves.hh"

/** \file
 * \ingroup bke
 * \brief Low-level operations for curves.
 */

namespace blender::bke::curves {

/**
 * Copy the size of every curve in #curve_ranges to the corresponding index in #counts.
 */
void fill_curve_counts(const bke::CurvesGeometry &curves,
                       Span<IndexRange> curve_ranges,
                       MutableSpan<int> counts);

/**
 * Turn an array of sizes into the offset at each index including all previous sizes.
 */
void accumulate_counts_to_offsets(MutableSpan<int> counts_to_offsets, int start_offset = 0);

}  // namespace blender::bke::curves
