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
 * Contributor(s): Jacques Guignot, Stephen Swaney
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <Python.h>
#include "Curve.h"
#include <stdio.h>

#include <BLI_arithb.h>
#include <BLI_blenlib.h>
#include <BKE_main.h>
#include <BKE_displist.h>
#include <BKE_global.h>
#include <BKE_object.h>
#include <BKE_library.h>
#include <BKE_curve.h>
#include <BKE_utildefines.h>
#include <MEM_guardedalloc.h>	/* because we wil be mallocing memory */

#include "CurNurb.h"
#include "Material.h"
#include "Object.h"
#include "gen_utils.h"


/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/*  Blender.Curve.__doc__                                                    */
/*****************************************************************************/

char M_Curve_doc[] = "The Blender Curve module\n\n\
This module provides access to **Curve Data** in Blender.\n\
Functions :\n\
	New(opt name) : creates a new curve object with the given name (optional)\n\
	Get(name) : retreives a curve  with the given name (mandatory)\n\
	get(name) : same as Get. Kept for compatibility reasons";
char M_Curve_New_doc[] = "";
char M_Curve_Get_doc[] = "xxx";



/*****************************************************************************/
/*  Python API function prototypes for the Curve module.                     */
/*****************************************************************************/
static PyObject *M_Curve_New( PyObject * self, PyObject * args );
static PyObject *M_Curve_Get( PyObject * self, PyObject * args );


/*****************************************************************************/
/*  Python BPy_Curve instance methods declarations:                          */
/*****************************************************************************/

PyObject *Curve_getName( BPy_Curve * self );
PyObject *Curve_setName( BPy_Curve * self, PyObject * args );
static PyObject *Curve_getPathLen( BPy_Curve * self );
static PyObject *Curve_setPathLen( BPy_Curve * self, PyObject * args );
static PyObject *Curve_getTotcol( BPy_Curve * self );
static PyObject *Curve_setTotcol( BPy_Curve * self, PyObject * args );
PyObject *Curve_getMode( BPy_Curve * self );
PyObject *Curve_setMode( BPy_Curve * self, PyObject * args );
PyObject *Curve_getBevresol( BPy_Curve * self );
PyObject *Curve_setBevresol( BPy_Curve * self, PyObject * args );
PyObject *Curve_getResolu( BPy_Curve * self );
PyObject *Curve_setResolu( BPy_Curve * self, PyObject * args );
PyObject *Curve_getResolv( BPy_Curve * self );
PyObject *Curve_setResolv( BPy_Curve * self, PyObject * args );
PyObject *Curve_getWidth( BPy_Curve * self );
PyObject *Curve_setWidth( BPy_Curve * self, PyObject * args );
PyObject *Curve_getExt1( BPy_Curve * self );
PyObject *Curve_setExt1( BPy_Curve * self, PyObject * args );
PyObject *Curve_getExt2( BPy_Curve * self );
PyObject *Curve_setExt2( BPy_Curve * self, PyObject * args );
static PyObject *Curve_getControlPoint( BPy_Curve * self, PyObject * args );
static PyObject *Curve_setControlPoint( BPy_Curve * self, PyObject * args );
static PyObject *Curve_getLoc( BPy_Curve * self );
static PyObject *Curve_setLoc( BPy_Curve * self, PyObject * args );
static PyObject *Curve_getRot( BPy_Curve * self );
static PyObject *Curve_setRot( BPy_Curve * self, PyObject * args );
static PyObject *Curve_getSize( BPy_Curve * self );
static PyObject *Curve_setSize( BPy_Curve * self, PyObject * args );
static PyObject *Curve_getNumCurves( BPy_Curve * self );
static PyObject *Curve_isNurb( BPy_Curve * self, PyObject * args );
static PyObject *Curve_isCyclic( BPy_Curve * self, PyObject * args);
static PyObject *Curve_getNumPoints( BPy_Curve * self, PyObject * args );
static PyObject *Curve_getNumPoints( BPy_Curve * self, PyObject * args );

static PyObject *Curve_appendPoint( BPy_Curve * self, PyObject * args );
static PyObject *Curve_appendNurb( BPy_Curve * self, PyObject * args );

static PyObject *Curve_getMaterials( BPy_Curve * self );

static PyObject *Curve_getBevOb( BPy_Curve * self );
static PyObject *Curve_setBevOb( BPy_Curve * self, PyObject * args );

static PyObject *Curve_getIter( BPy_Curve * self );
static PyObject *Curve_iterNext( BPy_Curve * self );

PyObject *Curve_getNurb( BPy_Curve * self, int n );
static int Curve_length( PyInstanceObject * inst );
void update_displists( void *data );

void makeDispList( Object * ob );
struct chartrans *text_to_curve( Object * ob, int mode );


/*****************************************************************************/
/*  Python method definitions for Blender.Curve module:             */
/*****************************************************************************/
struct PyMethodDef M_Curve_methods[] = {
	{"New", ( PyCFunction ) M_Curve_New, METH_VARARGS, M_Curve_New_doc},
	{"Get", M_Curve_Get, METH_VARARGS, M_Curve_Get_doc},
	{"get", M_Curve_Get, METH_VARARGS, M_Curve_Get_doc},
	{NULL, NULL, 0, NULL}
};


