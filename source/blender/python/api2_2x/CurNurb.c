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
 * Contributor(s): Stephen Swaney
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "Python.h"
#include "DNA_curve_types.h"
#include "BKE_curve.h"
#include "MEM_guardedalloc.h"

#include "gen_utils.h"
#include "CurNurb.h"
#include "BezTriple.h"


/*-------------------------------------------------------------

stuff in this section should be placed in bpy_types.h

-----------------------------------------------------------*/


/*
 * forward declarations go here
 */


extern PyMethodDef BPy_CurNurb_methods[];
PyObject *CurNurb_CreatePyObject( Nurb * blen_nurb );
static PyObject *CurNurb_setMatIndex( BPy_CurNurb * self, PyObject * args );
static PyObject *CurNurb_getMatIndex( BPy_CurNurb * self );
/* static PyObject* CurNurb_setXXX( BPy_CurNurb* self, PyObject* args ); */
PyObject *CurNurb_getPoint( BPy_CurNurb * self, int index );
static int CurNurb_length( PyInstanceObject * inst );
static PyObject *CurNurb_getIter( BPy_CurNurb * self );
static PyObject *CurNurb_iterNext( BPy_CurNurb * self );
PyObject *CurNurb_append( BPy_CurNurb * self, PyObject * args );
PyObject *CurNurb_pointAtIndex( Nurb * nurb, int index );
static PyObject *CurNurb_isNurb( BPy_CurNurb * self );
static PyObject *CurNurb_isCyclic( BPy_CurNurb * self );

char M_CurNurb_doc[] = "CurNurb";


/*	
  CurNurb_Type callback function prototypes:                          
*/

static void CurNurb_dealloc( BPy_CurNurb * self );
static int CurNurb_compare( BPy_CurNurb * a, BPy_CurNurb * b );
static PyObject *CurNurb_getAttr( BPy_CurNurb * self, char *name );
static int CurNurb_setAttr( BPy_CurNurb * self, char *name, PyObject * v );
static PyObject *CurNurb_repr( BPy_CurNurb * self );




void CurNurb_dealloc( BPy_CurNurb * self )
{
	PyObject_DEL( self );
}



static PyObject *CurNurb_getAttr( BPy_CurNurb * self, char *name )
{
	PyObject *attr = Py_None;

	if( strcmp( name, "mat_index" ) == 0 )
		attr = PyInt_FromLong( self->nurb->mat_nr );

	else if( strcmp( name, "points" ) == 0 )
		attr = PyInt_FromLong( self->nurb->pntsu );

	if( !attr )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create PyObject" );

	/* member attribute found, return it */
	if( attr != Py_None )
		return attr;

	/* not an attribute, search the methods table */
	return Py_FindMethod( BPy_CurNurb_methods, ( PyObject * ) self, name );
}


/*
  setattr
*/

static int CurNurb_setAttr( BPy_CurNurb * self, char *name, PyObject * value )
{
	PyObject *valtuple;
	PyObject *error = NULL;

	/* make a tuple to pass to our type methods */
	valtuple = Py_BuildValue( "(O)", value );

	if( !valtuple )
		return EXPP_ReturnIntError( PyExc_MemoryError,
					    "CurNurb.setAttr: cannot create pytuple" );

	if( strcmp( name, "mat_index" ) == 0 )
		error = CurNurb_setMatIndex( self, valtuple );

	else {			/* error - no match for name */
		Py_DECREF( valtuple );

		if( ( strcmp( name, "ZZZZ" ) == 0 ) ||	/* user tried to change a */
		    ( strcmp( name, "ZZZZ" ) == 0 ) )	/* constant dict type ... */
			return EXPP_ReturnIntError( PyExc_AttributeError,
						    "constant dictionary -- cannot be changed" );
		else
			return EXPP_ReturnIntError( PyExc_KeyError,
						    "attribute not found" );
	}


	Py_DECREF( valtuple );	/* since it is not being returned */
	if( error != Py_None )
		return -1;

	Py_DECREF( Py_None );
	return 0;		/* normal exit */
}

/*
  compare
  in this case, we consider two CurNurbs equal, if they point to the same
  blender data.
*/

static int CurNurb_compare( BPy_CurNurb * a, BPy_CurNurb * b )
{
	Nurb *pa = a->nurb;
	Nurb *pb = b->nurb;

	return ( pa == pb ) ? 0 : -1;
}


/*
  factory method to create a BPy_CurNurb from a Blender Nurb
*/

PyObject *CurNurb_CreatePyObject( Nurb * blen_nurb )
{
	BPy_CurNurb *pyNurb;

	pyNurb = ( BPy_CurNurb * ) PyObject_NEW( BPy_CurNurb, &CurNurb_Type );

	if( !pyNurb )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "could not create BPy_CurNurb PyObject" );

	pyNurb->nurb = blen_nurb;
	return ( PyObject * ) pyNurb;
}


