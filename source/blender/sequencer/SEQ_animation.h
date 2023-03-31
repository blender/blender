/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

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
struct SeqAnimationBackup;

bool SEQ_animation_curves_exist(struct Scene *scene);
bool SEQ_animation_drivers_exist(struct Scene *scene);
void SEQ_free_animdata(struct Scene *scene, struct Sequence *seq);
void SEQ_offset_animdata(struct Scene *scene, struct Sequence *seq, int ofs);
struct GSet *SEQ_fcurves_by_strip_get(const struct Sequence *seq, struct ListBase *fcurve_base);
typedef struct SeqAnimationBackup {
  ListBase curves;
  ListBase drivers;
} SeqAnimationBackup;
/**
 * Move all F-Curves and drivers from `scene` to `backup`.
 */
void SEQ_animation_backup_original(struct Scene *scene, struct SeqAnimationBackup *backup);
/**
 * Move all F-Curves and drivers from `backup` to `scene`.
 */
void SEQ_animation_restore_original(struct Scene *scene, struct SeqAnimationBackup *backup);
/**
 * Duplicate F-Curves and drivers used by `seq` from `backup` to `scene`.
 */
void SEQ_animation_duplicate_backup_to_scene(struct Scene *scene,
                                             struct Sequence *seq,
                                             struct SeqAnimationBackup *backup);

#ifdef __cplusplus
}
#endif
