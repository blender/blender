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

#pragma once

#include "intern/depsgraph_type.h"

#include "RNA_access.h"

struct ID;
struct PointerRNA;
struct PropertyRNA;

namespace DEG {

class DepsgraphBuilderCache;

/* Identifier for animated property. */
class AnimatedPropertyID {
 public:
  AnimatedPropertyID();
  AnimatedPropertyID(const PointerRNA *pointer_rna, const PropertyRNA *property_rna);
  AnimatedPropertyID(const PointerRNA &pointer_rna, const PropertyRNA *property_rna);
  AnimatedPropertyID(ID *id, StructRNA *type, const char *property_name);
  AnimatedPropertyID(ID *id, StructRNA *type, void *data, const char *property_name);

  uint32_t hash() const;

  bool operator<(const AnimatedPropertyID &other) const;
  friend bool operator==(const AnimatedPropertyID &a, const AnimatedPropertyID &b);

  /* Corresponds to PointerRNA.data. */
  void *data;
  const PropertyRNA *property_rna;
};

class AnimatedPropertyStorage {
 public:
  AnimatedPropertyStorage();

  void initializeFromID(DepsgraphBuilderCache *builder_cache, ID *id);

  void tagPropertyAsAnimated(const AnimatedPropertyID &property_id);
  void tagPropertyAsAnimated(const PointerRNA *pointer_rna, const PropertyRNA *property_rna);

  bool isPropertyAnimated(const AnimatedPropertyID &property_id);
  bool isPropertyAnimated(const PointerRNA *pointer_rna, const PropertyRNA *property_rna);

  /* The storage is fully initialized from all F-Curves from corresponding ID. */
  bool is_fully_initialized;

  /* indexed by PointerRNA.data. */
  Set<AnimatedPropertyID> animated_properties_set;
};

typedef map<ID *, AnimatedPropertyStorage *> AnimatedPropertyStorageMap;

/* Cached data which can be re-used by multiple builders. */
class DepsgraphBuilderCache {
 public:
  DepsgraphBuilderCache();
  ~DepsgraphBuilderCache();

  /* Makes sure storage for animated properties exists and initialized for the given ID. */
  AnimatedPropertyStorage *ensureAnimatedPropertyStorage(ID *id);
  AnimatedPropertyStorage *ensureInitializedAnimatedPropertyStorage(ID *id);

  /* Shortcuts to go through ensureInitializedAnimatedPropertyStorage and its
   * isPropertyAnimated.
   *
   * NOTE: Avoid using for multiple subsequent lookups, query for the storage once, and then query
   * the storage.
   *
   * TODO(sergey): Technically, this makes this class something else than just a cache, but what is
   * the better name? */
  template<typename... Args> bool isPropertyAnimated(ID *id, Args... args)
  {
    AnimatedPropertyStorage *animated_property_storage = ensureInitializedAnimatedPropertyStorage(
        id);
    return animated_property_storage->isPropertyAnimated(args...);
  }

  AnimatedPropertyStorageMap animated_property_storage_map_;
};

}  // namespace DEG
