/* 
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender, partially based on NMesh.c API.
 *
 * Contributor(s): Ken Hughes
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "Mesh.h" /*This must come first*/

#include "MEM_guardedalloc.h"

#include "DNA_key_types.h"
#include "DNA_armature_types.h"
#include "DNA_scene_types.h"
#include "DNA_oops_types.h"
#include "DNA_space_types.h"
#include "DNA_curve_types.h"

#include "BDR_editface.h"	/* make_tfaces */
#include "BDR_vpaint.h"
#include "BDR_editobject.h"

#include "BIF_editdeform.h"
#include "BIF_editkey.h"	/* insert_meshkey */
#include "BIF_editview.h"

#include "BKE_deform.h"
#include "BKE_mesh.h"
#include "BKE_material.h"
#include "BKE_main.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_displist.h"
#include "BKE_DerivedMesh.h"
#include "BKE_object.h"
#include "BKE_mball.h"
#include "BKE_utildefines.h"
#include "BKE_depsgraph.h"
#include "BSE_edit.h"		/* for void countall(); */

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "blendef.h"
#include "mydevice.h"
#include "Object.h"
#include "Key.h"
#include "Image.h"
#include "Material.h"
#include "Mathutils.h"
#include "constant.h"
#include "gen_utils.h"

/* EXPP Mesh defines */

#define MESH_SMOOTHRESH               30
#define MESH_SMOOTHRESH_MIN            1
#define MESH_SMOOTHRESH_MAX           80
#define MESH_SUBDIV                    1
#define MESH_SUBDIV_MIN                0
#define MESH_SUBDIV_MAX                6

#define MESH_HASFACEUV                 0
#define MESH_HASMCOL                   1
#define MESH_HASVERTUV                 2

/************************************************************************
 *
 * internal utilities
 *
 ************************************************************************/

/*
 * internal structures used for sorting edges and faces
 */

typedef struct SrchEdges {
	unsigned int v[2];		/* indices for verts */
	unsigned char swap;		/* non-zero if verts swapped */
#if 0
	unsigned int index;		/* index in original param list of this edge */
							/* (will be used by findEdges) */
#endif
} SrchEdges;

typedef struct SrchFaces {
	unsigned int v[4];		/* indices for verts */
	unsigned char order;	/* order of original verts, bitpacked */
} SrchFaces;

/*
 * compare edges by vertex indices
 */

int medge_comp( const void *va, const void *vb )
{
	const unsigned int *a = ((SrchEdges *)va)->v;
	const unsigned int *b = ((SrchEdges *)vb)->v;

	/* compare first index for differences */

	if (a[0] < b[0]) return -1;	
	else if (a[0] > b[0]) return 1;

	/* if first indices equal, compare second index for differences */

	else if (a[1] < b[1]) return -1;
	else return (a[1] > b[1]);
}

/*
 * compare faces by vertex indices
 */

int mface_comp( const void *va, const void *vb )
{
	const SrchFaces *a = va;
	const SrchFaces *b = vb;
	int i;

	/* compare indices, first to last, for differences */
	for( i = 0; i < 4; ++i ) {
		if( a->v[i] < b->v[i] )
			return -1;	
		if( a->v[i] > b->v[i] )
			return 1;
	}

	/*
	 * don't think this needs be done; if order is different then either
	 * (a) the face is good, just reversed or has a different starting
	 * vertex, or (b) face is bad (for 4 verts) and there's a "twist"
	 */

#if 0
	/* if all the same verts, compare their order */
	if( a->order < b->order )
		return -1;	
	if( a->order > b->order )
		return 1;	
#endif

	return 0;
}

/*
 * update the DAG for all objects linked to this mesh
 */

static void mesh_update( Mesh * mesh )
{
	Object_updateDag( (void*) mesh );
}

#ifdef CHECK_DVERTS /* not clear if this code is needed */

/*
 * if verts have been added or deleted, fix dverts also
 */

static void check_dverts(Mesh *me, int old_totvert)
{
	int totvert = me->totvert;

	/* if all verts have been deleted, free old dverts */
	if (totvert == 0) free_dverts(me->dvert, old_totvert);
	/* if verts have been added, expand me->dvert */
	else if (totvert > old_totvert) {
		MDeformVert *mdv = me->dvert;
		me->dvert = NULL;
		create_dverts(me);
		copy_dverts(me->dvert, mdv, old_totvert);
		free_dverts(mdv, old_totvert);
	}
	/* if verts have been deleted, shrink me->dvert */
	else {
		MDeformVert *mdv = me->dvert;
		me->dvert = NULL;
		create_dverts(me);
		copy_dverts(me->dvert, mdv, totvert);
		free_dverts(mdv, old_totvert);
	}

	return;
}
#endif


/************************************************************************
 *
 * Color attributes
 *
 ************************************************************************/

/*
 * get a color attribute
 */

static PyObject *MCol_getAttr( BPy_MCol * self, void *type )
{
	unsigned char param;
	PyObject *attr;

	switch( (int)type ) {
    case 'R':	/* these are backwards, but that how it works */
		param = self->color->b;
		break;
    case 'G':
		param = self->color->g;
		break;
    case 'B':	/* these are backwards, but that how it works */
		param = self->color->r;
		break;
    case 'A':
		param = self->color->a;
		break;
	default:
		{
			char errstr[1024];
			sprintf( errstr, "undefined type '%d' in MCol_getAttr", (int)type );
			return EXPP_ReturnPyObjError( PyExc_RuntimeError, errstr );
		}
	}

	attr = PyInt_FromLong( param );
	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"PyInt_FromLong() failed"); 
}

/*
 * set a color attribute
 */

static int MCol_setAttr( BPy_MCol * self, PyObject * value, void * type )
{
	unsigned char *param;

	switch( (int)type ) {
    case 'R':	/* these are backwards, but that how it works */
		param = &self->color->b;
		break;
    case 'G':
		param = &self->color->g;
		break;
    case 'B':	/* these are backwards, but that how it works */
		param = &self->color->r;
		break;
    case 'A':
		param = &self->color->a;
		break;
	default:
		{
			char errstr[1024];
			sprintf( errstr, "undefined type '%d' in MCol_setAttr", (int)type );
			return EXPP_ReturnIntError( PyExc_RuntimeError, errstr );
		}
	}

	return EXPP_setIValueClamped( value, param, 0, 255, 'b' );
}

/************************************************************************
 *
 * Python MCol_Type attributes get/set structure
 *
 ************************************************************************/

static PyGetSetDef BPy_MCol_getseters[] = {
	{"r",
	 (getter)MCol_getAttr, (setter)MCol_setAttr,
	 "red component",
	 (void *)'R'},
	{"g",
	 (getter)MCol_getAttr, (setter)MCol_setAttr,
	 "green component",
	 (void *)'G'},
	{"b",
	 (getter)MCol_getAttr, (setter)MCol_setAttr,
	 "blue component",
	 (void *)'B'},
	{"a",
	 (getter)MCol_getAttr, (setter)MCol_setAttr,
	 "alpha component",
	 (void *)'A'},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/************************************************************************
 *
 * Python MCol_Type methods
 *
 ************************************************************************/

static void MCol_dealloc( BPy_MCol * self )
{
	PyObject_DEL( self );
}

static PyObject *MCol_repr( BPy_MCol * self )
{
	return PyString_FromFormat( "[MCol %d %d %d %d]",
			(int)self->color->r, (int)self->color->g, 
			(int)self->color->b, (int)self->color->a ); 
}

/************************************************************************
 *
 * Python MCol_Type structure definition
 *
 ************************************************************************/

PyTypeObject MCol_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender MCol",           /* char *tp_name; */
	sizeof( BPy_MCol ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) MCol_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	( reprfunc ) MCol_repr,     /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,	        			/* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
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
	NULL,                       /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_MCol_getseters,       /* struct PyGetSetDef *tp_getset; */
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

static PyObject *MCol_CreatePyObject( MCol * color )
{
	BPy_MCol *obj = PyObject_NEW( BPy_MCol, &MCol_Type );

	if( !obj )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"PyObject_New() failed" );

	obj->color = color;
	return (PyObject *)obj;
}

/************************************************************************
 *
 * Vertex attributes
 *
 ************************************************************************/

/*
 * get a vertex's coordinate
 */

static PyObject *MVert_getCoord( BPy_MVert * self )
{
	struct MVert *v = &self->mesh->mvert[self->index];
	return newVectorObject( v->co, 3, Py_WRAP );
}

/*
 * set a vertex's coordinate
 */

static int MVert_setCoord( BPy_MVert * self, VectorObject * value )
{
	struct MVert *v = &self->mesh->mvert[self->index];
	int i;

	if( !VectorObject_Check( value ) || value->size != 3 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected vector argument of size 3" );

	for( i=0; i<3 ; ++i)
		v->co[i] = value->vec[i];

	return 0;
}

/*
 * get a vertex's index
 */

static PyObject *MVert_getIndex( BPy_MVert * self )
{
	PyObject *attr = PyInt_FromLong( self->index );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"PyInt_FromLong() failed" );
}

/*
 * get a vertex's normal
 */

static PyObject *MVert_getNormal( BPy_MVert * self )
{
	struct MVert *v = &self->mesh->mvert[self->index];
	float no[3];
	int i;

	for( i=0; i<3; ++i )
		no[i] = (float)(v->no[i] / 32767.0);
	return newVectorObject( no, 3, Py_NEW );
}

/*
 * get a vertex's select status
 */

static PyObject *MVert_getSel( BPy_MVert *self )
{
	struct MVert *v = &self->mesh->mvert[self->index];
	return EXPP_getBitfield( &v->flag, SELECT, 'b' );
}

/*
 * set a vertex's select status
 */

static int MVert_setSel( BPy_MVert *self, PyObject *value )
{
	struct MVert *v = &self->mesh->mvert[self->index];
	return EXPP_setBitfield( value, &v->flag, SELECT, 'b' );
}

/*
 * get a vertex's UV coordinates
 */

static PyObject *MVert_getUVco( BPy_MVert *self )
{
	if( !self->mesh->msticky )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
				"mesh has no 'sticky' coordinates" );

	return newVectorObject( self->mesh->msticky[self->index].co, 2, Py_WRAP );
}

/*
 * set a vertex's UV coordinates
 */

