/*  python.c      MIXED MODEL
 * 
 *  june 99
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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "Python.h"
#include "BPY_macros.h"
#include "b_interface.h"
#include "BPY_tools.h"
#include "BPY_main.h"

#include "opy_datablock.h"
#include "opy_nmesh.h"

#include "MEM_guardedalloc.h"
#include "BIF_editmesh.h" /* vertexnormals_mesh() */
#include "BDR_editface.h" /* make_tfaces */

#include "BKE_mesh.h"
#include "BKE_main.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_displist.h"
#include "BKE_screen.h"
#include "BKE_object.h"
#include "BPY_objtypes.h"
#include "BLI_blenlib.h"
#include "BIF_space.h"

#include "opy_vector.h"

#include "b_interface.h"
/* PROTOS */

static int convert_NMeshToMesh(Mesh *mesh, NMesh *nmesh);
static int unlink_existingMeshdata(Mesh *mesh);
void initNMesh(void);
PyObject *init_py_nmesh(void);
int BPY_check_sequence_consistency(PyObject *seq, PyTypeObject *against);

/* TYPE OBJECTS */

PyTypeObject NMesh_Type;
PyTypeObject NMFace_Type;
PyTypeObject NMVert_Type;
PyTypeObject NMCol_Type;

/* DEFINES */


#define COL_R (b)
#define COL_G (g)
#define COL_B (r)
#define COL_A (a)

#define COLOR_CONVERT(col,comp) (col##->COL_##)

/* GLOBALS */

static PyObject *g_nmeshmodule = NULL;

/*****************************/
/*	    Mesh Color Object    */
/*****************************/

static void NMCol_dealloc(PyObject *self) {
	PyMem_DEL(self);
}

static NMCol *newcol (char r, char g, char b, char a) {
	NMCol *mc= (NMCol *) PyObject_NEW(NMCol, &NMCol_Type);
	
	mc->r= r;
	mc->g= g;
	mc->b= b;
	mc->a= a;

	return mc;	
}

static char NMeshmodule_Col_doc[]=
"([r, g, b, a]) - Get a new mesh color\n\
\n\
[r=255, g=255, b=255, a=255] Specify the color components";

static PyObject *NMeshmodule_Col(PyObject *self, PyObject *args) {
	int r=255, g=255, b=255, a=255;
	
/*
if(PyArg_ParseTuple(args, "fff|f", &fr, &fg, &fb, &fa))
		return (PyObject *) newcol(255.0 * fr, 255.0 * fg, 255.0 * fb, 255.0 * fa);
		*/
	if(PyArg_ParseTuple(args, "|iiii", &r, &g, &b, &a))
		return (PyObject *) newcol(r, g, b, a);
	return NULL;		
}

static PyObject *NMCol_getattr(PyObject *self, char *name) {
	NMCol *mc= (NMCol *) self;
		
	if (strcmp(name, "r")==0) return Py_BuildValue("i", mc->r);
	else if (strcmp(name, "g")==0) return Py_BuildValue("i", mc->g);
	else if (strcmp(name, "b")==0) return Py_BuildValue("i", mc->b);
	else if (strcmp(name, "a")==0) return Py_BuildValue("i", mc->a);

	PyErr_SetString(PyExc_AttributeError, name);
	return NULL;
}

static int NMCol_setattr(PyObject *self, char *name, PyObject *v) {
	NMCol *mc= (NMCol *) self;
	int ival;
	
	if(!PyArg_Parse(v, "i", &ival)) return -1;
	
	CLAMP(ival, 0, 255);
	
	if (strcmp(name, "r")==0) mc->r= ival;
	else if (strcmp(name, "g")==0) mc->g= ival;
	else if (strcmp(name, "b")==0) mc->b= ival;
	else if (strcmp(name, "a")==0) mc->a= ival;
	else return -1;
	
	return 0;
}

PyObject *NMCol_repr(NMCol *self) 
{
	static char s[256];
	sprintf (s, "[NMCol - <%d, %d, %d, %d>]", self->r, self->g, self->b, self->a);
	return Py_BuildValue("s", s);
}

PyTypeObject NMCol_Type = {
	PyObject_HEAD_INIT(NULL)
	0,								/*ob_size*/
	"NMCol",						/*tp_name*/
	sizeof(NMCol),					/*tp_basicsize*/
	0,								/*tp_itemsize*/
	/* methods */
	(destructor) NMCol_dealloc,		/*tp_dealloc*/
	(printfunc) 0,		/*tp_print*/
	(getattrfunc) NMCol_getattr,	/*tp_getattr*/
	(setattrfunc) NMCol_setattr,	/*tp_setattr*/
	0,								/*tp_compare*/
	(reprfunc) NMCol_repr,								/*tp_repr*/
	0,								/*tp_as_number*/
	0,								/*tp_as_sequence*/
	0,								/*tp_as_mapping*/
	0,								/*tp_hash*/
	0,								/*tp_as_number*/
	0,								/*tp_as_sequence*/
	0,								/*tp_as_mapping*/
	0,								/*tp_hash*/
};


/*****************************/
/*    NMesh Python Object    */
/*****************************/


static void NMFace_dealloc(PyObject *self) {
	NMFace *mf= (NMFace *) self;
	
	Py_DECREF(mf->v);
	Py_DECREF(mf->uv);
	Py_DECREF(mf->col);
	
	PyMem_DEL(self);

}

static NMFace *newNMFace(PyObject *vertexlist) {
	NMFace *mf= PyObject_NEW(NMFace, &NMFace_Type);

	mf->v= vertexlist;
	mf->uv= PyList_New(0);
	mf->tpage= NULL;
	mf->mode = TF_DYNAMIC + TF_TEX;
	mf->flag= TF_SELECT;
	mf->transp= TF_SOLID;
	mf->col= PyList_New(0);
	
	mf->smooth= 0;
	mf->mat_nr= 0;
	
	return mf;
}

static char NMeshmodule_Face_doc[]=
"(vertexlist = None) - Get a new face, and pass optional vertex list";
static PyObject *NMeshmodule_Face(PyObject *self, PyObject *args) {
	PyObject *vertlist = NULL;
	BPY_TRY(PyArg_ParseTuple(args, "|O!", &PyList_Type, &vertlist));	

	if (!vertlist) {
		vertlist = PyList_New(0);
	}
	return (PyObject *) newNMFace(vertlist);
}

