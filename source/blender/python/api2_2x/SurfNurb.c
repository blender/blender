/*
 * $Id: SurfNurb.c 11400 2007-07-28 09:26:53Z campbellbarton $
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
 * Contributor(s): Stephen Swaney
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "SurfNurb.h" /*This must come first */

#include "BKE_curve.h"
#include "BDR_editcurve.h"	/* for convertspline */
#include "MEM_guardedalloc.h"
#include "gen_utils.h"
#include "gen_library.h"
#include "BezTriple.h"

/*
 * forward declarations go here
 */

static int SurfNurb_setPoint( BPy_SurfNurb * self, int index, PyObject * ob );
static int SurfNurb_length( PyInstanceObject * inst );
static PyObject *SurfNurb_getIter( BPy_SurfNurb * self );
static PyObject *SurfNurb_iterNext( BPy_SurfNurb * self );
PyObject *SurfNurb_append( BPy_SurfNurb * self, PyObject * args );

char M_SurfNurb_doc[] = "SurfNurb";

/*
   table of module methods
   these are the equivalent of class or static methods.
   you do not need an object instance to call one.
*/

static PyMethodDef M_SurfNurb_methods[] = {
/*   name, method, flags, doc_string                */
/*  {"Get", (PyCFunction) M_SurfNurb_method, METH_NOARGS, " () - doc string"}, */
/*   {"method", (PyCFunction) M_SurfNurb_method, METH_NOARGS, " () - doc string"}, */

	{NULL, NULL, 0, NULL}
};

/*
 * method table
 * table of instance methods
 * these methods are invoked on an instance of the type.
*/

static PyMethodDef BPy_SurfNurb_methods[] = {
# if 0
	{"append", ( PyCFunction ) SurfNurb_append, METH_VARARGS,
	 "( point ) - add a new point.  arg is BezTriple or list of x,y,z,w floats"},
#endif
	{NULL, NULL, 0, NULL}
};

/*
 * SurfNurb_appendPointToNurb
 * this is a non-bpy utility func to add a point to a given nurb.
 * notice the first arg is Nurb*.
 */

