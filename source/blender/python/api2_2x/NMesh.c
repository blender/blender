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
 * Contributor(s): Willian P. Germano, Jordi Rovira i Bonet, Joseph Gilbert,
 * Bala Gi
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "NMesh.h"

/* EXPP Mesh defines */

#define EXPP_NMESH_MODE_NOPUNOFLIP	ME_NOPUNOFLIP
#define EXPP_NMESH_MODE_TWOSIDED		ME_TWOSIDED
#define EXPP_NMESH_MODE_AUTOSMOOTH	ME_AUTOSMOOTH
#define EXPP_NMESH_MODE_SUBSURF			ME_SUBSURF
#define EXPP_NMESH_MODE_OPTIMAL			ME_OPT_EDGES

#define NMESH_FRAME_MAX				18000
#define NMESH_SMOOTHRESH			30
#define NMESH_SMOOTHRESH_MIN	1
#define NMESH_SMOOTHRESH_MAX	80
#define NMESH_SUBDIV					1
#define NMESH_SUBDIV_MIN			1
#define NMESH_SUBDIV_MAX			6

void mesh_update(Mesh *mesh)
{
	edge_drawflags_mesh(mesh);
	tex_space_mesh(mesh);
}

/*****************************/
/*			Mesh Color Object		 */
/*****************************/

static void NMCol_dealloc(PyObject *self)
{
	PyObject_DEL(self);
}

static BPy_NMCol *newcol (char r, char g, char b, char a)
{
	BPy_NMCol *mc = (BPy_NMCol *) PyObject_NEW (BPy_NMCol, &NMCol_Type);

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
	BPy_NMCol *mc = (BPy_NMCol *)self;

	if (strcmp(name, "r") == 0) return Py_BuildValue("i", mc->r);
	else if (strcmp(name, "g") == 0) return Py_BuildValue("i", mc->g);
	else if (strcmp(name, "b") == 0) return Py_BuildValue("i", mc->b);
	else if (strcmp(name, "a") == 0) return Py_BuildValue("i", mc->a);
	else if (strcmp(name, "__members__") == 0)
		return Py_BuildValue("[s,s,s,s]", "r", "g", "b", "a");

	return EXPP_ReturnPyObjError(PyExc_AttributeError, name);
}

static int NMCol_setattr(PyObject *self, char *name, PyObject *v)
{
	BPy_NMCol *mc = (BPy_NMCol *)self;
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

PyObject *NMCol_repr(BPy_NMCol *self) 
{
	static char s[256];
	sprintf (s, "[NMCol - <%d, %d, %d, %d>]", self->r, self->g, self->b, self->a);
	return Py_BuildValue("s", s);
}

PyTypeObject NMCol_Type =
{
	PyObject_HEAD_INIT(NULL)
	0,														/* ob_size */
	"Blender NMCol",							/* tp_name */
	sizeof(BPy_NMCol),						/* tp_basicsize */
	0,														/* tp_itemsize */
	/* methods */
	(destructor) NMCol_dealloc,		/* tp_dealloc */
	(printfunc) 0,								/* tp_print */
	(getattrfunc) NMCol_getattr,	/* tp_getattr */
	(setattrfunc) NMCol_setattr,	/* tp_setattr */
	0,														/* tp_compare */
	(reprfunc) NMCol_repr,				/* tp_repr */
	0,														/* tp_as_number */
	0,														/* tp_as_sequence */
	0,														/* tp_as_mapping */
	0,														/* tp_hash */
	0,														/* tp_as_number */
	0,														/* tp_as_sequence */
	0,														/* tp_as_mapping */
	0,														/* tp_hash */
};

/*****************************/
/*		NMesh Python Object		 */
/*****************************/
static void NMFace_dealloc (PyObject *self)
{
	BPy_NMFace *mf = (BPy_NMFace *)self;

	Py_DECREF(mf->v);
	Py_DECREF(mf->uv);
	Py_DECREF(mf->col);

	PyObject_DEL(self);
}

static PyObject *new_NMFace(PyObject *vertexlist)
{
	BPy_NMFace *mf = PyObject_NEW (BPy_NMFace, &NMFace_Type);
	PyObject *vlcopy;

	if (vertexlist) { /* create a copy of the given vertex list */
		PyObject *item;
		int i, len = PyList_Size(vertexlist);

		vlcopy = PyList_New(len);

		if (!vlcopy)
			return EXPP_ReturnPyObjError(PyExc_MemoryError,
					"couldn't create PyList");		

		for (i = 0; i < len; i++) {
			item = PySequence_GetItem(vertexlist, i); /* PySequence increfs */

			if (item)
				PyList_SET_ITEM(vlcopy, i, item);
			else
				return EXPP_ReturnPyObjError(PyExc_RuntimeError,
							"couldn't get vertex from a PyList");
		}
	}
	else /* create an empty vertex list */
		vlcopy = PyList_New(0);

	mf->v = vlcopy;
	mf->uv = PyList_New(0);
	mf->image = NULL;
	mf->mode = TF_DYNAMIC + TF_TEX;
	mf->flag = TF_SELECT;
	mf->transp = TF_SOLID;
	mf->col = PyList_New(0);

	mf->smooth = 0;
	mf->mat_nr = 0;

	return (PyObject *)mf;
}

static PyObject *M_NMesh_Face(PyObject *self, PyObject *args)
{
	PyObject *vertlist = NULL;

	if (!PyArg_ParseTuple(args, "|O!", &PyList_Type, &vertlist))
				return EXPP_ReturnPyObjError (PyExc_TypeError,
						 "expected a list of vertices or nothing as argument");

/*	if (!vertlist) vertlist = PyList_New(0); */

	return new_NMFace(vertlist);
}

static PyObject *NMFace_append(PyObject *self, PyObject *args)
{
	PyObject *vert;
	BPy_NMFace *f = (BPy_NMFace *)self;

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
	{NULL, NULL, 0, NULL}
};

static PyObject *NMFace_getattr(PyObject *self, char *name)
{
	BPy_NMFace *mf = (BPy_NMFace *)self;

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
			return Image_CreatePyObject (mf->image);
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

	else if ((strcmp(name, "normal") == 0) || (strcmp(name, "no") == 0))	{

		if (EXPP_check_sequence_consistency(mf->v, &NMVert_Type)) {

			float fNormal[3] = {0.0,0.0,0.0};
			float *vco[4] = {NULL, NULL, NULL, NULL};
			int nSize = PyList_Size(mf->v);
			int loop;

			if (nSize != 3 && nSize != 4)
				return EXPP_ReturnPyObjError (PyExc_AttributeError,
										"face must contain either 3 or 4 verts");

			for (loop = 0; loop < nSize; loop++) {
				BPy_NMVert *v = (BPy_NMVert *)PyList_GetItem(mf->v, loop);
				vco[loop] = (float *)v->co;
			}

			if (nSize == 4)
				CalcNormFloat4(vco[0], vco[1], vco[2], vco[3], fNormal);
			else
				CalcNormFloat(vco[0], vco[1], vco[2], fNormal);

			return Py_BuildValue("[f,f,f]",fNormal[0],fNormal[1],fNormal[2]);
		}
		else // EXPP_check_sequence_consistency failed
			return EXPP_ReturnPyObjError (PyExc_AttributeError,
				"this face does not contain a series of NMVerts");
	}

	else if (strcmp(name, "__members__") == 0)
		return Py_BuildValue("[s,s,s,s,s,s,s,s,s,s,s]",
										"v", "col", "mat", "materialIndex", "smooth",
										"image", "mode", "flag", "transp", "uv", "normal");
	return Py_FindMethod(NMFace_methods, (PyObject*)self, name);
}

static int NMFace_setattr(PyObject *self, char *name, PyObject *v)
{
	BPy_NMFace *mf = (BPy_NMFace *)self;
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
		PyObject *pyimg;
		if (!PyArg_Parse(v, "O!", &Image_Type, &pyimg))
				return EXPP_ReturnIntError(PyExc_TypeError,
							"expected image object");

		if (pyimg == Py_None) {
			mf->image = NULL;

			return 0;
		}

		mf->image = ((BPy_Image *)pyimg)->image;

		return 0;
	}

	return EXPP_ReturnIntError (PyExc_AttributeError, name);
}

static PyObject *NMFace_repr (PyObject *self)
{
	return PyString_FromString("[NMFace]");
}

