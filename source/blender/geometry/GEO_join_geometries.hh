/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_geometry_set.hh"

namespace blender::geometry {

/**
 * \param allow_merging_instance_references: If true, instance references from multiple instances
 *   components may be merged when they are the same. This is typically good because it reduces the
 *   amount of processing for later nodes. However, this may be undesirable in some cases if the
 *   instance references are modified afterwards and the calling code assumes that the instances
 *   references are just concatenated.
 */
bke::GeometrySet join_geometries(Span<bke::GeometrySet> geometries,
                                 const bke::AttributeFilter &attribute_filter,
                                 const std::optional<Span<bke::GeometryComponent::Type>>
                                     &component_types_to_join = std::nullopt,
                                 bool allow_merging_instance_references = true);

}  // namespace blender::geometry