/* XXX this code will be used later...
static PyObject *Method_getmode(PyObject *self, PyObject *args) {
	PyObject *dict, *list;
	PyObject *constants, *values, *c;
	int flag;
	int i, n;

	list = PyList_New(0);
	dict = PyObject_GetAttrString(g_nmeshmodule, "Const");

	if (!dict) return 0;

	constants = PyDict_Keys(dict);
	values = PyDict_Values(dict);
	
	n = PySequence_Length(constants);
	for (i = 0; i < n; i++)
	{
		flag = PyInt_AsLong(PySequence_GetItem(values, i));
		if (flag & ((NMFace*) self)->mode)
		{
			c = PySequence_GetItem(constants, i);
			PyList_Append(list, c) 
		}	
	}
	return list;
}
*/

static char NMFace_append_doc[]= "(vert) - appends Vertex 'vert' to face vertex list";

static PyObject *NMFace_append(PyObject *self, PyObject *args)
{
	PyObject *vert;
	NMFace *f= (NMFace *) self;

	BPY_TRY(PyArg_ParseTuple(args, "O!", &NMVert_Type, &vert));
	PyList_Append(f->v, vert);
	RETURN_INC(Py_None);
}


#undef MethodDef
#define MethodDef(func) {#func, NMFace_##func, METH_VARARGS, NMFace_##func##_doc}

static struct PyMethodDef NMFace_methods[] = {
	MethodDef(append),
	{NULL, NULL}
};

static PyObject *NMFace_getattr(PyObject *self, char *name) {
	NMFace *mf= (NMFace *) self;
		
	if(strcmp(name, "v")==0) 
		return Py_BuildValue("O", mf->v);
	else if (strcmp(name, "col")==0) 
		return Py_BuildValue("O", mf->col);
	else if (strcmp(name, "mat")==0) // emulation XXX
		return Py_BuildValue("i", mf->mat_nr);
	else if (strcmp(name, "materialIndex")==0) 
		return Py_BuildValue("i", mf->mat_nr);
	else if (strcmp(name, "smooth")==0)
		return Py_BuildValue("i", mf->smooth);
	else if (strcmp(name, "image")==0) {
		if (mf->tpage)
			return Py_BuildValue("O", (PyObject *) mf->tpage);
		else 
			RETURN_INC(Py_None);
	}
	else if (strcmp(name, "mode")==0) 
		return Py_BuildValue("i", mf->mode);
	else if (strcmp(name, "flag")==0) 
		return Py_BuildValue("i", mf->flag);
	else if (strcmp(name, "transp")==0)
		return Py_BuildValue("i", mf->transp);
	else if (strcmp(name, "uv")==0)
		return Py_BuildValue("O", mf->uv);
		
	return Py_FindMethod(NMFace_methods, (PyObject*)self, name);
/*
	PyErr_SetString(PyExc_AttributeError, name);
	return NULL;
*/
}

static int NMFace_setattr(PyObject *self, char *name, PyObject *v) {
	NMFace *mf= (NMFace *) self;
	int ival;
	PyObject *tmp;
	
	if (STREQ(name, "v")) {
		if(PySequence_Check(v)) {
			Py_DECREF(mf->v);
			mf->v= BPY_incr_ret(v);

			return 0;
		}
	} else if (STREQ(name, "col")) {
		if(PySequence_Check(v)) {
			Py_DECREF(mf->col);
			mf->col= BPY_incr_ret(v);

			return 0;
		}
	} else if (STREQ(name, "mat") || STREQ(name, "materialIndex")) {
		PyArg_Parse(v, "i", &ival);

		mf->mat_nr= ival;
		
		return 0;
	} else if (STREQ(name, "smooth")) {
		PyArg_Parse(v, "i", &ival);

		mf->smooth= ival?1:0;
		
		return 0;
	} else if (STREQ(name, "uv")) {
		if(PySequence_Check(v)) {
			Py_DECREF(mf->uv);
			mf->uv= BPY_incr_ret(v);
			return 0;
		}	
	} else if (STREQ(name, "flag")) {
			PyArg_Parse(v, "i", &ival);
			mf->flag = ival;
			return 0;
	} else if (STREQ(name, "mode")) {
			PyArg_Parse(v, "i", &ival);
			mf->mode = ival;
			return 0;
	} else if (STREQ(name, "transp")) {
			PyArg_Parse(v, "i", &ival);
			mf->transp = ival;
			return 0;
	} else if (STREQ(name, "image")) {
		PyArg_Parse(v, "O", &tmp);
		if (tmp == Py_None) {
			mf->tpage = 0;
			return 0;
		}
		if (!DataBlock_isType((DataBlock *) tmp, ID_IM))
		{
			PyErr_SetString(PyExc_TypeError, "expects Image Datablock type");
			return -1;
		}
		mf->tpage = (DataBlock *) tmp;
		return 0;
	}
	
	PyErr_SetString(PyExc_AttributeError, name);
	return -1;
}

static PyObject *NMFace_repr (PyObject *self)
{
	return PyString_FromString("[NMFace]");
}

static int NMFace_len(NMFace *self) 
{
	return PySequence_Length(self->v);
}

static PyObject *NMFace_item(NMFace *self, int i)
{
	return PySequence_GetItem(self->v, i); // new ref
}

static PyObject *NMFace_slice(NMFace *self, int begin, int end)
{
	return PyList_GetSlice(self->v, begin, end); // new ref
}

static PySequenceMethods NMFace_SeqMethods = {
	(inquiry)			NMFace_len,			/* sq_length	*/
	(binaryfunc)		0,					/* sq_concat	*/
	(intargfunc)		0,					/* sq_repeat	*/
	(intargfunc)		NMFace_item,		/* sq_item		*/
	(intintargfunc)		NMFace_slice,		/* sq_slice		*/
	(intobjargproc)		0,	/* sq_ass_item	*/
	(intintobjargproc)	0,	/* sq_ass_slice	*/
};


PyTypeObject NMFace_Type = {
	PyObject_HEAD_INIT(NULL)
	0,							/*ob_size*/
	"NMFace",						/*tp_name*/
	sizeof(NMFace),			/*tp_basicsize*/
	0,							/*tp_itemsize*/
	/* methods */
	(destructor) NMFace_dealloc,	/*tp_dealloc*/
	(printfunc) 0,		/*tp_print*/
	(getattrfunc) NMFace_getattr,	/*tp_getattr*/
	(setattrfunc) NMFace_setattr,/*tp_setattr*/
	0,							/*tp_compare*/
	(reprfunc) NMFace_repr,		/*tp_repr*/
	0,							/*tp_as_number*/
	&NMFace_SeqMethods,							/*tp_as_sequence*/
	0,							/*tp_as_mapping*/
	0,							/*tp_hash*/
};


