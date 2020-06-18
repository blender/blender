/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 */

#include "intern/eval/deg_eval_runtime_backup_sequencer.h"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BKE_sequencer.h"
#include "BKE_sound.h"

namespace DEG {

SequencerBackup::SequencerBackup(const Depsgraph *depsgraph) : depsgraph(depsgraph)
{
}

void SequencerBackup::init_from_scene(Scene *scene)
{
  Sequence *sequence;
  SEQ_BEGIN (scene->ed, sequence) {
    SequenceBackup sequence_backup(depsgraph);
    sequence_backup.init_from_sequence(sequence);
    if (!sequence_backup.isEmpty()) {
      sequences_backup.add(sequence->orig_sequence, sequence_backup);
    }
  }
  SEQ_END;
}

void SequencerBackup::restore_to_scene(Scene *scene)
{
  Sequence *sequence;
  SEQ_BEGIN (scene->ed, sequence) {
    SequenceBackup *sequence_backup = sequences_backup.lookup_ptr(sequence->orig_sequence);
    if (sequence_backup != nullptr) {
      sequence_backup->restore_to_sequence(sequence);
    }
  }
  SEQ_END;
  /* Cleanup audio while the scene is still known. */
  for (SequenceBackup &sequence_backup : sequences_backup.values()) {
    if (sequence_backup.scene_sound != nullptr) {
      BKE_sound_remove_scene_sound(scene, sequence_backup.scene_sound);
    }
  }
}

}  // namespace DEG
