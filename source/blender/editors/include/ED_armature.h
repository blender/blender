/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup editors
 */

#pragma once

#include <stdbool.h>

#include "BLI_listbase.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Base;
struct Bone;
struct Depsgraph;
struct EditBone;
struct GPUSelectResult;
struct ListBase;
struct Main;
struct Mesh;
struct MeshDeformModifierData;
struct Object;
struct ReportList;
struct Scene;
struct SelectPick_Params;
struct UndoType;
struct View3D;
struct ViewLayer;
struct bAction;
struct bArmature;
struct bContext;
struct bPoseChannel;
struct wmKeyConfig;
struct wmOperator;

#define BONESEL_ROOT (1u << 29)
#define BONESEL_TIP (1u << 30)
#define BONESEL_BONE (1u << 31)
#define BONESEL_ANY (BONESEL_TIP | BONESEL_ROOT | BONESEL_BONE)

/* useful macros */
#define EBONE_VISIBLE(arm, ebone) \
  (CHECK_TYPE_INLINE(arm, bArmature *), \
   CHECK_TYPE_INLINE(ebone, EditBone *), \
   (((arm)->layer & (ebone)->layer) && !((ebone)->flag & BONE_HIDDEN_A)))

#define EBONE_SELECTABLE(arm, ebone) \
  (EBONE_VISIBLE(arm, ebone) && !((ebone)->flag & BONE_UNSELECTABLE))

#define EBONE_EDITABLE(ebone) \
  (CHECK_TYPE_INLINE(ebone, EditBone *), \
   (((ebone)->flag & BONE_SELECTED) && !((ebone)->flag & BONE_EDITMODE_LOCKED)))

/* Used in `armature_select.c` and `pose_select.c`. */

#define BONE_SELECT_PARENT 0
#define BONE_SELECT_CHILD 1

/* armature_add.c */

/**
 * Default bone add, returns it selected, but without tail set.
 *
 * \note should be used everywhere, now it allocates bones still locally in functions.
 */
struct EditBone *ED_armature_ebone_add(struct bArmature *arm, const char *name);
struct EditBone *ED_armature_ebone_add_primitive(struct Object *obedit_arm,
                                                 float length,
                                                 bool view_aligned);

/* armature_edit.c */

/**
 * Adjust bone roll to align Z axis with vector `align_axis` is in local space and is normalized.
 */
float ED_armature_ebone_roll_to_vector(const struct EditBone *bone,
                                       const float align_axis[3],
                                       bool axis_only);
/**
 * \param centermode: 0 == do center, 1 == center new, 2 == center cursor.
 *
 * \note Exported for use in `editors/object/`.
 */
void ED_armature_origin_set(
    struct Main *bmain, struct Object *ob, const float cursor[3], int centermode, int around);
/**
 * See #BKE_armature_transform for object-mode transform.
 */
void ED_armature_edit_transform(struct bArmature *arm, const float mat[4][4], bool do_props);
void ED_armature_transform(struct bArmature *arm, const float mat[4][4], bool do_props);

/* armature_naming.c */

/**
 * Ensure the bone name is unique.
 * If bone is already in list, pass it as argument to ignore it.
 */
void ED_armature_ebone_unique_name(struct ListBase *ebones, char *name, struct EditBone *bone);

/**
 * Bone Rename (called by UI for renaming a bone).
 * Seems messy, but that's what you get with not using pointers but channel names :).
 * \warning make sure the original bone was not renamed yet!
 */
void ED_armature_bone_rename(struct Main *bmain,
                             struct bArmature *arm,
                             const char *oldnamep,
                             const char *newnamep);
/**
 * Renames (by flipping) all selected bones at once.
 *
 * This way if we are flipping related bones (e.g., Bone.L, Bone.R) at the same time
 * all the bones are safely renamed, without conflicting with each other.
 *
 * \param arm: Armature the bones belong to
 * \param bones_names: List of bone conflict elements (#LinkData pointing to names).
 * \param do_strip_numbers: if set, try to get rid of dot-numbers at end of bone names.
 */
void ED_armature_bones_flip_names(struct Main *bmain,
                                  struct bArmature *arm,
                                  struct ListBase *bones_names,
                                  bool do_strip_numbers);

/* armature_ops.c */

void ED_operatortypes_armature(void);
void ED_operatormacros_armature(void);
void ED_keymap_armature(struct wmKeyConfig *keyconf);

/* armature_relations.c */

/**
 * Join armature exec is exported for use in object->join objects operator.
 */
int ED_armature_join_objects_exec(struct bContext *C, struct wmOperator *op);

/* armature_select.c */

struct Base *ED_armature_base_and_ebone_from_select_buffer(struct Base **bases,
                                                           uint bases_len,
                                                           unsigned int select_id,
                                                           struct EditBone **r_ebone);
struct Object *ED_armature_object_and_ebone_from_select_buffer(struct Object **objects,
                                                               uint objects_len,
                                                               unsigned int select_id,
                                                               struct EditBone **r_ebone);
