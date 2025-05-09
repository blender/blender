/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_matrix_types.hh"

namespace blender::ocio {

class Config;

float3x3 calculate_white_point_matrix(const Config &config, float temperature, float tint);

}  // namespace blender::ocio
