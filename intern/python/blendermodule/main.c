/**
 * $Id$
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

/***************************************************************************

                          main.c  -  description
                             -------------------
    begin                : Fri Sep 15 19:19:43 CEST 2000
    copyright            : (C) 2000 by Jan Walter
    email                : jan@blender.nl
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/* CVS */
/* $Author$ */
/* $Date$ */
/* $RCSfile$ */
/* $Revision$ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include "Python.h"

static PyObject* ErrorObject;
static PyObject* _scene;

static PyObject* blend_connect(PyObject* self, PyObject* args);

/**************/
/* structures */
/**************/

typedef struct {
  PyObject_HEAD
  char name[24];
  PyObject* vertices;
  PyObject* normals;
  PyObject* faces;
} mshobject;

staticforward PyTypeObject Mshtype;

typedef struct {
  PyObject_HEAD
  char name[24];
  PyObject* matrix;
  PyObject* data;
  PyObject* type;
} objobject;

staticforward PyTypeObject Objtype;

typedef struct {
  PyObject_HEAD
  char name[24];
  PyObject* objects;
} sceobject;

staticforward PyTypeObject Scetype;

/********/
/* mesh */
/********/

static char msh_addFace__doc__[] =
"addFace(self, i1, i2, i3, i4, isSmooth)"
;

static PyObject*
msh_addFace(mshobject* self, PyObject* args)
{
  int index;
  int i1, i2, i3, i4;
  int isSmooth;
  PyObject *item = NULL;

  if (!PyArg_ParseTuple(args, "iiiii", &i1, &i2, &i3, &i4, &isSmooth))
    {
      return NULL;
    }
  item = PyList_New(5);
  PyList_SetItem(item, 0, PyInt_FromLong(i1));
  PyList_SetItem(item, 1, PyInt_FromLong(i2));
  PyList_SetItem(item, 2, PyInt_FromLong(i3));
  PyList_SetItem(item, 3, PyInt_FromLong(i4));
  PyList_SetItem(item, 4, PyInt_FromLong(isSmooth));
  PyList_Append(self->faces, item);
  index = PyList_Size(self->faces) - 1;

  return PyInt_FromLong(index);
}

static char msh_addVertex__doc__[] =
"addVertex(self, x, y, z, nx, ny, nz)"
;

static PyObject*
msh_addVertex(mshobject* self, PyObject* args)
{
  int   index;
  float x, y, z, nx, ny, nz;
  PyObject *item1 = NULL;
  PyObject *item2 = NULL;

  if (!PyArg_ParseTuple(args, "ffffff", &x, &y, &z, &nx, &ny, &nz))
    {
      return NULL;
    }
  item1 = PyList_New(3);
  item2 = PyList_New(3);
  PyList_SetItem(item1, 0, PyFloat_FromDouble(x));
  PyList_SetItem(item1, 1, PyFloat_FromDouble(y));
  PyList_SetItem(item1, 2, PyFloat_FromDouble(z));
  PyList_SetItem(item2, 0, PyFloat_FromDouble(nx));
  PyList_SetItem(item2, 1, PyFloat_FromDouble(ny));
  PyList_SetItem(item2, 2, PyFloat_FromDouble(nz));
  PyList_Append(self->vertices, item1);
  PyList_Append(self->normals, item2);
  index = PyList_Size(self->vertices) - 1;

  return PyInt_FromLong(index);
}

static struct PyMethodDef msh_methods[] = {
  {"addFace",   (PyCFunction)msh_addFace,
   METH_VARARGS, msh_addFace__doc__},
  {"addVertex", (PyCFunction)msh_addVertex,
   METH_VARARGS, msh_addVertex__doc__},

  { NULL, NULL }
};

static mshobject*
newmshobject(char* name)
{
  mshobject* self;

  self = PyObject_NEW(mshobject, &Mshtype);
  if (self == NULL)
    {
      return NULL;
    }
  strcpy(self->name, name);
  self->vertices = PyList_New(0);
  self->normals = PyList_New(0);
  self->faces = PyList_New(0);

  return self;
}

