/* 
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Willian P. Germano, Alex Mole, Joseph Gilbert
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include "Types.h"

char M_Types_doc[] =
"The Blender Types module\n\n\
This module is a dictionary of all Blender Python types";

struct PyMethodDef Null_methods[] = {{NULL, NULL}};

void types_InitAll(void)
{
 /* The internal types (lowercase first letter, like constant_Type) are only
	* set when some object initializes them.  But unless we do it early, we get
	* some easy and unpredictable (varies with platform, even distro) ways to
	* crash Blender.  Some modules also need this early up, so let's generalize
	* and init all our pytypes here. */

	Action_Type.ob_type = &PyType_Type;
	Armature_Type.ob_type = &PyType_Type;
	BezTriple_Type.ob_type = &PyType_Type;
	Bone_Type.ob_type = &PyType_Type;
	Build_Type.ob_type = &PyType_Type;
	Button_Type.ob_type = &PyType_Type;
	Camera_Type.ob_type = &PyType_Type;
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
	NMVert_Type.ob_type = &PyType_Type;
	NMesh_Type.ob_type = &PyType_Type;
	Object_Type.ob_type = &PyType_Type;
	Particle_Type.ob_type = &PyType_Type;
	Scene_Type.ob_type = &PyType_Type;
	Text_Type.ob_type = &PyType_Type;
	Texture_Type.ob_type = &PyType_Type;
	Wave_Type.ob_type = &PyType_Type;
	World_Type.ob_type = &PyType_Type;
	buffer_Type.ob_type = &PyType_Type;
	constant_Type.ob_type = &PyType_Type;
	euler_Type.ob_type = &PyType_Type;
	matrix_Type.ob_type = &PyType_Type;
	quaternion_Type.ob_type = &PyType_Type;
	rgbTuple_Type.ob_type = &PyType_Type;
	vector_Type.ob_type = &PyType_Type;
}

/*****************************************************************************/
/* Function:						 Types_Init																					 */
/*****************************************************************************/
PyObject *Types_Init (void)
{
	PyObject	*submodule, *dict;

	submodule = Py_InitModule3 ("Blender.Types", Null_methods, M_Types_doc);

	dict = PyModule_GetDict(submodule);

	/* The Blender Object Type */

	PyDict_SetItemString(dict, "ObjectType",	 (PyObject *)&Object_Type);

	/* Blender Object Data Types */

	PyDict_SetItemString(dict, "SceneType",		(PyObject *)&Scene_Type);

	PyDict_SetItemString(dict, "NMeshType",		 (PyObject *)&NMesh_Type);
	PyDict_SetItemString(dict, "NMFaceType",	 (PyObject *)&NMFace_Type);
	PyDict_SetItemString(dict, "NMVertType",	 (PyObject *)&NMVert_Type);
	PyDict_SetItemString(dict, "NMColType",		 (PyObject *)&NMCol_Type);

	PyDict_SetItemString(dict, "ArmatureType", (PyObject *)&Armature_Type);
	PyDict_SetItemString(dict, "BoneType",		 (PyObject *)&Bone_Type);

	PyDict_SetItemString(dict, "CurveType",		 (PyObject *)&Curve_Type);
	PyDict_SetItemString(dict, "IpoType",			 (PyObject *)&Ipo_Type);
	PyDict_SetItemString(dict, "MetaballType", (PyObject *)&Metaball_Type);

	PyDict_SetItemString(dict, "CameraType",	 (PyObject *)&Camera_Type);
	PyDict_SetItemString(dict, "ImageType",		 (PyObject *)&Image_Type);
	PyDict_SetItemString(dict, "LampType",		 (PyObject *)&Lamp_Type);
	PyDict_SetItemString(dict, "TextType",		 (PyObject *)&Text_Type);
	PyDict_SetItemString(dict, "MaterialType", (PyObject *)&Material_Type);

	PyDict_SetItemString(dict, "ButtonType",	 (PyObject *)&Button_Type);

	PyDict_SetItemString(dict, "LatticeType",  (PyObject *)&Lattice_Type);

	PyDict_SetItemString(dict, "TextureType",  (PyObject *)&Texture_Type);
	PyDict_SetItemString(dict, "MTexType",		 (PyObject *)&MTex_Type);

	/* External helper Types available to the main ones above */

	PyDict_SetItemString(dict, "vectorType",	 (PyObject *)&vector_Type);
	PyDict_SetItemString(dict, "bufferType",	 (PyObject *)&buffer_Type);
	PyDict_SetItemString(dict, "constantType", (PyObject *)&constant_Type);
	PyDict_SetItemString(dict, "rgbTupleType", (PyObject *)&rgbTuple_Type);
	PyDict_SetItemString(dict, "matrix_Type",  (PyObject *)&matrix_Type);
	PyDict_SetItemString(dict, "eulerType",  (PyObject *)&euler_Type);
	PyDict_SetItemString(dict, "quaternionType",	(PyObject *)&quaternion_Type);
	PyDict_SetItemString(dict, "BezTripleType", (PyObject *)&BezTriple_Type);
	PyDict_SetItemString(dict, "ActionType", (PyObject *)&Action_Type);

	return submodule;
}