static int MVert_setUVco( BPy_MVert *self, PyObject *value )
{
	float uvco[3] = {0.0, 0.0};
	struct MSticky *v;
	int i;

	/* 
	 * at least for now, don't allow creation of sticky coordinates if they
	 * don't already exist
	 */

	if( !self->mesh->msticky )
		return EXPP_ReturnIntError( PyExc_AttributeError,
				"mesh has no 'sticky' coordinates" );

	if( VectorObject_Check( value ) ) {
		VectorObject *vect = (VectorObject *)value;
		if( vect->size != 2 )
			return EXPP_ReturnIntError( PyExc_AttributeError,
					"expected 2D vector" );
		for( i = 0; i < vect->size; ++i )
			uvco[i] = vect->vec[i];
	} else if( !PyArg_ParseTuple( value, "ff",
				&uvco[0], &uvco[1] ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected 2D vector" );

	v = &self->mesh->msticky[self->index];

	for( i = 0; i < 2; ++i )
		v->co[i] = uvco[i];

	return 0;
}

/************************************************************************
 *
 * Python MVert_Type attributes get/set structure
 *
 ************************************************************************/

static PyGetSetDef BPy_MVert_getseters[] = {
	{"co",
	 (getter)MVert_getCoord, (setter)MVert_setCoord,
	 "vertex's coordinate",
	 NULL},
	{"index",
	 (getter)MVert_getIndex, (setter)NULL,
	 "vertex's index",
	 NULL},
	{"no",
	 (getter)MVert_getNormal, (setter)NULL,
	 "vertex's normal",
	 NULL},
	{"sel",
	 (getter)MVert_getSel, (setter)MVert_setSel,
	 "vertex's select status",
	 NULL},
	{"uvco",
	 (getter)MVert_getUVco, (setter)MVert_setUVco,
	 "vertex's UV coordinates",
	 NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/************************************************************************
 *
 * Python MVert_Type standard operations
 *
 ************************************************************************/

static void MVert_dealloc( BPy_MVert * self )
{
	PyObject_DEL( self );
}

static int MVert_compare( BPy_MVert * a, BPy_MVert * b )
{
	return( a->mesh == b->mesh && a->index == b->index ) ? 0 : -1;
}

static PyObject *MVert_repr( BPy_MVert * self )
{
	struct MVert *v = &self->mesh->mvert[self->index];
	char format[512];

	sprintf( format, "[MVert (%f %f %f) (%f %f %f) %d]",
			v->co[0], v->co[1], v->co[2], (float)(v->no[0] / 32767.0),
			(float)(v->no[1] / 32767.0), (float)(v->no[2] / 32767.0),
			self->index );

	return PyString_FromString( format );
}

/************************************************************************
 *
 * Python MVert_Type structure definition
 *
 ************************************************************************/

PyTypeObject MVert_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender MVert",           /* char *tp_name; */
	sizeof( BPy_MVert ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) MVert_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) MVert_compare,  /* cmpfunc tp_compare; */
	( reprfunc ) MVert_repr,    /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,	        			/* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
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
	NULL,                       /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_MVert_getseters,        /* struct PyGetSetDef *tp_getset; */
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

static PyObject *MVert_CreatePyObject( Mesh * mesh, int i )
{
	BPy_MVert *obj = PyObject_NEW( BPy_MVert, &MVert_Type );

	if( !obj )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"PyObject_New() failed" );

	obj->mesh = mesh;
	obj->index = i;
	return (PyObject *)obj;
}

/************************************************************************
 *
 * Vertex sequence 
 *
 ************************************************************************/

static int MVertSeq_len( BPy_MVertSeq * self )
{
	return self->mesh->totvert;
}

static PyObject *MVertSeq_item( BPy_MVertSeq * self, int i )
{
	if( i < 0 || i >= self->mesh->totvert )
		return EXPP_ReturnPyObjError( PyExc_IndexError,
					      "array index out of range" );

	return MVert_CreatePyObject( self->mesh, i );
};

static PySequenceMethods MVertSeq_as_sequence = {
	( inquiry ) MVertSeq_len,	/* sq_length */
	( binaryfunc ) 0,	/* sq_concat */
	( intargfunc ) 0,	/* sq_repeat */
	( intargfunc ) MVertSeq_item,	/* sq_item */
	( intintargfunc ) 0,	/* sq_slice */
	( intobjargproc ) 0,	/* sq_ass_item */
	( intintobjargproc ) 0,	/* sq_ass_slice */
	0,0,0,
};

/************************************************************************
 *
 * Python MVertSeq_Type iterator (iterates over vertices)
 *
 ************************************************************************/

/*
 * Initialize the interator index
 */

static PyObject *MVertSeq_getIter( BPy_MVertSeq * self )
{
	self->iter = 0;
	return EXPP_incr_ret ( (PyObject *) self );
}

/*
 * Return next MVert.
 */

static PyObject *MVertSeq_nextIter( BPy_MVertSeq * self )
{
	if( self->iter == self->mesh->totvert )
		return EXPP_ReturnPyObjError( PyExc_StopIteration,
				"iterator at end" );

	return MVert_CreatePyObject( self->mesh, self->iter++ );
}

/************************************************************************
 *
 * Python MVertSeq_Type methods
 *
 ************************************************************************/

static PyObject *MVertSeq_extend( BPy_MVertSeq * self, PyObject *args )
{
	int len;
	int i,j;
	PyObject *tmp;
	MVert *newvert, *tmpvert;
	Mesh *mesh = self->mesh;

	/* make sure we get a sequence of tuples of something */

	switch( PySequence_Size ( args ) ) {
	case 1:		/* better be a list or a tuple */
		args = PyTuple_GET_ITEM( args, 0 );
		if( !PySequence_Check ( args ) )
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a sequence of tuple triplets" );
		Py_INCREF( args );		/* so we can safely DECREF later */
		break;
	case 3:		/* take any three args and put into a tuple */
		tmp = PyTuple_GET_ITEM( args, 0 );
		if( PyTuple_Check( tmp ) ) {
			Py_INCREF( args );
			break;
		}
		args = Py_BuildValue( "((OOO))", tmp,
				PyTuple_GET_ITEM( args, 1 ), PyTuple_GET_ITEM( args, 2 ) );
		if( !args )
			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"Py_BuildValue() failed" );
		break;
	default:	/* anything else is definitely wrong */
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a sequence of tuple triplets" );
	}

	len = PySequence_Size( args );
	if( len == 0 ) {
		Py_DECREF ( args );
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"expected at least one tuple" );
	}

	newvert = MEM_callocN( sizeof( MVert ) * (mesh->totvert+len), "MVerts" );

	/* scan the input list and insert the new vertices */

	tmpvert = &newvert[mesh->totvert];
	for( i = 0; i < len; ++i ) {
		float co[3];
		tmp = PySequence_Fast_GET_ITEM( args, i );
		if( VectorObject_Check( tmp ) ) {
			if( ((VectorObject *)tmp)->size != 3 ) {
				MEM_freeN( newvert );
				Py_DECREF ( args );
				return EXPP_ReturnPyObjError( PyExc_ValueError,
					"expected vector of size 3" );
			}
			for( j = 0; j < 3; ++j )
				co[j] = ((VectorObject *)tmp)->vec[j];
		} else if( PyTuple_Check( tmp ) ) {
			int ok=1;
			PyObject *flt;
			if( PyTuple_Size( tmp ) != 3 )
				ok = 0;
			else	
				for( j = 0; ok && j < 3; ++j ) {
					flt = PyTuple_GET_ITEM( tmp, j );
					if( !PyNumber_Check ( flt ) )
						ok = 0;
					else
						co[j] = (float)PyFloat_AsDouble( flt );
				}

			if( !ok ) {
				MEM_freeN( newvert );
				Py_DECREF ( args );
				return EXPP_ReturnPyObjError( PyExc_ValueError,
					"expected tuple triplet of floats" );
			}
		}

	/* add the coordinate to the new list */
#if 0
		memcpy( tmpvert->co, co, sizeof(float)*3 );
#else
		{
			int i=3;
			while (i--) tmpvert->co[i] = co[i];
		}
#endif



	/* TODO: anything else which needs to be done when we add a vert? */
	/* probably not: NMesh's newvert() doesn't */
		++tmpvert;
	}

	/* if we got here we've added all the new verts, so just copy the old
	 * verts over and we're done */

	if( mesh->mvert ) {
		memcpy( newvert, mesh->mvert, mesh->totvert*sizeof(MVert) );
		MEM_freeN( mesh->mvert );
	}
	mesh->mvert = newvert;
	mesh->totvert += len;

#ifdef CHECK_DVERTS
	check_dverts( mesh, mesh->totvert - len );
#endif
	mesh_update( mesh );

	Py_DECREF ( args );
	return EXPP_incr_ret( Py_None );
}

static struct PyMethodDef BPy_MVertSeq_methods[] = {
	{"extend", (PyCFunction)MVertSeq_extend, METH_VARARGS,
		"add edges to mesh"},
	{NULL, NULL, 0, NULL}
};

/************************************************************************
 *
 * Python MVertSeq_Type standard operations
 *
 ************************************************************************/

static void MVertSeq_dealloc( BPy_MVertSeq * self )
{
	PyObject_DEL( self );
}

/*****************************************************************************/
/* Python NMVertSeq_Type structure definition:                               */
/*****************************************************************************/
PyTypeObject MVertSeq_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender MVertSeq",           /* char *tp_name; */
	sizeof( BPy_MVertSeq ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) MVertSeq_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	NULL,                       /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	&MVertSeq_as_sequence,	    /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
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
	( getiterfunc) MVertSeq_getIter, /* getiterfunc tp_iter; */
	( iternextfunc ) MVertSeq_nextIter, /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_MVertSeq_methods,       /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	NULL,                       /* struct PyGetSetDef *tp_getset; */
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

/************************************************************************
 *
 * Edge attributes
 *
 ************************************************************************/

/*
 * get an edge's crease value
 */

static PyObject *MEdge_getCrease( BPy_MEdge * self )
{
	struct MEdge *edge = &self->mesh->medge[self->index];
	PyObject *attr = PyInt_FromLong( edge->crease );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"PyInt_FromLong() failed" );
}

/*
 * set an edge's crease value
 */

static int MEdge_setCrease( BPy_MEdge * self, PyObject * value )
{
	struct MEdge *edge = &self->mesh->medge[self->index];
	return EXPP_setIValueClamped( value, &edge->crease, 0, 255, 'b' );
}

/*
 * get an edge's flag
 */

