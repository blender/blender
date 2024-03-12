/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 * \brief General operations, lookup, etc. for blender objects.
 */

#include <optional>

#include "BLI_bounds_types.hh"
#include "BLI_compiler_attrs.h"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_sys_types.h"
#include "BLI_vector.hh"

#include "DNA_object_enums.h"
#include "DNA_userdef_enums.h"

struct Base;
struct BoundBox;
struct Curve;
struct Depsgraph;
struct GpencilModifierData;
struct HookGpencilModifierData;
struct HookModifierData;
struct ID;
struct KDTree_3d;
struct KeyBlock;
struct Lattice;
struct LinkNode;
struct Main;
struct Mesh;
struct ModifierData;
struct MovieClip;
struct Object;
struct PartEff;
struct ParticleSystem;
struct RegionView3D;
struct RigidBodyWorld;
struct Scene;
struct SubsurfModifierData;
struct View3D;
struct ViewLayer;

void BKE_object_workob_clear(Object *workob);
/**
 * For calculation of the inverse parent transform, only used for editor.
 *
 * It assumes the object parent is already in the depsgraph.
 * Otherwise, after changing ob->parent you need to call:
 * - #DEG_relations_tag_update(bmain);
 * - #BKE_scene_graph_update_tagged(depsgraph, bmain);
 *
 * \return calculated object_to_world.
 */
blender::float4x4 BKE_object_calc_parent(Depsgraph *depsgraph, Scene *scene, Object *ob);

void BKE_object_transform_copy(Object *ob_tar, const Object *ob_src);
void BKE_object_copy_softbody(Object *ob_dst, const Object *ob_src, int flag);
ParticleSystem *BKE_object_copy_particlesystem(ParticleSystem *psys, int flag);
void BKE_object_copy_particlesystems(Object *ob_dst, const Object *ob_src, int flag);
void BKE_object_free_particlesystems(Object *ob);
void BKE_object_free_softbody(Object *ob);
void BKE_object_free_curve_cache(Object *ob);

/**
 * Free data derived from mesh, called when mesh changes or is freed.
 */
void BKE_object_free_derived_caches(Object *ob);
void BKE_object_free_caches(Object *object);

void BKE_object_modifier_hook_reset(Object *ob, HookModifierData *hmd);
void BKE_object_modifier_gpencil_hook_reset(Object *ob, HookGpencilModifierData *hmd);

/**
 * \return True if the object's type supports regular modifiers (not grease pencil modifiers).
 */
bool BKE_object_supports_modifiers(const Object *ob);
bool BKE_object_support_modifier_type_check(const Object *ob, int modifier_type);

/* Active modifier. */

/**
 * Set the object's active modifier.
 *
 * \param md: If nullptr, only clear the active modifier, otherwise
 * it must be in the #Object.modifiers list.
 */
void BKE_object_modifier_set_active(Object *ob, ModifierData *md);
ModifierData *BKE_object_active_modifier(const Object *ob);

/**
 * Copy a single modifier.
 *
 * \note *Do not* use this function to copy a whole modifier stack (see note below too). Use
 * `BKE_object_modifier_stack_copy` instead.
 *
 * \note Complex modifiers relaying on other data (like e.g. dynamic paint or fluid using particle
 * systems) are not always 100% 'correctly' copied here, since we have to use heuristics to decide
 * which particle system to use or add in `ob_dst`, and it's placement in the stack, etc. If used
 * more than once, this function should preferably be called in stack order.
 */
bool BKE_object_copy_modifier(
    Main *bmain, Scene *scene, Object *ob_dst, const Object *ob_src, ModifierData *md);
/**
 * Copy a single GPencil modifier.
 *
 * \note *Do not* use this function to copy a whole modifier stack. Use
 * `BKE_object_modifier_stack_copy` instead.
 */
bool BKE_object_copy_gpencil_modifier(Object *ob_dst, GpencilModifierData *gmd_src);
/**
 * Copy the whole stack of modifiers from one object into another.
 *
 * \warning *Does not* clear modifier stack and related data (particle systems, soft-body,
 * etc.) in `ob_dst`, if needed calling code must do it. The caller is also responsible for
 * ensuring the modifier identifiers are unique.
 *
 * \param do_copy_all: If true, even modifiers that should not support copying (like Hook one)
 * will be duplicated.
 */
