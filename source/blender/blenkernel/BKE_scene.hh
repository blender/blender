/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_function_ref.hh"
#include "BLI_sys_types.h"

#include "BKE_duplilist.hh"

struct Base;
struct Collection;
struct Depsgraph;
struct ImageFormatData;
struct GHash;
struct Main;
struct Object;
struct RenderData;
struct Scene;
struct SceneRenderView;
struct ToolSettings;
struct TransformOrientation;
struct TransformOrientationSlot;
struct UnitSettings;
struct ViewLayer;

enum eSceneCopyMethod {
  SCE_COPY_NEW = 0,
  SCE_COPY_EMPTY = 1,
  SCE_COPY_LINK_COLLECTION = 2,
  SCE_COPY_FULL = 3,
};

/** Use as the contents of a 'for' loop: `for (SETLOOPER(...)) { ... }`. */
#define SETLOOPER(_sce_basis, _sce_iter, _base) \
  _sce_iter = _sce_basis, \
  _base = _setlooper_base_step( \
      &_sce_iter, BKE_view_layer_context_active_PLACEHOLDER(_sce_basis), NULL); \
  _base; \
  _base = _setlooper_base_step(&_sce_iter, NULL, _base)

#define SETLOOPER_VIEW_LAYER(_sce_basis, _view_layer, _sce_iter, _base) \
  _sce_iter = _sce_basis, _base = _setlooper_base_step(&_sce_iter, _view_layer, NULL); \
  _base; \
  _base = _setlooper_base_step(&_sce_iter, NULL, _base)

#define SETLOOPER_SET_ONLY(_sce_basis, _sce_iter, _base) \
  _sce_iter = _sce_basis, _base = _setlooper_base_step(&_sce_iter, NULL, NULL); \
  _base; \
  _base = _setlooper_base_step(&_sce_iter, NULL, _base)

/**
 * Helper function for the #SETLOOPER and #SETLOOPER_VIEW_LAYER macros
 *
 * It iterates over the bases of the active layer and then the bases
 * of the active layer of the background (set) scenes recursively.
 */
Base *_setlooper_base_step(Scene **sce_iter, ViewLayer *view_layer, Base *base);

Scene *BKE_scene_add(Main *bmain, const char *name);

void BKE_scene_remove_rigidbody_object(Main *bmain, Scene *scene, Object *ob, bool free_us);

/**
 * Check if there is any instance of the object in the scene.
 */
bool BKE_scene_object_find(Scene *scene, Object *ob);
Object *BKE_scene_object_find_by_name(const Scene *scene, const char *name);

/* Scene base iteration function.
 * Define struct here, so no need to bother with alloc/free it.
 */
struct SceneBaseIter {
  DupliList duplilist;
  DupliObject *dupob;
  int dupob_index;
  float omat[4][4];
  Object *dupli_refob;
  int phase;
};

/**
 * Used by meta-balls, return *all* objects (including duplis)
 * existing in the scene (including scene's sets).
 */
int BKE_scene_base_iter_next(
    Depsgraph *depsgraph, SceneBaseIter *iter, Scene **scene, int val, Base **base, Object **ob);

void BKE_scene_base_flag_to_objects(const Scene *scene, ViewLayer *view_layer);
/**
 * Synchronize object base flags
 *
 * This is usually handled by the depsgraph.
 * However, in rare occasions we need to use the latest object flags
 * before depsgraph is fully updated.
 *
 * It should (ideally) only run for copy-on-written objects since this is
 * runtime data generated per-view-layer.
 */
void BKE_scene_object_base_flag_sync_from_base(Base *base);

/**
 * Sets the active scene, mainly used when running in background mode
 * (`--scene` command line argument).
 * This is also called to set the scene directly, bypassing windowing code.
 * Otherwise #WM_window_set_active_scene is used when changing scenes by the user.
 */
void BKE_scene_set_background(Main *bmain, Scene *sce);
/**
 * Called from `creator_args.cc`.
 */
Scene *BKE_scene_set_name(Main *bmain, const char *name);

/**
 * \param flag: copying options (see BKE_lib_id.hh's `LIB_ID_COPY_...` flags for more).
 */
ToolSettings *BKE_toolsettings_copy(ToolSettings *toolsettings, int flag);
void BKE_toolsettings_free(ToolSettings *toolsettings);

Scene *BKE_scene_duplicate(Main *bmain, Scene *sce, eSceneCopyMethod type);
void BKE_scene_groups_relink(Scene *sce);

/**
 * Check if the given scene can be deleted, i.e. if there is at least one other local Scene in the
 * given Main.
 */
