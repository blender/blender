/*
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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "BMesh.h" /* This must come first */
#include "Mathutils.h" /* This must come first */

#include "MEM_guardedalloc.h"

#include  "DNA_meshdata_types.h"

#include "BKE_mesh.h"
#include "BKE_library.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_scene.h"
#include "BKE_bmesh.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "blendef.h"
#include "Object.h"
#include "gen_utils.h"
#include "gen_library.h"


/* bmesh spesific includes */
#include "BKE_bmesh.h"

#define BMESH_SEQ_NORMAL			0
#define BMESH_SEQ_SELECTED			1
#define BMESH_SEQ_UNSELECTED		2
#define BMESH_SEQ_VISIBLE			3
#define BMESH_SEQ_HIDDEN			4

/* easy way to get the next selected or hidden item */
#define NEXT_FLAGGED(lb_item, f) \
	while ((lb_item) && ((lb_item)->flag & f)==0) (lb_item) = (lb_item)->next; 

#define NEXT_UNFLAGGED(lb_item, f) \
	while ((lb_item) && ((lb_item)->flag & f)==1) (lb_item) = (lb_item)->next;

/* This advances to the next item based on context */
#define NEXT_MODE(mode, iter)\
	switch (mode) {\
	case BMESH_SEQ_NORMAL:\
		break;\
	case BMESH_SEQ_SELECTED:\
		NEXT_FLAGGED(iter, SELECT);\
		break;\
	case BMESH_SEQ_UNSELECTED:\
		NEXT_UNFLAGGED(iter, SELECT);\
		break;\
	case BMESH_SEQ_VISIBLE:\
		NEXT_UNFLAGGED(iter, ME_HIDE);\
		break;\
	case BMESH_SEQ_HIDDEN:\
		NEXT_FLAGGED(iter, ME_HIDE);\
		break;\
	}\

/*
 * Python API function prototypes for the Blender module.
 */
/*static PyObject *M_BMesh_New( PyObject * self, PyObject * args );*/
PyObject *M_BMesh_GetEditMesh( PyObject * self, PyObject * args );

/* CreatePyObject */
PyObject *BMesh_CreatePyObject( BME_Mesh * bmesh )
{
	BPy_BMesh *bpybmesh;

	if( !bmesh )
		Py_RETURN_NONE;

	bpybmesh = ( BPy_BMesh * ) PyObject_NEW( BPy_BMesh, &BMesh_Type );

	if( bpybmesh == NULL ) {
		return ( NULL );
	}
	bpybmesh->bmesh= bmesh;
	return ( ( PyObject * ) bpybmesh );
}

PyObject *BMesh_Vert_CreatePyObject( BME_Vert *data)
{
	BPy_BMesh_Vert *value = PyObject_NEW( BPy_BMesh_Vert, &BMesh_Vert_Type);
	value->bvert= data;
	return (PyObject *)value;
}
PyObject *BMesh_Edge_CreatePyObject( BME_Edge *data)
{
	BPy_BMesh_Edge *value = PyObject_NEW( BPy_BMesh_Edge, &BMesh_Edge_Type);
	value->bedge= data;
	return (PyObject *)value;
}

PyObject *BMesh_Loop_CreatePyObject( BME_Loop *data)
{
	BPy_BMesh_Loop *value = PyObject_NEW( BPy_BMesh_Loop, &BMesh_Loop_Type);
	value->bloop= data;
	return (PyObject *)value;
}

PyObject *BMesh_Poly_CreatePyObject( BME_Poly *data)
{
	BPy_BMesh_Poly *value = PyObject_NEW( BPy_BMesh_Poly, &BMesh_Poly_Type);
	value->bpoly= data;
	return (PyObject *)value;
}

PyObject *BMesh_VertSeq_CreatePyObject( BME_Mesh *bmesh, BME_Vert *iter, void * mode)
{
	BPy_BMesh_VertSeq *value = PyObject_NEW( BPy_BMesh_VertSeq, &BMesh_VertSeq_Type);
	value->bmesh= bmesh;
	value->iter= iter;
	value->mode= (long)mode;
	return (PyObject *)value;
}
PyObject *BMesh_EdgeSeq_CreatePyObject( BME_Mesh *bmesh, BME_Edge *iter, void * mode)
{
	BPy_BMesh_EdgeSeq *value = PyObject_NEW( BPy_BMesh_EdgeSeq, &BMesh_EdgeSeq_Type);
	value->bmesh= bmesh;
	value->iter= iter;
	value->mode= (long)mode;
	return (PyObject *)value;
}
PyObject *BMesh_LoopSeq_CreatePyObject( BME_Loop *iter )
{
	BPy_BMesh_LoopSeq *value = PyObject_NEW( BPy_BMesh_LoopSeq, &BMesh_LoopSeq_Type);
	value->iter= iter;
	value->iter_init= iter; /* this is a reference */
	return (PyObject *)value;
}
PyObject *BMesh_PolySeq_CreatePyObject( BME_Mesh *bmesh, BME_Poly *iter, void * mode)
{
	BPy_BMesh_PolySeq *value = PyObject_NEW( BPy_BMesh_PolySeq, &BMesh_PolySeq_Type);
	value->bmesh= bmesh;
	value->iter= iter;
	value->mode= (long)mode;
	return (PyObject *)value;
}


static void BMesh_dealloc( BPy_BMesh * self )
{
	PyObject_DEL( self );
}

static void BMesh_Vert_dealloc( BPy_BMesh_Vert* self )
{
	PyObject_DEL( self );
}

static void BMesh_Edge_dealloc( BPy_BMesh_Edge* self )
{
	PyObject_DEL( self );
}
static void BMesh_Loop_dealloc( BPy_BMesh_Loop* self )
{
	PyObject_DEL( self );
}
static void BMesh_Poly_dealloc( BPy_BMesh_Poly* self )
{
	PyObject_DEL( self );
}

static void BMesh_VertSeq_dealloc( BPy_BMesh_VertSeq * self )
{
	PyObject_DEL( self );
}

static void BMesh_EdgeSeq_dealloc( BPy_BMesh_EdgeSeq * self )
{
	PyObject_DEL( self );
}
static void BMesh_LoopSeq_dealloc( BPy_BMesh_LoopSeq * self )
{
	PyObject_DEL( self );
}
static void BMesh_PolySeq_dealloc( BPy_BMesh_PolySeq * self )
{
	PyObject_DEL( self );
}