struct Base *ED_armature_base_and_pchan_from_select_buffer(struct Base **bases,
                                                           uint bases_len,
                                                           unsigned int select_id,
                                                           struct bPoseChannel **r_pchan);
/**
 * For callers that don't need the pose channel.
 */
struct Base *ED_armature_base_and_bone_from_select_buffer(struct Base **bases,
                                                          uint bases_len,
                                                          unsigned int select_id,
                                                          struct Bone **r_bone);
bool ED_armature_edit_deselect_all(struct Object *obedit);
bool ED_armature_edit_deselect_all_visible(struct Object *obedit);
bool ED_armature_edit_deselect_all_multi_ex(struct Base **bases, uint bases_len);
bool ED_armature_edit_deselect_all_visible_multi_ex(struct Base **bases, uint bases_len);
bool ED_armature_edit_deselect_all_visible_multi(struct bContext *C);
/**
 * \return True when pick finds an element or the selection changed.
 */
bool ED_armature_edit_select_pick_bone(struct bContext *C,
                                       struct Base *basact,
                                       struct EditBone *ebone,
                                       int selmask,
                                       const struct SelectPick_Params *params);
/**
 * Bone selection picking for armature edit-mode in the view3d.
 */
bool ED_armature_edit_select_pick(struct bContext *C,
                                  const int mval[2],
                                  const struct SelectPick_Params *params);
/**
 * Perform a selection operation on elements which have been 'touched',
 * use for lasso & border select but can be used elsewhere too.
 *
 * Tagging is done via #EditBone.temp.i using: #BONESEL_ROOT, #BONESEL_TIP, #BONESEL_BONE
 * And optionally ignoring end-points using the #BONESEL_ROOT, #BONESEL_TIP right shifted 16 bits.
 * (used when the values are clipped outside the view).
 *
 * \param sel_op: #eSelectOp type.
 *
 * \note Visibility checks must be done by the caller.
 */
bool ED_armature_edit_select_op_from_tagged(struct bArmature *arm, int sel_op);

/* armature_skinning.c */

#define ARM_GROUPS_NAME 1
#define ARM_GROUPS_ENVELOPE 2
#define ARM_GROUPS_AUTO 3
void ED_object_vgroup_calc_from_armature(struct ReportList *reports,
                                         struct Depsgraph *depsgraph,
                                         struct Scene *scene,
                                         struct Object *ob,
                                         struct Object *par,
                                         int mode,
                                         bool mirror);

/* editarmature_undo.c */

/** Export for ED_undo_sys. */
void ED_armature_undosys_type(struct UndoType *ut);

/* armature_utils.c */

/** Sync selection to parent for connected children. */
void ED_armature_edit_sync_selection(struct ListBase *edbo);
void ED_armature_edit_validate_active(struct bArmature *arm);
/**
 * Update the layers_used variable after bones are moved between layer
 * \note Used to be done in drawing code in 2.7, but that won't work with
 * Copy-on-Write, as drawing uses evaluated copies.
 */
void ED_armature_edit_refresh_layer_used(struct bArmature *arm);
/**
 * \param clear_connected: When false caller is responsible for keeping the flag in a valid state.
 */
void ED_armature_ebone_remove_ex(struct bArmature *arm,
                                 struct EditBone *exBone,
                                 bool clear_connected);
void ED_armature_ebone_remove(struct bArmature *arm, struct EditBone *exBone);
bool ED_armature_ebone_is_child_recursive(struct EditBone *ebone_parent,
                                          struct EditBone *ebone_child);
/**
 * Finds the first parent shared by \a ebone_child
 *
 * \param ebone_child: Children bones to search
 * \param ebone_child_tot: Size of the ebone_child array
 * \return The shared parent or NULL.
 */
struct EditBone *ED_armature_ebone_find_shared_parent(struct EditBone *ebone_child[],
                                                      unsigned int ebone_child_tot);
void ED_armature_ebone_to_mat3(struct EditBone *ebone, float r_mat[3][3]);
void ED_armature_ebone_to_mat4(struct EditBone *ebone, float r_mat[4][4]);
void ED_armature_ebone_from_mat3(struct EditBone *ebone, const float mat[3][3]);
void ED_armature_ebone_from_mat4(struct EditBone *ebone, const float mat[4][4]);
/**
 * Return a pointer to the bone of the given name
 */
struct EditBone *ED_armature_ebone_find_name(const struct ListBase *edbo, const char *name);
/**
 * \see #BKE_pose_channel_get_mirrored (pose-mode, matching function)
 */
struct EditBone *ED_armature_ebone_get_mirrored(const struct ListBase *edbo, struct EditBone *ebo);
void ED_armature_ebone_transform_mirror_update(struct bArmature *arm,
                                               struct EditBone *ebo,
                                               bool check_select);
/**
 * If edit-bone (partial) selected, copy data.
 * context; edit-mode armature, with mirror editing enabled.
 */
void ED_armature_edit_transform_mirror_update(struct Object *obedit);
/** Put edit-mode back in Object. */
void ED_armature_from_edit(struct Main *bmain, struct bArmature *arm);
/** Put armature in edit-mode. */
void ED_armature_to_edit(struct bArmature *arm);
void ED_armature_edit_free(struct bArmature *arm);
void ED_armature_ebone_listbase_temp_clear(struct ListBase *lb);

