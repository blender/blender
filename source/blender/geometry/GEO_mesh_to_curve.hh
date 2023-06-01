/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_index_mask.hh"

#include "BKE_curves.hh"

struct Mesh;

/** \file
 * \ingroup geo
 */

namespace blender::geometry {

/**
 * Convert the mesh into one or many poly curves. Since curves cannot have branches,
 * intersections of more than three edges will become breaks in curves. Attributes that
 * are not built-in on meshes and not curves are transferred to the result curve.
 */
bke::CurvesGeometry mesh_to_curve_convert(
    const Mesh &mesh,
    const IndexMask &selection,
    const bke::AnonymousAttributePropagationInfo &propagation_info);

bke::CurvesGeometry create_curve_from_vert_indices(
    const bke::AttributeAccessor &mesh_attributes,
    Span<int> vert_indices,
    Span<int> curve_offsets,
    IndexRange cyclic_curves,
    const bke::AnonymousAttributePropagationInfo &propagation_info);

}  // namespace blender::geometry
