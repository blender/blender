/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_vector.hh"

#include "BKE_collection.hh"

#include "DNA_layer_types.h"
#include "DNA_listBase.h"
#include "DNA_object_enums.h"

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

enum eViewLayerCopyMethod {
  VIEWLAYER_ADD_NEW = 0,
  VIEWLAYER_ADD_EMPTY = 1,
  VIEWLAYER_ADD_COPY = 2,
};

/**
 * Returns the default view layer to view in work-spaces if there is
 * none linked to the workspace yet.
 */
ViewLayer *BKE_view_layer_default_view(const Scene *scene);
/**
 * Returns the default view layer to render if we need to render just one.
 */
ViewLayer *BKE_view_layer_default_render(const Scene *scene);
/**
 * Returns view layer with matching name, or NULL if not found.
 */
ViewLayer *BKE_view_layer_find(const Scene *scene, const char *layer_name);
/**
 * Add a new view layer by default, a view layer has the master collection.
 */
ViewLayer *BKE_view_layer_add(Scene *scene,
                              const char *name,
                              ViewLayer *view_layer_source,
                              int type);

/* DEPRECATED */
/**
 * This is a placeholder to know which areas of the code need to be addressed
 * for the Workspace changes. Never use this, you should typically get the
 * active layer from the context or window.
 */
ViewLayer *BKE_view_layer_context_active_PLACEHOLDER(const Scene *scene);

void BKE_view_layer_free(ViewLayer *view_layer);
/**
 * Free (or release) any data used by this #ViewLayer.
 */
void BKE_view_layer_free_ex(ViewLayer *view_layer, bool do_id_user);

/**
 * Free the bases of this #ViewLayer, and what they reference.
 * This includes `baseact`, `object_bases`, `object_bases_hash`, and `layer_collections`.
 */
void BKE_view_layer_free_object_content(ViewLayer *view_layer);

/**
 * Tag all the selected objects of a render-layer.
 */
void BKE_view_layer_selected_objects_tag(const Scene *scene, ViewLayer *view_layer, int tag);

/**
 * Fallback for when a Scene has no camera to use.
 *
 * \param view_layer: in general you want to use the same #ViewLayer that is used for depsgraph.
 * If rendering you pass the scene active layer, when viewing in the viewport
 * you want to get #ViewLayer from context.
 */
Object *BKE_view_layer_camera_find(const Scene *scene, ViewLayer *view_layer);
/**
 * Find the #ViewLayer a #LayerCollection belongs to.
 */
ViewLayer *BKE_view_layer_find_from_collection(const Scene *scene, LayerCollection *lc);
Base *BKE_view_layer_base_find(ViewLayer *view_layer, Object *ob);
void BKE_view_layer_base_deselect_all(const Scene *scene, ViewLayer *view_layer);

void BKE_view_layer_base_select_and_set_active(ViewLayer *view_layer, Base *selbase);

/**
 * Only copy internal data of #ViewLayer from source to already allocated/initialized destination.
 *
 * \param flag: Copying options (see BKE_lib_id.hh's LIB_ID_COPY_... flags for more).
 */
void BKE_view_layer_copy_data(Scene *scene_dst,
                              const Scene *scene_src,
                              ViewLayer *view_layer_dst,
                              const ViewLayer *view_layer_src,
                              int flag);

void BKE_view_layer_rename(Main *bmain, Scene *scene, ViewLayer *view_layer, const char *newname);

/**
 * Get the active collection
 */
LayerCollection *BKE_layer_collection_get_active(ViewLayer *view_layer);
/**
 * Activate collection
 */
bool BKE_layer_collection_activate(ViewLayer *view_layer, LayerCollection *lc);
/**
 * Activate first parent collection.
 */
LayerCollection *BKE_layer_collection_activate_parent(ViewLayer *view_layer, LayerCollection *lc);

/**
 * Get the total number of collections (including all the nested collections)
 */
