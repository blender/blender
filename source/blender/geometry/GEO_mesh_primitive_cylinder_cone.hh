/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>
#include <string>

struct Mesh;

namespace blender::geometry {

struct ConeAttributeOutputs {
  std::optional<std::string> top_id;
  std::optional<std::string> bottom_id;
  std::optional<std::string> side_id;
  std::optional<std::string> uv_map_id;
};

enum class ConeFillType {
  None = 0,
  NGon = 1,
  Triangles = 2,
};

Mesh *create_cylinder_or_cone_mesh(float radius_top,
                                   float radius_bottom,
                                   float depth,
                                   int circle_segments,
                                   int side_segments,
                                   int fill_segments,
                                   ConeFillType fill_type,
                                   ConeAttributeOutputs &attribute_outputs);

}  // namespace blender::geometry
