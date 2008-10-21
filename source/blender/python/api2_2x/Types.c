/* 
 * $Id$
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Willian P. Germano, Alex Mole, Joseph Gilbert
 *
 * ***** END GPL LICENSE BLOCK *****
*/

#include "Types.h"
#include "IDProp.h"
#include "gen_utils.h"
#include "BLI_blenlib.h"
/* 
   stuff pasted from the old Types.h
   is only need here now
*/

extern PyTypeObject IDGroup_Type, IDArray_Type;
extern PyTypeObject Action_Type, Armature_Type;
extern PyTypeObject Pose_Type;
extern PyTypeObject BezTriple_Type, Bone_Type, Button_Type;
extern PyTypeObject Camera_Type;
extern PyTypeObject CurNurb_Type, SurfNurb_Type;
extern PyTypeObject Curve_Type;
extern PyTypeObject Effect_Type, Font_Type;
extern PyTypeObject Image_Type, Ipo_Type, IpoCurve_Type;
extern PyTypeObject Lamp_Type, Lattice_Type;
extern PyTypeObject Material_Type, Metaball_Type, MTex_Type;
extern PyTypeObject NMFace_Type, NMEdge_Type, NMVert_Type, NMCol_Type,
	   NMesh_Type;
extern PyTypeObject MFace_Type, MVert_Type, PVert_Type, MEdge_Type, MCol_Type,
	   Mesh_Type;

extern PyTypeObject Object_Type;
extern PyTypeObject Group_Type;
extern PyTypeObject Particle_Type;
extern PyTypeObject Scene_Type, RenderData_Type;
extern PyTypeObject Text_Type, Text3d_Type, Texture_Type;
extern PyTypeObject World_Type;
extern PyTypeObject property_Type;
extern PyTypeObject buffer_Type, constant_Type, euler_Type;
extern PyTypeObject matrix_Type, quaternion_Type, rgbTuple_Type, vector_Type;
extern PyTypeObject point_Type;
extern PyTypeObject Modifier_Type, ModSeq_Type;
extern PyTypeObject EditBone_Type;
extern PyTypeObject ThemeSpace_Type;
extern PyTypeObject ThemeUI_Type;
extern PyTypeObject TimeLine_Type;

/* includes to get structs for CSizeof */
#include "Armature.h"
#include "Bone.h"
#include "BezTriple.h"
#include "Camera.h"
#include "Constraint.h"
#include "Curve.h"
#include "CurNurb.h"
#include "Draw.h"
#include "Effect.h"
#include "Ipo.h"
#include "Ipocurve.h"
#include "Key.h"
#include "Lamp.h"
#include "Lattice.h"
#include "Library.h"
#include "Mathutils.h"
#include "Geometry.h"
#include "Mesh.h"
#include "Metaball.h"
#include "Modifier.h"
#include "NMesh.h"
#include "Node.h"
#include "Object.h"
#include "Group.h"
#include "Registry.h"
#include "Scene.h"
#include "Sound.h"
#include "SurfNurb.h"
#include "Sys.h"
#include "Text.h"
#include "Texture.h"
#include "Window.h"
#include "World.h"
#include "Particle.h"

char M_Types_doc[] = "The Blender Types module\n\n\
This module is a dictionary of all Blender Python types";

