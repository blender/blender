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
 * Contributor(s): Willian P. Germano
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/* This file is opy_nmesh.c from bpython modified to work with the new
 * implementation of the Blender Python API */

#include "NMesh.h"

void mesh_update(Mesh *mesh)
{
	edge_drawflags_mesh(mesh);
	tex_space_mesh(mesh);
}

/*****************************/
/*      Mesh Color Object    */
/*****************************/

static void NMCol_dealloc(PyObject *self)
{
  PyMem_DEL(self);
}

static C_NMCol *newcol (char r, char g, char b, char a)
{
  C_NMCol *mc = (C_NMCol *) PyObject_NEW (C_NMCol, &NMCol_Type);

  mc->r= r;
  mc->g= g;
  mc->b= b;
  mc->a= a;

  return mc;  
}

static PyObject *M_NMesh_Col(PyObject *self, PyObject *args)
{
  short r = 255, g = 255, b = 255, a = 255;

  if(PyArg_ParseTuple(args, "|hhhh", &r, &g, &b, &a))
    return (PyObject *) newcol(r, g, b, a);

  return NULL;  
}

static PyObject *NMCol_getattr(PyObject *self, char *name)
{
  C_NMCol *mc = (C_NMCol *)self;

  if (strcmp(name, "r") == 0) return Py_BuildValue("i", mc->r);
  else if (strcmp(name, "g") == 0) return Py_BuildValue("i", mc->g);
  else if (strcmp(name, "b") == 0) return Py_BuildValue("i", mc->b);
  else if (strcmp(name, "a") == 0) return Py_BuildValue("i", mc->a);

  return EXPP_ReturnPyObjError(PyExc_AttributeError, name);
}

static int NMCol_setattr(PyObject *self, char *name, PyObject *v)
{
  C_NMCol *mc = (C_NMCol *)self;
  short ival;

  if(!PyArg_Parse(v, "h", &ival)) return -1;

  ival = (short)EXPP_ClampInt(ival, 0, 255);

  if (strcmp(name, "r") == 0) mc->r = ival;
  else if (strcmp(name, "g") == 0) mc->g = ival;
  else if (strcmp(name, "b") == 0) mc->b = ival;
  else if (strcmp(name, "a")==0) mc->a = ival;
  else return -1;

  return 0;
}

PyObject *NMCol_repr(C_NMCol *self) 
{
  static char s[256];
  sprintf (s, "[NMCol - <%d, %d, %d, %d>]", self->r, self->g, self->b, self->a);
  return Py_BuildValue("s", s);
}

PyTypeObject NMCol_Type =
{
  PyObject_HEAD_INIT(&PyType_Type)
  0,                            /* ob_size */
  "NMCol",                      /* tp_name */
  sizeof(C_NMCol),              /* tp_basicsize */
  0,                            /* tp_itemsize */
  /* methods */
  (destructor) NMCol_dealloc,   /* tp_dealloc */
  (printfunc) 0,                /* tp_print */
  (getattrfunc) NMCol_getattr,  /* tp_getattr */
  (setattrfunc) NMCol_setattr,  /* tp_setattr */
  0,                            /* tp_compare */
  (reprfunc) NMCol_repr,        /* tp_repr */
  0,                            /* tp_as_number */
  0,                            /* tp_as_sequence */
  0,                            /* tp_as_mapping */
  0,                            /* tp_hash */
  0,                            /* tp_as_number */
  0,                            /* tp_as_sequence */
  0,                            /* tp_as_mapping */
  0,                            /* tp_hash */
};

/*****************************/
/*    NMesh Python Object    */
/*****************************/
static void NMFace_dealloc (PyObject *self)
{
  C_NMFace *mf = (C_NMFace *)self;

  Py_DECREF(mf->v);
  Py_DECREF(mf->uv);
  Py_DECREF(mf->col);

  PyMem_DEL(self);
}

static C_NMFace *new_NMFace(PyObject *vertexlist)
{
  C_NMFace *mf = PyObject_NEW (C_NMFace, &NMFace_Type);

  mf->v = vertexlist;
  mf->uv = PyList_New(0);
  mf->image = NULL;
  mf->mode = TF_DYNAMIC + TF_TEX;
  mf->flag = TF_SELECT;
  mf->transp = TF_SOLID;
  mf->col = PyList_New(0);

  mf->smooth= 0;
  mf->mat_nr= 0;

  return mf;
}

static PyObject *M_NMesh_Face(PyObject *self, PyObject *args)
{
  PyObject *vertlist = NULL;

  if (!PyArg_ParseTuple(args, "|O!", &PyList_Type, &vertlist))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                        "expected a list of vertices or nothing as argument");

  if (!vertlist) vertlist = PyList_New(0);

  return (PyObject *)new_NMFace(vertlist);
}

static PyObject *NMFace_append(PyObject *self, PyObject *args)
{
  PyObject *vert;
  C_NMFace *f = (C_NMFace *)self;

  if (!PyArg_ParseTuple(args, "O!", &NMVert_Type, &vert))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                  "expected an NMVert object");

  PyList_Append(f->v, vert);

  return EXPP_incr_ret(Py_None);
}

#undef MethodDef
#define MethodDef(func) {#func, NMFace_##func, METH_VARARGS, NMFace_##func##_doc}

static struct PyMethodDef NMFace_methods[] =
{
  MethodDef(append),
  {NULL, NULL}
};

static PyObject *NMFace_getattr(PyObject *self, char *name)
{
  C_NMFace *mf = (C_NMFace *)self;

  if(strcmp(name, "v") == 0)
    return Py_BuildValue("O", mf->v);
  else if (strcmp(name, "col") == 0) 
    return Py_BuildValue("O", mf->col);
  else if (strcmp(name, "mat") == 0) // emulation XXX
    return Py_BuildValue("i", mf->mat_nr);
  else if (strcmp(name, "materialIndex") == 0) 
    return Py_BuildValue("i", mf->mat_nr);
  else if (strcmp(name, "smooth") == 0)
    return Py_BuildValue("i", mf->smooth);

  else if (strcmp(name, "image") == 0) {
    if (mf->image)
      return Py_BuildValue("O", (PyObject *)mf->image);
    else 
      return EXPP_incr_ret(Py_None);
  }

  else if (strcmp(name, "mode") == 0) 
    return Py_BuildValue("i", mf->mode);
  else if (strcmp(name, "flag") == 0) 
    return Py_BuildValue("i", mf->flag);
  else if (strcmp(name, "transp") == 0)
    return Py_BuildValue("i", mf->transp);
  else if (strcmp(name, "uv") == 0)
    return Py_BuildValue("O", mf->uv);

  return Py_FindMethod(NMFace_methods, (PyObject*)self, name);
}