static int NMFace_len(BPy_NMFace *self) 
{
	return PySequence_Length(self->v);
}

static PyObject *NMFace_item(BPy_NMFace *self, int i)
{
	return PySequence_GetItem(self->v, i); // new ref
}

static PyObject *NMFace_slice(BPy_NMFace *self, int begin, int end)
{
	return PyList_GetSlice(self->v, begin, end); // new ref
}

static PySequenceMethods NMFace_SeqMethods =
{
	(inquiry)			NMFace_len,					 /* sq_length */
	(binaryfunc)		0,								 /* sq_concat */
	(intargfunc)		0,								 /* sq_repeat */
	(intargfunc)		NMFace_item,			 /* sq_item */
	(intintargfunc)		NMFace_slice,		 /* sq_slice */
	(intobjargproc)		0,							 /* sq_ass_item */
	(intintobjargproc)	0,						 /* sq_ass_slice */
};

PyTypeObject NMFace_Type =
{
	PyObject_HEAD_INIT(NULL)
	0,														/*ob_size*/
	"Blender NMFace",							/*tp_name*/
	sizeof(BPy_NMFace),						/*tp_basicsize*/
	0,														/*tp_itemsize*/
	/* methods */
	(destructor) NMFace_dealloc,	/*tp_dealloc*/
	(printfunc) 0,								/*tp_print*/
	(getattrfunc) NMFace_getattr, /*tp_getattr*/
	(setattrfunc) NMFace_setattr, /*tp_setattr*/
	0,														/*tp_compare*/
	(reprfunc) NMFace_repr,				/*tp_repr*/
	0,														/*tp_as_number*/
	&NMFace_SeqMethods,						/*tp_as_sequence*/
	0,														/*tp_as_mapping*/
	0,														/*tp_hash*/
};

static BPy_NMVert *newvert(float *co)
{
	BPy_NMVert *mv = PyObject_NEW(BPy_NMVert, &NMVert_Type);

	mv->co[0] = co[0]; mv->co[1] = co[1]; mv->co[2] = co[2];

	mv->no[0] = mv->no[1] = mv->no[2] = 0.0;
	mv->uvco[0] = mv->uvco[1] = mv->uvco[2] = 0.0;
	mv->flag = 0;
	
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
	PyObject_DEL(self);
}

static PyObject *NMVert_getattr(PyObject *self, char *name)
{
	BPy_NMVert *mv = (BPy_NMVert *)self;

	if (!strcmp(name, "co") || !strcmp(name, "loc"))
					return newVectorObject(mv->co, 3);

	else if (strcmp(name, "no") == 0) return newVectorObject(mv->no, 3);		 
	else if (strcmp(name, "uvco") == 0) return newVectorObject(mv->uvco, 3);		 
	else if (strcmp(name, "index") == 0) return PyInt_FromLong(mv->index);
	else if (strcmp(name, "sel") == 0) return PyInt_FromLong(mv->flag & 1);
	else if (strcmp(name, "__members__") == 0)
		return Py_BuildValue("[s,s,s,s,s]", "co", "no", "uvco", "index", "sel");

	return EXPP_ReturnPyObjError (PyExc_AttributeError, name);
}

static int NMVert_setattr(PyObject *self, char *name, PyObject *v)
{
	BPy_NMVert *mv = (BPy_NMVert *)self;
	int i;
	
	if (strcmp(name,"index") == 0) {
		PyArg_Parse(v, "i", &i);
		mv->index = i;
		return 0;
	}
	else if (strcmp(name, "sel") == 0) {
		PyArg_Parse(v, "i", &i);
		mv->flag = i?1:0;
		return 0;
	}
	else if (strcmp(name, "uvco") == 0) {

			if (!PyArg_ParseTuple(v, "ff|f",
							&(mv->uvco[0]), &(mv->uvco[1]), &(mv->uvco[2])))
				return EXPP_ReturnIntError (PyExc_AttributeError,
											"Vector tuple or triple expected");

		return 0;
	} 
	
	return EXPP_ReturnIntError (PyExc_AttributeError, name);
}

static int NMVert_len(BPy_NMVert *self)
{
	return 3;
}

static PyObject *NMVert_item(BPy_NMVert *self, int i)
{
	if (i < 0 || i >= 3)
		return EXPP_ReturnPyObjError (PyExc_IndexError,
							 "array index out of range");

	return Py_BuildValue("f", self->co[i]);
}

static PyObject *NMVert_slice(BPy_NMVert *self, int begin, int end)
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

static int NMVert_ass_item(BPy_NMVert *self, int i, PyObject *ob)
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

static int NMVert_ass_slice(BPy_NMVert *self, int begin, int end, PyObject *seq)
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
	(inquiry)			NMVert_len,							 /* sq_length */
	(binaryfunc)		0,										 /* sq_concat */
	(intargfunc)		0,										 /* sq_repeat */
	(intargfunc)		NMVert_item,					 /* sq_item */
	(intintargfunc)		NMVert_slice,				 /* sq_slice */
	(intobjargproc)		NMVert_ass_item,		 /* sq_ass_item */
	(intintobjargproc)	NMVert_ass_slice,  /* sq_ass_slice */
};

PyTypeObject NMVert_Type =
{
	PyObject_HEAD_INIT(NULL)
	0,														 /*ob_size*/
	"Blender NMVert",							 /*tp_name*/
	sizeof(BPy_NMVert),						 /*tp_basicsize*/
	0,														 /*tp_itemsize*/
	/* methods */
	(destructor) NMVert_dealloc,	 /*tp_dealloc*/
	(printfunc) 0,								 /*tp_print*/
	(getattrfunc) NMVert_getattr,  /*tp_getattr*/
	(setattrfunc) NMVert_setattr,  /*tp_setattr*/
	0,														 /*tp_compare*/
	(reprfunc) 0,									 /*tp_repr*/
	0,														 /*tp_as_number*/
	&NMVert_SeqMethods,						 /*tp_as_sequence*/
};

static void NMesh_dealloc(PyObject *self)
{
	BPy_NMesh *me = (BPy_NMesh *)self;

	Py_DECREF(me->name);
	Py_DECREF(me->verts);
	Py_DECREF(me->faces);
	Py_DECREF(me->materials);

	PyObject_DEL(self);
}

static PyObject *NMesh_addMaterial (PyObject *self, PyObject *args)
{
	BPy_NMesh *me = (BPy_NMesh *)self;
	BPy_Material *pymat;
	Material *mat;
	PyObject *iter;
	int i, len = 0;

	if (!PyArg_ParseTuple (args, "O!", &Material_Type, &pymat))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected Blender Material PyObject");

	mat = pymat->material;
	len = PyList_Size(me->materials);

	if (len >= 16)
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
						"object data material lists can't have more than 16 materials");

	for (i = 0; i < len; i++) {
		iter = PyList_GetItem(me->materials, i);
		if (mat == Material_FromPyObject(iter))
			return EXPP_ReturnPyObjError (PyExc_AttributeError,
						"material already in the list");
	}

	PyList_Append(me->materials, (PyObject *)pymat);

	return EXPP_incr_ret (Py_None);
}

static PyObject *NMesh_removeAllKeys (PyObject *self, PyObject *args)
{
	BPy_NMesh *nm = (BPy_NMesh *)self;
	Mesh *me = nm->mesh;

	if (!PyArg_ParseTuple (args, ""))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
							"this function expects no arguments");

	if (!me || !me->key) return EXPP_incr_ret (Py_False);

	me->key->id.us--;
	me->key = 0;

	return EXPP_incr_ret (Py_True);
}

static PyObject *NMesh_insertKey(PyObject *self, PyObject *args)
{
	int fra = -1, oldfra = -1;
	char *type = NULL;
	short typenum;
	BPy_NMesh *nm = (BPy_NMesh *)self;
	Mesh *mesh = nm->mesh;

	if (!PyArg_ParseTuple(args, "|is", &fra, &type))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
					 "expected nothing or an int and optionally a string as arguments");

	if (!type || !strcmp(type, "relative"))
		typenum = 1;
	else if (!strcmp(type, "absolute"))
		typenum = 2;
	else
		return EXPP_ReturnPyObjError (PyExc_AttributeError,
						 "if given, type should be 'relative' or 'absolute'");

	if (fra > 0) {
		fra = EXPP_ClampInt(fra, 1, NMESH_FRAME_MAX);
		oldfra = G.scene->r.cfra;
		G.scene->r.cfra = fra;
	}

	if (!mesh)
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
				"update this NMesh first with its .update() method");

	insert_meshkey(mesh, typenum);

	if (fra > 0) G.scene->r.cfra = oldfra;

	return EXPP_incr_ret (Py_None);
}

