/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation */

/** \file
 * \ingroup depsgraph
 */

#include "intern/eval/deg_eval_runtime_backup_object.h"

#include <cstring>

#include "DNA_mesh_types.h"

#include "BLI_listbase.h"

#include "BKE_action.h"
#include "BKE_object.h"

namespace blender::deg {

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
  if (object->light_linking) {
    light_linking_runtime = object->light_linking->runtime;
  }
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
  /* Backup runtime data of all modifiers. */
  backup_modifier_runtime_data(object);
  /* Backup runtime data of all pose channels. */
  backup_pose_channel_runtime_data(object);
}

void ObjectRuntimeBackup::backup_modifier_runtime_data(Object *object)
{
  LISTBASE_FOREACH (ModifierData *, modifier_data, &object->modifiers) {
    if (modifier_data->runtime == nullptr) {
      continue;
    }

    const SessionUUID &session_uuid = modifier_data->session_uuid;
    BLI_assert(BLI_session_uuid_is_generated(&session_uuid));

    modifier_runtime_data.add(session_uuid, ModifierDataBackup(modifier_data));
    modifier_data->runtime = nullptr;
  }
}

void ObjectRuntimeBackup::backup_pose_channel_runtime_data(Object *object)
{
  if (object->pose != nullptr) {
    LISTBASE_FOREACH (bPoseChannel *, pchan, &object->pose->chanbase) {
      const SessionUUID &session_uuid = pchan->runtime.session_uuid;
      BLI_assert(BLI_session_uuid_is_generated(&session_uuid));

      pose_channel_runtime_data.add(session_uuid, pchan->runtime);
      BKE_pose_channel_runtime_reset(&pchan->runtime);
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
  if (ELEM(object->type, OB_MESH, OB_LATTICE, OB_CURVES_LEGACY, OB_FONT) && data_eval != nullptr) {
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
      /* Do same thing as object update: override actual object data pointer with evaluated
       * datablock, but only if the evaluated data has the same type as the original data. */
      if (GS(((ID *)object->data)->name) == GS(data_eval->name)) {
        object->data = data_eval;
      }

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
  else if (ELEM(object->type, OB_CURVES, OB_POINTCLOUD, OB_VOLUME, OB_GREASE_PENCIL)) {
    if (object->id.recalc & ID_RECALC_GEOMETRY) {
      /* Free evaluated caches. */
      object->data = data_orig;
      BKE_object_free_derived_caches(object);
    }
    else {
      object->data = object->runtime.data_eval;
    }
  }

  if (light_linking_runtime) {
    /* Lazily allocate light linking on the evaluated object for the cases when the object is only
     * a receiver or a blocker and does not need its own LightLinking on the original object. */
    if (!object->light_linking) {
      object->light_linking = MEM_cnew<LightLinking>(__func__);
    }
    object->light_linking->runtime = *light_linking_runtime;
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
    const SessionUUID &session_uuid = modifier_data->session_uuid;
    BLI_assert(BLI_session_uuid_is_generated(&session_uuid));

    optional<ModifierDataBackup> backup = modifier_runtime_data.pop_try(session_uuid);
    if (backup.has_value()) {
      modifier_data->runtime = backup->runtime;
    }
  }

  for (ModifierDataBackup &backup : modifier_runtime_data.values()) {
    const ModifierTypeInfo *modifier_type_info = BKE_modifier_get_info(backup.type);
    BLI_assert(modifier_type_info != nullptr);
    modifier_type_info->freeRuntimeData(backup.runtime);
  }
}

void ObjectRuntimeBackup::restore_pose_channel_runtime_data(Object *object)
{
  if (object->pose != nullptr) {
    LISTBASE_FOREACH (bPoseChannel *, pchan, &object->pose->chanbase) {
      const SessionUUID &session_uuid = pchan->runtime.session_uuid;
      optional<bPoseChannel_Runtime> runtime = pose_channel_runtime_data.pop_try(session_uuid);
      if (runtime.has_value()) {
        pchan->runtime = *runtime;
      }
    }
  }
  for (bPoseChannel_Runtime &runtime : pose_channel_runtime_data.values()) {
    BKE_pose_channel_runtime_free(&runtime);
  }
}

}  // namespace blender::deg
