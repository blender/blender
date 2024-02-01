/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_attribute.hh"
#include "BKE_geometry_set.hh"

#include "DNA_node_types.h"

#include "FN_field.hh"

namespace blender::geometry {

/**
 * Returns the parts of the geometry that are on the selection for the given domain. If the domain
 * is not applicable for the component, e.g. face domain for point cloud, nothing happens to that
 * component. If no component can work with the domain, then `error_message` is set to true.
 */
void separate_geometry(bke::GeometrySet &geometry_set,
                       bke::AttrDomain domain,
                       GeometryNodeDeleteGeometryMode mode,
                       const fn::Field<bool> &selection_field,
                       const bke::AnonymousAttributePropagationInfo &propagation_info,
                       bool &r_is_error);

}  // namespace blender::geometry
