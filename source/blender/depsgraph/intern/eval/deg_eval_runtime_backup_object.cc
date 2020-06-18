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

#include "intern/eval/deg_eval_runtime_backup_object.h"

#include <cstring>

#include "DNA_mesh_types.h"

#include "BLI_listbase.h"

#include "BKE_action.h"
#include "BKE_object.h"

namespace DEG {

ObjectRuntimeBackup::ObjectRuntimeBackup(const Depsgraph * /*depsgraph*/)
    : base_flag(0), base_local_view_bits(0)
{
  /* TODO(sergey): Use something like BKE_object_runtime_reset(). */
  memset(&runtime, 0, sizeof(runtime));
}

void ObjectRuntimeBackup::init_from_object(Object *object)
{
  /* Store evaluated mesh and curve_cache, and make sure we don't free it. */
  runtime = object->runtime;
  BKE_object_runtime_reset(object);
  /* Keep bbox (for now at least). */
  object->runtime.bb = runtime.bb;
  /* Object update will override actual object->data to an evaluated version.
   * Need to make sure we don't have data set to evaluated one before free
   * anything. */
  object->data = runtime.data_orig;
  /* Make a backup of base flags. */
  base_flag = object->base_flag;
  base_local_view_bits = object->base_local_view_bits;
  /* Backup tuntime data of all modifiers. */
  backup_modifier_runtime_data(object);
  /* Backup runtime data of all pose channels. */
  backup_pose_channel_runtime_data(object);
}

inline ModifierDataBackupID create_modifier_data_id(const ModifierData *modifier_data)
{
  return ModifierDataBackupID(modifier_data->orig_modifier_data,
                              static_cast<ModifierType>(modifier_data->type));
}

void ObjectRuntimeBackup::backup_modifier_runtime_data(Object *object)
{
  LISTBASE_FOREACH (ModifierData *, modifier_data, &object->modifiers) {
    if (modifier_data->runtime == nullptr) {
      continue;
    }
    BLI_assert(modifier_data->orig_modifier_data != nullptr);
    ModifierDataBackupID modifier_data_id = create_modifier_data_id(modifier_data);
    modifier_runtime_data.add(modifier_data_id, modifier_data->runtime);
    modifier_data->runtime = nullptr;
  }
}

void ObjectRuntimeBackup::backup_pose_channel_runtime_data(Object *object)
{
  if (object->pose != nullptr) {
    LISTBASE_FOREACH (bPoseChannel *, pchan, &object->pose->chanbase) {
      /* This is nullptr in Edit mode. */
      if (pchan->orig_pchan != nullptr) {
        pose_channel_runtime_data.add(pchan->orig_pchan, pchan->runtime);
        BKE_pose_channel_runtime_reset(&pchan->runtime);
      }
    }
  }
}

void ObjectRuntimeBackup::restore_to_object(Object *object)
{
  ID *data_orig = object->runtime.data_orig;
  ID *data_eval = runtime.data_eval;
  BoundBox *bb = object->runtime.bb;
  object->runtime = runtime;
  object->runtime.data_orig = data_orig;
  object->runtime.bb = bb;
  if (object->type == OB_MESH && data_eval != nullptr) {
    if (object->id.recalc & ID_RECALC_GEOMETRY) {
      /* If geometry is tagged for update it means, that part of
       * evaluated mesh are not valid anymore. In this case we can not
       * have any "persistent" pointers to point to an invalid data.
       *
       * We restore object's data datablock to an original copy of
       * that datablock. */
      object->data = data_orig;

      /* After that, immediately free the invalidated caches. */
      BKE_object_free_derived_caches(object);
    }
    else {
      /* Do same thing as object update: override actual object data
       * pointer with evaluated datablock. */
      object->data = data_eval;

      /* Evaluated mesh simply copied edit_mesh pointer from
       * original mesh during update, need to make sure no dead
       * pointers are left behind. */
      if (object->type == OB_MESH) {
        Mesh *mesh_eval = (Mesh *)data_eval;
        Mesh *mesh_orig = (Mesh *)data_orig;
        mesh_eval->edit_mesh = mesh_orig->edit_mesh;
      }
    }
  }
  else if (ELEM(object->type, OB_HAIR, OB_POINTCLOUD, OB_VOLUME)) {
    if (object->id.recalc & ID_RECALC_GEOMETRY) {
      /* Free evaluated caches. */
      object->data = data_orig;
      BKE_object_free_derived_caches(object);
    }
    else {
      object->data = object->runtime.data_eval;
    }
  }

  object->base_flag = base_flag;
  object->base_local_view_bits = base_local_view_bits;
  /* Restore modifier's runtime data.
   * NOTE: Data of unused modifiers will be freed there. */
  restore_modifier_runtime_data(object);
  restore_pose_channel_runtime_data(object);
}

void ObjectRuntimeBackup::restore_modifier_runtime_data(Object *object)
{
  LISTBASE_FOREACH (ModifierData *, modifier_data, &object->modifiers) {
    BLI_assert(modifier_data->orig_modifier_data != nullptr);
    ModifierDataBackupID modifier_data_id = create_modifier_data_id(modifier_data);
    void *runtime = modifier_runtime_data.pop_default(modifier_data_id, nullptr);
    if (runtime != nullptr) {
      modifier_data->runtime = runtime;
    }
  }

  for (ModifierRuntimeDataBackup::Item item : modifier_runtime_data.items()) {
    const ModifierTypeInfo *modifier_type_info = BKE_modifier_get_info(item.key.type);
    BLI_assert(modifier_type_info != nullptr);
    modifier_type_info->freeRuntimeData(item.value);
  }
}

void ObjectRuntimeBackup::restore_pose_channel_runtime_data(Object *object)
{
  if (object->pose != nullptr) {
    LISTBASE_FOREACH (bPoseChannel *, pchan, &object->pose->chanbase) {
      /* This is nullptr in Edit mode. */
      if (pchan->orig_pchan != nullptr) {
        Optional<bPoseChannel_Runtime> runtime = pose_channel_runtime_data.pop_try(
            pchan->orig_pchan);
        if (runtime.has_value()) {
          pchan->runtime = runtime.extract();
        }
      }
    }
  }
  for (bPoseChannel_Runtime &runtime : pose_channel_runtime_data.values()) {
    BKE_pose_channel_runtime_free(&runtime);
  }
}

}  // namespace DEG
