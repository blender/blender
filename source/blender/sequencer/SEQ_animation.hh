/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "DNA_listBase.h"

#include "ANIM_action.hh"
#include "ANIM_action_legacy.hh"

struct GSet;
struct ListBase;
struct Scene;
struct Sequence;
struct SeqAnimationBackup;

bool SEQ_animation_keyframes_exist(Scene *scene);
bool SEQ_animation_drivers_exist(Scene *scene);
void SEQ_free_animdata(Scene *scene, Sequence *seq);
void SEQ_offset_animdata(Scene *scene, Sequence *seq, int ofs);
/**
 * Return whether the fcurve targets the given sequence.
 */
bool SEQ_fcurve_matches(const Sequence &seq, const FCurve &fcurve);
struct SeqAnimationBackup {
  /* `curves` and `channel_bag` here represent effectively the same data (the
   * fcurves that animate the Scene that the sequence belongs to), just for
   * legacy and layered actions, respectively. Therefore only one or the other
   * should ever have data stored in them, never both. */
  ListBase curves;
  blender::animrig::ChannelBag channel_bag;

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