static PyObject *NMesh_getSelectedFaces(PyObject *self, PyObject *args)
{
	BPy_NMesh *nm = (BPy_NMesh *)self;
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
	if (((BPy_NMesh *)self)->sel_face < 0)
		return EXPP_incr_ret(Py_None);

	return Py_BuildValue("i", ((BPy_NMesh *)self)->sel_face);
}

static PyObject *NMesh_hasVertexUV(PyObject *self, PyObject *args)
{
	BPy_NMesh *me = (BPy_NMesh *)self;
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
	BPy_NMesh *me = (BPy_NMesh *)self;
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
	BPy_NMesh *me= (BPy_NMesh *)self;
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
	int recalc_normals = 0;
	BPy_NMesh *nmesh = (BPy_NMesh *)self;
	Mesh *mesh = nmesh->mesh;

	if (!PyArg_ParseTuple(args, "|i", &recalc_normals))
			return EXPP_ReturnPyObjError (PyExc_AttributeError,
				"expected nothing or an int (0 or 1) as argument");

	if (recalc_normals && recalc_normals != 1)
			return EXPP_ReturnPyObjError (PyExc_ValueError,
				"expected 0 or 1 as argument");

	if (mesh) {
		unlink_existingMeshData(mesh);
		convert_NMeshToMesh(mesh, nmesh);
	} else { 
		nmesh->mesh = Mesh_fromNMesh(nmesh);
		mesh = nmesh->mesh;
	}

	if (recalc_normals) vertexnormals_mesh(mesh, 0);

	mesh_update(mesh);

	nmesh_updateMaterials(nmesh);

	if (nmesh->name && nmesh->name != Py_None)
		new_id(&(G.main->mesh), &mesh->id, PyString_AsString(nmesh->name));

	if (!during_script())
		allqueue(REDRAWVIEW3D, 0);

	return PyInt_FromLong(1);
}

/** Implementation of the python method getVertexInfluence for an NMesh object.
 * This method returns a list of pairs (string,float) with bone names and
 * influences that this vertex receives.
 * @author Jordi Rovira i Bonet
 */
static PyObject *NMesh_getVertexInfluences(PyObject *self, PyObject *args)
{
	int index;
	PyObject* influence_list = NULL;

	/* Get a reference to the mesh object wrapped in here. */
	Mesh *me = ((BPy_NMesh*)self)->mesh;

	if (!me)
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
								"unlinked nmesh: call its .update() method first");

	/* Parse the parameters: only on integer (vertex index) */
	if (!PyArg_ParseTuple(args, "i", &index))
				return EXPP_ReturnPyObjError (PyExc_TypeError,
								 "expected int argument (index of the vertex)");

	/* Proceed only if we have vertex deformation information and index is valid*/
	if (me->dvert) {
		if ((index >= 0) && (index < me->totvert)) {

			int i;
			MDeformWeight *sweight = NULL;
	
			/* Number of bones influencing the vertex */
			int totinfluences=me->dvert[index].totweight;
	
	/* Build the list only with weights and names of the influent bones */
			/*influence_list = PyList_New(totinfluences);*/
			influence_list = PyList_New(0);

	/* Get the reference of the first weight structure */
			sweight = me->dvert[index].dw;			

			for (i=0; i<totinfluences; i++) {

		/*Add the weight and the name of the bone, which is used to identify it*/

				if (sweight->data) /* valid bone: return its name */
/*					PyList_SetItem(influence_list, i,
						Py_BuildValue("[sf]", sweight->data->name, sweight->weight));
				else // NULL bone: return Py_None instead
					PyList_SetItem(influence_list, i,
						Py_BuildValue("[Of]", Py_None, sweight->weight));*/
					PyList_Append(influence_list,
						Py_BuildValue("[sf]", sweight->data->name, sweight->weight));

		/* Next weight */
				sweight++;
			}
		}
		else //influence_list = PyList_New(0);
			return EXPP_ReturnPyObjError (PyExc_IndexError,
							"vertex index out of range");
	}
	else influence_list = PyList_New(0);

	return influence_list;
}

Mesh *Mesh_fromNMesh(BPy_NMesh *nmesh)
{
	Mesh *mesh = NULL;
	mesh = add_mesh();

	if (!mesh)
		EXPP_ReturnPyObjError(PyExc_RuntimeError,
						 "FATAL: could not create mesh object");

	mesh->id.us = 0; /* no user yet */
	G.totmesh++;
	convert_NMeshToMesh(mesh, nmesh);

	return mesh;
}

PyObject *NMesh_link(PyObject *self, PyObject *args) 
{/*
	BPy_Object *bl_obj;

	if (!PyArg_ParseTuple(args, "O!", &Object_Type, &bl_obj))
			return EXPP_ReturnPyErrorObj (PyExc_TypeError,
						"NMesh can only be linked to Objects");

	bl_obj->data = (PyObject *)self;*/

/* Better use object.link(nmesh), no need for this nmesh.link(object) */

	return EXPP_incr_ret(Py_None);
}

static PyObject *NMesh_getMaxSmoothAngle (BPy_NMesh *self)
{
	PyObject *attr = PyInt_FromLong (self->smoothresh);

	if (attr) return attr;

	return EXPP_ReturnPyObjError (PyExc_RuntimeError,
						 "couldn't get NMesh.maxSmoothAngle attribute");
}