/* 
 * Visible Methods for BMesh datatypes
 */
PyObject *BMesh_Poly_flip( BPy_BMesh_Poly *self )
{
	if (BME_loop_reverse(self->bmesh, self->bpoly))
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}


PyObject *BMesh_VertSeq_add( BPy_BMesh_VertSeq *self, PyObject * args)
{
	VectorObject *vec=NULL;
	
	if(!PyArg_ParseTuple(args, "O!", &vector_Type, &vec) || vec->size != 3)
		return EXPP_ReturnPyObjError(PyExc_TypeError, 
			"bmesh.verts.add expects a 3D vector\n");
	
	return BMesh_Vert_CreatePyObject( BME_MV(self->bmesh, vec->vec) );
}

PyObject *BMesh_VertSeq_remove( BPy_BMesh_VertSeq *self, PyObject * args)
{
	BPy_BMesh_Vert *bpybvert;
	
	if(!PyArg_ParseTuple(args, "O!", &BMesh_Vert_Type, &bpybvert))
		return EXPP_ReturnPyObjError(PyExc_TypeError, 
				"bmesh.verts.remove expects a bmesh vertex\n");
	
	if (BME_KV(self->bmesh, bpybvert->bvert))
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}

PyObject *BMesh_VertSeq_transform( BPy_BMesh_VertSeq *self, PyObject * args)
{
	MatrixObject *bpymat=NULL;
	BME_Vert *bvert = self->bmesh->verts.first;
	
	if(!PyArg_ParseTuple(args, "O!", &matrix_Type, &bpymat))
		return EXPP_ReturnPyObjError(PyExc_TypeError, 
				"bmesh.verts.transform expects a matrix\n");
	
	if( bpymat->colSize != 4 || bpymat->rowSize != 4 )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
				"matrix must be a 4x4 transformation matrix\n"
				"for example as returned by object.getMatrix()" );
	
	
	switch (self->mode) {
	case BMESH_SEQ_NORMAL:
		for (;bvert;bvert=bvert->next)
			Mat4MulVecfl( (float(*)[4])*bpymat->matrix, bvert->co );
		break;
	case BMESH_SEQ_SELECTED:
		for (;bvert;bvert=bvert->next)
			if (bvert->flag & SELECT)
				Mat4MulVecfl( (float(*)[4])*bpymat->matrix, bvert->co );
		break;
	case BMESH_SEQ_UNSELECTED:
		for (;bvert;bvert=bvert->next)
			if ((bvert->flag & SELECT)==0)
				Mat4MulVecfl( (float(*)[4])*bpymat->matrix, bvert->co );
		break;
	case BMESH_SEQ_VISIBLE:
		for (;bvert;bvert=bvert->next)
			if ((bvert->flag & ME_HIDE)==0)
				Mat4MulVecfl( (float(*)[4])*bpymat->matrix, bvert->co );
		break;
	case BMESH_SEQ_HIDDEN:
		for (;bvert;bvert=bvert->next)
			if (bvert->flag & ME_HIDE)
				Mat4MulVecfl( (float(*)[4])*bpymat->matrix, bvert->co );
		break;
	}
	
	Py_RETURN_NONE;
}


PyObject *BMesh_EdgeSeq_add( BPy_BMesh_VertSeq *self, PyObject * args)
{
	BPy_BMesh_Vert *bpybvert1, *bpybvert2;
	BME_Edge *bedge;
	if(!PyArg_ParseTuple(args, "O!O!",	&BMesh_Vert_Type, &bpybvert1,
										&BMesh_Vert_Type, &bpybvert2))
		return EXPP_ReturnPyObjError(PyExc_TypeError, 
				"bmesh.edges.add expects 2 bmesh verts\n");
	bedge = BME_ME(self->bmesh, bpybvert1->bvert, bpybvert2->bvert);
	
	if (bedge)
		return BMesh_Edge_CreatePyObject(bedge);
	Py_RETURN_NONE;
}

PyObject *BMesh_EdgeSeq_remove( BPy_BMesh_VertSeq *self, PyObject * args)
{
	BPy_BMesh_Edge *bpybedge;
	if(!PyArg_ParseTuple(args, "O!",	&BMesh_Edge_Type, &bpybedge))	
		return EXPP_ReturnPyObjError(PyExc_TypeError, 
		"bmesh.edges.remove expects 1 edge\n");

	if (BME_KE(self->bmesh, bpybedge->bedge))
		Py_RETURN_TRUE;
	else	
		Py_RETURN_FALSE;
}
PyObject *BMesh_PolySeq_add( BPy_BMesh_VertSeq *self, PyObject * args)
{
	BPy_BMesh_Vert *bpybvert1, *bpybvert2;
	PyObject *pylist, *list_item;
	BME_Edge **edge_array;
	BME_Poly *new_poly; 
	int list_len, i, final_list_len;
	
	if(!PyArg_ParseTuple(args, "O!O!O!",&BMesh_Vert_Type,	&bpybvert1,
										&BMesh_Vert_Type,	&bpybvert2,
										&PyList_Type,		&pylist))
		return EXPP_ReturnPyObjError(PyExc_TypeError,
				"bmesh.edges.add expects 2 bmesh verts and a list of edges\n");
	
	list_len= PyList_Size(pylist);
	edge_array = MEM_mallocN(sizeof(BME_Edge*)*list_len, "bmesh edge array");
	for (i=0, final_list_len=0; i<list_len; i++) {
		list_item= PyList_GET_ITEM(pylist, i);
		
		if (BPy_BMesh_Edge_Check(list_item)) {
			edge_array[final_list_len] = ((BPy_BMesh_Edge *)list_item)->bedge;
			final_list_len++;
		}
	}
	
	new_poly = BME_MF(self->bmesh, bpybvert1->bvert, bpybvert2->bvert, edge_array, final_list_len);
	MEM_freeN(edge_array);
	
	if (new_poly)
		return BMesh_Poly_CreatePyObject(new_poly);
	
	Py_RETURN_NONE;
}
PyObject *BMesh_PolySeq_remove( BPy_BMesh_VertSeq *self, PyObject * args)
{
	BPy_BMesh_Poly *bpybpoly;
	if(!PyArg_ParseTuple(args, "O!",	&BMesh_Poly_Type, &bpybpoly))	
		return EXPP_ReturnPyObjError(PyExc_TypeError, 
		"bmesh.polys.remove expects 1 poly\n");

	if (BME_KF(self->bmesh, bpybpoly->bpoly))
		Py_RETURN_TRUE;
	else	
		Py_RETURN_FALSE;
}


