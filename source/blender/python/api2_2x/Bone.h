/* 
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
 * Contributor(s): Joseph Gilbert
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#ifndef EXPP_BONE_H
#define EXPP_BONE_H

#include <Python.h>
#include "DNA_armature_types.h"

//-------------------TYPE CHECKS---------------------------------
#define BoneObject_Check(v) ((v)->ob_type == &Bone_Type)
#define EditBoneObject_Check(v) ((v)->ob_type == &EditBone_Type)
//-------------------TYPEOBJECT----------------------------------
PyTypeObject EditBone_Type;
PyTypeObject Bone_Type;
//-------------------STRUCT DEFINITION----------------------------
typedef struct {
	PyObject_HEAD
	Bone * bone;
} BPy_Bone;

typedef struct {
	PyObject_HEAD
	struct Bone *temp; //temp tracking
	char parent[32];
	char name[32];
	float roll;
	float head[3];
	float tail[3];
	int flag;
	float length;
	float dist;
	float weight;
	float xwidth;
	float zwidth;
	float ease1;
	float ease2;
	float rad_head;
	float rad_tail;
	short segments;
} BPy_EditBone;
//-------------------VISIBLE PROTOTYPES-------------------------
PyObject *PyBone_FromBone(struct Bone *bone);
struct Bone *PyBone_AsBone(BPy_Bone *py_Bone);

#endif

























/*
#ifndef EXPP_BONE_H
#define EXPP_BONE_H

#include <Python.h>
#include "DNA_armature_types.h"
#include "Mathutils.h"

//--------------------------Python BPy_Bone structure definition.-------
typedef struct {
	PyObject_HEAD
		//reference to data if bone is linked to an armature
	Bone * bone;
	//list of vars that define the boneclass
	char *name;
	char *parent;
	float roll;
	int flag;
	int boneclass;
	float dist;
	float weight;
	VectorObject *head;
	VectorObject *tail;
	VectorObject *loc;
	VectorObject *dloc;
	VectorObject *size;
	VectorObject *dsize;
	QuaternionObject *quat;
	QuaternionObject *dquat;
	MatrixObject *obmat;
	MatrixObject *parmat;
	MatrixObject *defmat;
	MatrixObject *irestmat;
	MatrixObject *posemat;
} BPy_Bone;

//------------------------------visible prototypes----------------------
PyObject *Bone_CreatePyObject( struct Bone *obj );
int Bone_CheckPyObject( PyObject * py_obj );
Bone *Bone_FromPyObject( PyObject * py_obj );
PyObject *Bone_Init( void );
int updateBoneData( BPy_Bone * self, Bone * parent );

#endif

*/
