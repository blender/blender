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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */
#ifndef __BKE_SCENE_H__
#define __BKE_SCENE_H__

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct AviCodecData;
struct Collection;
struct Depsgraph;
struct Main;
struct Object;
struct RenderData;
struct Scene;
struct TransformOrientation;
struct UnitSettings;
struct View3DCursor;
struct ViewLayer;
struct ViewRender;
struct WorkSpace;

typedef enum eSceneCopyMethod {
  SCE_COPY_NEW = 0,
  SCE_COPY_EMPTY = 1,
  SCE_COPY_LINK_COLLECTION = 2,
  SCE_COPY_FULL = 3,
} eSceneCopyMethod;

/* Use as the contents of a 'for' loop: for (SETLOOPER(...)) { ... */
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

struct Base *_setlooper_base_step(struct Scene **sce_iter,
                                  struct ViewLayer *view_layer,
                                  struct Base *base);

void free_avicodecdata(struct AviCodecData *acd);

void BKE_scene_free_ex(struct Scene *sce, const bool do_id_user);
void BKE_scene_free(struct Scene *sce);
void BKE_scene_init(struct Scene *sce);
struct Scene *BKE_scene_add(struct Main *bmain, const char *name);

void BKE_scene_remove_rigidbody_object(struct Main *bmain, struct Scene *scene, struct Object *ob);

bool BKE_scene_object_find(struct Scene *scene, struct Object *ob);
struct Object *BKE_scene_object_find_by_name(struct Scene *scene, const char *name);

/* Scene base iteration function.
 * Define struct here, so no need to bother with alloc/free it.
 */
typedef struct SceneBaseIter {
  struct ListBase *duplilist;
  struct DupliObject *dupob;
  float omat[4][4];
  struct Object *dupli_refob;
  int phase;
} SceneBaseIter;

int BKE_scene_base_iter_next(struct Depsgraph *depsgraph,
                             struct SceneBaseIter *iter,
                             struct Scene **scene,
                             int val,
                             struct Base **base,
                             struct Object **ob);

void BKE_scene_base_flag_to_objects(struct ViewLayer *view_layer);
void BKE_scene_object_base_flag_sync_from_base(struct Base *base);

void BKE_scene_set_background(struct Main *bmain, struct Scene *sce);
struct Scene *BKE_scene_set_name(struct Main *bmain, const char *name);

struct ToolSettings *BKE_toolsettings_copy(struct ToolSettings *toolsettings, const int flag);
void BKE_toolsettings_free(struct ToolSettings *toolsettings);

void BKE_scene_copy_data(struct Main *bmain,
                         struct Scene *sce_dst,
                         const struct Scene *sce_src,
                         const int flag);
struct Scene *BKE_scene_copy(struct Main *bmain, struct Scene *sce, int type);
void BKE_scene_groups_relink(struct Scene *sce);

void BKE_scene_make_local(struct Main *bmain, struct Scene *sce, const bool lib_local);

struct Scene *BKE_scene_find_from_collection(const struct Main *bmain,
                                             const struct Collection *collection);

#ifdef DURIAN_CAMERA_SWITCH
struct Object *BKE_scene_camera_switch_find(struct Scene *scene);  // DURIAN_CAMERA_SWITCH
#endif
int BKE_scene_camera_switch_update(struct Scene *scene);

char *BKE_scene_find_marker_name(struct Scene *scene, int frame);
char *BKE_scene_find_last_marker_name(struct Scene *scene, int frame);

int BKE_scene_frame_snap_by_seconds(struct Scene *scene, double interval_in_seconds, int cfra);

/* checks for cycle, returns 1 if it's all OK */
bool BKE_scene_validate_setscene(struct Main *bmain, struct Scene *sce);

float BKE_scene_frame_get(const struct Scene *scene);
float BKE_scene_frame_get_from_ctime(const struct Scene *scene, const float frame);
void BKE_scene_frame_set(struct Scene *scene, double cfra);

struct TransformOrientationSlot *BKE_scene_orientation_slot_get_from_flag(struct Scene *scene,
                                                                          int slot_index);
struct TransformOrientationSlot *BKE_scene_orientation_slot_get(struct Scene *scene, int flag);
void BKE_scene_orientation_slot_set_index(struct TransformOrientationSlot *orient_slot,
                                          int orientation);
int BKE_scene_orientation_slot_get_index(const struct TransformOrientationSlot *orient_slot);

/* **  Scene evaluation ** */

void BKE_scene_graph_update_tagged(struct Depsgraph *depsgraph, struct Main *bmain);

void BKE_scene_graph_update_for_newframe(struct Depsgraph *depsgraph, struct Main *bmain);

void BKE_scene_view_layer_graph_evaluated_ensure(struct Main *bmain,
                                                 struct Scene *scene,
                                                 struct ViewLayer *view_layer);

