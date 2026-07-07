/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

namespace blender {

struct Scene;
struct Strip;

namespace seq {

Strip *select_active_get(const Scene *scene);
bool select_active_get_pair(Scene *scene, Strip **r_strip_act, Strip **r_strip_other);
void select_active_set(Scene *scene, Strip *strip);

}  // namespace seq
}  // namespace blender