static NMVert *newvert(float *co) {
	NMVert *mv= PyObject_NEW(NMVert, &NMVert_Type);

	VECCOPY(mv->co, co);
	mv->no[0]= mv->no[1]= mv->no[2]= 0.0;
	mv->uvco[0]= mv->uvco[1]= mv->uvco[2]= 0.0;
	
	return mv;
}

static char NMeshmodule_Vert_doc[]=
"([x, y, z]) - Get a new vertice\n\
\n\
[x, y, z] Specify new coordinates";

static PyObject *NMeshmodule_Vert(PyObject *self, PyObject *args) {
	float co[3]= {0.0, 0.0, 0.0};
	
	BPY_TRY(PyArg_ParseTuple(args, "|fff", &co[0], &co[1], &co[2]));
	
	return (PyObject *) newvert(co);
}

static void NMVert_dealloc(PyObject *self) {
	PyMem_DEL(self);
}

static PyObject *NMVert_getattr(PyObject *self, char *name) {
	NMVert *mv= (NMVert *) self;

	if (STREQ(name, "co") || STREQ(name, "loc")) return newVectorObject(mv->co, 3);
	else if (STREQ(name, "no")) return newVectorObject(mv->no, 3);		
	else if (STREQ(name, "uvco")) return newVectorObject(mv->uvco, 3);		
	else if (STREQ(name, "index")) return PyInt_FromLong(mv->index);		
	
	PyErr_SetString(PyExc_AttributeError, name);
	return NULL;
}

static int NMVert_setattr(PyObject *self, char *name, PyObject *v) {
	NMVert *mv= (NMVert *) self;
	int i;
	
	if (STREQ(name,"index")) {
		PyArg_Parse(v, "i", &i);
		mv->index= i;
		return 0;
	} else if (STREQ(name, "uvco")) {
		if (!PyArg_ParseTuple(v, "ff|f", &(mv->uvco[0]), &(mv->uvco[1]), &(mv->uvco[2]))) {
			PyErr_SetString(PyExc_AttributeError, "Vector tuple or triple expected");
			return -1;
		}	
		return 0;
/*
PyErr_SetString(PyExc_AttributeError, "Use slice assignment: uvco[i]");
		return -1;
		*/
	}	
	
	PyErr_SetString(PyExc_AttributeError, name);
	return -1;
}


static int NMVert_len(NMVert *self) {
	return 3;
}

static PyObject *NMVert_item(NMVert *self, int i)
{
	if (i < 0 || i >= 3) {
		PyErr_SetString(PyExc_IndexError, "array index out of range");
		return NULL;
	}
	return Py_BuildValue("f", self->co[i]);
}

static PyObject *NMVert_slice(NMVert *self, int begin, int end)
{
	PyObject *list;
	int count;
	
	if (begin<0) begin= 0;
	if (end>3) end= 3;
	if (begin>end) begin= end;
		
	list= PyList_New(end-begin);

	for (count= begin; count<end; count++)
		PyList_SetItem(list, count-begin, PyFloat_FromDouble(self->co[count]));
	
	return list;
}

static int NMVert_ass_item(NMVert *self, int i, PyObject *ob)
{
	if (i < 0 || i >= 3) {
		PyErr_SetString(PyExc_IndexError, "array assignment index out of range");
		return -1;
	}

	if (!PyNumber_Check(ob)) {
		PyErr_SetString(PyExc_IndexError, "NMVert member must be a number");
		return -1;
	}
	
	self->co[i]= PyFloat_AsDouble(ob);
/* 	if(!PyArg_Parse(ob, "f", &)) return -1; */
	
	return 0;
}

/** I guess this hurts...
  * sorry, couldn't resist (strubi) */

static int NMVert_ass_slice(NMVert *self, int begin, int end, PyObject *seq)
{
	int count;
	
	if (begin<0) begin= 0;
	if (end>3) end= 3;
	if (begin>end) begin= end;

	if (!PySequence_Check(seq)) {
		PyErr_SetString(PyExc_TypeError, "illegal argument type for built-in operation");
		return -1;		
	}

	if (PySequence_Length(seq)!=(end-begin)) {
		PyErr_SetString(PyExc_TypeError, "size mismatch in slice assignment");
		return -1;
	}
	
	for (count= begin; count<end; count++) {
		PyObject *ob= PySequence_GetItem(seq, count);
		if (!PyArg_Parse(ob, "f", &self->co[count])) {
			Py_DECREF(ob);
			return -1;
		}
		Py_DECREF(ob);
	}
		
	return 0;
}

static PySequenceMethods NMVert_SeqMethods = {
	(inquiry)			NMVert_len,			/* sq_length	*/
	(binaryfunc)		0,					/* sq_concat	*/
	(intargfunc)		0,					/* sq_repeat	*/
	(intargfunc)		NMVert_item,		/* sq_item		*/
	(intintargfunc)		NMVert_slice,		/* sq_slice		*/
	(intobjargproc)		NMVert_ass_item,	/* sq_ass_item	*/
	(intintobjargproc)	NMVert_ass_slice,	/* sq_ass_slice	*/
};

PyTypeObject NMVert_Type = {
	PyObject_HEAD_INIT(NULL)
	0,                             /*ob_size*/
	"NMVert",                      /*tp_name*/
	sizeof(NMVert),                /*tp_basicsize*/
	0,                             /*tp_itemsize*/
	/* methods */
	(destructor) NMVert_dealloc,   /*tp_dealloc*/
	(printfunc) 0,	               /*tp_print*/
	(getattrfunc) NMVert_getattr,  /*tp_getattr*/
	(setattrfunc) NMVert_setattr,  /*tp_setattr*/
	0,                             /*tp_compare*/
	(reprfunc) 0,                  /*tp_repr*/
	0,                             /*tp_as_number*/
	&NMVert_SeqMethods,            /*tp_as_sequence*/
};


static void NMesh_dealloc(PyObject *self) {
	NMesh *me= (NMesh *) self;

	Py_DECREF(me->name);
	Py_DECREF(me->verts);
	Py_DECREF(me->faces);
	
	PyMem_DEL(self);
}