static PyObject *MEdge_getFlag( BPy_MEdge * self )
{
	struct MEdge *edge = &self->mesh->medge[self->index];
	PyObject *attr = PyInt_FromLong( edge->flag );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"PyInt_FromLong() failed" );
}

/*
 * set an edge's flag
 */

static int MEdge_setFlag( BPy_MEdge * self, PyObject * value )
{
	struct MEdge *edge = &self->mesh->medge[self->index];
	short param;
	static short bitmask = 1 		/* 1=select */
				| ME_EDGEDRAW
				| ME_EDGERENDER
				| ME_SEAM
				| ME_FGON;

	if( !PyInt_CheckExact ( value ) ) {
		char errstr[128];
		sprintf ( errstr , "expected int bitmask of 0x%04x", bitmask );
		return EXPP_ReturnIntError( PyExc_TypeError, errstr );
	}
	param = (short)PyInt_AS_LONG ( value );

	if ( ( param & bitmask ) != param )
		return EXPP_ReturnIntError( PyExc_ValueError,
						"invalid bit(s) set in mask" );

	edge->flag = param;

	return 0;
}

/*
 * get an edge's first vertex
 */

static PyObject *MEdge_getV1( BPy_MEdge * self )
{
	struct MEdge *edge = &self->mesh->medge[self->index];
	return MVert_CreatePyObject( self->mesh, edge->v1 );
}

/*
 * set an edge's first vertex
 */

static int MEdge_setV1( BPy_MEdge * self, BPy_MVert * value )
{
	struct MEdge *edge = &self->mesh->medge[self->index];
	edge->v1 = value->index;
	return 0;
}

/*
 * get an edge's second vertex
 */

static PyObject *MEdge_getV2( BPy_MEdge * self )
{
	struct MEdge *edge = &self->mesh->medge[self->index];
	return MVert_CreatePyObject( self->mesh, edge->v2 );
}

/*
 * set an edge's second vertex
 */

static int MEdge_setV2( BPy_MEdge * self, BPy_MVert * value )
{
	struct MEdge *edge = &self->mesh->medge[self->index];
	edge->v2 = value->index;
	return 0;
}

/*
 * get an edges's index
 */

static PyObject *MEdge_getIndex( BPy_MEdge * self )
{
	PyObject *attr = PyInt_FromLong( self->index );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"PyInt_FromLong() failed" );
}

/************************************************************************
 *
 * Python MEdge_Type attributes get/set structure
 *
 ************************************************************************/

static PyGetSetDef BPy_MEdge_getseters[] = {
	{"crease",
	 (getter)MEdge_getCrease, (setter)MEdge_setCrease,
	 "edge's crease value",
	 NULL},
	{"flag",
	 (getter)MEdge_getFlag, (setter)MEdge_setFlag,
	 "edge's flags",
	 NULL},
	{"v1",
	 (getter)MEdge_getV1, (setter)MEdge_setV1,
	 "edge's first vertex",
	 NULL},
	{"v2",
	 (getter)MEdge_getV2, (setter)MEdge_setV2,
	 "edge's second vertex",
	 NULL},
	{"index",
	 (getter)MEdge_getIndex, (setter)NULL,
	 "edge's index",
	 NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/************************************************************************
 *
 * Python MEdge_Type iterator (iterates over vertices)
 *
 ************************************************************************/

/*
 * Initialize the interator index
 */

static PyObject *MEdge_getIter( BPy_MEdge * self )
{
	self->iter = 0;
	return EXPP_incr_ret ( (PyObject *) self );
}

/*
 * Return next MVert.  Throw an exception after the second vertex.
 */

static PyObject *MEdge_nextIter( BPy_MEdge * self )
{
	if( self->iter == 2 )
		return EXPP_ReturnPyObjError( PyExc_StopIteration,
				"iterator at end" );

	self->iter++;
	if( self->iter == 1 )
		return MEdge_getV1( self );
	else
		return MEdge_getV2( self );
}

/************************************************************************
 *
 * Python MEdge_Type standard operations
 *
 ************************************************************************/

static void MEdge_dealloc( BPy_MEdge * self )
{
	PyObject_DEL( self );
}

static int MEdge_compare( BPy_MEdge * a, BPy_MEdge * b )
{
	return( a->mesh == b->mesh && a->index == b->index ) ? 0 : -1;
}

static PyObject *MEdge_repr( BPy_MEdge * self )
{
	struct MEdge *edge = &self->mesh->medge[self->index];

	return PyString_FromFormat( "[MEdge (%d %d) %d %d]",
			(int)edge->v1, (int)edge->v2, (int)edge->crease,
			(int)self->index );
}

/************************************************************************
 *
 * Python MEdge_Type structure definition
 *
 ************************************************************************/

PyTypeObject MEdge_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender MEdge",           /* char *tp_name; */
	sizeof( BPy_MEdge ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) MEdge_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) MEdge_compare,  /* cmpfunc tp_compare; */
	( reprfunc ) MEdge_repr,    /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
    NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
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
	( getiterfunc) MEdge_getIter, /* getiterfunc tp_iter; */
	( iternextfunc ) MEdge_nextIter, /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	NULL,                       /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_MEdge_getseters,        /* struct PyGetSetDef *tp_getset; */
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

static PyObject *MEdge_CreatePyObject( Mesh * mesh, int i )
{
	BPy_MEdge *obj = PyObject_NEW( BPy_MEdge, &MEdge_Type );

	if( !obj )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"PyObject_New() failed" );

	obj->mesh = mesh;
	obj->index = i;
	return (PyObject *)obj;
}

/************************************************************************
 *
 * Edge sequence 
 *
 ************************************************************************/

static int MEdgeSeq_len( BPy_MEdgeSeq * self )
{
	return self->mesh->totedge;
}

static PyObject *MEdgeSeq_item( BPy_MEdgeSeq * self, int i )
{
	if( i < 0 || i >= self->mesh->totedge )
		return EXPP_ReturnPyObjError( PyExc_IndexError,
					      "array index out of range" );

	return MEdge_CreatePyObject( self->mesh, i );
}

static PySequenceMethods MEdgeSeq_as_sequence = {
	( inquiry ) MEdgeSeq_len,	/* sq_length */
	( binaryfunc ) 0,	/* sq_concat */
	( intargfunc ) 0,	/* sq_repeat */
	( intargfunc ) MEdgeSeq_item,	/* sq_item */
	( intintargfunc ) 0,	/* sq_slice */
	( intobjargproc ) 0,	/* sq_ass_item */
	( intintobjargproc ) 0,	/* sq_ass_slice */
	0,0,0,
};

/************************************************************************
 *
 * Python MEdgeSeq_Type iterator (iterates over edges)
 *
 ************************************************************************/

/*
 * Initialize the interator index
 */

static PyObject *MEdgeSeq_getIter( BPy_MEdgeSeq * self )
{
	self->iter = 0;
	return EXPP_incr_ret ( (PyObject *) self );
}

/*
 * Return next MEdge.
 */

static PyObject *MEdgeSeq_nextIter( BPy_MEdgeSeq * self )
{
	if( self->iter == self->mesh->totedge )
		return EXPP_ReturnPyObjError( PyExc_StopIteration,
				"iterator at end" );

	return MEdge_CreatePyObject( self->mesh, self->iter++ );
}

/************************************************************************
 *
 * Python MEdgeSeq_Type methods
 *
 ************************************************************************/

/*
 * Create edges from tuples of vertices.  Duplicate new edges, or
 * edges which already exist,
 */

