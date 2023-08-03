/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_geometry_set.hh"

struct Object;

namespace blender::bke {

/**
 * \note This doesn't extract instances from the "dupli" system for non-geometry-nodes instances.
 */
GeometrySet object_get_evaluated_geometry_set(const Object &object);

bool object_has_geometry_set_instances(const Object &object);

}  // namespace blender::bke