/*
 *  Python method structure definition for Blender.Object module:
 */
struct PyMethodDef M_BMesh_methods[] = {
	/*{"New", ( PyCFunction ) M_BMesh_New, METH_VARARGS,
	 "(name) Add a new empty bmesh"},*/
	{"Get", ( PyCFunction ) M_BMesh_GetEditMesh, METH_VARARGS,
	 "(name) - return the bmeshwith the name 'name',\
	 returns None if notfound.\nIf 'name' is not specified, it returns a list of all bmeshs."},
	{NULL, NULL, 0, NULL}
};

static PyMethodDef BPy_BMesh_Vert_methods[] = {
	{NULL, NULL, 0, NULL}
};
static PyMethodDef BPy_BMesh_Edge_methods[] = {
	{NULL, NULL, 0, NULL}
};
static PyMethodDef BPy_BMesh_Loop_methods[] = {
	{NULL, NULL, 0, NULL}
};
static PyMethodDef BPy_BMesh_Poly_methods[] = {
	{"flip", ( PyCFunction ) BMesh_Poly_flip, METH_NOARGS,
	 "() flip this polygon"},
	{NULL, NULL, 0, NULL}
};

static PyMethodDef BPy_BMesh_VertSeq_methods[] = {
	{"add", ( PyCFunction ) BMesh_VertSeq_add, METH_VARARGS,
	 "(co) add a new vertex"},
	{"remove", ( PyCFunction ) BMesh_VertSeq_remove, METH_VARARGS,
	 "(bvert) remove a vertex"},
	{"transform", ( PyCFunction ) BMesh_VertSeq_transform, METH_VARARGS,
	 "(matrix) transform verts by a matrix"},
	{NULL, NULL, 0, NULL}
};
static PyMethodDef BPy_BMesh_EdgeSeq_methods[] = {
	{"add", ( PyCFunction ) BMesh_EdgeSeq_add, METH_VARARGS,
	 "(v1,v2) add a new vertex"},
	{"remove", ( PyCFunction ) BMesh_EdgeSeq_remove, METH_VARARGS,
	 "(bedge) add a new vertex"},
	{NULL, NULL, 0, NULL}
};

static PyMethodDef BPy_BMesh_LoopSeq_methods[] = {
	{NULL, NULL, 0, NULL},
};
static PyMethodDef BPy_BMesh_PolySeq_methods[] = {
	{"add", ( PyCFunction ) BMesh_PolySeq_add, METH_VARARGS,
	 "(*verts) add a new vertex"},
	{"remove", ( PyCFunction ) BMesh_PolySeq_remove, METH_VARARGS,
	 "(bpoly) add a new vertex"},
	{NULL, NULL, 0, NULL},
};


/*
 * Python BPy_BMesh methods table:
 */

static PyObject *BPy_BMesh_copy( BPy_BMesh * self );

static PyMethodDef BPy_BMesh_methods[] = {
	/* name, method, flags, doc */
	{"__copy__", ( PyCFunction ) BPy_BMesh_copy, METH_VARARGS,
	 "() - Return a copy of the bmeshcontaining the same objects."},
	{"copy", ( PyCFunction ) BPy_BMesh_copy, METH_VARARGS,
	 "() - Return a copy of the bmeshcontaining the same objects."},
	{NULL, NULL, 0, NULL}
};


static PyObject *BPy_BMesh_copy( BPy_BMesh * self )
{
	return BMesh_CreatePyObject(BME_copy_mesh(self->bmesh));
}


/*
 * Python BPy_BMesh getsetettr funcs
 * This can accept sequence types also since
 * bmesh is in the same place in teh struct
 */
static PyObject *BMesh_getVerts( BPy_BMesh * self, void * mode )
{
	return BMesh_VertSeq_CreatePyObject(self->bmesh, NULL, mode);
}

static PyObject *BMesh_getEdges( BPy_BMesh * self, void * mode )
{
	return BMesh_EdgeSeq_CreatePyObject(self->bmesh, NULL, mode);
}
/*static PyObject *BMesh_getLoops( BPy_BMesh * self)
{
	return BMesh_LoopSeq_CreatePyObject(self->bmesh->loops.first);
}*/

static PyObject *BMesh_getPolys( BPy_BMesh * self, void * mode)
{
	return BMesh_PolySeq_CreatePyObject(self->bmesh, NULL, mode);
}

/*
 * Python attributes get/set structure
 */