static PyObject *MEdgeSeq_extend( BPy_MEdgeSeq * self, PyObject *args )
{
	int len, nverts;
	int i, j;
	int new_edge_count, good_edges;
	SrchEdges *oldpair, *newpair, *tmppair, *tmppair2;
	PyObject *tmp;
	BPy_MVert *e[4];
	MEdge *tmpedge;
	Mesh *mesh = self->mesh;

	/* make sure we get a sequence of tuples of something */

	switch( PySequence_Size ( args ) ) {
	case 1:		/* better be a list or a tuple */
		args = PyTuple_GET_ITEM( args, 0 );
		if( !PySequence_Check ( args ) )
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a sequence of tuple pairs" );
		Py_INCREF( args );		/* so we can safely DECREF later */
		break;
	case 2:	
	case 3:
	case 4:		/* two to four args may be individual verts */
		tmp = PyTuple_GET_ITEM( args, 0 );
		if( PyTuple_Check( tmp ) ) {/* maybe just tuples, so use args as-is */
			Py_INCREF( args );		/* so we can safely DECREF later */
			break;
		}
		args = Py_BuildValue( "(O)", args );
		if( !args )
			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"Py_BuildValue() failed" );
		break;
	default:	/* anything else is definitely wrong */
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a sequence of tuple pairs" );
	}

	/* make sure there is something to add */
	len = PySequence_Size( args );
	if( len == 0 ) {
		Py_DECREF( args );
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"expected at least one tuple" );
	}

	/* verify the param list and get a total count of number of edges */
	new_edge_count = 0;
	for( i = 0; i < len; ++i ) {
		tmp = PySequence_Fast_GET_ITEM( args, i );

		/* not a tuple of MVerts... error */
		if( !PyTuple_Check( tmp ) ||
				EXPP_check_sequence_consistency( tmp, &MVert_Type ) != 1 ) {
			Py_DECREF( args );
			return EXPP_ReturnPyObjError( PyExc_ValueError,
				"expected sequence of MVert tuples" );
		}

		/* not the right number of MVerts... error */
		nverts = PyTuple_Size( tmp );
		if( nverts < 2 || nverts > 4 ) {
			Py_DECREF( args );
			return EXPP_ReturnPyObjError( PyExc_ValueError,
				"expected 2 to 4 MVerts per tuple" );
		}
		if( nverts == 2 )
			++new_edge_count;	/* if only two vert, then add only edge */
		else
			new_edge_count += nverts;	/* otherwise, one edge per vert */
	}

	/* OK, commit to allocating the search structures */
	newpair = (SrchEdges *)MEM_callocN( sizeof(SrchEdges)*new_edge_count,
			"MEdgePairs" );

	/* scan the input list and build the new edge pair list */
	len = PySequence_Size( args );
	tmppair = newpair;
	for( i = 0; i < len; ++i ) {
		tmp = PySequence_Fast_GET_ITEM( args, i );
		nverts = PyTuple_Size( tmp );

		/* get copies of vertices */
		for(j = 0; j < nverts; ++j )
			e[j] = (BPy_MVert *)PyTuple_GET_ITEM( tmp, j );

		if( nverts == 2 )
			nverts = 1;	/* again, two verts give just one edge */

		/* now add the edges to the search list */
		for(j = 0; j < nverts; ++j ) {
			int k = j+1;
			if( k == nverts )	/* final edge */ 
				k = 0;

			/* sort verts into search list, abort if two are the same */
			if( e[j]->index < e[k]->index ) {
				tmppair->v[0] = e[j]->index;
				tmppair->v[1] = e[k]->index;
				tmppair->swap = 0;
			} else if( e[j]->index > e[k]->index ) {
				tmppair->v[0] = e[k]->index;
				tmppair->v[1] = e[j]->index;
				tmppair->swap = 1;
			} else {
				MEM_freeN( newpair );
				Py_DECREF( args );
				return EXPP_ReturnPyObjError( PyExc_ValueError,
						"tuple contains duplicate vertices" );
			}
			tmppair++;
		}
	}

	/* sort the new edge pairs */
	qsort( newpair, new_edge_count, sizeof(SrchEdges), medge_comp );

	/*
	 * find duplicates in the new list and mark.  if it's a duplicate,
	 * then mark by setting second vert index to 0 (a real edge won't have
	 * second vert index of 0 since verts are sorted)
	 */

	good_edges = new_edge_count;	/* all edges are good at this point */

	tmppair = newpair;		/* "last good edge" */
	tmppair2 = &tmppair[1];	/* "current candidate edge" */
	for( i = 0; i < new_edge_count; ++i ) {
		if( tmppair->v[0] != tmppair2->v[0] ||
				tmppair->v[1] != tmppair2->v[1] )
			tmppair = tmppair2;	/* last != current, so current == last */
		else {
			tmppair2->v[1] = 0; /* last == current, so mark as duplicate */
			--good_edges;		/* one less good edge */
		}
		tmppair2++;
	}

	/* if mesh has edges, see if any of the new edges are already in it */
	if( mesh->totedge ) {
		oldpair = (SrchEdges *)MEM_callocN( sizeof(SrchEdges)*mesh->totedge,
				"MEdgePairs" );

		/*
		 * build a search list of new edges (don't need to update "swap"
		 * field, since we're not creating edges here)
		 */
		tmppair = oldpair;
		tmpedge = mesh->medge;
		for( i = 0; i < mesh->totedge; ++i ) {
			if( tmpedge->v1 < tmpedge->v2 ) {
				tmppair->v[0] = tmpedge->v1;
				tmppair->v[1] = tmpedge->v2;
			} else {
				tmppair->v[0] = tmpedge->v2;
				tmppair->v[1] = tmpedge->v1;
			}
			++tmpedge;
			++tmppair;
		}

	/* sort the old edge pairs */
		qsort( oldpair, mesh->totedge, sizeof(SrchEdges), medge_comp );

	/* eliminate new edges already in the mesh */
		tmppair = newpair;
		for( i = new_edge_count; i-- ; ) {
			if( tmppair->v[1] ) {
				if( bsearch( tmppair, oldpair, mesh->totedge,
							sizeof(SrchEdges), medge_comp ) ) {
					tmppair->v[1] = 0;	/* mark as duplicate */
					--good_edges;
				} 
			}
			tmppair++;
		}
		MEM_freeN( oldpair );
	}

	/* if any new edges are left, add to list */
	if( good_edges ) {
		int totedge = mesh->totedge+good_edges;	/* new edge count */

	/* allocate new edge list */
		tmpedge = MEM_callocN(totedge*sizeof(MEdge), "NMesh_addEdges");

	/* if we're appending, copy the old edge list and delete it */
		if( mesh->medge ) {
			memcpy( tmpedge, mesh->medge, mesh->totedge*sizeof(MEdge));
			MEM_freeN( mesh->medge );
		}
		mesh->medge = tmpedge;		/* point to the new edge list */

	/* point to the first edge we're going to add */
		tmpedge = &mesh->medge[mesh->totedge];
		tmppair = newpair;

	/* as we find a good edge, add it */
		while( good_edges ) {
			if( tmppair->v[1] ) {	/* not marked as duplicate ! */
				if( !tmppair->swap ) {
					tmpedge->v1 = tmppair->v[0];
					tmpedge->v2 = tmppair->v[1];
				} else {
					tmpedge->v1 = tmppair->v[1];
					tmpedge->v2 = tmppair->v[0];
				}
				tmpedge->flag = ME_EDGEDRAW | ME_EDGERENDER;
				mesh->totedge++;
				--good_edges;
				++tmpedge;
			}
			tmppair++;
		}
	}

	/* clean up and leave */
	mesh_update( mesh );
	MEM_freeN( newpair );
	Py_DECREF ( args );
	return EXPP_incr_ret( Py_None );
}

static struct PyMethodDef BPy_MEdgeSeq_methods[] = {
	{"extend", (PyCFunction)MEdgeSeq_extend, METH_VARARGS,
		"add edges to mesh"},
	{NULL, NULL, 0, NULL}
};

/************************************************************************
 *
 * Python MEdgeSeq_Type standard operators
 *
 ************************************************************************/

static void MEdgeSeq_dealloc( BPy_MEdgeSeq * self )
{
	PyObject_DEL( self );
}

/*****************************************************************************/
/* Python NMEdgeSeq_Type structure definition:                               */
/*****************************************************************************/
PyTypeObject MEdgeSeq_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender MEdgeSeq",           /* char *tp_name; */
	sizeof( BPy_MEdgeSeq ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) MEdgeSeq_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	NULL,                       /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	&MEdgeSeq_as_sequence,	    /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
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
	( getiterfunc) MEdgeSeq_getIter, /* getiterfunc tp_iter; */
	( iternextfunc ) MEdgeSeq_nextIter, /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_MEdgeSeq_methods,       /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	NULL,                       /* struct PyGetSetDef *tp_getset; */
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

/************************************************************************
 *
 * Face attributes
 *
 ************************************************************************/

/*
 * get a face's vertices
 */

static PyObject *MFace_getVerts( BPy_MFace * self )
{
	struct MFace *face = &self->mesh->mface[self->index];
	PyObject *attr = PyTuple_New( face->v4 ? 4 : 3 );

	if( !attr )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"PyTuple_New() failed" );

	PyTuple_SetItem( attr, 0, MVert_CreatePyObject( self->mesh, face->v1 ) );
	PyTuple_SetItem( attr, 1, MVert_CreatePyObject( self->mesh, face->v2 ) );
	PyTuple_SetItem( attr, 2, MVert_CreatePyObject( self->mesh, face->v3 ) );
	if( face->v4 )
		PyTuple_SetItem( attr, 3, 
				MVert_CreatePyObject( self->mesh, face->v4 ) );

	return attr;
}

/*
 * set a face's vertices
 */

static int MFace_setVerts( BPy_MFace * self, PyObject * args )
{
	struct MFace *face = &self->mesh->mface[self->index];
	BPy_MVert *v1, *v2, *v3, *v4 = NULL;

	if( !PyArg_ParseTuple ( args, "O!O!O!|O!", &MVert_Type, &v1,
				&MVert_Type, &v2, &MVert_Type, &v3, &MVert_Type, &v4 ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
			"expected tuple of 3 or 4 MVerts" );

	face->v1 = v1->index;
	face->v2 = v2->index;
	face->v3 = v3->index;
	if( v4 )
		face->v4 = v4->index;
	return 0;
}

/*
 * get face's material index
 */

static PyObject *MFace_getMat( BPy_MFace * self )
{
	struct MFace *face = &self->mesh->mface[self->index];
	PyObject *attr = PyInt_FromLong( face->mat_nr );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"PyInt_FromLong() failed" );
}

/*
 * set face's material index
 */

static int MFace_setMat( BPy_MFace * self, PyObject * value )
{
	struct MFace *face = &self->mesh->mface[self->index];
	return EXPP_setIValueRange( value, &face->mat_nr, 0, 15, 'b' );
}

/*
 * get a face's index
 */

static PyObject *MFace_getIndex( BPy_MFace * self )
{
	PyObject *attr = PyInt_FromLong( self->index );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"PyInt_FromLong() failed" );
}

/*
 * get face's normal index
 */

static PyObject *MFace_getNormal( BPy_MFace * self )
{
	struct MFace *face = &self->mesh->mface[self->index];
	float *vert[4];
	float no[3];

	vert[0] = self->mesh->mvert[face->v1].co;
	vert[1] = self->mesh->mvert[face->v2].co;
	vert[2] = self->mesh->mvert[face->v3].co;
	vert[3] = self->mesh->mvert[face->v4].co;
	if( face->v4 )
		CalcNormFloat4( vert[0], vert[1], vert[2], vert[3], no );
	else
		CalcNormFloat( vert[0], vert[1], vert[2], no );

	return newVectorObject( no, 3, Py_NEW );
}

/*
 * get one of a face's mface flag bits
 */

static PyObject *MFace_getMFlagBits( BPy_MFace * self, void * type )
{
	struct MFace *face = &self->mesh->mface[self->index];
	return EXPP_getBitfield( &face->flag, (int)type, 'b' );
}

/*
 * set one of a face's mface flag bits
 */

static int MFace_setMFlagBits( BPy_MFace * self, PyObject * value,
		void * type )
{
	struct MFace *face = &self->mesh->mface[self->index];
	return EXPP_setBitfield( value, &face->flag, (int)type, 'b' );
}

/*
 * get face's texture image
 */

static PyObject *MFace_getImage( BPy_MFace *self )
{
	TFace *face;
	if( !self->mesh->tface )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"face has no texture values" );

	face = &self->mesh->tface[self->index];

	if( face->tpage )
		return Image_CreatePyObject( face->tpage );
	else
		return EXPP_incr_ret( Py_None );
}

/*
 * change or clear face's texture image
 */

static int MFace_setImage( BPy_MFace *self, PyObject *value )
{
	TFace *face;
	if( !self->mesh->tface )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"face has no texture values" );

	face = &self->mesh->tface[self->index];
    if( value == Py_None )
        face->tpage = NULL;		/* should memory be freed? */
    else {
        if( !BPy_Image_Check( value ) )
            return EXPP_ReturnIntError( PyExc_TypeError,
					"expected image object" );
        face->tpage = ( ( BPy_Image * ) value )->image;
    }

    return 0;
}