#if 0
static PyObject *SurfNurb_appendPointToNurb( Nurb * nurb, PyObject * args )
{

	int i;
	int size;
	PyObject *pyOb;
	int npoints = nurb->pntsu;

	/*
	   do we have a list of four floats or a BezTriple?
	*/
	if( !PyArg_ParseTuple( args, "O", &pyOb ))
		return EXPP_ReturnPyObjError
				( PyExc_RuntimeError,
				  "Internal error parsing arguments" );



	/* if curve is empty, adjust type depending on input type */
	if (nurb->bezt==NULL && nurb->bp==NULL) {
		if (BPy_BezTriple_Check( pyOb ))
			nurb->type |= CU_BEZIER;
		else if (PySequence_Check( pyOb ))
			nurb->type |= CU_NURBS;
		else
			return( EXPP_ReturnPyObjError( PyExc_TypeError,
					  "Expected a BezTriple or a Sequence of 4 (or 5) floats" ) );
	}



	if ((nurb->type & 7)==CU_BEZIER) {
		BezTriple *tmp;

		if( !BPy_BezTriple_Check( pyOb ) )
			return( EXPP_ReturnPyObjError( PyExc_TypeError,
					  "Expected a BezTriple\n" ) );

/*		printf("\ndbg: got a BezTriple\n"); */
		tmp = nurb->bezt;	/* save old points */
		nurb->bezt =
			( BezTriple * ) MEM_mallocN( sizeof( BezTriple ) *
						     ( npoints + 1 ),
						     "SurfNurb_append2" );

		if( !nurb->bezt )
			return ( EXPP_ReturnPyObjError
				 ( PyExc_MemoryError, "allocation failed" ) );

		/* copy old points to new */
		if( tmp ) {
			memmove( nurb->bezt, tmp, sizeof( BezTriple ) * npoints );
			MEM_freeN( tmp );
		}

		nurb->pntsu++;
		/* add new point to end of list */
		memcpy( nurb->bezt + npoints,
			BezTriple_FromPyObject( pyOb ), sizeof( BezTriple ) );

	}
	else if( PySequence_Check( pyOb ) ) {
		size = PySequence_Size( pyOb );
/*		printf("\ndbg: got a sequence of size %d\n", size );  */
		if( size == 4 || size == 5 ) {
			BPoint *tmp;

			tmp = nurb->bp;	/* save old pts */

			nurb->bp =
				( BPoint * ) MEM_mallocN( sizeof( BPoint ) *
							  ( npoints + 1 ),
							  "SurfNurb_append1" );
			if( !nurb->bp )
				return ( EXPP_ReturnPyObjError
					 ( PyExc_MemoryError,
					   "allocation failed" ) );

			memmove( nurb->bp, tmp, sizeof( BPoint ) * npoints );
			if( tmp )
				MEM_freeN( tmp );

			++nurb->pntsu;
			/* initialize new BPoint from old */
			memcpy( nurb->bp + npoints, nurb->bp,
				sizeof( BPoint ) );

			for( i = 0; i < 4; ++i ) {
				PyObject *item = PySequence_GetItem( pyOb, i );

				if (item == NULL)
					return NULL;


				nurb->bp[npoints].vec[i] = ( float ) PyFloat_AsDouble( item );
				Py_DECREF( item );
			}

			if (size == 5) {
				PyObject *item = PySequence_GetItem( pyOb, i );

				if (item == NULL)
					return NULL;

				nurb->bp[npoints].alfa = ( float ) PyFloat_AsDouble( item );
				Py_DECREF( item );
			}
			else {
				nurb->bp[npoints].alfa = 0.0f;
			}

			makeknots( nurb, 1, nurb->flagu >> 1 );

		} else {
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a sequence of 4 or 5 floats" );
		}

	} else {
		/* bail with error */
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a sequence of 4 or 5 floats" );

	}

	return ( EXPP_incr_ret( Py_None ) );
}

/*
 * SurfNurb_append( point )
 * append a new point to a nurb curve.
 * arg is BezTriple or list of xyzw floats 
 */

PyObject *SurfNurb_append( BPy_SurfNurb * self, PyObject * args )
{
	Nurb *nurb = self->nurb;

	return SurfNurb_appendPointToNurb( nurb, args );
}
#endif

#if 0
/*
 * SurfNurb_getMatIndex
 *
 * returns index into material list
 */

static PyObject *SurfNurb_getMatIndex( BPy_SurfNurb * self )
{
	return PyInt_FromLong( ( long ) self->nurb->mat_nr );
}

/*
 *  SurfNurb_setMatIndex
 *
 *  set index into material list
 */

static int SurfNurb_setMatIndex( BPy_SurfNurb * self, PyObject * args )
{
	args = PyNumber_Int( args );

	if( !args )
		return EXPP_ReturnIntError( PyExc_TypeError,
			   "expected integer argument" );

	/* fixme:  some range checking would be nice! */
	/* can't do range checking without knowing the "parent" curve! */
	self->nurb->mat_nr = ( short )PyInt_AS_LONG( args );
	Py_DECREF( args );

	return 0;
}
#endif

/*
 * SurfNurb_getPointsU
 *
 * returns number of control points in U direction
 */

static PyObject *SurfNurb_getPointsU( BPy_SurfNurb * self )
{
	return PyInt_FromLong( ( long ) self->nurb->pntsu );
}

/*
 * SurfNurb_getPointsV
 *
 * returns number of control points in V direction
 */

static PyObject *SurfNurb_getPointsV( BPy_SurfNurb * self )
{
	return PyInt_FromLong( ( long ) self->nurb->pntsv );
}

/*
 * SurfNurb_getFlagU
 *
 * returns curve's flagu
 */