static char NMesh_getSelectedFaces_doc[] = "(flag = None) - returns list of selected Faces\n\
If flag = 1, return indices instead";
static PyObject *NMesh_getSelectedFaces(PyObject *self, PyObject *args)
{
	NMesh *nm= (NMesh *) self;
	Mesh *me = nm->mesh;
	int flag = 0;

	TFace *tf;
	int i;
	PyObject *l= PyList_New(0);

	if (me == NULL) return NULL;

	tf = me->tface;
	if (tf == 0) {
		return l;
	}

	if (!PyArg_ParseTuple(args, "|i", &flag)) 
		return NULL;
	if (flag) {
		for (i =0 ; i < me->totface; i++) {
			if (tf[i].flag & TF_SELECT ) {
				PyList_Append(l, PyInt_FromLong(i));
			}
		}		
	} else {
		for (i =0 ; i < me->totface; i++) {
			if (tf[i].flag & TF_SELECT ) {
				PyList_Append(l, PyList_GetItem(nm->faces, i));
			}
		}		
	}
	return l;
}


static char NMesh_getActiveFace_doc[] = "returns the index of the active face ";
static PyObject *NMesh_getActiveFace(PyObject *self, PyObject *args)
{
	if (((NMesh *)self)->sel_face < 0)
		RETURN_INC(Py_None);
	return Py_BuildValue("i", ((NMesh *)self)->sel_face);
}

static char NMesh_hasVertexUV_doc[] = "(flag = None) - returns 1 if Mesh has per vertex UVs ('Sticky')\n\
The optional argument sets the Sticky flag";

static PyObject *NMesh_hasVertexUV(PyObject *self, PyObject *args)
{
	NMesh *me= (NMesh *) self;
	int flag;

	if (args) {
		if (PyArg_ParseTuple(args, "i", &flag)) {
			if(flag) me->flags |= NMESH_HASVERTUV;
			else me->flags &= ~NMESH_HASVERTUV;
		}
	}
	PyErr_Clear();
	if (me->flags & NMESH_HASVERTUV)
		return BPY_incr_ret(Py_True);
	else
		return BPY_incr_ret(Py_False);
}

static char NMesh_hasFaceUV_doc[] = "(flag = None) - returns 1 if Mesh has textured faces\n\
The optional argument sets the textured faces flag";

static PyObject *NMesh_hasFaceUV(PyObject *self, PyObject *args)
{
	NMesh *me= (NMesh *) self;
	int flag = -1;

	BPY_TRY(PyArg_ParseTuple(args, "|i", &flag));

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
		return BPY_incr_ret(Py_True);
	else
		return BPY_incr_ret(Py_False);
}


static char NMesh_hasVertexColours_doc[] = "(flag = None) - returns 1 if Mesh has vertex colours.\n\
The optional argument sets the vertex colour flag";

static PyObject *NMesh_hasVertexColours(PyObject *self, PyObject *args)
{
	NMesh *me= (NMesh *) self;
	int flag = -1;

	BPY_TRY(PyArg_ParseTuple(args, "|i", &flag));

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
		return BPY_incr_ret(Py_True);
	else
		return BPY_incr_ret(Py_False);

}


static char NMesh_update_doc[] = "updates the Mesh";
static PyObject *NMesh_update(PyObject *self, PyObject *args)
{
	NMesh *nmesh= (NMesh *) self;
	Mesh *mesh = nmesh->mesh;
	
	if (mesh) {
		unlink_existingMeshdata(mesh);
		convert_NMeshToMesh(mesh, nmesh);
		mesh_update(mesh);
	} else {  
		nmesh->mesh = Mesh_fromNMesh(nmesh);
	}

	nmesh_updateMaterials(nmesh);
/** This is another ugly fix due to the weird material handling of blender.
  * it makes sure that object material lists get updated (by their length)
  * according to their data material lists, otherwise blender crashes.
  * It just stupidly runs through all objects...BAD BAD BAD.
  */
	test_object_materials((ID *)mesh);

	if (!during_script())
		allqueue(REDRAWVIEW3D, 0);
	return PyInt_FromLong(1);

}


Mesh *Mesh_fromNMesh(NMesh *nmesh)
{
	Mesh *mesh= NULL;
	mesh = mesh_new(); // new empty mesh Bobject
	if (!mesh) {
		PyErr_SetString(PyExc_RuntimeError, "FATAL: could not create mesh object");
		return NULL;
	}
	
	convert_NMeshToMesh(mesh, nmesh);
	mesh_update(mesh);
	return mesh;
}

#ifdef EXPERIMENTAL

static char NMesh_asMesh_doc[] = "returns free Mesh datablock object from NMesh";
static PyObject *NMesh_asMesh(PyObject *self, PyObject *args)
{
	char *name= NULL;
	Mesh *mesh= NULL;
	NMesh *nmesh;
	int recalc_normals= 1;

	nmesh = (NMesh *) self;
	
	BPY_TRY(PyArg_ParseTuple(args, "|si", &name, &recalc_normals));
	
	if (!PySequence_Check(nmesh->verts))
		return BPY_err_ret_ob(PyExc_AttributeError, 
			"nmesh vertices are not a sequence");
	if (!PySequence_Check(nmesh->faces))
		return BPY_err_ret_ob(PyExc_AttributeError, 
			"nmesh faces are not a sequence");
	if (!PySequence_Check(nmesh->materials))
		return BPY_err_ret_ob(PyExc_AttributeError, 
			"nmesh materials are not a sequence");
	if (!BPY_check_sequence_consistency(nmesh->verts, &NMVert_Type))
		return BPY_err_ret_ob(PyExc_AttributeError, 
			"nmesh vertices must be NMVerts");
	if (!BPY_check_sequence_consistency(nmesh->faces, &NMFace_Type))
		return BPY_err_ret_ob(PyExc_AttributeError, 
			"nmesh faces must be NMFaces");

	mesh = Mesh_fromNMesh(nmesh);
	return DataBlock_fromData(mesh);
}

#endif
static char NMesh_link_doc[] = "(object) - Links NMesh data with Object 'object'";

PyObject * NMesh_link(PyObject *self, PyObject *args) 
{
	return DataBlock_link(self, args);
}

#undef MethodDef
#define MethodDef(func) {#func, NMesh_##func, METH_VARARGS, NMesh_##func##_doc}

static struct PyMethodDef NMesh_methods[] = {
	MethodDef(hasVertexColours),
	MethodDef(hasFaceUV),
	MethodDef(hasVertexUV),
	MethodDef(getActiveFace),
	MethodDef(getSelectedFaces),
	MethodDef(update),
#ifdef EXPERIMENTAL	
	MethodDef(asMesh),
#endif	
	{NULL, NULL}
};