static void
msh_dealloc(mshobject* self)
{
  mshobject* msh = (mshobject*) self;

  Py_DECREF(msh->vertices);
  Py_DECREF(msh->normals);
  Py_DECREF(msh->faces);

  PyMem_DEL(self);
}

static int
msh_print(mshobject* self, FILE* fp, int flags)
{
  fprintf(fp, "Mesh(name = \"%s\",\n", self->name);
  fprintf(fp, "     vertices = %d,\n", PyList_Size(self->vertices));
  fprintf(fp, "     faces = %d)\n", PyList_Size(self->faces));

  return 0;
}

static PyObject*
msh_repr(mshobject* self)
{
  PyObject* s;

  s = PyString_FromString("Mesh()\n");

  return s;
}

static PyObject*
msh_str(mshobject* self)
{
  PyObject* s;

  s = PyString_FromString("Mesh()\n");

  return s;
}

#include "structmember.h"

static struct memberlist msh_memberlist[] = {
  /* XXXX Add lines like { "foo", T_INT, OFF(foo), RO }  */
  {"vertices", T_OBJECT, offsetof(mshobject, vertices), RO},
  {"normals", T_OBJECT, offsetof(mshobject, normals), RO},
  {"faces", T_OBJECT, offsetof(mshobject, faces), RO},
  {NULL}
};

static PyObject*
msh_getattr(mshobject* self, char* name)
{
  PyObject* rv;

  /* XXXX Add your own getattr code here */
  rv = PyMember_Get((char*) self, msh_memberlist, name);
  if (rv)
    {
      return rv;
    }
  PyErr_Clear();

  return Py_FindMethod(msh_methods, (PyObject*)self, name);
}


static int
msh_setattr(mshobject* self, char* name, PyObject* v)
{
  /* XXXX Add your own setattr code here */
  if ( v == NULL )
    {
      PyErr_SetString(PyExc_AttributeError, "Cannot delete attribute");
      return -1;
    }

  return PyMember_Set((char*)/*XXXX*/0, msh_memberlist, name, v);
}

static char Mshtype__doc__[] =
""
;

static PyTypeObject Mshtype = {
  PyObject_HEAD_INIT(&PyType_Type)
  0,				/*ob_size*/
  "Mesh",			/*tp_name*/
  sizeof(mshobject),	/*tp_basicsize*/
  0,				/*tp_itemsize*/
  /* methods */
  (destructor) msh_dealloc,	/*tp_dealloc*/
  (printfunc) msh_print,	/*tp_print*/
  (getattrfunc) msh_getattr,	/*tp_getattr*/
  (setattrfunc) msh_setattr,	/*tp_setattr*/
  (cmpfunc) 0,	/*tp_compare*/
  (reprfunc) msh_repr,		/*tp_repr*/
  0,		/*tp_as_number*/
  0,		/*tp_as_sequence*/
  0,		/*tp_as_mapping*/
  (hashfunc) 0,		/*tp_hash*/
  (ternaryfunc) 0,	/*tp_call*/
  (reprfunc) msh_str,		/*tp_str*/

  /* Space for future expansion */
  0L,0L,0L,0L,
  Mshtype__doc__ /* Documentation string */
};

/**********/
/* object */
/**********/

static struct PyMethodDef obj_methods[] = {

  { NULL, NULL }
};

