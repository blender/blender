/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BKE_collection.h"

#include "DNA_layer_types.h"
#include "DNA_listBase.h"
#include "DNA_object_enums.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TODO_LAYER_OVERRIDE  /* CollectionOverride */
#define TODO_LAYER_OPERATORS /* collection mamanger and property panel operators */
#define TODO_LAYER           /* generic todo */

struct Base;
struct BlendDataReader;
struct BlendLibReader;
struct BlendWriter;
struct Collection;
struct Depsgraph;
struct LayerCollection;
struct Main;
struct Object;
struct RenderEngine;
struct Scene;
struct View3D;
struct ViewLayer;

typedef enum eViewLayerCopyMethod {
  VIEWLAYER_ADD_NEW = 0,
  VIEWLAYER_ADD_EMPTY = 1,
  VIEWLAYER_ADD_COPY = 2,
} eViewLayerCopyMethod;

/**
 * Returns the default view layer to view in work-spaces if there is
 * none linked to the workspace yet.
 */
struct ViewLayer *BKE_view_layer_default_view(const struct Scene *scene);
/**
 * Returns the default view layer to render if we need to render just one.
 */
struct ViewLayer *BKE_view_layer_default_render(const struct Scene *scene);
/**
 * Returns view layer with matching name, or NULL if not found.
 */
struct ViewLayer *BKE_view_layer_find(const struct Scene *scene, const char *layer_name);
/**
 * Add a new view layer by default, a view layer has the master collection.
 */
struct ViewLayer *BKE_view_layer_add(struct Scene *scene,
                                     const char *name,
                                     struct ViewLayer *view_layer_source,
                                     int type);

/* DEPRECATED */
/**
 * This is a placeholder to know which areas of the code need to be addressed
 * for the Workspace changes. Never use this, you should typically get the
 * active layer from the context or window.
 */
struct ViewLayer *BKE_view_layer_context_active_PLACEHOLDER(const struct Scene *scene);

void BKE_view_layer_free(struct ViewLayer *view_layer);
/**
 * Free (or release) any data used by this #ViewLayer.
 */
void BKE_view_layer_free_ex(struct ViewLayer *view_layer, bool do_id_user);

/**
 * Free the bases of this #ViewLayer, and what they reference.
 * This includes baseact, object_bases, object_bases_hash, and layer_collections.
 */
void BKE_view_layer_free_object_content(struct ViewLayer *view_layer);

/**
 * Tag all the selected objects of a render-layer.
 */
void BKE_view_layer_selected_objects_tag(const struct Scene *scene,
                                         struct ViewLayer *view_layer,
                                         int tag);

/**
 * Fallback for when a Scene has no camera to use.
 *
 * \param view_layer: in general you want to use the same #ViewLayer that is used for depsgraph.
 * If rendering you pass the scene active layer, when viewing in the viewport
 * you want to get #ViewLayer from context.
 */
struct Object *BKE_view_layer_camera_find(const struct Scene *scene, struct ViewLayer *view_layer);
/**
 * Find the #ViewLayer a #LayerCollection belongs to.
 */
struct ViewLayer *BKE_view_layer_find_from_collection(const struct Scene *scene,
                                                      struct LayerCollection *lc);
struct Base *BKE_view_layer_base_find(struct ViewLayer *view_layer, struct Object *ob);
void BKE_view_layer_base_deselect_all(const struct Scene *scene, struct ViewLayer *view_layer);

void BKE_view_layer_base_select_and_set_active(struct ViewLayer *view_layer, struct Base *selbase);

/**
 * Only copy internal data of #ViewLayer from source to already allocated/initialized destination.
 *
 * \param flag: Copying options (see BKE_lib_id.h's LIB_ID_COPY_... flags for more).
 */
void BKE_view_layer_copy_data(struct Scene *scene_dst,
                              const struct Scene *scene_src,
                              struct ViewLayer *view_layer_dst,
                              const struct ViewLayer *view_layer_src,
                              int flag);

void BKE_view_layer_rename(struct Main *bmain,
                           struct Scene *scene,
                           struct ViewLayer *view_layer,
                           const char *name);

/**
 * Get the active collection
 */
struct LayerCollection *BKE_layer_collection_get_active(struct ViewLayer *view_layer);
/**
 * Activate collection
 */