int BKE_layer_collection_count(const ViewLayer *view_layer);

/**
 * Get the collection for a given index.
 */
LayerCollection *BKE_layer_collection_from_index(ViewLayer *view_layer, int index);
/**
 * \return -1 if not found.
 */
int BKE_layer_collection_findindex(ViewLayer *view_layer, const LayerCollection *lc);

void BKE_layer_collection_resync_forbid();
void BKE_layer_collection_resync_allow();

/**
 * Helper to fix older pre-2.80 blend-files.
 *
 * Ensures the given `view_layer` as a valid first-level layer collection, i.e. a single one
 * matching the scene's master collection. This is a requirement for `BKE_layer_collection_sync`.
 */
void BKE_layer_collection_doversion_2_80(const Scene *scene, ViewLayer *view_layer);

/**
 * Tag all viewlayers of all the scenes of the given `main` as being #VIEW_LAYER_OUT_OF_SYNC.
 *
 * Also directly update all local viewlayers (used in 3DView in local mode).
 *
 * \return `true` if all viewlayers were successfully tagged (or resynced for the local ones),
 * `false` otherwise. See also #BKE_layer_collection_sync.
 */
bool BKE_main_collection_sync(const Main *bmain);
/** Same as for #BKE_main_collection_sync, but for a single scene only. */
bool BKE_scene_collection_sync(const Scene *scene);
/**
 * Similar to #BKE_main_collection_sync, but does additional cache cleanups and depsgraph tagging,
 * required after remapping objects/collections ID pointers.
 *
 * \return `true` if all viewlayers were successfully tagged (or resynced for the local ones),
 * `false` otherwise. See also #BKE_layer_collection_sync.
 */
bool BKE_main_collection_sync_remap(const Main *bmain);
/**
 * Update view layer collection tree from collections used in the scene.
 * This is used when collections are removed or added, both while editing
 * and on file loaded in case linked data changed or went missing.
 *
 * \warning Calling this function directly should almost never be necessary, and should be avoided
 * at all costs. It is utterly unsafe in multi-threaded context, among other risks. The typical
 * process is to tag view layers for updates with #BKE_view_layer_need_resync_tag (or the more
 * general #BKE_scene_collection_sync/#BKE_main_collection_sync), and only enure the layers are
 * up-to-date when actually needed, using #BKE_view_layer_synced_ensure and realted API.
 *
 * \return `true` if the viewlayer was successfully resynced (or already in sync), `false` if a
 * resync was needed but could not be performed (e.g. because resync is locked by one or more calls
 * to #BKE_layer_collection_resync_forbid).
 */
bool BKE_layer_collection_sync(const Scene *scene, ViewLayer *view_layer);
/**
 * Sync the local visibility of collections & objects, for the given 3D Viewport in 'local' view
 * mode.
 *
 * \return `true` if the local viewport info were successfully resynced, `false` otherwise. See
 * also #BKE_layer_collection_sync.
 */
bool BKE_layer_collection_local_sync(const Scene *scene, ViewLayer *view_layer, const View3D *v3d);
/**
 * Sync the local visibility of collections & objects, for all 3D Viewports in 'local' view mode.
 *
 * \return `true` if all local info were successfully resynced, `false` otherwise. See also
 * #BKE_layer_collection_sync.
 */
bool BKE_layer_collection_local_sync_all(const Main *bmain);

/**
 * Return the first matching #LayerCollection in the #ViewLayer for the Collection.
 */
LayerCollection *BKE_layer_collection_first_from_scene_collection(const ViewLayer *view_layer,
                                                                  const Collection *collection);
/**
 * See if view layer has the scene collection linked directly, or indirectly (nested).
 */
bool BKE_view_layer_has_collection(const ViewLayer *view_layer, const Collection *collection);
/**
 * See if the object is in any of the scene layers of the scene.
 */
bool BKE_scene_has_object(Scene *scene, Object *ob);

/* Selection and hiding. */

