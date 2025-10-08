/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BLI_compiler_attrs.h"
#include "BLI_span.hh"

#include "DNA_windowmanager_enums.h"

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
struct bArmature;
struct bContext;
struct bPoseChannel;
struct wmKeyConfig;
struct wmOperator;

#define BONESEL_ROOT (1u << 29)
#define BONESEL_TIP (1u << 30)
#define BONESEL_BONE (1u << 31)
#define BONESEL_ANY (BONESEL_TIP | BONESEL_ROOT | BONESEL_BONE)

#define EBONE_SELECTABLE(arm, ebone) \
  (blender::animrig::bone_is_visible(arm, ebone) && !((ebone)->flag & BONE_UNSELECTABLE))

#define EBONE_EDITABLE(ebone) \
  (CHECK_TYPE_INLINE(ebone, EditBone *), \
   (((ebone)->flag & BONE_SELECTED) && !((ebone)->flag & BONE_EDITMODE_LOCKED)))

/* Used in `armature_select.cc` and `pose_select.cc`. */

#define BONE_SELECT_PARENT 0
#define BONE_SELECT_CHILD 1

/* `armature_add.cc` */

/**
 * Default bone add, returns it selected, but without tail set.
 *
 * \note should be used everywhere, now it allocates bones still locally in functions.
 */
EditBone *ED_armature_ebone_add(bArmature *arm, const char *name);
EditBone *ED_armature_ebone_add_primitive(Object *obedit_arm, float length, bool view_aligned);

void ED_armature_ebone_copy(EditBone *dest, const EditBone *source);

/**
 * Get current armature from the context, including properties editor pinning.
 */
bArmature *ED_armature_context(const bContext *C);

/**
 * Adjust bone roll to align Z axis with vector `align_axis` is in local space and is normalized.
 */
float ED_armature_ebone_roll_to_vector(const EditBone *bone,
                                       const float align_axis[3],
                                       bool axis_only);
/**
 * \param centermode: 0 == do center, 1 == center new, 2 == center cursor.
 *
 * \note Exported for use in `editors/object/`.
 */
void ED_armature_origin_set(
    Main *bmain, Object *ob, const float cursor[3], int centermode, int around);
/**
 * See #BKE_armature_transform for object-mode transform.
 */
void ED_armature_edit_transform(bArmature *arm, const float mat[4][4], bool do_props);
void ED_armature_transform(bArmature *arm, const float mat[4][4], bool do_props);

/* `armature_naming.cc` */

/**
 * Ensure the bone name is unique.
 * If bone is already in list, pass it as argument to ignore it.
 */
void ED_armature_ebone_unique_name(ListBase *ebones, char *name, EditBone *bone);

/**
 * Bone Rename (called by UI for renaming a bone).
 * Seems messy, but that's what you get with not using pointers but channel names :).
 * \warning make sure the original bone was not renamed yet!
 */