static PyObject *Types_CSizeof(PyObject * self, PyObject *o)
{
	int ret = 0;
	char type[32];

	if(o) {
		BLI_snprintf(type, 32, "%s", PyString_AsString(PyObject_Str(o)));

		if(BLI_streq(type, "<type 'Blender Action'>")==1) {
			ret = sizeof(struct bAction);
		} else if (BLI_streq(type, "<type 'Armature'>")==1) {
			ret = sizeof(struct bArmature);
		} else if (BLI_streq(type, "<type 'BezTriple'>")==1) {
			ret = sizeof(struct BezTriple);
		} else if (BLI_streq(type, "<type 'Bone'>")==1) {
			ret = sizeof(struct Bone);
		} else if (BLI_streq(type, "<type 'Blender Camera'>")==1) {
			ret = sizeof(struct Camera);
		} else if (BLI_streq(type, "<type 'CurNurb'>")==1) {
			ret = sizeof(struct Nurb);
		} else if (BLI_streq(type, "<type 'Curve'>")==1) {
			ret = sizeof(struct Curve);
		} else if (BLI_streq(type, "<type 'Blender Group'>")==1) {
			ret = sizeof(struct Group);
		} else if (BLI_streq(type, "<type 'Blender IDProperty'>")==1) {
			ret = sizeof(struct IDProperty);
		} else if (BLI_streq(type, "<type 'Blender Image'>")==1) {
			ret = sizeof(struct Image);
		} else if (BLI_streq(type, "<type 'Blender Ipo'>")==1) {
			ret = sizeof(struct Ipo);
		} else if (BLI_streq(type, "<type 'IpoCurve'>")==1) {
			ret = sizeof(struct IpoCurve);
		} else if (BLI_streq(type, "<type 'Blender Lamp'>")==1) {
			ret = sizeof(struct Lamp);
		} else if (BLI_streq(type, "<type 'Blender Lattice'>")==1) {
			ret = sizeof(struct Lattice);
		} else if (BLI_streq(type, "<type 'Blender MCol'>")==1) {
			ret = sizeof(struct MCol);
		} else if (BLI_streq(type, "<type 'Blender MEdge'>")==1) {
			ret = sizeof(struct MEdge);
		} else if (BLI_streq(type, "<type 'Blender MFace'>")==1) {
			ret = sizeof(struct MFace);
		} else if (BLI_streq(type, "<type 'Blender MTex'>")==1) {
			ret = sizeof(struct MTex);
		} else if (BLI_streq(type, "<type 'Blender MVert'>")==1) {
			ret = sizeof(struct MVert);
		} else if (BLI_streq(type, "<type 'Blender Material'>")==1) {
			ret = sizeof(struct Material);
		} else if (BLI_streq(type, "<type 'Blender Mesh'>")==1) {
			ret = sizeof(struct Mesh);
		} else if (BLI_streq(type, "<type 'Blender Metaball'>")==1) {
			ret = sizeof(struct MetaBall);
		} else if (BLI_streq(type, "<type 'Blender.Modifiers'>")==1) {
			ret = sizeof(struct ModifierData);
		} else if (BLI_streq(type, "<type 'Blender Modifier'>")==1) {
			ret = sizeof(struct ModifierData);
		} else if (BLI_streq(type, "<type 'Blender Object'>")==1) {
			ret = sizeof(struct Object);
		} else if (BLI_streq(type, "<type 'Pose'>")==1) {
			ret = sizeof(struct bPose);
		} else if (BLI_streq(type, "<type 'Blender RenderData'>")==1) {
			ret = sizeof(struct RenderData);
		} else if (BLI_streq(type, "<type 'Scene'>")==1) {
			ret = sizeof(struct Scene);
		} else if (BLI_streq(type, "<type 'SurfNurb'>")==1) {
			ret = sizeof(struct Nurb);
		} else if (BLI_streq(type, "<type 'Text3d'>")==1) {
			ret = sizeof(struct Curve);
		} else if (BLI_streq(type, "<type 'Blender Text'>")==1) {
			ret = sizeof(struct Text);
		} else if (BLI_streq(type, "<type 'Blender Texture'>")==1) {
			ret = sizeof(struct Tex);
		} else {
			ret = -1;
		}
	}
	
	return PyInt_FromLong(ret);
}

struct PyMethodDef M_Types_methods[] = {
	{"CSizeof", Types_CSizeof, METH_O, 
		"(type) - Returns sizeof of the underlying C structure of the given type"},
	{NULL, NULL, 0, NULL}
};



/* The internal types (lowercase first letter, like constant_Type) are only
 * set when some object initializes them.  But unless we do it early, we get
 * some easy and unpredictable (varies with platform, even distro) ways to
 * crash Blender.  Some modules also need this early up, so let's generalize
 * and init all our pytypes here. 
 */

