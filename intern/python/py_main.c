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

/* CVS */

/* $Author$ */
/* $Date$ */
/* $RCSfile$ */
/* $Revision$ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


/*  Jan Walter's stuff */
#include "Python.h"
#include "blender.h"

static PyObject* ErrorObject;

uint* mcol_to_vcol(Mesh *me);
void mcol_to_rgb(uint col, float *r, float *g, float *b);
void initBlender();
static PyObject* blend_connect(PyObject* self, PyObject* args);
static PyObject* blend_getObject(PyObject* self, PyObject* args);
/*  Jan Walter's stuff */

/*  Daniel Dunbar's stuff */
void start_python (void);
void end_python(void);
void txt_do_python (Text* text);
void do_all_scripts(short event);
void do_all_scriptlist(ListBase* list, short event);
void do_pyscript(ID *id, short event);
void clear_bad_scriptlink(ID* id, Text* byebye);
void clear_bad_scriptlinks(Text *byebye);
void free_scriptlink(ScriptLink *slink);
void copy_scriptlink(ScriptLink *scriptlink);

void start_python (void)
{
  Py_SetProgramName("blender");
  Py_Initialize();
  initBlender();
}

void end_python(void)
{
  Py_Finalize();
}

void txt_do_python (Text* text)
{
  char filename[] = "test.py";
  FILE* fp = NULL;

  if (text->name)
    {
      fp = fopen(text->name, "r");
    }
  else
    {
      fp = fopen(filename, "r");
    }
  if (fp)
    {
      if (text->name)
	{
	  PyRun_AnyFile(fp, text->name);
	}
      else
	{
	  PyRun_AnyFile(fp, filename);
	}
    }
  else
    {
      if (text->name)
	{
	  printf("Couldn't run %s ...\n", text->name);
	}
      else
	{
	  printf("Couldn't run test.py ...\n");
	}
    }
}

void do_all_scripts(short event)
{
}

void do_all_scriptlist(ListBase* list, short event)
{
}

void do_pyscript(ID *id, short event)
{
}

void clear_bad_scriptlink(ID* id, Text* byebye)
{
}

void clear_bad_scriptlinks(Text *byebye)
{
}

void free_scriptlink(ScriptLink *slink)
{
}

void copy_scriptlink(ScriptLink *scriptlink)
{
}
/*  Daniel Dunbar's stuff */

ID* find_name_in_list(ID* list, const char* name)
{
  while (list)
    {
      if (STREQ(name, list->name+2))
	{
	  break;
	}
      else
	{
	  list = list->next;
	}
    }

  return list;
}

/*  Jan Walter's stuff */
/**************/
/* structures */
/**************/

typedef struct {
  PyObject_HEAD
  char name[24];
  PyObject* Lens;
  PyObject* ClSta;
  PyObject* ClEnd;
} camobject;

staticforward PyTypeObject Camtype;

typedef struct {
  PyObject_HEAD
  PyObject* startFrame;
  PyObject* endFrame;
  PyObject* currentFrame;
  PyObject* xResolution;
  PyObject* yResolution;
  PyObject* pixelAspectRatio;
} dspobject;

staticforward PyTypeObject Dsptype;

typedef struct {
  PyObject_HEAD
  char name[24];
  PyObject* R;
  PyObject* G;
  PyObject* B;
} lmpobject;

staticforward PyTypeObject Lmptype;

typedef struct {
  PyObject_HEAD
  char name[24];
  PyObject* R;
  PyObject* G;
  PyObject* B;
} matobject;

staticforward PyTypeObject Mattype;

typedef struct {
  PyObject_HEAD
  char name[24];
  PyObject* vertices;
  PyObject* normals;
  PyObject* colors;
  PyObject* faces;
  PyObject* texture;
  PyObject* texcoords;
} mshobject;

staticforward PyTypeObject Mshtype;