bool BKE_scene_can_be_removed(const Main *bmain, const Scene *scene);
/**
 * Find a replacement scene for the given one (typically when the given scene is going to be
 * deleted).
 *
 * By default, it will simply return one of its nearest neighbors in Main (the previous one if
 * possible).
 *
 * If a validation callback is provided, only a scene which returns `true` when passed to this
 * callback will be returned. Scenes before the given one are checked first, in reversed order (so
 * starting from the given one).
 *
 * \returns A valid replacement scene, or nullptr if no suitable replacement scene was found.
 */
Scene *BKE_scene_find_replacement(
    const Main &bmain,
    const Scene &scene,
    blender::FunctionRef<bool(const Scene &scene)> scene_validate_cb = nullptr);

bool BKE_scene_has_view_layer(const Scene *scene, const ViewLayer *layer);
Scene *BKE_scene_find_from_collection(const Main *bmain, const Collection *collection);

Object *BKE_scene_camera_switch_find(Scene *scene);
bool BKE_scene_camera_switch_update(Scene *scene);

const char *BKE_scene_find_marker_name(const Scene *scene, int frame);
/**
 * Return the current marker for this frame,
 * we can have more than 1 marker per frame, this just returns the first (unfortunately).
 */
const char *BKE_scene_find_last_marker_name(const Scene *scene, int frame);

float BKE_scene_frame_snap_by_seconds(const Scene *scene, double interval_in_seconds, float frame);

/**
 * Checks for cycle, returns true if it's all OK.
 */
bool BKE_scene_validate_setscene(Main *bmain, Scene *sce);

/**
 * Return fractional frame number taking into account sub-frames and time
 * remapping. This the time value used by animation, modifiers and physics
 * evaluation. */
float BKE_scene_ctime_get(const Scene *scene);
/**
 * Convert integer frame number to fractional frame number taking into account
 * sub-frames and time remapping.
 */
float BKE_scene_frame_to_ctime(const Scene *scene, int frame);

/**
 * Get current fractional frame based on frame and sub-frame.
 */
float BKE_scene_frame_get(const Scene *scene);
/**
 * Set current frame and sub-frame based on a fractional frame.
 */
void BKE_scene_frame_set(Scene *scene, float frame);

TransformOrientationSlot *BKE_scene_orientation_slot_get_from_flag(Scene *scene, int flag);
TransformOrientationSlot *BKE_scene_orientation_slot_get(Scene *scene, int slot_index);
/**
 * Activate a transform orientation in a 3D view based on an enum value.
 *
 * \param orientation: If this is #V3D_ORIENT_CUSTOM or greater, the custom transform orientation
 * with index \a orientation - #V3D_ORIENT_CUSTOM gets activated.
 */
void BKE_scene_orientation_slot_set_index(TransformOrientationSlot *orient_slot, int orientation);
int BKE_scene_orientation_slot_get_index(const TransformOrientationSlot *orient_slot);
int BKE_scene_orientation_get_index(Scene *scene, int slot_index);
int BKE_scene_orientation_get_index_from_flag(Scene *scene, int flag);

/* **  Scene evaluation ** */

void BKE_scene_update_sound(Depsgraph *depsgraph, Main *bmain);
void BKE_scene_update_tag_audio_volume(Depsgraph *, Scene *scene);

void BKE_scene_graph_update_tagged(Depsgraph *depsgraph, Main *bmain);
void BKE_scene_graph_evaluated_ensure(Depsgraph *depsgraph, Main *bmain);

void BKE_scene_graph_update_for_newframe(Depsgraph *depsgraph);
/**
 * Applies changes right away, does all sets too.
 */
void BKE_scene_graph_update_for_newframe_ex(Depsgraph *depsgraph, bool clear_recalc);

/**
 * Ensures given scene/view_layer pair has a valid, up-to-date depsgraph.
 *
 * \warning Sets matching depsgraph as active,
 * so should only be called from the active editing context (usually, from operators).
 */
void BKE_scene_view_layer_graph_evaluated_ensure(Main *bmain, Scene *scene, ViewLayer *view_layer);

/**
 * Return default view.
 */
SceneRenderView *BKE_scene_add_render_view(Scene *sce, const char *name);
bool BKE_scene_remove_render_view(Scene *scene, SceneRenderView *srv);

/* Render profile. */

int get_render_subsurf_level(const RenderData *r, int lvl, bool for_render);
int get_render_child_particle_number(const RenderData *r, int child_num, bool for_render);

bool BKE_scene_use_shading_nodes_custom(Scene *scene);
bool BKE_scene_use_spherical_stereo(Scene *scene);