bool BKE_layer_collection_activate(struct ViewLayer *view_layer, struct LayerCollection *lc);
/**
 * Activate first parent collection.
 */
struct LayerCollection *BKE_layer_collection_activate_parent(struct ViewLayer *view_layer,
                                                             struct LayerCollection *lc);

/**
 * Get the total number of collections (including all the nested collections)
 */
int BKE_layer_collection_count(const struct ViewLayer *view_layer);

/**
 * Get the collection for a given index.
 */
struct LayerCollection *BKE_layer_collection_from_index(struct ViewLayer *view_layer, int index);
/**
 * \return -1 if not found.
 */
int BKE_layer_collection_findindex(struct ViewLayer *view_layer, const struct LayerCollection *lc);

void BKE_layer_collection_resync_forbid(void);
void BKE_layer_collection_resync_allow(void);

/**
 * Helper to fix older pre-2.80 blend-files.
 *
 * Ensures the given `view_layer` as a valid first-level layer collection, i.e. a single one
 * matching the scene's master collection. This is a requirement for `BKE_layer_collection_sync`.
 */
void BKE_layer_collection_doversion_2_80(const struct Scene *scene, struct ViewLayer *view_layer);

void BKE_main_collection_sync(const struct Main *bmain);
void BKE_scene_collection_sync(const struct Scene *scene);
/**
 * Update view layer collection tree from collections used in the scene.
 * This is used when collections are removed or added, both while editing
 * and on file loaded in case linked data changed or went missing.
 */
void BKE_layer_collection_sync(const struct Scene *scene, struct ViewLayer *view_layer);
void BKE_layer_collection_local_sync(const struct Scene *scene,
                                     struct ViewLayer *view_layer,
                                     const struct View3D *v3d);
/**
 * Sync the local collection for all the 3D Viewports.
 */
void BKE_layer_collection_local_sync_all(const struct Main *bmain);

void BKE_main_collection_sync_remap(const struct Main *bmain);

/**
 * Return the first matching #LayerCollection in the #ViewLayer for the Collection.
 */
struct LayerCollection *BKE_layer_collection_first_from_scene_collection(
    const struct ViewLayer *view_layer, const struct Collection *collection);
/**
 * See if view layer has the scene collection linked directly, or indirectly (nested).
 */
bool BKE_view_layer_has_collection(const struct ViewLayer *view_layer,
                                   const struct Collection *collection);
/**
 * See if the object is in any of the scene layers of the scene.
 */
bool BKE_scene_has_object(struct Scene *scene, struct Object *ob);

/* Selection and hiding. */

/**
 * Select all the objects of this layer collection
 *
 * It also select the objects that are in nested collections.
 * \note Recursive.
 */
bool BKE_layer_collection_objects_select(const struct Scene *scene,
                                         struct ViewLayer *view_layer,
                                         struct LayerCollection *lc,
                                         bool deselect);
bool BKE_layer_collection_has_selected_objects(const struct Scene *scene,
                                               struct ViewLayer *view_layer,
                                               struct LayerCollection *lc);
bool BKE_layer_collection_has_layer_collection(struct LayerCollection *lc_parent,
                                               struct LayerCollection *lc_child);

/**
 * Update after toggling visibility of an object base.
 */
void BKE_base_set_visible(struct Scene *scene,
                          struct ViewLayer *view_layer,
                          struct Base *base,
                          bool extend);
bool BKE_base_is_visible(const struct View3D *v3d, const struct Base *base);
bool BKE_object_is_visible_in_viewport(const struct View3D *v3d, const struct Object *ob);
/**
 * Isolate the collection - hide all other collections but this one.
 * Make sure to show all the direct parents and all children of the layer collection as well.
 * When extending we simply show the collections and its direct family.
 *
 * If the collection or any of its parents is disabled, make it enabled.
 * Don't change the children disable state though.
 */
void BKE_layer_collection_isolate_global(struct Scene *scene,
                                         struct ViewLayer *view_layer,
                                         struct LayerCollection *lc,
                                         bool extend);
/**
 * Isolate the collection locally
 *
 * Same as #BKE_layer_collection_isolate_local but for a viewport
 */
