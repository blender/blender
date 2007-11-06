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
 * Contributor(s): Willian P. Germano
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include "charRGBA.h" /*This must come first */
#include "gen_utils.h"

/* This file is heavily based on the old bpython Constant object code in
	 Blender */

/*****************************************************************************/
/* Python charRGBA_Type callback function prototypes:			  */
/*****************************************************************************/
static PyObject *charRGBA_repr( BPy_charRGBA * self );

static int charRGBALength( BPy_charRGBA * self );

static PyObject *charRGBASubscript( BPy_charRGBA * self, PyObject * key );
static int charRGBAAssSubscript( BPy_charRGBA * self, PyObject * who,
				 PyObject * cares );

static PyObject *charRGBAItem( BPy_charRGBA * self, int i );
static int charRGBAAssItem( BPy_charRGBA * self, int i, PyObject * ob );
static PyObject *charRGBASlice( BPy_charRGBA * self, int begin, int end );
static int charRGBAAssSlice( BPy_charRGBA * self, int begin, int end,
			     PyObject * seq );
static PyObject *charRGBA_getColor( BPy_charRGBA * self, void * type);
static int charRGBA_setColor( BPy_charRGBA * self, PyObject * value, void * type);

/*****************************************************************************/
/* Python charRGBA_Type Mapping Methods table:			*/
/*****************************************************************************/
static PyMappingMethods charRGBAAsMapping = {
	( inquiry ) charRGBALength,	/* mp_length                            */
	( binaryfunc ) charRGBASubscript,	/* mp_subscript                 */
	( objobjargproc ) charRGBAAssSubscript,	/* mp_ass_subscript */
};

/*****************************************************************************/
/* Python charRGBA_Type Sequence Methods table:			*/
/*****************************************************************************/
static PySequenceMethods charRGBAAsSequence = {
	( inquiry ) charRGBALength,	/* sq_length */
	( binaryfunc ) 0,	/* sq_concat */
	( intargfunc ) 0,	/* sq_repeat */
	( intargfunc ) charRGBAItem,	/* sq_item */
	( intintargfunc ) charRGBASlice,	/* sq_slice */
	( intobjargproc ) charRGBAAssItem,	/* sq_ass_item */
	( intintobjargproc ) charRGBAAssSlice,	/* sq_ass_slice       */
};

