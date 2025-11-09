/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

struct MovieReader;
struct Scene;

namespace blender::seq {

void seq_anim_add_suffix(Scene *scene, MovieReader *anim, int view_id);
void seq_multiview_name(
    Scene *scene, int view_id, const char *prefix, const char *ext, char *r_path, size_t r_size);
/**
 * The number of files will vary according to the stereo format.
 */
int seq_num_files(Scene *scene, char views_format, bool is_multiview);

}  // namespace blender::seq