void BKE_layer_collection_isolate_local(const struct Scene *scene,
                                        struct ViewLayer *view_layer,
                                        const struct View3D *v3d,
                                        struct LayerCollection *lc,
                                        bool extend);
/**
 * Hide/show all the elements of a collection.
 * Don't change the collection children enable/disable state,
 * but it may change it for the collection itself.
 */
void BKE_layer_collection_set_visible(const struct Scene *scene,
                                      struct ViewLayer *view_layer,
                                      struct LayerCollection *lc,
                                      bool visible,
                                      bool hierarchy);
void BKE_layer_collection_set_flag(struct LayerCollection *lc, int flag, bool value);

/* Evaluation. */

/**
 * Applies object's restrict flags on top of flags coming from the collection
 * and stores those in `base->flag`. #BASE_ENABLED_AND_MAYBE_VISIBLE_IN_VIEWPORT ignores viewport
 * flags visibility (i.e., restriction and local collection).
 */
void BKE_base_eval_flags(struct Base *base);

void BKE_layer_eval_view_layer_indexed(struct Depsgraph *depsgraph,
                                       struct Scene *scene,
                                       int view_layer_index);

/* .blend file I/O */

void BKE_view_layer_blend_write(struct BlendWriter *writer,
                                const struct Scene *scene,
                                struct ViewLayer *view_layer);
void BKE_view_layer_blend_read_data(struct BlendDataReader *reader, struct ViewLayer *view_layer);
void BKE_view_layer_blend_read_lib(struct BlendLibReader *reader,
                                   struct ID *self_id,
                                   struct ViewLayer *view_layer);

/* iterators */

typedef struct ObjectsVisibleIteratorData {
  struct ViewLayer *view_layer;
  const struct View3D *v3d;
} ObjectsVisibleIteratorData;

void BKE_view_layer_selected_objects_iterator_begin(BLI_Iterator *iter, void *data_in);
void BKE_view_layer_selected_objects_iterator_next(BLI_Iterator *iter);
void BKE_view_layer_selected_objects_iterator_end(BLI_Iterator *iter);

void BKE_view_layer_visible_objects_iterator_begin(BLI_Iterator *iter, void *data_in);
void BKE_view_layer_visible_objects_iterator_next(BLI_Iterator *iter);
void BKE_view_layer_visible_objects_iterator_end(BLI_Iterator *iter);

void BKE_view_layer_selected_editable_objects_iterator_begin(BLI_Iterator *iter, void *data_in);
void BKE_view_layer_selected_editable_objects_iterator_next(BLI_Iterator *iter);
void BKE_view_layer_selected_editable_objects_iterator_end(BLI_Iterator *iter);

struct ObjectsInModeIteratorData {
  int object_mode;
  int object_type;
  struct ViewLayer *view_layer;
  const struct View3D *v3d;
  struct Base *base_active;
};

void BKE_view_layer_bases_in_mode_iterator_begin(BLI_Iterator *iter, void *data_in);
void BKE_view_layer_bases_in_mode_iterator_next(BLI_Iterator *iter);
void BKE_view_layer_bases_in_mode_iterator_end(BLI_Iterator *iter);

void BKE_view_layer_selected_bases_iterator_begin(BLI_Iterator *iter, void *data_in);
void BKE_view_layer_selected_bases_iterator_next(BLI_Iterator *iter);
void BKE_view_layer_selected_bases_iterator_end(BLI_Iterator *iter);

void BKE_view_layer_visible_bases_iterator_begin(BLI_Iterator *iter, void *data_in);
void BKE_view_layer_visible_bases_iterator_next(BLI_Iterator *iter);
void BKE_view_layer_visible_bases_iterator_end(BLI_Iterator *iter);

#define FOREACH_SELECTED_OBJECT_BEGIN(_view_layer, _v3d, _instance) \
  { \
    struct ObjectsVisibleIteratorData data_ = {NULL}; \
    data_.view_layer = _view_layer; \
    data_.v3d = _v3d; \
    ITER_BEGIN (BKE_view_layer_selected_objects_iterator_begin, \
                BKE_view_layer_selected_objects_iterator_next, \
                BKE_view_layer_selected_objects_iterator_end, \
                &data_, \
                Object *, \
                _instance)

#define FOREACH_SELECTED_OBJECT_END \
  ITER_END; \
  } \
  ((void)0)