/*
 * get face's texture mode
 */

static PyObject *MFace_getMode( BPy_MFace *self )
{
	PyObject *attr;

	if( !self->mesh->tface )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"face has no texture values" );

	attr = PyInt_FromLong( self->mesh->tface[self->index].mode );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"PyInt_FromLong() failed" );
}

/*
 * set face's texture mode
 */

static int MFace_setMode( BPy_MFace *self, PyObject *value )
{
	int param;
	static short bitmask = TF_SELECT | TF_HIDE;

	if( !self->mesh->tface )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"face has no texture values" );

	if( !PyInt_CheckExact ( value ) ) {
		char errstr[128];
		sprintf ( errstr , "expected int bitmask of 0x%04x", bitmask );
		return EXPP_ReturnIntError( PyExc_TypeError, errstr );
	}
	param = PyInt_AS_LONG ( value );

	/* only one face can be active, so don't allow that here */

	if( ( param & bitmask ) == TF_ACTIVE )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"cannot make a face active; use 'activeFace' attribute" );
	
	if( ( param & bitmask ) != param )
		return EXPP_ReturnIntError( PyExc_ValueError,
						"invalid bit(s) set in mask" );

	/* merge active setting with other new params */
	param |= (self->mesh->tface[self->index].flag & TF_ACTIVE);
	self->mesh->tface[self->index].flag = (char)param;

	return 0;
}

/*
 * get face's texture flags
 */

static PyObject *MFace_getFlag( BPy_MFace *self )
{
	PyObject *attr;

	if( !self->mesh->tface )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"face has no texture values" );

	attr = PyInt_FromLong( self->mesh->tface[self->index].mode );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"PyInt_FromLong() failed" );
}

/*
 * set face's texture flag
 */

static int MFace_setFlag( BPy_MFace *self, PyObject *value )
{
	int param;
	static short bitmask = TF_DYNAMIC
				| TF_TEX
				| TF_SHAREDVERT
				| TF_LIGHT
				| TF_SHAREDCOL
				| TF_TILES
				| TF_BILLBOARD
				| TF_TWOSIDE
				| TF_INVISIBLE
				| TF_OBCOL
				| TF_BILLBOARD2
				| TF_SHADOW
				| TF_BMFONT;

	if( !self->mesh->tface )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"face has no texture values" );

	if( !PyInt_CheckExact ( value ) ) {
		char errstr[128];
		sprintf ( errstr , "expected int bitmask of 0x%04x", bitmask );
		return EXPP_ReturnIntError( PyExc_TypeError, errstr );
	}
	param = PyInt_AS_LONG ( value );
	
	if( param == 0xffff )		/* if param is ALL, set everything but HALO */
		param = bitmask ^ TF_BILLBOARD;
	else if( ( param & bitmask ) != param )
		return EXPP_ReturnIntError( PyExc_ValueError,
						"invalid bit(s) set in mask" );

	/* Blender UI doesn't allow these on at the same time */

	if( ( param & (TF_BILLBOARD | TF_BILLBOARD2) ) == 
			(TF_BILLBOARD | TF_BILLBOARD2) )
		return EXPP_ReturnIntError( PyExc_ValueError,
						"HALO and BILLBOARD cannot be enabled simultaneously" );

	self->mesh->tface[self->index].mode = (short)param;

	return 0;
}

/*
 * get face's texture transparency setting
 */

static PyObject *MFace_getTransp( BPy_MFace *self )
{
	PyObject *attr;
	if( !self->mesh->tface )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"face has no texture values" );

	attr = PyInt_FromLong( self->mesh->tface[self->index].transp );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"PyInt_FromLong() failed" );
}

/*
 * set face's texture transparency setting
 */

static int MFace_setTransp( BPy_MFace *self, PyObject *value )
{
	if( !self->mesh->tface )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"face has no texture values" );

	return EXPP_setIValueRange( value,
			&self->mesh->tface[self->index].transp, TF_SOLID, TF_SUB, 'b' );
}

/*
 * get a face's texture UV values
 */

static PyObject *MFace_getUV( BPy_MFace * self )
{
	TFace *face;
	PyObject *attr;
	int length, i;

	if( !self->mesh->tface )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"face has no texture values" );

	face = &self->mesh->tface[self->index];
	length = self->mesh->mface[self->index].v4 ? 4 : 3;
	attr = PyTuple_New( length );

	if( !attr )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"PyTuple_New() failed" );

	for( i=0; i<length; ++i ) {
		PyObject *vector = newVectorObject( face->uv[i], 2, Py_WRAP );
		if( !vector )
			return NULL;
		PyTuple_SetItem( attr, i, vector );
	}

	return attr;
}

/*
 * set a face's texture UV values
 */

static int MFace_setUV( BPy_MFace * self, PyObject * value )
{
	TFace *face;
	int length, i;

	if( !self->mesh->tface )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"face has no texture values" );

	if( EXPP_check_sequence_consistency( value, &vector_Type ) != 1 )
		return EXPP_ReturnIntError( PyExc_TypeError,
					    "expected sequence of vectors" );

	length = self->mesh->mface[self->index].v4 ? 4 : 3;
	if( length != PyTuple_Size( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
					    "size of vertex and UV lists differ" );

	face = &self->mesh->tface[self->index];
	for( i=0; i<length; ++i ) {
		VectorObject *vector = (VectorObject *)PyTuple_GET_ITEM( value, i );
		face->uv[i][0] = vector->vec[0];
		face->uv[i][1] = vector->vec[1];
	}
	return 0;
}

/*
 * get a face's vertex colors. note that if mesh->tfaces is defined, then 
 * it takes precedent over mesh->mcol
 */

static PyObject *MFace_getCol( BPy_MFace * self )
{
	PyObject *attr;
	int length, i;
	MCol * mcol;

	/* if there's no mesh color vectors or texture faces, nothing to do */

	if( !self->mesh->mcol && !self->mesh->tface )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"face has no vertex colors" );

	if( self->mesh->tface )
		mcol = (MCol *) self->mesh->tface[self->index].col;
	else
		mcol = &self->mesh->mcol[self->index*4];

	length = self->mesh->mface[self->index].v4 ? 4 : 3;
	attr = PyTuple_New( length );

	if( !attr )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"PyTuple_New() failed" );

	for( i=0; i<length; ++i ) {
		PyObject *color = MCol_CreatePyObject( &mcol[i] );
		if( !color )
			return NULL;
		PyTuple_SetItem( attr, i, color );
	}

	return attr;
}

/************************************************************************
 *
 * Python MFace_Type attributes get/set structure
 *
 ************************************************************************/

static PyGetSetDef BPy_MFace_getseters[] = {
    {"verts",
     (getter)MFace_getVerts, (setter)MFace_setVerts,
     "face's vertices",
     NULL},
    {"v",
     (getter)MFace_getVerts, (setter)MFace_setVerts,
     "deprecated: see 'verts'",
     NULL},
    {"mat",
     (getter)MFace_getMat, (setter)MFace_setMat,
     "face's material index",
     NULL},
    {"index",
     (getter)MFace_getIndex, (setter)NULL,
     "face's index",
     NULL},
    {"no",
     (getter)MFace_getNormal, (setter)NULL,
     "face's normal",
     NULL},

    {"hide",
     (getter)MFace_getMFlagBits, (setter)MFace_setMFlagBits,
     "face hidden in edit mode",
     (void *)ME_HIDE},
    {"sel",
     (getter)MFace_getMFlagBits, (setter)MFace_setMFlagBits,
     "face selected in edit mode",
     (void *)ME_FACE_SEL},
    {"smooth",
     (getter)MFace_getMFlagBits, (setter)MFace_setMFlagBits,
     "face smooth enabled",
     (void *)ME_SMOOTH},

	/* attributes for texture faces (mostly, I think) */

    {"col",
     (getter)MFace_getCol, (setter)NULL,
     "face's vertex colors",
     NULL},
    {"flag",
     (getter)MFace_getFlag, (setter)MFace_setFlag,
     "flags associated with texture faces",
     NULL},
    {"image",
     (getter)MFace_getImage, (setter)MFace_setImage,
     "image associated with texture faces",
     NULL},
    {"mode",
     (getter)MFace_getMode, (setter)MFace_setMode,
     "modes associated with texture faces",
     NULL},
    {"transp",
     (getter)MFace_getTransp, (setter)MFace_setTransp,
     "transparency of texture faces",
     NULL},
    {"uv",
     (getter)MFace_getUV, (setter)MFace_setUV,
     "face's UV coordinates",
     NULL},

	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/************************************************************************
 *
 * Python MFace_Type iterator (iterates over vertices)
 *
 ************************************************************************/

/*
 * Initialize the interator index
 */

static PyObject *MFace_getIter( BPy_MFace * self )
{
	self->iter = 0;
	return EXPP_incr_ret ( (PyObject *) self );
}

/*
 * Return next MVert.  Throw an exception after the final vertex.
 */

static PyObject *MFace_nextIter( BPy_MFace * self )
{
	struct MFace *face = &self->mesh->mface[self->index];
	int len = self->mesh->mface[self->index].v4 ? 4 : 3;

	if( self->iter == len )
		return EXPP_ReturnPyObjError( PyExc_StopIteration,
				"iterator at end" );

	++self->iter;
	switch ( self->iter ) {
	case 1:
		return MVert_CreatePyObject( self->mesh, face->v1 );
	case 2:
		return MVert_CreatePyObject( self->mesh, face->v2 );
	case 3:
		return MVert_CreatePyObject( self->mesh, face->v3 );
	default :
		return MVert_CreatePyObject( self->mesh, face->v4 );
	}
}

/************************************************************************
 *
 * Python MFace_Type methods
 *
 ************************************************************************/

/************************************************************************
 *
 * Python MFace_Type standard operations
 *
 ************************************************************************/

static void MFace_dealloc( BPy_MFace * self )
{
	PyObject_DEL( self );
}

static int MFace_compare( BPy_MFace * a, BPy_MFace * b )
{
	return( a->mesh == b->mesh && a->index == b->index ) ? 0 : -1;
}

static PyObject *MFace_repr( BPy_MFace* self )
{
	struct MFace *face = &self->mesh->mface[self->index];

	if( face->v4 )
		return PyString_FromFormat( "[MFace (%d %d %d %d) %d]",
				(int)face->v1, (int)face->v2, 
				(int)face->v3, (int)face->v4, (int)self->index ); 
	else
		return PyString_FromFormat( "[MFace (%d %d %d) %d]",
				(int)face->v1, (int)face->v2,
				(int)face->v3, (int)self->index ); 
}

/************************************************************************
 *
 * Python MFace_Type structure definition
 *
 ************************************************************************/

PyTypeObject MFace_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender MFace",            /* char *tp_name; */
	sizeof( BPy_MFace ),        /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) MFace_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) MFace_compare,  /* cmpfunc tp_compare; */
	( reprfunc ) MFace_repr,    /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,	        			/* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
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
	( getiterfunc ) MFace_getIter, /* getiterfunc tp_iter; */
	( iternextfunc ) MFace_nextIter, /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	NULL,                       /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_MFace_getseters,        /* struct PyGetSetDef *tp_getset; */
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

