/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_vector_types.hh"

#include <cstdint>

namespace blender::io::stl {

#pragma pack(push, 1)
struct PackedTriangle {
  float3 normal;
  float3 vertices[3];
  uint16_t attribute_byte_count;
};
#pragma pack(pop)

inline constexpr size_t BINARY_HEADER_SIZE = 80;
inline constexpr size_t BINARY_STRIDE = sizeof(PackedTriangle);

static_assert(sizeof(PackedTriangle) == 12 + (12 * 3) + 2,
              "PackedTriangle expected size mismatch");

}  // namespace blender::io::stl