bool BKE_object_modifier_stack_copy(Object *ob_dst,
                                    const Object *ob_src,
                                    bool do_copy_all,
                                    int flag_subdata);
void BKE_object_link_modifiers(Object *ob_dst, const Object *ob_src);
void BKE_object_free_modifiers(Object *ob, int flag);
void BKE_object_free_shaderfx(Object *ob, int flag);

bool BKE_object_exists_check(Main *bmain, const Object *obtest);
/**
 * Actual check for internal data, not context or flags.
 */
bool BKE_object_is_in_editmode(const Object *ob);
bool BKE_object_is_in_editmode_vgroup(const Object *ob);
bool BKE_object_is_in_wpaint_select_vert(const Object *ob);
bool BKE_object_has_mode_data(const Object *ob, eObjectMode object_mode);
bool BKE_object_is_mode_compat(const Object *ob, eObjectMode object_mode);

bool BKE_object_data_is_in_editmode(const Object *ob, const ID *id);

char *BKE_object_data_editmode_flush_ptr_get(ID *id);

/**
 * Updates select_id of all objects in the given \a bmain.
 */
void BKE_object_update_select_id(Main *bmain);

enum eObjectVisibilityResult {
  OB_VISIBLE_SELF = 1,
  OB_VISIBLE_PARTICLES = 2,
  OB_VISIBLE_INSTANCES = 4,
  OB_VISIBLE_ALL = (OB_VISIBLE_SELF | OB_VISIBLE_PARTICLES | OB_VISIBLE_INSTANCES),
};

/**
 * Return which parts of the object are visible, as evaluated by depsgraph.
 */
int BKE_object_visibility(const Object *ob, int dag_eval_mode);

/**
 * More general add: creates minimum required data, but without vertices etc.
 *
 * \param bmain: The main to add the object to. May be null for #LIB_ID_CREATE_NO_MAIN behavior.
 */
Object *BKE_object_add_only_object(Main *bmain, int type, const char *name) ATTR_RETURNS_NONNULL;
/**
 * General add: to scene, with layer from area and default name.
 *
 * Object is added to the active #Collection.
 * If there is no linked collection to the active #ViewLayer we create a new one.
 *
 * \note Creates minimum required data, but without vertices etc.
 */
Object *BKE_object_add(Main *bmain,
                       Scene *scene,
                       ViewLayer *view_layer,
                       int type,
                       const char *name) ATTR_NONNULL(1, 2, 3) ATTR_RETURNS_NONNULL;
/**
 * Add a new object, using another one as a reference
 *
 * \param ob_src: object to use to determine the collections of the new object.
 */
Object *BKE_object_add_from(
    Main *bmain, Scene *scene, ViewLayer *view_layer, int type, const char *name, Object *ob_src)
    ATTR_NONNULL(1, 2, 3, 6) ATTR_RETURNS_NONNULL;
/**
 * Add a new object, but assign the given data-block as the `ob->data`
 * for the newly created object.
 *
 * \param data: The data-block to assign as `ob->data` for the new object.
 * This is assumed to be of the correct type.
 * \param do_id_user: If true, #id_us_plus() will be called on data when
 * assigning it to the object.
 */
Object *BKE_object_add_for_data(Main *bmain,
                                const Scene *scene,
                                ViewLayer *view_layer,
                                int type,
                                const char *name,
                                ID *data,
                                bool do_id_user) ATTR_RETURNS_NONNULL;
void *BKE_object_obdata_add_from_type(Main *bmain, int type, const char *name) ATTR_NONNULL(1);
/**
 * Return -1 on failure.
 */
int BKE_object_obdata_to_type(const ID *id) ATTR_NONNULL(1);

/**
 * Returns true if the Object is from an external blend file (libdata).
 */
bool BKE_object_is_libdata(const Object *ob);
/**
 * Returns true if the Object data is from an external blend file (libdata).
 */
bool BKE_object_obdata_is_libdata(const Object *ob);