static int NMFace_setattr(PyObject *self, char *name, PyObject *v)
{
  C_NMFace *mf = (C_NMFace *)self;
  short ival;

  if (strcmp(name, "v") == 0) {

    if(PySequence_Check(v)) {
      Py_DECREF(mf->v);
      mf->v = EXPP_incr_ret(v);

      return 0;
    }
  }
	else if (strcmp(name, "col") == 0) {

    if(PySequence_Check(v)) {
      Py_DECREF(mf->col);
      mf->col = EXPP_incr_ret(v);

			return 0;
    }
	}
	else if (!strcmp(name, "mat") || !strcmp(name, "materialIndex")) {
    PyArg_Parse(v, "h", &ival);
    mf->mat_nr= ival;
    
    return 0;
  }
	else if (strcmp(name, "smooth") == 0) {
    PyArg_Parse(v, "h", &ival);
    mf->smooth = ival?1:0;

    return 0;
  }
	else if (strcmp(name, "uv") == 0) {

    if(PySequence_Check(v)) {
      Py_DECREF(mf->uv);
      mf->uv = EXPP_incr_ret(v);

			return 0;
    }
  }
	else if (strcmp(name, "flag") == 0) {
      PyArg_Parse(v, "h", &ival);
      mf->flag = ival;

			return 0;
  }
	else if (strcmp(name, "mode") == 0) {
      PyArg_Parse(v, "h", &ival);
      mf->mode = ival;

			return 0;
  }
	else if (strcmp(name, "transp") == 0) {
      PyArg_Parse(v, "h", &ival);
      mf->transp = ival;

			return 0;
  }
	else if (strcmp(name, "image") == 0) {
    PyObject *img;
    PyArg_Parse(v, "O", &img);

		if (img == Py_None) {
      mf->image = NULL;

			return 0;
    }

		// XXX if PyType ... XXXXXXX

    mf->image = img;

    return 0;
  }

  return EXPP_ReturnIntError (PyExc_AttributeError, name);
}

static PyObject *NMFace_repr (PyObject *self)
{
  return PyString_FromString("[NMFace]");
}

static int NMFace_len(C_NMFace *self) 
{
  return PySequence_Length(self->v);
}

static PyObject *NMFace_item(C_NMFace *self, int i)
{
  return PySequence_GetItem(self->v, i); // new ref
}

static PyObject *NMFace_slice(C_NMFace *self, int begin, int end)
{
  return PyList_GetSlice(self->v, begin, end); // new ref
}

static PySequenceMethods NMFace_SeqMethods =
{
  (inquiry)     NMFace_len,          /* sq_length */
  (binaryfunc)    0,                 /* sq_concat */
  (intargfunc)    0,                 /* sq_repeat */
  (intargfunc)    NMFace_item,       /* sq_item */
  (intintargfunc)   NMFace_slice,    /* sq_slice */
  (intobjargproc)   0,               /* sq_ass_item */
  (intintobjargproc)  0,             /* sq_ass_slice */
};

PyTypeObject NMFace_Type =
{
  PyObject_HEAD_INIT(&PyType_Type)
  0,                            /*ob_size*/
  "NMFace",                     /*tp_name*/
  sizeof(C_NMFace),             /*tp_basicsize*/
  0,                            /*tp_itemsize*/
  /* methods */
  (destructor) NMFace_dealloc,  /*tp_dealloc*/
  (printfunc) 0,                /*tp_print*/
  (getattrfunc) NMFace_getattr, /*tp_getattr*/
  (setattrfunc) NMFace_setattr, /*tp_setattr*/
  0,                            /*tp_compare*/
  (reprfunc) NMFace_repr,       /*tp_repr*/
  0,                            /*tp_as_number*/
  &NMFace_SeqMethods,           /*tp_as_sequence*/
  0,                            /*tp_as_mapping*/
  0,                            /*tp_hash*/
};

static C_NMVert *newvert(float *co)
{
  C_NMVert *mv = PyObject_NEW(C_NMVert, &NMVert_Type);

  mv->co[0] = co[0]; mv->co[1] = co[1]; mv->co[2] = co[2];

  mv->no[0] = mv->no[1] = mv->no[2] = 0.0;
  mv->uvco[0] = mv->uvco[1] = mv->uvco[2] = 0.0;
  
  return mv;
}

static PyObject *M_NMesh_Vert(PyObject *self, PyObject *args)
{
  float co[3]= {0.0, 0.0, 0.0};
 
  if (!PyArg_ParseTuple(args, "|fff", &co[0], &co[1], &co[2]))
         return EXPP_ReturnPyObjError (PyExc_TypeError,
                  "expected three floats (or nothing) as arguments");

  return (PyObject *)newvert(co);
}

static void NMVert_dealloc(PyObject *self)
{
  PyMem_DEL(self);
}

static PyObject *NMVert_getattr(PyObject *self, char *name)
{
  C_NMVert *mv = (C_NMVert *)self;

  if (!strcmp(name, "co") || !strcmp(name, "loc"))
          return newVectorObject(mv->co, 3);

  else if (strcmp(name, "no") == 0)    return newVectorObject(mv->no, 3);    
  else if (strcmp(name, "uvco") == 0)  return newVectorObject(mv->uvco, 3);    
  else if (strcmp(name, "index") == 0) return PyInt_FromLong(mv->index);    

  return EXPP_ReturnPyObjError (PyExc_AttributeError, name);
}

static int NMVert_setattr(PyObject *self, char *name, PyObject *v)
{
  C_NMVert *mv = (C_NMVert *)self;
  int i;
  
  if (strcmp(name,"index") == 0) {
    PyArg_Parse(v, "i", &i);
    mv->index = i;
    return 0;
  } else if (strcmp(name, "uvco") == 0) {

      if (!PyArg_ParseTuple(v, "ff|f",
              &(mv->uvco[0]), &(mv->uvco[1]), &(mv->uvco[2])))
        return EXPP_ReturnIntError (PyExc_AttributeError,
                      "Vector tuple or triple expected");

    return 0;
  } 
  
  return EXPP_ReturnIntError (PyExc_AttributeError, name);
}

