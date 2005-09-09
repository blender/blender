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
 * Contributor(s): Pontus Lidman
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <BLI_blenlib.h>
#include <BKE_global.h>
#include <BKE_main.h>

#include "Key.h"
#include "NMesh.h" /* we create NMesh.NMVert objects */
#include "Ipo.h"
#include "BezTriple.h"

#include "constant.h"
#include "gen_utils.h"

#define KEY_TYPE_MESH    0
#define KEY_TYPE_CURVE   1
#define KEY_TYPE_LATTICE 2

/* macro from blenkernel/intern/key.c:98 */
#define GS(a)	(*((short *)(a)))

static void Key_dealloc( PyObject * self );
static PyObject *Key_getattr( PyObject * self, char *name );
static void KeyBlock_dealloc( PyObject * self );
static PyObject *KeyBlock_getattr( PyObject * self, char *name );
static PyObject *Key_repr( BPy_Key * self );

PyTypeObject Key_Type = {
	PyObject_HEAD_INIT( NULL ) 0,	/*ob_size */
	"Blender Key",	/*tp_name */
	sizeof( BPy_Key ),	/*tp_basicsize */
	0,			/*tp_itemsize */
	/* methods */
	( destructor ) Key_dealloc,	/*tp_dealloc */
	( printfunc ) 0,	/*tp_print */
	( getattrfunc ) Key_getattr,	/*tp_getattr */
	( setattrfunc ) 0,	/*tp_setattr */
	0, /*tp_compare*/
	( reprfunc ) Key_repr, /* tp_repr */
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

PyTypeObject KeyBlock_Type = {
	PyObject_HEAD_INIT( NULL ) 0,	/*ob_size */
	"Blender KeyBlock",	/*tp_name */
	sizeof( BPy_KeyBlock ),	/*tp_basicsize */
	0,			/*tp_itemsize */
	/* methods */
	( destructor ) KeyBlock_dealloc,	/*tp_dealloc */
	( printfunc ) 0,	/*tp_print */
	( getattrfunc ) KeyBlock_getattr,	/*tp_getattr */
	( setattrfunc ) 0,	/*tp_setattr */
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

static PyObject *Key_getBlocks( PyObject * self );
static PyObject *Key_getType( PyObject * self );
static PyObject *Key_getIpo( PyObject * self );
static PyObject *Key_getValue( PyObject * self );

static struct PyMethodDef Key_methods[] = {
	{ "getType", (PyCFunction) Key_getType, METH_NOARGS, "Get key type" },
	{ "getValue", (PyCFunction) Key_getValue, METH_NOARGS, "Get key value" },
	{ "getBlocks", (PyCFunction) Key_getBlocks, METH_NOARGS, "Get key blocks" },
	{ "getIpo", (PyCFunction) Key_getIpo, METH_NOARGS, "Get key Ipo" },
	{ 0, 0, 0, 0 }
};

static PyObject *KeyBlock_getData( PyObject * self );
static PyObject *KeyBlock_getPos( PyObject * self );

static struct PyMethodDef KeyBlock_methods[] = {
	{ "getPos", (PyCFunction) KeyBlock_getPos, METH_NOARGS,
		"Get keyblock position"},
	{ "getData", (PyCFunction) KeyBlock_getData, METH_NOARGS,
		"Get keyblock data" },
	{ 0, 0, 0, 0 }
};

static void Key_dealloc( PyObject * self )
{
	PyObject_DEL( self );
}

static PyObject *new_Key(Key * oldkey)
{
	BPy_Key *k = PyObject_NEW( BPy_Key, &Key_Type );

	if( !oldkey ) {
		k->key = 0;
	} else {
		k->key = oldkey;
	}
	return ( PyObject * ) k;
}

PyObject *Key_CreatePyObject( Key * k )
{
	BPy_Key *key = ( BPy_Key * ) new_Key( k );

	return ( PyObject * ) key;
}

static PyObject *Key_getattr( PyObject * self, char *name )
{
	BPy_Key *k = ( BPy_Key * ) self;
	if ( strcmp( name, "id" ) == 0 ) {
		return PyString_FromString( k->key->id.name );
	} else if ( strcmp( name, "type" ) == 0 ) {
		return Key_getType(self);
	} else if ( strcmp( name, "value" ) == 0 ) {
		return Key_getValue(self);
	} else if ( strcmp( name, "blocks" ) == 0 ) {
		return Key_getBlocks(self);
	} else if ( strcmp( name, "ipo" ) == 0 ) {
		return Key_getIpo(self);
	}
	return Py_FindMethod( Key_methods, ( PyObject * ) self, name );

}

static PyObject *Key_repr( BPy_Key * self )
{
	return PyString_FromFormat( "[Key \"%s\"]", self->key->id.name + 2 );
}

static PyObject *Key_getValue( PyObject * self )
{
	BPy_Key *k = ( BPy_Key * ) self;

	return PyFloat_FromDouble( k->key->curval );
}

static PyObject *Key_getIpo( PyObject * self )
{
	BPy_Key *k = ( BPy_Key * ) self;
	BPy_Ipo *new_ipo;

	if (k->key->ipo) {
		new_ipo = ( BPy_Ipo * ) PyObject_NEW( BPy_Ipo, &Ipo_Type );
		new_ipo->ipo = k->key->ipo;
		return (PyObject *) new_ipo;
	} else {
		return EXPP_incr_ret( Py_None );
	}
}

static PyObject *Key_getType( PyObject * self )
{
	BPy_Key *k = ( BPy_Key * ) self;
	int idcode;
	int type = -1;

	idcode = GS( k->key->from->name );

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

static PyObject *Key_getBlocks( PyObject * self )
{
	BPy_Key *k = ( BPy_Key * ) self;
	Key *key = k->key;
	KeyBlock *kb;
	PyObject *keyblock_object;
	PyObject *l = PyList_New( 0 );

	for (kb = key->block.first; kb; kb = kb->next) {
		
		keyblock_object =  KeyBlock_CreatePyObject( kb, key );
		PyList_Append( l, keyblock_object );
	}

	return l;
}

static void KeyBlock_dealloc( PyObject * self )
{
	PyObject_DEL( self );
}

static PyObject *new_KeyBlock( KeyBlock * oldkeyBlock, Key *parentKey)
{
	BPy_KeyBlock *kb = PyObject_NEW( BPy_KeyBlock, &KeyBlock_Type );

	kb->key = parentKey;

	if( !oldkeyBlock ) {
		kb->keyblock = 0;
	} else {
		kb->keyblock = oldkeyBlock;
	}
	return ( PyObject * ) kb;
}

PyObject *KeyBlock_CreatePyObject( KeyBlock * kb, Key *parentKey )
{
	BPy_KeyBlock *keyBlock = ( BPy_KeyBlock * ) new_KeyBlock( kb, parentKey );

	return ( PyObject * ) keyBlock;
}

static PyObject *KeyBlock_getattr( PyObject * self, char *name )
{
	if ( strcmp( name, "pos" ) == 0 ) {
		return KeyBlock_getPos(self);
	} else if ( strcmp( name, "data" ) == 0 ) {
		return KeyBlock_getData(self);
	} else if ( strcmp( name, "pos" ) == 0 ) {
		return KeyBlock_getPos(self);
	}
	return Py_FindMethod( KeyBlock_methods, ( PyObject * ) self, name );
}

static PyObject *KeyBlock_getPos( PyObject * self )
{
	BPy_KeyBlock *kb = ( BPy_KeyBlock * ) self;
	return PyFloat_FromDouble( kb->keyblock->pos );
}

static PyObject *KeyBlock_getData( PyObject * self )
{
	/* If this is a mesh key, data is an array of MVert.
	   If lattice, data is an array of BPoint
	   If curve, data is an array of BezTriple */

	BPy_KeyBlock *kb = ( BPy_KeyBlock * ) self;
	Key *key = kb->key;
	char *datap;
	int datasize;
	int idcode;
	int i;

	PyObject *l = PyList_New( kb->keyblock->totelem );

	idcode = GS( key->from->name );

	if (!kb->keyblock->data) {
		return EXPP_incr_ret( Py_None );
	}

	switch(idcode) {
	case ID_ME:

		datasize = sizeof(MVert);

		for (i=0, datap = kb->keyblock->data; i<kb->keyblock->totelem; i++) {

			BPy_NMVert *mv = PyObject_NEW( BPy_NMVert, &NMVert_Type );
			MVert *vert = (MVert *) datap;

			mv->co[0]=vert->co[0];
			mv->co[1]=vert->co[1];
			mv->co[2]=vert->co[2];
			mv->no[0] = vert->no[0] / 32767.0;
			mv->no[1] = vert->no[1] / 32767.0;
			mv->no[2] = vert->no[2] / 32767.0;

			mv->uvco[0] = mv->uvco[1] = mv->uvco[2] = 0.0;
			mv->index = i;
			mv->flag = 0;

			PyList_SetItem(l, i, ( PyObject * ) mv);

			datap += datasize;
		}
		break;

	case ID_CU:

		datasize = sizeof(BezTriple);

		for (i=0, datap = kb->keyblock->data; i<kb->keyblock->totelem; i++) {

			BezTriple *bt = (BezTriple *) datap;
			PyObject *pybt = BezTriple_CreatePyObject( bt );

			PyList_SetItem( l, i, pybt );

			datap += datasize;
		}
		break;

	case ID_LT:

		datasize = sizeof(BPoint);

		for (i=0, datap = kb->keyblock->data; i<kb->keyblock->totelem; i++) {
			/* Lacking a python class for BPoint, use a list of four floats */
			BPoint *bp = (BPoint *) datap;
			PyObject *bpoint = PyList_New( 4 );

			PyList_SetItem( bpoint, 0, PyFloat_FromDouble( bp->vec[0] ) );
			PyList_SetItem( bpoint, 1, PyFloat_FromDouble( bp->vec[1] ) );
			PyList_SetItem( bpoint, 2, PyFloat_FromDouble( bp->vec[2] ) );
			PyList_SetItem( bpoint, 3, PyFloat_FromDouble( bp->vec[3] ) );

			PyList_SetItem( l, i, bpoint );

			datap += datasize;
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
		for (key_iter = G.main->key.first; key_iter; key_iter=key_iter->id.next) {
			if  (strcmp ( key_iter->id.name + 2, name ) == 0 ) {
				return Key_CreatePyObject( key_iter );
			}
		}

		PyOS_snprintf( error_msg, sizeof( error_msg ),
			"Key \"%s\" not found", name );
		return ( EXPP_ReturnPyObjError ( PyExc_NameError, error_msg ) );
		
	}

	else {

		PyObject *keylist;

		keylist = PyList_New( BLI_countlist( &( G.main->key ) ) );

		for ( i=0, key_iter = G.main->key.first; key_iter; key_iter=key_iter->id.next, i++ ) {
			PyList_SetItem(keylist, i, Key_CreatePyObject(key_iter));
		}
		return keylist;
	}
}


struct PyMethodDef M_Key_methods[] = {
	{"Get", M_Key_Get, METH_VARARGS, "Get a key or all key names"},
	{NULL, NULL, 0, NULL}
};

PyObject *Key_Init( void )
{
	PyObject *submodule, *KeyTypes;

	Key_Type.ob_type = &PyType_Type;
	KeyBlock_Type.ob_type = &PyType_Type;
	
	submodule =
		Py_InitModule3( "Blender.Key", M_Key_methods, "Key module" );

	KeyTypes = PyConstant_New( );

	PyConstant_Insert(( BPy_constant * ) KeyTypes, "MESH", PyInt_FromLong(KEY_TYPE_MESH));
	PyConstant_Insert(( BPy_constant * ) KeyTypes, "CURVE", PyInt_FromLong(KEY_TYPE_CURVE));
	PyConstant_Insert(( BPy_constant * ) KeyTypes, "LATTICE", PyInt_FromLong(KEY_TYPE_LATTICE));

	PyModule_AddObject( submodule, "Types", KeyTypes);

	/*
	PyModule_AddIntConstant( submodule, "TYPE_MESH",    KEY_TYPE_MESH );
	PyModule_AddIntConstant( submodule, "TYPE_CURVE",   KEY_TYPE_CURVE );
	PyModule_AddIntConstant( submodule, "TYPE_LATTICE", KEY_TYPE_LATTICE );
	*/
	return submodule;
}