/**
 * Select all the objects of this layer collection
 *
 * It also select the objects that are in nested collections.
 * \note Recursive.
 */
bool BKE_layer_collection_objects_select(const Scene *scene,
                                         ViewLayer *view_layer,
                                         LayerCollection *lc,
                                         bool deselect);
bool BKE_layer_collection_has_selected_objects(const Scene *scene,
                                               ViewLayer *view_layer,
                                               LayerCollection *lc);
bool BKE_layer_collection_has_layer_collection(LayerCollection *lc_parent,
                                               LayerCollection *lc_child);

/**
 * Update after toggling visibility of an object base.
 */
void BKE_base_set_visible(Scene *scene, ViewLayer *view_layer, Base *base, bool extend);
bool BKE_base_is_visible(const View3D *v3d, const Base *base);
bool BKE_object_is_visible_in_viewport(const View3D *v3d, const Object *ob);
/**
 * Isolate the collection - hide all other collections but this one.
 * Make sure to show all the direct parents and all children of the layer collection as well.
 * When extending we simply show the collections and its direct family.
 *
 * If the collection or any of its parents is disabled, make it enabled.
 * Don't change the children disable state though.
 */
void BKE_layer_collection_isolate_global(Scene *scene,
                                         ViewLayer *view_layer,
                                         LayerCollection *lc,
                                         bool extend);
/**
 * Isolate the collection locally
 *
 * Same as #BKE_layer_collection_isolate_local but for a viewport
 */
void BKE_layer_collection_isolate_local(const Scene *scene,
                                        ViewLayer *view_layer,
                                        const View3D *v3d,
                                        LayerCollection *lc,
                                        bool extend);
/**
 * Hide/show all the elements of a collection.
 * Don't change the collection children enable/disable state,
 * but it may change it for the collection itself.
 */
void BKE_layer_collection_set_visible(
    const Scene *scene, ViewLayer *view_layer, LayerCollection *lc, bool visible, bool hierarchy);
void BKE_layer_collection_set_flag(LayerCollection *lc, int flag, bool value);

/* Evaluation. */

/**
 * Applies object's restrict flags on top of flags coming from the collection
 * and stores those in `base->flag`. #BASE_ENABLED_AND_MAYBE_VISIBLE_IN_VIEWPORT ignores viewport
 * flags visibility (i.e., restriction and local collection).
 */
void BKE_base_eval_flags(Base *base);

void BKE_layer_eval_view_layer_indexed(Depsgraph *depsgraph, Scene *scene, int view_layer_index);

/* .blend file I/O */

void BKE_view_layer_blend_write(BlendWriter *writer, const Scene *scene, ViewLayer *view_layer);
void BKE_view_layer_blend_read_data(BlendDataReader *reader, ViewLayer *view_layer);
void BKE_view_layer_blend_read_after_liblink(BlendLibReader *reader,
                                             ID *self_id,
                                             ViewLayer *view_layer);

/* iterators */

struct ObjectsVisibleIteratorData {
  /** Can be null, in this case all scene objects are iterated over. */
  ViewLayer *view_layer;
  /** Can be null, in this case local-view & view settings are ignored. */
  const View3D *v3d;
};

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
  ViewLayer *view_layer;
  const View3D *v3d;
  Base *base_active;
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
    ObjectsVisibleIteratorData data_ = {NULL}; \
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
    ObjectsVisibleIteratorData data_ = {NULL}; \
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
    ObjectsVisibleIteratorData data_ = {NULL}; \
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
    ObjectsInModeIteratorData data_; \
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
    ObjectsVisibleIteratorData data_ = {NULL}; \
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

