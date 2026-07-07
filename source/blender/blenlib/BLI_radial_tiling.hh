/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#pragma once

#include "BLI_math_vector_types.hh"

namespace blender {

float4 calculate_out_variables(bool calculate_r_gon_parameter_field,
                               bool calculate_max_unit_parameter,
                               bool normalize_r_gon_parameter,
                               float r_gon_sides,
                               float r_gon_roundness,
                               float2 coord);

float calculate_out_segment_id(float r_gon_sides, float2 coord);

}  // namespace blender