static PyGetSetDef charRGBA_getseters[] = {
	{"R",
	 (getter)charRGBA_getColor, (setter)charRGBA_setColor,
	 "the red component",
	 (void *) 0},
	{"r",
	 (getter)charRGBA_getColor, (setter)charRGBA_setColor,
	 "the red component",
	 (void *) 0},
	{"G",
	 (getter)charRGBA_getColor, (setter)charRGBA_setColor,
	 "the green component",
	 (void *) 1},
	{"g",
	 (getter)charRGBA_getColor, (setter)charRGBA_setColor,
	 "the green component",
	 (void *) 1},
	{"B",
	 (getter)charRGBA_getColor, (setter)charRGBA_setColor,
	 "the blue component",
	 (void *) 2},
	{"b",
	 (getter)charRGBA_getColor, (setter)charRGBA_setColor,
	 "the blue component",
	 (void *) 2},
	{"A",
	 (getter)charRGBA_getColor, (setter)charRGBA_setColor,
	 "the alpha component",
	 (void *) 3},
	{"a",
	 (getter)charRGBA_getColor, (setter)charRGBA_setColor,
	 "the alpha component",
	 (void *) 3},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/*****************************************************************************/
/* Python charRGBA_Type structure definition:				*/
/*****************************************************************************/
PyTypeObject charRGBA_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"charRGBA",                 /* tp_name */
	sizeof( BPy_charRGBA ),     /* tp_basicsize */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	NULL,			            /* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	( reprfunc ) charRGBA_repr,	/* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	&charRGBAAsSequence,	    /* PySequenceMethods *tp_as_sequence; */
	&charRGBAAsMapping,	        /* PyMappingMethods *tp_as_mapping; */

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
	charRGBA_getseters,         /* struct PyGetSetDef *tp_getset; */
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
/* Function:	charRGBA_New	                                        */
/*****************************************************************************/
PyObject *charRGBA_New( char *rgba )
{
	BPy_charRGBA *charRGBA = NULL;

	/*
	 * When called the first time, charRGBA_Type.tp_dealloc will be NULL.
	 * If that's the case, initialize the PyTypeObject.  If the
	 * initialization succeeds, then create a new object.
	 */

	if( charRGBA_Type.tp_dealloc || PyType_Ready( &charRGBA_Type ) >= 0 ) {
		charRGBA = ( BPy_charRGBA * ) PyObject_NEW( BPy_charRGBA,
				&charRGBA_Type );
	}

	if( charRGBA == NULL )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create charRGBA object" );

	/* rgba is a pointer to the first item of a char[4] array */
	charRGBA->rgba[0] = &rgba[0];
	charRGBA->rgba[1] = &rgba[1];
	charRGBA->rgba[2] = &rgba[2];
	charRGBA->rgba[3] = &rgba[3];

	return ( PyObject * ) charRGBA;
}

/*****************************************************************************/
/* Functions:	 charRGBA_getCol and charRGBA_setCol         	 */
/* Description:	 These functions get/set rgba color triplet values.	The  */
/*		 get function returns a tuple, the set one accepts three     */
/*		 chars (separated or in a tuple) as arguments.		    */
/*****************************************************************************/
PyObject *charRGBA_getCol( BPy_charRGBA * self )
{
	PyObject *list = PyList_New( 4 );

	if( !list )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create PyList" );

	PyList_SET_ITEM( list, 0, PyInt_FromLong( *(self->rgba[0])) );
	PyList_SET_ITEM( list, 1, PyInt_FromLong( *(self->rgba[1])) );
	PyList_SET_ITEM( list, 2, PyInt_FromLong( *(self->rgba[2])) );
	PyList_SET_ITEM( list, 3, PyInt_FromLong( *(self->rgba[3])) );
	return list;
}

PyObject *charRGBA_setCol( BPy_charRGBA * self, PyObject * args )
{
	int ok;
	char r = 0, g = 0, b = 0, a = 0;

	if( PyObject_Length( args ) == 4 )
		ok = PyArg_ParseTuple( args, "bbbb", &r, &g, &b, &a );

	else
		ok = PyArg_ParseTuple( args, "|(bbbb)", &r, &g, &b, &a );

	if( !ok )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected 1-byte ints [b,b,b,b] or b,b,b,b as arguments (or nothing)" );

	*( self->rgba[0] ) = (char)EXPP_ClampInt( r, 0, 255 );
	*( self->rgba[1] ) = (char)EXPP_ClampInt( g, 0, 255 );
	*( self->rgba[2] ) = (char)EXPP_ClampInt( b, 0, 255 );
	*( self->rgba[3] ) = (char)EXPP_ClampInt( a, 0, 255 );

	return EXPP_incr_ret( Py_None );
}

/* return color value for one of the components */

static PyObject *charRGBA_getColor( BPy_charRGBA * self, void * type)
{
	int index = ((long)type) & 3; 
	return PyInt_FromLong ( *self->rgba[index] );
}

/* sets the color value of one of the components */

static int charRGBA_setColor( BPy_charRGBA * self, PyObject * value,
		void * type)
{
	int index = ((long)type) & 3; 
	PyObject *num = PyNumber_Int( value );

	/* argument must be a number */
	if( !num )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected char argument" );

	/* clamp valut to 0..255 then assign */
	*self->rgba[index] = (char)EXPP_ClampInt( (int)PyInt_AS_LONG(value),
			0, 255 );
	Py_DECREF( num );
	return 0;
}

/*****************************************************************************/
/* Section:	 charRGBA as Mapping					 */
/*		 These functions provide code to access charRGBA objects as  */
/*		  mappings.						 */
/*****************************************************************************/
static int charRGBALength( BPy_charRGBA * self )
{
	return 4;
}

static PyObject *charRGBASubscript( BPy_charRGBA * self, PyObject * key )
{
	char *name = NULL;
	int i;

	if( PyNumber_Check( key ) )
		return charRGBAItem( self, ( int ) PyInt_AsLong( key ) );

	if( !PyArg_ParseTuple( key, "s", &name ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected int or string argument" );

	if( !strcmp( name, "R" ) || !strcmp( name, "r" ) )
		i = 0;
	else if( !strcmp( name, "G" ) || !strcmp( name, "g" ) )
		i = 1;
	else if( !strcmp( name, "B" ) || !strcmp( name, "b" ) )
		i = 2;
	else if( !strcmp( name, "A" ) || !strcmp( name, "a" ) )
		i = 3;
	else
		return EXPP_ReturnPyObjError( PyExc_AttributeError, name );

	return PyInt_FromLong( (long)(*self->rgba[i]) );
}

static int charRGBAAssSubscript( BPy_charRGBA * self, PyObject * key,
				 PyObject * v )
{
	char *name = NULL;
	int i;

	if( !PyNumber_Check( v ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
					    "value to assign must be a number" );

	if( PyNumber_Check( key ) )
		return charRGBAAssItem( self, ( int ) PyInt_AsLong( key ), v );

	if( !PyArg_Parse( key, "s", &name ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
					    "expected int or string argument" );

	if( !strcmp( name, "R" ) || !strcmp( name, "r" ) )
		i = 0;
	else if( !strcmp( name, "G" ) || !strcmp( name, "g" ) )
		i = 1;
	else if( !strcmp( name, "B" ) || !strcmp( name, "b" ) )
		i = 2;
	else if( !strcmp( name, "A" ) || !strcmp( name, "a" ) )
		i = 3;
	else
		return EXPP_ReturnIntError( PyExc_AttributeError, name );

	*( self->rgba[i] ) = (char)EXPP_ClampInt( PyInt_AsLong( v ), 0, 255 );

	return 0;
}

/*****************************************************************************/
/* Section:  charRGBA as Sequence					*/
/*	     These functions provide code to access charRGBA objects as	 */
/*		 sequences.						*/
/*****************************************************************************/
static PyObject *charRGBAItem( BPy_charRGBA * self, int i )
{
	if( i < 0 || i >= 4 )
		return EXPP_ReturnPyObjError( PyExc_IndexError,
					      "array index out of range" );

	return PyInt_FromLong( *(self->rgba[i]) );
}

static PyObject *charRGBASlice( BPy_charRGBA * self, int begin, int end )
{
	PyObject *list;
	int count;

	if( begin < 0 )
		begin = 0;
	if( end > 4 )
		end = 4;
	if( begin > end )
		begin = end;

	list = PyList_New( end - begin );

	for( count = begin; count < end; count++ )
		PyList_SetItem( list, count - begin,
				PyInt_FromLong( *( self->rgba[count] ) ) );

	return list;
}

static int charRGBAAssItem( BPy_charRGBA * self, int i, PyObject * ob )
{
	if( i < 0 || i >= 4 )
		return EXPP_ReturnIntError( PyExc_IndexError,
					    "array assignment index out of range" );

	if( !PyNumber_Check( ob ) )
		return EXPP_ReturnIntError( PyExc_IndexError,
					    "color component must be a number" );

	*( self->rgba[i] ) = (char)EXPP_ClampInt( PyInt_AsLong( ob ), 0, 255 );

	return 0;
}

static int charRGBAAssSlice( BPy_charRGBA * self, int begin, int end,
			     PyObject * seq )
{
	int count;

	if( begin < 0 )
		begin = 0;
	if( end > 4 )
		end = 4;
	if( begin > end )
		begin = end;

	if( !PySequence_Check( seq ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
					    "illegal argument type for built-in operation" );

	if( PySequence_Length( seq ) != ( end - begin ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
					    "size mismatch in slice assignment" );

	for( count = begin; count < end; count++ ) {
		char value;
		PyObject *ob = PySequence_GetItem( seq, count );

		if( !PyArg_Parse( ob, "b", &value ) ) {
			Py_DECREF( ob );
			return -1;
		}

		*( self->rgba[count] ) = (char)EXPP_ClampInt( value, 0, 255 );

		Py_DECREF( ob );
	}

	return 0;
}

/*****************************************************************************/
/* Function:	charRGBA_repr						*/
/* Description: This is a callback function for the BPy_charRGBA type. It  */
/*		builds a meaninful string to represent charRGBA objects.   */
/*****************************************************************************/
static PyObject *charRGBA_repr( BPy_charRGBA * self )
{
	char r, g, b, a;

	r = *( self->rgba[0] );
	g = *( self->rgba[1] );
	b = *( self->rgba[2] );
	a = *( self->rgba[3] );

	return PyString_FromFormat( "[%d, %d, %d, %d]", r, g, b, a );
}