/*****************************************************************************/
/*  Python BPy_Curve instance methods table:                                 */
/*****************************************************************************/
static PyMethodDef BPy_Curve_methods[] = {
	{"getName", ( PyCFunction ) Curve_getName,
	 METH_NOARGS, "() - Return Curve Data name"},
	{"setName", ( PyCFunction ) Curve_setName,
	 METH_VARARGS, "() - Sets Curve Data name"},
	{"getPathLen", ( PyCFunction ) Curve_getPathLen,
	 METH_NOARGS, "() - Return Curve path length"},
	{"setPathLen", ( PyCFunction ) Curve_setPathLen,
	 METH_VARARGS, "(int) - Sets Curve path length"},
	{"getTotcol", ( PyCFunction ) Curve_getTotcol,
	 METH_NOARGS, "() - Return the number of materials of the curve"},
	{"setTotcol", ( PyCFunction ) Curve_setTotcol,
	 METH_VARARGS, "(int) - Sets the number of materials of the curve"},
	{"getFlag", ( PyCFunction ) Curve_getMode,
	 METH_NOARGS, "() - Return flag (see the doc for semantic)"},
	{"setFlag", ( PyCFunction ) Curve_setMode,
	 METH_VARARGS, "(int) - Sets flag (see the doc for semantic)"},
	{"getBevresol", ( PyCFunction ) Curve_getBevresol,
	 METH_NOARGS, "() - Return bevel resolution"},
	{"setBevresol", ( PyCFunction ) Curve_setBevresol,
	 METH_VARARGS, "(int) - Sets bevel resolution"},
	{"getResolu", ( PyCFunction ) Curve_getResolu,
	 METH_NOARGS, "() - Return U resolution"},
	{"setResolu", ( PyCFunction ) Curve_setResolu,
	 METH_VARARGS, "(int) - Sets U resolution"},
	{"getResolv", ( PyCFunction ) Curve_getResolv,
	 METH_NOARGS, "() - Return V resolution"},
	{"setResolv", ( PyCFunction ) Curve_setResolv,
	 METH_VARARGS, "(int) - Sets V resolution"},
	{"getWidth", ( PyCFunction ) Curve_getWidth,
	 METH_NOARGS, "() - Return curve width"},
	{"setWidth", ( PyCFunction ) Curve_setWidth,
	 METH_VARARGS, "(int) - Sets curve width"},
	{"getExt1", ( PyCFunction ) Curve_getExt1,
	 METH_NOARGS, "() - Returns extent 1 of the bevel"},
	{"setExt1", ( PyCFunction ) Curve_setExt1,
	 METH_VARARGS, "(int) - Sets  extent 1 of the bevel"},
	{"getExt2", ( PyCFunction ) Curve_getExt2,
	 METH_NOARGS, "() - Return extent 2 of the bevel "},
	{"setExt2", ( PyCFunction ) Curve_setExt2,
	 METH_VARARGS, "(int) - Sets extent 2 of the bevel "},
	{"getControlPoint", ( PyCFunction ) Curve_getControlPoint,
	 METH_VARARGS, "(int numcurve,int numpoint) -\
Gets a control point.Depending upon the curve type, returne a list of 4 or 9 floats"},
	{"setControlPoint", ( PyCFunction ) Curve_setControlPoint,
	 METH_VARARGS, "(int numcurve,int numpoint,float x,float y,float z,\
float w)(nurbs) or  (int numcurve,int numpoint,float x1,...,x9(bezier)\
Sets a control point "},
	{"getLoc", ( PyCFunction ) Curve_getLoc,
	 METH_NOARGS, "() - Gets Location of the curve (a 3-tuple) "},
	{"setLoc", ( PyCFunction ) Curve_setLoc,
	 METH_VARARGS, "(3-tuple) - Sets Location "},
	{"getRot", ( PyCFunction ) Curve_getRot,
	 METH_NOARGS, "() - Gets curve rotation"},
	{"setRot", ( PyCFunction ) Curve_setRot,
	 METH_VARARGS, "(3-tuple) - Sets curve rotation"},
	{"getSize", ( PyCFunction ) Curve_getSize,
	 METH_NOARGS, "() - Gets curve size"},
	{"setSize", ( PyCFunction ) Curve_setSize,
	 METH_VARARGS, "(3-tuple) - Sets curve size"},
	{"getNumCurves", ( PyCFunction ) Curve_getNumCurves,
	 METH_NOARGS, "() - Gets number of curves in Curve"},
	{"isNurb", ( PyCFunction ) Curve_isNurb,
	 METH_VARARGS,
	 "(nothing or integer) - returns 1 if curve is type Nurb, O otherwise."},
	{"isCyclic", ( PyCFunction ) Curve_isCyclic,
	 METH_VARARGS, "( nothing or integer ) - returns true if curve is cyclic (closed), false otherwise."},
	{"getNumPoints", ( PyCFunction ) Curve_getNumPoints,
	 METH_VARARGS,
	 "(nothing or integer) - returns the number of points of the specified curve"},
	{"appendPoint", ( PyCFunction ) Curve_appendPoint, METH_VARARGS,
	 "( int numcurve, list of coordinates) - adds a new point to end of curve"},
	{"appendNurb", ( PyCFunction ) Curve_appendNurb, METH_VARARGS,
	 "( new_nurb ) - adds a new nurb to the Curve"},
	{"update", ( PyCFunction ) Curve_update, METH_NOARGS,
	 "( ) - updates display lists after changes to Curve"},
	{"getMaterials", ( PyCFunction ) Curve_getMaterials, METH_NOARGS,
	 "() - returns list of materials assigned to this Curve"},
	{"getBevOb", ( PyCFunction ) Curve_getBevOb, METH_NOARGS,
	 "() - returns Bevel Object assigned to this Curve"},
	{"setBevOb", ( PyCFunction ) Curve_setBevOb, METH_VARARGS,
	 "() - assign a Bevel Object to this Curve"},
	{NULL, NULL, 0, NULL}
};


/*****************************************************************************/
/*  Python Curve_Type callback function prototypes:                         */
/*****************************************************************************/
static void CurveDeAlloc( BPy_Curve * msh );
/* static int CurvePrint (BPy_Curve *msh, FILE *fp, int flags); */
static int CurveSetAttr( BPy_Curve * msh, char *name, PyObject * v );
static PyObject *CurveGetAttr( BPy_Curve * msh, char *name );
static PyObject *CurveRepr( BPy_Curve * msh );

PyObject *Curve_CreatePyObject( struct Curve *curve );
int Curve_CheckPyObject( PyObject * py_obj );
struct Curve *Curve_FromPyObject( PyObject * py_obj );

static PySequenceMethods Curve_as_sequence = {
	( inquiry ) Curve_length,	/* sq_length   */
	( binaryfunc ) 0,	/* sq_concat */
	( intargfunc ) 0,	/* sq_repeat */
	( intargfunc ) Curve_getNurb,	/* sq_item */
	( intintargfunc ) 0,	/* sq_slice */
	0,			/* sq_ass_item */
	0,			/* sq_ass_slice */
	( objobjproc ) 0,	/* sq_contains */
	0,
	0
};