static PyObject *NMesh_setMaxSmoothAngle (PyObject *self, PyObject *args)
{
	short value = 0;
	BPy_NMesh *nmesh = (BPy_NMesh *)self;

	if (!PyArg_ParseTuple(args, "h", &value))
		return EXPP_ReturnPyObjError (PyExc_AttributeError,
							 "expected an int in [1, 80] as argument");

	nmesh->smoothresh =
		(short)EXPP_ClampInt (value, NMESH_SMOOTHRESH_MIN, NMESH_SMOOTHRESH_MAX);

	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject *NMesh_getSubDivLevels (BPy_NMesh *self)
{
	PyObject *attr = Py_BuildValue ("[h,h]", self->subdiv[0], self->subdiv[1]);

	if (attr) return attr;

	return EXPP_ReturnPyObjError (PyExc_RuntimeError,
						 "couldn't get NMesh.subDivLevels attribute");
}

static PyObject *NMesh_setSubDivLevels (PyObject *self, PyObject *args)
{
	short display = 0, render = 0;
	BPy_NMesh *nmesh = (BPy_NMesh *)self;

	if (!PyArg_ParseTuple(args, "(hh)", &display, &render))
		return EXPP_ReturnPyObjError (PyExc_AttributeError,
							 "expected a sequence [int, int] as argument");

	nmesh->subdiv[0] =
		(short)EXPP_ClampInt (display, NMESH_SUBDIV_MIN, NMESH_SUBDIV_MAX);

	nmesh->subdiv[1] =
		(short)EXPP_ClampInt (render, NMESH_SUBDIV_MIN, NMESH_SUBDIV_MAX);

	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject *NMesh_getMode (BPy_NMesh *self)
{
	PyObject *attr = PyInt_FromLong (self->mode);

	if (attr) return attr;

	return EXPP_ReturnPyObjError (PyExc_RuntimeError,
						 "couldn't get NMesh.mode attribute");
}

static PyObject *NMesh_setMode (PyObject *self, PyObject *args)
{
	BPy_NMesh *nmesh = (BPy_NMesh *)self;
	char *m[5] = {NULL, NULL, NULL, NULL, NULL};
	short i, mode = 0;

	if (!PyArg_ParseTuple(args, "|sssss", &m[0], &m[1], &m[2], &m[3], &m[4]))
		return EXPP_ReturnPyObjError (PyExc_AttributeError,
							 "expected from none to 5 strings as argument(s)");

	for (i = 0; i < 5; i++) {
		if (!m[i]) break;
		if (strcmp(m[i], "NoVNormalsFlip") == 0)
			mode |= EXPP_NMESH_MODE_NOPUNOFLIP;
		else if (strcmp(m[i], "TwoSided") == 0)
			mode |= EXPP_NMESH_MODE_TWOSIDED;
		else if (strcmp(m[i], "AutoSmooth") == 0)
			mode |= EXPP_NMESH_MODE_AUTOSMOOTH;
		else if (strcmp(m[i], "SubSurf") == 0)
			mode |= EXPP_NMESH_MODE_SUBSURF;
		else if (strcmp(m[i], "Optimal") == 0)
			mode |= EXPP_NMESH_MODE_OPTIMAL;
		else
			return EXPP_ReturnPyObjError (PyExc_AttributeError,
							 "unknown NMesh mode");
	}

	nmesh->mode = mode;

	Py_INCREF (Py_None);
	return Py_None;
}

#undef MethodDef
#define MethodDef(func) {#func, NMesh_##func, METH_VARARGS, NMesh_##func##_doc}

static struct PyMethodDef NMesh_methods[] =
{
	MethodDef(addVertGroup),
	MethodDef(removeVertGroup),
	MethodDef(assignVertsToGroup),
	MethodDef(removeVertsFromGroup),
	MethodDef(getVertsFromGroup),
	MethodDef(renameVertGroup),
	MethodDef(getVertGroupNames),
	MethodDef(hasVertexColours),
	MethodDef(hasFaceUV),
	MethodDef(hasVertexUV),
	MethodDef(getActiveFace),
	MethodDef(getSelectedFaces),
	MethodDef(getVertexInfluences),
	MethodDef(addMaterial),
	MethodDef(insertKey),
	MethodDef(removeAllKeys),
	MethodDef(update),
	MethodDef(setMode),
	MethodDef(setMaxSmoothAngle),
	MethodDef(setSubDivLevels),
	{"getMode", (PyCFunction)NMesh_getMode, METH_NOARGS, NMesh_getMode_doc},
	{"getMaxSmoothAngle", (PyCFunction)NMesh_getMaxSmoothAngle, METH_NOARGS,
		NMesh_getMaxSmoothAngle_doc},
	{"getSubDivLevels", (PyCFunction)NMesh_getSubDivLevels, METH_NOARGS,
		NMesh_getSubDivLevels_doc},
	{NULL, NULL, 0, NULL}
};

static PyObject *NMesh_getattr(PyObject *self, char *name)
{
	BPy_NMesh *me = (BPy_NMesh *)self;

	if (strcmp(name, "name") == 0) 
		return EXPP_incr_ret(me->name);

	else if (strcmp(name, "mode") == 0)
		return PyInt_FromLong(me->mode);

	else if (strcmp(name, "block_type") == 0) /* for compatibility */
		return PyString_FromString("NMesh");

	else if (strcmp(name, "materials") == 0)
		return EXPP_incr_ret(me->materials);

	else if (strcmp(name, "verts") == 0)
		return EXPP_incr_ret(me->verts);

	else if (strcmp(name, "maxSmoothAngle") == 0)
		return PyInt_FromLong(me->smoothresh);

	else if (strcmp(name, "subDivLevels") == 0)
		return Py_BuildValue("[h,h]", me->subdiv[0], me->subdiv[1]);

	else if (strcmp(name, "users") == 0) {
		if (me->mesh) {
			return PyInt_FromLong(me->mesh->id.us); 
		}
		else { /* it's a free mesh: */
			return Py_BuildValue("i", 0); 
		}
	}

	else if (strcmp(name, "faces") == 0)
		return EXPP_incr_ret(me->faces);

	else if (strcmp(name, "__members__") == 0)
		return Py_BuildValue("[s,s,s,s,s,s,s]",
			"name", "materials", "verts", "users", "faces", "maxSmoothAngle",
			"subdivLevels");

	return Py_FindMethod(NMesh_methods, (PyObject*)self, name);
}

static int NMesh_setattr(PyObject *self, char *name, PyObject *v)
{
	BPy_NMesh *me = (BPy_NMesh *)self;

	if (!strcmp(name, "name")) {

		if (!PyString_Check(v))
			return EXPP_ReturnIntError (PyExc_TypeError,
							"expected string argument");

		Py_DECREF (me->name);
		me->name = EXPP_incr_ret(v);
	}

	else if (!strcmp(name, "mode")) {
		short mode;

		if (!PyInt_Check(v))
			return EXPP_ReturnIntError (PyExc_TypeError,
							"expected int argument");

		mode = (short)PyInt_AsLong(v);
		if (mode >= 0) me->mode = mode;
		else
			return EXPP_ReturnIntError (PyExc_ValueError,
							"expected positive int argument");
	}

	else if (!strcmp(name, "verts") || !strcmp(name, "faces") ||
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
			return EXPP_ReturnIntError (PyExc_TypeError, "expected a sequence");
	}

	else if (!strcmp(name, "maxSmoothAngle")) {
		short smoothresh = 0;

		if (!PyInt_Check(v))
			return EXPP_ReturnIntError (PyExc_TypeError,
							"expected int argument");

		smoothresh = (short)PyInt_AsLong(v);

		me->smoothresh =
			EXPP_ClampInt(smoothresh, NMESH_SMOOTHRESH_MIN, NMESH_SMOOTHRESH_MAX);
	}

	else if (!strcmp(name, "subDivLevels")) {
		int subdiv[2] = {0,0};
		int i;
		PyObject *tmp;

		if (!PySequence_Check(v) || (PySequence_Length(v) != 2))
			return EXPP_ReturnIntError (PyExc_TypeError,
							"expected a list [int, int] as argument");

		for (i = 0; i < 2; i++) {
			tmp = PySequence_GetItem(v, i);
			if (tmp) {
				if (!PyInt_Check(tmp)) {
					Py_DECREF (tmp);
					return EXPP_ReturnIntError (PyExc_TypeError,
						"expected a list [int, int] as argument");
				}

				subdiv[i] = PyInt_AsLong (tmp);
				me->subdiv[i] =
					(short)EXPP_ClampInt(subdiv[i], NMESH_SUBDIV_MIN, NMESH_SUBDIV_MAX);
				Py_DECREF (tmp);
			}
			else return EXPP_ReturnIntError (PyExc_RuntimeError,
				"couldn't retrieve subdiv values from list");
		}
	}

	else
			return EXPP_ReturnIntError (PyExc_AttributeError, name);

	return 0;
}

PyTypeObject NMesh_Type =
{
	PyObject_HEAD_INIT(NULL)
	0,														 /*ob_size*/
	"Blender NMesh",							 /*tp_name*/
	sizeof(BPy_NMesh),						 /*tp_basicsize*/
	0,														 /*tp_itemsize*/
	/* methods */
	(destructor)	NMesh_dealloc,	 /*tp_dealloc*/
	(printfunc)		0,							 /*tp_print*/
	(getattrfunc) NMesh_getattr,	 /*tp_getattr*/
	(setattrfunc) NMesh_setattr,	 /*tp_setattr*/
};

static BPy_NMFace *nmface_from_data(BPy_NMesh *mesh, int vidxs[4],
								char mat_nr, char flag, TFace *tface, MCol *col) 
{
	BPy_NMFace *newf = PyObject_NEW (BPy_NMFace, &NMFace_Type);
	int i, len;

	if (vidxs[3]) len = 4;
	else if (vidxs[2]) len = 3;
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
			newf->image = (Image *)tface->tpage;
		else
			newf->image = NULL;

		newf->mode = tface->mode;			/* draw mode */
		newf->flag = tface->flag;			/* select flag */
		newf->transp = tface->transp; /* transparency flag */
		col = (MCol *) (tface->col);	/* XXX weird, tface->col is uint[4] */
	}
	else {
		newf->mode = TF_DYNAMIC; /* just to initialize it to something meaninful,*/
		/* since without tfaces there are no tface->mode's, obviously. */
		newf->image = NULL;
		newf->uv = PyList_New(0); 
	} 

	newf->mat_nr = mat_nr;
	newf->smooth = flag & ME_SMOOTH;

	if (col) {
		newf->col = PyList_New(4);
		for(i = 0; i < 4; i++, col++) {
			PyList_SetItem(newf->col, i, 
				(PyObject *)newcol(col->b, col->g, col->r, col->a));
		}
	}
	else newf->col = PyList_New(0);

	return newf;
}

static BPy_NMVert *nmvert_from_data(BPy_NMesh *me,
								MVert *vert, MSticky *st, float *co, int idx, char flag)
{
	BPy_NMVert *mv = PyObject_NEW(BPy_NMVert, &NMVert_Type);

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
	mv->flag = flag & 1;

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
	BPy_NMesh *me = PyObject_NEW (BPy_NMesh, &NMesh_Type);
	me->flags = 0;
	me->mode = EXPP_NMESH_MODE_TWOSIDED; /* default for new meshes */
	me->subdiv[0] = NMESH_SUBDIV;
	me->subdiv[1] = NMESH_SUBDIV;
	me->smoothresh = NMESH_SMOOTHRESH;

	me->object = NULL; /* not linked to any object yet */

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
			mfaces = dlm->mface;
			tfaces = dlm->tface;
			mcols = dlm->mcol;

			totvert = dlm->totvert;
			totface = dlm->totface;
		}
		else {
			me->name = PyString_FromString(oldmesh->id.name+2);
			me->mesh = oldmesh;
			me->mode = oldmesh->flag; /* yes, we save the mesh flags in nmesh->mode*/
			me->subdiv[0] = oldmesh->subdiv;
			me->subdiv[1] = oldmesh->subdivr;
			me->smoothresh = oldmesh->smoothresh;

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
											(PyObject *)nmvert_from_data(me, oldmv, oldst, vco, i, oldmv->flag));	
		}

		me->faces = PyList_New(totface);
		for (i = 0; i < totface; i++) {
			TFace *oldtf = tfaces?&tfaces[i]:NULL;
			MCol *oldmc = mcols?&mcols[i*4]:NULL;
			MFace *oldmf = &mfaces[i];
			int vidxs[4];
			vidxs[0] = oldmf->v1;
			vidxs[1] = oldmf->v2;
			vidxs[2] = oldmf->v3;
			vidxs[3] = oldmf->v4;

			PyList_SetItem (me->faces, i,
									(PyObject *)nmface_from_data(me, vidxs, oldmf->mat_nr, oldmf->flag, oldtf, oldmc));
		}
		me->materials = EXPP_PyList_fromMaterialList(oldmesh->mat, oldmesh->totcol);
	}

	return (PyObject *)me;	
}

