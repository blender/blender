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
 * This is a new part of Blender.
 *
 * Contributor(s): Pontus Lidman, Johnny Matthews, Ken Hughes,
 *   Michael Reimpell
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "Key.h" /*This must come first*/

#include "DNA_scene_types.h"

#include <BLI_blenlib.h>
#include <BKE_global.h>
#include <BKE_main.h>
#include <BKE_curve.h>
#include <BKE_library.h>
#include "BIF_space.h"

#include "Ipocurve.h"
#include "NMesh.h" /* we create NMesh.NMVert objects */
#include "Ipo.h"
#include "BezTriple.h"

#include "BSE_editipo.h"
#include "mydevice.h"
#include "BKE_depsgraph.h"
#include "blendef.h"
#include "constant.h"
#include "gen_utils.h"
#include "gen_library.h"

#define KEY_TYPE_MESH    0
#define KEY_TYPE_CURVE   1
#define KEY_TYPE_LATTICE 2

/* macro from blenkernel/intern/key.c:98 */
#define GS(a)	(*((short *)(a)))

static int Key_compare( BPy_Key * a, BPy_Key * b );
static PyObject *Key_repr( BPy_Key * self );
static void Key_dealloc( BPy_Key * self );

static PyObject *Key_getBlocks( BPy_Key * self );
static PyObject *Key_getType( BPy_Key * self );
static PyObject *Key_getRelative( BPy_Key * self );
static PyObject *Key_getIpo( BPy_Key * self );
static int Key_setIpo( BPy_Key * self, PyObject * args );
static PyObject *Key_getValue( BPy_Key * self );
static int Key_setRelative( BPy_Key * self, PyObject * value );

static struct PyMethodDef Key_methods[] = {
	{ "getBlocks", (PyCFunction) Key_getBlocks, METH_NOARGS, "Get key blocks" },
	{ "getIpo", (PyCFunction) Key_getIpo, METH_NOARGS, "Get key Ipo" },
	{ 0, 0, 0, 0 }
};

