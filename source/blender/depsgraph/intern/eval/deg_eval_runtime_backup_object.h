/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "DNA_object_types.h"
#include "DNA_session_uuid_types.h"

#include "BLI_session_uuid.h"

#include "intern/eval/deg_eval_runtime_backup_modifier.h"
#include "intern/eval/deg_eval_runtime_backup_pose.h"

struct Object;

namespace blender::deg {

struct Depsgraph;

class ObjectRuntimeBackup {
 public:
  ObjectRuntimeBackup(const Depsgraph *depsgraph);

  /* Make a backup of object's evaluation runtime data, additionally
   * make object to be safe for free without invalidating backed up
   * pointers. */
  void init_from_object(Object *object);
  void backup_modifier_runtime_data(Object *object);
  void backup_pose_channel_runtime_data(Object *object);

  /* Restore all fields to the given object. */
  void restore_to_object(Object *object);
  /* NOTE: Will free all runtime data which has not been restored. */
  void restore_modifier_runtime_data(Object *object);
  void restore_pose_channel_runtime_data(Object *object);

  Object_Runtime runtime;
  short base_flag;
  unsigned short base_local_view_bits;
  Map<SessionUUID, ModifierDataBackup> modifier_runtime_data;
  Map<SessionUUID, bPoseChannel_Runtime> pose_channel_runtime_data;
};

}  // namespace blender::deg
