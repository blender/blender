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

#ifndef __BKE_OBJECT_H__
#define __BKE_OBJECT_H__

/** \file
 * \ingroup bke
 * \brief General operations, lookup, etc. for blender objects.
 */
#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_compiler_attrs.h"

struct Base;
struct BoundBox;
struct Depsgraph;
struct GpencilModifierData;
struct HookGpencilModifierData;
struct HookModifierData;
struct ID;
struct Main;
struct Mesh;
struct ModifierData;
struct MovieClip;
struct Object;
struct RegionView3D;
struct RigidBodyWorld;
struct Scene;
struct ShaderFxData;
struct SoftBody;
struct View3D;
struct ViewLayer;

#include "DNA_object_enums.h"

void BKE_object_workob_clear(struct Object *workob);
void BKE_object_workob_calc_parent(struct Depsgraph *depsgraph,
                                   struct Scene *scene,
                                   struct Object *ob,
                                   struct Object *workob);

void BKE_object_transform_copy(struct Object *ob_tar, const struct Object *ob_src);
void BKE_object_copy_softbody(struct Object *ob_dst, const struct Object *ob_src, const int flag);
struct ParticleSystem *BKE_object_copy_particlesystem(struct ParticleSystem *psys, const int flag);
void BKE_object_copy_particlesystems(struct Object *ob_dst,
                                     const struct Object *ob_src,
                                     const int flag);
void BKE_object_free_particlesystems(struct Object *ob);
void BKE_object_free_softbody(struct Object *ob);
void BKE_object_free_curve_cache(struct Object *ob);

void BKE_object_free(struct Object *ob);
void BKE_object_free_derived_caches(struct Object *ob);
void BKE_object_free_derived_mesh_caches(struct Object *ob);
void BKE_object_free_caches(struct Object *object);

void BKE_object_modifier_hook_reset(struct Object *ob, struct HookModifierData *hmd);
void BKE_object_modifier_gpencil_hook_reset(struct Object *ob,
                                            struct HookGpencilModifierData *hmd);
bool BKE_object_modifier_gpencil_use_time(struct Object *ob, struct GpencilModifierData *md);

bool BKE_object_shaderfx_use_time(struct Object *ob, struct ShaderFxData *md);

bool BKE_object_support_modifier_type_check(const struct Object *ob, int modifier_type);

void BKE_object_link_modifiers(struct Scene *scene,
                               struct Object *ob_dst,
                               const struct Object *ob_src);
void BKE_object_free_modifiers(struct Object *ob, const int flag);
void BKE_object_free_shaderfx(struct Object *ob, const int flag);

void BKE_object_make_proxy(struct Main *bmain,
                           struct Object *ob,
                           struct Object *target,
                           struct Object *gob);
void BKE_object_copy_proxy_drivers(struct Object *ob, struct Object *target);

bool BKE_object_exists_check(struct Main *bmain, const struct Object *obtest);
bool BKE_object_is_in_editmode(const struct Object *ob);
bool BKE_object_is_in_editmode_vgroup(const struct Object *ob);
bool BKE_object_is_in_wpaint_select_vert(const struct Object *ob);
bool BKE_object_has_mode_data(const struct Object *ob, eObjectMode object_mode);
bool BKE_object_is_mode_compat(const struct Object *ob, eObjectMode object_mode);

bool BKE_object_data_is_in_editmode(const struct ID *id);

void BKE_object_update_select_id(struct Main *bmain);

typedef enum eObjectVisibilityResult {
  OB_VISIBLE_SELF = 1,
  OB_VISIBLE_PARTICLES = 2,
  OB_VISIBLE_INSTANCES = 4,
  OB_VISIBLE_ALL = (OB_VISIBLE_SELF | OB_VISIBLE_PARTICLES | OB_VISIBLE_INSTANCES),
} eObjectVisibilityResult;

int BKE_object_visibility(const struct Object *ob, const int dag_eval_mode);

void BKE_object_init(struct Object *ob);
struct Object *BKE_object_add_only_object(struct Main *bmain, int type, const char *name)
    ATTR_NONNULL(1) ATTR_RETURNS_NONNULL;
struct Object *BKE_object_add(struct Main *bmain,
                              struct Scene *scene,
                              struct ViewLayer *view_layer,
                              int type,
                              const char *name) ATTR_NONNULL(1, 2, 3) ATTR_RETURNS_NONNULL;
struct Object *BKE_object_add_from(struct Main *bmain,
                                   struct Scene *scene,
                                   struct ViewLayer *view_layer,
                                   int type,
                                   const char *name,
                                   struct Object *ob_src)
    ATTR_NONNULL(1, 2, 3, 6) ATTR_RETURNS_NONNULL;
