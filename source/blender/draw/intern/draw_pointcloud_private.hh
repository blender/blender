/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

struct PointCloud;
struct GPUBatch;
namespace blender::gpu {
class VertBuf;
}
struct GPUMaterial;

namespace blender::draw {

gpu::VertBuf *pointcloud_position_and_radius_get(PointCloud *pointcloud);
GPUBatch **pointcloud_surface_shaded_get(PointCloud *pointcloud,
                                         GPUMaterial **gpu_materials,
                                         int mat_len);
GPUBatch *pointcloud_surface_get(PointCloud *pointcloud);

}  // namespace blender::draw