static PyObject *MFace_CreatePyObject( Mesh * mesh, int i )
{
	BPy_MFace *obj = PyObject_NEW( BPy_MFace, &MFace_Type );

	if( !obj )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"PyObject_New() failed" );

	obj->mesh = mesh;
	obj->index = i;
	return (PyObject *)obj;
}

/************************************************************************
 *
 * Face sequence 
 *
 ************************************************************************/

static int MFaceSeq_len( BPy_MFaceSeq * self )
{
	return self->mesh->totface;
}

static PyObject *MFaceSeq_item( BPy_MFaceSeq * self, int i )
{
	if( i < 0 || i >= self->mesh->totface )
		return EXPP_ReturnPyObjError( PyExc_IndexError,
					      "array index out of range" );

	return MFace_CreatePyObject( self->mesh, i );
}

static PySequenceMethods MFaceSeq_as_sequence = {
	( inquiry ) MFaceSeq_len,      /* sq_length */
	( binaryfunc ) 0,	           /* sq_concat */
	( intargfunc ) 0,	           /* sq_repeat */
	( intargfunc ) MFaceSeq_item,  /* sq_item */
	( intintargfunc ) 0,           /* sq_slice */
	( intobjargproc ) 0,           /* sq_ass_item */
	( intintobjargproc ) 0,        /* sq_ass_slice */
	0,0,0,
};

/************************************************************************
 *
 * Python MFaceSeq_Type iterator (iterates over faces)
 *
 ************************************************************************/

/*
 * Initialize the interator index
 */

static PyObject *MFaceSeq_getIter( BPy_MFaceSeq * self )
{
	self->iter = 0;
	return EXPP_incr_ret ( (PyObject *) self );
}

/*
 * Return next MFace.
 */

static PyObject *MFaceSeq_nextIter( BPy_MFaceSeq * self )
{
	if( self->iter == self->mesh->totface )
		return EXPP_ReturnPyObjError( PyExc_StopIteration,
				"iterator at end" );

	return MFace_CreatePyObject( self->mesh, self->iter++ );
}

/************************************************************************
 *
 * Python MFaceSeq_Type methods
 *
 ************************************************************************/

static PyObject *MFaceSeq_extend( BPy_MEdgeSeq * self, PyObject *args )
{
	/*
	 * (a) check input for valid edge objects, faces which consist of
	 *     only three or four edges
	 * (b) check input to be sure edges form a closed face (each edge
	 *     contains verts in two other different edges?)
	 *
	 * (1) build list of new faces; remove duplicates
	 *   * use existing "v4=0 rule" for 3-vert faces
	 * (2) build list of existing faces for searching
	 * (3) from new face list, remove existing faces:
	 */

	int len, nverts;
	int i, j, k, new_face_count;
	int good_faces;
	SrchFaces *oldpair, *newpair, *tmppair, *tmppair2;
	PyObject *tmp;
	MFace *tmpface;
	Mesh *mesh = self->mesh;

	/* make sure we get a sequence of tuples of something */

	switch( PySequence_Size ( args ) ) {
	case 1:		/* better be a list or a tuple */
		args = PyTuple_GET_ITEM( args, 0 );
		if( !PySequence_Check ( args ) )
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a sequence of tuple pairs" );
		Py_INCREF( args );		/* so we can safely DECREF later */
		break;
	case 2:	
	case 3:
	case 4:		/* two to four args may be individual verts */
		tmp = PyTuple_GET_ITEM( args, 0 );
		if( PyTuple_Check( tmp ) ) {/* maybe just tuples, so use args as-is */
			Py_INCREF( args );		/* so we can safely DECREF later */
			break;
		}
		args = Py_BuildValue( "(O)", args );
		if( !args )
			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"Py_BuildValue() failed" );
		break;
	default:	/* anything else is definitely wrong */
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a sequence of tuple pairs" );
	}

	/* make sure there is something to add */
	len = PySequence_Size( args );
	if( len == 0 ) {
		Py_DECREF( args );
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"expected at least one tuple" );
	}

	/* verify the param list and get a total count of number of edges */
	new_face_count = 0;
	for( i = 0; i < len; ++i ) {
		tmp = PySequence_Fast_GET_ITEM( args, i );

		/* not a tuple of MVerts... error */
		if( !PyTuple_Check( tmp ) ||
				EXPP_check_sequence_consistency( tmp, &MVert_Type ) != 1 ) {
			Py_DECREF( args );
			return EXPP_ReturnPyObjError( PyExc_ValueError,
				"expected sequence of MVert tuples" );
		}

		/* not the right number of MVerts... error */
		nverts = PyTuple_Size( tmp );
		if( nverts < 2 || nverts > 4 ) {
			Py_DECREF( args );
			return EXPP_ReturnPyObjError( PyExc_ValueError,
				"expected 2 to 4 MVerts per tuple" );
		}

		if( nverts != 2 )		/* new faces cannot have only 2 verts */
			++new_face_count;
	}

	/* OK, commit to allocating the search structures */
	newpair = (SrchFaces *)MEM_callocN( sizeof(SrchFaces)*new_face_count,
			"MFacePairs" );

	/* scan the input list and build the new face pair list */
	len = PySequence_Size( args );
	tmppair = newpair;
	for( i = 0; i < len; ++i ) {
		unsigned int vert[4]={0,0,0,0};
		unsigned char order[4]={0,1,2,3};
		tmp = PySequence_Fast_GET_ITEM( args, i );
		nverts = PyTuple_Size( tmp );

		if( nverts == 2 )	/* again, ignore 2-vert tuples */
			break;

		/* get copies of vertices */
		for( j = 0; j < nverts; ++j ) {
			BPy_MVert *e = (BPy_MVert *)PyTuple_GET_ITEM( tmp, j );
			vert[j] = e->index;
		}

		/* convention says triangular faces always have v4 == 0 */
		if( nverts == 3 )
			tmppair->v[3] = 0;

		/*
		 * sort the verts before placing in pair list.  the order of
		 * vertices in the face is very important, so keep track of
		 * the original order
		 */

		for( j = nverts-1; j >= 0; --j ) {
			for( k = 0; k < j; ++k ) {
				if( vert[k] > vert[k+1] ) {
					SWAP( int, vert[k], vert[k+1] );
					SWAP( char, order[k], order[k+1] );
				} else if( vert[k] == vert[k+1] ) {
					MEM_freeN( newpair );
					Py_DECREF( args );
					return EXPP_ReturnPyObjError( PyExc_ValueError,
						"tuple contains duplicate vertices" );
				}
			}
			tmppair->v[j] = vert[j];
		}

		/* pack order into a byte */
		tmppair->order = order[0]|(order[1]<<2)|(order[2]<<4)|(order[3]<<6);
		++tmppair;
	}

	/* sort the new face pairs */
	qsort( newpair, new_face_count, sizeof(SrchFaces), mface_comp );

	/*
	 * find duplicates in the new list and mark.  if it's a duplicate,
	 * then mark by setting second vert index to 0 (a real edge won't have
	 * second vert index of 0 since verts are sorted)
	 */

	good_faces = new_face_count;	/* assume all faces good to start */

	tmppair = newpair;	/* "last good edge" */
	tmppair2 = &tmppair[1];	/* "current candidate edge" */
	for( i = 0; i < new_face_count; ++i ) {
		if( mface_comp( tmppair, tmppair2 ) )
			tmppair = tmppair2;	/* last != current, so current == last */
		else {
			tmppair2->v[1] = 0; /* last == current, so mark as duplicate */
			--good_faces;		/* one less good face */
		}
		tmppair2++;
	}

	/* if mesh has faces, see if any of the new faces are already in it */
	if( mesh->totface ) {
		oldpair = (SrchFaces *)MEM_callocN( sizeof(SrchFaces)*mesh->totface,
				"MFacePairs" );

		tmppair = oldpair;
		tmpface = mesh->mface;
		for( i = 0; i < mesh->totface; ++i ) {
			unsigned char order[4]={0,1,2,3};
			int verts[4];
			verts[0]=tmpface->v1;
			verts[1]=tmpface->v2;
			verts[2]=tmpface->v3;
			verts[3]=tmpface->v4;
	
			len = ( tmpface->v4 ) ? 3 : 2;
			tmppair->v[3] = 0;	/* for triangular faces */

		/* sort the verts before placing in pair list here too */
			for( j = len; j >= 0; --j ) {
				for( k = 0; k < j; ++k )
					if( verts[k] > verts[k+1] ) {
						SWAP( int, verts[k], verts[k+1] );
						SWAP( unsigned char, order[k], order[k+1] );
					}
				tmppair->v[j] = verts[j];
			}

		/* pack order into a byte */
			tmppair->order = order[0]|(order[1]<<2)|(order[2]<<4)|(order[3]<<6);
			++tmppair;
			++tmpface;
		}

	/* sort the old face pairs */
		qsort( oldpair, mesh->totface, sizeof(SrchFaces), mface_comp );

	/* eliminate new faces already in the mesh */
		tmppair = newpair;
		for( i = len; i-- ; ) {
			if( tmppair->v[1] ) {
				if( bsearch( tmppair, oldpair, mesh->totface, 
						sizeof(SrchFaces), mface_comp ) ) {
					tmppair->v[1] = 0;	/* mark as duplicate */
					--good_faces;
				} 
			}
			tmppair++;
		}
		MEM_freeN( oldpair );
	}

	/* if any new faces are left, add to list */
	if( good_faces ) {
		int totface = mesh->totface+good_faces;	/* new face count */

	/* allocate new face list */
		tmpface = MEM_callocN(totface*sizeof(MFace), "NMesh_addFaces");

	/* if we're appending, copy the old face list and delete it */
		if( mesh->mface ) {
			memcpy( tmpface, mesh->mface, mesh->totface*sizeof(MFace));
			MEM_freeN( mesh->mface );
		}
		mesh->mface = tmpface;		/* point to the new face list */

	/* point to the first face we're going to add */
		tmpface = &mesh->mface[mesh->totface];
		tmppair = newpair;

	/* as we find a good face, add it */
		while ( good_faces ) {
			if( tmppair->v[1] ) {
				int i;
				unsigned int index[4];
				unsigned char order = tmppair->order;

		/* unpack the order of the vertices */
				for( i = 0; i < 4; ++i ) {
					index[(order & 0x03)] = i;
					order >>= 2;
				}

		/* now place vertices in the proper order */
				tmpface->v1 = tmppair->v[index[0]];
				tmpface->v2 = tmppair->v[index[1]];
				tmpface->v3 = tmppair->v[index[2]];
				tmpface->v4 = tmppair->v[index[3]];
				tmpface->flag = 0;
				mesh->totface++;
				++tmpface;
				--good_faces;
			}
			tmppair++;
		}
	}

	/* clean up and leave */
	mesh_update( mesh );
	Py_DECREF ( args );
	MEM_freeN( newpair );
	return EXPP_incr_ret( Py_None );
}

