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

struct Scene;
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

void ED_pose_deselectall(struct Object *ob, int test, int doundo);

void ED_armature_from_edit(struct Scene *scene, struct Object *obedit);
void ED_armature_to_edit(struct Object *ob);
void ED_armature_edit_free(struct Object *ob);
void ED_armature_edit_remake(struct Object *obedit);

#endif /* ED_ARMATURE_H */



