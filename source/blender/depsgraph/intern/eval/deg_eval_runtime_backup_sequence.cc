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

#include "intern/eval/deg_eval_runtime_backup_sequence.h"

#include "DNA_sequence_types.h"

namespace blender::deg {

SequenceBackup::SequenceBackup(const Depsgraph * /*depsgraph*/)
{
  reset();
}

void SequenceBackup::reset()
{
  scene_sound = nullptr;
  BLI_listbase_clear(&anims);
}

void SequenceBackup::init_from_sequence(Sequence *sequence)
{
  scene_sound = sequence->scene_sound;
  anims = sequence->anims;

  sequence->scene_sound = nullptr;
  BLI_listbase_clear(&sequence->anims);
}

void SequenceBackup::restore_to_sequence(Sequence *sequence)
{
  sequence->scene_sound = scene_sound;
  sequence->anims = anims;
  reset();
}

bool SequenceBackup::isEmpty() const
{
  return (scene_sound == nullptr) && BLI_listbase_is_empty(&anims);
}

}  // namespace blender::deg
