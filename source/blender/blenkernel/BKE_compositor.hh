/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include <string>

#include "BLI_set.hh"

namespace blender {

struct Scene;
struct ViewLayer;
struct bContext;

namespace bke::compositor {

/* Get the set of all passes used by the compositor for the given view layer, identified by their
 * pass names. This might be a superset of the passes actually supported by the render engine, in
 * which case, the compositor will return an invalid output and issue a warning. */
Set<std::string> get_used_passes(const Scene &scene, const ViewLayer *view_layer);

/* Checks if the viewport compositor is currently being used. This is similar to
 * DRWContext::is_viewport_compositor_enabled but checks all 3D views. */
bool is_viewport_compositor_used(const bContext &context);

}  // namespace bke::compositor
}  // namespace blender
