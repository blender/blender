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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ED_armature.h
 *  \ingroup editors
 */

#ifndef __ED_ARMATURE_H__
#define __ED_ARMATURE_H__

#ifdef __cplusplus
extern "C" {
#endif

struct bArmature;
struct Base;
struct bContext;
struct Bone;
struct bPoseChannel;
struct DerivedMesh;
struct IDProperty;
struct ListBase;
struct MeshDeformModifierData;
struct Object;
struct RegionView3D;
struct ReportList;
struct Scene;
struct SK_Sketch;
struct View3D;
struct ViewContext;
struct wmKeyConfig;
struct wmOperator;

typedef struct EditBone {
	struct EditBone *next, *prev;
	struct IDProperty *prop; /* User-Defined Properties on this Bone */
	struct EditBone *parent; /* Editbones have a one-way link  (i.e. children refer
	                          * to parents.  This is converted to a two-way link for
	                          * normal bones when leaving editmode. */
	void    *temp;          /* Used to store temporary data */

	char    name[64];       /* MAXBONENAME */
	float   roll;           /* Roll along axis.  We'll ultimately use the axis/angle method
	                         * for determining the transformation matrix of the bone.  The axis
	                         * is tail-head while roll provides the angle. Refer to Graphics
	                         * Gems 1 p. 466 (section IX.6) if it's not already in here somewhere*/

	float head[3];          /* Orientation and length is implicit during editing */
	float tail[3];
	/* All joints are considered to have zero rotation with respect to
	 * their parents.	Therefore any rotations specified during the
	 * animation are automatically relative to the bones' rest positions*/
	int flag;
	int layer;
	
	float dist, weight;
	float xwidth, length, zwidth;  /* put them in order! transform uses this as scale */
	float ease1, ease2;
	float rad_head, rad_tail;
	
	float oldlength;        /* for envelope scaling */
	
	short segments;
} EditBone;

#define BONESEL_ROOT    (1 << 28)
#define BONESEL_TIP     (1 << 29)
#define BONESEL_BONE    (1 << 30)
#define BONESEL_ANY     (BONESEL_TIP | BONESEL_ROOT | BONESEL_BONE)

#define BONESEL_NOSEL   (1u << 31u)

/* useful macros */
#define EBONE_VISIBLE(arm, ebone) ( \
	CHECK_TYPE_INLINE(arm, bArmature *), \
	CHECK_TYPE_INLINE(ebone, EditBone *), \
	(((arm)->layer & (ebone)->layer) && !((ebone)->flag & BONE_HIDDEN_A)) \
	)

#define EBONE_SELECTABLE(arm, ebone) (EBONE_VISIBLE(arm, ebone) && !(ebone->flag & BONE_UNSELECTABLE))

#define EBONE_EDITABLE(ebone) ( \
	CHECK_TYPE_INLINE(ebone, EditBone *), \
	(((ebone)->flag & BONE_SELECTED) && !((ebone)->flag & BONE_EDITMODE_LOCKED)) \
	)

/* used in armature_select_hierarchy_exec() */
#define BONE_SELECT_PARENT  0
#define BONE_SELECT_CHILD   1

/* armature_ops.c */
void ED_operatortypes_armature(void);
void ED_operatormacros_armature(void);
void ED_keymap_armature(struct wmKeyConfig *keyconf);

/* editarmature.c */
void ED_armature_from_edit(struct bArmature *arm);
void ED_armature_to_edit(struct bArmature *arm);
void ED_armature_edit_free(struct bArmature *arm);
void ED_armature_deselect_all(struct Object *obedit, int toggle);
void ED_armature_deselect_all_visible(struct Object *obedit);

int ED_do_pose_selectbuffer(struct Scene *scene, struct Base *base, unsigned int *buffer, 
                            short hits, bool extend, bool deselect, bool toggle, bool do_nearest);
bool mouse_armature(struct bContext *C, const int mval[2], bool extend, bool deselect, bool toggle);
int join_armature_exec(struct bContext *C, struct wmOperator *op);
struct Bone *get_indexed_bone(struct Object *ob, int index);
float ED_rollBoneToVector(EditBone *bone, const float new_up_axis[3], const bool axis_only);
EditBone *ED_armature_bone_find_name(const ListBase *edbo, const char *name);
EditBone *ED_armature_bone_get_mirrored(const struct ListBase *edbo, EditBone *ebo);
void ED_armature_sync_selection(struct ListBase *edbo);
void ED_armature_validate_active(struct bArmature *arm);