/*****************************************************************************/
/* Python Curve_Type structure definition:                                   */
/*****************************************************************************/
PyTypeObject Curve_Type = {
	PyObject_HEAD_INIT( NULL ) /* required macro */ 
	0,	/* ob_size */
	"Curve",		/* tp_name - for printing */
	sizeof( BPy_Curve ),	/* tp_basicsize - for allocation */
	0,			/* tp_itemsize  - for allocation */
	/* methods for standard operations */
	( destructor ) CurveDeAlloc,	/* tp_dealloc */
	0,			/* tp_print */
	( getattrfunc ) CurveGetAttr,	/* tp_getattr */
	( setattrfunc ) CurveSetAttr,	/* tp_setattr */
	0,			/* tp_compare */
	( reprfunc ) CurveRepr,	/* tp_repr */
	/* methods for standard classes */
	0,			/* tp_as_number */
	&Curve_as_sequence,	/* tp_as_sequence */
	0,			/* tp_as_mapping */
	0,			/* tp_as_hash */
	0,			/* tp_call */
	0,			/* tp_str */
	0,			/* tp_getattro */
	0,			/* tp_setattro */
	0,			/* tp_as_buffer */
	/* Flags to define presence of optional/expaned features */
	Py_TPFLAGS_HAVE_ITER,	/* tp_flags */
	0,			/* tp_doc - documentation string */
	0,			/* tp_traverse */

	/* delete references to contained objects */
	0,			/* tp_clear */

	0,			/* tp_richcompare - rich comparisions */
	0,			/* tp_weaklistoffset - weak reference enabler */

	/* new release 2.2 stuff - Iterators */
	( getiterfunc ) Curve_getIter,	/* tp_iter */
	( iternextfunc ) Curve_iterNext,	/* tp_iternext */

	/*  Attribute descriptor and subclassing stuff */
	BPy_Curve_methods,	/* tp_methods */
	0,			/* tp_members */
	0,			/* tp_getset; */
	0,			/* tp_base; */
	0,			/* tp_dict; */
	0,			/* tp_descr_get; */
	0,			/* tp_descr_set; */
	0,			/* tp_dictoffset; */
	0,			/* tp_init; */
	0,			/* tp_alloc; */
	0,			/* tp_new; */
	0,			/* tp_free;  Low-level free-memory routine */
	0,			/* tp_is_gc */
	0,			/* tp_bases; */
	0,			/* tp_mro;  method resolution order */
	0,			/* tp_defined; */
	0,			/* tp_weakllst */
	0,
	0
};

/*****************************************************************************/
/* Function:              M_Curve_New                                       */
/* Python equivalent:     Blender.Curve.New                                 */
/*****************************************************************************/
static PyObject *M_Curve_New( PyObject * self, PyObject * args )
{
	char buf[24];
	char *name = NULL;
	BPy_Curve *pycurve;	/* for Curve Data object wrapper in Python */
	Curve *blcurve = 0;	/* for actual Curve Data we create in Blender */

	if( !PyArg_ParseTuple( args, "|s", &name ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_AttributeError,
			   "expected string argument or no argument" ) );

	blcurve = add_curve( OB_CURVE );	/* first create the Curve Data in Blender */

	if( blcurve == NULL )	/* bail out if add_curve() failed */
		return ( EXPP_ReturnPyObjError
			 ( PyExc_RuntimeError,
			   "couldn't create Curve Data in Blender" ) );

	/* return user count to zero because add_curve() inc'd it */
	blcurve->id.us = 0;
	/* create python wrapper obj */
	pycurve = ( BPy_Curve * ) PyObject_NEW( BPy_Curve, &Curve_Type );

	if( pycurve == NULL )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_MemoryError,
			   "couldn't create Curve Data object" ) );

	pycurve->curve = blcurve;	/* link Python curve wrapper to Blender Curve */
	if( name ) {
		PyOS_snprintf( buf, sizeof( buf ), "%s", name );
		rename_id( &blcurve->id, buf );
	}

	return ( PyObject * ) pycurve;
}

/*****************************************************************************/
/* Function:              M_Curve_Get                                       */
/* Python equivalent:     Blender.Curve.Get                                 */
/*****************************************************************************/
static PyObject *M_Curve_Get( PyObject * self, PyObject * args )
{

	char *name = NULL;
	Curve *curv_iter;
	BPy_Curve *wanted_curv;

	if( !PyArg_ParseTuple( args, "|s", &name ) )	/* expects nothing or a string */
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected string argument" ) );
	if( name ) {		/*a name has been given */
		/* Use the name to search for the curve requested */
		wanted_curv = NULL;
		curv_iter = G.main->curve.first;

		while( ( curv_iter ) && ( wanted_curv == NULL ) ) {

			if( strcmp( name, curv_iter->id.name + 2 ) == 0 ) {
				wanted_curv = ( BPy_Curve * )
					PyObject_NEW( BPy_Curve, &Curve_Type );
				if( wanted_curv )
					wanted_curv->curve = curv_iter;
			}

			curv_iter = curv_iter->id.next;
		}

		if( wanted_curv == NULL ) {	/* Requested curve doesn't exist */
			char error_msg[64];
			PyOS_snprintf( error_msg, sizeof( error_msg ),
				       "Curve \"%s\" not found", name );
			return ( EXPP_ReturnPyObjError
				 ( PyExc_NameError, error_msg ) );
		}


		return ( PyObject * ) wanted_curv;
	} /* end  of if(name) */
	else {
		/* no name has been given; return a list of all curves by name.  */
		PyObject *curvlist;

		curv_iter = G.main->curve.first;
		curvlist = PyList_New( 0 );

		if( curvlist == NULL )
			return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
							"couldn't create PyList" ) );

		while( curv_iter ) {
			BPy_Curve *found_cur =
				( BPy_Curve * ) PyObject_NEW( BPy_Curve,
							      &Curve_Type );
			found_cur->curve = curv_iter;
			PyList_Append( curvlist, ( PyObject * ) found_cur );

			curv_iter = curv_iter->id.next;
		}

		return ( curvlist );
	}			/* end of else */
}

/*****************************************************************************/
/* Function:              Curve_Init                                         */
/*****************************************************************************/
PyObject *Curve_Init( void )
{
	PyObject *submodule;

	Curve_Type.ob_type = &PyType_Type;

	submodule =
		Py_InitModule3( "Blender.Curve", M_Curve_methods,
				M_Curve_doc );
	return ( submodule );
}

/*****************************************************************************/
/* Python BPy_Curve methods:                                                 */
/* gives access to                                                           */
/* name, pathlen totcol flag bevresol                                        */
/* resolu resolv width ext1 ext2                                             */
/* controlpoint loc rot size                                                 */
/* numpts                                                                    */
/*****************************************************************************/


PyObject *Curve_getName( BPy_Curve * self )
{
	PyObject *attr = PyString_FromString( self->curve->id.name + 2 );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Curve.name attribute" ) );
}

