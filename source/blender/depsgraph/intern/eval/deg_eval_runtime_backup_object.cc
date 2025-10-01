/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#include "BLI_session_uid.h"

#include "intern/eval/deg_eval_runtime_backup_object.h"

#include <cstring>

#include "DNA_mesh_types.h"

#include "BLI_listbase.h"

#include "BKE_action.hh"
#include "BKE_light_linking.h"
#include "BKE_mesh_types.hh"
#include "BKE_modifier.hh"
#include "BKE_object.hh"
#include "BKE_object_types.hh"

namespace blender::deg {

ObjectRuntimeBackup::ObjectRuntimeBackup(const Depsgraph * /*depsgraph*/)
    : base_flag(0), base_local_view_bits(0)
{
  /* TODO(sergey): Use something like BKE_object_runtime_reset(). */
  runtime = {};
}

void ObjectRuntimeBackup::init_from_object(Object *object)
{
  /* Store evaluated mesh and curve_cache, and make sure we don't free it. */
  runtime = *object->runtime;
  if (object->light_linking) {
    light_linking_runtime = object->light_linking->runtime;
  }
  BKE_object_runtime_reset(object);
  /* Keep bounding-box (for now at least). */
  object->runtime->bounds_eval = runtime.bounds_eval;
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

    modifier_runtime_data.add(modifier_data->persistent_uid, ModifierDataBackup(modifier_data));
    modifier_data->runtime = nullptr;
  }
}

void ObjectRuntimeBackup::backup_pose_channel_runtime_data(Object *object)
{
  if (object->pose != nullptr) {
    LISTBASE_FOREACH (bPoseChannel *, pchan, &object->pose->chanbase) {
      const SessionUID &session_uid = pchan->runtime.session_uid;
      BLI_assert(BLI_session_uid_is_generated(&session_uid));

      pose_channel_runtime_data.add(session_uid, pchan->runtime);
      BKE_pose_channel_runtime_reset(&pchan->runtime);
    }
  }
}

void ObjectRuntimeBackup::restore_to_object(Object *object)
{
  ID *data_orig = object->runtime->data_orig;
  ID *data_eval = runtime.data_eval;
  std::optional<Bounds<float3>> bounds = object->runtime->bounds_eval;
  *object->runtime = runtime;
  object->runtime->data_orig = data_orig;
  object->runtime->bounds_eval = bounds;
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
        mesh_eval->runtime->edit_mesh = mesh_orig->runtime->edit_mesh;
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
      object->data = object->runtime->data_eval;
    }
  }

  if (light_linking_runtime) {
    /* Lazily allocate light linking on the evaluated object for the cases when the object is only
     * a receiver or a blocker and does not need its own LightLinking on the original object. */
    BKE_light_linking_ensure(object);
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
    std::optional<ModifierDataBackup> backup = modifier_runtime_data.pop_try(
        modifier_data->persistent_uid);
    if (backup.has_value()) {
      modifier_data->runtime = backup->runtime;
    }
  }

  for (ModifierDataBackup &backup : modifier_runtime_data.values()) {
    const ModifierTypeInfo *modifier_type_info = BKE_modifier_get_info(backup.type);
    BLI_assert(modifier_type_info != nullptr);
    modifier_type_info->free_runtime_data(backup.runtime);

    if (backup.type == eModifierType_Subsurf) {
      if (object->type == OB_MESH) {
        Mesh *mesh = (Mesh *)object->data;
        if (mesh->runtime->subsurf_runtime_data == backup.runtime) {
          mesh->runtime->subsurf_runtime_data = nullptr;
        }
      }
    }
  }
}

void ObjectRuntimeBackup::restore_pose_channel_runtime_data(Object *object)
{
  if (object->pose != nullptr) {
    LISTBASE_FOREACH (bPoseChannel *, pchan, &object->pose->chanbase) {
      const SessionUID &session_uid = pchan->runtime.session_uid;
      std::optional<bPoseChannel_Runtime> runtime = pose_channel_runtime_data.pop_try(session_uid);
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