static PyObject *NMesh_getattr(PyObject *self, char *name) {
	NMesh *me= (NMesh *) self;
	
	if (STREQ(name, "name")) 
		return BPY_incr_ret(me->name);
	
	else if (STREQ(name, "block_type"))
	  return PyString_FromString("NMesh");

	else if (STREQ(name, "materials"))
		return BPY_incr_ret(me->materials);

	else if (STREQ(name, "verts"))
		return BPY_incr_ret(me->verts);
		
	else if (STREQ(name, "users")) {
		if (me->mesh) {
			return PyInt_FromLong(me->mesh->id.us); 
		} else { // it's a free mesh:
			return Py_BuildValue("i", 0); 
		}
	}

	else if (STREQ(name, "faces"))
		return BPY_incr_ret(me->faces);

	return Py_FindMethod(NMesh_methods, (PyObject*)self, name);
	
	PyErr_SetString(PyExc_AttributeError, name);
	return NULL;
}

static int NMesh_setattr(PyObject *self, char *name, PyObject *v) {
	NMesh *me= (NMesh *) self;

	if (STREQ3(name, "verts", "faces", "materials")) {
		if(PySequence_Check(v)) {
			if(STREQ(name, "materials")) {
				Py_DECREF(me->materials);
				me->materials= BPY_incr_ret(v);
			} else if (STREQ(name, "verts")) {
				Py_DECREF(me->verts);
				me->verts= BPY_incr_ret(v);
			} else {
				Py_DECREF(me->faces);
				me->faces= BPY_incr_ret(v);				
			}
		} else {
			PyErr_SetString(PyExc_AttributeError, "expected a sequence");
			return -1;
		}
	} else {
		PyErr_SetString(PyExc_AttributeError, name);
		return -1;
	}

	return 0;
}

PyTypeObject NMesh_Type = {
	PyObject_HEAD_INIT(NULL)
	0,								/*ob_size*/
	"NMesh",						/*tp_name*/
	sizeof(NMesh),					/*tp_basicsize*/
	0,								/*tp_itemsize*/
	/* methods */
	(destructor)	NMesh_dealloc,	/*tp_dealloc*/
	(printfunc)		0,	/*tp_print*/
	(getattrfunc)	NMesh_getattr,	/*tp_getattr*/
	(setattrfunc)	NMesh_setattr,	/*tp_setattr*/
};

static NMFace *nmface_from_data(NMesh *mesh, int vidxs[4], char mat_nr, char flag, TFace *tface, MCol *col) 
{
	NMFace *newf= PyObject_NEW(NMFace, &NMFace_Type);
	int i, len;

	if(vidxs[3]) len= 4;
	else if(vidxs[2]) len= 3;
	else len= 2;

	newf->v= PyList_New(len);

	for (i=0; i<len; i++)	
		PyList_SetItem(newf->v, i, BPY_incr_ret(PyList_GetItem(mesh->verts, vidxs[i])));

	if (tface) {
		newf->uv = PyList_New(len); // per-face UV coordinates
		for (i = 0; i < len; i++)
		{
			PyList_SetItem(newf->uv, i, Py_BuildValue("(ff)", tface->uv[i][0], tface->uv[i][1]));
		}
		if (tface->tpage)
			newf->tpage = (DataBlock *) DataBlock_fromData((void *) tface->tpage); /* pointer to image per face */
		else
			newf->tpage = 0;
		newf->mode = tface->mode;     /* draw mode */
		newf->flag = tface->flag;		/* select flag */
		newf->transp = tface->transp;  /* transparency flag */
		col = (MCol *) (tface->col);
	} else {
		newf->tpage = 0;
		newf->uv = PyList_New(0); 
	}	
	
	newf->mat_nr= mat_nr;
	newf->smooth= flag&ME_SMOOTH;

	if (col) {
		newf->col= PyList_New(4);
		for(i=0; i<4; i++, col++)
			PyList_SetItem(newf->col, i, 
				(PyObject *) newcol(col->b, col->g, col->r, col->a));
	} else {
		newf->col= PyList_New(0);
	}
	return newf;
}

static NMFace *nmface_from_shortdata(NMesh *mesh, MFace *face, TFace *tface, MCol *col) 
{
	int vidxs[4];
	vidxs[0]= face->v1;
	vidxs[1]= face->v2;
	vidxs[2]= face->v3;
	vidxs[3]= face->v4;
		
	return nmface_from_data(mesh, vidxs, face->mat_nr, face->flag, tface, col);
}

static NMFace *nmface_from_intdata(NMesh *mesh, MFaceInt *face, TFace *tface, MCol *col) 
{
	int vidxs[4];
	vidxs[0]= face->v1;
	vidxs[1]= face->v2;
	vidxs[2]= face->v3;
	vidxs[3]= face->v4;
		
	return nmface_from_data(mesh, vidxs, face->mat_nr, face->flag, tface, col);
}

static NMVert *nmvert_from_data(NMesh *me, MVert *vert, MSticky *st, float *co, int idx)
{			
	NMVert *mv= PyObject_NEW(NMVert, &NMVert_Type);
			
	VECCOPY (mv->co, co);
		
	mv->no[0]= vert->no[0]/32767.0;
	mv->no[1]= vert->no[1]/32767.0;
	mv->no[2]= vert->no[2]/32767.0;
	
	if (st) {
		mv->uvco[0]= st->co[0];
		mv->uvco[1]= st->co[1];
		mv->uvco[2]= 0.0;
		
	} else mv->uvco[0]= mv->uvco[1]= mv->uvco[2]= 0.0;

	mv->index= idx;
			
	return mv;
}

static int get_active_faceindex(Mesh *me)
{
	TFace *tf;
	int i;

	if (me == NULL) return -1;

	tf = me->tface;
	if (tf == 0) return -1;
	
	for (i =0 ; i < me->totface; i++) {
		if (tf[i].flag & TF_ACTIVE ) {
			return i;
		}
	}		
	return -1;
}