PyObject *Curve_setName( BPy_Curve * self, PyObject * args )
{
	char *name;
	char buf[50];

	if( !PyArg_ParseTuple( args, "s", &( name ) ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected string argument" ) );
	PyOS_snprintf( buf, sizeof( buf ), "%s", name );
	rename_id( &self->curve->id, buf );	/* proper way in Blender */

	Curve_update( self );

	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *Curve_getPathLen( BPy_Curve * self )
{
	PyObject *attr = PyInt_FromLong( ( long ) self->curve->pathlen );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Curve.pathlen attribute" ) );
}


static PyObject *Curve_setPathLen( BPy_Curve * self, PyObject * args )
{

	if( !PyArg_ParseTuple( args, "i", &( self->curve->pathlen ) ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected int argument" ) );

	Py_INCREF( Py_None );
	return Py_None;
}


static PyObject *Curve_getTotcol( BPy_Curve * self )
{
	PyObject *attr = PyInt_FromLong( ( long ) self->curve->totcol );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Curve.totcol attribute" ) );
}


static PyObject *Curve_setTotcol( BPy_Curve * self, PyObject * args )
{

	if( !PyArg_ParseTuple( args, "i", &( self->curve->totcol ) ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected int argument" ) );

	Py_INCREF( Py_None );
	return Py_None;
}


PyObject *Curve_getMode( BPy_Curve * self )
{
	PyObject *attr = PyInt_FromLong( ( long ) self->curve->flag );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Curve.flag attribute" ) );
}


PyObject *Curve_setMode( BPy_Curve * self, PyObject * args )
{

	if( !PyArg_ParseTuple( args, "i", &( self->curve->flag ) ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected int argument" ) );

	Py_INCREF( Py_None );
	return Py_None;
}


PyObject *Curve_getBevresol( BPy_Curve * self )
{
	PyObject *attr = PyInt_FromLong( ( long ) self->curve->bevresol );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Curve.bevresol attribute" ) );
}


PyObject *Curve_setBevresol( BPy_Curve * self, PyObject * args )
{
	short value;

	if( !PyArg_ParseTuple( args, "h", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
			"expected integer argument" ) );

	if(value > 10 || value < 0)
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
			"acceptable values are between 10 and 0" ) );
	self->curve->bevresol = value;

	return EXPP_incr_ret( Py_None );
}


PyObject *Curve_getResolu( BPy_Curve * self )
{
	PyObject *attr = PyInt_FromLong( ( long ) self->curve->resolu );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Curve.resolu attribute" ) );
}


PyObject *Curve_setResolu( BPy_Curve * self, PyObject * args )
{
	short value;

	if( !PyArg_ParseTuple( args, "h", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
			"expected integer argument" ) );

	if(value > 128 || value < 1)
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
			"acceptable values are between 128 and 1" ) );
	self->curve->resolu = value;

	return EXPP_incr_ret( Py_None );
}



PyObject *Curve_getResolv( BPy_Curve * self )
{
	PyObject *attr = PyInt_FromLong( ( long ) self->curve->resolv );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Curve.resolv attribute" ) );
}


PyObject *Curve_setResolv( BPy_Curve * self, PyObject * args )
{
	short value;

	if( !PyArg_ParseTuple( args, "h", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
			"expected integer argument" ) );

	if(value > 128 || value < 1)
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
			"acceptable values are between 128 and 1" ) );
	self->curve->resolv = value;

	return EXPP_incr_ret( Py_None );
}



PyObject *Curve_getWidth( BPy_Curve * self )
{
	PyObject *attr = PyFloat_FromDouble( ( double ) self->curve->width );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Curve.width attribute" ) );
}


PyObject *Curve_setWidth( BPy_Curve * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
			"expected float argument" ) );

	if(value > 2.0f || value < 0.0f)
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
			"acceptable values are between 2.0 and 0.0" ) );
	self->curve->width = value;

	return EXPP_incr_ret( Py_None );
}


PyObject *Curve_getExt1( BPy_Curve * self )
{
	PyObject *attr = PyFloat_FromDouble( ( double ) self->curve->ext1 );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Curve.ext1 attribute" ) );
}


PyObject *Curve_setExt1( BPy_Curve * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
			"expected float argument" ) );

	if(value > 5.0f || value < 0.0f)
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
			"acceptable values are between 5.0 and 0.0" ) );
	self->curve->ext1 = value;

	return EXPP_incr_ret( Py_None );
}



PyObject *Curve_getExt2( BPy_Curve * self )
{
	PyObject *attr = PyFloat_FromDouble( ( double ) self->curve->ext2 );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Curve.ext2 attribute" ) );
}


PyObject *Curve_setExt2( BPy_Curve * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
			"expected float argument" ) );

	if(value > 2.0f || value < 0.0f)
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
			"acceptable values are between 2.0 and 0.0" ) );
	self->curve->ext2 = value;

	return EXPP_incr_ret( Py_None );
}


/*
static PyObject *Curve_setControlPoint(BPy_Curve *self, PyObject *args)
{
  Nurb*ptrnurb = self->curve->nurb.first;
  int numcourbe,numpoint,i,j;
  float x,y,z,w;
  float bez[9];
  if (!ptrnurb){ Py_INCREF(Py_None);return Py_None;}

  if (ptrnurb->bp)
    if (!PyArg_ParseTuple(args, "iiffff", &numcourbe,&numpoint,&x,&y,&z,&w))  
      return (EXPP_ReturnPyObjError (PyExc_AttributeError,
								"expected int int float float float float arguments"));
  if (ptrnurb->bezt)
    if (!PyArg_ParseTuple(args, "iifffffffff", &numcourbe,&numpoint,
						bez,bez+1,bez+2,bez+3,bez+4,bez+5,bez+6,bez+7,bez+8))  
      return (EXPP_ReturnPyObjError (PyExc_AttributeError,
					"expected int int float float float float float float "
					"float float float arguments"));

  for(i = 0;i< numcourbe;i++)
    ptrnurb=ptrnurb->next;
  if (ptrnurb->bp)
    {
      ptrnurb->bp[numpoint].vec[0] = x;
      ptrnurb->bp[numpoint].vec[1] = y;
      ptrnurb->bp[numpoint].vec[2] = z;
      ptrnurb->bp[numpoint].vec[3] = w;
    }
  if (ptrnurb->bezt)
    {
      for(i = 0;i<3;i++)
	for(j = 0;j<3;j++)
	  ptrnurb->bezt[numpoint].vec[i][j] = bez[i*3+j];
    }
	
  Py_INCREF(Py_None);
  return Py_None;
}
*/


/*
 * Curve_setControlPoint
 * this function sets an EXISTING control point.
 * it does NOT add a new one.
 */