#define FOREACH_OBJECT_FLAG_BEGIN(_scene, _view_layer, _v3d, _flag, _instance) \
  { \
    IteratorBeginCb func_begin; \
    IteratorCb func_next, func_end; \
    void *data_in; \
\
    ObjectsVisibleIteratorData data_select_ = {NULL}; \
    data_select_.view_layer = _view_layer; \
    data_select_.v3d = _v3d; \
\
    SceneObjectsIteratorExData data_flag_ = {NULL}; \
    data_flag_.scene = _scene; \
    switch ((data_flag_.flag = _flag)) { \
      case SELECT: { \
        func_begin = &BKE_view_layer_selected_objects_iterator_begin; \
        func_next = &BKE_view_layer_selected_objects_iterator_next; \
        func_end = &BKE_view_layer_selected_objects_iterator_end; \
        data_in = &data_select_; \
        break; \
      } \
      case 0: { \
        func_begin = BKE_scene_objects_iterator_begin; \
        func_next = BKE_scene_objects_iterator_next; \
        func_end = BKE_scene_objects_iterator_end; \
        data_in = data_flag_.scene; \
        break; \
      } \
      default: { \
        func_begin = BKE_scene_objects_iterator_begin_ex; \
        func_next = BKE_scene_objects_iterator_next_ex; \
        func_end = BKE_scene_objects_iterator_end_ex; \
        data_in = &data_flag_; \
        break; \
      } \
    } \
    if (data_select_.view_layer) { \
      BKE_view_layer_synced_ensure(data_flag_.scene, data_select_.view_layer); \
    } \
    ITER_BEGIN (func_begin, func_next, func_end, data_in, Object *, _instance)

#define FOREACH_OBJECT_FLAG_END \
  ITER_END; \
  } \
  ((void)0)

/* `layer_utils.cc` */

struct ObjectsInViewLayerParams {
  uint no_dup_data : 1;

  bool (*filter_fn)(const Object *ob, void *user_data);
  void *filter_userdata;
};

blender::Vector<Object *> BKE_view_layer_array_selected_objects_params(
    ViewLayer *view_layer, const View3D *v3d, const ObjectsInViewLayerParams *params);

/**
 * Use this in rare cases we need to detect a pair of objects (active, selected).
 * This returns the other non-active selected object.
 *
 * Returns NULL with it finds multiple other selected objects
 * as behavior in this case would be random from the user perspective.
 */
Object *BKE_view_layer_non_active_selected_object(const Scene *scene,
                                                  ViewLayer *view_layer,
                                                  const View3D *v3d);

struct ObjectsInModeParams {
  int object_mode;
  uint no_dup_data : 1;

  bool (*filter_fn)(const Object *ob, void *user_data);
  void *filter_userdata;
};

blender::Vector<Base *> BKE_view_layer_array_from_bases_in_mode_params(
    const Scene *scene,
    ViewLayer *view_layer,
    const View3D *v3d,
    const ObjectsInModeParams *params);

blender::Vector<Object *> BKE_view_layer_array_from_objects_in_mode_params(
    const Scene *scene,
    ViewLayer *view_layer,
    const View3D *v3d,
    const ObjectsInModeParams *params);

bool BKE_view_layer_filter_edit_mesh_has_uvs(const Object *ob, void *user_data);
bool BKE_view_layer_filter_edit_mesh_has_edges(const Object *ob, void *user_data);

/* Utility functions that wrap common arguments (add more as needed). */

blender::Vector<Object *> BKE_view_layer_array_from_objects_in_edit_mode(const Scene *scene,
                                                                         ViewLayer *view_layer,
                                                                         const View3D *v3d);
blender::Vector<Base *> BKE_view_layer_array_from_bases_in_edit_mode(const Scene *scene,
                                                                     ViewLayer *view_layer,
                                                                     const View3D *v3d);
blender::Vector<Object *> BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
    const Scene *scene, ViewLayer *view_layer, const View3D *v3d);

blender::Vector<Base *> BKE_view_layer_array_from_bases_in_edit_mode_unique_data(
    const Scene *scene, ViewLayer *view_layer, const View3D *v3d);
blender::Vector<Object *> BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
    const Scene *scene, ViewLayer *view_layer, const View3D *v3d);
