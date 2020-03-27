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
 * The Original Code is Copyright (C) 2017 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 */

#include "intern/eval/deg_eval_runtime_backup.h"

#include "intern/eval/deg_eval_copy_on_write.h"

#include "BLI_utildefines.h"

#include "DRW_engine.h"

namespace DEG {

RuntimeBackup::RuntimeBackup(const Depsgraph *depsgraph)
    : have_backup(false),
      animation_backup(depsgraph),
      scene_backup(depsgraph),
      sound_backup(depsgraph),
      object_backup(depsgraph),
      drawdata_ptr(nullptr),
      movieclip_backup(depsgraph),
      volume_backup(depsgraph)
{
  drawdata_backup.first = drawdata_backup.last = nullptr;
}

void RuntimeBackup::init_from_id(ID *id)
{
  if (!deg_copy_on_write_is_expanded(id)) {
    return;
  }
  have_backup = true;

  animation_backup.init_from_id(id);

  const ID_Type id_type = GS(id->name);
  switch (id_type) {
    case ID_OB:
      object_backup.init_from_object(reinterpret_cast<Object *>(id));
      break;
    case ID_SCE:
      scene_backup.init_from_scene(reinterpret_cast<Scene *>(id));
      break;
    case ID_SO:
      sound_backup.init_from_sound(reinterpret_cast<bSound *>(id));
      break;
    case ID_MC:
      movieclip_backup.init_from_movieclip(reinterpret_cast<MovieClip *>(id));
      break;
    case ID_VO:
      volume_backup.init_from_volume(reinterpret_cast<Volume *>(id));
      break;
    default:
      break;
  }

  /* Note that we never free GPU draw data from here since that's not
   * safe for threading and draw data is likely to be re-used. */
  drawdata_ptr = DRW_drawdatalist_from_id(id);
  if (drawdata_ptr != nullptr) {
    drawdata_backup = *drawdata_ptr;
    drawdata_ptr->first = drawdata_ptr->last = nullptr;
  }
}

void RuntimeBackup::restore_to_id(ID *id)
{
  if (!have_backup) {
    return;
  }

  animation_backup.restore_to_id(id);

  const ID_Type id_type = GS(id->name);
  switch (id_type) {
    case ID_OB:
      object_backup.restore_to_object(reinterpret_cast<Object *>(id));
      break;
    case ID_SCE:
      scene_backup.restore_to_scene(reinterpret_cast<Scene *>(id));
      break;
    case ID_SO:
      sound_backup.restore_to_sound(reinterpret_cast<bSound *>(id));
      break;
    case ID_MC:
      movieclip_backup.restore_to_movieclip(reinterpret_cast<MovieClip *>(id));
      break;
    case ID_VO:
      volume_backup.restore_to_volume(reinterpret_cast<Volume *>(id));
      break;
    default:
      break;
  }
  if (drawdata_ptr != nullptr) {
    *drawdata_ptr = drawdata_backup;
  }
}

}  // namespace DEG
