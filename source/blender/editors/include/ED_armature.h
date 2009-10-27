/**
 * $Id:
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef ED_ARMATURE_H
#define ED_ARMATURE_H

struct bContext;
struct Scene;
struct Object;
struct Base;
struct Bone;
struct bArmature;
struct bPoseChannel;
struct wmOperator;
struct wmKeyConfig;
struct ListBase;
struct View3D;
struct ViewContext;
struct RegionView3D;
struct SK_Sketch;

typedef struct EditBone
{
	struct EditBone *next, *prev;
	struct EditBone *parent;/*	Editbones have a one-way link  (i.e. children refer
									to parents.  This is converted to a two-way link for
									normal bones when leaving editmode.	*/
	void	*temp;			/*	Used to store temporary data */

	char	name[32];
	float	roll;			/*	Roll along axis.  We'll ultimately use the axis/angle method
								for determining the transformation matrix of the bone.  The axis
								is tail-head while roll provides the angle. Refer to Graphics
								Gems 1 p. 466 (section IX.6) if it's not already in here somewhere*/

	float	head[3];			/*	Orientation and length is implicit during editing */
	float	tail[3];	
							/*	All joints are considered to have zero rotation with respect to
							their parents.	Therefore any rotations specified during the
							animation are automatically relative to the bones' rest positions*/
	int		flag;

	int		parNr;		/* Used for retrieving values from the menu system */
	
	float dist, weight;
	float xwidth, length, zwidth;	/* put them in order! transform uses this as scale */
	float ease1, ease2;
	float rad_head, rad_tail;
	short layer, segments;
	
	float oldlength;				/* for envelope scaling */

} EditBone;

#define	BONESEL_ROOT	0x10000000
#define	BONESEL_TIP		0x20000000
#define	BONESEL_BONE	0x40000000
#define BONESEL_ANY		(BONESEL_TIP|BONESEL_ROOT|BONESEL_BONE)

#define BONESEL_NOSEL	0x80000000	/* Indicates a negative number */

/* useful macros */
#define EBONE_VISIBLE(arm, ebone) ((arm->layer & ebone->layer) && !(ebone->flag & BONE_HIDDEN_A))
#define EBONE_EDITABLE(ebone) ((ebone->flag & BONE_SELECTED) && !(ebone->flag & BONE_EDITMODE_LOCKED)) 

/* used in bone_select_hierachy() */
#define BONE_SELECT_PARENT	0
#define BONE_SELECT_CHILD	1

/* armature_ops.c */
void ED_operatortypes_armature(void);
void ED_operatormacros_armature(void);
void ED_keymap_armature(struct wmKeyConfig *keyconf);

/* editarmature.c */
void ED_armature_from_edit(struct Object *obedit);
void ED_armature_to_edit(struct Object *ob);
void ED_armature_edit_free(struct Object *ob);
void ED_armature_edit_remake(struct Object *obedit);
void ED_armature_deselectall(struct Object *obedit, int toggle, int doundo);

int ED_do_pose_selectbuffer(struct Scene *scene, struct Base *base, unsigned int *buffer, 
							short hits, short extend);
void mouse_armature(struct bContext *C, short mval[2], int extend);
int join_armature_exec(struct bContext *C, struct wmOperator *op);
struct Bone *get_indexed_bone (struct Object *ob, int index);
float ED_rollBoneToVector(EditBone *bone, float new_up_axis[3]);
EditBone *ED_armature_bone_get_mirrored(struct ListBase *edbo, EditBone *ebo); // XXX this is needed for populating the context iterators
void ED_armature_sync_selection(struct ListBase *edbo);

void add_primitive_bone(struct Scene *scene, struct View3D *v3d, struct RegionView3D *rv3d);
EditBone *addEditBone(struct bArmature *arm, char *name); /* used by COLLADA importer */

void transform_armature_mirror_update(struct Object *obedit);
void clear_armature(struct Scene *scene, struct Object *ob, char mode);
void docenter_armature (struct Scene *scene, struct View3D *v3d, struct Object *ob, int centermode);

void ED_armature_apply_transform(struct Object *ob, float mat[4][4]);

#define ARM_GROUPS_NAME		1
#define ARM_GROUPS_ENVELOPE	2
#define ARM_GROUPS_AUTO		3

void create_vgroups_from_armature(struct Scene *scene, struct Object *ob, struct Object *par, int mode);

void auto_align_armature(struct Scene *scene, struct View3D *v3d, short mode);
void unique_editbone_name(struct ListBase *ebones, char *name, EditBone *bone); /* if bone is already in list, pass it as param to ignore it */
void ED_armature_bone_rename(struct bArmature *arm, char *oldnamep, char *newnamep);

void undo_push_armature(struct bContext *C, char *name);

/* poseobject.c */
void ED_armature_exit_posemode(struct bContext *C, struct Base *base);
void ED_armature_enter_posemode(struct bContext *C, struct Base *base);
int ED_pose_channel_in_IK_chain(struct Object *ob, struct bPoseChannel *pchan);
void ED_pose_deselectall(struct Object *ob, int test, int doundo);
void ED_pose_recalculate_paths(struct bContext *C, struct Scene *scene, struct Object *ob);

/* sketch */

int ED_operator_sketch_mode_active_stroke(struct bContext *C);
int ED_operator_sketch_full_mode(struct bContext *C);
int ED_operator_sketch_mode(const struct bContext *C);

void BIF_convertSketch(struct bContext *C);
void BIF_deleteSketch(struct bContext *C);
void BIF_selectAllSketch(struct bContext *C, int mode); /* -1: deselect, 0: select, 1: toggle */

void  BIF_makeListTemplates(const struct bContext *C);
char *BIF_listTemplates(const struct bContext *C);
int   BIF_currentTemplate(const struct bContext *C);
void  BIF_freeTemplates(struct bContext *C);
void  BIF_setTemplate(struct bContext *C, int index);
int   BIF_nbJointsTemplate(const struct bContext *C);
char * BIF_nameBoneTemplate(const struct bContext *C);

void BDR_drawSketch(const struct bContext *vc);
int BDR_drawSketchNames(struct ViewContext *vc);

#endif /* ED_ARMATURE_H */