/**
 * Perform deep-copy of object and its 'children' data-blocks (obdata, materials, actions, etc.).
 *
 * \param dupflag: Controls which sub-data are also duplicated
 * (see #eDupli_ID_Flags in DNA_userdef_types.h).
 *
 * \note This function does not do any remapping to new IDs, caller must do it
 * (\a #BKE_libblock_relink_to_newid()).
 * \note Caller MUST free \a newid pointers itself (#BKE_main_id_newptr_and_tag_clear()) and call
 * updates of DEG too (#DAG_relations_tag_update()).
 */
Object *BKE_object_duplicate(Main *bmain,
                             Object *ob,
                             eDupli_ID_Flags dupflag,
                             uint duplicate_options);

/**
 * Use with newly created objects to set their size (used to apply scene-scale).
 */
void BKE_object_obdata_size_init(Object *ob, float size);

void BKE_object_scale_to_mat3(Object *ob, float r_mat[3][3]);
void BKE_object_rot_to_mat3(const Object *ob, float r_mat[3][3], bool use_drot);
void BKE_object_mat3_to_rot(Object *ob, float r_mat[3][3], bool use_compat);
void BKE_object_to_mat3(Object *ob, float r_mat[3][3]);
void BKE_object_to_mat4(Object *ob, float r_mat[4][4]);
/**
 * Applies the global transformation \a mat to the \a ob using a relative parent space if
 * supplied.
 *
 * \param mat: the global transformation mat that the object should be set object to.
 * \param parent: the parent space in which this object will be set relative to
 * (should probably always be parent_eval).
 * \param use_compat: true to ensure that rotations are set using the
 * min difference between the old and new orientation.
 */
void BKE_object_apply_mat4_ex(Object *ob,
                              const float mat[4][4],
                              Object *parent,
                              const float parentinv[4][4],
                              bool use_compat);
/** See #BKE_object_apply_mat4_ex */
void BKE_object_apply_mat4(Object *ob, const float mat[4][4], bool use_compat, bool use_parent);

/**
 * Use parent's world location and rotation as the child's origin. The parent inverse will
 * become identity when the parent has no shearing. Otherwise, it is non-identity and contains the
 * object's local matrix data that cannot be decomposed into location, rotation and scale.
 *
 * Assumes the object's world matrix has no shear.
 * Assumes parent exists.
 */
void BKE_object_apply_parent_inverse(Object *ob);

void BKE_object_matrix_local_get(Object *ob, float r_mat[4][4]);

bool BKE_object_pose_context_check(const Object *ob);

Object *BKE_object_pose_armature_get(Object *ob);
/**
 * A version of #BKE_object_pose_armature_get with an additional check.
 * When `ob` isn't an armature: only return the referenced pose object
 * when the active object is in weight paint mode.
 *
 * \note Some callers need to check that pose bones are selectable
 * which isn't the case when the object using the armature isn't in weight-paint mode.
 */
Object *BKE_object_pose_armature_get_with_wpaint_check(Object *ob);
Object *BKE_object_pose_armature_get_visible(Object *ob,
                                             const Scene *scene,
                                             ViewLayer *view_layer,
                                             View3D *v3d);

/**
 * Access pose array with special check to get pose object when in weight paint mode.
 */
blender::Vector<Object *> BKE_object_pose_array_get_ex(const Scene *scene,
                                                       ViewLayer *view_layer,
                                                       View3D *v3d,
                                                       bool unique);
blender::Vector<Object *> BKE_object_pose_array_get_unique(const Scene *scene,
                                                           ViewLayer *view_layer,
                                                           View3D *v3d);
blender::Vector<Object *> BKE_object_pose_array_get(const Scene *scene,
                                                    ViewLayer *view_layer,
                                                    View3D *v3d);

blender::Vector<Base *> BKE_object_pose_base_array_get_ex(const Scene *scene,
                                                          ViewLayer *view_layer,
                                                          View3D *v3d,
                                                          bool unique);
blender::Vector<Base *> BKE_object_pose_base_array_get_unique(const Scene *scene,
                                                              ViewLayer *view_layer,
                                                              View3D *v3d);
