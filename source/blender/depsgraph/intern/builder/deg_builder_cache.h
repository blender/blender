/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2018 Blender Foundation */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "intern/depsgraph_type.h"

#include "RNA_access.h"

struct ID;
struct PointerRNA;
struct PropertyRNA;

namespace blender::deg {

class DepsgraphBuilderCache;

/* Identifier for animated property. */
class AnimatedPropertyID {
 public:
  AnimatedPropertyID();
  AnimatedPropertyID(const PointerRNA *pointer_rna, const PropertyRNA *property_rna);
  AnimatedPropertyID(const PointerRNA &pointer_rna, const PropertyRNA *property_rna);
  AnimatedPropertyID(const ID *id, StructRNA *type, const char *property_name);
  AnimatedPropertyID(const ID *id, StructRNA *type, void *data, const char *property_name);

  uint64_t hash() const;
  friend bool operator==(const AnimatedPropertyID &a, const AnimatedPropertyID &b);

  /* Corresponds to PointerRNA.data. */
  const void *data;
  const PropertyRNA *property_rna;

  MEM_CXX_CLASS_ALLOC_FUNCS("AnimatedPropertyID");
};

class AnimatedPropertyStorage {
 public:
  AnimatedPropertyStorage();

  void initializeFromID(DepsgraphBuilderCache *builder_cache, const ID *id);

  void tagPropertyAsAnimated(const AnimatedPropertyID &property_id);
  void tagPropertyAsAnimated(const PointerRNA *pointer_rna, const PropertyRNA *property_rna);

  bool isPropertyAnimated(const AnimatedPropertyID &property_id);
  bool isPropertyAnimated(const PointerRNA *pointer_rna, const PropertyRNA *property_rna);

  bool isAnyPropertyAnimated(const PointerRNA *pointer_rna);

  /* The storage is fully initialized from all F-Curves from corresponding ID. */
  bool is_fully_initialized;

  /* indexed by PointerRNA.data. */
  Set<const void *> animated_objects_set;
  Set<AnimatedPropertyID> animated_properties_set;

  MEM_CXX_CLASS_ALLOC_FUNCS("AnimatedPropertyStorage");
};

/* Cached data which can be re-used by multiple builders. */
class DepsgraphBuilderCache {
 public:
  ~DepsgraphBuilderCache();

  /* Makes sure storage for animated properties exists and initialized for the given ID. */
  AnimatedPropertyStorage *ensureAnimatedPropertyStorage(const ID *id);
  AnimatedPropertyStorage *ensureInitializedAnimatedPropertyStorage(const ID *id);

  /* Shortcuts to go through ensureInitializedAnimatedPropertyStorage and its
   * isPropertyAnimated.
   *
   * NOTE: Avoid using for multiple subsequent lookups, query for the storage once, and then query
   * the storage.
   *
   * TODO(sergey): Technically, this makes this class something else than just a cache, but what is
   * the better name? */
  template<typename... Args> bool isPropertyAnimated(const ID *id, Args... args)
  {
    AnimatedPropertyStorage *animated_property_storage = ensureInitializedAnimatedPropertyStorage(
        id);
    return animated_property_storage->isPropertyAnimated(args...);
  }

  bool isAnyPropertyAnimated(const PointerRNA *ptr)
  {
    AnimatedPropertyStorage *animated_property_storage = ensureInitializedAnimatedPropertyStorage(
        ptr->owner_id);
    return animated_property_storage->isAnyPropertyAnimated(ptr);
  }

  Map<const ID *, AnimatedPropertyStorage *> animated_property_storage_map_;

  MEM_CXX_CLASS_ALLOC_FUNCS("DepsgraphBuilderCache");
};

}  // namespace blender::deg
