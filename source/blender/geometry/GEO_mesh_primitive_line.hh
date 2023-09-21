/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_vector_types.hh"

namespace blender::geometry {

Mesh *create_line_mesh(float3 start, float3 delta, int count);

}  // namespace blender::geometry