/**
 * Free's bones and their properties.
 */
void ED_armature_ebone_listbase_free(struct ListBase *lb, bool do_id_user);
void ED_armature_ebone_listbase_copy(struct ListBase *lb_dst,
                                     struct ListBase *lb_src,
                                     bool do_id_user);

int ED_armature_ebone_selectflag_get(const struct EditBone *ebone);
void ED_armature_ebone_selectflag_set(struct EditBone *ebone, int flag);
void ED_armature_ebone_select_set(struct EditBone *ebone, bool select);
void ED_armature_ebone_selectflag_enable(struct EditBone *ebone, int flag);
void ED_armature_ebone_selectflag_disable(struct EditBone *ebone, int flag);

/* pose_edit.c */
struct Object *ED_pose_object_from_context(struct bContext *C);
bool ED_object_posemode_exit_ex(struct Main *bmain, struct Object *ob);
bool ED_object_posemode_exit(struct bContext *C, struct Object *ob);
/** This function is used to process the necessary updates for. */
bool ED_object_posemode_enter_ex(struct Main *bmain, struct Object *ob);
bool ED_object_posemode_enter(struct bContext *C, struct Object *ob);

/** Corresponds to #eAnimvizCalcRange. */
typedef enum ePosePathCalcRange {
  POSE_PATH_CALC_RANGE_CURRENT_FRAME,
  POSE_PATH_CALC_RANGE_CHANGED,
  POSE_PATH_CALC_RANGE_FULL,
} ePosePathCalcRange;
/**
 * For the object with pose/action: update paths for those that have got them
 * This should selectively update paths that exist...
 *
 * To be called from various tools that do incremental updates.
 */
void ED_pose_recalculate_paths(struct bContext *C,
                               struct Scene *scene,
                               struct Object *ob,
                               ePosePathCalcRange range);

/* pose_select.c */

/**
 * \return True when pick finds an element or the selection changed.
 */
bool ED_armature_pose_select_pick_bone(struct ViewLayer *view_layer,
                                       struct View3D *v3d,
                                       struct Object *ob,
                                       struct Bone *bone,
                                       const struct SelectPick_Params *params);
/**
 * Called for mode-less pose selection.
 * assumes the active object is still on old situation.
 *
 * \return True when pick finds an element or the selection changed.
 */
bool ED_armature_pose_select_pick_with_buffer(struct ViewLayer *view_layer,
                                              struct View3D *v3d,
                                              struct Base *base,
                                              const struct GPUSelectResult *buffer,
                                              short hits,
                                              const struct SelectPick_Params *params,
                                              bool do_nearest);
/**
 * While in weight-paint mode, a single pose may be active as well.
 * While not common, it's possible we have multiple armatures deforming a mesh.
 *
 * This function de-selects all other objects, and selects the new base.
 * It can't be set to the active object because we need
 * to keep this set to the weight paint object.
 */
void ED_armature_pose_select_in_wpaint_mode(struct ViewLayer *view_layer,
                                            struct Base *base_select);
bool ED_pose_deselect_all_multi_ex(struct Base **bases,
                                   uint bases_len,
                                   int select_mode,
                                   bool ignore_visibility);
bool ED_pose_deselect_all_multi(struct bContext *C, int select_mode, bool ignore_visibility);
/**
 * 'select_mode' is usual SEL_SELECT/SEL_DESELECT/SEL_TOGGLE/SEL_INVERT.
 * When true, 'ignore_visibility' makes this func also affect invisible bones
 * (hidden or on hidden layers).
 */
bool ED_pose_deselect_all(struct Object *ob, int select_mode, bool ignore_visibility);
void ED_pose_bone_select_tag_update(struct Object *ob);
/**
 * Utility method for changing the selection status of a bone.
 */
void ED_pose_bone_select(struct Object *ob, struct bPoseChannel *pchan, bool select);

/* meshlaplacian.c */
void ED_mesh_deform_bind_callback(struct Object *object,
                                  struct MeshDeformModifierData *mmd,
                                  struct Mesh *cagemesh,
                                  float *vertexcos,
                                  int totvert,
                                  float cagemat[4][4]);

/* Pose backups, pose_backup.c */
struct PoseBackup;
/**
 * Create a backup of those bones that are animated in the given action.
 */
struct PoseBackup *ED_pose_backup_create_selected_bones(
    const struct Object *ob, const struct bAction *action) ATTR_WARN_UNUSED_RESULT;
struct PoseBackup *ED_pose_backup_create_all_bones(
    const struct Object *ob, const struct bAction *action) ATTR_WARN_UNUSED_RESULT;
bool ED_pose_backup_is_selection_relevant(const struct PoseBackup *pose_backup);
void ED_pose_backup_restore(const struct PoseBackup *pbd);
void ED_pose_backup_free(struct PoseBackup *pbd);

#ifdef __cplusplus
}
#endif
