/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include "IO_ply.hh"
#include "ply_data.hh"

namespace blender {

struct PointCloud;

namespace io::ply {

/**
 * Converts the #PlyData data-structure to a point cloud that represents gsplat.
 * \return A new pointcloud that can be used inside blender.
 *
 * NOTE: #PlyData must represent gaussian splat (contain all related attributes).
 */
PointCloud *convert_gsplat_ply_to_point_cloud(const PlyData &data, const PLYImportParams &params);

}  // namespace io::ply
}  // namespace blender
