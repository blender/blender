/* SPDX-FileCopyrightText: 2004 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Scene;

/* **********************************************************************
 * sequencer.c
 *
 * Sequencer editing functions
 * **********************************************************************
 */

void seq_anim_add_suffix(struct Scene *scene, struct anim *anim, int view_id);
void seq_multiview_name(struct Scene *scene,
                        int view_id,
                        const char *prefix,
                        const char *ext,
                        char *r_path,
                        size_t r_size);
/**
 * The number of files will vary according to the stereo format.
 */
int seq_num_files(struct Scene *scene, char views_format, bool is_multiview);

#ifdef __cplusplus
}
#endif
