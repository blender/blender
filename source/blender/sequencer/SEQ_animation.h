/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

#pragma once

/** \file
 * \ingroup sequencer
 */

#ifdef __cplusplus
extern "C" {
#endif

struct GSet;
struct ListBase;
struct Scene;
struct Sequence;

void SEQ_free_animdata(struct Scene *scene, struct Sequence *seq);
void SEQ_offset_animdata(struct Scene *scene, struct Sequence *seq, int ofs);
struct GSet *SEQ_fcurves_by_strip_get(const struct Sequence *seq, struct ListBase *fcurve_base);
/**
 * Move all `F-Curves` from `scene` to `list`.
 */
void SEQ_animation_backup_original(struct Scene *scene, struct ListBase *list);
/**
 * Move all `F-Curves` from `list` to `scene`.
 */
void SEQ_animation_restore_original(struct Scene *scene, struct ListBase *list);
/**
 * Duplicate `F-Curves` used by `seq` from `list` to `scene`.
 */
void SEQ_animation_duplicate(struct Scene *scene, struct Sequence *seq, struct ListBase *list);

#ifdef __cplusplus
}
#endif
