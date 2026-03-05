/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_geometry_set.hh"
#include "BKE_node_socket_value.hh"

namespace blender::geometry {

void mix_geometries(bke::GeometrySet &a, const bke::GeometrySet &b, float factor);

void mix_bundles(nodes::Bundle &a, const nodes::Bundle &b, float factor);

/**
 * Mixes both geometries if possible (e.g. if corresponding meshes have the same number of
 * vertices), or index mapping is possible via the `id` attribute. Also mix single values, lists,
 * and bundle item values when types are compatible.
 */
void mix_socket_values(bke::SocketValueVariant &a,
                       const bke::SocketValueVariant &b,
                       const float factor);

}  // namespace blender::geometry
