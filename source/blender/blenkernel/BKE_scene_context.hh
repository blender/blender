/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

namespace blender {

struct bContext;
struct Scene;

namespace bke {

/**
 * Gets the scene based on the space/editor context.
 *
 * In the video sequence editor, this returns the sequencer scene, otherwise it returns the active
 * scene in the window.
 */
Scene *scene_or_sequencer_scene_from_context(const bContext &C);

}  // namespace bke
}  // namespace blender