/*
 *  CurNurb_repr
 */
static PyObject *CurNurb_repr( BPy_CurNurb * self )
{				/* used by 'repr' */

	return PyString_FromFormat( "[CurNurb \"%d\"]", self->nurb->type );
}

/* XXX Can't this be simply removed? */
static PyObject *M_CurNurb_New( PyObject * self, PyObject * args )
{
	return ( PyObject * ) 0;

}



/*
 * CurNurb_append( point )
 * append a new point to a nurb curve.
 * arg is BezTriple or list of xyzw floats 
 */

PyObject *CurNurb_append( BPy_CurNurb * self, PyObject * args )
{
	Nurb *nurb = self->nurb;

	return CurNurb_appendPointToNurb( nurb, args );
}



/*
 * CurNurb_appendPointToNurb
 * this is a non-bpy utility func to add a point to a given nurb
 */

PyObject *CurNurb_appendPointToNurb( Nurb * nurb, PyObject * args )
{

	int i;
	int size;
	PyObject *pyOb;
	int npoints;

	/*
	   do we have a list of four floats or a BezTriple?
	 */
	PyArg_ParseTuple( args, "O", &pyOb );

	if( BezTriple_CheckPyObject( pyOb ) ) {
		BezTriple *tmp;
		npoints = nurb->pntsu;

/*		printf("\ndbg: got a BezTriple\n"); */
		tmp = nurb->bezt;	/* save old points */
		nurb->bezt =
			( BezTriple * ) MEM_mallocN( sizeof( BezTriple ) *
						     ( npoints + 1 ),
						     "CurNurb_append2" );

		if( !nurb->bezt )
			return ( EXPP_ReturnPyObjError
				 ( PyExc_MemoryError, "allocation failed" ) );

		/* copy old points to new */
		memmove( nurb->bezt, tmp, sizeof( BezTriple ) * npoints );
		if( tmp )
			MEM_freeN( tmp );
		nurb->pntsu++;
		/* add new point to end of list */
		memcpy( nurb->bezt + npoints,
			BezTriple_FromPyObject( pyOb ), sizeof( BezTriple ) );

	} else if( PySequence_Check( pyOb ) ) {
		size = PySequence_Size( pyOb );
/*		printf("\ndbg: got a sequence of size %d\n", size );  */
		if( size == 4 ) {
			BPoint *tmp;
			npoints = nurb->pntsu;

			tmp = nurb->bp;	/* save old pts */

			nurb->bp =
				( BPoint * ) MEM_mallocN( sizeof( BPoint ) *
							  ( npoints + 1 ),
							  "CurNurb_append1" );
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
				float tmpx =
					( float ) PyFloat_AsDouble
					( PySequence_GetItem( pyOb, i ) );
				nurb->bp[npoints].vec[i] = tmpx;

			}

			makeknots( nurb, 1, nurb->flagu >> 1 );

		} else if( size == 3 ) {	/* 3 xyz coords */
			printf( "\nNot Yet Implemented!\n" );

		}

	} else {
		/* bail with error */
		return ( EXPP_ReturnPyObjError
			 ( PyExc_AttributeError, "expected better stuff" ) );

	}

	return ( EXPP_incr_ret( Py_None ) );
}


/*
 *  CurNurb_setMatIndex
 *
 *  set index into material list
 */

static PyObject *CurNurb_setMatIndex( BPy_CurNurb * self, PyObject * args )
{
	int index;

	if( !PyArg_ParseTuple( args, "i", &( index ) ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_AttributeError,
			   "expected integer argument" ) );

	/* fixme:  some range checking would be nice! */
	self->nurb->mat_nr = index;

	Py_INCREF( Py_None );
	return Py_None;
}

/*
 * CurNurb_getMatIndex
 *
 * returns index into material list
 */

static PyObject *CurNurb_getMatIndex( BPy_CurNurb * self )
{
	PyObject *index = PyInt_FromLong( ( long ) self->nurb->mat_nr );

	if( index )
		return index;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"could not get material index" ) );
}


/*
 * CurNurb_getIter
 *
 * create an iterator for our CurNurb.
 * this iterator returns the points for this CurNurb.
 */

static PyObject *CurNurb_getIter( BPy_CurNurb * self )
{
	self->bp = self->nurb->bp;
	self->bezt = self->nurb->bezt;
	self->atEnd = 0;
	self->nextPoint = 0;

	/* set exhausted flag if both bp and bezt are zero */
	if( ( !self->bp ) && ( !self->bezt ) )
		self->atEnd = 1;

	Py_INCREF( self );
	return ( PyObject * ) self;
}