void types_InitAll( void )
{
	Action_Type.ob_type = &PyType_Type;
	Pose_Type.ob_type = &PyType_Type;
	Armature_Type.ob_type = &PyType_Type;
	BezTriple_Type.ob_type = &PyType_Type;
	Bone_Type.ob_type = &PyType_Type;
	Button_Type.ob_type = &PyType_Type;
	Camera_Type.ob_type = &PyType_Type;
	CurNurb_Type.ob_type = &PyType_Type;
	Curve_Type.ob_type = &PyType_Type;
	Effect_Type.ob_type = &PyType_Type;
	Image_Type.ob_type = &PyType_Type;
	Ipo_Type.ob_type = &PyType_Type;
	IpoCurve_Type.ob_type = &PyType_Type;
	Lamp_Type.ob_type = &PyType_Type;
	Lattice_Type.ob_type = &PyType_Type;
	Material_Type.ob_type = &PyType_Type;
	Metaball_Type.ob_type = &PyType_Type;
	MTex_Type.ob_type = &PyType_Type;
	NMCol_Type.ob_type = &PyType_Type;
	NMFace_Type.ob_type = &PyType_Type;
	NMEdge_Type.ob_type = &PyType_Type;
	NMVert_Type.ob_type = &PyType_Type;
	NMesh_Type.ob_type = &PyType_Type;
	MFace_Type.ob_type = &PyType_Type;
   	MVert_Type.ob_type = &PyType_Type;
   	PVert_Type.ob_type = &PyType_Type;
   	MEdge_Type.ob_type = &PyType_Type;
   	MCol_Type.ob_type = &PyType_Type;
   	Mesh_Type.ob_type = &PyType_Type;
	Object_Type.ob_type = &PyType_Type;
	Group_Type.ob_type = &PyType_Type;
	RenderData_Type.ob_type = &PyType_Type;
	Scene_Type.ob_type = &PyType_Type;
	SurfNurb_Type.ob_type = &PyType_Type;
	Text_Type.ob_type = &PyType_Type;
	Text3d_Type.ob_type = &PyType_Type;
	Texture_Type.ob_type = &PyType_Type;
	//TimeLine_Type.ob_type = &PyType_Type;
	World_Type.ob_type = &PyType_Type;
	buffer_Type.ob_type = &PyType_Type;
	constant_Type.ob_type = &PyType_Type;
	euler_Type.ob_type = &PyType_Type;
	matrix_Type.ob_type = &PyType_Type;
	quaternion_Type.ob_type = &PyType_Type;
	PyType_Ready( &rgbTuple_Type );
	vector_Type.ob_type = &PyType_Type;
	property_Type.ob_type = &PyType_Type;
	point_Type.ob_type = &PyType_Type;
	PyType_Ready( &Modifier_Type );
	PyType_Ready( &ModSeq_Type );
	PyType_Ready( &EditBone_Type );
	PyType_Ready( &ThemeSpace_Type );
	PyType_Ready( &ThemeUI_Type );
	IDProp_Init_Types();
}