#define FOREACH_SELECTED_EDITABLE_OBJECT_BEGIN(_view_layer, _v3d, _instance) \
  { \
    struct ObjectsVisibleIteratorData data_ = {NULL}; \
    data_.view_layer = _view_layer; \
    data_.v3d = _v3d; \
    ITER_BEGIN (BKE_view_layer_selected_editable_objects_iterator_begin, \
                BKE_view_layer_selected_editable_objects_iterator_next, \
                BKE_view_layer_selected_editable_objects_iterator_end, \
                &data_, \
                Object *, \
                _instance)

#define FOREACH_SELECTED_EDITABLE_OBJECT_END \
  ITER_END; \
  } \
  ((void)0)

#define FOREACH_VISIBLE_OBJECT_BEGIN(_view_layer, _v3d, _instance) \
  { \
    struct ObjectsVisibleIteratorData data_ = {NULL}; \
    data_.view_layer = _view_layer; \
    data_.v3d = _v3d; \
    ITER_BEGIN (BKE_view_layer_visible_objects_iterator_begin, \
                BKE_view_layer_visible_objects_iterator_next, \
                BKE_view_layer_visible_objects_iterator_end, \
                &data_, \
                Object *, \
                _instance)

#define FOREACH_VISIBLE_OBJECT_END \
  ITER_END; \
  } \
  ((void)0)

#define FOREACH_BASE_IN_MODE_BEGIN( \
    _scene, _view_layer, _v3d, _object_type, _object_mode, _instance) \
  { \
    struct ObjectsInModeIteratorData data_; \
    memset(&data_, 0, sizeof(data_)); \
    data_.object_mode = _object_mode; \
    data_.object_type = _object_type; \
    data_.view_layer = _view_layer; \
    data_.v3d = _v3d; \
    BKE_view_layer_synced_ensure(_scene, _view_layer); \
    data_.base_active = BKE_view_layer_active_base_get(_view_layer); \
    ITER_BEGIN (BKE_view_layer_bases_in_mode_iterator_begin, \
                BKE_view_layer_bases_in_mode_iterator_next, \
                BKE_view_layer_bases_in_mode_iterator_end, \
                &data_, \
                Base *, \
                _instance)

#define FOREACH_BASE_IN_MODE_END \
  ITER_END; \
  } \
  ((void)0)

#define FOREACH_BASE_IN_EDIT_MODE_BEGIN(_scene, _view_layer, _v3d, _instance) \
  FOREACH_BASE_IN_MODE_BEGIN (_scene, _view_layer, _v3d, -1, OB_MODE_EDIT, _instance)

#define FOREACH_BASE_IN_EDIT_MODE_END FOREACH_BASE_IN_MODE_END

#define FOREACH_OBJECT_IN_MODE_BEGIN( \
    _scene, _view_layer, _v3d, _object_type, _object_mode, _instance) \
  FOREACH_BASE_IN_MODE_BEGIN (_scene, _view_layer, _v3d, _object_type, _object_mode, _base) { \
    Object *_instance = _base->object;

#define FOREACH_OBJECT_IN_MODE_END \
  } \
  FOREACH_BASE_IN_MODE_END

#define FOREACH_OBJECT_IN_EDIT_MODE_BEGIN(_scene, _view_layer, _v3d, _instance) \
  FOREACH_BASE_IN_EDIT_MODE_BEGIN (_scene, _view_layer, _v3d, _base) { \
    Object *_instance = _base->object;

#define FOREACH_OBJECT_IN_EDIT_MODE_END \
  } \
  FOREACH_BASE_IN_EDIT_MODE_END

#define FOREACH_SELECTED_BASE_BEGIN(view_layer, _instance) \
  ITER_BEGIN (BKE_view_layer_selected_bases_iterator_begin, \
              BKE_view_layer_selected_bases_iterator_next, \
              BKE_view_layer_selected_bases_iterator_end, \
              view_layer, \
              Base *, \
              _instance)

#define FOREACH_SELECTED_BASE_END ITER_END

