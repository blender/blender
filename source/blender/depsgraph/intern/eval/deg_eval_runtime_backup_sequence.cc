/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#include "intern/eval/deg_eval_runtime_backup_sequence.h"

#include "DNA_sequence_types.h"

#include "BLI_listbase.h"

namespace blender::deg {

StripBackup::StripBackup(const Depsgraph * /*depsgraph*/)
{
  reset();
}

void StripBackup::reset()
{
  scene_sound = nullptr;
  BLI_listbase_clear(&anims);
}

void StripBackup::init_from_strip(Strip *strip)
{
  scene_sound = strip->scene_sound;
  anims = strip->anims;

  strip->scene_sound = nullptr;
  BLI_listbase_clear(&strip->anims);
}

void StripBackup::restore_to_strip(Strip *strip)
{
  strip->scene_sound = scene_sound;
  strip->anims = anims;
  reset();
}

bool StripBackup::isEmpty() const
{
  return (scene_sound == nullptr) && BLI_listbase_is_empty(&anims);
}

}  // namespace blender::deg
