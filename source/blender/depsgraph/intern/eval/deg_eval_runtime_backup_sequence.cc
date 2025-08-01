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

StripModifierDataBackup::StripModifierDataBackup()
{
  reset();
}

void StripModifierDataBackup::reset()
{
  sound_in = nullptr;
  sound_out = nullptr;
  last_buf = nullptr;
}

void StripModifierDataBackup::init_from_modifier(StripModifierData *smd)
{
  if (smd->type == seqModifierType_SoundEqualizer) {
    sound_in = smd->runtime.last_sound_in;
    sound_out = smd->runtime.last_sound_out;
    last_buf = smd->runtime.last_buf;

    smd->runtime.last_sound_in = nullptr;
    smd->runtime.last_sound_out = nullptr;
    smd->runtime.last_buf = nullptr;
  }
}

void StripModifierDataBackup::restore_to_modifier(StripModifierData *smd)
{
  if (smd->type == seqModifierType_SoundEqualizer) {
    smd->runtime.last_sound_in = sound_in;
    smd->runtime.last_sound_out = sound_out;
    smd->runtime.last_buf = last_buf;
  }
  reset();
}

bool StripModifierDataBackup::isEmpty() const
{
  return sound_in == nullptr && sound_out == nullptr && last_buf == nullptr;
}

StripBackup::StripBackup(const Depsgraph * /*depsgraph*/)
{
  reset();
}

void StripBackup::reset()
{
  scene_sound = nullptr;
  BLI_listbase_clear(&anims);
  modifiers.clear();
}

void StripBackup::init_from_strip(Strip *strip)
{
  scene_sound = strip->scene_sound;
  anims = strip->anims;

  LISTBASE_FOREACH (StripModifierData *, smd, &strip->modifiers) {
    StripModifierDataBackup mod_backup;
    mod_backup.init_from_modifier(smd);
    if (!mod_backup.isEmpty()) {
      modifiers.add(smd->persistent_uid, mod_backup);
    }
  }

  strip->scene_sound = nullptr;
  BLI_listbase_clear(&strip->anims);
}

void StripBackup::restore_to_strip(Strip *strip)
{
  strip->scene_sound = scene_sound;
  strip->anims = anims;

  LISTBASE_FOREACH (StripModifierData *, smd, &strip->modifiers) {
    std::optional<StripModifierDataBackup> backup = modifiers.pop_try(smd->persistent_uid);
    if (backup.has_value()) {
      backup->restore_to_modifier(smd);
    }
  }

  reset();
}

bool StripBackup::isEmpty() const
{
  return (scene_sound == nullptr) && BLI_listbase_is_empty(&anims) && modifiers.is_empty();
}

}  // namespace blender::deg
