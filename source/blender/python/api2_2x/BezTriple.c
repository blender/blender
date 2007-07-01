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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Jacques Guignot RIP 2005,
 *    Stephen Swaney, Ken Hughes
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "BezTriple.h" /*This must come first */
#include "DNA_ipo_types.h"

#include "MEM_guardedalloc.h"
#include "gen_utils.h"


/***************************************************************************
  Python API function prototypes for the BezTriple module.                  
***************************************************************************/
static PyObject *M_BezTriple_New( PyObject * self, PyObject * args );
static PyObject *M_BezTriple_Get( PyObject * self, PyObject * args );

/*************************************
 Doc strings for the BezTriple module
*************************************/

static char M_BezTriple_doc[] = "The Blender BezTriple module\n";

/****************************************************************************
  Python BPy_BezTriple instance methods declarations:                        
****************************************************************************/
static PyObject *BezTriple_oldsetPoints( BPy_BezTriple * self, PyObject * args );
static int BezTriple_setPoints( BPy_BezTriple * self, PyObject * args );
static PyObject *BezTriple_getPoints( BPy_BezTriple * self );
static PyObject *BezTriple_getTriple( BPy_BezTriple * self );

/****************************************************************************
 Python method structure definition for Blender.BezTriple module:          
****************************************************************************/

