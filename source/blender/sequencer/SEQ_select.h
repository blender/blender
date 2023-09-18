/* SPDX-FileCopyrightText: 2004 Blender Authors
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
struct Sequence;

struct Sequence *SEQ_select_active_get(struct Scene *scene);
bool SEQ_select_active_get_pair(struct Scene *scene,
                                struct Sequence **r_seq_act,
                                struct Sequence **r_seq_other);
void SEQ_select_active_set(struct Scene *scene, struct Sequence *seq);

#ifdef __cplusplus
}
#endif
