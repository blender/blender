/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
#ifndef BIF_EDITARMATURE_H
#define BIF_EDITARMATURE_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

struct Object;
struct Bone;
struct bArmature;

typedef struct EditBone
{
	struct EditBone *next, *prev;
	struct EditBone *parent;/*	Editbones have a one-way link  (i.e. children refer
									to parents.  This is converted to a two-way link for
									normal bones when leaving editmode.	*/
	void	*temp;		/*	Used to store temporary data */

	char	name[32];
	char	oldname[32];
	float	roll;			/*	Roll along axis.  We'll ultimately use the axis/angle method
								for determining the transformation matrix of the bone.  The axis
								is tail-head while roll provides the angle. Refer to Graphics
								Gems 1 p. 466 (section IX.6) if it's not already in here somewhere*/

	float	head[3];			/*	Orientation and length is implicit during editing */
	float	tail[3];	
							/*	All joints are considered to have zero rotation with respect to
							their parents.	Therefore any rotations specified during the
							animation are automatically relative to the bones' rest positions*/
	short	sHead[2];
	short	sTail[2];
	int		flag;

	int		parNr;		/* Used for retrieving values from the menu system */
	/*	Storage for transformation data used by the posing system.
		Maybe a better solution would be to make bones a blenderObject
		variant?  Or perhaps to use a TOTALLY custom transformation scheme
		for pose element, rather than trying to use the existing transObject
		system?
	*/
	float dist, weight;
	float loc[3], dloc[3];
	float size[3], dsize[3];
	float rot[3], drot[3];
	float quat[4], dquat[4];
	float obmat[4][4];


} EditBone;


void	adduplicate_armature(void);
void	addvert_armature(void);
void	add_primitiveArmature(int type);
void	apply_rot_armature (struct Object *ob, float mat[3][3]);
void	armaturebuts(void);
void	clear_armature(struct Object *ob, char mode);
void	clever_numbuts_armature (void);
void	clever_numbuts_posearmature (void);
void	delete_armature(void);
void	deselectall_armature(void);
void	deselectall_posearmature (int test);
void	draw_armature(struct Object *ob);
void	extrude_armature(void);
void	free_editArmature(void);
void	join_armature(void);
void	load_editArmature(void);
char*	make_bone_menu(struct bArmature *arm);
void	make_bone_parent(void);
void	make_editArmature(void);
void	make_trans_bones (char mode);
void	mousepose_armature(void);
void	mouse_armature(void);
void	remake_editArmature(void);
void	selectconnected_armature(void);
void	selectconnected_posearmature(void);
void	select_bone_by_name (struct bArmature *arm, char *name, int select);
struct	Bone *get_first_selected_bone (void);
void auto_align_armature(void);

#define	BONESEL_TIP		0x08000000
#define	BONESEL_ROOT	0x04000000
#define BONESEL_BONE	(BONESEL_TIP|BONESEL_ROOT)
#define BONESEL_NOSEL	0x80000000	/* Indicates a negative number */

#endif

