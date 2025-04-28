/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include <string>

#include "BLI_set.hh"

struct Scene;
struct ViewLayer;

namespace blender::bke::compositor {

/* Get the set of all passes used by the compositor for the given view layer, identified by their
 * pass names. */
Set<std::string> get_used_passes(const Scene &scene, const ViewLayer *view_layer);

}  // namespace blender::bke::compositor