struct PyMethodDef M_BezTriple_methods[] = {
	{"New", ( PyCFunction ) M_BezTriple_New, METH_VARARGS | METH_KEYWORDS,
	 0},
/*	{"New", ( PyCFunction ) M_BezTriple_New, METH_O, 0}, */
	{"Get", M_BezTriple_Get, METH_VARARGS, 0},
	{"get", M_BezTriple_Get, METH_VARARGS, 0},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python BPy_BezTriple methods table:                                        */
/*****************************************************************************/
static PyMethodDef BPy_BezTriple_methods[] = {
	/* name, method, flags, doc */
	{"setPoints", ( PyCFunction ) BezTriple_oldsetPoints, METH_VARARGS,
	 "(str) - Change BezTriple point coordinates"},
	{"getPoints", ( PyCFunction ) BezTriple_getPoints, METH_NOARGS,
	 "() - return BezTriple knot point x and y coordinates"},
	{"getTriple", ( PyCFunction ) BezTriple_getTriple, METH_NOARGS,
	 "() - return list of 3 floating point triplets.  order is H1, knot, H2"},
	{NULL, NULL, 0, NULL}
};

/****************************************************************************
 Function:              M_BezTriple_New                                   
 Python equivalent:     Blender.BezTriple.New                             
****************************************************************************/

static PyObject *M_BezTriple_New( PyObject* self, PyObject * args )
{
	float numbuf[9];
	PyObject* in_args = NULL;
	int length;

	/* accept list, tuple, or 3 or 9 args (which better be floats) */

	length = PyTuple_Size( args );
	if( length == 3 || length == 9 )
		in_args = args;
	else if( !PyArg_ParseTuple( args, "|O", &in_args) )
		goto TypeError;

	if( !in_args ) {
		numbuf[0] = 0.0f; numbuf[1] = 0.0f; numbuf[2] = 0.0f;
		numbuf[3] = 0.0f; numbuf[4] = 0.0f; numbuf[5] = 0.0f;
		numbuf[6] = 0.0f; numbuf[7] = 0.0f; numbuf[8] = 0.0f;
	} else {
		int i, length;
		if( !PySequence_Check( in_args ) )
			goto TypeError;

		length = PySequence_Length( in_args );
		if( length != 9 && length != 3 )
			goto TypeError;
		
		for( i = 0; i < length; i++ ) {
			PyObject *item, *pyfloat;
			item = PySequence_ITEM( in_args, i);
			if( !item )
				goto TypeError;
			pyfloat = PyNumber_Float( item );
			Py_DECREF( item );
			if( !pyfloat )
				goto TypeError;
			numbuf[i] = ( float )PyFloat_AS_DOUBLE( pyfloat );
			Py_DECREF( pyfloat );
		}

		if( length == 3 ) {
			numbuf[3] = numbuf[0]; numbuf[6] = numbuf[0];
			numbuf[4] = numbuf[1]; numbuf[7] = numbuf[1];
			numbuf[5] = numbuf[2]; numbuf[8] = numbuf[2];
		}
	}

	return newBezTriple( numbuf );

TypeError:
	return EXPP_ReturnPyObjError( PyExc_TypeError,
			"expected sequence of 3 or 9 floats or nothing" );
}

/****************************************************************************
 Function:              M_BezTriple_Get                                   
 Python equivalent:     Blender.BezTriple.Get                             
 Description:           Receives a string and returns the ipo data obj  
                        whose name matches the string.  If no argument is  
                        passed in, a list of all ipo data names in the  
                        current scene is returned.                         
****************************************************************************/
static PyObject *M_BezTriple_Get( PyObject * self, PyObject * args )
{
	return 0;
}

/****************************************************************************
 Function:    BezTriple_dealloc                                            
 Description: This is a callback function for the BPy_BezTriple type. It is  
              the destructor function.                                     
****************************************************************************/
static void BezTriple_dealloc( BPy_BezTriple * self )
{
	if( self->own_memory)
		MEM_freeN( self->beztriple );
	
	PyObject_DEL( self );
}

/*
 * BezTriple_getTriple
 * 
 * Get the coordinate data for a BezTriple.  Returns a list of 3 points.
 * List order is handle1, knot, handle2.  each point consists of a list
 * of x,y,z float values.
 */

static PyObject *BezTriple_getTriple( BPy_BezTriple * self )
{
	BezTriple *bezt = self->beztriple;
	return Py_BuildValue( "[[fff][fff][fff]]",
				       bezt->vec[0][0], bezt->vec[0][1], bezt->vec[0][2],
				       bezt->vec[1][0], bezt->vec[1][1], bezt->vec[1][2],
				       bezt->vec[2][0], bezt->vec[2][1], bezt->vec[2][2] );
}

/*
 * BezTriple_setTriple
 *
 * Set the cordinate data for a BezTriple.  Takes a sequence of 3 points,
 * of the same format at BezTriple_getTriple.
 */

static int BezTriple_setTriple( BPy_BezTriple * self, PyObject * args )
{
	int i, j;
	struct BezTriple *bezt = self->beztriple;
	float vec[3][3];

	if( !PySequence_Check( args ) || PySequence_Size( args ) != 3 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected three sequences of three floats" );

	for( i = 0; i < 3; i++ ) {
		PyObject *obj1 = PySequence_ITEM( args, i );
		if( !PySequence_Check( obj1 ) || PySequence_Size( obj1 ) != 3 ) {
			Py_DECREF( obj1 );
			return EXPP_ReturnIntError( PyExc_TypeError,
					"expected three sequences of three floats" );
		}
		for( j = 0; j < 3; j++ ) {
			PyObject *obj2 = PySequence_ITEM( obj1, j );
			PyObject *num = PyNumber_Float( obj2 );
			Py_DECREF( obj2 );

			if( !num ) {
				Py_DECREF( obj1 );
				return EXPP_ReturnIntError( PyExc_ValueError,
						"expected float parameter" );
			}
			vec[i][j] = ( float )PyFloat_AsDouble( num );
			Py_DECREF( num );
		}
		Py_DECREF( obj1 );
	}

	for( i = 0; i < 3; i++ )
		for( j = 0; j < 3; j++ )
			bezt->vec[i][j] = vec[i][j];

	return 0;
}

/*
 * BezTriple_getPoint
 * 
 * Get the coordinate data for a BezTriple.  Returns the control point,
 * as a list of x,y float values.
 */

static PyObject *BezTriple_getPoints( BPy_BezTriple * self )
{
	BezTriple *bezt = self->beztriple;
	return Py_BuildValue( "[ff]", bezt->vec[1][0], bezt->vec[1][1] );
}

/*
 * BezTriple_setPoint
 * 
 * Set the coordinate data for a BezTriple.  Accepts the x,y for the control
 * point and builds handle values based on control point.
 */

static int BezTriple_setPoints( BPy_BezTriple * self, PyObject * args )
{
	int i;
	struct BezTriple *bezt = self->beztriple;
	float vec[2];

	if( !PySequence_Check( args ) || PySequence_Size( args ) != 2 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected sequence of two floats" );

	for( i = 0; i < 2; i++ ) {
		PyObject *obj = PySequence_ITEM( args, i );
		PyObject *num = PyNumber_Float( obj );
		Py_DECREF( obj );

		if( !num )
			return EXPP_ReturnIntError( PyExc_ValueError,
					"expected float parameter" );
		vec[i] = ( float )PyFloat_AsDouble( num );
		Py_DECREF( num );
	}

	for( i = 0; i < 2; i++ ) {
		bezt->vec[0][i] = vec[i] - 1;
		bezt->vec[1][i] = vec[i];
		bezt->vec[2][i] = vec[i] + 1;
	}

	/* experimental fussing with handles - ipo.c: calchandles_ipocurve */
	if( bezt->vec[0][0] > bezt->vec[1][0] )
		bezt->vec[0][0] = bezt->vec[1][0];

	if( bezt->vec[2][0] < bezt->vec[1][0] )
		bezt->vec[2][0] = bezt->vec[1][0];

	return 0;
}

static PyObject *BezTriple_getTilt( BPy_BezTriple * self )
{
	return PyFloat_FromDouble( self->beztriple->alfa );
}

static int BezTriple_setTilt( BPy_BezTriple * self, PyObject *value )
{
	PyObject *num = PyNumber_Float( value );

	if( !num )
		return EXPP_ReturnIntError( PyExc_TypeError, "expected a float" );

	self->beztriple->alfa = (float)PyFloat_AsDouble( num );
	Py_DECREF( num );
	return 0;
}

static PyObject *BezTriple_getWeight( BPy_BezTriple * self )
{
	return PyFloat_FromDouble( self->beztriple->weight );
}

static int BezTriple_setWeight( BPy_BezTriple * self, PyObject *value )
{
	PyObject *num = PyNumber_Float( value );

	if( !num )
		return EXPP_ReturnIntError( PyExc_TypeError, "expected a float" );

	self->beztriple->weight = (float)PyFloat_AsDouble( num );
	Py_DECREF( num );
	return 0;
}

static PyObject *BezTriple_getRadius( BPy_BezTriple * self )
{
	return PyFloat_FromDouble( self->beztriple->radius );
}

static int BezTriple_setRadius( BPy_BezTriple * self, PyObject *value )
{
	PyObject *num = PyNumber_Float( value );

	if( !num )
		return EXPP_ReturnIntError( PyExc_TypeError, "expected a float" );

	self->beztriple->radius = (float)PyFloat_AsDouble( num );
	Py_DECREF( num );
	return 0;
}

static PyObject *BezTriple_getHide( BPy_BezTriple * self )
{
	return PyInt_FromLong( self->beztriple->hide == IPO_BEZ );
}

static int BezTriple_setHide( BPy_BezTriple * self, PyObject *value )
{
	if( PyObject_IsTrue( value ) )
		self->beztriple->hide = IPO_BEZ;
	else
		self->beztriple->hide = 0;
	return 0;
}

static PyObject *BezTriple_getSelects( BPy_BezTriple * self )
{
	BezTriple *bezt = self->beztriple;

	return Py_BuildValue( "[iii]", bezt->f1, bezt->f2, bezt->f3 );
}

static int BezTriple_setSelects( BPy_BezTriple * self, PyObject *args )
{
	struct BezTriple *bezt = self->beztriple;
	PyObject *ob1, *ob2, *ob3;

       /* only accept a sequence of three booleans */

	if( !PySequence_Check( args ) || PySequence_Size( args ) != 3 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected sequence of three integers" );

	ob1 = PySequence_ITEM( args, 0 );
	ob2 = PySequence_ITEM( args, 1 );
	ob3 = PySequence_ITEM( args, 2 );

       /* assign the selects */
	bezt->f1 = ( char )PyObject_IsTrue( ob1 );
	bezt->f2 = ( char )PyObject_IsTrue( ob2 );
	bezt->f3 = ( char )PyObject_IsTrue( ob3 );

	Py_DECREF( ob1 );
	Py_DECREF( ob2 );
	Py_DECREF( ob3 );

	return 0;
}

static PyObject *BezTriple_getHandles( BPy_BezTriple * self )
{
	BezTriple *bezt = self->beztriple;

	return Py_BuildValue( "[ii]", bezt->h1, bezt->h2 );
}

static int BezTriple_setHandles( BPy_BezTriple * self, PyObject *args )
{
	struct BezTriple *bezt = self->beztriple;
	PyObject *ob1, *ob2;
	short h1, h2;

       /* only accept a sequence of two ints */

	if( !PySequence_Check( args ) || PySequence_Size( args ) != 2 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected sequence of two integers" );

	ob1 = PySequence_ITEM( args, 0 );
	ob2 = PySequence_ITEM( args, 1 );

	if( !PyInt_Check( ob1 ) || !PyInt_Check( ob2 ) ) {
		Py_DECREF( ob1 );
		Py_DECREF( ob2 );
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected sequence of two integers" );
	}

	h1 = ( short ) PyInt_AsLong( ob1 );
	h2 = ( short ) PyInt_AsLong( ob2 );
	Py_DECREF( ob1 );
	Py_DECREF( ob2 );

	if( h1 < HD_FREE || h2 < HD_FREE ||
			h1 > HD_AUTO_ANIM || h2 > HD_AUTO_ANIM )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"expected int in range [0,4]" );

       /* assign the handles */

	bezt->h1 = h1;
	bezt->h2 = h2;

	return 0;
}

/*
 * Python BezTriple attributes get/set structure
 */

static PyGetSetDef BPy_BezTriple_getseters[] = {
	{"pt",
	 (getter)BezTriple_getPoints, (setter)BezTriple_setPoints,
	 "point knot values",
	 NULL},
	{"vec",
	 (getter)BezTriple_getTriple, (setter)BezTriple_setTriple,
	 "point handle and knot values",
	 NULL},
	{"tilt",
	 (getter)BezTriple_getTilt, (setter)BezTriple_setTilt,
	 "point tilt",
	 NULL},
	{"hide",
	 (getter)BezTriple_getHide, (setter)BezTriple_setHide,
	 "point hide status",
	 NULL},
	{"selects",
	 (getter)BezTriple_getSelects, (setter)BezTriple_setSelects,
	 "point select statuses",
	 NULL},
	{"handleTypes",
	 (getter)BezTriple_getHandles, (setter)BezTriple_setHandles,
	 "point handle types",
	 NULL},
	{"weight",
	 (getter)BezTriple_getWeight, (setter)BezTriple_setWeight,
	 "point weight",
	 NULL},
	{"radius",
	 (getter)BezTriple_getRadius, (setter)BezTriple_setRadius,
	 "point radius",
	 NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/*****************************************************************************/
/* Function:    BezTriple_repr                                               */
/* Description: This is a callback function for the BPy_BezTriple type. It   */
/*              builds a meaninful string to represent BezTriple objects.    */
/*****************************************************************************/
static PyObject *BezTriple_repr( BPy_BezTriple * self )
{
	char str[512];
	sprintf( str,
		"[BezTriple [%.6f, %.6f, %.6f] [%.6f, %.6f, %.6f] [%.6f, %.6f, %.6f]\n",
		 self->beztriple->vec[0][0], self->beztriple->vec[0][1], self->beztriple->vec[0][2],
		 self->beztriple->vec[1][0], self->beztriple->vec[1][1], self->beztriple->vec[1][2],
		 self->beztriple->vec[2][0], self->beztriple->vec[2][1], self->beztriple->vec[2][2]);
	return PyString_FromString( str );
}

/************************************************************************
 *
 * Python BezTriple_Type structure definition
 *
 ************************************************************************/

PyTypeObject BezTriple_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"BezTriple",                /* char *tp_name; */
	sizeof( BPy_BezTriple ),    /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) BezTriple_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	( reprfunc ) BezTriple_repr,     /* reprfunc tp_repr; */

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
	BPy_BezTriple_methods,      /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_BezTriple_getseters,    /* struct PyGetSetDef *tp_getset; */
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

static PyObject *M_BezTriple_HandleDict( void )
{
	PyObject *HM = PyConstant_New(  );

	if( HM ) {
		BPy_constant *d = ( BPy_constant * ) HM;

		PyConstant_Insert( d, "FREE", PyInt_FromLong( HD_FREE ) );
		PyConstant_Insert( d, "AUTO", PyInt_FromLong( HD_AUTO ) );
		PyConstant_Insert( d, "VECT", PyInt_FromLong( HD_VECT ) );
		PyConstant_Insert( d, "ALIGN", PyInt_FromLong( HD_ALIGN ) );
		PyConstant_Insert( d, "AUTOANIM", PyInt_FromLong( HD_AUTO_ANIM ) );
	}
	return HM;
}

/*
  BezTriple_Init
*/

PyObject *BezTriple_Init( void )
{
	PyObject *submodule;
	PyObject *HandleTypes = M_BezTriple_HandleDict( );

	if( PyType_Ready( &BezTriple_Type ) < 0 )
		return NULL;

	submodule = Py_InitModule3( "Blender.BezTriple",
								M_BezTriple_methods,
								M_BezTriple_doc );
	if( HandleTypes )
		PyModule_AddObject( submodule, "HandleTypes", HandleTypes );


	return submodule;
}

/* Three Python BezTriple_Type helper functions needed by the Object module: */

/****************************************************************************
 Function:    BezTriple_CreatePyObject                                    
 Description: This function will create a new BPy_BezTriple from an existing 
              Blender ipo structure.                                       
****************************************************************************/
PyObject *BezTriple_CreatePyObject( BezTriple * bzt )
{
	BPy_BezTriple *pybeztriple;

	pybeztriple =
		( BPy_BezTriple * ) PyObject_NEW( BPy_BezTriple, &BezTriple_Type );

	if( !pybeztriple )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create BPy_BezTriple object" );

	pybeztriple->beztriple = bzt;
	pybeztriple->own_memory = 0;

	return ( PyObject * ) pybeztriple;
}


/*****************************************************************************/
/* Function:    BezTriple_FromPyObject                                      */
/* Description: This function returns the Blender beztriple from the given   */
/*              PyObject.                                                    */
/*****************************************************************************/
BezTriple *BezTriple_FromPyObject( PyObject * pyobj )
{
	return ( ( BPy_BezTriple * ) pyobj )->beztriple;
}


/*
  Create a new BezTriple
  input args is a sequence - either 3 or 9 floats
*/

PyObject *newBezTriple( float *numbuf )
{
	int i, j, num;
	PyObject *pyobj = NULL;
	BezTriple *bzt = NULL;

	/* create our own beztriple data */
	bzt = MEM_callocN( sizeof( BezTriple ), "new bpytriple");

	/* check malloc */
	if( !bzt )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					       "MEM_callocN failed");

	/* copy the data */
	num = 0;
	for( i = 0; i < 3; i++ ) {
		for( j = 0; j < 3; j++) {
			bzt->vec[i][j] = numbuf[num++];
		}
	}
	bzt->h1 = HD_ALIGN;
	bzt->h2 = HD_ALIGN;

	/* wrap it */
	pyobj = BezTriple_CreatePyObject( bzt );

  	/* we own it. must free later */
	( ( BPy_BezTriple * )pyobj)->own_memory = 1;

	return pyobj;
}

/* #####DEPRECATED###### */

static PyObject *BezTriple_oldsetPoints( BPy_BezTriple * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args,
			(setter)BezTriple_setPoints );
}