PyObject *new_NMesh(Mesh *oldmesh) 
{
	return new_NMesh_internal (oldmesh, NULL, NULL);
}

static PyObject *M_NMesh_New(PyObject *self, PyObject *args) 
{
	char *name = NULL;
	PyObject *ret = NULL;

	if (!PyArg_ParseTuple(args, "|s", &name))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
							"expected nothing or a string as argument");

	ret = new_NMesh(NULL);

	if (ret && name) {
		BPy_NMesh *nmesh = (BPy_NMesh *)ret;
		Py_DECREF (nmesh->name);
		nmesh->name = PyString_FromString(name);
	}

	return ret;
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

/* Note: NMesh.GetRawFromObject gets the display list mesh from Blender:
 * the vertices are already transformed / deformed. */
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

/* @hack: to mark that (deformed) mesh is readonly, so the update function
 * will not try to write it. */

	((BPy_NMesh *) nmesh)->mesh = 0;

	return nmesh;
}

static void mvert_from_data(MVert *mv, MSticky *st, BPy_NMVert *from) 
{
	mv->co[0] = from->co[0]; mv->co[1] = from->co[1]; mv->co[2] = from->co[2];

	mv->no[0] = from->no[0]*32767.0;
	mv->no[1] = from->no[1]*32767.0;
	mv->no[2] = from->no[2]*32767.0;
		
	mv->flag = (from->flag & 1);
	mv->mat_nr = 0;

	if (st) {
		st->co[0] = from->uvco[0];
		st->co[1] = from->uvco[1];
	}
}

/*@ TODO: this function is just a added hack. Don't look at the
 * RGBA/BRGA confusion, it just works, but will never work with
 * a restructured Blender */

