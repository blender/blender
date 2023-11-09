/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_attribute.hh"

struct Mesh;

namespace blender::geometry {

struct ConeAttributeOutputs {
  bke::AnonymousAttributeIDPtr top_id;
  bke::AnonymousAttributeIDPtr bottom_id;
  bke::AnonymousAttributeIDPtr side_id;
  bke::AnonymousAttributeIDPtr uv_map_id;
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