struct Object *BKE_object_add_for_data(struct Main *bmain,
                                       struct ViewLayer *view_layer,
                                       int type,
                                       const char *name,
                                       struct ID *data,
                                       bool do_id_user) ATTR_RETURNS_NONNULL;
void *BKE_object_obdata_add_from_type(struct Main *bmain, int type, const char *name)
    ATTR_NONNULL(1);

void BKE_object_copy_data(struct Main *bmain,
                          struct Object *ob_dst,
                          const struct Object *ob_src,
                          const int flag);
struct Object *BKE_object_copy(struct Main *bmain, const struct Object *ob);
void BKE_object_make_local(struct Main *bmain, struct Object *ob, const bool lib_local);
void BKE_object_make_local_ex(struct Main *bmain,
                              struct Object *ob,
                              const bool lib_local,
                              const bool clear_proxy);
bool BKE_object_is_libdata(const struct Object *ob);
bool BKE_object_obdata_is_libdata(const struct Object *ob);

struct Object *BKE_object_duplicate(struct Main *bmain,
                                    const struct Object *ob,
                                    const int dupflag);

void BKE_object_obdata_size_init(struct Object *ob, const float scale);

void BKE_object_scale_to_mat3(struct Object *ob, float mat[3][3]);
void BKE_object_rot_to_mat3(const struct Object *ob, float mat[3][3], bool use_drot);
void BKE_object_mat3_to_rot(struct Object *ob, float mat[3][3], bool use_compat);
void BKE_object_to_mat3(struct Object *ob, float mat[3][3]);
void BKE_object_to_mat4(struct Object *ob, float mat[4][4]);
void BKE_object_apply_mat4(struct Object *ob,
                           float mat[4][4],
                           const bool use_compat,
                           const bool use_parent);
void BKE_object_apply_mat4_ex(struct Object *ob,
                              float mat[4][4],
                              struct Object *parent,
                              float parentinv[4][4],
                              const bool use_compat);
void BKE_object_matrix_local_get(struct Object *ob, float mat[4][4]);

bool BKE_object_pose_context_check(const struct Object *ob);
struct Object *BKE_object_pose_armature_get(struct Object *ob);
struct Object *BKE_object_pose_armature_get_visible(struct Object *ob,
                                                    struct ViewLayer *view_layer,
                                                    struct View3D *v3d);

struct Object **BKE_object_pose_array_get_ex(struct ViewLayer *view_layer,
                                             struct View3D *v3d,
                                             unsigned int *r_objects_len,
                                             bool unique);
struct Object **BKE_object_pose_array_get_unique(struct ViewLayer *view_layer,
                                                 struct View3D *v3d,
                                                 unsigned int *r_objects_len);
struct Object **BKE_object_pose_array_get(struct ViewLayer *view_layer,
                                          struct View3D *v3d,
                                          unsigned int *r_objects_len);

struct Base **BKE_object_pose_base_array_get_ex(struct ViewLayer *view_layer,
                                                struct View3D *v3d,
                                                unsigned int *r_bases_len,
                                                bool unique);
struct Base **BKE_object_pose_base_array_get_unique(struct ViewLayer *view_layer,
                                                    struct View3D *v3d,
                                                    unsigned int *r_bases_len);
struct Base **BKE_object_pose_base_array_get(struct ViewLayer *view_layer,
                                             struct View3D *v3d,
                                             unsigned int *r_bases_len);

void BKE_object_get_parent_matrix(struct Object *ob, struct Object *par, float parentmat[4][4]);
void BKE_object_where_is_calc(struct Depsgraph *depsgraph, struct Scene *scene, struct Object *ob);
void BKE_object_where_is_calc_ex(struct Depsgraph *depsgraph,
                                 struct Scene *scene,
                                 struct RigidBodyWorld *rbw,
                                 struct Object *ob,
                                 float r_originmat[3][3]);
void BKE_object_where_is_calc_time(struct Depsgraph *depsgraph,
                                   struct Scene *scene,
                                   struct Object *ob,
                                   float ctime);
void BKE_object_where_is_calc_mat4(struct Object *ob, float obmat[4][4]);

/* possibly belong in own moduke? */
struct BoundBox *BKE_boundbox_alloc_unit(void);
void BKE_boundbox_init_from_minmax(struct BoundBox *bb, const float min[3], const float max[3]);
void BKE_boundbox_calc_center_aabb(const struct BoundBox *bb, float r_cent[3]);
void BKE_boundbox_calc_size_aabb(const struct BoundBox *bb, float r_size[3]);
void BKE_boundbox_minmax(const struct BoundBox *bb,
                         float obmat[4][4],
                         float r_min[3],
                         float r_max[3]);

