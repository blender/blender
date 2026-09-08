/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#pragma once

#include "NOD_nodes_srna.hh"

namespace blender {

struct bNodeTree;

namespace nodes {

std::shared_ptr<GeneratedTreeSrnaData> create_scene_compositor_effect_inputs_srna(
    const bNodeTree &tree);

}  // namespace nodes
}  // namespace blender
