
/** CSG wrapper module 
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
  * $Id$
  */

// TODO

#include "Python.h" 
#include "BPY_csg.h"
#include "BKE_booleanops_mesh.h"                    
#include "BKE_booleanops.h"                      
#include "MEM_guardedalloc.h"     
#include "b_interface.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef DEBUG
	#define CSG_DEBUG(str)   \
			{ printf str; }   
#else
	#define CSG_DEBUG(str)   \
		{}
#endif


///////////////////////////////////////////////////////////////
// CSG python object struct


typedef struct _CSGMesh {
	PyObject_VAR_HEAD
 	CSG_MeshDescriptor *imesh;
} PyCSGMesh;

// PROTOS

static PyObject *newPyCSGMesh(CSG_MeshDescriptor *imesh);

static void CSGMesh_dealloc(PyObject *self);
static PyObject *CSGMesh_getattr(PyObject *self, char *attr);


static char CSGMesh_Type_doc[] = "CSG mesh type"; 

static PyTypeObject PyCSGMesh_Type = {
	PyObject_HEAD_INIT(NULL)
	0,                                /*ob_size*/
	"CSGMesh",                        /*tp_name*/
	sizeof(PyCSGMesh),                  /*tp_basicsize*/
	0,                                /*tp_itemsize*/
	/* methods */
	(destructor) CSGMesh_dealloc,     /*tp_dealloc*/
	(printfunc)0,                     /*tp_print*/
	(getattrfunc)CSGMesh_getattr,     /*tp_getattr*/
	(setattrfunc)0,                   /*tp_setattr*/
	(cmpfunc)0,                       /*tp_compare*/
	(reprfunc)0,                      /*tp_repr*/
	0,                                /*tp_as_number*/
	0,                                /*tp_as_sequence*/
	0,                                /*tp_as_mapping*/
	(hashfunc)0,                      /*tp_hash*/
	(ternaryfunc)0,                   /*tp_call*/
	(reprfunc)0,                      /*tp_str*/

	/* Space for future expansion */
	0L,0L,0L,0L,
	CSGMesh_Type_doc /* Documentation string */
};

///////////////////////////////////////////////////////////////
// CSG object methods


static PyObject *CSGMesh_add(PyObject *self, PyObject *args)
{
	CSG_MeshDescriptor *new_imesh = 
	   (CSG_MeshDescriptor *) MEM_mallocN(sizeof(CSG_MeshDescriptor),
	                                      "CSG_IMesh");

	PyCSGMesh *c2;
	int success = 0;

	PyCSGMesh *c1 = (PyCSGMesh *) self;
	if (!PyArg_ParseTuple(args, "O!", &PyCSGMesh_Type, &c2)) return NULL;

	success = CSG_PerformOp(c1->imesh, c2->imesh, 2, new_imesh);

	if (!success) {
		PyErr_SetString(PyExc_RuntimeError, "Sorry. Didn't work");
		return NULL; // exception
	}
	return newPyCSGMesh(new_imesh);
}


static PyMethodDef CSGMesh_methods[] = {
	{"union", CSGMesh_add, METH_VARARGS, 0 },
	// add more methods here
	{NULL, NULL, 0, NULL}
};


static void CSGMesh_dealloc(PyObject *self)
{
	CSG_MeshDescriptor *imesh = ((PyCSGMesh *) self)->imesh;
	CSG_DEBUG(("object was destroyed\n"));
	// TODO: delete (free) struct ptr
	CSG_DestroyMeshDescriptor(imesh);
	MEM_freeN(imesh);
	PyMem_DEL(self);
}

static PyObject *CSGMesh_getattr(PyObject *self, char *attr)
{
	return Py_FindMethod(CSGMesh_methods, (PyObject *) self, attr);
}
///////////////////////////////////////////////////////////////
// CSG module methods

static PyObject *newPyCSGMesh(CSG_MeshDescriptor *imesh)
{
	PyCSGMesh *c = PyObject_NEW(PyCSGMesh, &PyCSGMesh_Type);
	CSG_DEBUG(("object was created\n"));
	c->imesh = imesh;
	// add init bla here
	return (PyObject *) c;
}

static PyObject *CSGmodule_CSGMesh(PyObject *self, PyObject *args)
{
	char *name;

	Object *obj;
	CSG_MeshDescriptor *new_imesh;

	if (!PyArg_ParseTuple(args, "s", &name)) return NULL;
	new_imesh = (CSG_MeshDescriptor *) MEM_mallocN(
	                                       sizeof(CSG_MeshDescriptor),
	                                      "CSG_IMesh");

	// get object by name, return its mesh data
	// and do the conversion
	//	CSG_LoadBlenderMesh(name, new_imesh);

	obj = (Object *) getFromList(getObjectList(), name);
	
	if (!obj) {
		PyErr_SetString(PyExc_AttributeError, 
		                "requested Object does not exist");
		return NULL;
	}	

	if (obj->type != OB_MESH) {
		PyErr_SetString(PyExc_TypeError, "Mesh object expected");
		return NULL;
	}

	if (!CSG_LoadBlenderMesh(obj, new_imesh)) {
		PyErr_SetString(PyExc_RuntimeError, 
		                "FATAL: Could not acquire mesh data");
		return NULL;
	}
	return newPyCSGMesh(new_imesh);
}

static PyObject *CSGmodule_toBlenderMeshObject(PyObject *self, 
                                               PyObject *args)
{
	Object *new_object;
	PyCSGMesh *pmesh;
	CSG_MeshDescriptor *c;

	float identity[4][4] = { {1.0, 0.0, 0.0, 0.0},
	                         {0.0, 1.0, 0.0, 0.0},
	                         {0.0, 0.0, 1.0, 0.0},
	                         {0.0, 0.0, 0.0, 1.0}};


	if (!PyArg_ParseTuple(args, "O!", &PyCSGMesh_Type, &pmesh)) return NULL;
	c = pmesh->imesh; 
	new_object = object_new(OB_MESH);

	if (!PyArg_ParseTuple(self, "")) return NULL;
	// TODO: blender mesh conversion
	ConvertCSGDescriptorsToMeshObject(new_object, &c->m_descriptor,
	                                  &c->m_face_iterator,
	                                  &c->m_vertex_iterator, 
	                                  identity);

	// return resulting object
	return DataBlock_fromData(new_object);
}

static PyMethodDef CSGmodule_methods[] = {
	{"CSGMesh",  CSGmodule_CSGMesh            , METH_VARARGS, 0 },
	{"toObject", CSGmodule_toBlenderMeshObject, METH_VARARGS, 0 },
	{NULL, NULL, 0, NULL}
};

// MODULE INITIALIZATION

void initcsg()
{
	PyObject *mod;
	PyCSGMesh_Type.ob_type = &PyType_Type;
	mod = Py_InitModule("csg", CSGmodule_methods); 	
							   
}

