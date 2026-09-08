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
void seq_multiview_name(const Scene *scene,
                        int view_id,
                        const char *prefix,
                        const char *ext,
                        char *r_path,
                        size_t r_path_maxncpy);

/** Resolve one individual-view filepath. The returned suffix is optional. */
bool seq_multiview_view_filepath_get(const Scene &scene,
                                     const char *filepath,
                                     int view_id,
                                     char *r_filepath,
                                     size_t r_filepath_maxncpy,
                                     const char **r_suffix);
/**
 * The number of files that a multi-view enabled strip needs to resolve the requested
 * \a views_format.
 */
int seq_multiview_num_files_get(const Scene *scene, eImageFormat_ViewsFormat views_format);

}  // namespace seq
}  // namespace blender