EditBone *ED_armature_edit_bone_add_primitive(struct Object *obedit_arm, float length, bool view_aligned);
EditBone *ED_armature_edit_bone_add(struct bArmature *arm, const char *name);
void ED_armature_edit_bone_remove(struct bArmature *arm, EditBone *exBone);

bool ED_armature_ebone_is_child_recursive(EditBone *ebone_parent, EditBone *ebone_child);
EditBone *ED_armature_bone_find_shared_parent(EditBone *ebone_child[], const unsigned int ebone_child_tot);

void ED_armature_ebone_to_mat3(EditBone *ebone, float mat[3][3]);
void ED_armature_ebone_to_mat4(EditBone *ebone, float mat[4][4]);

void ED_armature_ebone_from_mat3(EditBone *ebone, float mat[3][3]);
void ED_armature_ebone_from_mat4(EditBone *ebone, float mat[4][4]);

void transform_armature_mirror_update(struct Object *obedit);
void ED_armature_origin_set(struct Scene *scene, struct Object *ob, float cursor[3], int centermode, int around);

void ED_armature_transform_bones(struct bArmature *arm, float mat[4][4]);
void ED_armature_apply_transform(struct Object *ob, float mat[4][4]);
void ED_armature_transform(struct bArmature *arm, float mat[4][4]);

#define ARM_GROUPS_NAME     1
#define ARM_GROUPS_ENVELOPE 2
#define ARM_GROUPS_AUTO     3

void create_vgroups_from_armature(struct ReportList *reports, struct Scene *scene, struct Object *ob, struct Object *par, int mode, bool mirror);

void unique_editbone_name(struct ListBase *ebones, char *name, EditBone *bone); /* if bone is already in list, pass it as param to ignore it */
void ED_armature_bone_rename(struct bArmature *arm, const char *oldnamep, const char *newnamep);

void undo_push_armature(struct bContext *C, const char *name);

/* low level selection functions which handle */
int  ED_armature_ebone_selectflag_get(const EditBone *ebone);
void ED_armature_ebone_selectflag_set(EditBone *ebone, int flag);
void ED_armature_ebone_select_set(EditBone *ebone, bool select);
void ED_armature_ebone_selectflag_enable(EditBone *ebone, int flag);
void ED_armature_ebone_selectflag_disable(EditBone *ebone, int flag);

/* poseobject.c */
void ED_armature_exit_posemode(struct bContext *C, struct Base *base);
void ED_armature_enter_posemode(struct bContext *C, struct Base *base);
void ED_pose_de_selectall(struct Object *ob, int select_mode, const bool ignore_visibility);
void ED_pose_bone_select(struct Object *ob, struct bPoseChannel *pchan, bool select);
void ED_pose_recalculate_paths(struct Scene *scene, struct Object *ob);
struct Object *ED_pose_object_from_context(struct bContext *C);

/* sketch */

int ED_operator_sketch_mode_active_stroke(struct bContext *C);
int ED_operator_sketch_full_mode(struct bContext *C);
int ED_operator_sketch_mode(const struct bContext *C);

void BIF_convertSketch(struct bContext *C);
void BIF_deleteSketch(struct bContext *C);
void BIF_selectAllSketch(struct bContext *C, int mode); /* -1: deselect, 0: select, 1: toggle */

void  BIF_makeListTemplates(const struct bContext *C);
int   BIF_currentTemplate(const struct bContext *C);
void  BIF_freeTemplates(struct bContext *C);
void  BIF_setTemplate(struct bContext *C, int index);
int   BIF_nbJointsTemplate(const struct bContext *C);
const char *BIF_nameBoneTemplate(const struct bContext *C);

void BDR_drawSketch(const struct bContext *vc);
int BDR_drawSketchNames(struct ViewContext *vc);

/* meshlaplacian.c */
void mesh_deform_bind(struct Scene *scene,
                      struct MeshDeformModifierData *mmd,
                      float *vertexcos, int totvert, float cagemat[4][4]);
	
#ifdef __cplusplus
}
#endif

#endif /* __ED_ARMATURE_H__ */