static void assign_perFaceColors(TFace *tf, BPy_NMFace *from)
{
	MCol *col;
	int i;

	col = (MCol *)(tf->col);

	if (col) {
		int len = PySequence_Length(from->col);
		
		if(len > 4) len = 4;
		
		for (i = 0; i < len; i++, col++) {
			BPy_NMCol *mc = (BPy_NMCol *)PySequence_GetItem(from->col, i);
			if(!BPy_NMCol_Check(mc)) {
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

static int assignFaceUV(TFace *tf, BPy_NMFace *nmface)
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
		tf->tpage = (void *)nmface->image;
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

static void mface_from_data(MFace *mf, TFace *tf, MCol *col, BPy_NMFace *from)
{
	BPy_NMVert *nmv;

	int i = PyList_Size(from->v);
	if (i >= 1) {
		nmv = (BPy_NMVert *)PyList_GetItem(from->v, 0);
		if (BPy_NMVert_Check(nmv) && nmv->index != -1) mf->v1 = nmv->index;
		else mf->v1 = 0;
	}
	if (i >= 2) {
		nmv = (BPy_NMVert *)PyList_GetItem(from->v, 1);
		if (BPy_NMVert_Check(nmv) && nmv->index != -1) mf->v2 = nmv->index;
		else mf->v2 = 0;
	}
	if (i >= 3) {
		nmv = (BPy_NMVert *)PyList_GetItem(from->v, 2);
		if (BPy_NMVert_Check(nmv) && nmv->index != -1) mf->v3 = nmv->index;
		else mf->v3= 0;
	}
	if(i >= 4) {
		nmv = (BPy_NMVert *)PyList_GetItem(from->v, 3);
		if (BPy_NMVert_Check(nmv) && nmv->index != -1) mf->v4 = nmv->index;
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
			BPy_NMCol *mc = (BPy_NMCol *) PySequence_GetItem(from->col, i);
			if(!BPy_NMCol_Check(mc)) {
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
static int check_validFaceUV(BPy_NMesh *nmesh)
{
	PyObject *faces;
	BPy_NMFace *nmface;
	int i, n;

	faces = nmesh->faces;
	for (i = 0; i < PySequence_Length(faces); i++) {
		nmface = (BPy_NMFace *)PyList_GetItem(faces, i);
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

/* this is a copy of unlink_mesh in mesh.c, because ... */
void EXPP_unlink_mesh(Mesh *me)
{
	int a;

	if(me==0) return;

	for(a=0; a<me->totcol; a++) {
		if(me->mat[a]) me->mat[a]->id.us--;
		me->mat[a]= 0;
	}
/*	... here we want to preserve mesh keys */
/* if users want to get rid of them, they can use mesh.removeAllKeys() */
/*
	if(me->key) me->key->id.us--;
	me->key= 0;
*/
	if(me->texcomesh) me->texcomesh= 0;
}

static int unlink_existingMeshData(Mesh *mesh)
{
	freedisplist(&mesh->disp);
	EXPP_unlink_mesh(mesh);
	if(mesh->mvert) MEM_freeN(mesh->mvert);
	if(mesh->mface) MEM_freeN(mesh->mface);
	if(mesh->mcol) MEM_freeN(mesh->mcol);
	if(mesh->msticky) MEM_freeN(mesh->msticky);
	if(mesh->mat) MEM_freeN(mesh->mat);
	if(mesh->tface) MEM_freeN(mesh->tface);
	return 1;
}

Material **nmesh_updateMaterials(BPy_NMesh *nmesh)
{
	Material **matlist;
	Mesh *mesh = nmesh->mesh;
	int len = PyList_Size(nmesh->materials);

	if (!mesh) {
		printf("FATAL INTERNAL ERROR: illegal call to updateMaterials()\n");
		return 0;
	}

	if (len > 0) {
		matlist = EXPP_newMaterialList_fromPyList(nmesh->materials);
		EXPP_incr_mats_us(matlist, len);

		if (mesh->mat) MEM_freeN(mesh->mat);

		mesh->mat = matlist;

	} else {
		matlist = 0;
	}
	mesh->totcol = len;

/**@ This is another ugly fix due to the weird material handling of blender.
	* it makes sure that object material lists get updated (by their length)
	* according to their data material lists, otherwise blender crashes.
	* It just stupidly runs through all objects...BAD BAD BAD.
	*/
	test_object_materials((ID *)mesh);

	return matlist;
}

PyObject *NMesh_assignMaterials_toObject(BPy_NMesh *nmesh, Object *ob)
{
	BPy_Material *pymat;
	Material *ma;
	int i;
	short old_matmask;
	Mesh *mesh = nmesh->mesh;
	int nmats; /* number of mats == len(nmesh->materials)*/

	old_matmask = ob->colbits; /*@ HACK: save previous colbits */
	ob->colbits = 0;	/* make assign_material work on mesh linked material */

	nmats = PyList_Size(nmesh->materials);

	if (nmats > 0 && !mesh->mat) {
		ob->totcol = nmats;
		mesh->totcol = nmats;
		mesh->mat = MEM_callocN(sizeof(void *)*nmats, "bpy_memats");

		if (ob->mat) MEM_freeN(ob->mat);
		ob->mat = MEM_callocN(sizeof(void *)*nmats, "bpy_obmats");
	}

	for (i = 0; i < nmats; i++) {
		pymat = (BPy_Material *)PySequence_GetItem(nmesh->materials, i);

		if (Material_CheckPyObject ((PyObject *)pymat)) {
			ma = pymat->material;
			assign_material(ob, ma, i+1);/*@ XXX don't use this function anymore*/
		}
		else {
			Py_DECREF (pymat);
			return EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected Material type in attribute list 'materials'!");
		} 

		Py_DECREF (pymat);
	}

	ob->colbits = old_matmask; /*@ HACK */

	ob->actcol = 1;
	return EXPP_incr_ret (Py_None);
}

static int convert_NMeshToMesh (Mesh *mesh, BPy_NMesh *nmesh)
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

	/* Minor note: we used 'mode' because 'flag' was already used internally
	 * by nmesh */
	mesh->flag = nmesh->mode;
	mesh->smoothresh = nmesh->smoothresh;
	mesh->subdiv = nmesh->subdiv[0];
	mesh->subdivr = nmesh->subdiv[1];

	/*@ material assignment moved to PutRaw */
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
		BPy_NMFace *mf = (BPy_NMFace *)PySequence_GetItem(nmesh->faces, i);
			
		j = PySequence_Length(mf->v);
		while (j--) {
			BPy_NMVert *mv = (BPy_NMVert *)PySequence_GetItem(mf->v, j);
			if (BPy_NMVert_Check(mv)) mv->index = -1;
			Py_DECREF(mv);
		}

		Py_DECREF(mf);
	}

	for (i = 0; i < mesh->totvert; i++) {
		BPy_NMVert *mv = (BPy_NMVert *)PySequence_GetItem(nmesh->verts, i);
		mv->index = i;
		Py_DECREF(mv);
	}

	newmv = mesh->mvert;
	newst = mesh->msticky;
	for (i = 0; i < mesh->totvert; i++) {
		PyObject *mv = PySequence_GetItem (nmesh->verts, i);
		mvert_from_data(newmv, newst, (BPy_NMVert *)mv);
		Py_DECREF(mv);

		newmv++;
		if (newst) newst++;
	}

/*	assign per face texture UVs */

	/* check face UV flag, then check whether there was one 
	 * UV coordinate assigned, if yes, make tfaces */
	if ((nmesh->flags & NMESH_HASFACEUV) || (check_validFaceUV(nmesh))) {
		make_tfaces(mesh); /* initialize TFaces */

		newmc = mesh->mcol;
		newmf = mesh->mface;
		newtf = mesh->tface;
		for (i = 0; i<mesh->totface; i++) {
			PyObject *mf = PySequence_GetItem(nmesh->faces, i);
			mface_from_data(newmf, newtf, newmc, (BPy_NMFace *) mf);
			Py_DECREF(mf);

			newtf++;
			newmf++;
			if (newmc) newmc += 4;
		}

		nmesh->flags |= NMESH_HASFACEUV;
	}
	else {
		newmc = mesh->mcol;
		newmf = mesh->mface;

		for (i = 0; i < mesh->totface; i++) {
			PyObject *mf = PySequence_GetItem(nmesh->faces, i);
			mface_from_data(newmf, 0, newmc, (BPy_NMFace *) mf);
			Py_DECREF(mf);

			newmf++;
			if (newmc) newmc += 4; /* there are 4 MCol's per face */
		}
	}

	return 1;
}

static PyObject *M_NMesh_PutRaw(PyObject *self, PyObject *args) 
{
	char *name = NULL;
	Mesh *mesh = NULL;
	Object *ob = NULL;
	BPy_NMesh *nmesh;
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

	if (!mesh || mesh->id.us == 0) {
		ob = add_object(OB_MESH);
		if (!ob) {
			PyErr_SetString(PyExc_RuntimeError,
						 "Fatal: could not create mesh object");
		return 0;
		}

		if (!mesh) mesh = (Mesh *)ob->data;
		else set_mesh(ob, mesh); // also does id.us++
	}

	if (name)
		new_id(&(G.main->mesh), &mesh->id, name);
	else if (nmesh->name && nmesh->name != Py_None)
		new_id(&(G.main->mesh), &mesh->id, PyString_AsString(nmesh->name));

	unlink_existingMeshData(mesh);
	convert_NMeshToMesh(mesh, nmesh);
	nmesh->mesh = mesh;

	if (recalc_normals) vertexnormals_mesh(mesh, 0);

	mesh_update(mesh);
	
	if (!during_script())
		allqueue(REDRAWVIEW3D, 0);

	// @OK...this requires some explanation:
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
	// to the Object instead (MaterialButtons: [OB] "link materials to object")
	//
	// This feature implies that pointers to materials can be stored in
	// an object or a mesh. The number of total materials MUST be
	// synchronized (ob->totcol <-> mesh->totcol). We avoid the dangerous
	// direct access by calling blenderkernel/material.c:assign_material().

	// The flags setting the material binding is found in ob->colbits, where 
	// each bit indicates the binding PER MATERIAL 

	if (ob) { // we created a new object
		nmesh->object = ob; // linking so vgrouping methods know which obj to work on
		NMesh_assignMaterials_toObject(nmesh, ob);
		EXPP_synchronizeMaterialLists (ob, ob->data);
		return Object_CreatePyObject(ob);
	}
	else {
		mesh->mat = EXPP_newMaterialList_fromPyList(nmesh->materials);
		EXPP_incr_mats_us (mesh->mat, PyList_Size (nmesh->materials));
		return EXPP_incr_ret (Py_None);
	}

}

#undef MethodDef
#define MethodDef(func) \
	{#func, M_NMesh_##func, METH_VARARGS, M_NMesh_##func##_doc}

static struct PyMethodDef M_NMesh_methods[] = {
	MethodDef(Col),
	MethodDef(Vert),
	MethodDef(Face),
	MethodDef(New),
	MethodDef(GetRaw),
	MethodDef(GetRawFromObject),
	MethodDef(PutRaw),
	{NULL, NULL, 0, NULL}
};

static PyObject *M_NMesh_Modes (void)
{
	PyObject *Modes = M_constant_New();

	if (Modes) {
		BPy_constant *d = (BPy_constant *)Modes;

		constant_insert(d, "NOVNORMALSFLIP",
										PyInt_FromLong(EXPP_NMESH_MODE_NOPUNOFLIP));
		constant_insert(d, "TWOSIDED", PyInt_FromLong(EXPP_NMESH_MODE_TWOSIDED));
		constant_insert(d, "AUTOSMOOTH",PyInt_FromLong(EXPP_NMESH_MODE_AUTOSMOOTH));
		constant_insert(d, "SUBSURF", PyInt_FromLong(EXPP_NMESH_MODE_SUBSURF));
		constant_insert(d, "OPTIMAL", PyInt_FromLong(EXPP_NMESH_MODE_OPTIMAL));
	}

	return Modes;
}

#undef EXPP_ADDCONST
#define EXPP_ADDCONST(dict, name) \
			 constant_insert(dict, #name, PyInt_FromLong(TF_##name))
/* Set constants for face drawing mode -- see drawmesh.c */

static PyObject *M_NMesh_FaceModesDict (void)
{
	PyObject *FM = M_constant_New();

	if (FM) {
		BPy_constant *d = (BPy_constant *)FM;

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
	}

	return FM;
}

static PyObject *M_NMesh_FaceFlagsDict (void)
{
	PyObject *FF = M_constant_New();

	if (FF) {
		BPy_constant *d = (BPy_constant *)FF;

		EXPP_ADDCONST(d, SELECT);
		EXPP_ADDCONST(d, HIDE);
		EXPP_ADDCONST(d, ACTIVE);
	}

	return FF;
}

static PyObject *M_NMesh_FaceTranspModesDict (void)
{
	PyObject *FTM = M_constant_New();

	if (FTM) {
		BPy_constant *d = (BPy_constant *)FTM;

		EXPP_ADDCONST(d, SOLID);
		EXPP_ADDCONST(d, ADD);
		EXPP_ADDCONST(d, ALPHA);
		EXPP_ADDCONST(d, SUB);
	}

	return FTM;
}

PyObject *NMesh_Init (void) 
{
	PyObject *submodule;

	PyObject *Modes = M_NMesh_Modes ();
	PyObject *FaceFlags = M_NMesh_FaceFlagsDict ();
	PyObject *FaceModes = M_NMesh_FaceModesDict ();
	PyObject *FaceTranspModes = M_NMesh_FaceTranspModesDict ();

	NMCol_Type.ob_type = &PyType_Type;
	NMFace_Type.ob_type = &PyType_Type;
	NMVert_Type.ob_type = &PyType_Type;
	NMesh_Type.ob_type = &PyType_Type;

	submodule = Py_InitModule3("Blender.NMesh", M_NMesh_methods, M_NMesh_doc);

	if (Modes) PyModule_AddObject (submodule, "Modes", Modes);
	if (FaceFlags) PyModule_AddObject (submodule, "FaceFlags", FaceFlags);
	if (FaceModes) PyModule_AddObject (submodule, "FaceModes", FaceModes);
	if (FaceTranspModes)
					PyModule_AddObject (submodule, "FaceTranspModes", FaceTranspModes);

	g_nmeshmodule = submodule;
	return submodule;
}

/* These are needed by Object.c */

PyObject *NMesh_CreatePyObject (Mesh *me, Object *ob)
{
	BPy_NMesh *nmesh = (BPy_NMesh *)new_NMesh (me);

	if (nmesh) nmesh->object = ob; /* linking nmesh and object for vgrouping methods */

	return (PyObject *)nmesh;
}

int NMesh_CheckPyObject (PyObject *pyobj)
{
	return (pyobj->ob_type == &NMesh_Type);
}

Mesh *Mesh_FromPyObject (PyObject *pyobj, Object *ob)
{
	if (pyobj->ob_type == &NMesh_Type) {
		Mesh *mesh;
		BPy_NMesh *nmesh = (BPy_NMesh *)pyobj;

		if (nmesh->mesh) {
			mesh = nmesh->mesh;
			unlink_existingMeshData(mesh);
			convert_NMeshToMesh(mesh, nmesh);
		}
		else { 
			nmesh->mesh = Mesh_fromNMesh(nmesh);
			mesh = nmesh->mesh;
		}

		nmesh->object = ob; /* linking for vgrouping methods */

		if (nmesh->name && nmesh->name != Py_None)
			new_id(&(G.main->mesh), &mesh->id, PyString_AsString(nmesh->name));

		mesh_update(mesh);

		nmesh_updateMaterials(nmesh);

		return mesh;
	}

	return NULL;
}

static PyObject *NMesh_addVertGroup (PyObject *self, PyObject *args)
{
	char* groupStr;
	struct Object* object;
	PyObject *tempStr;

	if (!PyArg_ParseTuple(args, "s", &groupStr))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected string argument");

	if (( (BPy_NMesh*)self )->object == NULL)
		return EXPP_ReturnPyObjError (PyExc_AttributeError, 
			"mesh must be linked to an object first...");

	object = ((BPy_NMesh*)self)->object;

	//get clamped name
	tempStr = PyString_FromStringAndSize(groupStr, 32);
	groupStr = PyString_AsString(tempStr);

	add_defgroup_name (object, groupStr);

	allqueue (REDRAWBUTSALL, 1);

	return EXPP_incr_ret (Py_None);
}

static PyObject *NMesh_removeVertGroup (PyObject *self, PyObject *args)
{
	char* groupStr;
	struct Object* object;
	int nIndex;
	bDeformGroup* pGroup;

	if (!PyArg_ParseTuple(args, "s", &groupStr))
				return EXPP_ReturnPyObjError (PyExc_TypeError,
								 "expected string argument");

	if (( (BPy_NMesh*)self )->object == NULL)
		return EXPP_ReturnPyObjError (PyExc_AttributeError, 
			"mesh must be linked to an object first...");

	object = ((BPy_NMesh*)self)->object;

	pGroup = get_named_vertexgroup(object, groupStr);
	if(pGroup == NULL)
		return EXPP_ReturnPyObjError (PyExc_AttributeError,
				 "group does not exist!");

	nIndex = get_defgroup_num(object, pGroup);
	if(nIndex == -1)
		return EXPP_ReturnPyObjError (PyExc_AttributeError,
				 "no deform groups assigned to mesh");
	nIndex++;
	object->actdef = nIndex;

	del_defgroup(object);

	allqueue (REDRAWBUTSALL, 1);

	return EXPP_incr_ret (Py_None);
}

static PyObject *NMesh_assignVertsToGroup (PyObject *self, PyObject *args)
{
	//listObject is an integer list of vertex indices to add to group
	//groupStr = group name
	//weight is a float defining the weight this group has on this vertex
	//assignmode = "replace", "add", "subtract"
	//							replace weight - add addition weight to vertex for this group
	//				- remove group influence from this vertex
	//the function will not like it if your in editmode...

	char* groupStr;
	char* assignmodeStr = NULL;
	int nIndex;	
	int assignmode;
	float weight = 1.0;
	struct Object* object;
	bDeformGroup* pGroup;
	PyObject* listObject; 
	int tempInt;
	int x;

	if (( (BPy_NMesh*)self )->object == NULL)
		return EXPP_ReturnPyObjError (PyExc_AttributeError, 
			"mesh must be linked to an object first...");

	if (!PyArg_ParseTuple(args, "sO!fs", &groupStr, &PyList_Type, &listObject, 
		 &weight, &assignmodeStr)) {
				return EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected string, list,	float, string arguments");
	}

	object = ((BPy_NMesh*)self)->object;
	
	if (object->data == NULL)
		return EXPP_ReturnPyObjError (PyExc_AttributeError,
			"object contains no data...");

	pGroup = get_named_vertexgroup(object, groupStr);
	if(pGroup == NULL)
		return EXPP_ReturnPyObjError(PyExc_AttributeError, "group does not exist!");

	nIndex = get_defgroup_num(object, pGroup);
		if(nIndex == -1)
			return EXPP_ReturnPyObjError (PyExc_AttributeError,
					 "no deform groups assigned to mesh");

	if(assignmodeStr == NULL)
		assignmode = 1; /* default */
	else if(STREQ(assignmodeStr, "replace"))
		assignmode = 1;
	else if(STREQ(assignmodeStr, "add"))
		assignmode = 2;
	else if(STREQ(assignmodeStr, "subtract"))
		assignmode = 3;
	else
		return EXPP_ReturnPyObjError (PyExc_ValueError, "bad assignment mode");

	//makes a set of dVerts corresponding to the mVerts
	if (!((Mesh*)object->data)->dvert) {
		create_dverts((Mesh*)object->data);
	}

	//loop list adding verts to group
	for (x = 0; x < PyList_Size(listObject); x++) {
		if (!(PyArg_Parse((PyList_GetItem(listObject, x)), "i", &tempInt)))
			return EXPP_ReturnPyObjError (PyExc_TypeError,
											"python list integer not parseable");

		if (tempInt < 0 || tempInt >= ((Mesh*)object->data)->totvert)
			return EXPP_ReturnPyObjError (PyExc_ValueError,
											"bad vertex index in list");

		add_vert_defnr(object, nIndex, tempInt, weight, assignmode);
	}

	return EXPP_incr_ret (Py_None);
}

static PyObject *NMesh_removeVertsFromGroup (PyObject *self, PyObject *args)
{
	//not passing a list will remove all verts from group

	char* groupStr;
	int nIndex;	
	struct Object* object;
	bDeformGroup* pGroup;
	PyObject* listObject; 
	int tempInt;
	int x, argc;

	/* argc is the number of parameters passed in: 1 (no list given) or 2: */
	argc = PyObject_Length(args);

	if (!PyArg_ParseTuple(args, "s|O!", &groupStr, &PyList_Type, &listObject))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected string and optional list argument");

	if (( (BPy_NMesh*)self )->object == NULL)
		return EXPP_ReturnPyObjError (PyExc_AttributeError, 
			"mesh must be linked to an object first...");

	object = ((BPy_NMesh*)self)->object;

	if (object->data == NULL)
		return EXPP_ReturnPyObjError (PyExc_AttributeError,
			"object contains no data...");

	if ((!((Mesh*)object->data)->dvert))
		return EXPP_ReturnPyObjError (PyExc_AttributeError,
			"this mesh contains no deform vertices...'");

	pGroup = get_named_vertexgroup(object, groupStr);
	if(pGroup == NULL)
		return EXPP_ReturnPyObjError (PyExc_AttributeError,
			"group does not exist!");

	nIndex = get_defgroup_num(object, pGroup);
		if(nIndex == -1)
			return EXPP_ReturnPyObjError (PyExc_AttributeError,
				"no deform groups assigned to mesh");

	if (argc == 1) /* no list given */ {
		//enter editmode
		if((G.obedit == 0))	
		{	
			//set current object
			BASACT->object = object;
			G.obedit= BASACT->object;
		}
		//set current vertex group
		nIndex++;
		object->actdef = nIndex;

		//clear all dVerts in active group
		remove_verts_defgroup (1);

		//exit editmode
		G.obedit = 0;
	}
	else
	{
		if(G.obedit != 0)	//remove_vert_def_nr doesn't like it if your in editmode
			G.obedit = 0;		

		//loop list adding verts to group
		for(x = 0; x < PyList_Size(listObject); x++) {
			if(!(PyArg_Parse((PyList_GetItem(listObject, x)), "i", &tempInt)))
				return EXPP_ReturnPyObjError (PyExc_TypeError,
						"python list integer not parseable");

			if(tempInt < 0 || tempInt >= ((Mesh*)object->data)->totvert)
				return EXPP_ReturnPyObjError (PyExc_ValueError,
								"bad vertex index in list");

			remove_vert_def_nr (object, nIndex, tempInt);
		}
	}

	return EXPP_incr_ret (Py_None);
}

static PyObject *NMesh_getVertsFromGroup (PyObject *self, PyObject *args)
{
	//not passing a list will return all verts from group
	//passing indecies not part of the group will not return data in pyList
	//can be used as a index/group check for a vertex

	char* groupStr;
	int nIndex;	
	int weightRet;
	struct Object* object;
	bDeformGroup* pGroup;
	MVert *mvert;
	MDeformVert *dvert;
	float weight;
	int i, k, l1, l2, count;
	int num = 0;
	PyObject* tempVertexList = NULL;
	PyObject* vertexList;
	PyObject* listObject; 
	int tempInt;
	int x;

	listObject = (void*)-2054456;	//can't use NULL macro because compiler thinks
									//it's a 0 and we need to check 0 index vertex pos
	l1 = FALSE;
	l2 = FALSE;
	weightRet = 0;

	if (!PyArg_ParseTuple(args, "s|iO!", &groupStr, &weightRet,
												&PyList_Type, &listObject))
				return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected string and optional int and list arguments");

	if (weightRet < 0 || weightRet > 1)
		return EXPP_ReturnPyObjError (PyExc_ValueError, 
			"return weights flag must be 0 or 1...");

	if(((BPy_NMesh*)self)->object == NULL)
		return EXPP_ReturnPyObjError (PyExc_AttributeError, 
			"mesh must be linked to an object first...");

	object = ((BPy_NMesh*)self)->object;

	if(object->data == NULL)
		return EXPP_ReturnPyObjError (PyExc_AttributeError,
								 "object contains no data...");

	if ((!((Mesh*)object->data)->dvert))
		return EXPP_ReturnPyObjError (PyExc_AttributeError,
								 "this mesh contains no deform vertices...'");

	pGroup = get_named_vertexgroup(object, groupStr);
	if(pGroup == NULL)
		return EXPP_ReturnPyObjError (PyExc_AttributeError,
						"group does not exist!");

	nIndex = get_defgroup_num(object, pGroup);
		if(nIndex == -1)
			return EXPP_ReturnPyObjError (PyExc_AttributeError,
					 "no deform groups assigned to mesh");

	//temporary list
	tempVertexList = PyList_New(((Mesh*)object->data)->totvert); 
	if (tempVertexList == NULL)
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
			"getVertsFromGroup: can't create pylist!");

	count = 0;

	if (listObject == (void *)-2054456) //do entire group
	{	
		for(k = 0; k < ((Mesh*)object->data)->totvert ; k++)
		{
			dvert = ((Mesh*)object->data)->dvert + k;

			for (i=0 ; i < dvert->totweight; i++)
			{
				if (dvert->dw[i].def_nr == nIndex) 
				{
					mvert = ((Mesh*)object->data)->mvert + k;
					weight = dvert->dw[i].weight;
					//printf("index =%3d weight:%10f\n", k, weight);
					
					if(weightRet == 1)
						PyList_SetItem(tempVertexList, count,
									Py_BuildValue("(i,f)", k, weight));
					else if (weightRet == 0)
						PyList_SetItem(tempVertexList, count, Py_BuildValue("i", k));

					count++;
				}
			}
		}
	}
	else	//do single vertex
	{
		//loop list adding verts to group
		for(x = 0; x < PyList_Size(listObject); x++)
		{
			if(!(PyArg_Parse((PyList_GetItem(listObject, x)), "i", &tempInt)))
				return EXPP_ReturnPyObjError (PyExc_TypeError,
					"python list integer not parseable");

			if(tempInt < 0 || tempInt >= ((Mesh*)object->data)->totvert)
				return EXPP_ReturnPyObjError (PyExc_ValueError,
					"bad vertex index in list");

			num = tempInt;
			dvert = ((Mesh*)object->data)->dvert + num;
			for (i=0 ; i < dvert->totweight; i++)
			{
				l1 = TRUE;
				if (dvert->dw[i].def_nr == nIndex) 
				{
					l2 = TRUE;
					mvert = ((Mesh*)object->data)->mvert + num;

					weight = dvert->dw[i].weight;
					//printf("index =%3d weight:%10f\n", num, weight);

					if(weightRet == 1) {
						PyList_SetItem(tempVertexList, count,
							Py_BuildValue("(i,f)", num, weight));
					}
					else if (weightRet == 0)
						PyList_SetItem(tempVertexList, count, Py_BuildValue("i", num));

					count++;
				}
				if(l2 == FALSE)
					printf("vertex at index %d is not part of passed group...\n", tempInt);
			}
			if(l1 == FALSE)
				printf("vertex at index %d is not assigned to a vertex group...\n", tempInt);

			l1 = l2 = FALSE;	//reset flags
		}
	}
	//only return what we need
	vertexList = PyList_GetSlice(tempVertexList, 0, count);

	Py_DECREF(tempVertexList);

	return (vertexList);
}

static PyObject *NMesh_renameVertGroup (PyObject *self, PyObject *args)
{
	char * oldGr = NULL; 
	char * newGr = NULL;
	bDeformGroup * defGroup = NULL;
	/*PyObject *tempStr;*/


	if(!((BPy_NMesh*)self)->object)
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
			"This mesh must be linked to an object");

	if (!PyArg_ParseTuple(args, "ss", &oldGr, &newGr))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"Expected string & string argument");

	defGroup = get_named_vertexgroup(((BPy_NMesh*)self)->object, oldGr);
	if(defGroup == NULL)
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
			"Couldn't find the expected vertex group");

	//set name
	PyOS_snprintf(defGroup->name, 32, newGr);
	unique_vertexgroup_name(defGroup, ((BPy_NMesh*)self)->object);

	return EXPP_incr_ret (Py_None);
}

static PyObject *NMesh_getVertGroupNames (PyObject *self, PyObject *args)
{
	bDeformGroup * defGroup;
	PyObject *list;

	if(!((BPy_NMesh*)self)->object)
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
			"This mesh must be linked to an object");

	list = PyList_New(0); 
	for (defGroup = (((BPy_NMesh*)self)->object)->defbase.first; defGroup; defGroup=defGroup->next){
		if(PyList_Append(list,PyString_FromString(defGroup->name)) < 0)
			return EXPP_ReturnPyObjError (PyExc_RuntimeError,
				"Couldn't add item to list");
	}

	return list;
}
