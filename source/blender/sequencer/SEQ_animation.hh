/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "DNA_listBase.h"

struct GSet;
struct ListBase;
struct Scene;
struct Sequence;
struct SeqAnimationBackup;

bool SEQ_animation_curves_exist(Scene *scene);
bool SEQ_animation_drivers_exist(Scene *scene);
void SEQ_free_animdata(Scene *scene, Sequence *seq);
void SEQ_offset_animdata(Scene *scene, Sequence *seq, int ofs);
GSet *SEQ_fcurves_by_strip_get(const Sequence *seq, ListBase *fcurve_base);
struct SeqAnimationBackup {
  ListBase curves;
  ListBase drivers;
};
/**
 * Move all F-Curves and drivers from `scene` to `backup`.
 */
void SEQ_animation_backup_original(Scene *scene, SeqAnimationBackup *backup);
/**
 * Move all F-Curves and drivers from `backup` to `scene`.
 */
void SEQ_animation_restore_original(Scene *scene, SeqAnimationBackup *backup);
/**
 * Duplicate F-Curves and drivers used by `seq` from `backup` to `scene`.
 */
void SEQ_animation_duplicate_backup_to_scene(Scene *scene,
                                             Sequence *seq,
                                             SeqAnimationBackup *backup);