blender::Vector<Base *> BKE_object_pose_base_array_get(const Scene *scene,
                                                       ViewLayer *view_layer,
                                                       View3D *v3d);

void BKE_object_get_parent_matrix(Object *ob, Object *par, float r_parentmat[4][4]);

/**
 * Compute object world transform and store it in `ob->object_to_world().ptr()`.
 */
void BKE_object_where_is_calc(Depsgraph *depsgraph, Scene *scene, Object *ob);
void BKE_object_where_is_calc_ex(
    Depsgraph *depsgraph, Scene *scene, RigidBodyWorld *rbw, Object *ob, float r_originmat[3][3]);
void BKE_object_where_is_calc_time(Depsgraph *depsgraph, Scene *scene, Object *ob, float ctime);
/**
 * Calculate object transformation matrix without recalculating dependencies and
 * constraints -- assume dependencies are already solved by depsgraph.
 * No changes to object and its parent would be done.
 * Used for bundles orientation in 3d space relative to parented blender camera.
 */
void BKE_object_where_is_calc_mat4(Object *ob, float r_obmat[4][4]);

/* Possibly belong in its own module? */

void BKE_boundbox_init_from_minmax(BoundBox *bb, const float min[3], const float max[3]);
void BKE_boundbox_minmax(const BoundBox *bb,
                         const float obmat[4][4],
                         float r_min[3],
                         float r_max[3]);

/**
 * Retrieve the bounds of the object's geometry, in the local space of the object
 * (not accounting for the object's transform). For evaluated objects, this includes
 * the evaluated geometry (not just #Object.data).
 */
std::optional<blender::Bounds<blender::float3>> BKE_object_boundbox_get(const Object *ob);
void BKE_object_dimensions_get(Object *ob, float r_vec[3]);

/**
 * Retrieve the bounds of the evaluated object's geometry, stored on the original object as part of
 * the latest dependency graph evaluation, or fall back to the current bounds of the object if no
 * such cache exists. For evaluated objects this indirection is unnecessary, so
 * #BKE_object_boundbox_get should be used instead.
 */
std::optional<blender::Bounds<blender::float3>> BKE_object_boundbox_eval_cached_get(
    const Object *ob);
/** Similar to #BKE_object_boundbox_eval_cached_get but gives the size of the bounds instead. */
void BKE_object_dimensions_eval_cached_get(Object *ob, float r_vec[3]);

/**
 * The original scale and object matrix can be passed in so any difference
 * of the objects matrix and the final matrix can be accounted for,
 * typically this caused by parenting, constraints or delta-scale.
 *
 * Re-using these values from the object causes a feedback loop
 * when multiple values are modified at once in some situations. see: #69536.
 */
void BKE_object_dimensions_set_ex(Object *ob,
                                  const float value[3],
                                  int axis_mask,
                                  const float ob_scale_orig[3],
                                  const float ob_obmat_orig[4][4]);
void BKE_object_dimensions_set(Object *ob, const float value[3], int axis_mask);

void BKE_object_empty_draw_type_set(Object *ob, int value);

std::optional<blender::Bounds<blender::float3>> BKE_object_evaluated_geometry_bounds(
    const Object *ob);
void BKE_object_minmax(Object *ob, float r_min[3], float r_max[3]);
bool BKE_object_minmax_dupli(Depsgraph *depsgraph,
                             Scene *scene,
                             Object *ob,
                             float r_min[3],
                             float r_max[3],
                             bool use_hidden);
/**
 * Calculate visual bounds from an empty objects draw-type.
 *
 * \note This is not part of the calculation used by #BKE_object_boundbox_get
 * as these bounds represent the extents of visual guides (use for viewport culling for e.g.)
 */
bool BKE_object_minmax_empty_drawtype(const Object *ob, float r_min[3], float r_max[3]);

/**
 * Sometimes min-max isn't enough, we need to loop over each point.
 */
void BKE_object_foreach_display_point(Object *ob,
                                      const float obmat[4][4],
                                      void (*func_cb)(const float[3], void *),
                                      void *user_data);
void BKE_scene_foreach_display_point(Depsgraph *depsgraph,
                                     void (*func_cb)(const float[3], void *),
                                     void *user_data);