static PyObject *CurNurb_iterNext( BPy_CurNurb * self )
{
	PyObject *po;		/* return value */
	Nurb *pnurb = self->nurb;
	int npoints = pnurb->pntsu;

	/* are we at end already? */
	if( self->atEnd )
		return ( EXPP_ReturnPyObjError( PyExc_StopIteration,
						"iterator at end" ) );

	if( self->nextPoint < npoints ) {

		po = CurNurb_pointAtIndex( self->nurb, self->nextPoint );
		self->nextPoint++;

		return po;

	} else {
		self->atEnd = 1;	/* set flag true */
	}

	return ( EXPP_ReturnPyObjError( PyExc_StopIteration,
					"iterator at end" ) );
}



/*
 * CurNurb_isNurb()
 * test whether spline nurb or bezier
 */

static PyObject *CurNurb_isNurb( BPy_CurNurb * self )
{
	/* NOTE: a Nurb has bp and bezt pointers
	 * depending on type.
	 * It is possible both are NULL if no points exist.
	 * in that case, we return False
	 */

	if( self->nurb->bp ) {
		return EXPP_incr_ret_True();
	} else {
		return EXPP_incr_ret_False();
	}
}

/*
 * CurNurb_isCyclic()
 * test whether spline cyclic (closed) or not (open)
 */

static PyObject *CurNurb_isCyclic( BPy_CurNurb * self )
{
        /* supposing that the flagu is always set */ 

	if( self->nurb->flagu & CU_CYCLIC ) {
		return EXPP_incr_ret_True();
	} else {
		return EXPP_incr_ret_False();
	}
}

/*
   table of module methods
   these are the equivalent of class or static methods.
   you do not need an object instance to call one.
  
*/

static PyMethodDef M_CurNurb_methods[] = {
/*   name, method, flags, doc_string                */
	{"New", ( PyCFunction ) M_CurNurb_New, METH_VARARGS | METH_KEYWORDS,
	 " () - doc string"},
/*  {"Get", (PyCFunction) M_CurNurb_method, METH_NOARGS, " () - doc string"}, */
/*   {"method", (PyCFunction) M_CurNurb_method, METH_NOARGS, " () - doc string"}, */

	{NULL, NULL, 0, NULL}
};



/*
 * method table
 * table of instance methods
 * these methods are invoked on an instance of the type.
*/

static PyMethodDef BPy_CurNurb_methods[] = {
/*   name,     method,                    flags,         doc               */
/*  {"method", (PyCFunction) CurNurb_method, METH_NOARGS, " () - doc string"} */
	{"setMatIndex", ( PyCFunction ) CurNurb_setMatIndex, METH_VARARGS,
	 "( index ) - set index into materials list"},
	{"getMatIndex", ( PyCFunction ) CurNurb_getMatIndex, METH_NOARGS,
	 "( ) - get current material index"},
	{"append", ( PyCFunction ) CurNurb_append, METH_VARARGS,
	 "( point ) - add a new point.  arg is BezTriple or list of x,y,z,w floats"},
	{"isNurb", ( PyCFunction ) CurNurb_isNurb, METH_NOARGS,
	 "( ) - boolean function tests if this spline is type nurb or bezier"},
	{"isCyclic", ( PyCFunction ) CurNurb_isCyclic, METH_NOARGS,
	 "( ) - boolean function tests if this spline is cyclic (closed) or not (open)"},
	{NULL, NULL, 0, NULL}
};


/* 
 *   methods for CurNurb as sequece
 */

static PySequenceMethods CurNurb_as_sequence = {
	( inquiry ) CurNurb_length,	/* sq_length   */
	( binaryfunc ) 0,	/* sq_concat */
	( intargfunc ) 0,	/* sq_repeat */
	( intargfunc ) CurNurb_getPoint,	/* sq_item */
	( intintargfunc ) 0,	/* sq_slice */
	0,			/* sq_ass_item */
	0,			/* sq_ass_slice */
	( objobjproc ) 0,	/* sq_contains */
	0,
	0
};



/*
  Object Type definition
  full blown 2.3 struct
*/