#define FOREACH_VISIBLE_BASE_BEGIN(_scene, _view_layer, _v3d, _instance) \
  { \
    struct ObjectsVisibleIteratorData data_ = {NULL}; \
    data_.view_layer = _view_layer; \
    data_.v3d = _v3d; \
    BKE_view_layer_synced_ensure(_scene, _view_layer); \
    ITER_BEGIN (BKE_view_layer_visible_bases_iterator_begin, \
                BKE_view_layer_visible_bases_iterator_next, \
                BKE_view_layer_visible_bases_iterator_end, \
                &data_, \
                Base *, \
                _instance)

#define FOREACH_VISIBLE_BASE_END \
  ITER_END; \
  } \
  ((void)0)

#define FOREACH_OBJECT_BEGIN(scene, view_layer, _instance) \
  { \
    Object *_instance; \
    Base *_base; \
    BKE_view_layer_synced_ensure(scene, view_layer); \
    for (_base = (Base *)BKE_view_layer_object_bases_get(view_layer)->first; _base; \
         _base = _base->next) \
    { \
      _instance = _base->object;

#define FOREACH_OBJECT_END \
  } \
  } \
  ((void)0)

#define FOREACH_OBJECT_FLAG_BEGIN(scene, _view_layer, _v3d, flag, _instance) \
  { \
    IteratorBeginCb func_begin; \
    IteratorCb func_next, func_end; \
    void *data_in; \
\
    struct ObjectsVisibleIteratorData data_select_ = {NULL}; \
    data_select_.view_layer = _view_layer; \
    data_select_.v3d = _v3d; \
\
    struct SceneObjectsIteratorExData data_flag_ = {NULL}; \
    data_flag_.scene = scene; \
    data_flag_.flag = flag; \
\
    if (flag == SELECT) { \
      func_begin = &BKE_view_layer_selected_objects_iterator_begin; \
      func_next = &BKE_view_layer_selected_objects_iterator_next; \
      func_end = &BKE_view_layer_selected_objects_iterator_end; \
      data_in = &data_select_; \
    } \
    else if (flag != 0) { \
      func_begin = BKE_scene_objects_iterator_begin_ex; \
      func_next = BKE_scene_objects_iterator_next_ex; \
      func_end = BKE_scene_objects_iterator_end_ex; \
      data_in = &data_flag_; \
    } \
    else { \
      func_begin = BKE_scene_objects_iterator_begin; \
      func_next = BKE_scene_objects_iterator_next; \
      func_end = BKE_scene_objects_iterator_end; \
      data_in = (scene); \
    } \
    ITER_BEGIN (func_begin, func_next, func_end, data_in, Object *, _instance)

#define FOREACH_OBJECT_FLAG_END \
  ITER_END; \
  } \
  ((void)0)

/* layer_utils.c */

struct ObjectsInViewLayerParams {
  uint no_dup_data : 1;

  bool (*filter_fn)(const struct Object *ob, void *user_data);
  void *filter_userdata;
};

struct Object **BKE_view_layer_array_selected_objects_params(
    struct ViewLayer *view_layer,
    const struct View3D *v3d,
    uint *r_len,
    const struct ObjectsInViewLayerParams *params);

/**
 * Use this in rare cases we need to detect a pair of objects (active, selected).
 * This returns the other non-active selected object.
 *
 * Returns NULL with it finds multiple other selected objects
 * as behavior in this case would be random from the user perspective.
 */
struct Object *BKE_view_layer_non_active_selected_object(const struct Scene *scene,
                                                         struct ViewLayer *view_layer,
                                                         const struct View3D *v3d);

struct ObjectsInModeParams {
  int object_mode;
  uint no_dup_data : 1;

  bool (*filter_fn)(const struct Object *ob, void *user_data);
  void *filter_userdata;
};

struct Base **BKE_view_layer_array_from_bases_in_mode_params(
    const struct Scene *scene,
    struct ViewLayer *view_layer,
    const struct View3D *v3d,
    uint *r_len,
    const struct ObjectsInModeParams *params);

struct Object **BKE_view_layer_array_from_objects_in_mode_params(
    const struct Scene *scene,
    struct ViewLayer *view_layer,
    const struct View3D *v3d,
    uint *len,
    const struct ObjectsInModeParams *params);

bool BKE_view_layer_filter_edit_mesh_has_uvs(const struct Object *ob, void *user_data);
bool BKE_view_layer_filter_edit_mesh_has_edges(const struct Object *ob, void *user_data);