static PyGetSetDef BPy_Key_getsetters[] = {
	{"type",(getter)Key_getType, (setter)NULL,
	 "Key Type",NULL},
	{"value",(getter)Key_getValue, (setter)NULL,
	 "Key value",NULL},
	{"ipo",(getter)Key_getIpo, (setter)Key_setIpo,
	 "Ipo linked to key",NULL},
	{"blocks",(getter)Key_getBlocks, (setter)NULL,
	 "Blocks linked to the key",NULL},
	{"relative",(getter)Key_getRelative, (setter)Key_setRelative,
	 "Non-zero is key is relative",NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

static PyObject *KeyBlock_getData( PyObject * self );
static PyObject *KeyBlock_getCurval( BPy_KeyBlock * self );
static PyObject *KeyBlock_getName( BPy_KeyBlock * self );
static PyObject *KeyBlock_getPos( BPy_KeyBlock * self );
static PyObject *KeyBlock_getSlidermin( BPy_KeyBlock * self );
static PyObject *KeyBlock_getSlidermax( BPy_KeyBlock * self );
static PyObject *KeyBlock_getVgroup( BPy_KeyBlock * self );

static int KeyBlock_setName( BPy_KeyBlock *, PyObject * args  );
static int KeyBlock_setVgroup( BPy_KeyBlock *, PyObject * args  );
static int KeyBlock_setSlidermin( BPy_KeyBlock *, PyObject * args  );
static int KeyBlock_setSlidermax( BPy_KeyBlock *, PyObject * args  );

static void KeyBlock_dealloc( BPy_KeyBlock * self );
static int KeyBlock_compare( BPy_KeyBlock * a, BPy_KeyBlock * b );
static PyObject *KeyBlock_repr( BPy_KeyBlock * self );

static struct PyMethodDef KeyBlock_methods[] = {
	{ "getData", (PyCFunction) KeyBlock_getData, METH_NOARGS,
		"Get keyblock data" },
	{ 0, 0, 0, 0 }
};

static PyGetSetDef BPy_KeyBlock_getsetters[] = {
		{"curval",(getter)KeyBlock_getCurval, (setter)NULL,
		 "Current value of the corresponding IpoCurve",NULL},
		{"name",(getter)KeyBlock_getName, (setter)KeyBlock_setName,
		 "Keyblock Name",NULL},
		{"pos",(getter)KeyBlock_getPos, (setter)NULL,
		 "Keyblock Pos",NULL},
		{"slidermin",(getter)KeyBlock_getSlidermin, (setter)KeyBlock_setSlidermin,
		 "Keyblock Slider Minimum",NULL},
		{"slidermax",(getter)KeyBlock_getSlidermax, (setter)KeyBlock_setSlidermax,
		 "Keyblock Slider Maximum",NULL},
		{"vgroup",(getter)KeyBlock_getVgroup, (setter)KeyBlock_setVgroup,
		 "Keyblock VGroup",NULL},
		{"data",(getter)KeyBlock_getData, (setter)NULL,
		 "Keyblock VGroup",NULL},
		{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

PyTypeObject Key_Type = {
	PyObject_HEAD_INIT( NULL ) 0,	/*ob_size */
	"Blender Key",					/*tp_name */
	sizeof( BPy_Key ),				/*tp_basicsize */
	0,								/*tp_itemsize */
	/* methods */
	( destructor ) Key_dealloc,/* destructor tp_dealloc; */
	( printfunc ) 0,				/*tp_print */
	( getattrfunc ) 0,	/*tp_getattr */
	( setattrfunc ) 0,			 	/*tp_setattr */
	( cmpfunc) Key_compare, 		/*tp_compare*/
	( reprfunc ) Key_repr, 			/* tp_repr */
	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	( hashfunc ) GenericLib_hash,	/* hashfunc tp_hash; */
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
	Key_methods,           		/* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_Key_getsetters,     	/* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                      	/* PyObject *tp_dict; */
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

PyTypeObject KeyBlock_Type = {
	PyObject_HEAD_INIT( NULL ) 0,	/*ob_size */
	"Blender KeyBlock",	/*tp_name */
	sizeof( BPy_KeyBlock ),	/*tp_basicsize */
	0,			/*tp_itemsize */
	/* methods */
	( destructor ) KeyBlock_dealloc,/* destructor tp_dealloc; */
	( printfunc ) 0,				/*tp_print */
	( getattrfunc ) 0,	/*tp_getattr */
	( setattrfunc ) 0,			 	/*tp_setattr */
	( cmpfunc) KeyBlock_compare, 		/*tp_compare*/
	( reprfunc ) KeyBlock_repr, 			/* tp_repr */
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
	KeyBlock_methods, 			/* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_KeyBlock_getsetters,    /* struct PyGetSetDef *tp_getset; */
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

PyObject *Key_CreatePyObject( Key * blenkey )
{
	BPy_Key *bpykey = PyObject_NEW( BPy_Key, &Key_Type );
	/* blenkey may be NULL so be careful */
	bpykey->key = blenkey;
	return ( PyObject * ) bpykey;
}

static void Key_dealloc( BPy_Key * self )
{
	PyObject_DEL( self );
}

static int Key_compare( BPy_Key * a, BPy_Key * b )
{
	return ( a->key == b->key ) ? 0 : -1;
}

static PyObject *Key_repr( BPy_Key * self )
{
	return PyString_FromFormat( "[Key \"%s\"]", self->key->id.name + 2 );
}

static PyObject *Key_getIpo( BPy_Key * self )
{
	if (self->key->ipo)
		return Ipo_CreatePyObject( self->key->ipo );
	Py_RETURN_NONE;
}

static int Key_setIpo( BPy_Key * self, PyObject * value )
{
	return GenericLib_assignData(value, (void **) &self->key->ipo, 0, 1, ID_IP, ID_KE);
}

static PyObject *Key_getRelative( BPy_Key * self )
{
	if( self->key->type == KEY_RELATIVE )
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}

static int Key_setRelative( BPy_Key * self, PyObject * value )
{
	if( PyObject_IsTrue( value ) )
		self->key->type = KEY_RELATIVE;
	else
		self->key->type = KEY_NORMAL;
	allqueue(REDRAWIPO, 0);
	allspace(REMAKEIPO, 0);

	return 0;
}

static PyObject *Key_getType( BPy_Key * self )
{
	int idcode;
	int type = -1;

	idcode = GS( self->key->from->name );

	switch( idcode ) {
	case ID_ME:
		type = KEY_TYPE_MESH;
		break;
	case ID_CU:
		type = KEY_TYPE_CURVE;
		break;
	case ID_LT:
		type = KEY_TYPE_LATTICE;
		break;
	}

	return PyInt_FromLong( type );
}

static PyObject *Key_getBlocks( BPy_Key * self )
{
	Key *key = self->key;
	KeyBlock *kb;
	int i=0;
	PyObject *l = PyList_New( BLI_countlist( &(key->block)) );

	for (kb = key->block.first; kb; kb = kb->next, i++)
		PyList_SET_ITEM( l, i, KeyBlock_CreatePyObject( kb, key ) );
	
	return l;
}

static PyObject *Key_getValue( BPy_Key * self )
{
	BPy_Key *k = ( BPy_Key * ) self;

	return PyFloat_FromDouble( k->key->curval );
}

/* ------------ Key Block Functions -------------- */
PyObject *KeyBlock_CreatePyObject( KeyBlock * keyblock, Key *parentKey )
{
	BPy_KeyBlock *bpykb = PyObject_NEW( BPy_KeyBlock, &KeyBlock_Type );
	bpykb->key = parentKey;
	bpykb->keyblock = keyblock; /* keyblock maye be NULL, thats ok */
	return ( PyObject * ) bpykb;
}

static PyObject *KeyBlock_getCurval( BPy_KeyBlock * self ) {
	return PyFloat_FromDouble( self->keyblock->curval );
}

static PyObject *KeyBlock_getName( BPy_KeyBlock * self ) {
	return PyString_FromString(self->keyblock->name);
}

static PyObject *KeyBlock_getPos( BPy_KeyBlock * self ){
	return PyFloat_FromDouble( self->keyblock->pos );			
}

static PyObject *KeyBlock_getSlidermin( BPy_KeyBlock * self ){
	return PyFloat_FromDouble( self->keyblock->slidermin );	
}

static PyObject *KeyBlock_getSlidermax( BPy_KeyBlock * self ){
	return PyFloat_FromDouble( self->keyblock->slidermax );
}

static PyObject *KeyBlock_getVgroup( BPy_KeyBlock * self ){
	return PyString_FromString(self->keyblock->vgroup);
}

static int KeyBlock_setName( BPy_KeyBlock * self, PyObject * args ){
	char* text = NULL;
 
	text = PyString_AsString ( args );
	if( !text )
		return EXPP_ReturnIntError( PyExc_TypeError,
					      "expected string argument" );							
	strncpy( self->keyblock->name, text , 32);

	return 0;	
}

static int KeyBlock_setVgroup( BPy_KeyBlock * self, PyObject * args  ){
	char* text = NULL;

	text = PyString_AsString ( args );
	if( !text )
		return EXPP_ReturnIntError( PyExc_TypeError,
					      "expected string argument" );							
	strncpy( self->keyblock->vgroup, text , 32);

	return 0;	
}
static int KeyBlock_setSlidermin( BPy_KeyBlock * self, PyObject * args  ){
	return EXPP_setFloatClamped ( args, &self->keyblock->slidermin,
								-10.0f,
								10.0f );	
}
static int KeyBlock_setSlidermax( BPy_KeyBlock * self, PyObject * args  ){
	return EXPP_setFloatClamped ( args, &self->keyblock->slidermax,
								-10.0f,
								10.0f );
}

static void KeyBlock_dealloc( BPy_KeyBlock * self )
{
	PyObject_DEL( self );
}

static int KeyBlock_compare( BPy_KeyBlock * a, BPy_KeyBlock * b )
{
	return ( a->keyblock == b->keyblock ) ? 0 : -1;
}

static PyObject *KeyBlock_repr( BPy_KeyBlock * self )
{
	return PyString_FromFormat( "[KeyBlock \"%s\"]", self->keyblock->name );
}


static Curve *find_curve( Key *key )
{
	Curve *cu;

	if( !key )
		return NULL;

	for( cu = G.main->curve.first; cu; cu = cu->id.next ) {
		if( cu->key == key )
			break;
	}
	return cu;
}

static PyObject *KeyBlock_getData( PyObject * self )
{
	/* If this is a mesh key, data is an array of MVert coords.
	   If lattice, data is an array of BPoint coords
	   If curve, data is an array of BezTriple or BPoint */

	char *datap;
	int datasize;
	int idcode;
	int i;
	Curve *cu;
	Nurb* nu;
	PyObject *l;
	BPy_KeyBlock *kb = ( BPy_KeyBlock * ) self;
	Key *key = kb->key;

	if( !kb->keyblock->data ) {
		Py_RETURN_NONE;
	}

	l = PyList_New( kb->keyblock->totelem );
	if( !l )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
				"PyList_New() failed" );							

	idcode = GS( key->from->name );

	switch(idcode) {
	case ID_ME:

		for (i=0, datap = kb->keyblock->data; i<kb->keyblock->totelem; i++) {

			BPy_NMVert *mv = PyObject_NEW( BPy_NMVert, &NMVert_Type );
			MVert *vert = (MVert *) datap;

			mv->co[0]=vert->co[0];
			mv->co[1]=vert->co[1];
			mv->co[2]=vert->co[2];
			mv->no[0] = 0.0;
			mv->no[1] = 0.0;
			mv->no[2] = 0.0;

			mv->uvco[0] = mv->uvco[1] = mv->uvco[2] = 0.0;
			mv->index = i;
			mv->flag = 0;

			PyList_SetItem(l, i, ( PyObject * ) mv);

			datap += kb->key->elemsize;
		}
		break;

	case ID_CU:
		cu = find_curve ( key );
		if( !cu )
			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "key is no linked to any curve!" );							
		datasize = count_curveverts(&cu->nurb);
		nu = cu->nurb.first;
		if( nu->bezt ) {
			datasize /= 3;
			Py_DECREF (l);	
			l = PyList_New( datasize );
			for( i = 0, datap = kb->keyblock->data; i < datasize;
					i++, datap += sizeof(float)*12 ) {
				/* 
				 * since the key only stores the control point and not the
				 * other BezTriple attributes, build a Py_NEW BezTriple
				 */
				PyObject *pybt = newBezTriple( (float *)datap );
				PyList_SetItem( l, i, pybt );
			}
		} else {
			for( i = 0, datap = kb->keyblock->data; i < datasize;
					i++, datap += kb->key->elemsize ) {
				PyObject *pybt;
				float *fp = (float *)datap;
				pybt = Py_BuildValue( "[f,f,f]", fp[0],fp[1],fp[2]);
				if( !pybt ) {
					Py_DECREF( l );
					return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "Py_BuildValue() failed" );							
				}
				PyList_SetItem( l, i, pybt );
			}
		}
		break;

	case ID_LT:

		for( i = 0, datap = kb->keyblock->data; i < kb->keyblock->totelem;
				i++, datap += kb->key->elemsize ) {
			/* Lacking a python class for BPoint, use a list of three floats */
			PyObject *pybt;
			float *fp = (float *)datap;
			pybt = Py_BuildValue( "[f,f,f]", fp[0],fp[1],fp[2]);
			if( !pybt ) {
				Py_DECREF( l );
				return EXPP_ReturnPyObjError( PyExc_MemoryError,
					  "Py_BuildValue() failed" );							
			}
			PyList_SetItem( l, i, pybt );
		}
		break;
	}
	
	return l;
}

static PyObject *M_Key_Get( PyObject * self, PyObject * args )
{
	char *name = NULL;
	Key *key_iter;
	char error_msg[64];
	int i;

	if( !PyArg_ParseTuple( args, "|s", &name ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
			"expected string argument (or nothing)" ) );

	if ( name ) {
		for (key_iter = G.main->key.first; key_iter; 
				key_iter=key_iter->id.next) {
			if  (strcmp ( key_iter->id.name + 2, name ) == 0 ) {
				return Key_CreatePyObject( key_iter );
			}
		}

		PyOS_snprintf( error_msg, sizeof( error_msg ),
			"Key \"%s\" not found", name );
		return EXPP_ReturnPyObjError ( PyExc_NameError, error_msg );
		
	} else {

		PyObject *keylist;

		keylist = PyList_New( BLI_countlist( &( G.main->key ) ) );

		for ( i=0, key_iter = G.main->key.first; key_iter;
				key_iter=key_iter->id.next, i++ ) {
			PyList_SetItem(keylist, i, Key_CreatePyObject(key_iter));
		}
		return keylist;
	}
}

struct PyMethodDef M_Key_methods[] = {
	{"Get", M_Key_Get, METH_VARARGS, "Get a key or all key names"},
	{NULL, NULL, 0, NULL}
};

static PyObject *M_Key_TypesDict( void )
{
	PyObject *T = PyConstant_New(  );

	if( T ) {
		BPy_constant *d = ( BPy_constant * ) T;

		PyConstant_Insert( d, "MESH", PyInt_FromLong( KEY_TYPE_MESH ) );
		PyConstant_Insert( d, "CURVE", PyInt_FromLong( KEY_TYPE_CURVE ) );
		PyConstant_Insert( d, "LATTICE", PyInt_FromLong( KEY_TYPE_LATTICE ) );
	}

	return T;
}

PyObject *Key_Init( void )
{
	PyObject *submodule;
	PyObject *Types = NULL;

	if( PyType_Ready( &Key_Type ) < 0 || PyType_Ready( &KeyBlock_Type ) < 0 )
		return NULL;

	submodule =
		Py_InitModule3( "Blender.Key", M_Key_methods, "Key module" );

	Types = M_Key_TypesDict(  );
	if( Types )
		PyModule_AddObject( submodule, "Types", Types );

	return submodule;
}

