/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#include "intern/eval/deg_eval_runtime_backup_sequencer.h"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BLI_assert.h"

#include "BKE_sound.h"

#include "SEQ_iterator.h"

namespace blender::deg {

SequencerBackup::SequencerBackup(const Depsgraph *depsgraph) : depsgraph(depsgraph) {}

static bool seq_init_cb(Sequence *seq, void *user_data)
{
  SequencerBackup *sb = (SequencerBackup *)user_data;
  SequenceBackup sequence_backup(sb->depsgraph);
  sequence_backup.init_from_sequence(seq);
  if (!sequence_backup.isEmpty()) {
    const SessionUUID &session_uuid = seq->runtime.session_uuid;
    BLI_assert(BLI_session_uuid_is_generated(&session_uuid));
    sb->sequences_backup.add(session_uuid, sequence_backup);
  }
  return true;
}

void SequencerBackup::init_from_scene(Scene *scene)
{
  if (scene->ed != nullptr) {
    SEQ_for_each_callback(&scene->ed->seqbase, seq_init_cb, this);
  }
}

static bool seq_restore_cb(Sequence *seq, void *user_data)
{
  SequencerBackup *sb = (SequencerBackup *)user_data;
  const SessionUUID &session_uuid = seq->runtime.session_uuid;
  BLI_assert(BLI_session_uuid_is_generated(&session_uuid));
  SequenceBackup *sequence_backup = sb->sequences_backup.lookup_ptr(session_uuid);
  if (sequence_backup != nullptr) {
    sequence_backup->restore_to_sequence(seq);
  }
  return true;
}

void SequencerBackup::restore_to_scene(Scene *scene)
{
  if (scene->ed != nullptr) {
    SEQ_for_each_callback(&scene->ed->seqbase, seq_restore_cb, this);
  }
  /* Cleanup audio while the scene is still known. */
  for (SequenceBackup &sequence_backup : sequences_backup.values()) {
    if (sequence_backup.scene_sound != nullptr) {
      BKE_sound_remove_scene_sound(scene, sequence_backup.scene_sound);
    }
  }
}

}  // namespace blender::deg