static int NMVert_len(C_NMVert *self)
{
  return 3;
}

static PyObject *NMVert_item(C_NMVert *self, int i)
{
  if (i < 0 || i >= 3)
    return EXPP_ReturnPyObjError (PyExc_IndexError,
               "array index out of range");

  return Py_BuildValue("f", self->co[i]);
}

static PyObject *NMVert_slice(C_NMVert *self, int begin, int end)
{
  PyObject *list;
  int count;
 
  if (begin < 0) begin = 0;
  if (end > 3) end = 3;
  if (begin > end) begin = end;

  list = PyList_New(end-begin);

  for (count = begin; count < end; count++)
    PyList_SetItem(list, count - begin, PyFloat_FromDouble(self->co[count]));

  return list;
}

static int NMVert_ass_item(C_NMVert *self, int i, PyObject *ob)
{
  if (i < 0 || i >= 3)
    return EXPP_ReturnIntError (PyExc_IndexError,
                    "array assignment index out of range");

  if (!PyNumber_Check(ob))
    return EXPP_ReturnIntError (PyExc_IndexError,
                    "NMVert member must be a number");

  self->co[i]= PyFloat_AsDouble(ob);

  return 0;
}

static int NMVert_ass_slice(C_NMVert *self, int begin, int end, PyObject *seq)
{
  int count;
  
  if (begin < 0) begin = 0;
  if (end > 3) end = 3;
  if (begin > end) begin = end;

  if (!PySequence_Check(seq))
    EXPP_ReturnIntError (PyExc_TypeError,
                    "illegal argument type for built-in operation");

  if (PySequence_Length(seq)!=(end-begin))
    EXPP_ReturnIntError (PyExc_TypeError,
                    "size mismatch in slice assignment");
 
  for (count = begin; count < end; count++) {
    PyObject *ob = PySequence_GetItem(seq, count);

    if (!PyArg_Parse(ob, "f", &self->co[count])) {
      Py_DECREF(ob);
      return -1;
    }

    Py_DECREF(ob);
  }

  return 0;
}

static PySequenceMethods NMVert_SeqMethods =
{
  (inquiry)     NMVert_len,              /* sq_length */
  (binaryfunc)    0,                     /* sq_concat */
  (intargfunc)    0,                     /* sq_repeat */
  (intargfunc)    NMVert_item,           /* sq_item */
  (intintargfunc)   NMVert_slice,        /* sq_slice */
  (intobjargproc)   NMVert_ass_item,     /* sq_ass_item */
  (intintobjargproc)  NMVert_ass_slice,  /* sq_ass_slice */
};

PyTypeObject NMVert_Type =
{
  PyObject_HEAD_INIT(&PyType_Type)
  0,                             /*ob_size*/
  "NMVert",                      /*tp_name*/
  sizeof(C_NMVert),              /*tp_basicsize*/
  0,                             /*tp_itemsize*/
  /* methods */
  (destructor) NMVert_dealloc,   /*tp_dealloc*/
  (printfunc) 0,                 /*tp_print*/
  (getattrfunc) NMVert_getattr,  /*tp_getattr*/
  (setattrfunc) NMVert_setattr,  /*tp_setattr*/
  0,                             /*tp_compare*/
  (reprfunc) 0,                  /*tp_repr*/
  0,                             /*tp_as_number*/
  &NMVert_SeqMethods,            /*tp_as_sequence*/
};

static void NMesh_dealloc(PyObject *self)
{
  C_NMesh *me = (C_NMesh *)self;

  Py_DECREF(me->name);
  Py_DECREF(me->verts);
  Py_DECREF(me->faces);
  
  PyMem_DEL(self);
}

static PyObject *NMesh_getSelectedFaces(PyObject *self, PyObject *args)
{
  C_NMesh *nm = (C_NMesh *)self;
  Mesh *me = nm->mesh;
  int flag = 0;

  TFace *tf;
  int i;
  PyObject *l = PyList_New(0);

  if (me == NULL) return NULL;

  tf = me->tface;
  if (tf == 0) return l;

  if (!PyArg_ParseTuple(args, "|i", &flag)) 
    return NULL;

	if (flag) {
    for (i = 0 ; i < me->totface; i++) {
      if (tf[i].flag & TF_SELECT )
				PyList_Append(l, PyInt_FromLong(i));
    }
  } else {
    for (i = 0 ; i < me->totface; i++) {
      if (tf[i].flag & TF_SELECT )
        PyList_Append(l, PyList_GetItem(nm->faces, i));
    }   
  }
  return l;
}

static PyObject *NMesh_getActiveFace(PyObject *self, PyObject *args)
{
  if (((C_NMesh *)self)->sel_face < 0)
    return EXPP_incr_ret(Py_None);

	return Py_BuildValue("i", ((C_NMesh *)self)->sel_face);
}

static PyObject *NMesh_hasVertexUV(PyObject *self, PyObject *args)
{
  C_NMesh *me = (C_NMesh *)self;
  int flag;

  if (args) {
    if (PyArg_ParseTuple(args, "i", &flag)) {
      if(flag) me->flags |= NMESH_HASVERTUV;
      else me->flags &= ~NMESH_HASVERTUV;
    }
  }
  PyErr_Clear();
  if (me->flags & NMESH_HASVERTUV)
    return EXPP_incr_ret(Py_True);
  else
    return EXPP_incr_ret(Py_False);
}

static PyObject *NMesh_hasFaceUV(PyObject *self, PyObject *args)
{
  C_NMesh *me = (C_NMesh *)self;
  int flag = -1;

  if (!PyArg_ParseTuple(args, "|i", &flag))
			  return EXPP_ReturnPyObjError (PyExc_TypeError,
									"expected int argument (or nothing)");

  switch (flag) {
  case 0:
    me->flags |= NMESH_HASFACEUV;
    break;
  case 1: 
    me->flags &= ~NMESH_HASFACEUV;
    break;
  default:
    break;
  }

  if (me->flags & NMESH_HASFACEUV)
    return EXPP_incr_ret(Py_True);
  else
    return EXPP_incr_ret(Py_False);
}

