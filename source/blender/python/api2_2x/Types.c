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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Willian P. Germano, Alex Mole
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include "Types.h"

struct PyMethodDef Null_methods[] = {{NULL, NULL}};

/*****************************************************************************/
/* Function:             Types_Init                                          */
/*****************************************************************************/
PyObject *Types_Init (void)
{
  PyObject  *submodule, *dict;

 /* These are only set when some object initializes them.  But unless we
	* do it now, we get an easy way to crash Blender. Maybe we'd better
	* have an Init function for all these internal types that more than one
	* module can use.  We could call it after setting the Blender dictionary */
  vector_Type.ob_type = &PyType_Type;
  rgbTuple_Type.ob_type = &PyType_Type;
	constant_Type.ob_type = &PyType_Type;
	buffer_Type.ob_type = &PyType_Type;
	Button_Type.ob_type = &PyType_Type;
	BezTriple_Type.ob_type = &PyType_Type;

	/* Another one that needs to be here: */
	Text_Type.ob_type = &PyType_Type;

  Texture_Type.ob_type = &PyType_Type;
	MTex_Type.ob_type = &PyType_Type;

  submodule = Py_InitModule3 ("Blender.Types", Null_methods, M_Types_doc);

  dict = PyModule_GetDict(submodule);

  /* The Blender Object Type */

  PyDict_SetItemString(dict, "ObjectType",   (PyObject *)&Object_Type);

  /* Blender Object Data Types */

  PyDict_SetItemString(dict, "SceneType",   (PyObject *)&Scene_Type);

  PyDict_SetItemString(dict, "NMeshType",    (PyObject *)&NMesh_Type);
  PyDict_SetItemString(dict, "NMFaceType",   (PyObject *)&NMFace_Type);
  PyDict_SetItemString(dict, "NMVertType",   (PyObject *)&NMVert_Type);
  PyDict_SetItemString(dict, "NMColType",    (PyObject *)&NMCol_Type);

  PyDict_SetItemString(dict, "ArmatureType", (PyObject *)&Armature_Type);
  PyDict_SetItemString(dict, "BoneType",     (PyObject *)&Bone_Type);

  PyDict_SetItemString(dict, "CurveType",    (PyObject *)&Curve_Type);
  PyDict_SetItemString(dict, "IpoType",      (PyObject *)&Ipo_Type);
  PyDict_SetItemString(dict, "MetaballType", (PyObject *)&Metaball_Type);

  PyDict_SetItemString(dict, "CameraType",   (PyObject *)&Camera_Type);
  PyDict_SetItemString(dict, "ImageType",    (PyObject *)&Image_Type);
  PyDict_SetItemString(dict, "LampType",     (PyObject *)&Lamp_Type);
  PyDict_SetItemString(dict, "TextType",     (PyObject *)&Text_Type);
  PyDict_SetItemString(dict, "MaterialType", (PyObject *)&Material_Type);

  PyDict_SetItemString(dict, "ButtonType",   (PyObject *)&Button_Type);

  PyDict_SetItemString(dict, "LatticeType",  (PyObject *)&Lattice_Type);

  PyDict_SetItemString(dict, "TextureType",  (PyObject *)&Texture_Type);
  PyDict_SetItemString(dict, "MTexType",     (PyObject *)&MTex_Type);

  /* External helper Types available to the main ones above */

  PyDict_SetItemString(dict, "vectorType",   (PyObject *)&vector_Type);
  PyDict_SetItemString(dict, "bufferType",   (PyObject *)&buffer_Type);
  PyDict_SetItemString(dict, "constantType", (PyObject *)&constant_Type);
  PyDict_SetItemString(dict, "rgbTupleType", (PyObject *)&rgbTuple_Type);
  PyDict_SetItemString(dict, "BezTripleType", (PyObject *)&BezTriple_Type);

  return submodule;
}