static struct PyMethodDef BPy_MFaceSeq_methods[] = {
	{"extend", (PyCFunction)MFaceSeq_extend, METH_VARARGS,
		"add edges to mesh"},
	{NULL, NULL, 0, NULL}
};

/************************************************************************
 *
 * Python MFaceSeq_Type standard operations
 *
 ************************************************************************/

static void MFaceSeq_dealloc( BPy_MFaceSeq * self )
{
	PyObject_DEL( self );
}

/*****************************************************************************/
/* Python NMFaceSeq_Type structure definition:                               */
/*****************************************************************************/
PyTypeObject MFaceSeq_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender MFaceSeq",           /* char *tp_name; */
	sizeof( BPy_MFaceSeq ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) MFaceSeq_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	NULL,                       /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	&MFaceSeq_as_sequence,	    /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
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
	( getiterfunc )MFaceSeq_getIter, /* getiterfunc tp_iter; */
	( iternextfunc )MFaceSeq_nextIter, /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_MFaceSeq_methods,       /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	NULL,                       /* struct PyGetSetDef *tp_getset; */
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

/************************************************************************
 *
 * Python BPy_Mesh methods
 *
 ************************************************************************/

static PyObject *Mesh_calcNormals( BPy_Mesh * self )
{
	Mesh *mesh = self->mesh;

	mesh_calc_normals( mesh->mvert, mesh->totvert, mesh->mface,
			mesh->totface, NULL );
	return EXPP_incr_ret( Py_None );
}

static PyObject *Mesh_vertexShade( BPy_Mesh * self )
{
	Base *base = FIRSTBASE;

	if( G.obedit )
		return EXPP_ReturnPyObjError(PyExc_RuntimeError,
				"can't shade vertices while in edit mode" );

	while( base ) {
		if( base->object->type == OB_MESH && 
				base->object->data == self->mesh ) {
			base->flag |= SELECT;
			set_active_base( base );
			make_vertexcol();
			countall();
			return EXPP_incr_ret( Py_None );
		}
		base = base->next;
	}
	return EXPP_ReturnPyObjError(PyExc_RuntimeError,
			"object not found in baselist!" );
}

/************************************************************************
 *
 * Mesh attributes
 *
 ************************************************************************/

static PyObject *Mesh_getVerts( BPy_Mesh * self )
{
	BPy_MVertSeq *seq = PyObject_NEW( BPy_MVertSeq, &MVertSeq_Type);
	seq->mesh = self->mesh;
	return (PyObject *)seq;
}

static PyObject *Mesh_getEdges( BPy_Mesh * self )
{
	BPy_MEdgeSeq *seq = PyObject_NEW( BPy_MEdgeSeq, &MEdgeSeq_Type);
	seq->mesh = self->mesh;
	return (PyObject *)seq;
}

static PyObject *Mesh_getFaces( BPy_Mesh * self )
{
	BPy_MFaceSeq *seq = PyObject_NEW( BPy_MFaceSeq, &MFaceSeq_Type);
	seq->mesh = self->mesh;
	return (PyObject *)seq;
}

static PyObject *Mesh_getMaterials( BPy_Mesh *self )
{
	return EXPP_PyList_fromMaterialList( self->mesh->mat,
			self->mesh->totcol, 1 );
}

static int Mesh_setMaterials( BPy_Mesh *self, PyObject * value )
{
    Material **matlist;
	int len;

    if( !EXPP_check_sequence_consistency( value, &Material_Type ) )
        return EXPP_ReturnIntError( PyExc_TypeError,
                  "list should only contain materials or None)" );

    len = PyList_Size( value );
    if( len > 16 )
        return EXPP_ReturnIntError( PyExc_TypeError,
                          "list can't have more than 16 materials" );

	/* free old material list (if it exists) and adjust user counts */
	if( self->mesh->mat ) {
		Mesh *me = self->mesh;
		int i;
		for( i = me->totcol; i-- > 0; )
			if( me->mat[i] )
           		me->mat[i]->id.us--;
		MEM_freeN( me->mat );
	}

	/* build the new material list, increment user count, store it */

	matlist = EXPP_newMaterialList_fromPyList( value );
	EXPP_incr_mats_us( matlist, len );
	self->mesh->mat = matlist;
    self->mesh->totcol = (short)len;

/**@ This is another ugly fix due to the weird material handling of blender.
    * it makes sure that object material lists get updated (by their length)
    * according to their data material lists, otherwise blender crashes.
    * It just stupidly runs through all objects...BAD BAD BAD.
    */

    test_object_materials( ( ID * ) self->mesh );

	return 0;
}

static PyObject *Mesh_getMaxSmoothAngle( BPy_Mesh * self )
{
    PyObject *attr = PyInt_FromLong( self->mesh->smoothresh );

    if( attr )
        return attr;

    return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"PyInt_FromLong() failed" );
}

static int Mesh_setMaxSmoothAngle( BPy_Mesh *self, PyObject *value )
{
    return EXPP_setIValueClamped( value, &self->mesh->smoothresh,
                            MESH_SMOOTHRESH_MIN,
                            MESH_SMOOTHRESH_MAX, 'h' );
}

static PyObject *Mesh_getSubDivLevels( BPy_Mesh * self )
{
	PyObject *attr = Py_BuildValue( "(h,h)",
			self->mesh->subdiv, self->mesh->subdivr );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"Py_BuildValue() failed" );
}

static int Mesh_setSubDivLevels( BPy_Mesh *self, PyObject *value )
{
	int subdiv[2];
	int i;
	PyObject *tmp;

#if 0
	if( !PyArg_ParseTuple( value, "ii", &subdiv, &subdivr ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
						"expected (int, int) as argument" );
#endif
	if( !PyTuple_Check( value ) || PyTuple_Size( value ) != 2 )
		return EXPP_ReturnIntError( PyExc_TypeError,
						"expected (int, int) as argument" );

	for( i = 0; i < 2; i++ ) {
		tmp = PyTuple_GET_ITEM( value, i );
		if( !PyInt_Check( tmp ) )
			return EXPP_ReturnIntError ( PyExc_TypeError,
				  "expected a list [int, int] as argument" );
		subdiv[i] = EXPP_ClampInt( PyInt_AsLong( tmp ),
						 MESH_SUBDIV_MIN,
						 MESH_SUBDIV_MAX );
	}

	self->mesh->subdiv = (short)subdiv[0];
	self->mesh->subdivr = (short)subdiv[1];
	return 0;
}

static PyObject *Mesh_getName( BPy_Mesh * self )
{
	PyObject *attr = PyString_FromString( self->mesh->id.name + 2 );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Mesh.name attribute" );
}

static int Mesh_setName( BPy_Mesh * self, PyObject * value )
{
	char *name;
	char buf[21];

	name = PyString_AsString ( value );
	if( !name )
		return EXPP_ReturnIntError( PyExc_TypeError,
					      "expected string argument" );

	PyOS_snprintf( buf, sizeof( buf ), "%s", name );

	rename_id( &self->mesh->id, buf );

	return 0;
}

static PyObject *Mesh_getUsers( BPy_Mesh * self )
{
	PyObject *attr = PyInt_FromLong( self->mesh->id.us );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Mesh.users attribute" );
}

static PyObject *Mesh_getFlag( BPy_Mesh * self, void *type )
{
	PyObject *attr;
	switch( (int)type ) {
	case MESH_HASFACEUV:
		attr = self->mesh->tface ? EXPP_incr_ret_True() :
			EXPP_incr_ret_False();
		break;
	case MESH_HASMCOL:
		attr = self->mesh->mcol ? EXPP_incr_ret_True() :
			EXPP_incr_ret_False();
		break;
	case MESH_HASVERTUV:
		attr = self->mesh->msticky ? EXPP_incr_ret_True() :
			EXPP_incr_ret_False();
		break;
	default:
		attr = NULL;
	}

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get attribute" );
}

static PyObject *Mesh_getMode( BPy_Mesh * self )
{
	PyObject *attr = PyInt_FromLong( self->mesh->flag );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Mesh.mode attribute" );
}

static int Mesh_setMode( BPy_Mesh *self, PyObject *value )
{
	short param;
	static short bitmask = ME_NOPUNOFLIP | ME_TWOSIDED | ME_AUTOSMOOTH;

	if( !PyInt_CheckExact ( value ) ) {
		char errstr[128];
		sprintf ( errstr , "expected int bitmask of 0x%04x", bitmask );
		return EXPP_ReturnIntError( PyExc_TypeError, errstr );
	}
	param = (short)PyInt_AS_LONG ( value );

	if( ( param & bitmask ) != param )
		return EXPP_ReturnIntError( PyExc_ValueError,
						"invalid bit(s) set in mask" );

	self->mesh->flag = param; 

	return 0;
}

static PyObject *Mesh_getActiveFace( BPy_Mesh * self )
{
	TFace *face;
	int i, totface;

	if( !self->mesh->tface )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"face has no texture values" );

	face = self->mesh->tface;
	totface = self->mesh->totface;

	for( i = 0; i < totface; ++face, ++i )
		if( face->flag & TF_ACTIVE ) {
			PyObject *attr = PyInt_FromLong( i );

			if( attr )
				return attr;

			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"PyInt_FromLong() failed" );
		}

	return EXPP_incr_ret( Py_None );
}

