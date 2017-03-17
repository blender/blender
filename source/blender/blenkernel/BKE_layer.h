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
 * Contributor(s): Blender Foundation, Dalai Felinto
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_LAYER_H__
#define __BKE_LAYER_H__

/** \file blender/blenkernel/BKE_layer.h
 *  \ingroup bke
 */

#include "BKE_collection.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TODO_LAYER_SYNC /* syncing of SceneCollection and LayerCollection trees*/
#define TODO_LAYER_SYNC_FILTER /* syncing of filter_objects across all trees */
#define TODO_LAYER_OVERRIDE /* CollectionOverride */
#define TODO_LAYER_CONTEXT /* get/set current (context) SceneLayer */
#define TODO_LAYER_BASE /* BaseLegacy to Base related TODO */
#define TODO_LAYER_OPERATORS /* collection mamanger and property panel operators */
#define TODO_LAYER_DEPSGRAPH /* placeholder for real Depsgraph fix */
#define TODO_LAYER /* generic todo */

#define ROOT_PROP "root"

struct EvaluationContext;
struct LayerCollection;
struct ID;
struct IDProperty;
struct ListBase;
struct Main;
struct Object;
struct Base;
struct RenderEngine;
struct Scene;
struct SceneCollection;
struct SceneLayer;

struct SceneLayer *BKE_scene_layer_render_active(const struct Scene *scene);
struct SceneLayer *BKE_scene_layer_context_active(struct Scene *scene);
struct SceneLayer *BKE_scene_layer_add(struct Scene *scene, const char *name);

bool BKE_scene_layer_remove(struct Main *bmain, struct Scene *scene, struct SceneLayer *sl);

void BKE_scene_layer_free(struct SceneLayer *sl);

void BKE_scene_layer_engine_set(struct SceneLayer *sl, const char *engine);

void BKE_scene_layer_selected_objects_tag(struct SceneLayer *sl, const int tag);

struct SceneLayer *BKE_scene_layer_find_from_collection(const struct Scene *scene, struct LayerCollection *lc);
struct Base *BKE_scene_layer_base_find(struct SceneLayer *sl, struct Object *ob);
void BKE_scene_layer_base_deselect_all(struct SceneLayer *sl);
void BKE_scene_layer_base_select(struct SceneLayer *sl, struct Base *selbase);

void BKE_layer_collection_free(struct SceneLayer *sl, struct LayerCollection *lc);

struct LayerCollection *BKE_layer_collection_active(struct SceneLayer *sl);

int BKE_layer_collection_count(struct SceneLayer *sl);

int BKE_layer_collection_findindex(struct SceneLayer *sl, const struct LayerCollection *lc);

bool BKE_layer_collection_move_above(const struct Scene *scene, struct LayerCollection *lc_dst, struct LayerCollection *lc_src);
bool BKE_layer_collection_move_below(const struct Scene *scene, struct LayerCollection *lc_dst, struct LayerCollection *lc_src);
bool BKE_layer_collection_move_into(const struct Scene *scene, struct LayerCollection *lc_dst, struct LayerCollection *lc_src);

void BKE_layer_collection_resync(const struct Scene *scene, const struct SceneCollection *sc);

struct LayerCollection *BKE_collection_link(struct SceneLayer *sl, struct SceneCollection *sc);

void BKE_collection_unlink(struct SceneLayer *sl, struct LayerCollection *lc);

bool BKE_scene_layer_has_collection(struct SceneLayer *sl, const struct SceneCollection *sc);
bool BKE_scene_has_object(struct Scene *scene, struct Object *ob);

/* syncing */

void BKE_layer_sync_new_scene_collection(struct Scene *scene, const struct SceneCollection *sc_parent, struct SceneCollection *sc);
void BKE_layer_sync_object_link(const struct Scene *scene, struct SceneCollection *sc, struct Object *ob);
void BKE_layer_sync_object_unlink(const struct Scene *scene, struct SceneCollection *sc, struct Object *ob);

/* override */

void BKE_collection_override_datablock_add(struct LayerCollection *lc, const char *data_path, struct ID *id);

/* engine settings */
typedef void (*CollectionEngineSettingsCB)(struct RenderEngine *engine, struct IDProperty *props);
struct IDProperty *BKE_layer_collection_engine_get(struct LayerCollection *lc, const int type, const char *engine_name);
struct IDProperty *BKE_object_collection_engine_get(struct Object *ob, const int type, const char *engine_name);
void BKE_layer_collection_engine_settings_callback_register(struct Main *bmain, const char *engine_name, CollectionEngineSettingsCB func);
void BKE_layer_collection_engine_settings_callback_free(void);
void BKE_layer_collection_engine_settings_create(struct IDProperty *root);

void BKE_collection_engine_property_add_float(struct IDProperty *props, const char *name, float value);
void BKE_collection_engine_property_add_int(struct IDProperty *props, const char *name, int value);
void BKE_collection_engine_property_add_bool(struct IDProperty *props, const char *name, bool value);

