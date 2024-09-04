/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "BLI_string_ref.hh"

struct Mesh;

namespace blender::geometry {

Mesh *create_grid_mesh(int verts_x,
                       int verts_y,
                       float size_x,
                       float size_y,
                       const std::optional<StringRef> &uv_map_id);

}  // namespace blender::geometry