struct SceneRenderView *BKE_scene_add_render_view(struct Scene *sce, const char *name);
bool BKE_scene_remove_render_view(struct Scene *scene, struct SceneRenderView *srv);

/* render profile */
int get_render_subsurf_level(const struct RenderData *r, int level, bool for_render);
int get_render_child_particle_number(const struct RenderData *r, int num, bool for_render);

bool BKE_scene_use_shading_nodes_custom(struct Scene *scene);
bool BKE_scene_use_spherical_stereo(struct Scene *scene);

bool BKE_scene_uses_blender_eevee(const struct Scene *scene);
bool BKE_scene_uses_blender_workbench(const struct Scene *scene);
bool BKE_scene_uses_cycles(const struct Scene *scene);

void BKE_scene_disable_color_management(struct Scene *scene);
bool BKE_scene_check_color_management_enabled(const struct Scene *scene);
bool BKE_scene_check_rigidbody_active(const struct Scene *scene);

int BKE_scene_num_threads(const struct Scene *scene);
int BKE_render_num_threads(const struct RenderData *r);

int BKE_render_preview_pixel_size(const struct RenderData *r);

/**********************************/

double BKE_scene_unit_scale(const struct UnitSettings *unit, const int unit_type, double value);

/* multiview */
bool BKE_scene_multiview_is_stereo3d(const struct RenderData *rd);
bool BKE_scene_multiview_is_render_view_active(const struct RenderData *rd,
                                               const struct SceneRenderView *srv);
bool BKE_scene_multiview_is_render_view_first(const struct RenderData *rd, const char *viewname);
bool BKE_scene_multiview_is_render_view_last(const struct RenderData *rd, const char *viewname);
int BKE_scene_multiview_num_views_get(const struct RenderData *rd);
struct SceneRenderView *BKE_scene_multiview_render_view_findindex(const struct RenderData *rd,
                                                                  const int view_id);
const char *BKE_scene_multiview_render_view_name_get(const struct RenderData *rd,
                                                     const int view_id);
int BKE_scene_multiview_view_id_get(const struct RenderData *rd, const char *viewname);
void BKE_scene_multiview_filepath_get(struct SceneRenderView *srv,
                                      const char *filepath,
                                      char *r_filepath);
void BKE_scene_multiview_view_filepath_get(const struct RenderData *rd,
                                           const char *filepath,
                                           const char *view,
                                           char *r_filepath);
const char *BKE_scene_multiview_view_suffix_get(const struct RenderData *rd, const char *viewname);
const char *BKE_scene_multiview_view_id_suffix_get(const struct RenderData *rd, const int view_id);
void BKE_scene_multiview_view_prefix_get(struct Scene *scene,
                                         const char *name,
                                         char *rprefix,
                                         const char **rext);
void BKE_scene_multiview_videos_dimensions_get(const struct RenderData *rd,
                                               const size_t width,
                                               const size_t height,
                                               size_t *r_width,
                                               size_t *r_height);
int BKE_scene_multiview_num_videos_get(const struct RenderData *rd);

/* depsgraph */
void BKE_scene_allocate_depsgraph_hash(struct Scene *scene);
void BKE_scene_ensure_depsgraph_hash(struct Scene *scene);
void BKE_scene_free_depsgraph_hash(struct Scene *scene);

struct Depsgraph *BKE_scene_get_depsgraph(struct Scene *scene,
                                          struct ViewLayer *view_layer,
                                          bool allocate);

void BKE_scene_transform_orientation_remove(struct Scene *scene,
                                            struct TransformOrientation *orientation);
struct TransformOrientation *BKE_scene_transform_orientation_find(const struct Scene *scene,
                                                                  const int index);
int BKE_scene_transform_orientation_get_index(const struct Scene *scene,
                                              const struct TransformOrientation *orientation);

void BKE_scene_cursor_rot_to_mat3(const struct View3DCursor *cursor, float mat[3][3]);
void BKE_scene_cursor_mat3_to_rot(struct View3DCursor *cursor,
                                  const float mat[3][3],
                                  bool use_compat);

void BKE_scene_cursor_rot_to_quat(const struct View3DCursor *cursor, float quat[4]);
void BKE_scene_cursor_quat_to_rot(struct View3DCursor *cursor,
                                  const float quat[4],
                                  bool use_compat);

/* Evaluation. */

/* Evaluate parts of sequences which needs to be done as a part of a dependency graph evaluation.
 * This does NOT include actual rendering of the strips, but rather makes them up-to-date for
 * animation playback and makes them ready for the sequencer's rendering pipeline to render them.
 */
void BKE_scene_eval_sequencer_sequences(struct Depsgraph *depsgraph, struct Scene *scene);

#ifdef __cplusplus
}
#endif

#endif
