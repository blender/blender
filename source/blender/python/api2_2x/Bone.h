/* 
 * $Id: Bone.h 12399 2007-10-26 08:19:40Z campbellbarton $
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
 * Contributor(s): Joseph Gilbert
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#ifndef EXPP_BONE_H
#define EXPP_BONE_H

#include <Python.h>
#include "DNA_armature_types.h"

/*-------------------TYPE CHECKS---------------------------------*/
#define BoneObject_Check(v) ((v)->ob_type == &Bone_Type)
#define EditBoneObject_Check(v) ((v)->ob_type == &EditBone_Type)
/*-------------------TYPEOBJECT----------------------------------*/
extern PyTypeObject EditBone_Type;
extern PyTypeObject Bone_Type;
/*-------------------STRUCT DEFINITION----------------------------*/

typedef struct {
	PyObject_HEAD
	Bone * bone;
} BPy_Bone;

typedef struct {
	PyObject_HEAD
	struct EditBone *editbone;
	struct EditBone *parent;
	char name[32];
	float roll;
	float head[3];
	float tail[3];
	int flag;
	float dist;
	float weight;
	float xwidth;
	float zwidth;
	float ease1;
	float ease2;
	float rad_head;
	float rad_tail;
	short segments;
	short layer;
} BPy_EditBone;
/*-------------------VISIBLE PROTOTYPES-------------------------*/
PyObject *PyBone_FromBone(struct Bone *bone);
struct Bone *PyBone_AsBone(BPy_Bone *py_Bone);
PyObject *PyEditBone_FromBone(Bone *bone);
PyObject *PyEditBone_FromEditBone(struct EditBone *editbone);

#endif