static objobject*
newobjobject(char* name)
{
  objobject* self = NULL;
  PyObject*  row1 = NULL;
  PyObject*  row2 = NULL;
  PyObject*  row3 = NULL;
  PyObject*  row4 = NULL;

  self = PyObject_NEW(objobject, &Objtype);
  if (self == NULL)
    {
      return NULL;
    }
  strcpy(self->name, name);
  self->matrix = PyList_New(4);
  row1 = PyList_New(4);
  row2 = PyList_New(4);
  row3 = PyList_New(4);
  row4 = PyList_New(4);
  PyList_SetItem(row1, 0, PyInt_FromLong(1));
  PyList_SetItem(row1, 1, PyInt_FromLong(0));
  PyList_SetItem(row1, 2, PyInt_FromLong(0));
  PyList_SetItem(row1, 3, PyInt_FromLong(0));
  PyList_SetItem(row2, 0, PyInt_FromLong(0));
  PyList_SetItem(row2, 1, PyInt_FromLong(1));
  PyList_SetItem(row2, 2, PyInt_FromLong(0));
  PyList_SetItem(row2, 3, PyInt_FromLong(0));
  PyList_SetItem(row3, 0, PyInt_FromLong(0));
  PyList_SetItem(row3, 1, PyInt_FromLong(0));
  PyList_SetItem(row3, 2, PyInt_FromLong(1));
  PyList_SetItem(row3, 3, PyInt_FromLong(0));
  PyList_SetItem(row4, 0, PyInt_FromLong(0));
  PyList_SetItem(row4, 1, PyInt_FromLong(0));
  PyList_SetItem(row4, 2, PyInt_FromLong(0));
  PyList_SetItem(row4, 3, PyInt_FromLong(1));
  PyList_SetItem(self->matrix, 0, row1);
  PyList_SetItem(self->matrix, 1, row2);
  PyList_SetItem(self->matrix, 2, row3);
  PyList_SetItem(self->matrix, 3, row4);
  Py_INCREF(Py_None);
  self->data = Py_None;
  Py_INCREF(Py_None);
  self->type = Py_None;

  return self;
}

static void
obj_dealloc(objobject* self)
{
  objobject* obj = (objobject*) self;

  Py_DECREF(obj->matrix);
  Py_DECREF(obj->data);
  Py_DECREF(obj->type);

  PyMem_DEL(self);
}

static int
obj_print(objobject* self, FILE* fp, int flags)
{
  fprintf(fp, "Object(name = \"%s\",\n", self->name);
/*    fprintf(fp, "       matrix = %s,\n", */
/*  	  PyString_AsString(mtx_repr((mtxobject*) self->matrix))); */
  if (self->type == Py_None)
    {
      fprintf(fp, "       data = None)\n");
    }
  else
    {
      fprintf(fp, "       data = %s(\"%s\"))\n",
	      PyString_AsString(self->type),
	      ((mshobject*) self->data)->name);
    }

  return 0;
}

static PyObject*
obj_repr(objobject* self)
{
  PyObject* s;

  s = PyString_FromString("Object()\n");

  return s;
}

static PyObject*
obj_str(objobject* self)
{
  PyObject* s;

  s = PyString_FromString("Object()\n");

  return s;
}

#include "structmember.h"

static struct memberlist obj_memberlist[] = {
  /* XXXX Add lines like { "foo", T_INT, OFF(foo), RO }  */
  {"data",   T_OBJECT, offsetof(objobject, data), RO},
  {"matrix", T_OBJECT, offsetof(objobject, matrix), RO},
  {"type",   T_OBJECT, offsetof(objobject, type), RO},
  {NULL}
};

static PyObject*
obj_getattr(objobject* self, char* name)
{
  PyObject* rv;

  /* XXXX Add your own getattr code here */
  rv = PyMember_Get((char*) self, obj_memberlist, name);
  if (rv)
    {
      return rv;
    }
  PyErr_Clear();

  return Py_FindMethod(obj_methods, (PyObject*)self, name);
}


static int
obj_setattr(objobject* self, char* name, PyObject* v)
{
  /* XXXX Add your own setattr code here */
  if ( v == NULL )
    {
      PyErr_SetString(PyExc_AttributeError, "Cannot delete attribute");
      return -1;
    }

  return PyMember_Set((char*)/*XXXX*/0, obj_memberlist, name, v);
}

static char Objtype__doc__[] =
""
;

