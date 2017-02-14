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

struct Iterator;
struct SceneCollection;
struct Object;
struct Base;
struct Main;
struct Scene;

struct SceneCollection *BKE_collection_add(struct Scene *scene, struct SceneCollection *sc_parent, const char *name);
bool BKE_collection_remove(struct Scene *scene, struct SceneCollection *sc);
struct SceneCollection *BKE_collection_master(struct Scene *scene);
void BKE_collection_master_free(struct Scene *scene);
void BKE_collection_object_add(struct Scene *scene, struct SceneCollection *sc, struct Object *object);
void BKE_collection_object_add_from(struct Scene *scene, struct Object *ob_src, struct Object *ob_dst);
void BKE_collection_object_remove(struct Main *bmain, struct Scene *scene, struct SceneCollection *sc, struct Object *object, const bool free_us);
void BKE_collections_object_remove(struct Main *bmain, struct Scene *scene, struct Object *object, const bool free_us);

typedef void (*BKE_scene_objects_Cb)(struct Object *ob, void *data);
typedef void (*BKE_scene_collections_Cb)(struct SceneCollection *ob, void *data);

void BKE_scene_collections_callback(struct Scene *scene, BKE_scene_collections_Cb callback, void *data);
void BKE_scene_objects_callback(struct Scene *scene, BKE_scene_objects_Cb callback, void *data);

/* iterators */
void BKE_scene_collections_Iterator_begin(struct Iterator *iter, void *data_in);
void BKE_scene_collections_Iterator_next(struct Iterator *iter);
void BKE_scene_collections_Iterator_end(struct Iterator *iter);

void BKE_scene_objects_Iterator_begin(struct Iterator *iter, void *data_in);
void BKE_scene_objects_Iterator_next(struct Iterator *iter);
void BKE_scene_objects_Iterator_end(struct Iterator *iter);

#define FOREACH_SCENE_COLLECTION(scene, _instance)                            \
	ITER_BEGIN(BKE_scene_collections_Iterator_begin,                          \
	           BKE_scene_collections_Iterator_next,                           \
	           BKE_scene_collections_Iterator_end,                            \
	           scene, SceneCollection *, _instance)

#define FOREACH_SCENE_COLLECTION_END                                          \
	ITER_END

#define FOREACH_SCENE_OBJECT(scene, _instance)                                \
	ITER_BEGIN(BKE_scene_objects_Iterator_begin,                              \
	           BKE_scene_objects_Iterator_next,                               \
	           BKE_scene_objects_Iterator_end,                                \
	           scene, Object *, _instance)

#define FOREACH_SCENE_OBJECT_END                                              \
	ITER_END

#ifdef __cplusplus
}
#endif

#endif /* __BKE_COLLECTION_H__ */