static PyObject *NMesh_hasVertexColours(PyObject *self, PyObject *args)
{
  C_NMesh *me= (C_NMesh *)self;
  int flag = -1;

  if (!PyArg_ParseTuple(args, "|i", &flag))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                 "expected int argument (or nothing)");

  switch (flag) {
  case 0:
    me->flags &= ~NMESH_HASMCOL;
    break;
  case 1: 
    me->flags |= NMESH_HASMCOL;
    break;
  default:
    break;
  }

  if (me->flags & NMESH_HASMCOL)
    return EXPP_incr_ret(Py_True);
  else
    return EXPP_incr_ret(Py_False);
}

static PyObject *NMesh_update(PyObject *self, PyObject *args)
{
  C_NMesh *nmesh = (C_NMesh *)self;
  Mesh *mesh = nmesh->mesh;

  if (mesh) {
    unlink_existingMeshData(mesh);
    convert_NMeshToMesh(mesh, nmesh);
    mesh_update(mesh);
  } else {  
    nmesh->mesh = Mesh_fromNMesh(nmesh);
  }

  nmesh_updateMaterials(nmesh);
/**@ This is another ugly fix due to the weird material handling of blender.
  * it makes sure that object material lists get updated (by their length)
  * according to their data material lists, otherwise blender crashes.
  * It just stupidly runs through all objects...BAD BAD BAD.
  */
  test_object_materials((ID *)mesh);

  if (!during_script())
    allqueue(REDRAWVIEW3D, 0);

  return PyInt_FromLong(1);
}

Mesh *Mesh_fromNMesh(C_NMesh *nmesh)
{
  Mesh *mesh = NULL;
  mesh = add_mesh(); /* us == 1, should we zero it for all added objs ? */

  if (!mesh) {
    EXPP_ReturnPyObjError(PyExc_RuntimeError,
             "FATAL: could not create mesh object");
		return NULL;
	}

  convert_NMeshToMesh(mesh, nmesh);
  mesh_update(mesh);

  return mesh;
}

PyObject *NMesh_link(PyObject *self, PyObject *args) 
{
// XXX  return DataBlock_link(self, args);
	return EXPP_incr_ret(Py_None);
}

#undef MethodDef
#define MethodDef(func) {#func, NMesh_##func, METH_VARARGS, NMesh_##func##_doc}

static struct PyMethodDef NMesh_methods[] =
{
  MethodDef(hasVertexColours),
  MethodDef(hasFaceUV),
  MethodDef(hasVertexUV),
  MethodDef(getActiveFace),
  MethodDef(getSelectedFaces),
  MethodDef(update),
  {NULL, NULL}
};

static PyObject *NMesh_getattr(PyObject *self, char *name)
{
  C_NMesh *me = (C_NMesh *)self;

  if (strcmp(name, "name") == 0) 
    return EXPP_incr_ret(me->name);

  else if (strcmp(name, "block_type") == 0)
    return PyString_FromString("NMesh");

  else if (strcmp(name, "materials") == 0)
    return EXPP_incr_ret(me->materials);

  else if (strcmp(name, "verts") == 0)
    return EXPP_incr_ret(me->verts);

  else if (strcmp(name, "users") == 0) {
    if (me->mesh) {
      return PyInt_FromLong(me->mesh->id.us); 
    }
		else { // it's a free mesh:
      return Py_BuildValue("i", 0); 
    }
  }

  else if (strcmp(name, "faces") == 0)
    return EXPP_incr_ret(me->faces);

  return Py_FindMethod(NMesh_methods, (PyObject*)self, name);
}

static int NMesh_setattr(PyObject *self, char *name, PyObject *v)
{
  C_NMesh *me = (C_NMesh *)self;

  if (!strcmp(name, "verts") || !strcmp(name, "faces") ||
                  !strcmp(name, "materials")) {

    if(PySequence_Check(v)) {

      if(strcmp(name, "materials") == 0) {
        Py_DECREF(me->materials);
        me->materials = EXPP_incr_ret(v);
      }
      else if (strcmp(name, "verts") == 0) {
        Py_DECREF(me->verts);
        me->verts = EXPP_incr_ret(v);
      }
      else {
        Py_DECREF(me->faces);
        me->faces = EXPP_incr_ret(v);       
      }
    }

    else
      return EXPP_ReturnIntError (PyExc_AttributeError, "expected a sequence");
  }

  else
      return EXPP_ReturnIntError (PyExc_AttributeError, name);

  return 0;
}

PyTypeObject NMesh_Type =
{
  PyObject_HEAD_INIT(&PyType_Type)
  0,                             /*ob_size*/
  "NMesh",                       /*tp_name*/
  sizeof(C_NMesh),               /*tp_basicsize*/
  0,                             /*tp_itemsize*/
  /* methods */
  (destructor)  NMesh_dealloc,   /*tp_dealloc*/
  (printfunc)   0,               /*tp_print*/
  (getattrfunc) NMesh_getattr,   /*tp_getattr*/
  (setattrfunc) NMesh_setattr,   /*tp_setattr*/
};

static C_NMFace *nmface_from_data(C_NMesh *mesh, int vidxs[4],
                char mat_nr, char flag, TFace *tface, MCol *col) 
{
  C_NMFace *newf = PyObject_NEW (C_NMFace, &NMFace_Type);
  int i, len;

  if(vidxs[3]) len = 4;
  else if(vidxs[2]) len = 3;
  else len = 2;

  newf->v = PyList_New(len);

  for (i = 0; i < len; i++) 
    PyList_SetItem(newf->v, i,
            EXPP_incr_ret(PyList_GetItem(mesh->verts, vidxs[i])));

  if (tface) {
    newf->uv = PyList_New(len); // per-face UV coordinates

    for (i = 0; i < len; i++) {
      PyList_SetItem(newf->uv, i,
              Py_BuildValue("(ff)", tface->uv[i][0], tface->uv[i][1]));
    }

    if (tface->tpage) /* pointer to image per face: */
      newf->image = NULL;// XXX Image_Get(tface->tpage);
    else
      newf->image = NULL;

    newf->mode = tface->mode;     /* draw mode */
    newf->flag = tface->flag;     /* select flag */
    newf->transp = tface->transp; /* transparency flag */
    col = (MCol *) (tface->col);
  }
	else {
    newf->image = NULL;
    newf->uv = PyList_New(0); 
  } 
  
  newf->mat_nr = mat_nr;
  newf->smooth = flag & ME_SMOOTH;

  if (col) {
    newf->col = PyList_New(4);
    for(i = 0; i < 4; i++, col++)
      PyList_SetItem(newf->col, i, 
        (PyObject *)newcol(col->b, col->g, col->r, col->a));
  }
	else newf->col = PyList_New(0);

  return newf;
}

