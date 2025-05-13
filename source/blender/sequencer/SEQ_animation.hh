/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "DNA_listBase.h"

#include "ANIM_action.hh"

struct ListBase;
struct Scene;
struct Strip;
struct SeqAnimationBackup;
namespace blender::seq {

bool animation_keyframes_exist(const Scene *scene);
bool animation_drivers_exist(Scene *scene);
void free_animdata(Scene *scene, Strip *strip);
void offset_animdata(const Scene *scene, Strip *strip, float ofs);

/**
 * Return whether the fcurve targets the given strip.
 */
bool fcurve_matches(const Strip &strip, const FCurve &fcurve);
struct AnimationBackup {
  /* `curves` and `channelbag` here represent effectively the same data (the
   * fcurves that animate the Scene that the sequence belongs to), just for
   * legacy and layered actions, respectively. Therefore only one or the other
   * should ever have data stored in them, never both. */
  ListBase curves;
  blender::animrig::Channelbag channelbag;

  ListBase drivers;
};
/**
 * Move all F-Curves and drivers from `scene` to `backup`.
 */
void animation_backup_original(Scene *scene, AnimationBackup *backup);
/**
 * Move all F-Curves and drivers from `backup` to `scene`.
 */
void animation_restore_original(Scene *scene, AnimationBackup *backup);
/**
 * Duplicate F-Curves and drivers used by `strip` from `backup` to `scene`.
 */
void animation_duplicate_backup_to_scene(Scene *scene, Strip *strip, AnimationBackup *backup);

}  // namespace blender::seq
