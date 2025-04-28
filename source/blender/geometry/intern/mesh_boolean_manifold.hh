/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "GEO_mesh_boolean.hh"

namespace blender::geometry::boolean {

Mesh *mesh_boolean_manifold(Span<const Mesh *> meshes,
                            Span<float4x4> transforms,
                            Span<Array<short>> material_remaps,
                            BooleanOpParameters op_params,
                            Vector<int> *r_intersecting_edges,
                            BooleanError *r_error);

}