static C_NMFace *nmface_from_shortdata(C_NMesh *mesh,
                MFace *face, TFace *tface, MCol *col) 
{
  int vidxs[4];
  vidxs[0] = face->v1;
  vidxs[1] = face->v2;
  vidxs[2] = face->v3;
  vidxs[3] = face->v4;

  return nmface_from_data(mesh, vidxs, face->mat_nr, face->flag, tface, col);
}

static C_NMFace *nmface_from_intdata(C_NMesh *mesh,
                MFaceInt *face, TFace *tface, MCol *col) 
{
  int vidxs[4];
  vidxs[0] = face->v1;
  vidxs[1] = face->v2;
  vidxs[2] = face->v3;
  vidxs[3] = face->v4;

  return nmface_from_data(mesh, vidxs, face->mat_nr, face->flag, tface, col);
}

static C_NMVert *nmvert_from_data(C_NMesh *me,
                MVert *vert, MSticky *st, float *co, int idx)
{
  C_NMVert *mv = PyObject_NEW(C_NMVert, &NMVert_Type);

  mv->co[0] = co[0]; mv->co[1] = co[1]; mv->co[2] = co[2];

  mv->no[0] = vert->no[0]/32767.0;
  mv->no[1] = vert->no[1]/32767.0;
  mv->no[2] = vert->no[2]/32767.0;

  if (st) {
    mv->uvco[0] = st->co[0];
    mv->uvco[1] = st->co[1];
    mv->uvco[2] = 0.0;

  } else mv->uvco[0] = mv->uvco[1] = mv->uvco[2] = 0.0;

  mv->index = idx;

  return mv;
}

static int get_active_faceindex(Mesh *me)
{
  TFace *tf;
  int i;

  if (me == NULL) return -1;

  tf = me->tface;
  if (tf == 0) return -1;

  for (i = 0 ; i < me->totface; i++)
    if (tf[i].flag & TF_ACTIVE ) return i;

  return -1;
}

static PyObject *new_NMesh_internal(Mesh *oldmesh,
                DispListMesh *dlm, float *extverts) 
{
  C_NMesh *me = PyObject_NEW(C_NMesh, &NMesh_Type);
  me->flags = 0;

  if (!oldmesh) {
    me->name = EXPP_incr_ret(Py_None);
    me->materials = PyList_New(0);
    me->verts = PyList_New(0);
    me->faces = PyList_New(0);
    me->mesh = 0;
  }
	else {
    MVert *mverts;
    MSticky *msticky;
    MFaceInt *mfaceints;
    MFace *mfaces;
    TFace *tfaces;
    MCol *mcols;
    int i, totvert, totface;

    if (dlm) {
      me->name = EXPP_incr_ret(Py_None);
      me->mesh = 0;

      msticky = NULL;
      mfaces = NULL;
      mverts = dlm->mvert;
      mfaceints = dlm->mface;
      tfaces = dlm->tface;
      mcols = dlm->mcol;

      totvert = dlm->totvert;
      totface = dlm->totface;
    }
		else {
      me->name = PyString_FromString(oldmesh->id.name+2);
      me->mesh = oldmesh;
      
      mfaceints = NULL;
      msticky = oldmesh->msticky;
      mverts = oldmesh->mvert;
      mfaces = oldmesh->mface;
      tfaces = oldmesh->tface;
      mcols = oldmesh->mcol;

      totvert = oldmesh->totvert;
      totface = oldmesh->totface;

      me->sel_face = get_active_faceindex(oldmesh);
    }

    if (msticky) me->flags |= NMESH_HASVERTUV;
    if (tfaces) me->flags |= NMESH_HASFACEUV;
    if (mcols) me->flags |= NMESH_HASMCOL;

    me->verts = PyList_New(totvert);

    for (i = 0; i < totvert; i++) {
      MVert *oldmv = &mverts[i];
      MSticky *oldst = msticky?&msticky[i]:NULL;
      float *vco = extverts?&extverts[i*3]:oldmv->co;

      PyList_SetItem(me->verts, i,
                      (PyObject *)nmvert_from_data(me, oldmv, oldst, vco, i));  
    }

    me->faces = PyList_New(totface);
    for (i = 0; i < totface; i++) {
      TFace *oldtf = tfaces?&tfaces[i]:NULL;
      MCol *oldmc = mcols?&mcols[i*4]:NULL;

      if (mfaceints) {      
        MFaceInt *oldmf = &mfaceints[i];
        PyList_SetItem (me->faces, i,
                        (PyObject *)nmface_from_intdata(me, oldmf, oldtf, oldmc));
      }
			else {
        MFace *oldmf = &mfaces[i];
        PyList_SetItem (me->faces, i,
                        (PyObject *)nmface_from_shortdata(me, oldmf, oldtf, oldmc));
      }
    }
    me->materials = NULL;// XXX PyList_fromMaterialList(oldmesh->mat, oldmesh->totcol);
  }

  return (PyObject *)me;  
}

PyObject *new_NMesh(Mesh *oldmesh) 
{
  return new_NMesh_internal (oldmesh, NULL, NULL);
}

static PyObject *M_NMesh_New(PyObject *self, PyObject *args) 
{
  return new_NMesh(NULL);
}

static PyObject *M_NMesh_GetRaw(PyObject *self, PyObject *args) 
{
  char *name = NULL;
  Mesh *oldmesh = NULL;

  if (!PyArg_ParseTuple(args, "|s", &name))
       return EXPP_ReturnPyObjError (PyExc_TypeError,
               "expected string argument (or nothing)"); 

  if (name) {
    oldmesh = (Mesh *)GetIdFromList(&(G.main->mesh), name);

    if (!oldmesh) return EXPP_incr_ret(Py_None);
  }

  return new_NMesh(oldmesh);
}

