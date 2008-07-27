/**
 * $Id$
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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef BIF_EDITARMATURE_H
#define BIF_EDITARMATURE_H

struct Object;
struct Base;
struct Bone;
struct bArmature;
struct ListBase;

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


void	adduplicate_armature(void);
void	addvert_armature(void);
void	add_primitiveArmature(int type);
void	apply_rot_armature (struct Object *ob, float mat[3][3]);
void 	docenter_armature (struct Object *ob, int centermode);

void	clear_armature(struct Object *ob, char mode);

void	delete_armature(void);
void	deselectall_armature(int toggle, int doundo);
void	deselectall_posearmature (struct Object *ob, int test, int doundo);
int		draw_armature(struct Base *base, int dt, int flag);

void	extrude_armature(int forked);
void	subdivide_armature(int numcuts);
void	fill_bones_armature(void);
void	merge_armature(void);

void	free_editArmature(void);

int		join_armature(void);
void 	separate_armature(void);
void	load_editArmature(void);

void	make_bone_parent(void);
void    clear_bone_parent(void);
struct Bone	*get_indexed_bone (struct Object *ob, int index);

void	make_editArmature(void);
void	make_trans_bones (char mode);

int		do_pose_selectbuffer(struct Base *base, unsigned int *buffer, short hits);

void generateSkeleton(void);

void	mouse_armature(void);
void	remake_editArmature(void);
void	selectconnected_armature(void);
void	selectconnected_posearmature(void);
void	select_bone_parent(void);
void	setflag_armature(short mode);
void    unique_editbone_name (struct ListBase *ebones, char *name);

void	auto_align_armature(short mode);

void	create_vgroups_from_armature(struct Object *ob, struct Object *par);
void 	add_verts_to_dgroups(struct Object *ob, struct Object *par, int heat, int mirror);

void	hide_selected_pose_bones(void);
void	hide_unselected_pose_bones(void);
void	show_all_pose_bones(void);

int		bone_looper(struct Object *ob, struct Bone *bone, void *data,
				int (*bone_func)(struct Object *, struct Bone *, void *));

void	undo_push_armature(char *name);
void	armature_bone_rename(struct bArmature *arm, char *oldname, char *newname);
void	armature_flip_names(void);
void 	armature_autoside_names(short axis);
EditBone *armature_bone_get_mirrored(EditBone *ebo);
void	transform_armature_mirror_update(void);

void	hide_selected_armature_bones(void);
void	hide_unselected_armature_bones(void);
void	show_all_armature_bones(void);
void	set_locks_armature_bones(short lock);

#define	BONESEL_ROOT	0x10000000
#define	BONESEL_TIP		0x20000000
#define	BONESEL_BONE	0x40000000
#define BONESEL_ANY		(BONESEL_TIP|BONESEL_ROOT|BONESEL_BONE)

#define BONESEL_NOSEL	0x80000000	/* Indicates a negative number */

#endif