static PyTypeObject Objtype = {
  PyObject_HEAD_INIT(&PyType_Type)
  0,				/*ob_size*/
  "Object",			/*tp_name*/
  sizeof(objobject),	/*tp_basicsize*/
  0,				/*tp_itemsize*/
  /* methods */
  (destructor) obj_dealloc,	/*tp_dealloc*/
  (printfunc) obj_print,	/*tp_print*/
  (getattrfunc) obj_getattr,	/*tp_getattr*/
  (setattrfunc) obj_setattr,	/*tp_setattr*/
  (cmpfunc) 0,	/*tp_compare*/
  (reprfunc) obj_repr,		/*tp_repr*/
  0,		/*tp_as_number*/
  0,		/*tp_as_sequence*/
  0,		/*tp_as_mapping*/
  (hashfunc) 0,		/*tp_hash*/
  (ternaryfunc) 0,	/*tp_call*/
  (reprfunc) obj_str,		/*tp_str*/

  /* Space for future expansion */
  0L,0L,0L,0L,
  Objtype__doc__ /* Documentation string */
};

/*********/
/* scene */
/*********/

static char sce_addObject__doc__[] =
"addObject(self, object)"
;

static PyObject*
sce_addObject(sceobject* self, PyObject* args)
{
  int index;
  PyObject* object = NULL;

  if (!PyArg_ParseTuple(args, "O", &object))
    {
      return NULL;
    }
  PyList_Append(self->objects, object);
  index = PyList_Size(self->objects) - 1;

  return PyInt_FromLong(index);
}

static struct PyMethodDef sce_methods[] = {
  {"addObject",   (PyCFunction)sce_addObject,
   METH_VARARGS, sce_addObject__doc__},

  { NULL, NULL }
};

static sceobject*
newsceobject(char* name)
{
  sceobject* self;

  self = PyObject_NEW(sceobject, &Scetype);
  if (self == NULL)
    {
      return NULL;
    }
  strcpy(self->name, name);
  self->objects = PyList_New(0);

  return self;
}

static void
sce_dealloc(sceobject* self)
{
  sceobject* sce = (sceobject*) self;

  Py_DECREF(sce->objects);

  PyMem_DEL(self);
}

static int
sce_print(sceobject* self, FILE* fp, int flags)
{
  fprintf(fp, "Scene(name = \"%s\",\n", self->name);
  fprintf(fp, "      objects = %d)\n", PyList_Size(self->objects));

  return 0;
}

static PyObject*
sce_repr(sceobject* self)
{
  PyObject* s;

  s = PyString_FromString("Scene()\n");

  return s;
}

static PyObject*
sce_str(sceobject* self)
{
  PyObject* s;

  s = PyString_FromString("Scene()\n");

  return s;
}

#include "structmember.h"

static struct memberlist sce_memberlist[] = {
  /* XXXX Add lines like { "foo", T_INT, OFF(foo), RO }  */
  {"objects", T_OBJECT, offsetof(sceobject, objects), RO},
  {NULL}
};

static PyObject*
sce_getattr(sceobject* self, char* name)
{
  PyObject* rv;

  /* XXXX Add your own getattr code here */
  rv = PyMember_Get((char*) self, sce_memberlist, name);
  if (rv)
    {
      return rv;
    }
  PyErr_Clear();

  return Py_FindMethod(sce_methods, (PyObject*)self, name);
}


static int
sce_setattr(sceobject* self, char* name, PyObject* v)
{
  /* XXXX Add your own setattr code here */
  if ( v == NULL )
    {
      PyErr_SetString(PyExc_AttributeError, "Cannot delete attribute");
      return -1;
    }

  return PyMember_Set((char*)/*XXXX*/0, sce_memberlist, name, v);
}

static char Scetype__doc__[] =
""
;