static PyObject *M_NMesh_GetRawFromObject(PyObject *self, PyObject *args) 
{
  char *name;
  Object *ob;
  PyObject *nmesh;

  if (!PyArg_ParseTuple(args, "s", &name))
       return EXPP_ReturnPyObjError (PyExc_TypeError,
               "expected string argument");

  ob = (Object*)GetIdFromList(&(G.main->object), name);

  if (!ob)
    return EXPP_ReturnPyObjError (PyExc_AttributeError, name);
  else if (ob->type != OB_MESH)
    return EXPP_ReturnPyObjError (PyExc_AttributeError,
                    "Object does not have Mesh data");
  else {
    Mesh *me = (Mesh*)ob->data;
    DispList *dl;

    if (mesh_uses_displist(me) && (dl = find_displist(&me->disp, DL_MESH)))
      nmesh = new_NMesh_internal(me, dl->mesh, NULL);
    else if ((dl= find_displist(&ob->disp, DL_VERTS)))
      nmesh = new_NMesh_internal(me, NULL, dl->verts);
    else
      nmesh = new_NMesh(me);
  }
  ((C_NMesh *) nmesh)->mesh = 0; // hack: to mark that (deformed) mesh is readonly,
                                 // so the update function will not try to write it.
  return nmesh;
}

static void mvert_from_data(MVert *mv, MSticky *st, C_NMVert *from) 
{
  mv->co[0] = from->co[0]; mv->co[1] = from->co[1]; mv->co[2] = from->co[2];

  mv->no[0] = from->no[0]*32767.0;
  mv->no[1] = from->no[1]*32767.0;
  mv->no[2] = from->no[2]*32767.0;
    
  mv->flag = 0;
  mv->mat_nr = 0;

  if (st) {
    st->co[0] = from->uvco[0];
    st->co[1] = from->uvco[1];
  }
}

/*@ TODO: this function is just a added hack. Don't look at the
 * RGBA/BRGA confusion, it just works, but will never work with
 * a restructured Blender */

static void assign_perFaceColors(TFace *tf, C_NMFace *from)
{
  MCol *col;
  int i;

  col = (MCol *)(tf->col);

  if (col) {
    int len = PySequence_Length(from->col);
    
    if(len > 4) len = 4;
    
    for (i = 0; i < len; i++, col++) {
      C_NMCol *mc = (C_NMCol *)PySequence_GetItem(from->col, i);
      if(!C_NMCol_Check(mc)) {
        Py_DECREF(mc);
        continue;
      }

      col->r = mc->b;
      col->b = mc->r;
      col->g = mc->g;
      col->a = mc->a;

      Py_DECREF(mc);
    }
  }
}

static int assignFaceUV(TFace *tf, C_NMFace *nmface)
{
  PyObject *fuv, *tmp;
  int i;

  fuv = nmface->uv;
  if (PySequence_Length(fuv) == 0)
    return 0;
  /* fuv = [(u_1, v_1), ... (u_n, v_n)] */
  for (i = 0; i < PySequence_Length(fuv); i++) {
    tmp = PyList_GetItem(fuv, i); /* stolen reference ! */
    if (!PyArg_ParseTuple(tmp, "ff", &(tf->uv[i][0]), &(tf->uv[i][1])))
      return 0;
  }
  if (nmface->image) /* image assigned ? */
  {
    tf->tpage = nmface->image; 
  }
  else
    tf->tpage = 0;

  tf->mode = nmface->mode; /* copy mode */
  tf->flag = nmface->flag; /* copy flag */
  tf->transp = nmface->transp; /* copy transp flag */

  /* assign vertex colours */
  assign_perFaceColors(tf, nmface);
  return 1;
}

static void mface_from_data(MFace *mf, TFace *tf, MCol *col, C_NMFace *from)
{
  C_NMVert *nmv;

  int i = PyList_Size(from->v);
  if(i >= 1) {
    nmv = (C_NMVert *)PyList_GetItem(from->v, 0);
    if (C_NMVert_Check(nmv) && nmv->index != -1) mf->v1 = nmv->index;
    else mf->v1 = 0;
  }
  if(i >= 2) {
    nmv = (C_NMVert *)PyList_GetItem(from->v, 1);
    if (C_NMVert_Check(nmv) && nmv->index != -1) mf->v2 = nmv->index;
    else mf->v2 = 0;
  }
  if(i >= 3) {
    nmv = (C_NMVert *)PyList_GetItem(from->v, 2);
    if (C_NMVert_Check(nmv) && nmv->index != -1) mf->v3 = nmv->index;
    else mf->v3= 0;
  }
  if(i >= 4) {
    nmv = (C_NMVert *)PyList_GetItem(from->v, 3);
    if (C_NMVert_Check(nmv) && nmv->index != -1) mf->v4 = nmv->index;
    else mf->v4= 0;
  }

  if (tf) {
    assignFaceUV(tf, from);
    if (PyErr_Occurred())
    {
      PyErr_Print();
      return;
    }

    test_index_face(mf, tf, i);
  }
	else {
    test_index_mface(mf, i);
  }

  mf->puno = 0;
  mf->mat_nr = from->mat_nr;
  mf->edcode = 0;
  if (from->smooth) 
    mf->flag = ME_SMOOTH;
  else
    mf->flag = 0;

  if (col) {
    int len = PySequence_Length(from->col);

    if(len > 4) len = 4;

    for (i = 0; i < len; i++, col++) {
      C_NMCol *mc = (C_NMCol *) PySequence_GetItem(from->col, i);
      if(!C_NMCol_Check(mc)) {
        Py_DECREF(mc);
        continue;
      }

      col->b = mc->r;
      col->g = mc->g;
      col->r = mc->b;
      col->a = mc->a;

      Py_DECREF(mc);
    }
  }
}

/* check for a valid UV sequence */
static int check_validFaceUV(C_NMesh *nmesh)
{
  PyObject *faces;
  C_NMFace *nmface;
  int i, n;

  faces = nmesh->faces;
  for (i = 0; i < PySequence_Length(faces); i++) {
    nmface = (C_NMFace *)PyList_GetItem(faces, i);
    n = PySequence_Length(nmface->uv);
    if (n != PySequence_Length(nmface->v))
    {
      if (n > 0) 
        printf("Warning: different length of vertex and UV coordinate "
               "list in face!\n");
      return 0;
    } 
  }
  return 1;
}

