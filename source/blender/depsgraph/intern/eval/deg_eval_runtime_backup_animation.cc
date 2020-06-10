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

#include "intern/eval/deg_eval_runtime_backup_animation.h"

#include "DNA_anim_types.h"

#include "BKE_animsys.h"

#include "RNA_access.h"
#include "RNA_types.h"

#include "intern/depsgraph.h"

namespace DEG {

namespace {

struct AnimatedPropertyStoreCalbackData {
  AnimationBackup *backup;

  /* ID which needs to be stored.
   * Is used to check possibly nested IDs which f-curves are pointing to. */
  ID *id;

  PointerRNA id_pointer_rna;
};

void animated_property_store_cb(ID *id, FCurve *fcurve, void *data_v)
{
  AnimatedPropertyStoreCalbackData *data = reinterpret_cast<AnimatedPropertyStoreCalbackData *>(
      data_v);
  if (fcurve->rna_path == nullptr || fcurve->rna_path[0] == '\0') {
    return;
  }
  if (id != data->id) {
    return;
  }

  /* Resolve path to the property. */
  PathResolvedRNA resolved_rna;
  if (!BKE_animsys_store_rna_setting(
          &data->id_pointer_rna, fcurve->rna_path, fcurve->array_index, &resolved_rna)) {
    return;
  }

  /* Read property value. */
  float value;
  if (!BKE_animsys_read_rna_setting(&resolved_rna, &value)) {
    return;
  }

  data->backup->values_backup.append({fcurve->rna_path, fcurve->array_index, value});
}

}  // namespace

AnimationValueBackup::AnimationValueBackup()
{
}

AnimationValueBackup::AnimationValueBackup(const string &rna_path, int array_index, float value)
    : rna_path(rna_path), array_index(array_index), value(value)
{
}

AnimationValueBackup::~AnimationValueBackup()
{
}

AnimationBackup::AnimationBackup(const Depsgraph *depsgraph)
{
  meed_value_backup = !depsgraph->is_active;
  reset();
}

void AnimationBackup::reset()
{
}

void AnimationBackup::init_from_id(ID *id)
{
  /* NOTE: This animation backup nicely preserves values which are animated and
   * are not touched by frame/depsgraph post_update handler.
   *
   * But it makes it impossible to have user edits to animated properties: for
   * example, translation of object with animated location will not work with
   * the current version of backup. */
  return;

  AnimatedPropertyStoreCalbackData data;
  data.backup = this;
  data.id = id;
  RNA_id_pointer_create(id, &data.id_pointer_rna);
  BKE_fcurves_id_cb(id, animated_property_store_cb, &data);
}

void AnimationBackup::restore_to_id(ID *id)
{
  return;

  PointerRNA id_pointer_rna;
  RNA_id_pointer_create(id, &id_pointer_rna);
  for (const AnimationValueBackup &value_backup : values_backup) {
    /* Resolve path to the property.
     *
     * NOTE: Do it again (after storing), since the sub-data pointers might be
     * changed after copy-on-write. */
    PathResolvedRNA resolved_rna;
    if (!BKE_animsys_store_rna_setting(&id_pointer_rna,
                                       value_backup.rna_path.c_str(),
                                       value_backup.array_index,
                                       &resolved_rna)) {
      return;
    }

    /* Write property value. */
    if (!BKE_animsys_write_rna_setting(&resolved_rna, value_backup.value)) {
      return;
    }
  }
}

}  // namespace DEG
