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

#pragma once

#include "DNA_object_types.h"

#include "intern/eval/deg_eval_runtime_backup_modifier.h"
#include "intern/eval/deg_eval_runtime_backup_pose.h"

struct Object;

namespace DEG {

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
  ModifierRuntimeDataBackup modifier_runtime_data;
  Map<bPoseChannel *, bPoseChannel_Runtime> pose_channel_runtime_data;
};

}  // namespace DEG
