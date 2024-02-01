/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_geometry_set.hh"

namespace blender::geometry {

/**
 * Mixes both geometries if possible (e.g. if corresponding meshes have the same number of
 * vertices).
 *
 * If mixing is not possible, the geometry from the `a` input is returned.
 */
bke::GeometrySet mix_geometries(bke::GeometrySet a, const bke::GeometrySet &b, float factor);

}  // namespace blender::geometry