struct BoundBox *BKE_object_boundbox_get(struct Object *ob);
void BKE_object_dimensions_get(struct Object *ob, float vec[3]);
void BKE_object_dimensions_set(struct Object *ob, const float value[3], int axis_mask);
void BKE_object_empty_draw_type_set(struct Object *ob, const int value);
void BKE_object_boundbox_flag(struct Object *ob, int flag, const bool set);
void BKE_object_boundbox_calc_from_mesh(struct Object *ob, struct Mesh *me_eval);
void BKE_object_minmax(struct Object *ob, float r_min[3], float r_max[3], const bool use_hidden);
bool BKE_object_minmax_dupli(struct Depsgraph *depsgraph,
                             struct Scene *scene,
                             struct Object *ob,
                             float r_min[3],
                             float r_max[3],
                             const bool use_hidden);

/* sometimes min-max isn't enough, we need to loop over each point */
void BKE_object_foreach_display_point(struct Object *ob,
                                      float obmat[4][4],
                                      void (*func_cb)(const float[3], void *),
                                      void *user_data);
void BKE_scene_foreach_display_point(struct Depsgraph *depsgraph,
                                     void (*func_cb)(const float[3], void *),
                                     void *user_data);

bool BKE_object_parent_loop_check(const struct Object *parent, const struct Object *ob);

void *BKE_object_tfm_backup(struct Object *ob);
void BKE_object_tfm_restore(struct Object *ob, void *obtfm_pt);

typedef struct ObjectTfmProtectedChannels {
  float loc[3], dloc[3];
  float scale[3], dscale[3];
  float rot[3], drot[3];
  float quat[4], dquat[4];
  float rotAxis[3], drotAxis[3];
  float rotAngle, drotAngle;
} ObjectTfmProtectedChannels;

void BKE_object_tfm_protected_backup(const struct Object *ob, ObjectTfmProtectedChannels *obtfm);

void BKE_object_tfm_protected_restore(struct Object *ob,
                                      const ObjectTfmProtectedChannels *obtfm,
                                      const short protectflag);

void BKE_object_tfm_copy(struct Object *object_dst, const struct Object *object_src);

void BKE_object_eval_reset(struct Object *ob_eval);

/* Dependency graph evaluation callbacks. */
void BKE_object_eval_local_transform(struct Depsgraph *depsgraph, struct Object *ob);
void BKE_object_eval_parent(struct Depsgraph *depsgraph, struct Object *ob);
void BKE_object_eval_constraints(struct Depsgraph *depsgraph,
                                 struct Scene *scene,
                                 struct Object *ob);
void BKE_object_eval_transform_final(struct Depsgraph *depsgraph, struct Object *ob);

bool BKE_object_eval_proxy_copy(struct Depsgraph *depsgraph, struct Object *object);
void BKE_object_eval_uber_transform(struct Depsgraph *depsgraph, struct Object *ob);
void BKE_object_eval_uber_data(struct Depsgraph *depsgraph,
                               struct Scene *scene,
                               struct Object *ob);

void BKE_object_eval_boundbox(struct Depsgraph *depsgraph, struct Object *object);
void BKE_object_synchronize_to_original(struct Depsgraph *depsgraph, struct Object *object);

void BKE_object_eval_ptcache_reset(struct Depsgraph *depsgraph,
                                   struct Scene *scene,
                                   struct Object *object);

void BKE_object_eval_transform_all(struct Depsgraph *depsgraph,
                                   struct Scene *scene,
                                   struct Object *object);

void BKE_object_eval_update_shading(struct Depsgraph *depsgraph, struct Object *object);
void BKE_object_data_select_update(struct Depsgraph *depsgraph, struct ID *object_data);

void BKE_object_eval_eval_base_flags(struct Depsgraph *depsgraph,
                                     struct Scene *scene,
                                     const int view_layer_index,
                                     struct Object *object,
                                     int base_index,
                                     const bool is_from_set);

void BKE_object_handle_data_update(struct Depsgraph *depsgraph,
                                   struct Scene *scene,
                                   struct Object *ob);
void BKE_object_handle_update(struct Depsgraph *depsgraph, struct Scene *scene, struct Object *ob);
void BKE_object_handle_update_ex(struct Depsgraph *depsgraph,
                                 struct Scene *scene,
                                 struct Object *ob,
                                 struct RigidBodyWorld *rbw,
                                 const bool do_proxy_update);
void BKE_object_sculpt_modifiers_changed(struct Object *ob);

void BKE_object_sculpt_data_create(struct Object *ob);

int BKE_object_obdata_texspace_get(
    struct Object *ob, short **r_texflag, float **r_loc, float **r_size, float **r_rot);

