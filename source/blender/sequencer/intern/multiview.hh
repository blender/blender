/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include <cstdlib>

#include "DNA_scene_enums.h"

namespace blender {

struct MovieReader;
struct Scene;

namespace seq {

void seq_anim_add_suffix(Scene *scene, MovieReader *anim, int view_id);
void seq_multiview_name(
    Scene *scene, int view_id, const char *prefix, const char *ext, char *r_path, size_t r_size);
/**
 * The number of files that a multi-view enabled strip needs to resolve the requested
 * \a views_format.
 */
int seq_multiview_num_files_get(const Scene *scene, eImageFormat_ViewsFormat views_format);

}  // namespace seq
}  // namespace blender