static int Mesh_setActiveFace( BPy_Mesh * self, PyObject * value )
{
	TFace *face;
	int param;

	/* if no texture faces, error */

	if( !self->mesh->tface )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"face has no texture values" );

	/* if param isn't an int, error */

	if( !PyInt_CheckExact( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected an int argument" );

	/* check for a valid index */

	param = PyInt_AsLong( value );
	if( param < 0 || param > self->mesh->totface )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"face index out of range" );

	face = self->mesh->tface;

	/* if requested face isn't already active, then inactivate all
	 * faces and activate the requested one */

	if( !( face[param].flag & TF_ACTIVE ) ) {
		int i;
		for( i = self->mesh->totface; i > 0; ++face, --i )
			face->flag &= ~TF_ACTIVE;
		self->mesh->tface[param].flag |= TF_ACTIVE;
	}
	return 0;
}

static void Mesh_dealloc( BPy_Mesh * self )
{
	PyObject_DEL( self );
}

static PyObject *Mesh_repr( BPy_Mesh * self )
{
	return PyString_FromFormat( "[Mesh \"%s\"]",
				    self->mesh->id.name + 2 );
}

static struct PyMethodDef BPy_Mesh_methods[] = {
	{"calcNormals", (PyCFunction)Mesh_calcNormals, METH_NOARGS,
		"all recalculate vertex normals"},
	{"vertexShade", (PyCFunction)Mesh_vertexShade, METH_VARARGS,
		"color vertices based on the current lighting setup"},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python NMesh_Type attributes get/set structure:                           */
/*****************************************************************************/
static PyGetSetDef BPy_Mesh_getseters[] = {
	{"verts",
	 (getter)Mesh_getVerts, (setter)NULL,
	 "The mesh's vertices (MVert)",
	 NULL},
	{"edges",
	 (getter)Mesh_getEdges, (setter)NULL,
	 "The mesh's edge data (MEdge)",
	 NULL},
	{"faces",
	 (getter)Mesh_getFaces, (setter)NULL,
	 "The mesh's face data (MFace)",
	 NULL},
	{"materials",
	 (getter)Mesh_getMaterials, (setter)Mesh_setMaterials,
	 "List of the mesh's materials",
	 NULL},
	{"degr",
	 (getter)Mesh_getMaxSmoothAngle, (setter)Mesh_setMaxSmoothAngle,
	 "The max angle for auto smoothing",
	 NULL},
	{"maxSmoothAngle",
	 (getter)Mesh_getMaxSmoothAngle, (setter)Mesh_setMaxSmoothAngle,
	 "deprecated: see 'degr'",
	 NULL},
	{"subDivLevels",
	 (getter)Mesh_getSubDivLevels, (setter)Mesh_setSubDivLevels,
	 "The display and rendering subdivision levels",
	 NULL},
	{"name",
	 (getter)Mesh_getName, (setter)Mesh_setName,
	 "The mesh's data name",
	 NULL},
	{"mode",
	 (getter)Mesh_getMode, (setter)Mesh_setMode,
	 "The mesh's mode bitfield",
	 NULL},


	{"faceUV",
	 (getter)Mesh_getFlag, (setter)NULL,
	 "UV-mapped textured faces enabled",
 	 (void *)MESH_HASFACEUV},
	{"vertexColors",
	 (getter)Mesh_getFlag, (setter)NULL,
	 "Vertex colors for the mesh enabled",
	 (void *)MESH_HASMCOL},
	{"vertexUV",
	 (getter)Mesh_getFlag, (setter)NULL,
	 "'Sticky' flag for per vertex UV coordinates enabled",
	 (void *)MESH_HASVERTUV},
	{"activeFace",
	 (getter)Mesh_getActiveFace, (setter)Mesh_setActiveFace,
	 "Index of the mesh's active texture face (in UV editor)",
	 NULL},
	{"users",
	 (getter)Mesh_getUsers, (setter)NULL,
	 "Number of users of the mesh",
	 NULL},

	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/*****************************************************************************/
/* Python Mesh_Type callback function prototypes:                           */
/*****************************************************************************/
static void Mesh_dealloc( BPy_Mesh * object );

/*****************************************************************************/
/* Python Mesh_Type structure definition:                                   */
/*****************************************************************************/
PyTypeObject Mesh_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender Mesh",             /* char *tp_name; */
	sizeof( BPy_Mesh ),         /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) Mesh_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	( reprfunc ) Mesh_repr,     /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
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
	BPy_Mesh_methods,          /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_Mesh_getseters,        /* struct PyGetSetDef *tp_getset; */
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

/*
 * get one or all mesh data objects
 */

static PyObject *M_Mesh_Get( PyObject * self, PyObject * args )
{
	char *name = NULL;
	Mesh *mesh = NULL;
	BPy_Mesh* obj;

	if( !PyArg_ParseTuple( args, "|s", &name ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected zero or one string arguments" );

	if( name ) {
		mesh = ( Mesh * ) GetIdFromList( &( G.main->mesh ), name );

		if( !mesh )
			return EXPP_incr_ret( Py_None );

		obj = PyObject_NEW( BPy_Mesh, &Mesh_Type );
		obj->mesh = mesh;
		return (PyObject *)obj;
	} else {			/* () - return a list with all meshes in the scene */
		PyObject *meshlist;
		Link *link;
		int index = 0;

		meshlist = PyList_New( BLI_countlist( &( G.main->mesh ) ) );

		if( !meshlist )
			return EXPP_ReturnPyObjError( PyExc_MemoryError,
					"couldn't create PyList" );

		link = G.main->mesh.first;
		index = 0;
		while( link ) {
			obj = ( BPy_Mesh * ) PyObject_NEW( BPy_Object,
					&Mesh_Type );
			obj->mesh = ( Mesh * )link;
			PyList_SetItem( meshlist, index, ( PyObject * ) obj );
			index++;
			link = link->next;
		}
		return meshlist;
	}
}

/*
 * create a new mesh data object
 */

static PyObject *M_Mesh_New( PyObject * self, PyObject * args )
{
	char *name = "Mesh";
	PyObject *ret = NULL;
	Mesh *mesh;
	BPy_Mesh *obj;
	char buf[21];

	if( !PyArg_ParseTuple( args, "|s", &name ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected nothing or a string as argument" );

	obj = (BPy_Mesh *)PyObject_NEW( BPy_Mesh, &Mesh_Type );

	if( !obj )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				       "PyObject_New() failed" );

	mesh = add_mesh(); /* doesn't return NULL now, but might someday */

	if( !mesh ) {
		Py_DECREF ( obj );
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				       "FATAL: could not create mesh object" );
	}
	mesh->id.us = 0;
	G.totmesh++;

	PyOS_snprintf( buf, sizeof( buf ), "%s", name );
	rename_id( &mesh->id, buf );

	obj->mesh = mesh;
	return (PyObject *)obj;
}

#define SUBDIVIDE_EXPERIMENT
#undef SUBDIVIDE_EXPERIMENT

#ifdef SUBDIVIDE_EXPERIMENT
#include <BIF_editmesh.h>

/*
 * test case 
 */

static PyObject *M_Mesh_Subdivide( PyObject * self, PyObject * args )
{
	struct Object *object; 
	struct Base *basact;
	char *name = NULL;

	PyArg_ParseTuple( args, "|s", &name );

	object = GetObjectByName( name );

	if( !object )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
				"Unknown object specified." );

	if( object->type != OB_MESH )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
						"Object specified is not a mesh." );

	basact = BASACT;

	/* if already in edit mode, get out */

	if( basact )
		exit_editmode( 1 );

	/* enter mesh edit mode, apply subdivide, then exit edit mode */

	G.obedit = object;
	enter_editmode( );
	esubdivideflag(1, 0.0, G.scene->toolsettings->editbutflag & B_BEAUTY,1,0);
	exit_editmode( 1 );

	/* return to previous edit set-up (hopefully?) */

	if( basact ) {
		BASACT = basact;
		enter_editmode( );
	}

	return EXPP_incr_ret( Py_None );
}
#endif

static struct PyMethodDef M_Mesh_methods[] = {
	{"New", (PyCFunction)M_Mesh_New, METH_VARARGS,
		"Create a new mesh"},
	{"Get", (PyCFunction)M_Mesh_Get, METH_VARARGS,
		"Get a mesh by name"},
#ifdef SUBDIVIDE_EXPERIMENT
	{"Subdivide", (PyCFunction)M_Mesh_Subdivide, METH_VARARGS,
		"Subdivide selected edges in a mesh (experimental)"},
#endif
	{NULL, NULL, 0, NULL},
};

static char M_Mesh_doc[] = "The Blender.Mesh submodule";

PyObject *Mesh_Init( void )
{
	PyObject *submodule;

	if( PyType_Ready( &MCol_Type ) < 0 )
		return NULL;
	if( PyType_Ready( &MVert_Type ) < 0 )
		return NULL;
	if( PyType_Ready( &MVertSeq_Type ) < 0 )
		return NULL;
	if( PyType_Ready( &MEdge_Type ) < 0 )
		return NULL;
	if( PyType_Ready( &MEdgeSeq_Type ) < 0 )
		return NULL;
	if( PyType_Ready( &MFace_Type ) < 0 )
		return NULL;
	if( PyType_Ready( &MFaceSeq_Type ) < 0 )
		return NULL;
	if( PyType_Ready( &Mesh_Type ) < 0 )
		return NULL;

	submodule =
		Py_InitModule3( "Blender.Mesh", M_Mesh_methods, M_Mesh_doc );

	return submodule;
}

/* These are needed by Object.c */

PyObject *Mesh_CreatePyObject( Mesh * me )
{
	BPy_Mesh *nmesh = PyObject_NEW( BPy_Mesh, &Mesh_Type );

	if( !nmesh )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
				"couldn't create BPy_Mesh object" );

	nmesh->mesh = me;

	return ( PyObject * ) nmesh;
}

int Mesh_CheckPyObject( PyObject * pyobj )
{
	return ( pyobj->ob_type == &Mesh_Type );
}

Mesh *Mesh_FromPyObject( PyObject * pyobj )
{
	BPy_Mesh *blen_obj;

	blen_obj = ( BPy_Mesh * ) pyobj;
	return blen_obj->mesh;

}