/*****************************************************************************/
/* Function:	 Types_Init					 	*/
/*****************************************************************************/
PyObject *Types_Init( void )
{
	PyObject *submodule, *dict;

	submodule =
		Py_InitModule3( "Blender.Types", M_Types_methods, M_Types_doc );

	dict = PyModule_GetDict( submodule );

	/* The Blender Object Type */

	PyDict_SetItemString( dict, "ObjectType",
			      ( PyObject * ) &Object_Type );

	/* Blender Object Data Types */

	PyDict_SetItemString( dict, "GroupType",
			      ( PyObject * ) &Group_Type );

	PyDict_SetItemString( dict, "SceneType", ( PyObject * ) &Scene_Type );
	PyDict_SetItemString( dict, "RenderDataType",
			      ( PyObject * ) &RenderData_Type );

	PyDict_SetItemString( dict, "NMeshType", ( PyObject * ) &NMesh_Type );
	PyDict_SetItemString( dict, "NMFaceType",
			      ( PyObject * ) &NMFace_Type );
	PyDict_SetItemString( dict, "NMVertType",
			      ( PyObject * ) &NMVert_Type );
	PyDict_SetItemString( dict, "NMEdgeType",
			      ( PyObject * ) &NMEdge_Type );
	PyDict_SetItemString( dict, "NMColType", ( PyObject * ) &NMCol_Type );

	PyDict_SetItemString( dict, "MeshType", ( PyObject * ) &Mesh_Type );
	PyDict_SetItemString( dict, "MFaceType",
			      ( PyObject * ) &MFace_Type );
	PyDict_SetItemString( dict, "MEdgeType",
			      ( PyObject * ) &MEdge_Type );
	PyDict_SetItemString( dict, "MVertType",
			      ( PyObject * ) &MVert_Type );
	PyDict_SetItemString( dict, "PVertType",
			      ( PyObject * ) &PVert_Type );
	PyDict_SetItemString( dict, "MColType", ( PyObject * ) &MCol_Type );

	PyDict_SetItemString( dict, "ArmatureType",
			      ( PyObject * ) &Armature_Type );
	PyDict_SetItemString( dict, "BoneType", ( PyObject * ) &Bone_Type );

	PyDict_SetItemString( dict, "CurNurbType",
			      ( PyObject * ) &CurNurb_Type );
	PyDict_SetItemString( dict, "SurfNurbType",
			      ( PyObject * ) &SurfNurb_Type );
	PyDict_SetItemString( dict, "CurveType", ( PyObject * ) &Curve_Type );

	PyDict_SetItemString( dict, "IpoType", ( PyObject * ) &Ipo_Type );
	PyDict_SetItemString( dict, "IpoCurveType", ( PyObject * ) &IpoCurve_Type );
	PyDict_SetItemString( dict, "MetaballType",
			      ( PyObject * ) &Metaball_Type );

	PyDict_SetItemString( dict, "CameraType",
			      ( PyObject * ) &Camera_Type );
	PyDict_SetItemString( dict, "ImageType", ( PyObject * ) &Image_Type );
	PyDict_SetItemString( dict, "LampType", ( PyObject * ) &Lamp_Type );
	PyDict_SetItemString( dict, "TextType", ( PyObject * ) &Text_Type );
	PyDict_SetItemString( dict, "Text3dType", ( PyObject * ) &Text3d_Type );
	PyDict_SetItemString( dict, "MaterialType",
			      ( PyObject * ) &Material_Type );

	PyDict_SetItemString( dict, "ButtonType",
			      ( PyObject * ) &Button_Type );

	PyDict_SetItemString( dict, "LatticeType",
			      ( PyObject * ) &Lattice_Type );

	PyDict_SetItemString( dict, "TextureType",
			      ( PyObject * ) &Texture_Type );
	PyDict_SetItemString( dict, "MTexType", ( PyObject * ) &MTex_Type );

	/* External helper Types available to the main ones above */

	PyDict_SetItemString( dict, "vectorType",
			      ( PyObject * ) &vector_Type );
	PyDict_SetItemString( dict, "bufferType",
			      ( PyObject * ) &buffer_Type );
	PyDict_SetItemString( dict, "constantType",
			      ( PyObject * ) &constant_Type );
	PyDict_SetItemString( dict, "rgbTupleType",
			      ( PyObject * ) &rgbTuple_Type );
	PyDict_SetItemString( dict, "matrixType",
			      ( PyObject * ) &matrix_Type );
	PyDict_SetItemString( dict, "eulerType", ( PyObject * ) &euler_Type );
	PyDict_SetItemString( dict, "quaternionType",
			      ( PyObject * ) &quaternion_Type );
	PyDict_SetItemString( dict, "BezTripleType",
			      ( PyObject * ) &BezTriple_Type );
	PyDict_SetItemString( dict, "ActionType",
			      ( PyObject * ) &Action_Type );
	PyDict_SetItemString( dict, "PoseType",
			      ( PyObject * ) &Pose_Type );
	PyDict_SetItemString( dict, "propertyType",
			      ( PyObject * ) &property_Type );
	PyDict_SetItemString( dict, "pointType",
			      ( PyObject * ) &point_Type );
	PyDict_SetItemString( dict, "ModifierType",
			      ( PyObject * ) &Modifier_Type );
	PyDict_SetItemString( dict, "ModSeqType",
			      ( PyObject * ) &ModSeq_Type );
	PyDict_SetItemString( dict, "EditBoneType",
			      ( PyObject * ) &EditBone_Type);
	PyDict_SetItemString( dict, "ThemeSpaceType",
			      ( PyObject * ) &ThemeSpace_Type);
	PyDict_SetItemString( dict, "ThemeUIType",
			      ( PyObject * ) &ThemeUI_Type);
	PyDict_SetItemString( dict, "IDGroupType",
			      ( PyObject * ) &IDGroup_Type);
	PyDict_SetItemString( dict, "IDArrayType",
			      ( PyObject * ) &IDArray_Type);
	return submodule;
}
