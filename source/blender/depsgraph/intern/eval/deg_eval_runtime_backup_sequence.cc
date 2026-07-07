/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#include "intern/eval/deg_eval_runtime_backup_sequence.h"

#include "DNA_sequence_types.h"

#include "SEQ_modifier.hh"
#include "SEQ_sequencer.hh"

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
  flag = STRIP_MODIFIER_FLAG_NONE;
  params_hash = 0;
}

void StripModifierDataBackup::init_from_modifier(StripModifierData *smd)
{
  blender::seq::StripModifierDataRuntime *runtime = smd->runtime;

  if (ELEM(smd->type,
           eSeqModifierType_SoundEqualizer,
           eSeqModifierType_Pitch,
           eSeqModifierType_Echo))
  {
    flag = runtime->flag;
    sound_in = runtime->last_sound_in;
    sound_out = runtime->last_sound_out;
    params_hash = runtime->params_hash;

    runtime->last_sound_in = nullptr;
    runtime->last_sound_out = nullptr;
  }
}

void StripModifierDataBackup::restore_to_modifier(StripModifierData *smd)
{
  blender::seq::StripModifierDataRuntime *runtime = smd->runtime;

  if (ELEM(smd->type,
           eSeqModifierType_SoundEqualizer,
           eSeqModifierType_Pitch,
           eSeqModifierType_Echo))
  {
    runtime->flag = flag;
    runtime->last_sound_in = sound_in;
    runtime->last_sound_out = sound_out;
    runtime->params_hash = params_hash;
  }
  reset();
}

bool StripModifierDataBackup::isEmpty() const
{
  return sound_in == nullptr && sound_out == nullptr;
}

StripBackup::StripBackup(const Depsgraph * /*depsgraph*/)
{
  reset();
}

void StripBackup::reset()
{
  scene_sound = nullptr;
  sound_time_stretch = nullptr;
  sound_time_stretch_fps = 0.0f;
  movie_readers.clear();
  modifiers.clear();
}

void StripBackup::init_from_strip(Strip *strip)
{
  scene_sound = strip->runtime->scene_sound;
  sound_time_stretch = strip->runtime->sound_time_stretch;
  sound_time_stretch_fps = strip->runtime->sound_time_stretch_fps;
  movie_readers = std::move(strip->runtime->movie_readers);

  for (StripModifierData &smd : strip->modifiers) {
    StripModifierDataBackup mod_backup;
    mod_backup.init_from_modifier(&smd);
    if (!mod_backup.isEmpty()) {
      modifiers.add(smd.persistent_uid, mod_backup);
    }
  }

  strip->runtime->scene_sound = nullptr;
  strip->runtime->sound_time_stretch = nullptr;
  strip->runtime->sound_time_stretch_fps = 0.0f;
  strip->runtime->movie_readers.clear();
}

void StripBackup::restore_to_strip(Strip *strip)
{
  strip->runtime->scene_sound = scene_sound;
  strip->runtime->sound_time_stretch = sound_time_stretch;
  strip->runtime->sound_time_stretch_fps = sound_time_stretch_fps;
  strip->runtime->movie_readers = std::move(movie_readers);

  for (StripModifierData &smd : strip->modifiers) {
    std::optional<StripModifierDataBackup> backup = modifiers.pop_try(smd.persistent_uid);
    if (backup) {
      backup->restore_to_modifier(&smd);
    }
  }

  reset();
}

bool StripBackup::isEmpty() const
{
  return (scene_sound == nullptr) && (sound_time_stretch == nullptr) && movie_readers.is_empty() &&
         modifiers.is_empty();
}

}  // namespace blender::deg