static PyObject *SurfNurb_getFlagU( BPy_SurfNurb * self )
{
	return PyInt_FromLong( ( long ) (self->nurb->flagu >> 1) );
}

/*
 *  SurfNurb_setFlagU
 *
 *  set curve's flagu and recalculate the knots
 *
 *  Possible values: 0 - uniform, 2 - endpoints, 4 - bezier
 *    bit 0 controls CU_CYCLIC
 */

static int SurfNurb_setFlagU( BPy_SurfNurb * self, PyObject * args )
{
	int flagu;

	args = PyNumber_Int( args );
	if( !args )
		return EXPP_ReturnIntError( PyExc_TypeError,
			   "expected integer argument" );

	flagu = ( int )PyInt_AS_LONG( args );
	Py_DECREF( args );

	if( flagu < 0 || flagu > 2 )
		return EXPP_ReturnIntError( PyExc_AttributeError,
				"expected integer argument in range [0,2]" );

	flagu = (flagu << 1) | (self->nurb->flagu & CU_CYCLIC);
	if( self->nurb->flagu != flagu ) {
		self->nurb->flagu = (short)flagu;
		makeknots( self->nurb, 1, self->nurb->flagu >> 1 );
	}

	return 0;
}

/*
 * SurfNurb_getFlagV
 *
 * returns curve's flagu
 */

static PyObject *SurfNurb_getFlagV( BPy_SurfNurb * self )
{
	return PyInt_FromLong( ( long ) (self->nurb->flagv >> 1) );
}

/*
 *  SurfNurb_setFlagV
 *
 *  set curve's flagu and recalculate the knots
 *
 *  Possible values: 0 - uniform, 1 - endpoints, 2 - bezier
 */

static int SurfNurb_setFlagV( BPy_SurfNurb * self, PyObject * args )
{
	int flagv;

	args = PyNumber_Int( args );
	if( !args )
		return EXPP_ReturnIntError( PyExc_TypeError,
			   "expected integer argument" );

	flagv = ( int )PyInt_AS_LONG( args );
	Py_DECREF( args );

	if( flagv < 0 || flagv > 2 )
		return EXPP_ReturnIntError( PyExc_AttributeError,
				"expected integer argument in range [0,2]" );

	flagv = (flagv << 1) | (self->nurb->flagv & CU_CYCLIC);
	if( self->nurb->flagv != flagv ) {
		self->nurb->flagv = (short)flagv;
		makeknots( self->nurb, 2, self->nurb->flagv >> 1 );
	}

	return 0;
}

/*
 * SurfNurb_getOrder
 *
 * returns curve's order
 */

static PyObject *SurfNurb_getOrderU( BPy_SurfNurb * self )
{
	return PyInt_FromLong( ( long ) self->nurb->orderu );
}

static int SurfNurb_setOrderU( BPy_SurfNurb * self, PyObject * args )
{
	int order;

	args = PyNumber_Int( args );
	if( !args )
		return EXPP_ReturnIntError( PyExc_TypeError,
			   "expected integer argument" );

	order = ( int )PyInt_AS_LONG( args );
	Py_DECREF( args );

	if( order < 2 ) order = 2;
	else if( order > 6 ) order = 6;

	if( self->nurb->pntsu < order )
		order = self->nurb->pntsu;

	self->nurb->orderu = (short)order;
	makeknots( self->nurb, 1, self->nurb->flagu >> 1 );

	return 0;
}

static PyObject *SurfNurb_getOrderV( BPy_SurfNurb * self )
{
	return PyInt_FromLong( ( long ) self->nurb->orderv );
}

