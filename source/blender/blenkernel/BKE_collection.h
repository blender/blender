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
 */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_compiler_compat.h"
#include "BLI_ghash.h"
#include "BLI_iterator.h"
#include "BLI_sys_types.h"

#include "DNA_listBase.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Structs */

struct BLI_Iterator;
struct Base;
struct BlendDataReader;
struct BlendExpander;
struct BlendLibReader;
struct BlendWriter;
struct Collection;
struct Library;
struct Main;
struct Object;
struct Scene;
struct SceneCollection;
struct ViewLayer;

typedef struct CollectionParent {
  struct CollectionParent *next, *prev;
  struct Collection *collection;
} CollectionParent;

/* Collections */

struct Collection *BKE_collection_add(struct Main *bmain,
                                      struct Collection *parent,
                                      const char *name);
void BKE_collection_add_from_object(struct Main *bmain,
                                    struct Scene *scene,
                                    const struct Object *ob_src,
                                    struct Collection *collection_dst);
void BKE_collection_add_from_collection(struct Main *bmain,
                                        struct Scene *scene,
                                        struct Collection *collection_src,
                                        struct Collection *collection_dst);
void BKE_collection_free(struct Collection *collection);
bool BKE_collection_delete(struct Main *bmain, struct Collection *collection, bool hierarchy);

struct Collection *BKE_collection_duplicate(struct Main *bmain,
                                            struct Collection *parent,
                                            struct Collection *collection,
                                            const uint duplicate_flags,
                                            const uint duplicate_options);

/* Master Collection for Scene */

struct Collection *BKE_collection_master_add(void);

/* Collection Objects */

bool BKE_collection_has_object(struct Collection *collection, const struct Object *ob);
bool BKE_collection_has_object_recursive(struct Collection *collection, struct Object *ob);
bool BKE_collection_has_object_recursive_instanced(struct Collection *collection,
                                                   struct Object *ob);
struct Collection *BKE_collection_object_find(struct Main *bmain,
                                              struct Scene *scene,
                                              struct Collection *collection,
                                              struct Object *ob);
bool BKE_collection_is_empty(struct Collection *collection);

bool BKE_collection_object_add(struct Main *bmain,
                               struct Collection *collection,
                               struct Object *ob);
void BKE_collection_object_add_from(struct Main *bmain,
                                    struct Scene *scene,
                                    struct Object *ob_src,
                                    struct Object *ob_dst);
bool BKE_collection_object_remove(struct Main *bmain,
                                  struct Collection *collection,
                                  struct Object *object,
                                  const bool free_us);
void BKE_collection_object_move(struct Main *bmain,
                                struct Scene *scene,
                                struct Collection *collection_dst,
                                struct Collection *collection_src,
                                struct Object *ob);

bool BKE_scene_collections_object_remove(struct Main *bmain,
                                         struct Scene *scene,
                                         struct Object *object,
                                         const bool free_us);
void BKE_collections_object_remove_nulls(struct Main *bmain);
void BKE_collections_child_remove_nulls(struct Main *bmain, struct Collection *old_collection);

/* Dependencies. */

bool BKE_collection_is_in_scene(struct Collection *collection);
void BKE_collections_after_lib_link(struct Main *bmain);
bool BKE_collection_object_cyclic_check(struct Main *bmain,
                                        struct Object *object,
                                        struct Collection *collection);

/* Object list cache. */

struct ListBase BKE_collection_object_cache_get(struct Collection *collection);
ListBase BKE_collection_object_cache_instanced_get(struct Collection *collection);
void BKE_collection_object_cache_free(struct Collection *collection);

struct Base *BKE_collection_or_layer_objects(const struct ViewLayer *view_layer,
                                             struct Collection *collection);

/* Editing. */

struct Collection *BKE_collection_from_index(struct Scene *scene, const int index);
void BKE_collection_new_name_get(struct Collection *collection_parent, char *rname);
const char *BKE_collection_ui_name_get(struct Collection *collection);
bool BKE_collection_objects_select(struct ViewLayer *view_layer,
                                   struct Collection *collection,
                                   bool deselect);

/* Collection children */

bool BKE_collection_child_add(struct Main *bmain,
                              struct Collection *parent,
                              struct Collection *child);

bool BKE_collection_child_add_no_sync(struct Collection *parent, struct Collection *child);

bool BKE_collection_child_remove(struct Main *bmain,
                                 struct Collection *parent,
                                 struct Collection *child);

bool BKE_collection_move(struct Main *bmain,
                         struct Collection *to_parent,
                         struct Collection *from_parent,
                         struct Collection *relative,
                         bool relative_after,
                         struct Collection *collection);

bool BKE_collection_cycle_find(struct Collection *new_ancestor, struct Collection *collection);
bool BKE_collection_cycles_fix(struct Main *bmain, struct Collection *collection);