static int unlink_existingMeshData(Mesh *mesh)
{
  freedisplist(&mesh->disp);
  unlink_mesh(mesh);
  if(mesh->mvert) MEM_freeN(mesh->mvert);
  if(mesh->mface) MEM_freeN(mesh->mface);
  if(mesh->mcol) MEM_freeN(mesh->mcol);
  if(mesh->msticky) MEM_freeN(mesh->msticky);
  if(mesh->mat) MEM_freeN(mesh->mat);
  if(mesh->tface) MEM_freeN(mesh->tface);
  return 1;
}

Material **nmesh_updateMaterials(C_NMesh *nmesh)
{
  Material **matlist;
  Mesh *mesh = nmesh->mesh;
  int len = PySequence_Length(nmesh->materials);

  if (!mesh) {
    printf("FATAL INTERNAL ERROR: illegal call to updateMaterials()\n");
    return 0;
  }

  if (len > 0) {
    matlist = newMaterialList_fromPyList(nmesh->materials);
    if (mesh->mat)
      MEM_freeN(mesh->mat);
    mesh->mat = matlist;
  } else {
    matlist = 0;
  }
  mesh->totcol = len;
  return matlist;
}

PyObject *NMesh_assignMaterials_toObject(C_NMesh *nmesh, Object *ob)
{
//  DataBlock *block;
//  Material *ma;
//  int i;
//  short old_matmask;

  //old_matmask = ob->colbits; // HACK: save previous colbits
  //ob->colbits = 0;           // make assign_material work on mesh linked material

//  for (i = 0; i < PySequence_Length(nmesh->materials); i++) {
//    block= (DataBlock *) PySequence_GetItem(nmesh->materials, i);
    
  //  if (DataBlock_isType(block, ID_MA)) {
    //  ma = (Material *) block->data;
   //   assign_material(ob, ma, i+1); // XXX don't use this function anymore
//    } else {
  //    PyErr_SetString(PyExc_TypeError, 
    //  "Material type in attribute list 'materials' expected!");
      //Py_DECREF(block);
     // return NULL;
//    } 
    
    //Py_DECREF(block);
  //}
  //ob->colbits = old_matmask; // HACK

//  ob->actcol = 1;
  return EXPP_incr_ret(Py_None);
}

static int convert_NMeshToMesh(Mesh *mesh, C_NMesh *nmesh)
{
  MFace *newmf;
  TFace *newtf;
  MVert *newmv;
  MSticky *newst;
  MCol *newmc;

  int i, j;

  mesh->mvert = NULL;
  mesh->mface = NULL;
  mesh->mcol = NULL;
  mesh->msticky = NULL;
  mesh->tface = NULL;
  mesh->mat = NULL;

  // material assignment moved to PutRaw
  mesh->totvert = PySequence_Length(nmesh->verts);
  if (mesh->totvert) {
    if (nmesh->flags&NMESH_HASVERTUV)
      mesh->msticky = MEM_callocN(sizeof(MSticky)*mesh->totvert, "msticky");

    mesh->mvert = MEM_callocN(sizeof(MVert)*mesh->totvert, "mverts");
  }

  if (mesh->totvert)
    mesh->totface = PySequence_Length(nmesh->faces);
  else
    mesh->totface = 0;

  if (mesh->totface) {
/*@ only create vertcol array if mesh has no texture faces */

/*@ TODO: get rid of double storage of vertex colours. In a mesh,
 * vertex colors can be stored the following ways:
 * - per (TFace*)->col
 * - per (Mesh*)->mcol
 * This is stupid, but will reside for the time being -- at least until
 * a redesign of the internal Mesh structure */

    if (!(nmesh->flags & NMESH_HASFACEUV) && (nmesh->flags&NMESH_HASMCOL))
      mesh->mcol = MEM_callocN(4*sizeof(MCol)*mesh->totface, "mcol");

    mesh->mface = MEM_callocN(sizeof(MFace)*mesh->totface, "mfaces");
  }

  /*@ This stuff here is to tag all the vertices referenced
   * by faces, then untag the vertices which are actually
   * in the vert list. Any vertices untagged will be ignored
   * by the mface_from_data function. It comes from my
   * screwed up decision to not make faces only store the
   * index. - Zr
   */
  for (i = 0; i < mesh->totface; i++) {
    C_NMFace *mf = (C_NMFace *)PySequence_GetItem(nmesh->faces, i);
      
    j = PySequence_Length(mf->v);
    while (j--) {
      C_NMVert *mv = (C_NMVert *)PySequence_GetItem(mf->v, j);
      if (C_NMVert_Check(mv)) mv->index = -1;
      Py_DECREF(mv);
    }

    Py_DECREF(mf);
  }

  for (i = 0; i < mesh->totvert; i++) {
    C_NMVert *mv = (C_NMVert *)PySequence_GetItem(nmesh->verts, i);
    mv->index = i;
    Py_DECREF(mv);
  }

  newmv = mesh->mvert;
  newst = mesh->msticky;
  for (i = 0; i < mesh->totvert; i++) {
    PyObject *mv = PySequence_GetItem (nmesh->verts, i);
    mvert_from_data(newmv, newst, (C_NMVert *)mv);
    Py_DECREF(mv);

    newmv++;
    if (newst) newst++;
  }

/*  assign per face texture UVs */

  /* check face UV flag, then check whether there was one 
   * UV coordinate assigned, if yes, make tfaces */
  if ((nmesh->flags & NMESH_HASFACEUV) || (check_validFaceUV(nmesh))) {
    make_tfaces(mesh); /* initialize TFaces */

    newmc = mesh->mcol;
    newmf = mesh->mface;
    newtf = mesh->tface;
    for (i = 0; i<mesh->totface; i++) {
      PyObject *mf = PySequence_GetItem(nmesh->faces, i);
      mface_from_data(newmf, newtf, newmc, (C_NMFace *) mf);
      Py_DECREF(mf);

      newtf++;
      newmf++;
      if (newmc) newmc++;
    }

    nmesh->flags |= NMESH_HASFACEUV;
  }
	else {
    newmc = mesh->mcol;
    newmf = mesh->mface;

    for (i = 0; i < mesh->totface; i++) {
      PyObject *mf = PySequence_GetItem(nmesh->faces, i);
      mface_from_data(newmf, 0, newmc, (C_NMFace *) mf);
      Py_DECREF(mf);

      newmf++;
      if (newmc) newmc++;
    }
  }
  return 1;
}