static PyObject *Curve_setControlPoint( BPy_Curve * self, PyObject * args )
{
	PyObject *listargs = 0;
	Nurb *ptrnurb = self->curve->nurb.first;
	int numcourbe, numpoint, i, j;

	if( !ptrnurb ) {
		Py_INCREF( Py_None );
		return Py_None;
	}

	if( ptrnurb->bp )
		if( !PyArg_ParseTuple
		    ( args, "iiO", &numcourbe, &numpoint, &listargs ) )
			return ( EXPP_ReturnPyObjError
				 ( PyExc_AttributeError,
				   "expected int int list arguments" ) );
	if( ptrnurb->bezt )
		if( !PyArg_ParseTuple
		    ( args, "iiO", &numcourbe, &numpoint, &listargs ) )
			return ( EXPP_ReturnPyObjError
				 ( PyExc_AttributeError,
				   "expected int int list arguments" ) );

	for( i = 0; i < numcourbe; i++ )
		ptrnurb = ptrnurb->next;

	if( ptrnurb->bp )
		for( i = 0; i < 4; i++ )
			ptrnurb->bp[numpoint].vec[i] =
				PyFloat_AsDouble( PyList_GetItem
						  ( listargs, i ) );

	if( ptrnurb->bezt )
		for( i = 0; i < 3; i++ )
			for( j = 0; j < 3; j++ )
				ptrnurb->bezt[numpoint].vec[i][j] =
					PyFloat_AsDouble( PyList_GetItem
							  ( listargs,
							    i * 3 + j ) );

	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *Curve_getControlPoint( BPy_Curve * self, PyObject * args )
{
	PyObject *liste = PyList_New( 0 );	/* return values */

	Nurb *ptrnurb;
	int i, j;
	/* input args: requested curve and point number on curve */
	int numcourbe, numpoint;

	if( !PyArg_ParseTuple( args, "ii", &numcourbe, &numpoint ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected int int arguments" ) );
	if( ( numcourbe < 0 ) || ( numpoint < 0 ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						" arguments must be non-negative" ) );

	/* if no nurbs in this curve obj */
	if( !self->curve->nurb.first )
		return liste;

	/* walk the list of nurbs to find requested numcourbe */
	ptrnurb = self->curve->nurb.first;
	for( i = 0; i < numcourbe; i++ ) {
		ptrnurb = ptrnurb->next;
		if( !ptrnurb )	/* if zero, we ran just ran out of curves */
			return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
							"curve index out of range" ) );
	}

	/* check numpoint param against pntsu */
	if( numpoint >= ptrnurb->pntsu )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"point index out of range" ) );

	if( ptrnurb->bp ) {	/* if we are a nurb curve, you get 4 values */
		for( i = 0; i < 4; i++ )
			PyList_Append( liste,
				       PyFloat_FromDouble( ptrnurb->
							   bp[numpoint].
							   vec[i] ) );
	}

	if( ptrnurb->bezt ) {	/* if we are a bezier, you get 9 values */
		for( i = 0; i < 3; i++ )
			for( j = 0; j < 3; j++ )
				PyList_Append( liste,
					       PyFloat_FromDouble( ptrnurb->
								   bezt
								   [numpoint].
								   vec[i]
								   [j] ) );
	}

	return liste;
}



static PyObject *Curve_getLoc( BPy_Curve * self )
{
	int i;
	PyObject *liste = PyList_New( 3 );
	for( i = 0; i < 3; i++ )
		PyList_SetItem( liste, i,
				PyFloat_FromDouble( self->curve->loc[i] ) );
	return liste;
}

static PyObject *Curve_setLoc( BPy_Curve * self, PyObject * args )
{
	PyObject *listargs = 0;
	int i;
	if( !PyArg_ParseTuple( args, "O", &listargs ) )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "expected list argument" );
	if( !PyList_Check( listargs ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "expected a list" ) );
	for( i = 0; i < 3; i++ ) {
		PyObject *xx = PyList_GetItem( listargs, i );
		self->curve->loc[i] = PyFloat_AsDouble( xx );
	}
	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *Curve_getRot( BPy_Curve * self )
{

	int i;
	PyObject *liste = PyList_New( 3 );
	for( i = 0; i < 3; i++ )
		PyList_SetItem( liste, i,
				PyFloat_FromDouble( self->curve->rot[i] ) );
	return liste;

}

static PyObject *Curve_setRot( BPy_Curve * self, PyObject * args )
{
	PyObject *listargs = 0;
	int i;
	if( !PyArg_ParseTuple( args, "O", &listargs ) )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "expected list argument" );
	if( !PyList_Check( listargs ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "expected a list" ) );
	for( i = 0; i < 3; i++ ) {
		PyObject *xx = PyList_GetItem( listargs, i );
		self->curve->rot[i] = PyFloat_AsDouble( xx );
	}
	Py_INCREF( Py_None );
	return Py_None;

}
static PyObject *Curve_getSize( BPy_Curve * self )
{
	int i;
	PyObject *liste = PyList_New( 3 );
	for( i = 0; i < 3; i++ )
		PyList_SetItem( liste, i,
				PyFloat_FromDouble( self->curve->size[i] ) );
	return liste;

}

static PyObject *Curve_setSize( BPy_Curve * self, PyObject * args )
{
	PyObject *listargs = 0;
	int i;
	if( !PyArg_ParseTuple( args, "O", &listargs ) )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "expected list argument" );
	if( !PyList_Check( listargs ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "expected a list" ) );
	for( i = 0; i < 3; i++ ) {
		PyObject *xx = PyList_GetItem( listargs, i );
		self->curve->size[i] = PyFloat_AsDouble( xx );
	}
	Py_INCREF( Py_None );
	return Py_None;
}


/*
 * Count the number of splines in a Curve Object
 * int getNumCurves()
 */

static PyObject *Curve_getNumCurves( BPy_Curve * self )
{
	Nurb *ptrnurb;
	PyObject *ret_val;
	int num_curves = 0;	/* start with no splines */

	/* get curve */
	ptrnurb = self->curve->nurb.first;
	if( ptrnurb ) {		/* we have some nurbs in this curve */
		while( 1 ) {
			++num_curves;
			ptrnurb = ptrnurb->next;
			if( !ptrnurb )	/* no more curves */
				break;
		}
	}

	ret_val = PyInt_FromLong( ( long ) num_curves );

	if( ret_val )
		return ret_val;

	/* oops! */
	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get number of curves" ) );
}


/*
 * count the number of points in a given spline
 * int getNumPoints( curve_num=0 )
 *
 */

