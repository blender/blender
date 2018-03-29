/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * Contributor(s): Dalai Felinto
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_COLLECTION_H__
#define __BKE_COLLECTION_H__

/** \file blender/blenkernel/BKE_collection.h
 *  \ingroup bke
 */

#include "BLI_ghash.h"
#include "BLI_iterator.h"
#include "DNA_listBase.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Base;
struct BLI_Iterator;
struct Group;
struct ID;
struct LayerCollection;
struct Main;
struct Object;
struct Scene;
struct SceneCollection;
struct ViewLayer;

struct SceneCollection *BKE_collection_add(
        struct ID *owner_id, struct SceneCollection *sc_parent, const int type, const char *name);
bool BKE_collection_remove(struct ID *owner_id, struct SceneCollection *sc);
void BKE_collection_copy_data(struct SceneCollection *sc_dst, struct SceneCollection *sc_src, const int flag);
struct SceneCollection *BKE_collection_duplicate(struct ID *owner_id, struct SceneCollection *scene_collection);
struct SceneCollection *BKE_collection_master(const struct ID *owner_id);
void BKE_collection_rename(const struct ID *owner_id, struct SceneCollection *sc, const char *name);
void BKE_collection_master_free(struct ID *owner_id, const bool do_id_user);
bool BKE_collection_object_add(const struct ID *owner_id, struct SceneCollection *sc, struct Object *object);
void BKE_collection_object_add_from(struct Scene *scene, struct Object *ob_src, struct Object *ob_dst);
bool BKE_collection_object_remove(struct Main *bmain, struct ID *owner_id, struct SceneCollection *sc, struct Object *object, const bool free_us);
bool BKE_collections_object_remove(struct Main *bmain, struct ID *owner_id, struct Object *object, const bool free_us);
void BKE_collection_object_move(struct ID *owner_id, struct SceneCollection *sc_dst, struct SceneCollection *sc_src, struct Object *ob);
bool BKE_collection_object_exists(struct SceneCollection *scene_collection, struct Object *ob);
struct SceneCollection *BKE_collection_from_index(struct Scene *scene, const int index);

bool BKE_collection_objects_select(struct ViewLayer *view_layer, struct SceneCollection *scene_collection);

struct Group *BKE_collection_group_create(struct Main *bmain, struct Scene *scene, struct LayerCollection *lc);

void BKE_collection_reinsert_after(const struct Scene *scene, struct SceneCollection *sc_reinsert, struct SceneCollection *sc_after);
void BKE_collection_reinsert_into(struct SceneCollection *sc_reinsert, struct SceneCollection *sc_into);

bool BKE_collection_move_above(const struct ID *owner_id, struct SceneCollection *sc_dst, struct SceneCollection *sc_src);
bool BKE_collection_move_below(const struct ID *owner_id, struct SceneCollection *sc_dst, struct SceneCollection *sc_src);
bool BKE_collection_move_into(const struct ID *owner_id, struct SceneCollection *sc_dst, struct SceneCollection *sc_src);

typedef void (*BKE_scene_objects_Cb)(struct Object *ob, void *data);
typedef void (*BKE_scene_collections_Cb)(struct SceneCollection *ob, void *data);

void BKE_scene_collections_callback(struct Scene *scene, BKE_scene_collections_Cb callback, void *data);
void BKE_scene_objects_callback(struct Scene *scene, BKE_scene_objects_Cb callback, void *data);

/* iterators */
void BKE_scene_collections_iterator_begin(struct BLI_Iterator *iter, void *data_in);
void BKE_scene_collections_iterator_next(struct BLI_Iterator *iter);
void BKE_scene_collections_iterator_end(struct BLI_Iterator *iter);

void BKE_scene_objects_iterator_begin(struct BLI_Iterator *iter, void *data_in);
void BKE_scene_objects_iterator_next(struct BLI_Iterator *iter);
void BKE_scene_objects_iterator_end(struct BLI_Iterator *iter);

#define FOREACH_SCENE_COLLECTION_BEGIN(_id, _instance)                        \
	ITER_BEGIN(BKE_scene_collections_iterator_begin,                          \
	           BKE_scene_collections_iterator_next,                           \
	           BKE_scene_collections_iterator_end,                            \
	           _id, SceneCollection *, _instance)

#define FOREACH_SCENE_COLLECTION_END                                          \
	ITER_END

#define FOREACH_SCENE_OBJECT_BEGIN(scene, _instance)                          \
	ITER_BEGIN(BKE_scene_objects_iterator_begin,                              \
	           BKE_scene_objects_iterator_next,                               \
	           BKE_scene_objects_iterator_end,                                \
	           scene, Object *, _instance)

#define FOREACH_SCENE_OBJECT_END                                              \
	ITER_END

#ifdef __cplusplus
}
#endif

#endif /* __BKE_COLLECTION_H__ */