/* Utility functions that wrap common arguments (add more as needed). */

struct Object **BKE_view_layer_array_from_objects_in_edit_mode(const struct Scene *scene,
                                                               struct ViewLayer *view_layer,
                                                               const struct View3D *v3d,
                                                               uint *r_len);
struct Base **BKE_view_layer_array_from_bases_in_edit_mode(const struct Scene *scene,
                                                           struct ViewLayer *view_layer,
                                                           const struct View3D *v3d,
                                                           uint *r_len);
struct Object **BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
    const struct Scene *scene,
    struct ViewLayer *view_layer,
    const struct View3D *v3d,
    uint *r_len);

struct Base **BKE_view_layer_array_from_bases_in_edit_mode_unique_data(
    const struct Scene *scene,
    struct ViewLayer *view_layer,
    const struct View3D *v3d,
    uint *r_len);
struct Object **BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
    const struct Scene *scene,
    struct ViewLayer *view_layer,
    const struct View3D *v3d,
    uint *r_len);
struct Object **BKE_view_layer_array_from_objects_in_mode_unique_data(const struct Scene *scene,
                                                                      struct ViewLayer *view_layer,
                                                                      const struct View3D *v3d,
                                                                      uint *r_len,
                                                                      eObjectMode mode);
struct Object *BKE_view_layer_active_object_get(const struct ViewLayer *view_layer);
struct Object *BKE_view_layer_edit_object_get(const struct ViewLayer *view_layer);

struct ListBase *BKE_view_layer_object_bases_get(struct ViewLayer *view_layer);
struct Base *BKE_view_layer_active_base_get(struct ViewLayer *view_layer);

struct LayerCollection *BKE_view_layer_active_collection_get(struct ViewLayer *view_layer);

void BKE_view_layer_need_resync_tag(struct ViewLayer *view_layer);
void BKE_view_layer_synced_ensure(const struct Scene *scene, struct ViewLayer *view_layer);

void BKE_scene_view_layers_synced_ensure(const struct Scene *scene);
void BKE_main_view_layers_synced_ensure(const struct Main *bmain);

struct ViewLayerAOV *BKE_view_layer_add_aov(struct ViewLayer *view_layer);
void BKE_view_layer_remove_aov(struct ViewLayer *view_layer, struct ViewLayerAOV *aov);
void BKE_view_layer_set_active_aov(struct ViewLayer *view_layer, struct ViewLayerAOV *aov);
/**
 * Update the naming and conflicts of the AOVs.
 *
 * Name must be unique between all AOVs.
 * Conflicts with render passes will show a conflict icon. Reason is that switching a render
 * engine or activating a render pass could lead to other conflicts that wouldn't be that clear
 * for the user.
 */
void BKE_view_layer_verify_aov(struct RenderEngine *engine,
                               struct Scene *scene,
                               struct ViewLayer *view_layer);
/**
 * Check if the given view layer has at least one valid AOV.
 */
bool BKE_view_layer_has_valid_aov(struct ViewLayer *view_layer);
struct ViewLayer *BKE_view_layer_find_with_aov(struct Scene *scene,
                                               struct ViewLayerAOV *view_layer_aov);

struct ViewLayerLightgroup *BKE_view_layer_add_lightgroup(struct ViewLayer *view_layer,
                                                          const char *name);
void BKE_view_layer_remove_lightgroup(struct ViewLayer *view_layer,
                                      struct ViewLayerLightgroup *lightgroup);
void BKE_view_layer_set_active_lightgroup(struct ViewLayer *view_layer,
                                          struct ViewLayerLightgroup *lightgroup);
struct ViewLayer *BKE_view_layer_find_with_lightgroup(
    struct Scene *scene, struct ViewLayerLightgroup *view_layer_lightgroup);
void BKE_view_layer_rename_lightgroup(struct Scene *scene,
                                      ViewLayer *view_layer,
                                      ViewLayerLightgroup *lightgroup,
                                      const char *name);

int BKE_lightgroup_membership_get(const struct LightgroupMembership *lgm, char *name);
int BKE_lightgroup_membership_length(const struct LightgroupMembership *lgm);
void BKE_lightgroup_membership_set(struct LightgroupMembership **lgm, const char *name);

#ifdef __cplusplus
}
#endif