bool BKE_object_parent_loop_check(const Object *parent, const Object *ob);

void *BKE_object_tfm_backup(Object *ob);
void BKE_object_tfm_restore(Object *ob, void *obtfm_pt);

struct ObjectTfmProtectedChannels {
  float loc[3], dloc[3];
  float scale[3], dscale[3];
  float rot[3], drot[3];
  float quat[4], dquat[4];
  float rotAxis[3], drotAxis[3];
  float rotAngle, drotAngle;
};

void BKE_object_tfm_protected_backup(const Object *ob, ObjectTfmProtectedChannels *obtfm);

void BKE_object_tfm_protected_restore(Object *ob,
                                      const ObjectTfmProtectedChannels *obtfm,
                                      short protectflag);

void BKE_object_tfm_copy(Object *object_dst, const Object *object_src);

/**
 * Restore the object->data to a non-modifier evaluated state.
 *
 * Some changes done directly in evaluated object require them to be reset
 * before being re-evaluated.
 * For example, we need to call this before #BKE_mesh_new_from_object(),
 * in case we removed/added modifiers in the evaluated object.
 */
void BKE_object_eval_reset(Object *ob_eval);

/* Dependency graph evaluation callbacks. */

void BKE_object_eval_local_transform(Depsgraph *depsgraph, Object *ob);
void BKE_object_eval_parent(Depsgraph *depsgraph, Object *ob);
void BKE_object_eval_constraints(Depsgraph *depsgraph, Scene *scene, Object *ob);
void BKE_object_eval_transform_final(Depsgraph *depsgraph, Object *ob);

void BKE_object_eval_uber_transform(Depsgraph *depsgraph, Object *object);
void BKE_object_eval_uber_data(Depsgraph *depsgraph, Scene *scene, Object *ob);

void BKE_object_eval_shading(Depsgraph *depsgraph, Object *ob);

void BKE_object_eval_light_linking(Depsgraph *depsgraph, Object *object);

/**
 * Assign #Object.data after modifier stack evaluation.
 */
void BKE_object_eval_assign_data(Object *object, ID *data, bool is_owned);

void BKE_object_sync_to_original(Depsgraph *depsgraph, Object *object);

void BKE_object_eval_ptcache_reset(Depsgraph *depsgraph, Scene *scene, Object *object);

void BKE_object_eval_transform_all(Depsgraph *depsgraph, Scene *scene, Object *object);

void BKE_object_data_select_update(Depsgraph *depsgraph, ID *object_data);
void BKE_object_select_update(Depsgraph *depsgraph, Object *object);

void BKE_object_eval_eval_base_flags(Depsgraph *depsgraph,
                                     Scene *scene,
                                     int view_layer_index,
                                     Object *object,
                                     int base_index,
                                     bool is_from_set);

void BKE_object_handle_data_update(Depsgraph *depsgraph, Scene *scene, Object *ob);
/**
 * \warning "scene" here may not be the scene object actually resides in.
 * When dealing with background-sets, "scene" is actually the active scene.
 * e.g. "scene" <-- set 1 <-- set 2 ("ob" lives here) <-- set 3 <-- ... <-- set n
 * rigid bodies depend on their world so use #BKE_object_handle_update_ex()
 * to also pass along the current rigid body world.
 */
void BKE_object_handle_update(Depsgraph *depsgraph, Scene *scene, Object *ob);
/**
 * The main object update call, for object matrix, constraints, keys and modifiers.
 * Requires flags to be set!
 *
 * Ideally we shouldn't have to pass the rigid body world,
 * but need bigger restructuring to avoid id.
 */
void BKE_object_handle_update_ex(Depsgraph *depsgraph,
                                 Scene *scene,
                                 Object *ob,
                                 RigidBodyWorld *rbw);

void BKE_object_sculpt_data_create(Object *ob);

bool BKE_object_obdata_texspace_get(Object *ob,
                                    char **r_texspace_flag,
                                    float **r_texspace_location,
                                    float **r_texspace_size);