static PyGetSetDef BPy_BMesh_getseters[] = {
	{"verts",
	 (getter)BMesh_getVerts, (setter)NULL,
	 "BMesh verts",
	 (void *)BMESH_SEQ_NORMAL},
	{"edges",
	 (getter)BMesh_getEdges, (setter)NULL,
	 "BMesh edges",
	 (void *)BMESH_SEQ_NORMAL},
	 /*Loops should not be directly accessed - Briggs*/
	/*{"loops",
	 (getter)BMesh_getLoops, (setter)NULL,
	 "BMesh faces",
	 NULL},*/
	{"polys",
	 (getter)BMesh_getPolys, (setter)NULL,
	 "BMesh faces",
	 (void *)BMESH_SEQ_NORMAL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/*
 * Blender.BMesh.New()
 */
/*PyObject *M_BMesh_New( PyObject * self, PyObject * args )
{
	return BMesh_CreatePyObject(BME_make_mesh());
}*/

/*
 * Blender.BMesh.Get()
 */
PyObject *M_BMesh_GetEditMesh( PyObject * self, PyObject * args )
{
	if (G.editMesh)
		return BMesh_CreatePyObject(G.editMesh);
	
	Py_RETURN_NONE;
}

/*****************************************************************************/
/* Function:	BMesh_FromPyObject					 */
/* Description: This function returns the Blender bmeshfrom the given	 */
/*		PyObject.						 */
/*****************************************************************************/
BME_Mesh *BMesh_FromPyObject( PyObject * pyob )
{
	return ((BPy_BMesh *)pyob)->bmesh;
}

/************************************************************************
 *
 * BMeshOb sequence 
 *
 ************************************************************************/
/*
 * create a thin BMeshOb object
 */

static int BMesh_VertSeq_len( BPy_BMesh_VertSeq * self )
{
	int tot=0;
	BME_Vert *item= self->bmesh->verts.first;
	
	switch (self->mode) {
	case BMESH_SEQ_NORMAL:
		return self->bmesh->totvert;
	case BMESH_SEQ_SELECTED:
		for (; item; item= item->next)	if(item->flag & SELECT) tot++;
		break;
	case BMESH_SEQ_UNSELECTED:
		for (; item; item= item->next)	if((item->flag & SELECT)==0) tot++;
		break;
	case BMESH_SEQ_VISIBLE:
		for (; item; item= item->next)	if ((item->flag & ME_HIDE)==0) tot++;
		break;
	case BMESH_SEQ_HIDDEN:
		for (; item; item= item->next)	if (item->flag & ME_HIDE) tot++;
		break;
	}
	return tot;
}
static int BMesh_EdgeSeq_len( BPy_BMesh_EdgeSeq * self )
{
	int tot=0;
	BME_Edge *item = self->bmesh->edges.first;
	
	switch (self->mode) {
	case BMESH_SEQ_NORMAL:
		return self->bmesh->totedge;
	case BMESH_SEQ_SELECTED:
		for (; item; item= item->next)	if (item->flag & SELECT) tot++;
		break;
	case BMESH_SEQ_UNSELECTED:
		for (; item; item= item->next)	if ((item->flag & SELECT)==0) tot++;
		break;
	case BMESH_SEQ_VISIBLE:
		for (; item; item= item->next)	if ((item->flag & ME_HIDE)==0) tot++;
		break;
	case BMESH_SEQ_HIDDEN:
		for (; item; item= item->next)	if (item->flag & ME_HIDE) tot++;
		break;
	}
	return tot;
}
static int BMesh_LoopSeq_len( BPy_BMesh_LoopSeq * self )
{
	/* odd code for measuring a circular linked list */
	int tot;
	BME_Loop *item = self->iter_init;
	if (!item)
		return 0;
	if (item==item->next)
		return 1; /* a circular list of 1? */
	
	tot= 1;
	for (tot=0, item = item->next; item != self->iter_init; item = item->next) {
		tot++;
	}
	return tot;
}
static int BMesh_PolySeq_len( BPy_BMesh_PolySeq * self )
{
	int tot=0;
	BME_Edge *item= self->bmesh->polys.first;
	
	switch (self->mode) {
	case BMESH_SEQ_NORMAL:
		return self->bmesh->totpoly;
	case BMESH_SEQ_SELECTED:
		for (; item; item= item->next)	if (item->flag & SELECT) tot++;
		break;
	case BMESH_SEQ_UNSELECTED:
		for (; item; item= item->next)	if ((item->flag & SELECT)==0) tot++;
		break;
	case BMESH_SEQ_VISIBLE:
		for (; item; item= item->next)	if ((item->flag & ME_HIDE)==0) tot++;
		break;
	case BMESH_SEQ_HIDDEN:
		for (; item; item= item->next)	if (item->flag & ME_HIDE) tot++;
		break;	
	}
	return tot;
}

static PySequenceMethods BMesh_VertSeq_as_sequence = {
	( inquiry ) BMesh_VertSeq_len,	/* sq_length */
	0,0,0,0,0,0,0,0,0
};
static PySequenceMethods BMesh_EdgeSeq_as_sequence = {
	( inquiry ) BMesh_EdgeSeq_len,	/* sq_length */
	0,0,0,0,0,0,0,0,0
};
static PySequenceMethods BMesh_LoopSeq_as_sequence = {
	( inquiry ) BMesh_LoopSeq_len,	/* sq_length */
	0,0,0,0,0,0,0,0,0
};
static PySequenceMethods BMesh_PolySeq_as_sequence = {
	( inquiry ) BMesh_PolySeq_len,	/* sq_length */
	0,0,0,0,0,0,0,0,0
};


/************************************************************************
 *
 * Python BMesh_VertSeq_Type iterator (iterates over BMeshObjects)
 *
 ************************************************************************/

/*
 * Initialize the interator index
 */

static PyObject *BMesh_VertSeq_getIter( BPy_BMesh_VertSeq * self )
{
	BME_Vert *data = self->bmesh->verts.first;
	
	/* get the first vert */
	NEXT_MODE(self->mode, data);
		
	if (!self->iter) {
		self->iter = data;
		return EXPP_incr_ret( (PyObject *) self );
	} else {
		return BMesh_VertSeq_CreatePyObject(self->bmesh, data, (void *)self->mode);
	}
}

static PyObject *BMesh_EdgeSeq_getIter( BPy_BMesh_EdgeSeq * self )
{
	
	BME_Edge *data = self->bmesh->edges.first;
	
	/* get the first edge */
	NEXT_MODE(self->mode, data);
	
	if (!self->iter) {
		self->iter = data;
		return EXPP_incr_ret( (PyObject *) self );
	} else {
		return BMesh_EdgeSeq_CreatePyObject(self->bmesh, data, (void *)self->mode);
	}
}
static PyObject *BMesh_LoopSeq_getIter( BPy_BMesh_LoopSeq * self )
{
	if (self->iter==NULL) {
		self->iter = self->iter_init;
		return EXPP_incr_ret( (PyObject *) self );
	} else {
		return BMesh_LoopSeq_CreatePyObject(self->iter_init);
	}
}
static PyObject *BMesh_PolySeq_getIter( BPy_BMesh_PolySeq * self )
{
	BME_Poly *data = self->bmesh->polys.first;
	
	/* get the first poly */
	NEXT_MODE(self->mode, data);
	
	if (!self->iter) {
		self->iter = data;
		return EXPP_incr_ret( (PyObject *) self );
	} else {
		return BMesh_PolySeq_CreatePyObject(self->bmesh, data, (void *)self->mode);
	}
}

/*
 * Return next BMeshOb.
 */
static PyObject *BMesh_VertSeq_nextIter( BPy_BMesh_VertSeq * self )
{
	PyObject *value;
	if( !(self->iter) || !(self->bmesh) ) {
		self->iter = NULL; /* so we can add objects again */
		return EXPP_ReturnPyObjError( PyExc_StopIteration,
				"iterator at end" );
	}
	
	value= BMesh_Vert_CreatePyObject( self->iter ); 
	self->iter= self->iter->next;
	
	NEXT_MODE(self->mode, self->iter);
	return value;
}

static PyObject *BMesh_EdgeSeq_nextIter( BPy_BMesh_EdgeSeq * self )
{
	PyObject *value;
	if( !(self->iter) || !(self->bmesh) ) {
		self->iter = NULL; /* so we can add objects again */
		return EXPP_ReturnPyObjError( PyExc_StopIteration,
				"iterator at end" );
	}
	
	value= BMesh_Edge_CreatePyObject( self->iter ); 
	self->iter= self->iter->next;
	
	NEXT_MODE(self->mode, self->iter);
	return value;
}
/* circular loop needs to be handeled differently*/
static PyObject *BMesh_LoopSeq_nextIter( BPy_BMesh_LoopSeq * self )
{
	PyObject *value;
	if( !self->iter ) {
		self->iter = NULL; /* so we can add objects again */
		return EXPP_ReturnPyObjError( PyExc_StopIteration,
				"iterator at end" );
	}
	
	value= BMesh_Loop_CreatePyObject( self->iter );
	if (self->iter->next == self->iter_init) {
		self->iter = NULL; /* */
	} else {
		self->iter= self->iter->next;
	}
	return value;
}

static PyObject *BMesh_PolySeq_nextIter( BPy_BMesh_PolySeq * self )
{
	PyObject *value;
	if( !(self->iter) || !(self->bmesh) ) {
		self->iter = NULL; /* so we can add objects again */
		return EXPP_ReturnPyObjError( PyExc_StopIteration,
				"iterator at end" );
	}
	
	value= BMesh_Poly_CreatePyObject( self->iter ); 
	self->iter= self->iter->next;
	
	NEXT_MODE(self->mode, self->iter);
	return value;
}


/*****************************************************************************/
/* PythonTypeObject callback function prototypes			 */
/*****************************************************************************/


/* Compare */
static int BMesh_compare( BPy_BMesh * a, BPy_BMesh * b )
{
	BME_Mesh *pa = a->bmesh, *pb = b->bmesh;
	return ( pa == pb ) ? 0 : -1;
}
/* items */
static int BMesh_Vert_compare( BPy_BMesh_Vert * a, BPy_BMesh_Vert * b )
{
	BME_Vert *pa = a->bvert, *pb = b->bvert;
	return ( pa == pb ) ? 0 : -1;
}
static int BMesh_Edge_compare( BPy_BMesh_Edge * a, BPy_BMesh_Edge * b )
{
	BME_Edge *pa = a->bedge, *pb = b->bedge;
	return ( pa == pb ) ? 0 : -1;
}
static int BMesh_Loop_compare( BPy_BMesh_Loop * a, BPy_BMesh_Loop * b )
{
	BME_Loop *pa = a->bloop, *pb = b->bloop;
	return ( pa == pb ) ? 0 : -1;
}
static int BMesh_Poly_compare( BPy_BMesh_Poly * a, BPy_BMesh_Poly * b )
{
	BME_Poly *pa = a->bpoly, *pb = b->bpoly;
	return ( pa == pb ) ? 0 : -1;
}
/* sequences */
static int BMesh_VertSeq_compare( BPy_BMesh_VertSeq * a, BPy_BMesh_VertSeq * b )
{
	BME_Mesh *pa = a->bmesh, *pb = b->bmesh;
	return ( pa == pb ) ? 0 : -1;
}
static int BMesh_EdgeSeq_compare( BPy_BMesh_EdgeSeq * a, BPy_BMesh_EdgeSeq * b )
{
	BME_Mesh *pa = a->bmesh, *pb = b->bmesh;
	return ( pa == pb ) ? 0 : -1;
}
static int BMesh_LoopSeq_compare( BPy_BMesh_LoopSeq * a, BPy_BMesh_LoopSeq * b )
{
	BME_Loop *pa = a->iter_init, *pb = b->iter_init;
	return ( pa == pb ) ? 0 : -1;
}
static int BMesh_PolySeq_compare( BPy_BMesh_PolySeq * a, BPy_BMesh_PolySeq * b )
{
	BME_Mesh *pa = a->bmesh, *pb = b->bmesh;
	return ( pa == pb ) ? 0 : -1;
}


/* repr - when printing */
static PyObject *BMesh_repr( BPy_BMesh * self )
{
	return PyString_FromString( "[BMesh]" );
}

/* items */
static PyObject *BMesh_Vert_repr( BPy_BMesh_Vert * self )
{
	return PyString_FromString( "[BMesh Vertex]" );
}
static PyObject *BMesh_Edge_repr( BPy_BMesh_Edge * self )
{
	return PyString_FromString( "[BMesh Edge]" );
}
static PyObject *BMesh_Loop_repr( BPy_BMesh_Loop * self )
{
	return PyString_FromString( "[BMesh Loop]" );
}
static PyObject *BMesh_Poly_repr( BPy_BMesh_Poly * self )
{
	return PyString_FromString( "[BMesh Poly]" );
}
/* sequences */
static PyObject *BMesh_VertSeq_repr( BPy_BMesh_VertSeq * self )
{
	return PyString_FromString( "[BMesh Vertex Sequence]" );
}
static PyObject *BMesh_EdgeSeq_repr( BPy_BMesh_EdgeSeq * self )
{
	return PyString_FromString( "[BMesh Edge Sequence]" );
}
static PyObject *BMesh_LoopSeq_repr( BPy_BMesh_LoopSeq * self )
{
	return PyString_FromString( "[BMesh Loop Sequence]" );
}
static PyObject *BMesh_PolySeq_repr( BPy_BMesh_PolySeq * self )
{
	return PyString_FromString( "[BMesh Poly Sequence]" );
}

/*
 * get a vertex's coordinate
 */

static PyObject *BMesh_Vert_getCoord( BPy_BMesh_Vert * self )
{
	return (PyObject *)newVectorObject( self->bvert->co, 3, Py_WRAP );
}

/*
 * set a vertex's coordinate
 */

static int BMesh_Vert_setCoord( BPy_BMesh_Vert * self, VectorObject * value )
{
	if( !VectorObject_Check( value ) || value->size != 3 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected vector argument of size 3" );

	self->bvert->co[0] = value->vec[0];
	self->bvert->co[1] = value->vec[1];
	self->bvert->co[2] = value->vec[2];
	return 0;
}

/*
 * get a vertex's normal
 */
static PyObject *BMesh_Vert_getNormal( BPy_BMesh_Vert * self )
{
	return (PyObject *)newVectorObject( self->bvert->no, 3, Py_WRAP );
}

/*
 * set a vertex's normal
 */
static int BMesh_Vert_setNormal( BPy_BMesh_Vert * self, VectorObject * value )
{
	if( !VectorObject_Check( value ) || value->size != 3 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected vector argument of size 3" );

	self->bvert->no[0]= value->vec[0];
	self->bvert->no[1]= value->vec[1];
	self->bvert->no[2]= value->vec[2];
	return 0;
}

/*
 * get a vertex's normal
 */
static PyObject *BMesh_Vert_getSel( BPy_BMesh_Vert * self )
{
	return EXPP_getBitfield( &self->bvert->flag, SELECT, 'h' );
}

/*
 * set a vertex's normal
 */
static int BMesh_Vert_setSel( BPy_BMesh_Vert * self, PyObject * value )
{
	return EXPP_setBitfield( value, &self->bvert->flag, SELECT, 'h' );
}	



/*****************************************************************************/
/* Python attributes get/set structure:                                      */
/*****************************************************************************/
static PyGetSetDef BPy_BMesh_Vert_getseters[] = {
	{"co",
	 (getter)BMesh_Vert_getCoord, (setter)BMesh_Vert_setCoord,
	 "BMesh Vert location",
	 NULL},
 	{"no",
	 (getter)BMesh_Vert_getNormal, (setter)BMesh_Vert_setNormal,
	 "BMesh Vert normals",
	 NULL},
	{"sel",
	 (getter)BMesh_Vert_getSel, (setter)BMesh_Vert_setSel,
	 "BMesh Vert selection state",
	 NULL},
	/*{"hide",
	 (getter)BMesh_Vert_getHide, (setter)BMesh_Vert_setHide,
	 "BMesh Vert hidden state",
	 NULL}, */
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};



/*
 * get a vertex's normal
 */
static PyObject *BMesh_Edge_getSel( BPy_BMesh_Edge * self )
{
	return EXPP_getBitfield( &self->bedge->flag, SELECT, 'h' );
}

/*
 * set a vertex's normal
 */
static int BMesh_Edge_setSel( BPy_BMesh_Edge * self, PyObject * value )
{
	return EXPP_setBitfield( value, &self->bedge->flag, SELECT, 'h' );
}


/*
 * get a vertex's normal
 */
static PyObject *BMesh_Edge_getVert( BPy_BMesh_Edge * self, void * mode )
{
	if (((long)mode)==0)
		return BMesh_Vert_CreatePyObject(self->bedge->v1);
	else
		return BMesh_Vert_CreatePyObject(self->bedge->v2);
}

static PyObject *BMesh_Edge_getLoop( BPy_BMesh_Edge * self, PyObject * value )
{ /* WARNING- NO BMESH ACCESS HERE!! */
	return BMesh_LoopSeq_CreatePyObject(self->bedge->loop);
}

/*****************************************************************************/
/* Python attributes get/set structure:                                      */
/*****************************************************************************/
static PyGetSetDef BPy_BMesh_Edge_getseters[] = {
	{"v1",
	 (getter)BMesh_Edge_getVert, (setter)NULL,
	 "Vert 1",
	 (void *)0},
 	{"v2",
	 (getter)BMesh_Edge_getVert, (setter)NULL,
	 "Vert 2",
	 (void *)1},
	{"sel",
	 (getter)BMesh_Edge_getSel, (setter)BMesh_Edge_setSel,
	 "Edge Selection",
	 NULL},
	{"loop",
	 (getter)BMesh_Edge_getLoop, (setter)NULL,
	 "Edge Selection",
	 NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};


/*
 * get a loops's vert
 */
static PyObject *BMesh_Loop_getVert( BPy_BMesh_Loop * self, void * mode )
{
	return BMesh_Vert_CreatePyObject(self->bloop->v);
}
static PyObject *BMesh_Loop_getEdge( BPy_BMesh_Loop * self, void * mode )
{
	return BMesh_Edge_CreatePyObject(self->bloop->e);
}
static PyObject *BMesh_Loop_getPoly( BPy_BMesh_Loop * self, void * mode )
{
	return BMesh_Poly_CreatePyObject(self->bloop->f);
}

/*****************************************************************************/
/* Python attributes get/set structure:                                      */
/*****************************************************************************/
static PyGetSetDef BPy_BMesh_Loop_getseters[] = {
	{"vert",
	 (getter)BMesh_Loop_getVert, NULL,
	 "Edge Selection",
	 NULL},
	{"edge",
	 (getter)BMesh_Loop_getEdge, NULL,
	 "Edge Selection",
	 NULL},
	{"poly",
	 (getter)BMesh_Loop_getPoly, NULL,
	 "Edge Selection",
	 NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/*
 * polys's selection
 */
static PyObject *BMesh_Poly_getSel( BPy_BMesh_Poly * self )
{
	return EXPP_getBitfield( &self->bpoly->flag, SELECT, 'h' );
}
static int BMesh_Poly_setSel( BPy_BMesh_Poly * self, PyObject * value )
{
	return EXPP_setBitfield( value, &self->bpoly->flag, SELECT, 'h' );
}

/*
 * polys's material
 */
static PyObject *BMesh_Poly_getMat( BPy_BMesh_Poly * self )
{
	return PyInt_FromLong((long)self->bpoly->mat_nr);
}
static int BMesh_Poly_setMat( BPy_BMesh_Poly * self, PyObject * value )
{
	return EXPP_setIValueRange( value, &self->bpoly->mat_nr, 0, 15, 'b' );
}

static PyObject *BMesh_Poly_getLoop( BPy_BMesh_Poly * self, PyObject * value )
{ /* WARNING- NO BMESH ACCESS HERE!! */
	return BMesh_LoopSeq_CreatePyObject(self->bpoly->loopbase);
}

/*****************************************************************************/
/* Python attributes get/set structure:                                      */
/*****************************************************************************/
static PyGetSetDef BPy_BMesh_Poly_getseters[] = {
	{"sel",
	 (getter)BMesh_Poly_getSel, (setter)BMesh_Poly_setSel,
	 "Poly Selection",
	 NULL},
	{"mat",
	 (getter)BMesh_Poly_getMat, (setter)BMesh_Poly_setMat,
	 "Poly Material Index",
	 NULL},
	{"loop",
	 (getter)BMesh_Poly_getLoop, NULL,
	 "Poly Loop",
	 NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

static PyGetSetDef BPy_BMesh_VertSeq_getseters[] = {
	{"selected",
	 (getter)BMesh_getVerts, (setter)NULL,
	 "Selected Verts",
	 (void *)BMESH_SEQ_SELECTED},
	{"unselected",
	 (getter)BMesh_getVerts, (setter)NULL,
	 "UnSelected Verts",
	 (void *)BMESH_SEQ_UNSELECTED},
	{"visible",
	 (getter)BMesh_getVerts, (setter)NULL,
	 "Visible Verts",
	 (void *)BMESH_SEQ_VISIBLE},
	{"hidden",
	 (getter)BMesh_getVerts, (setter)NULL,
	 "Hidden Verts",
	 (void *)BMESH_SEQ_HIDDEN},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

static PyGetSetDef BPy_BMesh_EdgeSeq_getseters[] = {
	{"selected",
	 (getter)BMesh_getEdges, (setter)NULL,
	 "Selected edges",
	 (void *)BMESH_SEQ_SELECTED},
	{"unselected",
	 (getter)BMesh_getEdges, (setter)NULL,
	 "UnSelected edges",
	 (void *)BMESH_SEQ_UNSELECTED},
	{"visible",
	 (getter)BMesh_getEdges, (setter)NULL,
	 "Visible Edges",
	 (void *)BMESH_SEQ_VISIBLE},
	{"hidden",
	 (getter)BMesh_getEdges, (setter)NULL,
	 "Hidden Edges",
	 (void *)BMESH_SEQ_HIDDEN},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

static PyGetSetDef BPy_BMesh_PolySeq_getseters[] = {
	{"selected",
	 (getter)BMesh_getPolys, (setter)NULL,
	 "Selected Polys",
	 (void *)BMESH_SEQ_SELECTED},
	{"unselected",
	 (getter)BMesh_getPolys, (setter)NULL,
	 "UnSelected Polys",
	 (void *)BMESH_SEQ_UNSELECTED},
	{"visible",
	 (getter)BMesh_getPolys, (setter)NULL,
	 "Visible Polys",
	 (void *)BMESH_SEQ_VISIBLE},
	{"hidden",
	 (getter)BMesh_getPolys, (setter)NULL,
	 "Hidden Polys",
	 (void *)BMESH_SEQ_HIDDEN},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/*
 * Python TypeBMesh structure definition:
 */
PyTypeObject BMesh_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender BMesh",             /* char *tp_name; */
	sizeof( BPy_BMesh ),         /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) BMesh_dealloc,						/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) BMesh_compare,   /* cmpfunc tp_compare; */
	( reprfunc ) BMesh_repr,     /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,						/* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_BMesh_methods,           /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_BMesh_getseters,         /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};

/*****************************************************************************/
/* Python Type BMeshVert structure definition:                               */
/*****************************************************************************/
PyTypeObject BMesh_Vert_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender BMesh Vert",             /* char *tp_name; */
	sizeof( BPy_BMesh_Vert ),         /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	(destructor) BMesh_Vert_dealloc,						/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) BMesh_Vert_compare,   /* cmpfunc tp_compare; */
	( reprfunc ) BMesh_Vert_repr,     /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,						/* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_BMesh_Vert_methods,     /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_BMesh_Vert_getseters,         /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};


/*****************************************************************************/
/* Python Type BMeshEdge structure definition:                               */
/*****************************************************************************/
PyTypeObject BMesh_Edge_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender BMesh Edge",             /* char *tp_name; */
	sizeof( BPy_BMesh_Edge ),         /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	(destructor) BMesh_Edge_dealloc,						/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) BMesh_Edge_compare,   /* cmpfunc tp_compare; */
	( reprfunc ) BMesh_Edge_repr,     /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,						/* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_BMesh_Edge_methods,     /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_BMesh_Edge_getseters,         /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};


PyTypeObject BMesh_Loop_Type = {
		PyObject_HEAD_INIT( NULL )  /* required py macro */
		0,                          /* ob_size */
		/*  For printing, in format "<module>.<name>" */
		"Blender BMesh Loop",             /* char *tp_name; */
		sizeof( BPy_BMesh_Loop ),         /* int tp_basicsize; */
		0,                          /* tp_itemsize;  For allocation */

		/* Methods to implement standard operations */

		(destructor) BMesh_Loop_dealloc,						/* destructor tp_dealloc; */
		NULL,                       /* printfunc tp_print; */
		NULL,                       /* getattrfunc tp_getattr; */
		NULL,                       /* setattrfunc tp_setattr; */
		( cmpfunc ) BMesh_Loop_compare,   /* cmpfunc tp_compare; */
		( reprfunc ) BMesh_Loop_repr,     /* reprfunc tp_repr; */

		/* Method suites for standard classes */

		NULL,                       /* PyNumberMethods *tp_as_number; */
		NULL,                       /* PySequenceMethods *tp_as_sequence; */
		NULL,                       /* PyMappingMethods *tp_as_mapping; */

		/* More standard operations (here for binary compatibility) */

		NULL,						/* hashfunc tp_hash; */
		NULL,                       /* ternaryfunc tp_call; */
		NULL,                       /* reprfunc tp_str; */
		NULL,                       /* getattrofunc tp_getattro; */
		NULL,                       /* setattrofunc tp_setattro; */

		/* Functions to access object as input/output buffer */
		NULL,                       /* PyBufferProcs *tp_as_buffer; */

	  /*** Flags to define presence of optional/expanded features ***/
		Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

		NULL,                       /*  char *tp_doc;  Documentation string */
	  /*** Assigned meaning in release 2.0 ***/
		/* call function for all accessible objects */
		NULL,                       /* traverseproc tp_traverse; */

		/* delete references to contained objects */
		NULL,                       /* inquiry tp_clear; */

	  /***  Assigned meaning in release 2.1 ***/
	  /*** rich comparisons ***/
		NULL,                       /* richcmpfunc tp_richcompare; */

	  /***  weak reference enabler ***/
		0,                          /* long tp_weaklistoffset; */

	  /*** Added in release 2.2 ***/
		/*   Iterators */
		NULL,                       /* getiterfunc tp_iter; */
		NULL,                       /* iternextfunc tp_iternext; */

	  /*** Attribute descriptor and subclassing stuff ***/
		BPy_BMesh_Loop_methods,     /* struct PyMethodDef *tp_methods; */
		NULL,                       /* struct PyMemberDef *tp_members; */
		BPy_BMesh_Loop_getseters,         /* struct PyGetSetDef *tp_getset; */
		NULL,                       /* struct _typeobject *tp_base; */
		NULL,                       /* PyObject *tp_dict; */
		NULL,                       /* descrgetfunc tp_descr_get; */
		NULL,                       /* descrsetfunc tp_descr_set; */
		0,                          /* long tp_dictoffset; */
		NULL,                       /* initproc tp_init; */
		NULL,                       /* allocfunc tp_alloc; */
		NULL,                       /* newfunc tp_new; */
		/*  Low-level free-memory routine */
		NULL,                       /* freefunc tp_free;  */
		/* For PyObject_IS_GC */
		NULL,                       /* inquiry tp_is_gc;  */
		NULL,                       /* PyObject *tp_bases; */
		/* method resolution order */
		NULL,                       /* PyObject *tp_mro;  */
		NULL,                       /* PyObject *tp_cache; */
		NULL,                       /* PyObject *tp_subclasses; */
		NULL,                       /* PyObject *tp_weaklist; */
		NULL
	};

/*****************************************************************************/
/* Python Type BMeshPoly structure definition:                               */
/*****************************************************************************/
PyTypeObject BMesh_Poly_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender BMesh Poly",             /* char *tp_name; */
	sizeof( BPy_BMesh_Poly ),         /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	(destructor) BMesh_Poly_dealloc,						/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) BMesh_Poly_compare,   /* cmpfunc tp_compare; */
	( reprfunc ) BMesh_Poly_repr,     /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,						/* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_BMesh_Poly_methods,     /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_BMesh_Poly_getseters,         /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};


/*****************************************************************************/
/* Python Type BMeshVert structure definition:                               */
/*****************************************************************************/
PyTypeObject BMesh_VertSeq_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender BMesh VertSeq",             /* char *tp_name; */
	sizeof( BPy_BMesh_VertSeq ),         /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	(destructor) BMesh_VertSeq_dealloc,						/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) BMesh_VertSeq_compare,   /* cmpfunc tp_compare; */
	( reprfunc ) BMesh_VertSeq_repr,     /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	&BMesh_VertSeq_as_sequence,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,						/* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	( getiterfunc) BMesh_VertSeq_getIter,                       /* getiterfunc tp_iter; */
	( iternextfunc ) BMesh_VertSeq_nextIter,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_BMesh_VertSeq_methods,     /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_BMesh_VertSeq_getseters,/* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};


/*****************************************************************************/
/* Python Type BMeshVert structure definition:                               */
/*****************************************************************************/
PyTypeObject BMesh_EdgeSeq_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender BMesh EdgeSeq",             /* char *tp_name; */
	sizeof( BPy_BMesh_EdgeSeq ),         /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	(destructor) BMesh_EdgeSeq_dealloc,						/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) BMesh_EdgeSeq_compare,   /* cmpfunc tp_compare; */
	( reprfunc ) BMesh_EdgeSeq_repr,     /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	&BMesh_EdgeSeq_as_sequence,	/* PySequenceMethods *tp_as_sequence; */
	NULL,                    	/* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,						/* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	( getiterfunc) BMesh_EdgeSeq_getIter,                       /* getiterfunc tp_iter; */
	( iternextfunc ) BMesh_EdgeSeq_nextIter,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_BMesh_EdgeSeq_methods,  /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_BMesh_EdgeSeq_getseters,/* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};

PyTypeObject BMesh_LoopSeq_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender BMesh LoopSeq",             /* char *tp_name; */
	sizeof( BPy_BMesh_LoopSeq ),         /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	(destructor) BMesh_LoopSeq_dealloc,						/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) BMesh_LoopSeq_compare,   /* cmpfunc tp_compare; */
	( reprfunc ) BMesh_LoopSeq_repr,     /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	&BMesh_LoopSeq_as_sequence,	/* PySequenceMethods *tp_as_sequence; */
	NULL,                    	/* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,						/* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	( getiterfunc) BMesh_LoopSeq_getIter,                       /* getiterfunc tp_iter; */
	( iternextfunc ) BMesh_LoopSeq_nextIter,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_BMesh_LoopSeq_methods,  /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	NULL,/*ADDME*/         /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};


/*****************************************************************************/
/* Python Type BMeshVert structure definition:                               */
/*****************************************************************************/
PyTypeObject BMesh_PolySeq_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender BMesh PolySeq",             /* char *tp_name; */
	sizeof( BPy_BMesh_PolySeq ),         /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	(destructor) BMesh_PolySeq_dealloc,						/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) BMesh_PolySeq_compare,   /* cmpfunc tp_compare; */
	( reprfunc ) BMesh_PolySeq_repr,     /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	&BMesh_PolySeq_as_sequence,	/* PySequenceMethods *tp_as_sequence; */
	NULL,                    	/* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,						/* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	( getiterfunc) BMesh_PolySeq_getIter,                       /* getiterfunc tp_iter; */
	( iternextfunc ) BMesh_PolySeq_nextIter,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_BMesh_PolySeq_methods,   /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_BMesh_PolySeq_getseters,/* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};


/*----------------------------------------------------------------------------*/

/*
 * Initialize the module and types
 */
PyObject *BMesh_Init( void )
{
	PyObject *submodule;
	if( PyType_Ready( &BMesh_Type ) < 0 )			return NULL;
	if( PyType_Ready( &BMesh_Vert_Type) < 0 )		return NULL;
	if( PyType_Ready( &BMesh_Edge_Type) < 0 )		return NULL;
	if( PyType_Ready( &BMesh_Loop_Type) < 0 )		return NULL;
	if( PyType_Ready( &BMesh_Poly_Type) < 0 )		return NULL;
	if( PyType_Ready( &BMesh_VertSeq_Type) < 0 )	return NULL;
	if( PyType_Ready( &BMesh_EdgeSeq_Type) < 0 )	return NULL;
	if( PyType_Ready( &BMesh_LoopSeq_Type) < 0 )	return NULL;
	if( PyType_Ready( &BMesh_PolySeq_Type) < 0 )	return NULL;
	
	submodule = Py_InitModule3( "Blender.BMesh", M_BMesh_methods,
				 "The Blender BMesh module\n\n\
This module provides access to **BMesh Data** in Blender.\n" );

	/*Add SUBMODULES to the module*/
	/*PyDict_SetItemString(dict, "Constraint", Constraint_Init()); //creates a *new* module*/
	return submodule;
}