PyTypeObject CurNurb_Type = {
	PyObject_HEAD_INIT( NULL ) /* required py macro */
	0,	/* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"CurNurb",		/* char *tp_name; */
	sizeof( CurNurb_Type ),	/* int tp_basicsize, */
	0,			/* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) CurNurb_dealloc,	/*    destructor tp_dealloc; */
	0,			/*    printfunc tp_print; */
	( getattrfunc ) CurNurb_getAttr,	/*    getattrfunc tp_getattr; */
	( setattrfunc ) CurNurb_setAttr,	/*    setattrfunc tp_setattr; */
	( cmpfunc ) CurNurb_compare,	/*    cmpfunc tp_compare; */
	( reprfunc ) CurNurb_repr,	/*    reprfunc tp_repr; */

	/* Method suites for standard classes */

	0,			/*    PyNumberMethods *tp_as_number; */
	&CurNurb_as_sequence,	/*    PySequenceMethods *tp_as_sequence; */
	0,			/*    PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	0,			/*    hashfunc tp_hash; */
	0,			/*    ternaryfunc tp_call; */
	0,			/*    reprfunc tp_str; */
	0,			/*    getattrofunc tp_getattro; */
	0,			/*    setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	0,			/*    PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,	/*    long tp_flags; */

	0,			/*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	0,			/*    traverseproc tp_traverse; */

	/* delete references to contained objects */
	0,			/*    inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	0,			/*  richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,			/* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	( getiterfunc ) CurNurb_getIter,	/*    getiterfunc tp_iter; */
	( iternextfunc ) CurNurb_iterNext,	/*    iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_CurNurb_methods,	/*    struct PyMethodDef *tp_methods; */
	0,			/*    struct PyMemberDef *tp_members; */
	0,			/*    struct PyGetSetDef *tp_getset; */
	0,			/*    struct _typeobject *tp_base; */
	0,			/*    PyObject *tp_dict; */
	0,			/*    descrgetfunc tp_descr_get; */
	0,			/*    descrsetfunc tp_descr_set; */
	0,			/*    long tp_dictoffset; */
	0,			/*    initproc tp_init; */
	0,			/*    allocfunc tp_alloc; */
	0,			/*    newfunc tp_new; */
	/*  Low-level free-memory routine */
	0,			/*    freefunc tp_free;  */
	/* For PyObject_IS_GC */
	0,			/*    inquiry tp_is_gc;  */
	0,			/*    PyObject *tp_bases; */
	/* method resolution order */
	0,			/*    PyObject *tp_mro;  */
	0,			/*    PyObject *tp_cache; */
	0,			/*    PyObject *tp_subclasses; */
	0,			/*    PyObject *tp_weaklist; */
	0
};


/*
 * CurNurb_length
 * returns the number of points in a Nurb
 * this is a tp_as_sequence method, not a regular instance method.
 */

static int CurNurb_length( PyInstanceObject * inst )
{
	Nurb *nurb;
	int len;

	if( CurNurb_CheckPyObject( ( PyObject * ) inst ) ) {
		nurb = ( ( BPy_CurNurb * ) inst )->nurb;
		len = nurb->pntsu;
		return len;
	}

	return EXPP_ReturnIntError( PyExc_RuntimeError,
				    "arg is not a BPy_CurNurb" );
}


/*
 * CurNurb_getPoint
 * returns the Nth point in a Nurb
 * this is one of the tp_as_sequence methods, hence the int N argument.
 * it is called via the [] operator, not as a usual instance method.
 */

PyObject *CurNurb_getPoint( BPy_CurNurb * self, int index )
{
	PyObject *pyo;
	Nurb *myNurb;

	int npoints;

	/* for convenince */
	myNurb = self->nurb;
	npoints = myNurb->pntsu;

	/* DELETED: bail if index < 0 */
	/* actually, this check is not needed since python treats */
	/* negative indices as starting from the right end of a sequence */

	/* bail if no Nurbs in Curve */
	if( npoints == 0 )
		return ( EXPP_ReturnPyObjError( PyExc_IndexError,
						"no points in this CurNurb" ) );

	if( index >= npoints )	/* out of range!  */
		return ( EXPP_ReturnPyObjError( PyExc_IndexError,
						"index out of range" ) );

	pyo = CurNurb_pointAtIndex( myNurb, index );

	return ( PyObject * ) pyo;
}


/* 
 * this is an internal routine.  not callable directly from python
 */

PyObject *CurNurb_pointAtIndex( Nurb * nurb, int index )
{
	PyObject *pyo;

	if( nurb->bp ) {	/* we have a nurb curve */
		int i;
		pyo = PyList_New( 4 );

		for( i = 0; i < 4; i++ ) {
			PyList_SetItem( pyo, i,
					PyFloat_FromDouble( nurb->bp[index].
							    vec[i] ) );

		}

	} else if( nurb->bezt ) {	/* we have a bezier */
		/* if an error occurs, we just pass it on */
		pyo = BezTriple_CreatePyObject( &( nurb->bezt[index] ) );

	} else			/* something is horribly wrong */
		/* neither bp or bezt is set && pntsu != 0 */
		return ( EXPP_ReturnPyObjError( PyExc_SystemError,
						"inconsistant structure found" ) );

	return ( pyo );
}


int CurNurb_CheckPyObject( PyObject * py_obj )
{
	return ( py_obj->ob_type == &CurNurb_Type );
}


PyObject *CurNurb_Init( void )
{
	PyObject *submodule;

	CurNurb_Type.ob_type = &PyType_Type;

	submodule =
		Py_InitModule3( "Blender.CurNurb", M_CurNurb_methods,
				M_CurNurb_doc );
	return ( submodule );
}