static int SurfNurb_setOrderV( BPy_SurfNurb * self, PyObject * args )
{
	int order;

	args = PyNumber_Int( args );
	if( !args )
		return EXPP_ReturnIntError( PyExc_TypeError,
			   "expected integer argument" );

	order = ( int )PyInt_AS_LONG( args );
	Py_DECREF( args );

	if( order < 2 ) order = 2;
	else if( order > 6 ) order = 6;

	if( self->nurb->pntsv < order )
		order = self->nurb->pntsv;

	self->nurb->orderv = (short)order;
	makeknots( self->nurb, 2, self->nurb->flagv >> 1 );
	return 0;
}

/*
 * SurfNurb_getCyclic()
 * test whether surface is cyclic (closed) or not (open)
 */

static PyObject *SurfNurb_getCyclicU( BPy_SurfNurb * self )
{
	if( self->nurb->flagu & CU_CYCLIC )
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}

static PyObject *SurfNurb_getCyclicV( BPy_SurfNurb * self )
{
	if( self->nurb->flagv & CU_CYCLIC )
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}

static int SurfNurb_setCyclicU( BPy_SurfNurb * self, PyObject * value )
{
	int param = PyObject_IsTrue( value );
	if( param == -1 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected True/False or 0/1" );
	
	if( param )
		self->nurb->flagu |= CU_CYCLIC;
	else
		self->nurb->flagu &= ~CU_CYCLIC;
	makeknots( self->nurb, 1, self->nurb->flagu >> 1 );
	return 0;
}

static int SurfNurb_setCyclicV( BPy_SurfNurb * self, PyObject * value )
{
	int param = PyObject_IsTrue( value );
	if( param == -1 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected True/False or 0/1" );
	
	if( param )
		self->nurb->flagv |= CU_CYCLIC;
	else
		self->nurb->flagv &= ~CU_CYCLIC;
	makeknots( self->nurb, 2, self->nurb->flagu >> 1 );
	return 0;
}


/*
 * SurfNurb_getIter
 *
 * create an iterator for our SurfNurb.
 * this iterator returns the points for this SurfNurb.
 */

static PyObject *SurfNurb_getIter( BPy_SurfNurb * self )
{
	self->bp = self->nurb->bp;
	self->bezt = self->nurb->bezt;
	self->nextPoint = 0;

	Py_INCREF( self );
	return ( PyObject * ) self;
}

static PyObject *SurfNurb_iterNext( BPy_SurfNurb * self )
{
	Nurb *pnurb = self->nurb;
	int npoints = pnurb->pntsu * pnurb->pntsv;

	if( self->bp && self->nextPoint < npoints )
		return SurfNurb_pointAtIndex( self->nurb, self->nextPoint++ );
	else
		return EXPP_ReturnPyObjError( PyExc_StopIteration,
						"iterator at end" );
}

/*
 * SurfNurb_length
 * returns the number of points in a Nurb
 * this is a tp_as_sequence method, not a regular instance method.
 */

static int SurfNurb_length( PyInstanceObject * inst )
{
	Nurb *nurb;

	if( BPy_SurfNurb_Check( ( PyObject * ) inst ) ) {
		nurb = ( ( BPy_SurfNurb * ) inst )->nurb;
		return (int)(nurb->pntsu * nurb->pntsu);
	}

	return EXPP_ReturnIntError( PyExc_RuntimeError,
				    "arg is not a BPy_SurfNurb" );
}


/*
 * SurfNurb_getPoint
 * returns the Nth point in a Nurb
 * this is one of the tp_as_sequence methods, hence the int N argument.
 * it is called via the [] operator, not as a usual instance method.
 */

PyObject *SurfNurb_getPoint( BPy_SurfNurb * self, int index )
{
	Nurb *myNurb;

	int npoints;

	/* for convenince */
	myNurb = self->nurb;
	npoints = myNurb->pntsu * myNurb->pntsv;

	/* bail if no Nurbs in Curve */
	if( npoints == 0 )
		return ( EXPP_ReturnPyObjError( PyExc_IndexError,
						"no points in this SurfNurb" ) );

	/* check index limits */
	if( index >= npoints || index < 0 )
		return ( EXPP_ReturnPyObjError( PyExc_IndexError,
						"index out of range" ) );

	return SurfNurb_pointAtIndex( myNurb, index );
}

/*
 * SurfNurb_setPoint
 * modifies the Nth point in a Nurb
 * this is one of the tp_as_sequence methods, hence the int N argument.
 * it is called via the [] = operator, not as a usual instance method.
 */
static int SurfNurb_setPoint( BPy_SurfNurb * self, int index, PyObject * pyOb )
{
	Nurb *nurb = self->nurb;
	int size;

	/* check index limits */
	if( index < 0 || index >= nurb->pntsu * nurb->pntsv )
		return EXPP_ReturnIntError( PyExc_IndexError,
					    "array assignment index out of range\n" );

	/* branch by curve type */
#if 0
	if ((nurb->type & 7)==CU_BEZIER) {	/* BEZIER */
		/* check parameter type */
		if( !BPy_BezTriple_Check( pyOb ) )
			return EXPP_ReturnIntError( PyExc_TypeError,
							"expected a BezTriple\n" );

		/* copy bezier in array */
		memcpy( nurb->bezt + index,
			BezTriple_FromPyObject( pyOb ), sizeof( BezTriple ) );

		return 0;	/* finished correctly */
	}
	else
#endif
   	{	/* NURBS or POLY */
		int i;

		/* check parameter type */
		if (!PySequence_Check( pyOb ))
			return EXPP_ReturnIntError( PyExc_TypeError,
							"expected a list of 4 (or optionaly 5 if the curve is 3D) floats\n" );

		size = PySequence_Size( pyOb );

		/* check sequence size */
		if( size != 4 && size != 5 ) 
			return EXPP_ReturnIntError( PyExc_TypeError,
							"expected a list of 4 (or optionaly 5 if the curve is 3D) floats\n" );

		/* copy x, y, z, w */
		for( i = 0; i < 4; ++i ) {
			PyObject *item = PySequence_GetItem( pyOb, i );

			if (item == NULL)
				return -1;

			nurb->bp[index].vec[i] = ( float ) PyFloat_AsDouble( item );
			Py_DECREF( item );
		}

		if (size == 5) {	/* set tilt, if present */
			PyObject *item = PySequence_GetItem( pyOb, i );

			if (item == NULL)
				return -1;

			nurb->bp[index].alfa = ( float ) PyFloat_AsDouble( item );
			Py_DECREF( item );
		}
		else {				/* if not, set default	*/
			nurb->bp[index].alfa = 0.0f;
		}

		return 0;	/* finished correctly */
	}
}


/* 
 * this is an internal routine.  not callable directly from python
 */

PyObject *SurfNurb_pointAtIndex( Nurb * nurb, int index )
{
	PyObject *pyo;

	if( nurb->bp ) {	/* we have a nurb curve */
		int i;

		/* add Tilt only if curve is 3D */
		if (nurb->flag & CU_3D)
			pyo = PyList_New( 5 );
		else
			pyo = PyList_New( 4 );

		for( i = 0; i < 4; i++ ) {
			PyList_SetItem( pyo, i,
					PyFloat_FromDouble( nurb->bp[index].
							    vec[i] ) );
		}

		/* add Tilt only if curve is 3D */
		if (nurb->flag & CU_3D)
			PyList_SetItem( pyo, 4,	PyFloat_FromDouble( nurb->bp[index].alfa ) );
		return pyo;

	} else			/* something is horribly wrong */
		return EXPP_ReturnPyObjError( PyExc_SystemError,
						"non-NURB surface found" );
}

/* 
 *   methods for SurfNurb as sequence
 */

static PySequenceMethods SurfNurb_as_sequence = {
	( inquiry ) SurfNurb_length,	/* sq_length   */
	( binaryfunc ) 0,	/* sq_concat */
	( intargfunc ) 0,	/* sq_repeat */
	( intargfunc ) SurfNurb_getPoint,	/* sq_item */
	( intintargfunc ) 0,	/* sq_slice */
	( intobjargproc ) SurfNurb_setPoint,	/* sq_ass_item */
	0,			/* sq_ass_slice */
	( objobjproc ) 0,	/* sq_contains */
	0,
	0
};

static PyGetSetDef BPy_SurfNurb_getseters[] = {
#if 0
	{"matIndex",
	 (getter)SurfNurb_getMatIndex, (setter)SurfNurb_setMatIndex,
	 "material index", NULL},
#endif
	{"pointsU",
	 (getter)SurfNurb_getPointsU, (setter)NULL,
	 "number of control points in U direction", NULL},
	{"pointsV",
	 (getter)SurfNurb_getPointsV, (setter)NULL,
	 "number of control points in V direction", NULL},
	{"flagU",
	 (getter)SurfNurb_getFlagU, (setter)SurfNurb_setFlagU,
	 "knot flag for U direction", NULL},
	{"flagV",
	 (getter)SurfNurb_getFlagV, (setter)SurfNurb_setFlagV,
	 "knot flag for V direction", NULL},
	{"cyclicU",
	 (getter)SurfNurb_getCyclicU, (setter)SurfNurb_setCyclicU,
	 "cyclic setting for U direction", NULL},
	{"cyclicV",
	 (getter)SurfNurb_getCyclicV, (setter)SurfNurb_setCyclicV,
	 "cyclic setting for V direction", NULL},
	{"orderU",
	 (getter)SurfNurb_getOrderU, (setter)SurfNurb_setOrderU,
	 "order setting for U direction", NULL},
	{"orderV",
	 (getter)SurfNurb_getOrderV, (setter)SurfNurb_setOrderV,
	 "order setting for V direction", NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/*
 * compare
 * in this case, we consider two SurfNurbs equal, if they point to the same
 * blender data.
*/
static int SurfNurb_compare( BPy_SurfNurb * a, BPy_SurfNurb * b )
{
	return ( a->nurb == b->nurb ) ? 0 : -1;
}

/*
 *  SurfNurb_repr
 */
static PyObject *SurfNurb_repr( BPy_SurfNurb * self )
{
	return PyString_FromFormat( "[SurfNurb \"%d\"]", self->nurb->type );
}

/*****************************************************************************/
/* Python SurfNurb_Type structure definition:                                */
/*****************************************************************************/
PyTypeObject SurfNurb_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"SurfNurb",                 /* char *tp_name; */
	sizeof( BPy_SurfNurb ),     /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	NULL,						/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) SurfNurb_compare, /* cmpfunc tp_compare; */
	( reprfunc ) SurfNurb_repr, /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	&SurfNurb_as_sequence,      /* PySequenceMethods *tp_as_sequence; */
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
	( getiterfunc ) SurfNurb_getIter,	/*    getiterfunc tp_iter; */
	( iternextfunc ) SurfNurb_iterNext,	/*    iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_SurfNurb_methods,       /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_SurfNurb_getseters,     /* struct PyGetSetDef *tp_getset; */
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
  factory method to create a BPy_SurfNurb from a Blender Nurb
*/

PyObject *SurfNurb_CreatePyObject( Nurb * blen_nurb )
{
	BPy_SurfNurb *pyNurb;

	pyNurb = ( BPy_SurfNurb * ) PyObject_NEW( BPy_SurfNurb, &SurfNurb_Type );

	if( !pyNurb )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "could not create BPy_SurfNurb PyObject" );

	pyNurb->nurb = blen_nurb;
	return ( PyObject * ) pyNurb;
}


PyObject *SurfNurb_Init( void )
{
	PyType_Ready( &SurfNurb_Type );
	return Py_InitModule3( "Blender.SurfNurb", M_SurfNurb_methods,
				M_SurfNurb_doc );
}