typedef struct {
  PyObject_HEAD
  char name[24];
  PyObject* matrix;
  PyObject* inverseMatrix;
  PyObject* materials;
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

/**********/
/* camera */
/**********/

static struct PyMethodDef cam_methods[] = {
  { NULL, NULL }
};

static camobject*
newcamobject(char* name)
{
  camobject* self;
  ID*        list = NULL;

  self = PyObject_NEW(camobject, &Camtype);
  if (self == NULL)
    {
      return NULL;
    }
  strcpy(self->name, name);
  self->Lens  = PyFloat_FromDouble(35.0);
  self->ClSta = PyFloat_FromDouble(0.1);
  self->ClEnd = PyFloat_FromDouble(100.0);

  return self;
}

static void
cam_dealloc(camobject* self)
{
  camobject* cam = (camobject*) self;

  Py_DECREF(cam->Lens);
  Py_DECREF(cam->ClSta);
  Py_DECREF(cam->ClEnd);

  PyMem_DEL(self);
}

static int
cam_print(camobject* self, FILE* fp, int flags)
{
  fprintf(fp, "Camera(name = \"%s\")\n", self->name);

  return 0;
}

static PyObject*
cam_repr(camobject* self)
{
  PyObject* s;

  s = PyString_FromString("Camera()\n");

  return s;
}

static PyObject*
cam_str(camobject* self)
{
  PyObject* s;

  s = PyString_FromString("Camera()\n");

  return s;
}

#include "structmember.h"

static struct memberlist cam_memberlist[] = {
  /* XXXX Add lines like { "foo", T_INT, OFF(foo), RO }  */
  {"Lens",  T_OBJECT, offsetof(camobject, Lens),  RO},
  {"ClSta", T_OBJECT, offsetof(camobject, ClSta), RO},
  {"ClEnd", T_OBJECT, offsetof(camobject, ClEnd), RO},
  {NULL}
};

static PyObject*
cam_getattr(camobject* self, char* name)
{
  PyObject* rv;

  /* XXXX Add your own getattr code here */
  rv = PyMember_Get((char*) self, cam_memberlist, name);
  if (rv)
    {
      return rv;
    }
  PyErr_Clear();

  return Py_FindMethod(cam_methods, (PyObject*)self, name);
}


static int
cam_setattr(camobject* self, char* name, PyObject* v)
{
  /* XXXX Add your own setattr code here */
  if ( v == NULL )
    {
      PyErr_SetString(PyExc_AttributeError, "Cannot delete attribute");
      return -1;
    }

  return PyMember_Set((char*)/*XXXX*/0, cam_memberlist, name, v);
}

static char Camtype__doc__[] =
""
;

static PyTypeObject Camtype = {
  PyObject_HEAD_INIT(&PyType_Type)
  0,				/*ob_size*/
  "Camera",			/*tp_name*/
  sizeof(camobject),	/*tp_basicsize*/
  0,				/*tp_itemsize*/
  /* methods */
  (destructor) cam_dealloc,	/*tp_dealloc*/
  (printfunc) cam_print,	/*tp_print*/
  (getattrfunc) cam_getattr,	/*tp_getattr*/
  (setattrfunc) cam_setattr,	/*tp_setattr*/
  (cmpfunc) 0,	/*tp_compare*/
  (reprfunc) cam_repr,		/*tp_repr*/
  0,		/*tp_as_number*/
  0,		/*tp_as_sequence*/
  0,		/*tp_as_mapping*/
  (hashfunc) 0,		/*tp_hash*/
  (ternaryfunc) 0,	/*tp_call*/
  (reprfunc) cam_str,		/*tp_str*/

  /* Space for future expansion */
  0L,0L,0L,0L,
  Camtype__doc__ /* Documentation string */
};

/********************/
/* display settings */
/********************/

static struct PyMethodDef dsp_methods[] = {
  { NULL, NULL }
};

static dspobject*
newdspobject(void)
{
  dspobject* self;
  ID*        list = NULL;

  self = PyObject_NEW(dspobject, &Dsptype);
  if (self == NULL)
    {
      return NULL;
    }
  self->startFrame       = PyInt_FromLong(1);
  self->endFrame         = PyInt_FromLong(250);
  self->currentFrame     = PyInt_FromLong(1);
  self->xResolution      = PyInt_FromLong(320);
  self->yResolution      = PyInt_FromLong(256);
  self->pixelAspectRatio = PyFloat_FromDouble(1.0);

  return self;
}

static void
dsp_dealloc(dspobject* self)
{
  dspobject* dsp = (dspobject*) self;

  Py_DECREF(dsp->startFrame);
  Py_DECREF(dsp->endFrame);
  Py_DECREF(dsp->currentFrame);
  Py_DECREF(dsp->xResolution);
  Py_DECREF(dsp->yResolution);
  Py_DECREF(dsp->pixelAspectRatio);

  PyMem_DEL(self);
}

static int
dsp_print(dspobject* self, FILE* fp, int flags)
{
  fprintf(fp, "DisplaySettings()\n");

  return 0;
}

static PyObject*
dsp_repr(dspobject* self)
{
  PyObject* s;

  s = PyString_FromString("DisplaySettings()()\n");

  return s;
}

static PyObject*
dsp_str(dspobject* self)
{
  PyObject* s;

  s = PyString_FromString("DisplaySettings()()\n");

  return s;
}

#include "structmember.h"

static struct memberlist dsp_memberlist[] = {
  /* XXXX Add lines like { "foo", T_INT, OFF(foo), RO }  */
  {"startFrame",       T_OBJECT, offsetof(dspobject, startFrame),       RO},
  {"endFrame",         T_OBJECT, offsetof(dspobject, endFrame),         RO},
  {"currentFrame",     T_OBJECT, offsetof(dspobject, currentFrame),     RO},
  {"xResolution",      T_OBJECT, offsetof(dspobject, xResolution),      RO},
  {"yResolution",      T_OBJECT, offsetof(dspobject, yResolution),      RO},
  {"pixelAspectRatio", T_OBJECT, offsetof(dspobject, pixelAspectRatio), RO},
  {NULL}
};

static PyObject*
dsp_getattr(dspobject* self, char* name)
{
  PyObject* rv;

  /* XXXX Add your own getattr code here */
  rv = PyMember_Get((char*) self, dsp_memberlist, name);
  if (rv)
    {
      return rv;
    }
  PyErr_Clear();

  return Py_FindMethod(dsp_methods, (PyObject*)self, name);
}


static int
dsp_setattr(dspobject* self, char* name, PyObject* v)
{
  /* XXXX Add your own setattr code here */
  if ( v == NULL )
    {
      PyErr_SetString(PyExc_AttributeError, "Cannot delete attribute");
      return -1;
    }

  return PyMember_Set((char*)/*XXXX*/0, dsp_memberlist, name, v);
}

static char Dsptype__doc__[] =
""
;

static PyTypeObject Dsptype = {
  PyObject_HEAD_INIT(&PyType_Type)
  0,				/*ob_size*/
  "DisplaySettings",			/*tp_name*/
  sizeof(dspobject),	/*tp_basicsize*/
  0,				/*tp_itemsize*/
  /* methods */
  (destructor) dsp_dealloc,	/*tp_dealloc*/
  (printfunc) dsp_print,	/*tp_print*/
  (getattrfunc) dsp_getattr,	/*tp_getattr*/
  (setattrfunc) dsp_setattr,	/*tp_setattr*/
  (cmpfunc) 0,	/*tp_compare*/
  (reprfunc) dsp_repr,		/*tp_repr*/
  0,		/*tp_as_number*/
  0,		/*tp_as_sequence*/
  0,		/*tp_as_mapping*/
  (hashfunc) 0,		/*tp_hash*/
  (ternaryfunc) 0,	/*tp_call*/
  (reprfunc) dsp_str,		/*tp_str*/

  /* Space for future expansion */
  0L,0L,0L,0L,
  Dsptype__doc__ /* Documentation string */
};

/********/
/* Lamp */
/********/

static struct PyMethodDef lmp_methods[] = {
  { NULL, NULL }
};

static lmpobject*
newlmpobject(char* name)
{
  lmpobject* self;
  ID*        list = NULL;

  self = PyObject_NEW(lmpobject, &Lmptype);
  if (self == NULL)
    {
      return NULL;
    }
  strcpy(self->name, name);
  self->R = PyFloat_FromDouble(0.8);
  self->G = PyFloat_FromDouble(0.8);
  self->B = PyFloat_FromDouble(0.8);

  return self;
}

static void
lmp_dealloc(lmpobject* self)
{
  lmpobject* lmp = (lmpobject*) self;

  Py_DECREF(lmp->R);
  Py_DECREF(lmp->G);
  Py_DECREF(lmp->B);

  PyMem_DEL(self);
}

static int
lmp_print(lmpobject* self, FILE* fp, int flags)
{
  fprintf(fp, "Lamp(name = \"%s\")\n", self->name);

  return 0;
}

static PyObject*
lmp_repr(lmpobject* self)
{
  PyObject* s;

  s = PyString_FromString("Lamp()\n");

  return s;
}

static PyObject*
lmp_str(lmpobject* self)
{
  PyObject* s;

  s = PyString_FromString("Lamp()\n");

  return s;
}

#include "structmember.h"

static struct memberlist lmp_memberlist[] = {
  /* XXXX Add lines like { "foo", T_INT, OFF(foo), RO }  */
  {"R", T_OBJECT, offsetof(lmpobject, R), RO},
  {"G", T_OBJECT, offsetof(lmpobject, G), RO},
  {"B", T_OBJECT, offsetof(lmpobject, B), RO},
  {NULL}
};

static PyObject*
lmp_getattr(lmpobject* self, char* name)
{
  PyObject* rv;

  /* XXXX Add your own getattr code here */
  rv = PyMember_Get((char*) self, lmp_memberlist, name);
  if (rv)
    {
      return rv;
    }
  PyErr_Clear();

  return Py_FindMethod(lmp_methods, (PyObject*)self, name);
}


static int
lmp_setattr(lmpobject* self, char* name, PyObject* v)
{
  /* XXXX Add your own setattr code here */
  if ( v == NULL )
    {
      PyErr_SetString(PyExc_AttributeError, "Cannot delete attribute");
      return -1;
    }

  return PyMember_Set((char*)/*XXXX*/0, lmp_memberlist, name, v);
}

static char Lmptype__doc__[] =
""
;

static PyTypeObject Lmptype = {
  PyObject_HEAD_INIT(&PyType_Type)
  0,				/*ob_size*/
  "Lamp",			/*tp_name*/
  sizeof(lmpobject),	/*tp_basicsize*/
  0,				/*tp_itemsize*/
  /* methods */
  (destructor) lmp_dealloc,	/*tp_dealloc*/
  (printfunc) lmp_print,	/*tp_print*/
  (getattrfunc) lmp_getattr,	/*tp_getattr*/
  (setattrfunc) lmp_setattr,	/*tp_setattr*/
  (cmpfunc) 0,	/*tp_compare*/
  (reprfunc) lmp_repr,		/*tp_repr*/
  0,		/*tp_as_number*/
  0,		/*tp_as_sequence*/
  0,		/*tp_as_mapping*/
  (hashfunc) 0,		/*tp_hash*/
  (ternaryfunc) 0,	/*tp_call*/
  (reprfunc) lmp_str,		/*tp_str*/

  /* Space for future expansion */
  0L,0L,0L,0L,
  Lmptype__doc__ /* Documentation string */
};

/************/
/* material */
/************/

static struct PyMethodDef mat_methods[] = {
  { NULL, NULL }
};

static matobject*
newmatobject(char* name)
{
  matobject* self;
  ID*        list = NULL;

  self = PyObject_NEW(matobject, &Mattype);
  if (self == NULL)
    {
      return NULL;
    }
  strcpy(self->name, name);
  self->R = PyFloat_FromDouble(0.8);
  self->G = PyFloat_FromDouble(0.8);
  self->B = PyFloat_FromDouble(0.8);

  return self;
}

static void
mat_dealloc(matobject* self)
{
  matobject* mat = (matobject*) self;

  Py_DECREF(mat->R);
  Py_DECREF(mat->G);
  Py_DECREF(mat->B);

  PyMem_DEL(self);
}

static int
mat_print(matobject* self, FILE* fp, int flags)
{
  fprintf(fp, "Material(name = \"%s\")\n", self->name);

  return 0;
}

static PyObject*
mat_repr(matobject* self)
{
  PyObject* s;

  s = PyString_FromString("Material()\n");

  return s;
}

static PyObject*
mat_str(matobject* self)
{
  PyObject* s;

  s = PyString_FromString("Material()\n");

  return s;
}

#include "structmember.h"

static struct memberlist mat_memberlist[] = {
  /* XXXX Add lines like { "foo", T_INT, OFF(foo), RO }  */
  {"R", T_OBJECT, offsetof(matobject, R), RO},
  {"G", T_OBJECT, offsetof(matobject, G), RO},
  {"B", T_OBJECT, offsetof(matobject, B), RO},
  {NULL}
};

static PyObject*
mat_getattr(matobject* self, char* name)
{
  PyObject* rv;

  /* XXXX Add your own getattr code here */
  rv = PyMember_Get((char*) self, mat_memberlist, name);
  if (rv)
    {
      return rv;
    }
  PyErr_Clear();

  return Py_FindMethod(mat_methods, (PyObject*)self, name);
}


static int
mat_setattr(matobject* self, char* name, PyObject* v)
{
  /* XXXX Add your own setattr code here */
  if ( v == NULL )
    {
      PyErr_SetString(PyExc_AttributeError, "Cannot delete attribute");
      return -1;
    }

  return PyMember_Set((char*)/*XXXX*/0, mat_memberlist, name, v);
}

static char Mattype__doc__[] =
""
;

static PyTypeObject Mattype = {
  PyObject_HEAD_INIT(&PyType_Type)
  0,				/*ob_size*/
  "Material",			/*tp_name*/
  sizeof(matobject),	/*tp_basicsize*/
  0,				/*tp_itemsize*/
  /* methods */
  (destructor) mat_dealloc,	/*tp_dealloc*/
  (printfunc) mat_print,	/*tp_print*/
  (getattrfunc) mat_getattr,	/*tp_getattr*/
  (setattrfunc) mat_setattr,	/*tp_setattr*/
  (cmpfunc) 0,	/*tp_compare*/
  (reprfunc) mat_repr,		/*tp_repr*/
  0,		/*tp_as_number*/
  0,		/*tp_as_sequence*/
  0,		/*tp_as_mapping*/
  (hashfunc) 0,		/*tp_hash*/
  (ternaryfunc) 0,	/*tp_call*/
  (reprfunc) mat_str,		/*tp_str*/

  /* Space for future expansion */
  0L,0L,0L,0L,
  Mattype__doc__ /* Documentation string */
};

/********/
/* mesh */
/********/

static char msh_addFace__doc__[] =
"addFace(self, i1, i2, i3, i4, isSmooth, matIndex)"
;

static PyObject*
msh_addFace(mshobject* self, PyObject* args)
{
  int       index;
  int       i1, i2, i3, i4;
  int       isSmooth, matIndex;
  PyObject* item = NULL;

  if (!PyArg_ParseTuple(args, "iiiiii", &i1, &i2, &i3, &i4,
			&isSmooth, &matIndex))
    {
      return NULL;
    }
  item = PyList_New(6);
  PyList_SetItem(item, 0, PyInt_FromLong(i1));
  PyList_SetItem(item, 1, PyInt_FromLong(i2));
  PyList_SetItem(item, 2, PyInt_FromLong(i3));
  PyList_SetItem(item, 3, PyInt_FromLong(i4));
  PyList_SetItem(item, 4, PyInt_FromLong(isSmooth));
  PyList_SetItem(item, 5, PyInt_FromLong(matIndex));
  PyList_Append(self->faces, item);
  index = PyList_Size(self->faces) - 1;

  return PyInt_FromLong(index);
}

static char msh_addTexCoords__doc__[] =
"addTexCoords(self, coords)"
;

static PyObject*
msh_addTexCoords(mshobject* self, PyObject* args)
{
  float     u, v;
  PyObject* item = NULL;

  if (!PyArg_ParseTuple(args, "ff",
			&u, &v))
    {
      return NULL;
    }
  if (u < 0.0)
    {
      u = 0.0;
    }
  if (u > 1.0)
    {
      u = 1.0;
    }
  if (v < 0.0)
    {
      v = 0.0;
    }
  if (v > 1.0)
    {
      v = 1.0;
    }
  item = PyList_New(2);
  PyList_SetItem(item, 0, PyFloat_FromDouble(u));
  PyList_SetItem(item, 1, PyFloat_FromDouble(v));
  PyList_Append(self->texcoords, item);

  Py_INCREF(Py_None);
  return Py_None;
}

static char msh_addTexture__doc__[] =
"addTexture(self, filename)"
;

static PyObject*
msh_addTexture(mshobject* self, PyObject* args)
{
  char* filename = NULL;

  if (!PyArg_ParseTuple(args, "s", &filename))
    {
      return NULL;
    }
  self->texture = PyString_FromString(filename);

  Py_INCREF(Py_None);
  return Py_None;
}

static char msh_addVertex__doc__[] =
"addVertex(self, x, y, z, nx, ny, nz, r = -1.0, g = 0.0, b = 0.0)"
;

static PyObject*
msh_addVertex(mshobject* self, PyObject* args)
{
  int       index;
  float     x, y, z, nx, ny, nz;
  float     r = -1.0, g = 0.0, b = 0.0;
  PyObject* item1 = NULL;
  PyObject* item2 = NULL;
  PyObject* item3 = NULL;

  if (!PyArg_ParseTuple(args, "ffffff|fff", &x, &y, &z, &nx, &ny, &nz,
			&r, &g, &b))
    {
      return NULL;
    }
  item1 = PyList_New(3);
  item2 = PyList_New(3);
  if (r != -1.0)
    {
      item3 = PyList_New(3);
    }
  PyList_SetItem(item1, 0, PyFloat_FromDouble(x));
  PyList_SetItem(item1, 1, PyFloat_FromDouble(y));
  PyList_SetItem(item1, 2, PyFloat_FromDouble(z));
  PyList_SetItem(item2, 0, PyFloat_FromDouble(nx));
  PyList_SetItem(item2, 1, PyFloat_FromDouble(ny));
  PyList_SetItem(item2, 2, PyFloat_FromDouble(nz));
  if (r != -1.0)
    {
      PyList_SetItem(item3, 0, PyFloat_FromDouble(r));
      PyList_SetItem(item3, 1, PyFloat_FromDouble(g));
      PyList_SetItem(item3, 2, PyFloat_FromDouble(b));
    }
  PyList_Append(self->vertices, item1);
  PyList_Append(self->normals, item2);
  if (r != -1.0)
    {
      PyList_Append(self->colors, item3);
    }
  index = PyList_Size(self->vertices) - 1;

  return PyInt_FromLong(index);
}

static struct PyMethodDef msh_methods[] = {
  {"addFace",   (PyCFunction)msh_addFace,
   METH_VARARGS, msh_addFace__doc__},
  {"addTexCoords", (PyCFunction)msh_addTexCoords,
   METH_VARARGS, msh_addTexCoords__doc__},
  {"addTexture", (PyCFunction)msh_addTexture,
   METH_VARARGS, msh_addTexture__doc__},
  {"addVertex", (PyCFunction)msh_addVertex,
   METH_VARARGS, msh_addVertex__doc__},

  { NULL, NULL }
};

static mshobject*
newmshobject(char* name)
{
  mshobject* self;
  ID*        list = NULL;

  self = PyObject_NEW(mshobject, &Mshtype);
  if (self == NULL)
    {
      return NULL;
    }
  strcpy(self->name, name);
  self->vertices  = PyList_New(0);
  self->normals   = PyList_New(0);
  self->colors    = PyList_New(0);
  self->faces     = PyList_New(0);
  self->texcoords = PyList_New(0);
  Py_INCREF(Py_None);
  self->texture = Py_None;

  return self;
}

static void
msh_dealloc(mshobject* self)
{
  mshobject* msh = (mshobject*) self;

  Py_DECREF(msh->vertices);
  Py_DECREF(msh->normals);
  Py_DECREF(msh->colors);
  Py_DECREF(msh->faces);
  Py_DECREF(msh->texture);
  Py_DECREF(msh->texcoords);

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
  {"vertices",  T_OBJECT, offsetof(mshobject, vertices),  RO},
  {"normals",   T_OBJECT, offsetof(mshobject, normals),   RO},
  {"colors",    T_OBJECT, offsetof(mshobject, colors),    RO},
  {"faces",     T_OBJECT, offsetof(mshobject, faces),     RO},
  {"texture",   T_OBJECT, offsetof(mshobject, texture),   RO},
  {"texcoords", T_OBJECT, offsetof(mshobject, texcoords), RO},
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
  self->inverseMatrix = PyList_New(4);
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
  PyList_SetItem(self->inverseMatrix, 0, row1);
  PyList_SetItem(self->inverseMatrix, 1, row2);
  PyList_SetItem(self->inverseMatrix, 2, row3);
  PyList_SetItem(self->inverseMatrix, 3, row4);
  self->materials = PyList_New(0);
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
  Py_DECREF(obj->inverseMatrix);
  Py_DECREF(obj->materials);
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
	      PyString_AsString(self->data));
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
  {"data",          T_OBJECT, offsetof(objobject, data),          RO},
  {"matrix",        T_OBJECT, offsetof(objobject, matrix),        RO},
  {"inverseMatrix", T_OBJECT, offsetof(objobject, inverseMatrix), RO},
  {"materials",     T_OBJECT, offsetof(objobject, materials),     RO},
  {"type",          T_OBJECT, offsetof(objobject, type),          RO},
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
  int       index;
  PyObject* object = NULL;

  if (!PyArg_ParseTuple(args, "O", &object))
    {
      return NULL;
    }
  PyList_Append(self->objects, object);
  index = PyList_Size(self->objects) - 1;

  return PyInt_FromLong(index);
}

