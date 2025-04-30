/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

struct Scene;
struct Strip;

namespace blender::seq {

bool sequencer_strip_generates_image(Strip *strip);
void strip_open_anim_file(Scene *scene, Strip *strip, bool openfile);

}  // namespace blender::seq
