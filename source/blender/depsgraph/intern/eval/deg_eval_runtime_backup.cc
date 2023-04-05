/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2017 Blender Foundation */

/** \file
 * \ingroup depsgraph
 */

#include "intern/eval/deg_eval_runtime_backup.h"

#include "intern/eval/deg_eval_copy_on_write.h"

#include "BLI_utildefines.h"

#include "DRW_engine.h"

namespace blender::deg {

RuntimeBackup::RuntimeBackup(const Depsgraph *depsgraph)
    : have_backup(false),
      id_data({nullptr}),
      animation_backup(depsgraph),
      scene_backup(depsgraph),
      sound_backup(depsgraph),
      object_backup(depsgraph),
      drawdata_ptr(nullptr),
      movieclip_backup(depsgraph),
      volume_backup(depsgraph),
      gpencil_backup(depsgraph)
{
  drawdata_backup.first = drawdata_backup.last = nullptr;
}

void RuntimeBackup::init_from_id(ID *id)
{
  if (!deg_copy_on_write_is_expanded(id)) {
    return;
  }
  have_backup = true;

  /* Clear, so freeing the expanded data doesn't touch this Python reference. */
  id_data.py_instance = id->py_instance;
  id->py_instance = nullptr;

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
    case ID_GD_LEGACY:
      gpencil_backup.init_from_gpencil(reinterpret_cast<bGPdata *>(id));
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

  id->py_instance = id_data.py_instance;

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
    case ID_GD_LEGACY:
      gpencil_backup.restore_to_gpencil(reinterpret_cast<bGPdata *>(id));
      break;
    default:
      break;
  }
  if (drawdata_ptr != nullptr) {
    *drawdata_ptr = drawdata_backup;
  }
}

}  // namespace blender::deg