Mesh *BKE_object_get_evaluated_mesh_no_subsurf(const Object *object);
/** Get evaluated mesh for given object. */
Mesh *BKE_object_get_evaluated_mesh(const Object *object);
/**
 * Get mesh which is not affected by modifiers:
 * - For original objects it will be same as `object->data`, and it is a mesh
 *   which is in the corresponding #Main.
 * - For copied-on-write objects it will give pointer to a copied-on-write
 *   mesh which corresponds to original object's mesh.
 */
Mesh *BKE_object_get_pre_modified_mesh(const Object *object);
/**
 * Get a mesh which corresponds to the very original mesh from #Main.
 * - For original objects it will be object->data.
 * - For evaluated objects it will be same mesh as corresponding original
 *   object uses as data.
 */
Mesh *BKE_object_get_original_mesh(const Object *object);

Mesh *BKE_object_get_editmesh_eval_final(const Object *object);
Mesh *BKE_object_get_editmesh_eval_cage(const Object *object);

/* Lattice accessors.
 * These functions return either the regular lattice, or the edit-mode lattice,
 * whichever is currently in use. */

Lattice *BKE_object_get_lattice(const Object *object);
Lattice *BKE_object_get_evaluated_lattice(const Object *object);

int BKE_object_insert_ptcache(Object *ob);
void BKE_object_delete_ptcache(Object *ob, int index);
KeyBlock *BKE_object_shapekey_insert(Main *bmain, Object *ob, const char *name, bool from_mix);
bool BKE_object_shapekey_remove(Main *bmain, Object *ob, KeyBlock *kb);
bool BKE_object_shapekey_free(Main *bmain, Object *ob);

bool BKE_object_flag_test_recursive(const Object *ob, short flag);

bool BKE_object_is_child_recursive(const Object *ob_parent, const Object *ob_child);

/**
 * Most important if this is modified it should _always_ return true, in certain
 * cases false positives are hard to avoid (shape keys for example).
 *
 * \return #ModifierMode flag.
 */
int BKE_object_is_modified(Scene *scene, Object *ob);
/**
 * Test if object is affected by deforming modifiers (for motion blur). again
 * most important is to avoid false positives, this is to skip computations
 * and we can still if there was actual deformation afterwards.
 */
int BKE_object_is_deform_modified(Scene *scene, Object *ob);

/**
 * Check of objects moves in time.
 *
 * \note This function is currently optimized for usage in combination
 * with modifier deformation checks (#ModifierTypeType::OnlyDeform),
 * so modifiers can quickly check if their target objects moves
 * (causing deformation motion blur) or not.
 *
 * This makes it possible to give some degree of false-positives here,
 * but it's currently an acceptable tradeoff between complexity and check
 * speed. In combination with checks of modifier stack and real life usage
 * percentage of false-positives shouldn't be that high.
 *
 * \note This function does not consider physics systems.
 */
bool BKE_object_moves_in_time(const Object *object, bool recurse_parent);

/** Return the number of scenes using (instantiating) that object in their collections. */
int BKE_object_scenes_users_get(Main *bmain, Object *ob);

MovieClip *BKE_object_movieclip_get(Scene *scene, Object *ob, bool use_default);

void BKE_object_runtime_reset(Object *object);
/**
 * Reset all pointers which we don't want to be shared when copying the object.
 */
void BKE_object_runtime_reset_on_copy(Object *object, int flag);
/**
 * The function frees memory used by the runtime data, but not the runtime field itself.
 *
 * All runtime data is cleared to ensure it's not used again,
 * in keeping with other `_free_data(..)` functions.
 */
void BKE_object_runtime_free_data(Object *object);

void BKE_object_batch_cache_dirty_tag(Object *ob);

/* this function returns a superset of the scenes selection based on relationships */

enum eObRelationTypes {
  OB_REL_NONE = 0,                      /* Just the selection as is. */
  OB_REL_PARENT = (1 << 0),             /* Immediate parent. */
  OB_REL_PARENT_RECURSIVE = (1 << 1),   /* Parents up to root of selection tree. */
  OB_REL_CHILDREN = (1 << 2),           /* Immediate children. */
  OB_REL_CHILDREN_RECURSIVE = (1 << 3), /* All children. */
  OB_REL_MOD_ARMATURE = (1 << 4),       /* Armatures related to the selected objects. */
  // OB_REL_SCENE_CAMERA = (1 << 5), /* You might want the scene camera too even if unselected? */
};