static PyTypeObject Scetype = {
  PyObject_HEAD_INIT(&PyType_Type)
  0,				/*ob_size*/
  "Scene",			/*tp_name*/
  sizeof(sceobject),	/*tp_basicsize*/
  0,				/*tp_itemsize*/
  /* methods */
  (destructor) sce_dealloc,	/*tp_dealloc*/
  (printfunc) sce_print,	/*tp_print*/
  (getattrfunc) sce_getattr,	/*tp_getattr*/
  (setattrfunc) sce_setattr,	/*tp_setattr*/
  (cmpfunc) 0,	/*tp_compare*/
  (reprfunc) sce_repr,		/*tp_repr*/
  0,		/*tp_as_number*/
  0,		/*tp_as_sequence*/
  0,		/*tp_as_mapping*/
  (hashfunc) 0,		/*tp_hash*/
  (ternaryfunc) 0,	/*tp_call*/
  (reprfunc) sce_str,		/*tp_str*/

  /* Space for future expansion */
  0L,0L,0L,0L,
  Scetype__doc__ /* Documentation string */
};

static char blend_Mesh__doc__[] =
"Creates an (empty) instance of a Blender mesh.\n\
    E.g.: \"m = Blender.Mesh('Plane')\"\n\
    To create faces first add vertices with \n\
    \"i1 = m.addVertex(x, y, z, nx, ny, nz)\"\n\
    then create faces with \"index = m.addFace(i1, i2, i3, i4, isSmooth)\".\
"
;

static PyObject*
blend_Mesh(PyObject* self, PyObject* args)
{
  if (!PyArg_ParseTuple(args, ""))
    {
      return NULL;
    }

  Py_INCREF(Py_None);
  return Py_None;
}

static char blend_Object__doc__[] =
"Creates an instance of a Blender object"
;

static PyObject*
blend_Object(PyObject* self, PyObject* args)
{
  char* name = NULL;

  if (!PyArg_ParseTuple(args, "s", &name))
    {
      return NULL;
    }

  return ((PyObject*) newobjobject(name));
}

static char blend_Scene__doc__[] =
"Creates an instance of a Blender scene"
;

static PyObject*
blend_Scene(PyObject* self, PyObject* args)
{
  char* name = NULL;

  if (!PyArg_ParseTuple(args, "s", &name))
    {
      return NULL;
    }

  return ((PyObject*) newsceobject(name));
}

static char blend_addMesh__doc__[] =
"Blender.addMesh(type, scene)\n\
    where type is one of [\"Plane\"]"
;

static PyObject*
blend_addMesh(PyObject* self, PyObject* args)
{
  char* type = NULL;
  PyObject* scene   = NULL;
  PyObject* tuple   = NULL;
  PyObject* object  = NULL;
  PyObject* mesh    = NULL;
  PyObject* index   = NULL;
  PyObject* indices = NULL;

  if (!PyArg_ParseTuple(args, "sO", &type, &scene))
    {
      return NULL;
    }

  if (strcmp(type, "Plane") == 0)
    {
      object = (PyObject*) newobjobject(type);
      mesh   = (PyObject*) newmshobject(type);
      indices = PyList_New(5);
      /* vertices */
      index = msh_addVertex((mshobject*) mesh,
			    Py_BuildValue("ffffff", 
					  1.0, 1.0, 0.0, 0.0, 0.0, 1.0));
      PyList_SetItem(indices, 0, index);
      index = msh_addVertex((mshobject*) mesh,
			    Py_BuildValue("ffffff", 
					  1.0, -1.0, 0.0, 0.0, 0.0, 1.0));
      PyList_SetItem(indices, 1, index);
      index = msh_addVertex((mshobject*) mesh,
			    Py_BuildValue("ffffff", 
					  -1.0, -1.0, 0.0, 0.0, 0.0, 1.0));
      PyList_SetItem(indices, 2, index);
      index = msh_addVertex((mshobject*) mesh,
			    Py_BuildValue("ffffff", 
					  -1.0, 1.0, 0.0, 0.0, 0.0, 1.0));
      PyList_SetItem(indices, 3, index);
      PyList_SetItem(indices, 4, PyInt_FromLong(0)); /* smooth flag */
      /* faces */
      msh_addFace((mshobject*) mesh,
		  Py_BuildValue("OOOOO",
				PyList_GetItem(indices, 0),
				PyList_GetItem(indices, 3),
				PyList_GetItem(indices, 2),
				PyList_GetItem(indices, 1),
				PyList_GetItem(indices, 4)));
      /* connection */
      blend_connect(self, Py_BuildValue("OO", object, mesh));
      blend_connect(self, Py_BuildValue("OO", scene, object));
      /* return value */
      tuple = PyTuple_New(2);
      PyTuple_SetItem(tuple, 0, object);
      PyTuple_SetItem(tuple, 1, mesh);

      return tuple;
    }

  Py_INCREF(Py_None);
  return Py_None;
}