struct Mesh *BKE_object_get_evaluated_mesh(const struct Depsgraph *depsgraph, struct Object *ob);
struct Mesh *BKE_object_get_final_mesh(struct Object *object);
struct Mesh *BKE_object_get_pre_modified_mesh(struct Object *object);
struct Mesh *BKE_object_get_original_mesh(struct Object *object);

int BKE_object_insert_ptcache(struct Object *ob);
void BKE_object_delete_ptcache(struct Object *ob, int index);
struct KeyBlock *BKE_object_shapekey_insert(struct Main *bmain,
                                            struct Object *ob,
                                            const char *name,
                                            const bool from_mix);
bool BKE_object_shapekey_remove(struct Main *bmain, struct Object *ob, struct KeyBlock *kb);
bool BKE_object_shapekey_free(struct Main *bmain, struct Object *ob);

bool BKE_object_flag_test_recursive(const struct Object *ob, short flag);

bool BKE_object_is_child_recursive(const struct Object *ob_parent, const struct Object *ob_child);
bool BKE_object_is_animated(struct Scene *scene, struct Object *ob);

/* return ModifierMode flag */
int BKE_object_is_modified(struct Scene *scene, struct Object *ob);
int BKE_object_is_deform_modified(struct Scene *scene, struct Object *ob);

int BKE_object_scenes_users_get(struct Main *bmain, struct Object *ob);

struct MovieClip *BKE_object_movieclip_get(struct Scene *scene,
                                           struct Object *ob,
                                           bool use_default);

void BKE_object_runtime_reset(struct Object *object);
void BKE_object_runtime_reset_on_copy(struct Object *object, const int flag);

void BKE_object_batch_cache_dirty_tag(struct Object *ob);

/* this function returns a superset of the scenes selection based on relationships */

typedef enum eObRelationTypes {
  OB_REL_NONE = 0,                      /* just the selection as is */
  OB_REL_PARENT = (1 << 0),             /* immediate parent */
  OB_REL_PARENT_RECURSIVE = (1 << 1),   /* parents up to root of selection tree*/
  OB_REL_CHILDREN = (1 << 2),           /* immediate children */
  OB_REL_CHILDREN_RECURSIVE = (1 << 3), /* All children */
  OB_REL_MOD_ARMATURE = (1 << 4),       /* Armatures related to the selected objects */
  OB_REL_SCENE_CAMERA = (1 << 5), /* you might want the scene camera too even if unselected? */
} eObRelationTypes;

typedef enum eObjectSet {
  OB_SET_SELECTED, /* Selected Objects */
  OB_SET_VISIBLE,  /* Visible Objects  */
  OB_SET_ALL,      /* All Objects      */
} eObjectSet;

struct LinkNode *BKE_object_relational_superset(struct ViewLayer *view_layer,
                                                eObjectSet objectSet,
                                                eObRelationTypes includeFilter);
struct LinkNode *BKE_object_groups(struct Main *bmain, struct Scene *scene, struct Object *ob);
void BKE_object_groups_clear(struct Main *bmain, struct Scene *scene, struct Object *object);

struct KDTree_3d *BKE_object_as_kdtree(struct Object *ob, int *r_tot);

bool BKE_object_modifier_use_time(struct Object *ob, struct ModifierData *md);

bool BKE_object_modifier_update_subframe(struct Depsgraph *depsgraph,
                                         struct Scene *scene,
                                         struct Object *ob,
                                         bool update_mesh,
                                         int parent_recursion,
                                         float frame,
                                         int type);

void BKE_object_type_set_empty_for_versioning(struct Object *ob);

bool BKE_object_empty_image_frame_is_visible_in_view3d(const struct Object *ob,
                                                       const struct RegionView3D *rv3d);
bool BKE_object_empty_image_data_is_visible_in_view3d(const struct Object *ob,
                                                      const struct RegionView3D *rv3d);

/* This is an utility function for Python's object.to_mesh() (the naming is not very clear though).
 * The result is owned by the object.
 *
 * The mesh will be freed when object is re-evaluated or is destroyed. It is possible to force to
 * clear memory sued by this mesh by calling BKE_object_to_mesh_clear().
 *
 * If preserve_all_data_layers is truth then the modifier stack is re-evaluated to ensure it
 * preserves all possible custom data layers.
 *
 * NOTE: Dependency graph argument is required when preserve_all_data_layers is truth, and is
 * ignored otherwise. */
struct Mesh *BKE_object_to_mesh(struct Depsgraph *depsgraph,
                                struct Object *object,
                                bool preserve_all_data_layers);

void BKE_object_to_mesh_clear(struct Object *object);

#ifdef __cplusplus
}
#endif

#endif
