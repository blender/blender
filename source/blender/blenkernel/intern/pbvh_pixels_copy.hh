/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_vector_types.hh"
#include "BLI_vector.hh"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "BKE_image_wrappers.hh"
#include "BKE_pbvh.hh"
#include "BKE_pbvh_pixels.hh"

#include "pbvh_uv_islands.hh"

namespace blender::bke::pbvh::pixels {

void copy_update(PBVH &pbvh,
                 Image &image,
                 ImageUser &image_user,
                 const uv_islands::MeshData &mesh_data);

}  // namespace blender::bke::pbvh::pixels