static char blend_connect__doc__[] =
"connect(obj1, obj2)"
;

static PyObject*
blend_connect(PyObject* self, PyObject* args)
{
  PyObject* obj1 = NULL;
  PyObject* obj2 = NULL;

  if (!PyArg_ParseTuple(args, "OO", &obj1, &obj2))
    {
      return NULL;
    }
  if (obj1->ob_type == &Objtype)
    {
      if (obj2->ob_type == &Mshtype)
	{
	  Py_INCREF(obj2);
	  ((objobject*) obj1)->data = obj2;
	  ((objobject*) obj1)->type = PyString_FromString("Mesh");
	}
    }
  else if (obj1->ob_type == &Scetype)
    {
      if (obj2->ob_type == &Objtype)
	{
	  sce_addObject((sceobject*) obj1, Py_BuildValue("(O)", obj2));
	}
    }

  Py_INCREF(Py_None);
  return Py_None;
}

static char blend_getCurrentScene__doc__[] =
"getCurrentScene()"
;

static PyObject*
blend_getCurrentScene(PyObject* self, PyObject* args)
{
  if (!PyArg_ParseTuple(args, ""))
    {
      return NULL;
    }

  Py_INCREF(_scene);
  return _scene;
}

/* List of methods defined in the module */

static struct PyMethodDef blend_methods[] = {
  {"Mesh",            (PyCFunction) blend_Mesh,
   METH_VARARGS, blend_Mesh__doc__},
  {"Object",          (PyCFunction) blend_Object,
   METH_VARARGS, blend_Object__doc__},
  {"Scene",           (PyCFunction) blend_Scene,
   METH_VARARGS, blend_Scene__doc__},
  {"addMesh",	      (PyCFunction) blend_addMesh,
   METH_VARARGS, blend_addMesh__doc__},
  {"connect",	      (PyCFunction) blend_connect,
   METH_VARARGS, blend_connect__doc__},
  {"getCurrentScene", (PyCFunction) blend_getCurrentScene,
   METH_VARARGS, blend_getCurrentScene__doc__},
  { NULL, (PyCFunction) NULL, 0, NULL }
};


/* Initialization function for the module (*must* be called initBlender) */

static char Blender_module_documentation[] =
"This is the Python API for Blender"
;

void
initBlender()
{
  PyObject* m;
  PyObject* d;

  /* Create the module and add the functions */
  m = Py_InitModule4("Blender", blend_methods,
		     Blender_module_documentation,
		     (PyObject*)NULL,PYTHON_API_VERSION);

  /* Add some symbolic constants to the module */
  d = PyModule_GetDict(m);
  ErrorObject = PyString_FromString("Blender.error");
  PyDict_SetItemString(d, "error", ErrorObject);

  /* XXXX Add constants here */
  _scene = (PyObject*) newsceobject("1");
  PyDict_SetItemString(d, "_scene", _scene);

  /* Check for errors */
  if (PyErr_Occurred())
    {
      Py_FatalError("can't initialize module Blender");
    }
}

int main(int argc, char* argv[])
{
  char filename[] = "test.py";
  FILE* fp = NULL;

  Py_SetProgramName("blender");
  Py_Initialize();
  initBlender();
  fp = fopen(filename, "r");
  PyRun_AnyFile(fp, filename);

  Py_Finalize();

  return EXIT_SUCCESS;
}