bool BKE_collection_has_collection(struct Collection *parent, struct Collection *collection);

void BKE_collection_parent_relations_rebuild(struct Collection *collection);
void BKE_main_collections_parent_relations_rebuild(struct Main *bmain);

/* .blend file I/O */

void BKE_collection_blend_write_nolib(struct BlendWriter *writer, struct Collection *collection);
void BKE_collection_blend_read_data(struct BlendDataReader *reader, struct Collection *collection);
void BKE_collection_blend_read_lib(struct BlendLibReader *reader, struct Collection *collection);
void BKE_collection_blend_read_expand(struct BlendExpander *expander,
                                      struct Collection *collection);

void BKE_collection_compat_blend_read_data(struct BlendDataReader *reader,
                                           struct SceneCollection *sc);
void BKE_collection_compat_blend_read_lib(struct BlendLibReader *reader,
                                          struct Library *lib,
                                          struct SceneCollection *sc);
void BKE_collection_compat_blend_read_expand(struct BlendExpander *expander,
                                             struct SceneCollection *sc);

/* Iteration callbacks. */

typedef void (*BKE_scene_objects_Cb)(struct Object *ob, void *data);
typedef void (*BKE_scene_collections_Cb)(struct Collection *ob, void *data);

/* Iteration over objects in collection. */

#define FOREACH_COLLECTION_VISIBLE_OBJECT_RECURSIVE_BEGIN(_collection, _object, _mode) \
  { \
    int _base_flag = (_mode == DAG_EVAL_VIEWPORT) ? BASE_ENABLED_VIEWPORT : BASE_ENABLED_RENDER; \
    int _object_restrict_flag = (_mode == DAG_EVAL_VIEWPORT) ? OB_RESTRICT_VIEWPORT : \
                                                               OB_RESTRICT_RENDER; \
    int _base_id = 0; \
    for (Base *_base = (Base *)BKE_collection_object_cache_get(_collection).first; _base; \
         _base = _base->next, _base_id++) { \
      Object *_object = _base->object; \
      if ((_base->flag & _base_flag) && (_object->restrictflag & _object_restrict_flag) == 0) {

#define FOREACH_COLLECTION_VISIBLE_OBJECT_RECURSIVE_END \
  } \
  } \
  } \
  ((void)0)

#define FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN(_collection, _object) \
  for (Base *_base = (Base *)BKE_collection_object_cache_get(_collection).first; _base; \
       _base = _base->next) { \
    Object *_object = _base->object; \
    BLI_assert(_object != NULL);

#define FOREACH_COLLECTION_OBJECT_RECURSIVE_END \
  } \
  ((void)0)

/* Iteration over collections in scene. */

void BKE_scene_collections_iterator_begin(struct BLI_Iterator *iter, void *data_in);
void BKE_scene_collections_iterator_next(struct BLI_Iterator *iter);
void BKE_scene_collections_iterator_end(struct BLI_Iterator *iter);

void BKE_scene_objects_iterator_begin(struct BLI_Iterator *iter, void *data_in);
void BKE_scene_objects_iterator_next(struct BLI_Iterator *iter);
void BKE_scene_objects_iterator_end(struct BLI_Iterator *iter);

#define FOREACH_SCENE_COLLECTION_BEGIN(scene, _instance) \
  ITER_BEGIN (BKE_scene_collections_iterator_begin, \
              BKE_scene_collections_iterator_next, \
              BKE_scene_collections_iterator_end, \
              scene, \
              Collection *, \
              _instance)

#define FOREACH_SCENE_COLLECTION_END ITER_END

#define FOREACH_COLLECTION_BEGIN(_bmain, _scene, Type, _instance) \
  { \
    Type _instance; \
    Collection *_instance_next; \
    bool is_scene_collection = (_scene) != NULL; \
\
    if (_scene) { \
      _instance_next = _scene->master_collection; \
    } \
    else { \
      _instance_next = (_bmain)->collections.first; \
    } \
\
    while ((_instance = _instance_next)) { \
      if (is_scene_collection) { \
        _instance_next = (_bmain)->collections.first; \
        is_scene_collection = false; \
      } \
      else { \
        _instance_next = _instance->id.next; \
      }

#define FOREACH_COLLECTION_END \
  } \
  } \
  ((void)0)

#define FOREACH_SCENE_OBJECT_BEGIN(scene, _instance) \
  ITER_BEGIN (BKE_scene_objects_iterator_begin, \
              BKE_scene_objects_iterator_next, \
              BKE_scene_objects_iterator_end, \
              scene, \
              Object *, \
              _instance)

#define FOREACH_SCENE_OBJECT_END ITER_END

#ifdef __cplusplus
}
#endif