static PyObject *M_NMesh_PutRaw(PyObject *self, PyObject *args) 
{
  char *name = NULL;
  Mesh *mesh = NULL;
  Object *ob = NULL;
  C_NMesh *nmesh;
  int recalc_normals = 1;

  if (!PyArg_ParseTuple(args, "O!|si",
                          &NMesh_Type, &nmesh, &name, &recalc_normals))
      return EXPP_ReturnPyObjError (PyExc_AttributeError,
        "expected an NMesh object and optionally also a string and an int");

  if (!PySequence_Check(nmesh->verts))
    return EXPP_ReturnPyObjError (PyExc_AttributeError,
                    "nmesh vertices are not a sequence");
  if (!PySequence_Check(nmesh->faces))
    return EXPP_ReturnPyObjError (PyExc_AttributeError,
                    "nmesh faces are not a sequence");
  if (!PySequence_Check(nmesh->materials))
    return EXPP_ReturnPyObjError (PyExc_AttributeError,
                    "nmesh materials are not a sequence");

  if (!EXPP_check_sequence_consistency(nmesh->verts, &NMVert_Type))
    return EXPP_ReturnPyObjError (PyExc_AttributeError,
                    "nmesh vertices must be NMVerts");
  if (!EXPP_check_sequence_consistency(nmesh->faces, &NMFace_Type))
    return EXPP_ReturnPyObjError (PyExc_AttributeError,
                    "nmesh faces must be NMFaces");

  if (name) 
    mesh = (Mesh *)GetIdFromList(&(G.main->mesh), name);
 
  if(!mesh || mesh->id.us == 0) {
    ob = add_object(OB_MESH);
    if (!ob) {
      PyErr_SetString(PyExc_RuntimeError, "Fatal: could not create mesh object");
      return 0;
    }
    if (mesh)
      set_mesh(ob, mesh);
    else
      mesh = (Mesh *)ob->data;
  }
  if (name) new_id(&(G.main->mesh), &mesh->id, name);

  unlink_existingMeshData(mesh);
  convert_NMeshToMesh(mesh, nmesh);
  nmesh->mesh = mesh;

  if(recalc_normals)
    vertexnormals_mesh(mesh, 0);

  mesh_update(mesh);
  
  if (!during_script())
    allqueue(REDRAWVIEW3D, 0);

  // OK...this requires some explanation:
  // Materials can be assigned two ways:
  // a) to the object data (in this case, the mesh)
  // b) to the Object
  //
  // Case a) is wanted, if Mesh data should be shared among objects,
  // as well as its materials (up to 16)
  // Case b) is wanted, when Mesh data should be shared, but not the
  // materials. For example, you want several checker boards sharing their
  // mesh data, but having different colors. So you would assign material
  // index 0 to all even, index 1 to all odd faces and bind the materials
  // to the Object instead (MaterialButtons: [OB] button "link materials to object")
  //
  // This feature implies that pointers to materials can be stored in
  // an object or a mesh. The number of total materials MUST be
  // synchronized (ob->totcol <-> mesh->totcol). We avoid the dangerous
  // direct access by calling blenderkernel/material.c:assign_material().

  // The flags setting the material binding is found in ob->colbits, where 
  // each bit indicates the binding PER MATERIAL 

  if (ob) { // we created a new object
    NMesh_assignMaterials_toObject(nmesh, ob);
    //return DataBlock_fromData(ob); /* XXX fix this */
    return EXPP_incr_ret (Py_None);
  }
	else
    return EXPP_incr_ret (Py_None);
}

#undef MethodDef
#define MethodDef(func) {#func, M_NMesh_##func, METH_VARARGS, M_NMesh_##func##_doc}

static struct PyMethodDef M_NMesh_methods[] = {
// These should be: Mesh.Col, Mesh.Vert, Mesh.Face in fure
// -- for ownership reasons
  MethodDef(Col),
  MethodDef(Vert),
  MethodDef(Face),
  MethodDef(New),
  MethodDef(GetRaw),
  MethodDef(GetRawFromObject),
  MethodDef(PutRaw),
  {NULL, NULL}
};

#undef EXPP_ADDCONST
#define EXPP_ADDCONST(dict, name) \
       constant_insert(dict, #name, PyInt_FromLong(TF_##name))

/*@ set constants for face drawing mode -- see drawmesh.c */

static void init_NMeshConst(C_constant *d)
{
  constant_insert(d, "BILLBOARD", PyInt_FromLong(TF_BILLBOARD2));
  constant_insert(d, "ALL", PyInt_FromLong(0xffff));
  constant_insert(d, "HALO", PyInt_FromLong(TF_BILLBOARD));
  EXPP_ADDCONST(d, DYNAMIC);
  EXPP_ADDCONST(d, INVISIBLE);
  EXPP_ADDCONST(d, LIGHT);
  EXPP_ADDCONST(d, OBCOL);
  EXPP_ADDCONST(d, SHADOW);
  EXPP_ADDCONST(d, SHAREDVERT);
  EXPP_ADDCONST(d, SHAREDCOL);
  EXPP_ADDCONST(d, TEX);
  EXPP_ADDCONST(d, TILES);
  EXPP_ADDCONST(d, TWOSIDE);
/* transparent modes */
  EXPP_ADDCONST(d, SOLID);
  EXPP_ADDCONST(d, ADD);
  EXPP_ADDCONST(d, ALPHA);
  EXPP_ADDCONST(d, SUB);
/* TFACE flags */
  EXPP_ADDCONST(d, SELECT);
  EXPP_ADDCONST(d, HIDE);
  EXPP_ADDCONST(d, ACTIVE);
}

PyObject *M_NMesh_Init (void) 
{
  PyObject *mod = Py_InitModule("Blender.NMesh", M_NMesh_methods);
  PyObject *dict = PyModule_GetDict(mod);
  PyObject *d = M_constant_New();

	PyDict_SetItemString(dict, "Const" , d);
  init_NMeshConst((C_constant *)d);

  g_nmeshmodule = mod;
  return mod;
}

/* Unimplemented stuff: */

Material **newMaterialList_fromPyList (PyObject *list) { return NULL; }