static PyObject *newNMesh_internal(Mesh *oldmesh, DispListMesh *dlm, float *extverts) 
{
	NMesh *me= PyObject_NEW(NMesh, &NMesh_Type);
	me->flags= 0;

	if (!oldmesh) {
		me->name= BPY_incr_ret(Py_None);
		me->materials= PyList_New(0);
		me->verts= PyList_New(0);
		me->faces= PyList_New(0);
		me->mesh= 0;
	} else {
		MVert *mverts;
		MSticky *msticky;
		MFaceInt *mfaceints;
		MFace *mfaces;
		TFace *tfaces;
		MCol *mcols;
		int i, totvert, totface;
		
		if (dlm) {
			me->name= BPY_incr_ret(Py_None);
			me->mesh= 0;

			msticky= NULL;
			mfaces= NULL;
			mverts= dlm->mvert;
			mfaceints= dlm->mface;
			tfaces= dlm->tface;
			mcols= dlm->mcol;
			
			totvert= dlm->totvert;
			totface= dlm->totface;
		} else {
			me->name= PyString_FromString(oldmesh->id.name+2);
			me->mesh= oldmesh;
			
			mfaceints= NULL;
			msticky= oldmesh->msticky;
			mverts= oldmesh->mvert;
			mfaces= oldmesh->mface;
			tfaces= oldmesh->tface;
			mcols= oldmesh->mcol;

			totvert= oldmesh->totvert;
			totface= oldmesh->totface;

			me->sel_face= get_active_faceindex(oldmesh);
		}

		if (msticky) me->flags |= NMESH_HASVERTUV;
		if (tfaces) me->flags |= NMESH_HASFACEUV;
		if (mcols) me->flags |= NMESH_HASMCOL;

		me->verts= PyList_New(totvert);
		for (i=0; i<totvert; i++) {
			MVert *oldmv= &mverts[i];
			MSticky *oldst= msticky?&msticky[i]:NULL;
			float *vco= extverts?&extverts[i*3]:oldmv->co;
			
			PyList_SetItem(me->verts, i, (PyObject *) nmvert_from_data(me, oldmv, oldst, vco, i));	
		}

		me->faces= PyList_New(totface);
		for (i=0; i<totface; i++) {
			TFace *oldtf= tfaces?&tfaces[i]:NULL;
			MCol *oldmc= mcols?&mcols[i*4]:NULL;

			if (mfaceints) {			
				MFaceInt *oldmf= &mfaceints[i];
				PyList_SetItem(me->faces, i, (PyObject *) nmface_from_intdata(me, oldmf, oldtf, oldmc));
			} else {
				MFace *oldmf= &mfaces[i];
				PyList_SetItem(me->faces, i, (PyObject *) nmface_from_shortdata(me, oldmf, oldtf, oldmc));
			}
		}
		me->materials = PyList_fromMaterialList(oldmesh->mat, oldmesh->totcol);
	}
	
	return (PyObject *) me;	
}

PyObject *newNMesh(Mesh *oldmesh) 
{
	return newNMesh_internal(oldmesh, NULL, NULL);
}

static char NMeshmodule_New_doc[]=
"() - returns a new, empty NMesh mesh object\n";

static PyObject *NMeshmodule_New(PyObject *self, PyObject *args) 
{
	return newNMesh(NULL);
}

static char NMeshmodule_GetRaw_doc[]=
"([name]) - Get a raw mesh from Blender\n\
\n\
[name] Name of the mesh to be returned\n\
\n\
If name is not specified a new empty mesh is\n\
returned, otherwise Blender returns an existing\n\
mesh.";

static PyObject *NMeshmodule_GetRaw(PyObject *self, PyObject *args) 
{
	char *name=NULL;
	Mesh *oldmesh=NULL;
	
	BPY_TRY(PyArg_ParseTuple(args, "|s", &name));	

	if(name) {
		oldmesh = (Mesh *) getFromList(getMeshList(), name);

		if (!oldmesh) return BPY_incr_ret(Py_None);
	}
	return newNMesh(oldmesh);
}

static char NMeshmodule_GetRawFromObject_doc[]=
"(name) - Get the raw mesh used by a Blender object\n"
"\n"
"(name) Name of the object to get the mesh from\n"
"\n"
"This returns the mesh as used by the object, which\n"
"means it contains all deformations and modifications.";

static PyObject *NMeshmodule_GetRawFromObject(PyObject *self, PyObject *args) 
{
	char *name;
	Object *ob;
	PyObject *nmesh;
	
	BPY_TRY(PyArg_ParseTuple(args, "s", &name));
	
	ob= (Object*) getFromList(getObjectList(), name);
	if (!ob)
		return BPY_err_ret_ob(PyExc_AttributeError, name);
	else if (ob->type!=OB_MESH)
		return BPY_err_ret_ob(PyExc_AttributeError, "Object does not have Mesh data");
	else {
		Mesh *me= (Mesh*) ob->data;
		DispList *dl;
		
		if (mesh_uses_displist(me) && (dl= find_displist(&me->disp, DL_MESH)))
			nmesh = newNMesh_internal(me, dl->mesh, NULL);
		else if ((dl= find_displist(&ob->disp, DL_VERTS)))
			nmesh = newNMesh_internal(me, NULL, dl->verts);
		else
			nmesh = newNMesh(me);
	}
	((NMesh *) nmesh)->mesh = 0; // hack: to mark that (deformed) mesh is readonly,
                                 // so the update function will not try to write it.
	return nmesh;
}

static void mvert_from_data(MVert *mv, MSticky *st, NMVert *from) 
{
	VECCOPY (mv->co, from->co);
	mv->no[0]= from->no[0]*32767.0;
	mv->no[1]= from->no[1]*32767.0;
	mv->no[2]= from->no[2]*32767.0;
		
	mv->flag= 0;
	mv->mat_nr= 0;

	if (st) {
		st->co[0]= from->uvco[0];
		st->co[1]= from->uvco[1];
	}
}

/* TODO: this function is just a added hack. Don't look at the
 * RGBA/BRGA confusion, it just works, but will never work with
 * a restructured Blender */

static void assign_perFaceColors(TFace *tf, NMFace *from)
{
	MCol *col;
	int i;

	col = (MCol *) (tf->col);

	if (col) {
		int len= PySequence_Length(from->col);
		
		if(len>4) len= 4;
		
		for (i=0; i<len; i++, col++) {
			NMCol *mc= (NMCol *) PySequence_GetItem(from->col, i);
			if(!NMCol_Check(mc)) {
				Py_DECREF(mc);
				continue;
			}
			
			col->r= mc->b;
			col->b= mc->r;
			col->g= mc->g;
			col->a= mc->a;

			Py_DECREF(mc);
		}
	}
}

