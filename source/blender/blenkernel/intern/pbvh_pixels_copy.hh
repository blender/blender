/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_paint_bvh.hh"

#include "pbvh_uv_islands.hh"

namespace blender::bke::pbvh::pixels {

void copy_update(Tree &pbvh,
                 Image &image,
                 ImageUser &image_user,
                 const uv_islands::MeshData &mesh_data);

}  // namespace blender::bke::pbvh::pixels