static char sce_getCurrentCamera__doc__[] =
"getCurrentCamera(self)"
;

static PyObject*
sce_getCurrentCamera(sceobject* self, PyObject* args)
{
  char*     name   = NULL;
  PyObject* camera = NULL;

  if (!PyArg_ParseTuple(args, ""))
    {
      return NULL;
    }
  if (G.scene->camera)
    {
      name = G.scene->camera->id.name+2;
      camera = blend_getObject(NULL, 
			       Py_BuildValue("(O)", 
					     PyString_FromString(name)));
      
      return camera;
    }
  else
    {
      Py_INCREF(Py_None);
      return Py_None;
    }
}

static struct PyMethodDef sce_methods[] = {
  {"addObject",   (PyCFunction)sce_addObject,
   METH_VARARGS, sce_addObject__doc__},
  {"getCurrentCamera",   (PyCFunction)sce_getCurrentCamera,
   METH_VARARGS, sce_getCurrentCamera__doc__},

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
    \"i1 = m.addVertex(x, y, z, nx, ny, nz, r = -1.0, r = 0.0, b = 0.0)\"\n\
    then create faces with \n\
    \"index = m.addFace(i1, i2, i3, i4, isSmooth, matIndex)\".\
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
"Blender.addMesh(type, sceneName)\n\
    where type is one of [\"Plane\"]"
;

static PyObject*
blend_addMesh(PyObject* self, PyObject* args)
{
  char*      type      = NULL;
  char*      sceneName = NULL;
  PyObject*  tuple     = NULL;
  PyObject*  object    = NULL;
  PyObject*  mesh      = NULL;
  PyObject*  index     = NULL;
  PyObject*  indices   = NULL;
  objobject* obj       = NULL;
  mshobject* msh       = NULL;

  if (!PyArg_ParseTuple(args, "ss", &type, &sceneName))
    {
      return NULL;
    }

  if (strcmp(type, "Plane") == 0)
    {
      obj = newobjobject(type);
      msh = newmshobject(type);
      object = (PyObject*) obj;
      mesh   = (PyObject*) msh;
      indices = PyList_New(6);
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
      PyList_SetItem(indices, 5, PyInt_FromLong(0)); /* material index */
      /* faces */
      msh_addFace((mshobject*) mesh,
		  Py_BuildValue("OOOOOO",
				PyList_GetItem(indices, 0),
				PyList_GetItem(indices, 3),
				PyList_GetItem(indices, 2),
				PyList_GetItem(indices, 1),
				PyList_GetItem(indices, 4),
				PyList_GetItem(indices, 5)));
      /* connection */
      blend_connect(self, Py_BuildValue("OO",
					PyString_FromString(obj->name),
					PyString_FromString(msh->name)));
      blend_connect(self, Py_BuildValue("OO",
					PyString_FromString(sceneName),
					PyString_FromString(obj->name)));
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

static char blend_getCamera__doc__[] =
"getCamera()"
;

static PyObject*
blend_getCamera(PyObject* self, PyObject* args)
{
  char*      name     = NULL;
  ID*        list     = NULL;
  Camera*    cam      = NULL;
  camobject* camera   = NULL;

  if (!PyArg_ParseTuple(args, "s", &name))
    {
      return NULL;
    }
  list = find_name_in_list((ID*) G.main->camera.first, name);
  if (list)
    {
      cam = (Camera*) list;
      camera = newcamobject(cam->id.name+2);
      camera->Lens  = PyFloat_FromDouble(cam->lens);
      camera->ClSta = PyFloat_FromDouble(cam->clipsta);
      camera->ClEnd = PyFloat_FromDouble(cam->clipend);

      return (PyObject*) camera;
    }
  else
    {
      Py_INCREF(Py_None);
      return Py_None;
    }
}

static char blend_getCurrentScene__doc__[] =
"getCurrentScene()"
;

static PyObject*
blend_getCurrentScene(PyObject* self, PyObject* args)
{
  char*      name   = NULL;
  sceobject* scene  = NULL;
  Base*      base   = NULL;
  Object*    obj    = NULL;

  if (!PyArg_ParseTuple(args, ""))
    {
      return NULL;
    }

  /* create scene in Python */
  name = G.scene->id.name+2;
  scene = newsceobject(name);

  /* add existing objects */
  base = G.scene->base.first;
  while (base)
    {
      obj = base->object;
      PyList_Append(scene->objects, PyString_FromString(obj->id.name+2));
      base = base->next;
    }

  return (PyObject*) scene;
}

static char blend_getDisplaySettings__doc__[] =
"getDisplaySettings()"
;

static PyObject*
blend_getDisplaySettings(PyObject* self, PyObject* args)
{
  RenderData* rd            = NULL;
  dspobject*  displayObject = NULL;

  if (!PyArg_ParseTuple(args, ""))
    {
      return NULL;
    }
  rd = &(G.scene->r);
  displayObject = newdspobject();
  displayObject->startFrame       = PyInt_FromLong(rd->sfra);
  displayObject->endFrame         = PyInt_FromLong(rd->efra);
  displayObject->currentFrame     = PyInt_FromLong(rd->cfra);
  displayObject->xResolution      = PyInt_FromLong(rd->xsch);
  displayObject->yResolution      = PyInt_FromLong(rd->ysch);
  displayObject->pixelAspectRatio = PyInt_FromLong(rd->yasp / 
						   (1.0 * rd->xasp));
  
  return (PyObject*) displayObject;
}

static char blend_getLamp__doc__[] =
"getLamp(name)"
;

static PyObject*
blend_getLamp(PyObject* self, PyObject* args)
{
  char*      name = NULL;
  ID*        list = NULL;
  Lamp*      lmp  = NULL;
  lmpobject* lamp = NULL;

  if (!PyArg_ParseTuple(args, "s", &name))
    {
      return NULL;
    }
  list = find_name_in_list((ID*) G.main->lamp.first, name);
  if (list)
    {
      lmp = (Lamp*) list;
      lamp = newlmpobject(lmp->id.name+2);
      lamp->R      = PyFloat_FromDouble(lmp->r);
      lamp->G      = PyFloat_FromDouble(lmp->g);
      lamp->B      = PyFloat_FromDouble(lmp->b);
/*        lamp->Dist   = PyFloat_FromDouble(lmp->dist); */
/*        lamp->SpoSi  = PyFloat_FromDouble(lmp->spotsize); */
/*        lamp->SpoBl  = PyFloat_FromDouble(lmp->spotblend); */
/*        lamp->Quad1  = PyFloat_FromDouble(lmp->att1); */
/*        lamp->Quad2  = PyFloat_FromDouble(lmp->att2); */
/*        lamp->HaInt  = PyFloat_FromDouble(lmp->haint); */

/*        lamp->OfsX   = PyFloat_FromDouble(lmp->); */
/*        lamp->OfsY   = PyFloat_FromDouble(lmp->); */
/*        lamp->OfsZ   = PyFloat_FromDouble(lmp->); */
/*        lamp->SizeX  = PyFloat_FromDouble(lmp->); */
/*        lamp->SizeY  = PyFloat_FromDouble(lmp->); */
/*        lamp->SizeZ  = PyFloat_FromDouble(lmp->); */
/*        lamp->texR   = PyFloat_FromDouble(lmp->); */
/*        lamp->texG   = PyFloat_FromDouble(lmp->); */
/*        lamp->texB   = PyFloat_FromDouble(lmp->); */
/*        lamp->DefVar = PyFloat_FromDouble(lmp->); */
/*        lamp->Col    = PyFloat_FromDouble(lmp->); */
/*        lamp->Nor    = PyFloat_FromDouble(lmp->); */
/*        lamp->Var    = PyFloat_FromDouble(lmp->); */

      return (PyObject*) lamp;
    }
  else
    {
      Py_INCREF(Py_None);
      return Py_None;
    }
}

static char blend_getMaterial__doc__[] =
"getMaterial(name)"
;

static PyObject*
blend_getMaterial(PyObject* self, PyObject* args)
{
  char*      name     = NULL;
  ID*        list     = NULL;
  Material*  mat      = NULL;
  matobject* material = NULL;

  if (!PyArg_ParseTuple(args, "s", &name))
    {
      return NULL;
    }
  list = find_name_in_list((ID*) G.main->mat.first, name);
  if (list)
    {
      mat = (Material*) list;
      material = newmatobject(mat->id.name+2);
      material->R = PyFloat_FromDouble(mat->r);
      material->G = PyFloat_FromDouble(mat->g);
      material->B = PyFloat_FromDouble(mat->b);

      return (PyObject*) material;
    }
  else
    {
      Py_INCREF(Py_None);
      return Py_None;
    }
}

static char blend_getMesh__doc__[] =
"getMesh(name)"
;

static PyObject*
blend_getMesh(PyObject* self, PyObject* args)
{
  int        i;
  float      r, g, b;
  char       dummy[]  = "";
  char*      name     = NULL;
  char*      filename = NULL;
  uint*      mcol     = NULL;
  ID*        list     = NULL;
  Mesh*      msh      = NULL;
  mshobject* mesh     = NULL;
  MFace*     mface    = NULL;
  TFace*     tface    = NULL;

  if (!PyArg_ParseTuple(args, "s", &name))
    {
      return NULL;
    }
  list = find_name_in_list((ID*) G.main->mesh.first, name);
  if (list)
    {
      msh = (Mesh*) list;
      mesh = newmshobject(msh->id.name+2);
      /* is there color information? */
      if (msh->mcol)
	{
	  mcol = mcol_to_vcol(msh);
	}
      /* add vertices */
      for (i = 0; i < msh->totvert; i++)
	{
	  if (msh->mcol && mcol)
	    {
	      mcol_to_rgb(*mcol, &r, &g, &b);
	      msh_addVertex(mesh,
			    Py_BuildValue("fffffffff",
					  msh->mvert[i].co[0],
					  msh->mvert[i].co[1],
					  msh->mvert[i].co[2],
					  msh->mvert[i].no[0] / 32767.0,
					  msh->mvert[i].no[1] / 32767.0,
					  msh->mvert[i].no[2] / 32767.0,
					  r, g, b));
	      mcol++;
	    }
	  else
	    {
	      msh_addVertex(mesh,
			    Py_BuildValue("ffffff",
					  msh->mvert[i].co[0],
					  msh->mvert[i].co[1],
					  msh->mvert[i].co[2],
					  msh->mvert[i].no[0] / 32767.0,
					  msh->mvert[i].no[1] / 32767.0,
					  msh->mvert[i].no[2] / 32767.0));
	    }
	}
      /* add faces */
      for (i = 0; i < msh->totface; i++)
	{
	  mface = ((MFace*) msh->mface)+i;
	  msh_addFace(mesh,
		      Py_BuildValue("iiiiii",
				    mface->v1, mface->v2,
				    mface->v3, mface->v4,
				    mface->flag, mface->mat_nr));
	}
      /* add texture coordinates */
      if (msh->tface)
	{
	  tface = (TFace*) msh->tface;
	  if (tface->tpage)
	    {
	      filename = ((Image*) tface->tpage)->name;
	    }
	  else
	    {
	      filename = dummy;
	    }
	  msh_addTexture(mesh,
			 Py_BuildValue("(O)", PyString_FromString(filename)));
	  for (i = 0; i < msh->totface; i++)
	    {
	      tface = (TFace*) msh->tface+i;
	      msh_addTexCoords(mesh,
			       Py_BuildValue("ff",
					     tface->uv[0][0], 
					     tface->uv[0][1]));
	      msh_addTexCoords(mesh,
			       Py_BuildValue("ff",
					     tface->uv[1][0], 
					     tface->uv[1][1]));
	      msh_addTexCoords(mesh,
			       Py_BuildValue("ff",
					     tface->uv[2][0], 
					     tface->uv[2][1]));
	      msh_addTexCoords(mesh,
			       Py_BuildValue("ff",
					     tface->uv[3][0], 
					     tface->uv[3][1]));
	    }
	}

      return (PyObject*) mesh;
    }
  else
    {
      Py_INCREF(Py_None);
      return Py_None;
    }
}

static char blend_getObject__doc__[] =
"getObject(name)"
;

static PyObject*
blend_getObject(PyObject* self, PyObject* args)
{
  int        i;
  float      inverse[4][4];
  char*      name   = NULL;
  ID*        list   = NULL;
  Object*    obj    = NULL;
  Mesh*      msh    = NULL;
  objobject* object = NULL;
  PyObject*  row1   = NULL;
  PyObject*  row2   = NULL;
  PyObject*  row3   = NULL;
  PyObject*  row4   = NULL;

  if (!PyArg_ParseTuple(args, "s", &name))
    {
      return NULL;
    }
  list = find_name_in_list((ID*) G.main->object.first, name);
  if (list)
    {
      obj = (Object*) list;
      object = newobjobject(obj->id.name+2);
      Py_DECREF(object->matrix);
      object->matrix = PyList_New(4);
      row1 = PyList_New(4);
      row2 = PyList_New(4);
      row3 = PyList_New(4);
      row4 = PyList_New(4);
      PyList_SetItem(row1, 0, PyFloat_FromDouble(obj->obmat[0][0]));
      PyList_SetItem(row1, 1, PyFloat_FromDouble(obj->obmat[0][1]));
      PyList_SetItem(row1, 2, PyFloat_FromDouble(obj->obmat[0][2]));
      PyList_SetItem(row1, 3, PyFloat_FromDouble(obj->obmat[0][3]));
      PyList_SetItem(row2, 0, PyFloat_FromDouble(obj->obmat[1][0]));
      PyList_SetItem(row2, 1, PyFloat_FromDouble(obj->obmat[1][1]));
      PyList_SetItem(row2, 2, PyFloat_FromDouble(obj->obmat[1][2]));
      PyList_SetItem(row2, 3, PyFloat_FromDouble(obj->obmat[1][3]));
      PyList_SetItem(row3, 0, PyFloat_FromDouble(obj->obmat[2][0]));
      PyList_SetItem(row3, 1, PyFloat_FromDouble(obj->obmat[2][1]));
      PyList_SetItem(row3, 2, PyFloat_FromDouble(obj->obmat[2][2]));
      PyList_SetItem(row3, 3, PyFloat_FromDouble(obj->obmat[2][3]));
      PyList_SetItem(row4, 0, PyFloat_FromDouble(obj->obmat[3][0]));
      PyList_SetItem(row4, 1, PyFloat_FromDouble(obj->obmat[3][1]));
      PyList_SetItem(row4, 2, PyFloat_FromDouble(obj->obmat[3][2]));
      PyList_SetItem(row4, 3, PyFloat_FromDouble(obj->obmat[3][3]));
      PyList_SetItem(object->matrix, 0, row1);
      PyList_SetItem(object->matrix, 1, row2);
      PyList_SetItem(object->matrix, 2, row3);
      PyList_SetItem(object->matrix, 3, row4);
      /* inverse matrix */
      Mat4Invert(inverse, obj->obmat);
      Py_DECREF(object->inverseMatrix);
      object->inverseMatrix = PyList_New(4);
      row1 = PyList_New(4);
      row2 = PyList_New(4);
      row3 = PyList_New(4);
      row4 = PyList_New(4);
      PyList_SetItem(row1, 0, PyFloat_FromDouble(inverse[0][0]));
      PyList_SetItem(row1, 1, PyFloat_FromDouble(inverse[0][1]));
      PyList_SetItem(row1, 2, PyFloat_FromDouble(inverse[0][2]));
      PyList_SetItem(row1, 3, PyFloat_FromDouble(inverse[0][3]));
      PyList_SetItem(row2, 0, PyFloat_FromDouble(inverse[1][0]));
      PyList_SetItem(row2, 1, PyFloat_FromDouble(inverse[1][1]));
      PyList_SetItem(row2, 2, PyFloat_FromDouble(inverse[1][2]));
      PyList_SetItem(row2, 3, PyFloat_FromDouble(inverse[1][3]));
      PyList_SetItem(row3, 0, PyFloat_FromDouble(inverse[2][0]));
      PyList_SetItem(row3, 1, PyFloat_FromDouble(inverse[2][1]));
      PyList_SetItem(row3, 2, PyFloat_FromDouble(inverse[2][2]));
      PyList_SetItem(row3, 3, PyFloat_FromDouble(inverse[2][3]));
      PyList_SetItem(row4, 0, PyFloat_FromDouble(inverse[3][0]));
      PyList_SetItem(row4, 1, PyFloat_FromDouble(inverse[3][1]));
      PyList_SetItem(row4, 2, PyFloat_FromDouble(inverse[3][2]));
      PyList_SetItem(row4, 3, PyFloat_FromDouble(inverse[3][3]));
      PyList_SetItem(object->inverseMatrix, 0, row1);
      PyList_SetItem(object->inverseMatrix, 1, row2);
      PyList_SetItem(object->inverseMatrix, 2, row3);
      PyList_SetItem(object->inverseMatrix, 3, row4);
      Py_DECREF(object->materials);
      object->materials = PyList_New(obj->totcol);
      for (i = 0; i < obj->totcol; i++)
	{
	  if (obj->mat[i])
	    {
	      PyList_SetItem(object->materials, i,
			     PyString_FromString(obj->mat[i]->id.name+2));
	    }
	  else
	    {
	      Py_INCREF(Py_None);
	      PyList_SetItem(object->materials, i, Py_None);
	    }
	}      /* check type */
      switch (obj->type)
  	{
  	case OB_EMPTY:
  	  printf("Empty\n");
  	  break;
  	case OB_MESH:
	  object->data = PyString_FromString(((Mesh*) obj->data)->id.name+2);
	  object->type = PyString_FromString("Mesh");
	  msh = (Mesh*) obj->data;
	  for (i = 0; i < msh->totcol; i++)
	    {
	      if (msh->mat[i])
		{
		  PyList_SetItem(object->materials, i,
				 PyString_FromString(msh->mat[i]->id.name+2));
		}
	    }
  	  break;
  	case OB_CURVE:
  	  printf("Curve\n");
  	  break;
  	case OB_SURF:
  	  printf("Surface\n");
  	  break;
  	case OB_FONT:
  	  printf("Font\n");
  	  break;
  	case OB_MBALL:
  	  printf("Metaball\n");
  	  break;
  	case OB_LAMP:
	  object->data = PyString_FromString(((Lamp*) obj->data)->id.name+2);
	  object->type = PyString_FromString("Lamp");
  	  break;
  	case OB_CAMERA:
	  object->data = PyString_FromString(((Camera*) obj->data)->id.name+2);
	  object->type = PyString_FromString("Camera");
  	  break;
  	case OB_IKA:
  	  printf("Ika\n");
  	  break;
  	case OB_WAVE:
  	  printf("Wave\n");
  	  break;
  	case OB_LATTICE:
  	  printf("Lattice\n");
  	  break;
  	default:
  	  printf("ERROR: py_main.c:blend_getObject(...)\n");
  	  printf("       unknown type ...\n");
  	  break;
  	}

      return (PyObject*) object;
    }
  else
    {
      Py_INCREF(Py_None);
      return Py_None;
    }
}

static char blend_isCamera__doc__[] =
"isCamera(name)"
;

static PyObject*
blend_isCamera(PyObject* self, PyObject* args)
{
  char*      name   = NULL;
  ID*        list   = NULL;
  Object*    obj    = NULL;

  if (!PyArg_ParseTuple(args, "s", &name))
    {
      return NULL;
    }
  list = find_name_in_list((ID*) G.main->object.first, name);
  if (list)
    {
      obj = (Object*) list;
      if (obj->type == OB_CAMERA)
	{
	  return PyInt_FromLong(1);
	}
      else
	{
	  return PyInt_FromLong(0);
	}
    }
  else
    {
      return NULL;
    }
}

static char blend_isLamp__doc__[] =
"isLamp(name)"
;

static PyObject*
blend_isLamp(PyObject* self, PyObject* args)
{
  char*      name   = NULL;
  ID*        list   = NULL;
  Object*    obj    = NULL;

  if (!PyArg_ParseTuple(args, "s", &name))
    {
      return NULL;
    }
  list = find_name_in_list((ID*) G.main->object.first, name);
  if (list)
    {
      obj = (Object*) list;
      if (obj->type == OB_LAMP)
	{
	  return PyInt_FromLong(1);
	}
      else
	{
	  return PyInt_FromLong(0);
	}
    }
  else
    {
      return NULL;
    }
}

static char blend_isMesh__doc__[] =
"isMesh(name)"
;

static PyObject*
blend_isMesh(PyObject* self, PyObject* args)
{
  char*      name   = NULL;
  ID*        list   = NULL;
  Object*    obj    = NULL;

  if (!PyArg_ParseTuple(args, "s", &name))
    {
      return NULL;
    }
  list = find_name_in_list((ID*) G.main->object.first, name);
  if (list)
    {
      obj = (Object*) list;
      if (obj->type == OB_MESH)
	{
	  return PyInt_FromLong(1);
	}
      else
	{
	  return PyInt_FromLong(0);
	}
    }
  else
    {
      return NULL;
    }
}

static char blend_setCurrentFrame__doc__[] =
"setCurrentFrame(frame)"
;

static PyObject*
blend_setCurrentFrame(PyObject* self, PyObject* args)
{
  int frame = -1;

  if (!PyArg_ParseTuple(args, "i", &frame))
    {
      return NULL;
    }
  CFRA = frame;
  drawview3d();

  Py_INCREF(Py_None);
  return Py_None;
}

/* List of methods defined in the module */

static struct PyMethodDef blend_methods[] = {
  {"Mesh",               (PyCFunction) blend_Mesh,
   METH_VARARGS, blend_Mesh__doc__},
  {"Object",             (PyCFunction) blend_Object,
   METH_VARARGS, blend_Object__doc__},
  {"Scene",              (PyCFunction) blend_Scene,
   METH_VARARGS, blend_Scene__doc__},
  {"addMesh",	         (PyCFunction) blend_addMesh,
   METH_VARARGS, blend_addMesh__doc__},
  {"connect",	         (PyCFunction) blend_connect,
   METH_VARARGS, blend_connect__doc__},
  {"getCamera",          (PyCFunction) blend_getCamera,
   METH_VARARGS, blend_getCamera__doc__},
  {"getCurrentScene",    (PyCFunction) blend_getCurrentScene,
   METH_VARARGS, blend_getCurrentScene__doc__},
  {"getDisplaySettings", (PyCFunction) blend_getDisplaySettings,
   METH_VARARGS, blend_getDisplaySettings__doc__},
  {"getLamp",            (PyCFunction) blend_getLamp,
   METH_VARARGS, blend_getLamp__doc__},
  {"getMaterial",        (PyCFunction) blend_getMaterial,
   METH_VARARGS, blend_getMaterial__doc__},
  {"getMesh",            (PyCFunction) blend_getMesh,
   METH_VARARGS, blend_getMesh__doc__},
  {"getObject",          (PyCFunction) blend_getObject,
   METH_VARARGS, blend_getObject__doc__},
  {"isCamera",           (PyCFunction) blend_isCamera,
   METH_VARARGS, blend_isCamera__doc__},
  {"isLamp",             (PyCFunction) blend_isLamp,
   METH_VARARGS, blend_isLamp__doc__},
  {"isMesh",             (PyCFunction) blend_isMesh,
   METH_VARARGS, blend_isMesh__doc__},
  {"setCurrentFrame",    (PyCFunction) blend_setCurrentFrame,
   METH_VARARGS, blend_setCurrentFrame__doc__},
  { NULL,                (PyCFunction) NULL, 0, NULL }
};

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

  /* Check for errors */
  if (PyErr_Occurred())
    {
      Py_FatalError("can't initialize module Blender");
    }
}
/*  Jan Walter's stuff */