void ED_armature_bone_rename(Main *bmain,
                             bArmature *arm,
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
void ED_armature_bones_flip_names(Main *bmain,
                                  bArmature *arm,
                                  ListBase *bones_names,
                                  bool do_strip_numbers);

/* `armature_ops.cc` */

void ED_operatortypes_armature();
void ED_operatormacros_armature();
void ED_keymap_armature(wmKeyConfig *keyconf);

/* `armature_relations.cc` */

/**
 * Join armature exec is exported for use in object->join objects operator.
 */
wmOperatorStatus ED_armature_join_objects_exec(bContext *C, wmOperator *op);

/* `armature_select.cc` */

Base *ED_armature_base_and_ebone_from_select_buffer(blender::Span<Base *> bases,
                                                    unsigned int select_id,
                                                    EditBone **r_ebone);
Object *ED_armature_object_and_ebone_from_select_buffer(blender::Span<Object *> objects,
                                                        unsigned int select_id,
                                                        EditBone **r_ebone);
Base *ED_armature_base_and_pchan_from_select_buffer(blender::Span<Base *> bases,
                                                    unsigned int select_id,
                                                    bPoseChannel **r_pchan);
/**
 * For callers that don't need the pose channel.
 */
Base *ED_armature_base_and_bone_from_select_buffer(blender::Span<Base *> bases,
                                                   unsigned int select_id,
                                                   Bone **r_bone);
bool ED_armature_edit_deselect_all(Object *obedit);
bool ED_armature_edit_deselect_all_visible(Object *obedit);
bool ED_armature_edit_deselect_all_multi_ex(blender::Span<Base *> bases);
bool ED_armature_edit_deselect_all_visible_multi_ex(blender::Span<Base *> bases);
bool ED_armature_edit_deselect_all_visible_multi(bContext *C);
/**
 * \return True when pick finds an element or the selection changed.
 */
bool ED_armature_edit_select_pick_bone(
    bContext *C, Base *basact, EditBone *ebone, int selmask, const SelectPick_Params &params);
/**
 * Bone selection picking for armature edit-mode in the view3d.
 */
bool ED_armature_edit_select_pick(bContext *C, const int mval[2], const SelectPick_Params &params);
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
bool ED_armature_edit_select_op_from_tagged(bArmature *arm, int sel_op);

/* `armature_skinning.cc` */

#define ARM_GROUPS_NAME 1
#define ARM_GROUPS_ENVELOPE 2
#define ARM_GROUPS_AUTO 3
void ED_object_vgroup_calc_from_armature(ReportList *reports,
                                         Depsgraph *depsgraph,
                                         Scene *scene,
                                         Object *ob,
                                         Object *par,
                                         int mode,
                                         bool mirror);

/* `editarmature_undo.cc` */

/** Export for ED_undo_sys. */
void ED_armature_undosys_type(UndoType *ut);

/* `armature_utils.cc` */

/** Sync selection to parent for connected children. */
void ED_armature_edit_sync_selection(ListBase *edbo);
/**
 * \param clear_connected: When false caller is responsible for keeping the flag in a valid state.
 */
void ED_armature_ebone_remove_ex(bArmature *arm, EditBone *exBone, bool clear_connected);
void ED_armature_ebone_remove(bArmature *arm, EditBone *exBone);
bool ED_armature_ebone_is_child_recursive(EditBone *ebone_parent, EditBone *ebone_child);
/**
 * Finds the first parent shared by \a ebone_child
 *
 * \param ebone_child: Children bones to search
 * \param ebone_child_tot: Size of the ebone_child array
 * \return The shared parent or NULL.
 */
EditBone *ED_armature_ebone_find_shared_parent(EditBone *ebone_child[],
                                               unsigned int ebone_child_tot);
void ED_armature_ebone_to_mat3(EditBone *ebone, float r_mat[3][3]);
void ED_armature_ebone_to_mat4(EditBone *ebone, float r_mat[4][4]);
void ED_armature_ebone_from_mat3(EditBone *ebone, const float mat[3][3]);
void ED_armature_ebone_from_mat4(EditBone *ebone, const float mat[4][4]);
/**
 * Return a pointer to the bone of the given name
 */
EditBone *ED_armature_ebone_find_name(const ListBase *edbo, const char *name);
/**
 * \see #BKE_pose_channel_get_mirrored (pose-mode, matching function)
 */
EditBone *ED_armature_ebone_get_mirrored(const ListBase *edbo, EditBone *ebo);
void ED_armature_ebone_transform_mirror_update(bArmature *arm, EditBone *ebo, bool check_select);
/**
 * If edit-bone (partial) selected, copy data.
 * context; edit-mode armature, with mirror editing enabled.
 */
void ED_armature_edit_transform_mirror_update(Object *obedit);
/** Put edit-mode back in Object. */
void ED_armature_from_edit(Main *bmain, bArmature *arm);
/** Put armature in edit-mode. */
void ED_armature_to_edit(bArmature *arm);
void ED_armature_edit_free(bArmature *arm);
void ED_armature_ebone_listbase_temp_clear(ListBase *lb);

/**
 * Free list of bones and their properties.
 */
void ED_armature_ebone_listbase_free(ListBase *lb, bool do_id_user);
void ED_armature_ebone_listbase_copy(ListBase *lb_dst, ListBase *lb_src, bool do_id_user);

int ED_armature_ebone_selectflag_get(const EditBone *ebone);
void ED_armature_ebone_selectflag_set(EditBone *ebone, int flag);
void ED_armature_ebone_select_set(EditBone *ebone, bool select);
void ED_armature_ebone_selectflag_enable(EditBone *ebone, int flag);
void ED_armature_ebone_selectflag_disable(EditBone *ebone, int flag);

/* `pose_edit.cc` */

Object *ED_pose_object_from_context(bContext *C);
bool ED_object_posemode_exit_ex(Main *bmain, Object *ob);
bool ED_object_posemode_exit(bContext *C, Object *ob);
/** This function is used to process the necessary updates for. */
bool ED_object_posemode_enter_ex(Main *bmain, Object *ob);
bool ED_object_posemode_enter(bContext *C, Object *ob);

/** Corresponds to #eAnimvizCalcRange. */
enum ePosePathCalcRange {
  POSE_PATH_CALC_RANGE_CURRENT_FRAME,
  POSE_PATH_CALC_RANGE_CHANGED,
  POSE_PATH_CALC_RANGE_FULL,
};
/**
 * For the object with pose/action: update paths for those that have got them
 * This should selectively update paths that exist...
 *
 * To be called from various tools that do incremental updates.
 */
void ED_pose_recalculate_paths(bContext *C, Scene *scene, Object *ob, ePosePathCalcRange range);

/* `pose_select.cc` */

/**
 * \return True when pick finds an element or the selection changed.
 */
bool ED_armature_pose_select_pick_bone(const Scene *scene,
                                       ViewLayer *view_layer,
                                       View3D *v3d,
                                       Object *ob,
                                       bPoseChannel *pchan,
                                       const SelectPick_Params &params) ATTR_NONNULL(1, 2, 3, 4);
/**
 * Called for mode-less pose selection.
 * assumes the active object is still on old situation.
 *
 * \return True when pick finds an element or the selection changed.
 */
bool ED_armature_pose_select_pick_with_buffer(const Scene *scene,
                                              ViewLayer *view_layer,
                                              View3D *v3d,
                                              Base *base,
                                              const GPUSelectResult *hit_results,
                                              int hits,
                                              const SelectPick_Params &params,
                                              bool do_nearest) ATTR_NONNULL(1, 2, 3, 4, 5);
/**
 * While in weight-paint mode, a single pose may be active as well.
 * While not common, it's possible we have multiple armatures deforming a mesh.
 *
 * This function de-selects all other objects, and selects the new base.
 * It can't be set to the active object because we need
 * to keep this set to the weight paint object.
 */
void ED_armature_pose_select_in_wpaint_mode(const Scene *scene,
                                            ViewLayer *view_layer,
                                            Base *base_select);
bool ED_pose_deselect_all_multi_ex(blender::Span<Base *> bases,
                                   int select_mode,
                                   bool ignore_visibility);
bool ED_pose_deselect_all_multi(bContext *C, int select_mode, bool ignore_visibility);
/**
 * 'select_mode' is usual SEL_SELECT/SEL_DESELECT/SEL_TOGGLE/SEL_INVERT.
 * When true, 'ignore_visibility' makes this func also affect invisible bones
 * (hidden or on hidden layers).
 */
bool ED_pose_deselect_all(Object *ob, int select_mode, bool ignore_visibility);
void ED_pose_bone_select_tag_update(Object *ob);
/**
 * Utility method for changing the selection status of a bone.
 * change_active determines whether to change the active bone of the armature when selecting pose
 * channels. It is false during range selection otherwise true.
 */
void ED_pose_bone_select(Object *ob, bPoseChannel *pchan, bool select, bool change_active);

/* `meshlaplacian.cc` */

void ED_mesh_deform_bind_callback(Object *object,
                                  MeshDeformModifierData *mmd,
                                  Mesh *cagemesh,
                                  float *vertexcos,
                                  int verts_num,
                                  float cagemat[4][4]);

EditBone *ED_armature_pick_ebone(bContext *C, const int xy[2], bool findunsel, Base **r_base);
bPoseChannel *ED_armature_pick_pchan(bContext *C, const int xy[2], bool findunsel, Base **r_base);
Bone *ED_armature_pick_bone(bContext *C, const int xy[2], bool findunsel, Base **r_base);