bool BKE_scene_uses_blender_eevee(const Scene *scene);
bool BKE_scene_uses_blender_workbench(const Scene *scene);
bool BKE_scene_uses_cycles(const Scene *scene);

bool BKE_scene_uses_shader_previews(const Scene *scene);

void BKE_scene_copy_data_eevee(Scene *sce_dst, const Scene *sce_src);

void BKE_scene_disable_color_management(Scene *scene);
bool BKE_scene_check_rigidbody_active(const Scene *scene);

int BKE_scene_num_threads(const Scene *scene);
int BKE_render_num_threads(const RenderData *r);

void BKE_render_resolution(const RenderData *r, const bool use_crop, int *r_width, int *r_height);
int BKE_render_preview_pixel_size(const RenderData *r);

/**********************************/

/* Multi-view. */

bool BKE_scene_multiview_is_stereo3d(const RenderData *rd);
/**
 * Return whether to render this #SceneRenderView.
 */
bool BKE_scene_multiview_is_render_view_active(const RenderData *rd, const SceneRenderView *srv);
/**
 * \return true if `viewname` is the first or if the name is NULL or not found.
 */
bool BKE_scene_multiview_is_render_view_first(const RenderData *rd, const char *viewname);
/**
 * \return true if `viewname` is the last or if the name is NULL or not found.
 */
bool BKE_scene_multiview_is_render_view_last(const RenderData *rd, const char *viewname);
int BKE_scene_multiview_num_views_get(const RenderData *rd);
SceneRenderView *BKE_scene_multiview_render_view_findindex(const RenderData *rd, int view_id);
const char *BKE_scene_multiview_render_view_name_get(const RenderData *rd, int view_id);
int BKE_scene_multiview_view_id_get(const RenderData *rd, const char *viewname);
void BKE_scene_multiview_filepath_get(const SceneRenderView *srv,
                                      const char *filepath,
                                      char *r_filepath);
/**
 * When multi-view is not used the `filepath` is as usual (e.g., `Image.jpg`).
 * When multi-view is on, even if only one view is enabled the view is incorporated
 * into the file name (e.g., `Image_L.jpg`). That allows for the user to re-render
 * individual views.
 */
void BKE_scene_multiview_view_filepath_get(const RenderData *rd,
                                           const char *filepath,
                                           const char *view,
                                           char *r_filepath);
const char *BKE_scene_multiview_view_suffix_get(const RenderData *rd, const char *viewname);
const char *BKE_scene_multiview_view_id_suffix_get(const RenderData *rd, int view_id);
void BKE_scene_multiview_view_prefix_get(Scene *scene,
                                         const char *filepath,
                                         char *r_prefix,
                                         const char **r_ext);
void BKE_scene_multiview_videos_dimensions_get(const RenderData *rd,
                                               const ImageFormatData *imf,
                                               size_t width,
                                               size_t height,
                                               size_t *r_width,
                                               size_t *r_height);
int BKE_scene_multiview_num_videos_get(const RenderData *rd, const ImageFormatData *imf);
/**
 * Calculate the final pixels-per-meter, from the scenes PPM & aspect data.
 */
void BKE_scene_ppm_get(const RenderData *rd, double r_ppm[2]);

/* depsgraph */
void BKE_scene_allocate_depsgraph_hash(Scene *scene);
void BKE_scene_ensure_depsgraph_hash(Scene *scene);
void BKE_scene_free_depsgraph_hash(Scene *scene);
void BKE_scene_free_view_layer_depsgraph(Scene *scene, ViewLayer *view_layer);

/**
 * \note Do not allocate new depsgraph.
 */
Depsgraph *BKE_scene_get_depsgraph(const Scene *scene, const ViewLayer *view_layer);
/**
 * \note Allocate new depsgraph if necessary.
 */
Depsgraph *BKE_scene_ensure_depsgraph(Main *bmain, Scene *scene, ViewLayer *view_layer);

GHash *BKE_scene_undo_depsgraphs_extract(Main *bmain);
void BKE_scene_undo_depsgraphs_restore(Main *bmain, GHash *depsgraph_extract);

void BKE_scene_transform_orientation_remove(Scene *scene, TransformOrientation *orientation);
TransformOrientation *BKE_scene_transform_orientation_find(const Scene *scene, int index);
/**
 * \return the index that \a orientation has within \a scene's transform-orientation list
 * or -1 if not found.
 */
int BKE_scene_transform_orientation_get_index(const Scene *scene,
                                              const TransformOrientation *orientation);
