/* 
 * $Id: 
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

#ifndef EXPP_POSE_H
#define EXPP_POSE_H

#include <Python.h>
#include "DNA_action_types.h"
#include "DNA_object_types.h"

//-------------------TYPE CHECKS---------------------------------
#define BPy_Pose_Check(v) ((v)->ob_type == &Pose_Type)
#define BPy_PoseBone_Check(v) ((v)->ob_type == &PoseBone_Type)
#define BPy_PoseBonesDict_Check(v) ((v)->ob_type == &PoseBonesDict_Type)
//-------------------TYPEOBJECT----------------------------------
extern PyTypeObject Pose_Type;
extern PyTypeObject PoseBone_Type;
extern PyTypeObject PoseBonesDict_Type;
//-------------------STRUCT DEFINITION----------------------------
typedef struct {
	PyObject_HEAD 
	PyObject *bonesMap;
	ListBase *bones;  
} BPy_PoseBonesDict;

typedef struct {
	PyObject_HEAD
	bPose *pose;
	char name[24];   //because poses have not names :(
	BPy_PoseBonesDict *Bones; 
} BPy_Pose;

typedef struct {
	PyObject_HEAD
	bPoseChannel *posechannel;
	
} BPy_PoseBone;

//-------------------VISIBLE PROTOTYPES-------------------------
PyObject *Pose_Init(void);
PyObject *PyPose_FromPose(bPose *pose, char *name);
PyObject *PyPoseBone_FromPosechannel(bPoseChannel *pchan);
Object *Object_FromPoseChannel(bPoseChannel *curr_pchan);
#endif