static int assignFaceUV(TFace *tf, NMFace *nmface)
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
	if (nmface->tpage) /* image assigned ? */
	{
		tf->tpage = nmface->tpage->data; 
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

static void mface_from_data(MFace *mf, TFace *tf, MCol *col, NMFace *from)
{
	NMVert *nmv;

	int i= PyList_Size(from->v);
	if(i>=1) {
		nmv= (NMVert *) PyList_GetItem(from->v, 0);
		if (NMVert_Check(nmv) && nmv->index!=-1) mf->v1= nmv->index;
		else mf->v1= 0;
	}
	if(i>=2) {
		nmv= (NMVert *) PyList_GetItem(from->v, 1);
		if (NMVert_Check(nmv) && nmv->index!=-1) mf->v2= nmv->index;
		else mf->v2= 0;
	}
	if(i>=3) {
		nmv= (NMVert *) PyList_GetItem(from->v, 2);
		if (NMVert_Check(nmv) && nmv->index!=-1) mf->v3= nmv->index;
		else mf->v3= 0;
	}
	if(i>=4) {
		nmv= (NMVert *) PyList_GetItem(from->v, 3);
		if (NMVert_Check(nmv) && nmv->index!=-1) mf->v4= nmv->index;
		else mf->v4= 0;
	}

	/*	this function is evil: 

			test_index_mface(mf, i);

		It rotates vertex indices, if there are illegal '0's (end marker)
		in the vertex index list.
		But it doesn't do that with vertex colours or texture coordinates...
	*/

	if (tf) {
		assignFaceUV(tf, from);
		if (PyErr_Occurred())
		{
			PyErr_Print();
			return;
		}

		test_index_face(mf, tf, i);
	} else {
		test_index_mface(mf, i);
	}

	mf->puno= 0;
	mf->mat_nr= from->mat_nr;
	mf->edcode= 0;
	if (from->smooth) 
		mf->flag= ME_SMOOTH;
	else
		mf->flag= 0;
	
	if (col) {
		int len= PySequence_Length(from->col);
		
		if(len>4) len= 4;
		
		for (i=0; i<len; i++, col++) {
			NMCol *mc= (NMCol *) PySequence_GetItem(from->col, i);
			if(!NMCol_Check(mc)) {
				Py_DECREF(mc);
				continue;
			}
			
			col->b= mc->r;
			col->g= mc->g;
			col->r= mc->b;
			col->a= mc->a;

			Py_DECREF(mc);
		}
	}
}


/* check for a valid UV sequence */
static int check_validFaceUV(NMesh *nmesh)
{
	PyObject *faces;
	NMFace *nmface;
	int i, n;

	faces = nmesh->faces;
	for (i = 0; i < PySequence_Length(faces); i++) {
		nmface = (NMFace *) PyList_GetItem(faces, i);
		n = 
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

static int unlink_existingMeshdata(Mesh *mesh)
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

Material **nmesh_updateMaterials(NMesh *nmesh)
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

PyObject *NMesh_assignMaterials_toObject(NMesh *nmesh, Object *ob)
{
	DataBlock *block;
	Material *ma;
	int i;
	short old_matmask;

	old_matmask = ob->colbits; // HACK: save previous colbits
	ob->colbits = 0;           // make assign_material work on mesh linked material

	for (i= 0; i < PySequence_Length(nmesh->materials); i++) {
		block= (DataBlock *) PySequence_GetItem(nmesh->materials, i);
		
		if (DataBlock_isType(block, ID_MA)) {
			ma = (Material *) block->data;
			assign_material(ob, ma, i+1); // XXX don't use this function anymore
		} else {
			PyErr_SetString(PyExc_TypeError, 
			"Material type in attribute list 'materials' expected!");
			Py_DECREF(block);
			return NULL;
		}	
		
		Py_DECREF(block);
	}
	ob->colbits = old_matmask; // HACK

	ob->actcol = 1;
	RETURN_INC(Py_None);
}

static int convert_NMeshToMesh(Mesh *mesh, NMesh *nmesh)
{
	MFace *newmf;
	TFace *newtf;
	MVert *newmv;
	MSticky *newst;
	MCol *newmc;

	int i, j;

	mesh->mvert= NULL;
	mesh->mface= NULL;
	mesh->mcol= NULL;
	mesh->msticky= NULL;
	mesh->tface = NULL;
	mesh->mat= NULL;

	// material assignment moved to PutRaw
	mesh->totvert= PySequence_Length(nmesh->verts);
	if (mesh->totvert) {
		if (nmesh->flags&NMESH_HASVERTUV)
			mesh->msticky= MEM_callocN(sizeof(MSticky)*mesh->totvert, "msticky");

		mesh->mvert= MEM_callocN(sizeof(MVert)*mesh->totvert, "mverts");
	}

	if (mesh->totvert)
		mesh->totface= PySequence_Length(nmesh->faces);
	else
		mesh->totface= 0;


	if (mesh->totface) {

/* only create vertcol array if mesh has no texture faces */

/* TODO: get rid of double storage of vertex colours. In a mesh,
 * vertex colors can be stored the following ways:
 * - per (TFace*)->col
 * - per (Mesh*)->mcol
 * This is stupid, but will reside for the time being -- at least until
 * a redesign of the internal Mesh structure */

		if (!(nmesh->flags & NMESH_HASFACEUV) && (nmesh->flags&NMESH_HASMCOL))
			mesh->mcol= MEM_callocN(4*sizeof(MCol)*mesh->totface, "mcol");
			
		mesh->mface= MEM_callocN(sizeof(MFace)*mesh->totface, "mfaces");
	}

	/* This stuff here is to tag all the vertices referenced
	 * by faces, then untag the vertices which are actually
	 * in the vert list. Any vertices untagged will be ignored
	 * by the mface_from_data function. It comes from my
	 * screwed up decision to not make faces only store the
	 * index. - Zr
	 */
	for (i=0; i<mesh->totface; i++) {
		NMFace *mf= (NMFace *) PySequence_GetItem(nmesh->faces, i);
			
		j= PySequence_Length(mf->v);
		while (j--) {
			NMVert *mv= (NMVert *) PySequence_GetItem(mf->v, j);
			if (NMVert_Check(mv)) mv->index= -1;
			Py_DECREF(mv);
		}
		
		Py_DECREF(mf);
	}
	
	for (i=0; i<mesh->totvert; i++) {
		NMVert *mv= (NMVert *) PySequence_GetItem(nmesh->verts, i);
		mv->index= i;
		Py_DECREF(mv);
	}	
	
	newmv= mesh->mvert;
	newst= mesh->msticky;
	for (i=0; i<mesh->totvert; i++) {
		PyObject *mv=  PySequence_GetItem(nmesh->verts, i);
		mvert_from_data(newmv, newst, (NMVert *)mv);
		Py_DECREF(mv);
		
		newmv++;
		if (newst) newst++;
	}

/*  assign per face texture UVs */

	/* check face UV flag, then check whether there was one 
	 * UV coordinate assigned, if yes, make tfaces */
	if ((nmesh->flags & NMESH_HASFACEUV) || (check_validFaceUV(nmesh))) {
		make_tfaces(mesh); /* initialize TFaces */

		newmc= mesh->mcol;
		newmf= mesh->mface;
		newtf= mesh->tface;
		for (i=0; i<mesh->totface; i++) {
			PyObject *mf= PySequence_GetItem(nmesh->faces, i);
			mface_from_data(newmf, newtf, newmc, (NMFace *) mf);
			Py_DECREF(mf);
				
			newtf++;
			newmf++;
			if (newmc) newmc++;
		}

		nmesh->flags |= NMESH_HASFACEUV;
	} else {

		newmc= mesh->mcol;
		newmf= mesh->mface;
		for (i=0; i<mesh->totface; i++) {
			PyObject *mf= PySequence_GetItem(nmesh->faces, i);
			mface_from_data(newmf, 0, newmc, (NMFace *) mf);
			Py_DECREF(mf);
				
			newmf++;
			if (newmc) newmc++;
		}
	}
	return 1;
}



static char NMeshmodule_PutRaw_doc[]=
"(mesh, [name, renormal]) - Return a raw mesh to Blender\n\
\n\
(mesh) The NMesh object to store\n\
[name] The mesh to replace\n\
[renormal=1] Flag to control vertex normal recalculation\n\
\n\
If the name of a mesh to replace is not given a new\n\
object is created and returned.";

static PyObject *NMeshmodule_PutRaw(PyObject *self, PyObject *args) 
{
	char *name= NULL;
	Mesh *mesh= NULL;
	Object *ob= NULL;
	NMesh *nmesh;
	int recalc_normals= 1;
	
	BPY_TRY(PyArg_ParseTuple(args, "O!|si", &NMesh_Type, &nmesh, &name, &recalc_normals));
	
	if (!PySequence_Check(nmesh->verts))
		return BPY_err_ret_ob(PyExc_AttributeError, "nmesh vertices are not a sequence");
	if (!PySequence_Check(nmesh->faces))
		return BPY_err_ret_ob(PyExc_AttributeError, "nmesh faces are not a sequence");
	if (!PySequence_Check(nmesh->materials))
		return BPY_err_ret_ob(PyExc_AttributeError, "nmesh materials are not a sequence");

	if (!BPY_check_sequence_consistency(nmesh->verts, &NMVert_Type))
		return BPY_err_ret_ob(PyExc_AttributeError, "nmesh vertices must be NMVerts");
	if (!BPY_check_sequence_consistency(nmesh->faces, &NMFace_Type))
		return BPY_err_ret_ob(PyExc_AttributeError, "nmesh faces must be NMFaces");
	
	if (name) 
		mesh= (Mesh *) getFromList(getMeshList(), name);
	/* returns new mesh if not found */
	
	if(!mesh || mesh->id.us==0) {
		ob= add_object(OB_MESH);
		if (!ob) {
			PyErr_SetString(PyExc_RuntimeError, "Fatal: could not create mesh object");
			return 0;
		}
		if (mesh)
			set_mesh(ob, mesh);
		else
			mesh= (Mesh *) ob->data;
	}
	if(name) new_id(getMeshList(), &mesh->id, name);
	
 	unlink_existingMeshdata(mesh);
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
		return DataBlock_fromData(ob);
	} else {
		RETURN_INC(Py_None);
	}	
}

#undef MethodDef
#define MethodDef(func) {#func, NMeshmodule_##func, METH_VARARGS, NMeshmodule_##func##_doc}

static struct PyMethodDef NMeshmodule_methods[] = {
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
#undef BPY_ADDCONST
#define BPY_ADDCONST(dict, name) insertConst(dict, #name, PyInt_FromLong(TF_##name))

/* set constants for face drawing mode -- see drawmesh.c */

static void init_NMeshConst(PyObject *d)
{
	insertConst(d, "BILLBOARD", PyInt_FromLong(TF_BILLBOARD2));
	//BPY_ADDCONST(d, BILLBOARD);
	insertConst(d, "ALL", PyInt_FromLong(0xffff));
	BPY_ADDCONST(d, DYNAMIC);
	BPY_ADDCONST(d, INVISIBLE);
	insertConst(d, "HALO", PyInt_FromLong(TF_BILLBOARD));
	BPY_ADDCONST(d, LIGHT);
	BPY_ADDCONST(d, OBCOL);
	BPY_ADDCONST(d, SHADOW);
	BPY_ADDCONST(d, SHAREDVERT);
	BPY_ADDCONST(d, SHAREDCOL);
	BPY_ADDCONST(d, TEX);
	BPY_ADDCONST(d, TILES);
	BPY_ADDCONST(d, TWOSIDE);
/* transparent modes */
	BPY_ADDCONST(d, SOLID);
	BPY_ADDCONST(d, ADD);
	BPY_ADDCONST(d, ALPHA);
	BPY_ADDCONST(d, SUB);
/* TFACE flags */
	BPY_ADDCONST(d, SELECT);
	BPY_ADDCONST(d, HIDE);
	BPY_ADDCONST(d, ACTIVE);
}

PyObject *init_py_nmesh(void) 
{
	PyObject *d;
	PyObject *mod= Py_InitModule(SUBMODULE(NMesh), NMeshmodule_methods);
	PyObject *dict= PyModule_GetDict(mod);

	NMesh_Type.ob_type= &PyType_Type;	
	NMVert_Type.ob_type= &PyType_Type;	
	NMFace_Type.ob_type= &PyType_Type;	
	NMCol_Type.ob_type= &PyType_Type;	
	
	d = ConstObject_New();
	PyDict_SetItemString(dict, "Const" , d);
	init_NMeshConst(d);

	g_nmeshmodule = mod;
	return mod;
}

#ifdef SHAREDMODULE
void initNMesh(void) 
{
	init_py_nmesh();
}
#endif
