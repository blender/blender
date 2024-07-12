/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_curves.hh"

namespace blender::geometry {

/**
 * Join each selected curve's end point with another curve's start point to form a single curve.
 *
 * \param connect_to_curve: Index of the curve to connect to, invalid indices are ignored
 *                          (set to -1 to leave a curve disconnected).
 * \param flip_direction: Flip direction of input curves.
 */
bke::CurvesGeometry curves_merge_endpoints(
    const bke::CurvesGeometry &src_curves,
    Span<int> connect_to_curve,
    Span<bool> flip_direction,
    const bke::AnonymousAttributePropagationInfo &propagation_info);

};  // namespace blender::geometry
