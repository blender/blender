/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_anonymous_attribute_id.hh"
#include "BKE_geometry_set.hh"

namespace blender::geometry {

bke::GeometrySet join_geometries(Span<bke::GeometrySet> geometries,
                                 const bke::AnonymousAttributePropagationInfo &propagation_info);

void join_attributes(const Span<const bke::GeometryComponent *> src_components,
                     bke::GeometryComponent &r_result,
                     const Span<StringRef> ignored_attributes = {});
}  // namespace blender::geometry
