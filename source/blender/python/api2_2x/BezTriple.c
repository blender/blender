/*
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
 * This is a new part of Blender.
 *
 * Contributor(s): Jacques Guignot RIP 2005,
 *    Stephen Swaney
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "BezTriple.h" /*This must come first */

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
static PyObject *BezTriple_setPoints( BPy_BezTriple * self, PyObject * args );
static PyObject *BezTriple_getPoints( BPy_BezTriple * self );
static PyObject *BezTriple_getTriple( BPy_BezTriple * self );

/****************************************************************************
 Python BezTriple_Type callback function prototypes:                      
*****************************************************************************/
static void BezTripleDeAlloc( BPy_BezTriple * self );
static int BezTripleSetAttr( BPy_BezTriple * self, char *name, PyObject * v );
static PyObject *BezTripleGetAttr( BPy_BezTriple * self, char *name );
static PyObject *BezTripleRepr( BPy_BezTriple * self );
//static PyObject *BezTriple_Str( BPy_BezTriple * self );

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
	{"setPoints", ( PyCFunction ) BezTriple_setPoints, METH_VARARGS,
	 "(str) - Change BezTriple point coordinates"},
	{"getPoints", ( PyCFunction ) BezTriple_getPoints, METH_NOARGS,
	 "() - return BezTriple knot point x and y coordinates"},
	{"getTriple", ( PyCFunction ) BezTriple_getTriple, METH_NOARGS,
	 "() - return list of 3 floating point triplets.  order is H1, knot, H2"},
	{NULL, NULL, 0, NULL}
};


/*****************************************************************************/
/* Python BezTriple_Type structure definition:                              */
/*****************************************************************************/
PyTypeObject BezTriple_Type = {
	PyObject_HEAD_INIT( NULL )	/*    required python macro            */
		0,		/* ob_size */
	"BezTriple",		/* tp_name */
	sizeof( BPy_BezTriple ),	/* tp_basicsize */
	0,			/* tp_itemsize */
	/* methods */
	( destructor ) BezTripleDeAlloc,	/* tp_dealloc */
	0,			/* tp_print */
	( getattrfunc ) BezTripleGetAttr,	/* tp_getattr */
	( setattrfunc ) BezTripleSetAttr,	/* tp_setattr */
	0,			/* tp_compare */
	( reprfunc ) BezTripleRepr,	/* tp_repr */
	0,			/* tp_as_number */
	0,			/* tp_as_sequence */
	0,			/* tp_as_mapping */
	0,			/* tp_as_hash */
	0,			/* tp_call */
	0,  /*  ( reprfunc) BezTriple_Str,	 tp_str */
	0,			/* tp_getattro */
	0,			/* tp_setattro */
	0,			/* tp_as_buffer */
	0,                      /* tp_flags */
	0,			/* tp_doc */
	0, 0, 0, 0, 0, 0,
	BPy_BezTriple_methods,	/* tp_methods */
	0,			/* tp_members */
	0,			/* tm_getset */
	0
};


/****************************************************************************
 Function:              M_BezTriple_New                                   
 Python equivalent:     Blender.BezTriple.New                             
****************************************************************************/