int BKE_collection_engine_property_value_get_int(struct IDProperty *props, const char *name);
float BKE_collection_engine_property_value_get_float(struct IDProperty *props, const char *name);
bool BKE_collection_engine_property_value_get_bool(struct IDProperty *props, const char *name);
void BKE_collection_engine_property_value_set_int(struct IDProperty *props, const char *name, int value);
void BKE_collection_engine_property_value_set_float(struct IDProperty *props, const char *name, float value);
void BKE_collection_engine_property_value_set_bool(struct IDProperty *props, const char *name, bool value);

/* evaluation */

void BKE_layer_eval_layer_collection_pre(struct EvaluationContext *eval_ctx,
                                         struct Scene *scene,
                                         struct SceneLayer *scene_layer);
void BKE_layer_eval_layer_collection(struct EvaluationContext *eval_ctx,
                                     struct Scene *scene,
                                     struct LayerCollection *layer_collection,
                                     struct LayerCollection *parent_layer_collection);
void BKE_layer_eval_layer_collection_post(struct EvaluationContext *eval_ctx,
                                          struct SceneLayer *scene_layer);

/* iterators */

void BKE_selected_objects_Iterator_begin(Iterator *iter, void *data_in);
void BKE_selected_objects_Iterator_next(Iterator *iter);
void BKE_selected_objects_Iterator_end(Iterator *iter);

void BKE_visible_objects_Iterator_begin(Iterator *iter, void *data_in);
void BKE_visible_objects_Iterator_next(Iterator *iter);
void BKE_visible_objects_Iterator_end(Iterator *iter);

void BKE_visible_bases_Iterator_begin(Iterator *iter, void *data_in);
void BKE_visible_bases_Iterator_next(Iterator *iter);
void BKE_visible_bases_Iterator_end(Iterator *iter);

#define FOREACH_SELECTED_OBJECT(sl, _instance)                                \
	ITER_BEGIN(BKE_selected_objects_Iterator_begin,                           \
	           BKE_selected_objects_Iterator_next,                            \
	           BKE_selected_objects_Iterator_end,                             \
	           sl, Object *, _instance)

#define FOREACH_SELECTED_OBJECT_END                                           \
	ITER_END

#define FOREACH_VISIBLE_OBJECT(sl, _instance)                                 \
	ITER_BEGIN(BKE_visible_objects_Iterator_begin,                            \
	           BKE_visible_objects_Iterator_next,                             \
	           BKE_visible_objects_Iterator_end,                              \
	           sl, Object *, _instance)

#define FOREACH_VISIBLE_OBJECT_END                                            \
	ITER_END


#define FOREACH_VISIBLE_BASE(sl, _instance)                                   \
	ITER_BEGIN(BKE_visible_bases_Iterator_begin,                              \
	           BKE_visible_bases_Iterator_next,                               \
	           BKE_visible_bases_Iterator_end,                                \
	           sl, Base *, _instance)

#define FOREACH_VISIBLE_BASE_END                                              \
	ITER_END


#define FOREACH_OBJECT(sl, _instance)                                         \
{                                                                             \
	Object *_instance;                                                        \
	Base *base;                                                               \
	for (base = (sl)->object_bases.first; base; base = base->next) {          \
		_instance = base->object;

#define FOREACH_OBJECT_END                                                    \
    }                                                                         \
}

#define FOREACH_OBJECT_FLAG(scene, sl, flag, _instance)                       \
{                                                                             \
	IteratorBeginCb func_begin;                                               \
	IteratorCb func_next, func_end;                                           \
	void *data_in;                                                            \
	                                                                          \
	if (flag == SELECT) {                                                     \
	    func_begin = &BKE_selected_objects_Iterator_begin;                    \
	    func_next = &BKE_selected_objects_Iterator_next;                      \
	    func_end = &BKE_selected_objects_Iterator_end;                        \
	    data_in = (sl);                                                       \
	}                                                                         \
	else {                                                                    \
	    func_begin = BKE_scene_objects_Iterator_begin;                        \
	    func_next = BKE_scene_objects_Iterator_next;                          \
	    func_end = BKE_scene_objects_Iterator_end;                            \
	    data_in = (scene);                                                    \
	}                                                                         \
	ITER_BEGIN(func_begin, func_next, func_end, data_in, Object *, _instance)


#define FOREACH_OBJECT_FLAG_END                                               \
	ITER_END                                                                  \
}

/* temporary hacky solution waiting for CoW depsgraph implementation */
#define DEG_OBJECT_ITER(sl_, instance_)                                       \
{                                                                             \
	/* flush all the depsgraph data to objects */                             \
	Object *instance_;                                                        \
	Base *base_;                                                              \
	for (base_ = (sl_)->object_bases.first; base_; base_ = base_->next) {     \
	    if ((base_->flag & BASE_VISIBLED) != 0) {                             \
	        instance_ = base_->object;                                        \
	        instance_->base_flag = base_->flag;                               \
	        instance_->base_collection_properties = base_->collection_properties;

#define DEG_OBJECT_ITER_END                                                   \
        }                                                                     \
    }                                                                         \
}

#ifdef __cplusplus
}
#endif

#endif /* __BKE_LAYER_H__ */