static PyObject *Curve_getNumPoints( BPy_Curve * self, PyObject * args )
{
	Nurb *ptrnurb;
	PyObject *ret_val;
	int curve_num = 0;	/* default spline number */
	int i;

	/* parse input arg */
	if( !PyArg_ParseTuple( args, "|i", &curve_num ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected int argument" ) );

	/* check arg - must be non-negative */
	if( curve_num < 0 )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"argument must be non-negative" ) );


	/* walk the list of curves looking for our curve */
	ptrnurb = self->curve->nurb.first;
	if( !ptrnurb ) {	/* no splines in this Curve */
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"no splines in this Curve" ) );
	}

	for( i = 0; i < curve_num; i++ ) {
		ptrnurb = ptrnurb->next;
		if( !ptrnurb )	/* if zero, we ran just ran out of curves */
			return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
							"curve index out of range" ) );
	}

	/* pntsu is the number of points in curve */
	ret_val = PyInt_FromLong( ( long ) ptrnurb->pntsu );

	if( ret_val )
		return ret_val;

	/* oops! */
	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get number of points for curve" ) );
}

/*
 * Test whether a given spline of a Curve is a nurb
 *  as opposed to a bezier
 * int isNurb( curve_num=0 )
 */

static PyObject *Curve_isNurb( BPy_Curve * self, PyObject * args )
{
	int curve_num = 0;	/* default value */
	int is_nurb;
	Nurb *ptrnurb;
	PyObject *ret_val;
	int i;

	/* parse and check input args */
	if( !PyArg_ParseTuple( args, "|i", &curve_num ) ) {
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected int argument" ) );
	}
	if( curve_num < 0 ) {
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"curve number must be non-negative" ) );
	}

	ptrnurb = self->curve->nurb.first;

	if( !ptrnurb )		/* no splines in this curve */
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"no splines in this Curve" ) );

	for( i = 0; i < curve_num; i++ ) {
		ptrnurb = ptrnurb->next;
		if( !ptrnurb )	/* if zero, we ran just ran out of curves */
			return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
							"curve index out of range" ) );
	}

	/* right now, there are only two curve types, nurb and bezier. */
	is_nurb = ptrnurb->bp ? 1 : 0;

	ret_val = PyInt_FromLong( ( long ) is_nurb );
	if( ret_val )
		return ret_val;

	/* oops */
	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get curve type" ) );
}

/* trying to make a check for closedness (cyclic), following on isNurb (above) 
   copy-pasting done by antont@kyperjokki.fi */

static PyObject *Curve_isCyclic( BPy_Curve * self, PyObject * args )
{
	int curve_num = 0;	/* default value */
	/* unused:*/
	/* int is_cyclic;
	 * PyObject *ret_val;*/
	Nurb *ptrnurb;
	int i;

	/* parse and check input args */
	if( !PyArg_ParseTuple( args, "|i", &curve_num ) ) {
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected int argument" ) );
	}
	if( curve_num < 0 ) {
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"curve number must be non-negative" ) );
	}

	ptrnurb = self->curve->nurb.first;

	if( !ptrnurb )		/* no splines in this curve */
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"no splines in this Curve" ) );

	for( i = 0; i < curve_num; i++ ) {
		ptrnurb = ptrnurb->next;
		if( !ptrnurb )	/* if zero, we ran just ran out of curves */
			return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
							"curve index out of range" ) );
	}

	if(  ptrnurb->flagu & CU_CYCLIC ){
		return EXPP_incr_ret_True();
	} else {
		return EXPP_incr_ret_False();
	}
}


/*
 * Curve_appendPoint( numcurve, new_point )
 * append a new point to indicated spline
 */

static PyObject *Curve_appendPoint( BPy_Curve * self, PyObject * args )
{
	int i;
	int nurb_num;		/* index of curve we append to */
	PyObject *coord_args;	/* coords for new point */
	PyObject *retval = NULL;
	PyObject *valtuple;
	Nurb *nurb = self->curve->nurb.first;	/* first nurb in Curve */

/* fixme - need to malloc new Nurb */
	if( !nurb )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_AttributeError, "no nurbs in this Curve" ) );

	if( !PyArg_ParseTuple( args, "iO", &nurb_num, &coord_args ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_AttributeError,
			   "expected int, coords as arguments" ) );

	/* 
	   chase down the list of Nurbs looking for our curve.
	 */
	for( i = 0; i < nurb_num; i++ ) {
		nurb = nurb->next;
		if( !nurb )	/* we ran off end of list */
			return ( EXPP_ReturnPyObjError
				 ( PyExc_AttributeError,
				   "curve index out of range" ) );
	}

	/* rebuild our arg tuple for appendPointToNurb() */
	valtuple = Py_BuildValue( "(O)", coord_args );
	
	retval =  CurNurb_appendPointToNurb( nurb, valtuple );
	Py_DECREF( valtuple );

	return retval;
}


/****
  appendNurb( new_point )
  create a new nurb in the Curve and add the point param to it.
  returns a refernce to the newly created nurb.
*****/

static PyObject *Curve_appendNurb( BPy_Curve * self, PyObject * args )
{
	Nurb *nurb_ptr = self->curve->nurb.first;
	Nurb **pptr = ( Nurb ** ) & ( self->curve->nurb.first );
	Nurb *new_nurb;


	/* walk to end of nurblist */
	if( nurb_ptr ) {
		while( nurb_ptr->next ) {
			nurb_ptr = nurb_ptr->next;
		}
		pptr = &nurb_ptr->next;
	}

	/* malloc new nurb */
	new_nurb = ( Nurb * ) MEM_callocN( sizeof( Nurb ), "appendNurb" );
	if( !new_nurb )
		return EXPP_ReturnPyObjError
			( PyExc_MemoryError, "unable to malloc Nurb" );

	if( CurNurb_appendPointToNurb( new_nurb, args ) ) {
		*pptr = new_nurb;
		new_nurb->resolu = self->curve->resolu;
		new_nurb->resolv = self->curve->resolv;
		new_nurb->hide = 0;
		new_nurb->flag = 1;


		if( new_nurb->bezt ) {	/* do setup for bezt */
			new_nurb->type = CU_BEZIER;
			new_nurb->bezt->h1 = HD_ALIGN;
			new_nurb->bezt->h2 = HD_ALIGN;
			new_nurb->bezt->f1 = 1;
			new_nurb->bezt->f2 = 1;
			new_nurb->bezt->f3 = 1;
			new_nurb->bezt->hide = 0;
			/* calchandlesNurb( new_nurb ); */
		} else {	/* set up bp */
			new_nurb->pntsv = 1;
			new_nurb->type = CU_NURBS;
			new_nurb->orderu = 4;
			new_nurb->flagu = 0;
			new_nurb->flagv = 0;
			new_nurb->bp->f1 = 0;
			new_nurb->bp->hide = 0;
			new_nurb->knotsu = 0;
			/*makenots( new_nurb, 1, new_nurb->flagu >> 1); */
		}

	} else {
		freeNurb( new_nurb );
		return NULL;	/* with PyErr already set */
	}

	return CurNurb_CreatePyObject( new_nurb );
}