static PyObject *M_BezTriple_New( PyObject* self, PyObject * args )
{
	PyObject* in_args = NULL;

	if( !PyArg_ParseTuple( args, "|O", &in_args) ) {
		return( EXPP_ReturnPyObjError
				( PyExc_AttributeError,
				  "expected sequence of 3 or 9 floats or nothing"));
	}

	if( !in_args ) {
		in_args = Py_BuildValue( "(fff)", 0.0, 0.0, 0.0 );
	}
		
	return newBezTriple( in_args );
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
 Function:    BezTripleDeAlloc                                            
 Description: This is a callback function for the BPy_BezTriple type. It is  
              the destructor function.                                     
****************************************************************************/
static void BezTripleDeAlloc( BPy_BezTriple * self )
{

	if( self->own_memory)
		MEM_freeN( self->beztriple );
	
	PyObject_DEL( self );
}

static PyObject *BezTriple_getPoints( BPy_BezTriple * self )
{
	struct BezTriple *bezt = self->beztriple;
	PyObject *l = PyList_New( 0 );
	int i;
	for( i = 0; i < 2; i++ ) {
		PyList_Append( l, PyFloat_FromDouble( bezt->vec[1][i] ) );
	}
	return l;
}


/*
 * BezTriple_getTriple
 * 
 * get the coordinate data for a BezTriple.
 *  returns a list of 3 points.
 * list order is handle1, knot, handle2.
 *  each point consists of a list of x,y,z float values.
 */

static PyObject *BezTriple_getTriple( BPy_BezTriple * self )
{
	int i;
	struct BezTriple *bezt = self->beztriple;
	PyObject *retlist = PyList_New( 0 );
	PyObject *point;

	for( i = 0; i < 3; i++ ) {
		point = Py_BuildValue( "[fff]",
				       bezt->vec[i][0],
				       bezt->vec[i][1], bezt->vec[i][2] );

		PyList_Append( retlist, point );
	}

	return retlist;
}


static PyObject *BezTriple_setPoints( BPy_BezTriple * self, PyObject * args )
{

	int i;
	struct BezTriple *bezt = self->beztriple;
	PyObject *popo = 0;

	if( !PyArg_ParseTuple( args, "O", &popo ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "expected sequence argument" ) );

	if( PySequence_Check( popo ) == 0 ) {
		puts( "error in BezTriple_setPoints - expected sequence" );
		Py_INCREF( Py_None );
		return Py_None;
	}

	{
		/*
		   some debug stuff 
		   this will become an overloaded args check
		 */
		int size = PySequence_Size( popo );
		printf( "\n dbg: sequence size is %d\n", size );
	}

	for( i = 0; i < 2; i++ ) {
		PyObject *o = PySequence_GetItem( popo, i );
		if( !o )
			printf( "\n bad o. o no!\n" );

		/*   bezt->vec[1][i] = PyFloat_AsDouble (PyTuple_GetItem (popo, i)); */
		bezt->vec[1][i] = (float)PyFloat_AsDouble( o );
		bezt->vec[0][i] = bezt->vec[1][i] - 1;
		bezt->vec[2][i] = bezt->vec[1][i] + 1;
	}

	/* experimental fussing with handles - ipo.c: calchandles_ipocurve */
	if( bezt->vec[0][0] > bezt->vec[1][0] )
		bezt->vec[0][0] = bezt->vec[1][0];

	if( bezt->vec[2][0] < bezt->vec[1][0] )
		bezt->vec[2][0] = bezt->vec[1][0];

	Py_INCREF( Py_None );
	return Py_None;
}


/*****************************************************************************/
/* Function:    BezTripleGetAttr                                            */
/* Description: This is a callback function for the BPy_BezTriple type. It   */
/*              taccesses BPy_BezTriple "member variables" and    methods.    */
/*****************************************************************************/
static PyObject *BezTripleGetAttr( BPy_BezTriple * self, char *name )
{
	if( strcmp( name, "pt" ) == 0 )
		return BezTriple_getPoints( self );
	else if( strcmp( name, "vec" ) == 0 )
		return BezTriple_getTriple( self );
	else if( strcmp( name, "tilt" ) == 0 )
		return PyFloat_FromDouble(self->beztriple->alfa);
	else if( strcmp( name, "__members__" ) == 0 )
		return Py_BuildValue( "[s,s,s]", "pt", "vec", "tilt" );

	/* look for default methods */
	return Py_FindMethod( BPy_BezTriple_methods, ( PyObject * ) self, name );
}

/*****************************************************************************/
/* Function:    BezTripleSetAttr                                            */
/* Description: This is a callback function for the BPy_BezTriple type. It */
/*               sets BezTriple Data attributes (member variables).  */
/*****************************************************************************/
static int BezTripleSetAttr( BPy_BezTriple * self, char *name, PyObject * value )
{
#if 0
	/*
	   this does not work at the moment:  Wed Apr  7  2004
	   when the necessary code to make pt act like a sequence is
	   available, it will be reenabled
	 */

	if( strcmp( name, "pt" ) == 0 )
		BezTriple_setPoints( self, value );

	return 0;		/* normal exit */
#endif
	if( strcmp( name, "tilt" ) == 0 ) {
		if (!PyFloat_Check( value ) )
			return EXPP_ReturnIntError( PyExc_TypeError, "expected a float" );

		self->beztriple->alfa = (float)PyFloat_AsDouble( value );
		return 0;
	}

	return ( EXPP_ReturnIntError( PyExc_AttributeError,
				      "cannot set a read-only attribute" ) );
}

/*****************************************************************************/
/* Function:    BezTripleRepr                                                */
/* Description: This is a callback function for the BPy_BezTriple type. It     */
/*              builds a meaninful string to represent  BezTriple objects.   */
/*****************************************************************************/
static PyObject *BezTripleRepr( BPy_BezTriple * self )
{
	/*      float vec[3][3];
	   float alfa;
	   short s[3][2];
	   short h1, h2;
	   char f1, f2, f3, hide;
	 */
	char str[1000];
	sprintf( str,
		 "BezTriple %f %f %f %f %f %f %f %f %f %f\n %d %d %d %d %d %d\n",
		 self->beztriple->vec[0][0], self->beztriple->vec[0][1],
		 self->beztriple->vec[0][2], self->beztriple->vec[1][0],
		 self->beztriple->vec[1][1], self->beztriple->vec[1][2],
		 self->beztriple->vec[2][0], self->beztriple->vec[2][1],
		 self->beztriple->vec[2][2], self->beztriple->alfa,
		 self->beztriple->h1, self->beztriple->h2, self->beztriple->f1,
		 self->beztriple->f2, self->beztriple->f3,
		 self->beztriple->hide );
	return PyString_FromString( str );
}


/*
 BezTriple_Str
 display object as string.
 equivalent to python str(o)
*/
/*
static PyObject *BezTriple_Str( BPy_BezTriple * self )
{
	BezTriple *p = self->beztriple;

// fixme: 
	return PyString_FromFormat(
		 "BezTriple (%f %f %f) (%f %f %f) (%f %f %f) alpha %f\n h1:%d h2:%d f1:%d f2:%d f3:%d hide:%d",
		 p->vec[0][0], p->vec[0][1],  p->vec[0][2], 
		 p->vec[1][0],  p->vec[1][1], p->vec[1][2],
		 p->vec[2][0], p->vec[2][1],  p->vec[2][2],
		 p->alfa,
		 p->h1, p->h2, p->f1,
		 p->f2, p->f3,
		 p->hide );

}
*/

/*
  BezTriple_Init
*/

PyObject *BezTriple_Init( void )
{
	PyObject *submodule;

	BezTriple_Type.ob_type = &PyType_Type;

	submodule = Py_InitModule3( "Blender.BezTriple",
								M_BezTriple_methods,
								M_BezTriple_doc );

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
/* Function:    BezTriple_CheckPyObject                                     */
/* Description: This function returns true when the given PyObject is of the */
/*              type BezTriple. Otherwise it will return false.             */
/*****************************************************************************/
int BezTriple_CheckPyObject( PyObject * pyobj )
{
	return ( pyobj->ob_type == &BezTriple_Type );
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

PyObject *newBezTriple( PyObject *args)
{
	BPy_BezTriple *pybez = NULL;
	int length;
	float numbuf[9];
	int status;
/*
  check input args:
    sequence of nine floats - x,y,z for h1, pt, h2
    sequence of 3 floats - x,y,z for pt with zero len handles in AUTO mode
*/

	/* do we have a sequence of the right length? */

	if( ! PySequence_Check( args )) {
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected better stuff"));
	}
	
	length = PySequence_Length( args );
	if( length != 9 && length != 3 )
		return  EXPP_ReturnPyObjError( PyExc_AttributeError,
					       "wrong number of points");
	{
		int i;
		if (length == 9)
			status = PyArg_ParseTuple( args, "fffffffff", 
						   &numbuf[0], 
						   &numbuf[1], 
						   &numbuf[2], 
						   &numbuf[3], 
						   &numbuf[4], 
						   &numbuf[5], 
						   &numbuf[6], 
						   &numbuf[7], 
						   &numbuf[8]);

		else if (length == 3)
			status = PyArg_ParseTuple( args, "fff",
						   &numbuf[0], 
						   &numbuf[1], 
						   &numbuf[2]);     
		else
			return EXPP_ReturnPyObjError
				( PyExc_AttributeError,
				  "wrong number of points");
		if ( !status )
			return EXPP_ReturnPyObjError
				( PyExc_AttributeError,
				  "sequence item not number");
	}

	/* create our bpy object */
	pybez = ( BPy_BezTriple* ) PyObject_New( BPy_BezTriple,
					       &BezTriple_Type );
	if( ! pybez )
		return  EXPP_ReturnPyObjError( PyExc_MemoryError,
					       "PyObject_New failed");
	pybez->beztriple = MEM_callocN( sizeof( BezTriple ), "new bpytriple");
	/* check malloc */

	pybez->own_memory = 1;  /* we own it. must free later */
	
	switch( length ) {
	case 9: {
		int i,j;
		int num = 0;
		for( i = 0; i < 3; i++ ){
			for( j = 0; j < 3; j++){
				pybez->beztriple->vec[i][j] = numbuf[num ++];
			}
		}
	}
		break;
	case 3: {
		int i;
		int num = 0;
		/* set h1, pt, and h2 to the same values. */
		for( i = 0; i < 3; i++ ) {
			pybez->beztriple->vec[0][i] = numbuf[num];
			pybez->beztriple->vec[1][i] = numbuf[num];
			pybez->beztriple->vec[2][i] = numbuf[num];
			++num;
		}
	}
		break;
	default:
		/* we should not be here! */
		break;
	}


	pybez->beztriple->h1 = HD_ALIGN;
	pybez->beztriple->h2 = HD_ALIGN;

	return ( PyObject* ) pybez;
}