enum eObjectSet {
  OB_SET_SELECTED, /* Selected Objects. */
  OB_SET_VISIBLE,  /* Visible Objects. */
  OB_SET_ALL,      /* All Objects. */
};

/**
 * Iterates over all objects of the given scene layer.
 * Depending on the #eObjectSet flag:
 * collect either #OB_SET_ALL, #OB_SET_VISIBLE or #OB_SET_SELECTED objects.
 * If #OB_SET_VISIBLE or#OB_SET_SELECTED are collected,
 * then also add related objects according to the given \a includeFilter.
 */
LinkNode *BKE_object_relational_superset(const Scene *scene,
                                         ViewLayer *view_layer,
                                         eObjectSet objectSet,
                                         eObRelationTypes includeFilter);
/**
 * \return All groups this object is a part of, caller must free.
 */
LinkNode *BKE_object_groups(Main *bmain, Scene *scene, Object *ob);
void BKE_object_groups_clear(Main *bmain, Scene *scene, Object *object);

/**
 * Return a KDTree_3d from the deformed object (in world-space).
 *
 * \note Only mesh objects currently support deforming, others are TODO.
 *
 * \param ob:
 * \param r_tot:
 * \return The KD-tree or nullptr if it can't be created.
 */
KDTree_3d *BKE_object_as_kdtree(Object *ob, int *r_tot);

/**
 * \note this function should eventually be replaced by depsgraph functionality.
 * Avoid calling this in new code unless there is a very good reason for it!
 */
bool BKE_object_modifier_update_subframe(Depsgraph *depsgraph,
                                         Scene *scene,
                                         Object *ob,
                                         bool update_mesh,
                                         int parent_recursion,
                                         float frame,
                                         int type);

bool BKE_object_empty_image_frame_is_visible_in_view3d(const Object *ob, const RegionView3D *rv3d);
bool BKE_object_empty_image_data_is_visible_in_view3d(const Object *ob, const RegionView3D *rv3d);

/**
 * This is an utility function for Python's object.to_mesh() (the naming is not very clear though).
 * The result is owned by the object.
 *
 * The mesh will be freed when object is re-evaluated or is destroyed. It is possible to force to
 * clear memory used by this mesh by calling BKE_object_to_mesh_clear().
 *
 * If preserve_all_data_layers is truth then the modifier stack is re-evaluated to ensure it
 * preserves all possible custom data layers.
 *
 * NOTE: Dependency graph argument is required when preserve_all_data_layers is truth, and is
 * ignored otherwise. */
Mesh *BKE_object_to_mesh(Depsgraph *depsgraph, Object *object, bool preserve_all_data_layers);

void BKE_object_to_mesh_clear(Object *object);

/**
 * This is an utility function for Python's `object.to_curve()`.
 * The result is owned by the object.
 *
 * The curve will be freed when object is re-evaluated or is destroyed. It is possible to force
 * clear memory used by this curve by calling #BKE_object_to_curve_clear().
 *
 * If apply_modifiers is true and the object is a curve one, then spline deform modifiers are
 * applied on the curve control points.
 */
Curve *BKE_object_to_curve(Object *object, Depsgraph *depsgraph, bool apply_modifiers);

void BKE_object_to_curve_clear(Object *object);

void BKE_object_check_uids_unique_and_report(const Object *object);

/**
 * Return the last subsurf modifier of an object, this does not check whether modifiers on top of
 * it are disabled. Return NULL if no such modifier is found.
 *
 * This does not check if the modifier is enabled as it is assumed that the caller verified that it
 * is enabled for its evaluation mode.
 */
SubsurfModifierData *BKE_object_get_last_subsurf_modifier(const Object *ob);

void BKE_object_replace_data_on_shallow_copy(Object *ob, ID *new_data);

PartEff *BKE_object_do_version_give_parteff_245(Object *ob);

bool BKE_object_supports_material_slots(Object *ob);