/* 
 *   Curve_update( )
 *   method to update display list for a Curve.
 *   used. after messing with control points
 */

PyObject *Curve_update( BPy_Curve * self )
{
/*	update_displists( ( void * ) self->curve );  */
	freedisplist( &self->curve->disp ); 

	Py_INCREF( Py_None );
	return Py_None;
}

/*
 * Curve_getMaterials
 *
 */

static PyObject *Curve_getMaterials( BPy_Curve * self )
{
	return ( EXPP_PyList_fromMaterialList( self->curve->mat,
					       self->curve->totcol, 1 ) );

}

/*****************************************************************************/
/* Function:    Curve_getBevOb                                               */
/* Description: Get the bevel object assign to the curve.                    */
/*****************************************************************************/
static PyObject *Curve_getBevOb( BPy_Curve * self)
{
	if( self->curve->bevobj ) {
		return Object_CreatePyObject( self->curve->bevobj );
	}

	return EXPP_incr_ret( Py_None );
}

/*****************************************************************************/
/* Function:    Curve_setBevOb                                               */
/* Description: Assign a bevel object to the curve.                          */
/*****************************************************************************/
PyObject *Curve_setBevOb( BPy_Curve * self, PyObject * args )
{
	BPy_Object *pybevobj;

	/* Parse and check input args */
	if( !PyArg_ParseTuple( args, "O", &pybevobj) ) {
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
					"expected object or None argument" ) );
	}

	/* Accept None */
	if( (PyObject *)pybevobj == Py_None ) {
		self->curve->bevobj = (Object *)NULL;
	} else {
	/* Accept Object with type 'Curve' */
		if( Object_CheckPyObject( ( PyObject * ) pybevobj ) && 
			pybevobj->object->type == OB_CURVE) {
			self->curve->bevobj = 
				Object_FromPyObject( ( PyObject * ) pybevobj );
		} else {
			return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected Curve object type or None argument" ) );
		}
	}

	return EXPP_incr_ret( Py_None );
}

/*
 * Curve_getIter
 *
 * create an iterator for our Curve.
 * this iterator returns the Nurbs for this Curve.
 * the iter_pointer always points to the next available item or null
 */

static PyObject *Curve_getIter( BPy_Curve * self )
{
	self->iter_pointer = self->curve->nurb.first;

	Py_INCREF( self );
	return ( PyObject * ) self;

}


/*
 * Curve_iterNext
 *  get the next item.
 *  iter_pointer always points to the next available element
 *   or NULL if at the end of the list.
 */

static PyObject *Curve_iterNext( BPy_Curve * self )
{
	PyObject *po;		/* return value */
	Nurb *pnurb;

	if( self->iter_pointer ) {
		pnurb = self->iter_pointer;
		self->iter_pointer = pnurb->next;	/* advance iterator */
		po = CurNurb_CreatePyObject( pnurb );	/* make a bpy_nurb */

		return ( PyObject * ) po;
	}

	/* if iter_pointer was null, we are at end */
	return ( EXPP_ReturnPyObjError
		 ( PyExc_StopIteration, "iterator at end" ) );
}



/* tp_sequence methods */

/*
 * Curve_length
 * returns the number of curves in a Curve
 * this is a tp_as_sequence method, not a regular instance method.
 */

static int Curve_length( PyInstanceObject * inst )
{
	if( Curve_CheckPyObject( ( PyObject * ) inst ) )
		return ( ( int ) PyInt_AsLong
			 ( Curve_getNumCurves( ( BPy_Curve * ) inst ) ) );

	return EXPP_ReturnIntError( PyExc_RuntimeError,
				    "arg is not a BPy_Curve" );

}



/*
 * Curve_getNurb
 * returns the Nth nurb in a Curve.
 * this is one of the tp_as_sequence methods, hence the int N argument.
 * it is called via the [] operator, not as a usual instance method.
 */

PyObject *Curve_getNurb( BPy_Curve * self, int n )
{
	PyObject *pyo;
	Nurb *pNurb;
	int i;

	/* bail if index < 0 */
	if( n < 0 )
		return ( EXPP_ReturnPyObjError( PyExc_IndexError,
						"index less than 0" ) );
	/* bail if no Nurbs in Curve */
	if( self->curve->nurb.first == 0 )
		return ( EXPP_ReturnPyObjError( PyExc_IndexError,
						"no Nurbs in this Curve" ) );
	/* set pointer to nth Nurb */
	for( pNurb = self->curve->nurb.first, i = 0;
	     pNurb != 0 && i < n; pNurb = pNurb->next, ++i )
		/**/;

	if( !pNurb )		/* we came to the end of the list */
		return ( EXPP_ReturnPyObjError( PyExc_IndexError,
						"index out of range" ) );

	pyo = CurNurb_CreatePyObject( pNurb );	/* make a bpy_curnurb */
	return ( PyObject * ) pyo;

}



/*****************************************************************************/
/* Function:    CurveDeAlloc                                                 */
/* Description: This is a callback function for the BPy_Curve type. It is    */
/*              the destructor function.                                     */
/*****************************************************************************/
static void CurveDeAlloc( BPy_Curve * self )
{
	PyObject_DEL( self );
}

