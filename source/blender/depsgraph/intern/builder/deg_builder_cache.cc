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
 * The Original Code is Copyright (C) 2018 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 */

#include "intern/builder/deg_builder_cache.h"

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"

#include "BLI_utildefines.h"

extern "C" {
#include "BKE_animsys.h"
}

namespace DEG {

/* Animated property storage. */

AnimatedPropertyID::AnimatedPropertyID() : data(NULL), property_rna(NULL)
{
}

AnimatedPropertyID::AnimatedPropertyID(const PointerRNA *pointer_rna,
                                       const PropertyRNA *property_rna)
    : AnimatedPropertyID(*pointer_rna, property_rna)
{
}

AnimatedPropertyID::AnimatedPropertyID(const PointerRNA &pointer_rna,
                                       const PropertyRNA *property_rna)
    : data(pointer_rna.data), property_rna(property_rna)
{
}

AnimatedPropertyID::AnimatedPropertyID(ID *id, StructRNA *type, const char *property_name)
    : data(id)
{
  property_rna = RNA_struct_type_find_property(type, property_name);
}

AnimatedPropertyID::AnimatedPropertyID(ID * /*id*/,
                                       StructRNA *type,
                                       void *data,
                                       const char *property_name)
    : data(data)
{
  property_rna = RNA_struct_type_find_property(type, property_name);
}

bool AnimatedPropertyID::operator<(const AnimatedPropertyID &other) const
{
  if (data < other.data) {
    return true;
  }
  else if (data == other.data) {
    return property_rna < other.property_rna;
  }
  return false;
}

namespace {

struct AnimatedPropertyCallbackData {
  PointerRNA pointer_rna;
  AnimatedPropertyStorage *animated_property_storage;
  DepsgraphBuilderCache *builder_cache;
};

void animated_property_cb(ID * /*id*/, FCurve *fcurve, void *data_v)
{
  if (fcurve->rna_path == NULL || fcurve->rna_path[0] == '\0') {
    return;
  }
  AnimatedPropertyCallbackData *data = static_cast<AnimatedPropertyCallbackData *>(data_v);
  /* Resolve property. */
  PointerRNA pointer_rna;
  PropertyRNA *property_rna = NULL;
  if (!RNA_path_resolve_property(
          &data->pointer_rna, fcurve->rna_path, &pointer_rna, &property_rna)) {
    return;
  }
  /* Get storage for the ID.
   * This is needed to deal with cases when nested datablock is animated by its parent. */
  AnimatedPropertyStorage *animated_property_storage = data->animated_property_storage;
  if (pointer_rna.id.data != data->pointer_rna.id.data) {
    animated_property_storage = data->builder_cache->ensureAnimatedPropertyStorage(
        reinterpret_cast<ID *>(pointer_rna.id.data));
  }
  /* Set the property as animated. */
  animated_property_storage->tagPropertyAsAnimated(&pointer_rna, property_rna);
}

}  // namespace

AnimatedPropertyStorage::AnimatedPropertyStorage() : is_fully_initialized(false)
{
}

void AnimatedPropertyStorage::initializeFromID(DepsgraphBuilderCache *builder_cache, ID *id)
{
  AnimatedPropertyCallbackData data;
  RNA_id_pointer_create(id, &data.pointer_rna);
  data.animated_property_storage = this;
  data.builder_cache = builder_cache;
  BKE_fcurves_id_cb(id, animated_property_cb, &data);
}

void AnimatedPropertyStorage::tagPropertyAsAnimated(const AnimatedPropertyID &property_id)
{
  animated_properties_set.insert(property_id);
}

void AnimatedPropertyStorage::tagPropertyAsAnimated(const PointerRNA *pointer_rna,
                                                    const PropertyRNA *property_rna)
{
  tagPropertyAsAnimated(AnimatedPropertyID(pointer_rna, property_rna));
}

bool AnimatedPropertyStorage::isPropertyAnimated(const AnimatedPropertyID &property_id)
{
  return animated_properties_set.find(property_id) != animated_properties_set.end();
}

bool AnimatedPropertyStorage::isPropertyAnimated(const PointerRNA *pointer_rna,
                                                 const PropertyRNA *property_rna)
{
  return isPropertyAnimated(AnimatedPropertyID(pointer_rna, property_rna));
}

/* Builder cache itself. */

DepsgraphBuilderCache::DepsgraphBuilderCache()
{
}

DepsgraphBuilderCache::~DepsgraphBuilderCache()
{
  for (AnimatedPropertyStorageMap::value_type &iter : animated_property_storage_map_) {
    AnimatedPropertyStorage *animated_property_storage = iter.second;
    OBJECT_GUARDED_DELETE(animated_property_storage, AnimatedPropertyStorage);
  }
}

AnimatedPropertyStorage *DepsgraphBuilderCache::ensureAnimatedPropertyStorage(ID *id)
{
  AnimatedPropertyStorageMap::iterator it = animated_property_storage_map_.find(id);
  if (it != animated_property_storage_map_.end()) {
    return it->second;
  }
  AnimatedPropertyStorage *animated_property_storage = OBJECT_GUARDED_NEW(AnimatedPropertyStorage);
  animated_property_storage_map_.insert(make_pair(id, animated_property_storage));
  return animated_property_storage;
}

AnimatedPropertyStorage *DepsgraphBuilderCache::ensureInitializedAnimatedPropertyStorage(ID *id)
{
  AnimatedPropertyStorage *animated_property_storage = ensureAnimatedPropertyStorage(id);
  if (!animated_property_storage->is_fully_initialized) {
    animated_property_storage->initializeFromID(this, id);
    animated_property_storage->is_fully_initialized = true;
  }
  return animated_property_storage;
}

}  // namespace DEG