blender::Vector<Object *> BKE_view_layer_array_from_objects_in_mode_unique_data(
    const Scene *scene, ViewLayer *view_layer, const View3D *v3d, eObjectMode mode);
Object *BKE_view_layer_active_object_get(const ViewLayer *view_layer);
Object *BKE_view_layer_edit_object_get(const ViewLayer *view_layer);

ListBase *BKE_view_layer_object_bases_get(ViewLayer *view_layer);
/**
 * Same as the above, but does not assert that the viewlayer is synced.
 *
 * \warning Use with _extreme_ care, as it means the data returned by this call may not be valid.
 */
ListBase *BKE_view_layer_object_bases_unsynced_get(ViewLayer *view_layer);

Base *BKE_view_layer_active_base_get(ViewLayer *view_layer);

LayerCollection *BKE_view_layer_active_collection_get(ViewLayer *view_layer);

/**
 * Tag the given view-layer as being #VIEW_LAYER_OUT_OF_SYNC with the hierarchy of collections and
 * objects it represents.
 *
 * This allows to defer the actual resync process to when up-to-date data is required (see
 * #BKE_view_layer_synced_ensure and related API).
 */
void BKE_view_layer_need_resync_tag(ViewLayer *view_layer);
/**
 * Ensure that the given `scene`'s `view_layer`  is fully in sync with the hierarchy of collections
 * and objects it represents.
 *
 * \return `true` if the viewlayer was successfully resynced, `false` otherwise. See also
 * #BKE_layer_collection_sync.
 */
bool BKE_view_layer_synced_ensure(const Scene *scene, ViewLayer *view_layer);
/**
 * \return `true` if all viewlayers were successfully resynced, `false` otherwise. See also
 * #BKE_layer_collection_sync.
 */
bool BKE_scene_view_layers_synced_ensure(const Scene *scene);
/**
 * \return `true` if all viewlayers were successfully resynced, `false` otherwise. See also
 * #BKE_layer_collection_sync.
 */
bool BKE_main_view_layers_synced_ensure(const Main *bmain);

ViewLayerAOV *BKE_view_layer_add_aov(ViewLayer *view_layer);
void BKE_view_layer_remove_aov(ViewLayer *view_layer, ViewLayerAOV *aov);
void BKE_view_layer_set_active_aov(ViewLayer *view_layer, ViewLayerAOV *aov);
/**
 * Update the naming and conflicts of the AOVs.
 *
 * Name must be unique between all AOVs.
 * Conflicts with render passes will show a conflict icon. Reason is that switching a render
 * engine or activating a render pass could lead to other conflicts that wouldn't be that clear
 * for the user.
 */
void BKE_view_layer_verify_aov(RenderEngine *engine, Scene *scene, ViewLayer *view_layer);
/**
 * Check if the given view layer has at least one valid AOV.
 */
bool BKE_view_layer_has_valid_aov(ViewLayer *view_layer);
ViewLayer *BKE_view_layer_find_with_aov(Scene *scene, ViewLayerAOV *aov);

ViewLayerLightgroup *BKE_view_layer_add_lightgroup(ViewLayer *view_layer, const char *name);
void BKE_view_layer_remove_lightgroup(ViewLayer *view_layer, ViewLayerLightgroup *lightgroup);
void BKE_view_layer_set_active_lightgroup(ViewLayer *view_layer, ViewLayerLightgroup *lightgroup);
ViewLayer *BKE_view_layer_find_with_lightgroup(Scene *scene, ViewLayerLightgroup *lightgroup);
void BKE_view_layer_rename_lightgroup(Scene *scene,
                                      ViewLayer *view_layer,
                                      ViewLayerLightgroup *lightgroup,
                                      const char *name);

int BKE_lightgroup_membership_get(const LightgroupMembership *lgm, char *name);
int BKE_lightgroup_membership_length(const LightgroupMembership *lgm);
void BKE_lightgroup_membership_set(LightgroupMembership **lgm, const char *name);