/*****************************************************************************/
/* Function:    CurveGetAttr                                                 */
/* Description: This is a callback function for the BPy_Curve type. It is    */
/*              the function that accesses BPy_Curve "member variables" and  */
/*              methods.                                                     */
/*****************************************************************************/
static PyObject *CurveGetAttr( BPy_Curve * self, char *name )
{				/* getattr */
	PyObject *attr = Py_None;

	if( strcmp( name, "name" ) == 0 )
		attr = PyString_FromString( self->curve->id.name + 2 );
	if( strcmp( name, "pathlen" ) == 0 )
		attr = PyInt_FromLong( self->curve->pathlen );
	if( strcmp( name, "totcol" ) == 0 )
		attr = PyInt_FromLong( self->curve->totcol );
	if( strcmp( name, "flag" ) == 0 )
		attr = PyInt_FromLong( self->curve->flag );
	if( strcmp( name, "bevresol" ) == 0 )
		attr = PyInt_FromLong( self->curve->bevresol );
	if( strcmp( name, "resolu" ) == 0 )
		attr = PyInt_FromLong( self->curve->resolu );
	if( strcmp( name, "resolv" ) == 0 )
		attr = PyInt_FromLong( self->curve->resolv );
	if( strcmp( name, "width" ) == 0 )
		attr = PyFloat_FromDouble( self->curve->width );
	if( strcmp( name, "ext1" ) == 0 )
		attr = PyFloat_FromDouble( self->curve->ext1 );
	if( strcmp( name, "ext2" ) == 0 )
		attr = PyFloat_FromDouble( self->curve->ext2 );
	if( strcmp( name, "loc" ) == 0 )
		return Curve_getLoc( self );
	if( strcmp( name, "rot" ) == 0 )
		return Curve_getRot( self );
	if( strcmp( name, "size" ) == 0 )
		return Curve_getSize( self );
	if( strcmp( name, "bevob" ) == 0 )
		return Curve_getBevOb( self );
#if 0
	if( strcmp( name, "numpts" ) == 0 )
		return Curve_getNumPoints( self );
#endif


	if( !attr )
		return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
						"couldn't create PyObject" ) );

	if( attr != Py_None )
		return attr;	/* member attribute found, return it */

	/* not an attribute, search the methods table */
	return Py_FindMethod( BPy_Curve_methods, ( PyObject * ) self, name );
}

/*****************************************************************************/
/* Function:    CurveSetAttr                                                 */
/* Description: This is a callback function for the BPy_Curve type. It      */
/*              sets Curve Data attributes (member variables). */
/*****************************************************************************/
static int CurveSetAttr( BPy_Curve * self, char *name, PyObject * value )
{
	PyObject *valtuple;
	PyObject *error = NULL;
	valtuple = Py_BuildValue( "(O)", value );
	/* resolu resolv width ext1 ext2  */
	if( !valtuple )
		return EXPP_ReturnIntError( PyExc_MemoryError,
					    "CurveSetAttr: couldn't create PyTuple" );

	if( strcmp( name, "name" ) == 0 )
		error = Curve_setName( self, valtuple );
	else if( strcmp( name, "pathlen" ) == 0 )
		error = Curve_setPathLen( self, valtuple );
	else if( strcmp( name, "bevresol" ) == 0 )
		error = Curve_setBevresol( self, valtuple );
	else if( strcmp( name, "resolu" ) == 0 )
		error = Curve_setResolu( self, valtuple );
	else if( strcmp( name, "resolv" ) == 0 )
		error = Curve_setResolv( self, valtuple );
	else if( strcmp( name, "width" ) == 0 )
		error = Curve_setWidth( self, valtuple );
	else if( strcmp( name, "ext1" ) == 0 )
		error = Curve_setExt1( self, valtuple );
	else if( strcmp( name, "ext2" ) == 0 )
		error = Curve_setExt2( self, valtuple );
	else if( strcmp( name, "loc" ) == 0 )
		error = Curve_setLoc( self, valtuple );
	else if( strcmp( name, "rot" ) == 0 )
		error = Curve_setRot( self, valtuple );
	else if( strcmp( name, "size" ) == 0 )
		error = Curve_setSize( self, valtuple );
	else if( strcmp( name, "bevob" ) == 0 )
		error = Curve_setBevOb( self, valtuple );

	else {			/* Error */
		Py_DECREF( valtuple );

		if( ( strcmp( name, "Types" ) == 0 )
		    || ( strcmp( name, "Modes" ) == 0 ) )
			return ( EXPP_ReturnIntError
				 ( PyExc_AttributeError,
				   "constant dictionary -- cannot be changed" ) );

		else
			return ( EXPP_ReturnIntError
				 ( PyExc_KeyError, "attribute not found" ) );
	}

	Py_DECREF( valtuple );

	if( error != Py_None )
		return -1;
	Py_DECREF( Py_None );
	return 0;
}


/*****************************************************************************/
/* Function:    CurveRepr                                                    */
/* Description: This is a callback function for the BPy_Curve type. It       */
/*              builds a meaninful string to represent curve objects.        */
/*****************************************************************************/
static PyObject *CurveRepr( BPy_Curve * self )
{				/* used by 'repr' */

	return PyString_FromFormat( "[Curve \"%s\"]",
				    self->curve->id.name + 2 );
}


/*
 * Curve_CreatePyObject
 * constructor to build a py object from blender data 
 */

PyObject *Curve_CreatePyObject( struct Curve * curve )
{
	BPy_Curve *blen_object;

	blen_object = ( BPy_Curve * ) PyObject_NEW( BPy_Curve, &Curve_Type );

	if( blen_object == NULL ) {
		return ( NULL );
	}
	blen_object->curve = curve;
	return ( ( PyObject * ) blen_object );

}

int Curve_CheckPyObject( PyObject * py_obj )
{
	return ( py_obj->ob_type == &Curve_Type );
}


struct Curve *Curve_FromPyObject( PyObject * py_obj )
{
	BPy_Curve *blen_obj;

	blen_obj = ( BPy_Curve * ) py_obj;
	return ( blen_obj->curve );

}



/*
 * NOTE:  this func has been replaced by freedisplist() in the recent
 *        display list refactoring.
 *
 * walk across all objects looking for curves
 *  so we can update their ob's disp list
 */

void update_displists( void *data )
{
#if 0
	Base *base;
	Object *ob;
	unsigned int layer;

	/* background */
	layer = G.scene->lay;

	base = G.scene->base.first;
	while( base ) {
		if( base->lay & layer ) {
			ob = base->object;

			if( ELEM( ob->type, OB_CURVE, OB_SURF ) ) {
				if( ob != G.obedit ) {
					if( ob->data == data ) {
						makeDispList( ob );
					}
				}
			} else if( ob->type == OB_FONT ) {
				Curve *cu = ob->data;
				if( cu->textoncurve ) {
					if( ( ( Curve * ) cu->textoncurve->
					      data )->key ) {
						text_to_curve( ob, 0 );
						makeDispList( ob );
					}
				}
			}
		}
		if( base->next == 0 && G.scene->set
		    && base == G.scene->base.last )
			base = G.scene->set->base.first;
		else
			base = base->next;
	}
#endif
}

