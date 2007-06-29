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

#include "Curve.h" /*This must come first*/

#include "BKE_main.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_curve.h"
#include "BKE_material.h"
#include "MEM_guardedalloc.h"	/* because we wil be mallocing memory */
#include "CurNurb.h"
#include "SurfNurb.h"
#include "Material.h"
#include "Object.h"
#include "Key.h"
#include "gen_utils.h"
#include "gen_library.h"
#include "mydevice.h"


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

static PyObject *Curve_getPathLen( BPy_Curve * self );
static PyObject *Curve_setPathLen( BPy_Curve * self, PyObject * args );
static PyObject *Curve_getTotcol( BPy_Curve * self );
static PyObject *Curve_setTotcol( BPy_Curve * self, PyObject * args );
#if 0
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
#endif
static PyObject *Curve_getControlPoint( BPy_Curve * self, PyObject * args );
static PyObject *Curve_setControlPoint( BPy_Curve * self, PyObject * args );
static PyObject *Curve_getLoc( BPy_Curve * self );
static PyObject *Curve_setLoc( BPy_Curve * self, PyObject * args );
static PyObject *Curve_getRot( BPy_Curve * self );
static PyObject *Curve_setRot( BPy_Curve * self, PyObject * args );
static PyObject *Curve_getSize( BPy_Curve * self );
static PyObject *Curve_setSize( BPy_Curve * self, PyObject * args );
static PyObject *Curve_getNumCurves( BPy_Curve * self );
static PyObject *Curve_getKey( BPy_Curve * self );
static PyObject *Curve_isNurb( BPy_Curve * self, PyObject * args );
static PyObject *Curve_isCyclic( BPy_Curve * self, PyObject * args);
static PyObject *Curve_getNumPoints( BPy_Curve * self, PyObject * args );

static PyObject *Curve_appendPoint( BPy_Curve * self, PyObject * args );
static PyObject *Curve_appendNurb( BPy_Curve * self, PyObject * args );

static PyObject *Curve_getMaterials( BPy_Curve * self );

static PyObject *Curve_getBevOb( BPy_Curve * self );
static PyObject *Curve_setBevOb( BPy_Curve * self, PyObject * args );

static PyObject *Curve_getTaperOb( BPy_Curve * self );
static PyObject *Curve_setTaperOb( BPy_Curve * self, PyObject * args );
static PyObject *Curve_copy( BPy_Curve * self );

static PyObject *Curve_getIter( BPy_Curve * self );
static PyObject *Curve_iterNext( BPy_Curve * self );

PyObject *Curve_getNurb( BPy_Curve * self, int n );
static int Curve_length( PyInstanceObject * inst );


struct chartrans *text_to_curve( Object * ob, int mode );
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
	return PyString_FromString( self->curve->id.name + 2 );
}

static int Curve_newsetName( BPy_Curve * self, PyObject * args )
{
	char *name;

	name = PyString_AsString( args );
	if( !name )
		return EXPP_ReturnIntError( PyExc_TypeError,
					      "expected string argument" );

	rename_id( &self->curve->id, name );	/* proper way in Blender */
	Curve_update( self );

	return 0;
}

static PyObject *Curve_getPathLen( BPy_Curve * self )
{
	return PyInt_FromLong( ( long ) self->curve->pathlen );
}


static int Curve_newsetPathLen( BPy_Curve * self, PyObject * args )
{
	PyObject *num;

	if( !PyNumber_Check( args ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected int argument" );

	num = PyNumber_Int( args );
	self->curve->pathlen = (short)PyInt_AS_LONG( num );
	Py_DECREF( num );

	return 0;
}

static PyObject *Curve_getTotcol( BPy_Curve * self )
{
	return PyInt_FromLong( ( long ) self->curve->totcol );
}


PyObject *Curve_getMode( BPy_Curve * self )
{
	return PyInt_FromLong( ( long ) self->curve->flag );
}


static int Curve_newsetMode( BPy_Curve * self, PyObject * args )
{
	PyObject *num;

	if( !PyNumber_Check( args ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected int argument" );

	num = PyNumber_Int( args );
	self->curve->flag = (short)PyInt_AS_LONG( num );
	Py_DECREF( num );

	return 0;
}

PyObject *Curve_getBevresol( BPy_Curve * self )
{
	return PyInt_FromLong( ( long ) self->curve->bevresol );
}

static int Curve_newsetBevresol( BPy_Curve * self, PyObject * args )
{
	short value;
	PyObject *num;

	if( !PyNumber_Check( args ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected int argument" );

	num = PyNumber_Int( args );
	value = (short)PyInt_AS_LONG( num );
	Py_DECREF( num );

	if( value > 10 || value < 0 )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"acceptable values are between 0 and 10" );

	self->curve->bevresol = value;
	return 0;
}


PyObject *Curve_getResolu( BPy_Curve * self )
{
	return PyInt_FromLong( ( long ) self->curve->resolu );
}


static int Curve_newsetResolu( BPy_Curve * self, PyObject * args )
{
	short value;
	Nurb *nu;
	PyObject *num;

	if( !PyNumber_Check( args ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected int argument" );

	num = PyNumber_Int( args );
	value = (short)PyInt_AS_LONG( num );
	Py_DECREF( num );

	if( value > 128 || value < 1 )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"acceptable values are between 1 and 128" );

	self->curve->resolu = value;
	/* propagate the change through all the curves */
	for( nu = self->curve->nurb.first; nu; nu = nu->next )
		nu->resolu = value;

	return 0;
}

PyObject *Curve_getResolv( BPy_Curve * self )
{
	return PyInt_FromLong( ( long ) self->curve->resolv );
}

static int Curve_newsetResolv( BPy_Curve * self, PyObject * args )
{
	short value;
	PyObject *num;

	if( !PyNumber_Check( args ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected int argument" );

	num = PyNumber_Int( args );
	value = (short)PyInt_AS_LONG( num );
	Py_DECREF( num );

	if(value > 128 || value < 1)
		return EXPP_ReturnIntError( PyExc_ValueError,
			"acceptable values are between 1 and 128" );
	self->curve->resolv = value;

	return 0;
}

PyObject *Curve_getWidth( BPy_Curve * self )
{
	return PyFloat_FromDouble( ( double ) self->curve->width );
}


static int Curve_newsetWidth( BPy_Curve * self, PyObject * args )
{
	float value;
	PyObject *num;

	if( !PyNumber_Check( args ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected float argument" );

	num = PyNumber_Float( args );
	value = (float)PyFloat_AS_DOUBLE( num );
	Py_DECREF( num );

	if(value > 2.0f || value < 0.0f)
		return EXPP_ReturnIntError( PyExc_ValueError,
				"acceptable values are between 2.0 and 0.0" );
	self->curve->width = value;

	return 0;
}


PyObject *Curve_getExt1( BPy_Curve * self )
{
	return PyFloat_FromDouble( ( double ) self->curve->ext1 );
}


static int Curve_newsetExt1( BPy_Curve * self, PyObject * args )
{
	float value;
	PyObject *num;

	if( !PyNumber_Check( args ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected float argument" );

	num = PyNumber_Float( args );
	value = (float)PyFloat_AS_DOUBLE( num );
	Py_DECREF( num );

	if(value > 5.0f || value < 0.0f)
		return EXPP_ReturnIntError( PyExc_ValueError,
				"acceptable values are between 0.0 and 5.0" );
	self->curve->ext1 = value;

	return 0;
}

PyObject *Curve_getExt2( BPy_Curve * self )
{
	return PyFloat_FromDouble( ( double ) self->curve->ext2 );
}


static int Curve_newsetExt2( BPy_Curve * self, PyObject * args )
{
	float value;
	PyObject *num;

	if( !PyNumber_Check( args ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected float argument" );

	num = PyNumber_Float( args );
	value = (float)PyFloat_AS_DOUBLE( num );
	Py_DECREF( num );

	if(value > 2.0f || value < 0.0f)
		return EXPP_ReturnIntError( PyExc_ValueError,
				"acceptable values are between 0.0 and 2.0" );
	self->curve->ext2 = value;

	return 0;
}

/*
 * Curve_setControlPoint
 * this function sets an EXISTING control point.
 * it does NOT add a new one.
 */

static PyObject *Curve_setControlPoint( BPy_Curve * self, PyObject * args )
{
	PyObject *listargs = 0;
	Nurb *ptrnurb = self->curve->nurb.first;
	int numcourbe = 0, numpoint = 0, i, j;

	if( !ptrnurb )
		Py_RETURN_NONE;

	if( ptrnurb->bp )
		if( !PyArg_ParseTuple
		    ( args, "iiO", &numcourbe, &numpoint, &listargs ) )
			return ( EXPP_ReturnPyObjError
				 ( PyExc_TypeError,
				   "expected int, int, list arguments" ) );
	if( ptrnurb->bezt )
		if( !PyArg_ParseTuple
		    ( args, "iiO", &numcourbe, &numpoint, &listargs ) )
			return ( EXPP_ReturnPyObjError
				 ( PyExc_TypeError,
				   "expected int, int, list arguments" ) );

	for( i = 0; i < numcourbe; i++ )
		ptrnurb = ptrnurb->next;

	if( ptrnurb->bp )
		for( i = 0; i < 4; i++ )
			ptrnurb->bp[numpoint].vec[i] =
				(float)PyFloat_AsDouble( PyList_GetItem ( listargs, i ) );

	if( ptrnurb->bezt )
		for( i = 0; i < 3; i++ )
			for( j = 0; j < 3; j++ )
				ptrnurb->bezt[numpoint].vec[i][j] =
					(float)PyFloat_AsDouble( PyList_GetItem
							  ( listargs,
							    i * 3 + j ) );

	Py_RETURN_NONE;
}


static PyObject *Curve_getControlPoint( BPy_Curve * self, PyObject * args )
{
	PyObject *liste;
	PyObject *item;

	Nurb *ptrnurb;
	int i, j;
	/* input args: requested curve and point number on curve */
	int numcourbe, numpoint;

	if( !PyArg_ParseTuple( args, "ii", &numcourbe, &numpoint ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected int int arguments" ) );
	if( ( numcourbe < 0 ) || ( numpoint < 0 ) )
		return ( EXPP_ReturnPyObjError( PyExc_ValueError,
						"arguments must be non-negative" ) );

	/* if no nurbs in this curve obj */
	if( !self->curve->nurb.first )
		return PyList_New( 0 );

	/* walk the list of nurbs to find requested numcourbe */
	ptrnurb = self->curve->nurb.first;
	for( i = 0; i < numcourbe; i++ ) {
		ptrnurb = ptrnurb->next;
		if( !ptrnurb )	/* if zero, we ran just ran out of curves */
			return ( EXPP_ReturnPyObjError( PyExc_ValueError,
							"curve index out of range" ) );
	}

	/* check numpoint param against pntsu */
	if( numpoint >= ptrnurb->pntsu )
		return ( EXPP_ReturnPyObjError( PyExc_ValueError,
						"point index out of range" ) );
	
	liste = PyList_New( 0 );
	if( ptrnurb->bp ) {	/* if we are a nurb curve, you get 4 values */
		for( i = 0; i < 4; i++ ) {
			item = PyFloat_FromDouble( ptrnurb->bp[numpoint].vec[i] );
			PyList_Append( liste, item );
			Py_DECREF(item);
		}
	} else if( ptrnurb->bezt ) {	/* if we are a bezier, you get 9 values */
		for( i = 0; i < 3; i++ )
			for( j = 0; j < 3; j++ ) {
				item = PyFloat_FromDouble( ptrnurb->bezt[numpoint].vec[i][j] );
				PyList_Append( liste, item );
				Py_DECREF(item);
			}
	}

	return liste;
}

static PyObject *Curve_getLoc( BPy_Curve * self )
{
	return Py_BuildValue( "[f,f,f]", self->curve->loc[0],
				self->curve->loc[1], self->curve->loc[2] );
}

static int Curve_newsetLoc( BPy_Curve * self, PyObject * args )
{
	float loc[3];
	int i;

	if( ( !PyList_Check( args ) && !PyTuple_Check( args ) ) ||
			PySequence_Size( args ) != 3 ) {
TypeError:
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected a sequence of three floats" );
	}

	for( i = 0; i < 3; i++ ) {
		PyObject *item = PySequence_GetItem( args, i );
		PyObject *num = PyNumber_Float( item );
		Py_DECREF( item );
		if( !num )
			goto TypeError;
		loc[i] = PyFloat_AS_DOUBLE( num );
		Py_DECREF( num );
	}
	memcpy( self->curve->loc, loc, sizeof( loc ) );

	return 0;
}

static PyObject *Curve_getRot( BPy_Curve * self )
{
	return Py_BuildValue( "[f,f,f]", self->curve->rot[0],
				self->curve->rot[1], self->curve->rot[2] );
}

static int Curve_newsetRot( BPy_Curve * self, PyObject * args )
{
	float rot[3];
	int i;

	if( ( !PyList_Check( args ) && !PyTuple_Check( args ) ) ||
			PySequence_Size( args ) != 3 ) {
TypeError:
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected a sequence of three floats" );
	}

	for( i = 0; i < 3; i++ ) {
		PyObject *item = PySequence_GetItem( args, i );
		PyObject *num = PyNumber_Float( item );
		Py_DECREF( item );
		if( !num )
			goto TypeError;
		rot[i] = PyFloat_AS_DOUBLE( num );
		Py_DECREF( num );
	}
	memcpy( self->curve->rot, rot, sizeof( rot ) );

	return 0;
}

static PyObject *Curve_getSize( BPy_Curve * self )
{
	return Py_BuildValue( "[f,f,f]", self->curve->size[0],
				self->curve->size[1], self->curve->size[2] );
}

static int Curve_newsetSize( BPy_Curve * self, PyObject * args )
{
	float size[3];
	int i;

	if( ( !PyList_Check( args ) && !PyTuple_Check( args ) ) ||
			PySequence_Size( args ) != 3 ) {
TypeError:
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected a sequence of three floats" );
	}

	for( i = 0; i < 3; i++ ) {
		PyObject *item = PySequence_GetItem( args, i );
		PyObject *num = PyNumber_Float( item );
		Py_DECREF( item );
		if( !num )
			goto TypeError;
		size[i] = PyFloat_AS_DOUBLE( num );
		Py_DECREF( num );
	}
	memcpy( self->curve->size, size, sizeof( size ) );

	return 0;
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
		for(;;) {
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
	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get number of curves" );
}

/*
 * get the key object linked to this curve
 */

static PyObject *Curve_getKey( BPy_Curve * self )
{
	PyObject *keyObj;

	if (self->curve->key)
		keyObj = Key_CreatePyObject(self->curve->key);
	else keyObj = EXPP_incr_ret(Py_None);

	return keyObj;
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
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected int argument" ) );

	/* check arg - must be non-negative */
	if( curve_num < 0 )
		return ( EXPP_ReturnPyObjError( PyExc_ValueError,
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
			return ( EXPP_ReturnPyObjError( PyExc_ValueError,
							"curve index out of range" ) );
	}

	/* pntsu is the number of points in curve */
	ret_val = PyInt_FromLong( ( long ) ptrnurb->pntsu );

	if( ret_val )
		return ret_val;

	/* oops! */
	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"couldn't get number of points for curve" );
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
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected int argument" ) );
	}
	if( curve_num < 0 ) {
		return ( EXPP_ReturnPyObjError( PyExc_ValueError,
						"curve number must be non-negative" ) );
	}

	ptrnurb = self->curve->nurb.first;

	if( !ptrnurb )		/* no splines in this curve */
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"no splines in this Curve" ) );

	for( i = 0; i < curve_num; i++ ) {
		ptrnurb = ptrnurb->next;
		if( !ptrnurb )	/* if zero, we ran just ran out of curves */
			return ( EXPP_ReturnPyObjError( PyExc_ValueError,
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
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected int argument" ) );
	}
	if( curve_num < 0 ) {
		return ( EXPP_ReturnPyObjError( PyExc_ValueError,
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
			 ( PyExc_TypeError,
			   "expected int, coords as arguments" ) );

	/* 
	   chase down the list of Nurbs looking for our curve.
	 */
	for( i = 0; i < nurb_num; i++ ) {
		nurb = nurb->next;
		if( !nurb )	/* we ran off end of list */
			return EXPP_ReturnPyObjError( PyExc_ValueError,
					"curve index out of range" );
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
	Nurb *nu = self->curve->nurb.first;

	/* recalculate handles for each curve: calchandlesNurb() will make
	 * sure curves are bezier first */
	while( nu ) {
		calchandlesNurb ( nu );
		nu = nu->next;
	}

	Object_updateDag( (void*) self->curve );

	Py_RETURN_NONE;
}

/*
 * Curve_getMaterials
 *
 */

static PyObject *Curve_getMaterials( BPy_Curve * self )
{
	return EXPP_PyList_fromMaterialList( self->curve->mat,
			self->curve->totcol, 1 );
}

static int Curve_setMaterials( BPy_Curve *self, PyObject * value )
{
	Material **matlist;
	int len;

	if( !PySequence_Check( value ) ||
			!EXPP_check_sequence_consistency( value, &Material_Type ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"sequence should only contain materials or None)" );

	len = PySequence_Size( value );
	if( len > 16 )
		return EXPP_ReturnIntError( PyExc_TypeError,
			"list can't have more than 16 materials" );

	/* free old material list (if it exists) and adjust user counts */
	if( self->curve->mat ) {
		Curve *cur = self->curve;
		int i;
		for( i = cur->totcol; i-- > 0; )
			if( cur->mat[i] )
           		cur->mat[i]->id.us--;
		MEM_freeN( cur->mat );
	}

	/* build the new material list, increment user count, store it */

	matlist = EXPP_newMaterialList_fromPyList( value );
	EXPP_incr_mats_us( matlist, len );
	self->curve->mat = matlist;
	self->curve->totcol = (short)len;

/**@ This is another ugly fix due to the weird material handling of blender.
    * it makes sure that object material lists get updated (by their length)
    * according to their data material lists, otherwise blender crashes.
    * It just stupidly runs through all objects...BAD BAD BAD.
    */

	test_object_materials( ( ID * ) self->curve );

	return 0;
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
/* Function:    Curve_newsetBevOb                                            */
/* Description: Assign a bevel object to the curve.                          */
/*****************************************************************************/
static int Curve_newsetBevOb( BPy_Curve * self, PyObject * args )
{
	
	if (BPy_Object_Check( args ) && ((BPy_Object *)args)->object->data == self->curve )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"Can't bevel an object to itself" );
	
	return GenericLib_assignData(args, (void **) &self->curve->bevobj, 0, 0, ID_OB, OB_CURVE);
}

/*****************************************************************************/
/* Function:    Curve_getTaperOb                                             */
/* Description: Get the taper object assign to the curve.                    */
/*****************************************************************************/

static PyObject *Curve_getTaperOb( BPy_Curve * self)
{
	if( self->curve->taperobj )
		return Object_CreatePyObject( self->curve->taperobj );

	Py_RETURN_NONE;
}

/*****************************************************************************/
/* Function:    Curve_newsetTaperOb                                          */
/* Description: Assign a taper object to the curve.                          */
/*****************************************************************************/

static int Curve_newsetTaperOb( BPy_Curve * self, PyObject * args )
{
	if (BPy_Object_Check( args ) && ((BPy_Object *)args)->object->data == self->curve )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"Can't taper an object to itself" );
	
	return GenericLib_assignData(args, (void **) &self->curve->taperobj, 0, 0, ID_OB, OB_CURVE);
}

/*****************************************************************************/
/* Function:    Curve_copy                                                   */
/* Description: Return a copy of this curve data.                            */
/*****************************************************************************/

PyObject *Curve_copy( BPy_Curve * self )
{
	BPy_Curve *pycurve;	/* for Curve Data object wrapper in Python */
	Curve *blcurve = 0;	/* for actual Curve Data we create in Blender */

	/* copies the data */
	blcurve = copy_curve( self->curve );	/* first create the Curve Data in Blender */

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
	return ( PyObject * ) pycurve;
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
	Nurb *pnurb;

	if( self->iter_pointer ) {
		pnurb = self->iter_pointer;
		self->iter_pointer = pnurb->next;	/* advance iterator */
		if( (pnurb->type & 7) == CU_BEZIER || pnurb->pntsv <= 1 )
			return CurNurb_CreatePyObject( pnurb ); /* make a bpy_curnurb */
		else
			return SurfNurb_CreatePyObject( pnurb ); /* make a bpy_surfnurb */
	}

	/* if iter_pointer was null, we are at end */
	return EXPP_ReturnPyObjError( PyExc_StopIteration,
			"iterator at end" );
}

/* tp_sequence methods */

/*
 * Curve_length
 * returns the number of curves in a Curve
 * this is a tp_as_sequence method, not a regular instance method.
 */

static int Curve_length( PyInstanceObject * inst )
{
	if( BPy_Curve_Check( ( PyObject * ) inst ) )
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

	/* until there is a Surface BPyType, distinquish between a curve and a
	 * surface based on whether it's a Bezier and the v size */
	if( (pNurb->type & 7) == CU_BEZIER || pNurb->pntsv <= 1 )
		return CurNurb_CreatePyObject( pNurb );	/* make a bpy_curnurb */
	else
		return SurfNurb_CreatePyObject( pNurb );	/* make a bpy_surfnurb */

}

/*****************************************************************************/
/* Function:    Curve_compare		                                         */
/* Description: This compares 2 curve python types, == or != only.			 */
/*****************************************************************************/
static int Curve_compare( BPy_Curve * a, BPy_Curve * b )
{
	return ( a->curve == b->curve ) ? 0 : -1;
}

/*****************************************************************************/
/* Function:    Curve_repr                                                   */
/* Description: This is a callback function for the BPy_Curve type. It       */
/*              builds a meaninful string to represent curve objects.        */
/*****************************************************************************/
static PyObject *Curve_repr( BPy_Curve * self )
{				/* used by 'repr' */

	return PyString_FromFormat( "[Curve \"%s\"]",
				    self->curve->id.name + 2 );
}

/* attributes for curves */

static PyGetSetDef Curve_getseters[] = {
	GENERIC_LIB_GETSETATTR,
	{"pathlen",
	 (getter)Curve_getPathLen, (setter)Curve_newsetPathLen,
	 "The path length,  used to set the number of frames for an animation (not the physical length)",
	NULL},
	{"totcol",
	 (getter)Curve_getTotcol, (setter)NULL,
	 "The maximum number of linked materials",
	NULL},
	{"flag",
	 (getter)Curve_getMode, (setter)Curve_newsetMode,
	 "The flag bitmask",
	NULL},
	{"bevresol",
	 (getter)Curve_getBevresol, (setter)Curve_newsetBevresol,
	 "The bevel resolution",
	NULL},
	{"resolu",
	 (getter)Curve_getResolu, (setter)Curve_newsetResolu,
	 "The resolution in U direction",
	NULL},
	{"resolv",
	 (getter)Curve_getResolv, (setter)Curve_newsetResolv,
	 "The resolution in V direction",
	NULL},
	{"width",
	 (getter)Curve_getWidth, (setter)Curve_newsetWidth,
	 "The curve width",
	NULL},
	{"ext1",
	 (getter)Curve_getExt1, (setter)Curve_newsetExt1,
	 "The extent1 value (for bevels)",
	NULL},
	{"ext2",
	 (getter)Curve_getExt2, (setter)Curve_newsetExt2,
	 "The extent2 value (for bevels)",
	NULL},
	{"loc",
	 (getter)Curve_getLoc, (setter)Curve_newsetLoc,
	 "The data location (from the center)",
	NULL},
	{"rot",
	 (getter)Curve_getRot, (setter)Curve_newsetRot,
	 "The data rotation (from the center)",
	NULL},
	{"size",
	 (getter)Curve_getSize, (setter)Curve_newsetSize,
	 "The data size (from the center)",
	NULL},
	{"bevob",
	 (getter)Curve_getBevOb, (setter)Curve_newsetBevOb,
	 "The bevel object",
	NULL},
	{"taperob",
	 (getter)Curve_getTaperOb, (setter)Curve_newsetTaperOb,
	 "The taper object",
	NULL},
	{"key",
	 (getter)Curve_getKey, (setter)NULL,
	 "The shape key for the curve (if any)",
	NULL},
	{"materials",
	 (getter)Curve_getMaterials, (setter)Curve_setMaterials,
	 "The materials associated with the curve",
	NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */

};

/*****************************************************************************/
/* Function:              M_Curve_New                                       */
/* Python equivalent:     Blender.Curve.New                                 */
/*****************************************************************************/
static PyObject *M_Curve_New( PyObject * self, PyObject * args )
{
	char *name = "Curve";
	BPy_Curve *pycurve;	/* for Curve Data object wrapper in Python */
	Curve *blcurve = 0;	/* for actual Curve Data we create in Blender */

	if( !PyArg_ParseTuple( args, "|s", &name ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError,
			   "expected string argument or no argument" ) );

	blcurve = add_curve( name, OB_CURVE );	/* first create the Curve Data in Blender */

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
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
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
			Py_DECREF(found_cur);
			curv_iter = curv_iter->id.next;
		}

		return ( curvlist );
	}			/* end of else */
}

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
	{"getKey", ( PyCFunction ) Curve_getKey,
	 METH_NOARGS, "() - Gets curve key"},
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
	{"getTaperOb", ( PyCFunction ) Curve_getTaperOb, METH_NOARGS,
	 "() - returns Taper Object assigned to this Curve"},
	{"setTaperOb", ( PyCFunction ) Curve_setTaperOb, METH_VARARGS,
	 "() - assign a Taper Object to this Curve"},
	{"__copy__", ( PyCFunction ) Curve_copy, METH_NOARGS,
	 "() - make a copy of this curve data"},
	{"copy", ( PyCFunction ) Curve_copy, METH_NOARGS,
	 "() - make a copy of this curve data"},
	{NULL, NULL, 0, NULL}
};


/*****************************************************************************/
/*  Python Curve_Type callback function prototypes:                         */
/*****************************************************************************/
static int Curve_compare( BPy_Curve * a, BPy_Curve * b );
static PyObject *Curve_repr( BPy_Curve * msh );

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
	PyObject_HEAD_INIT( NULL )          /* required macro */ 
	0,                                  /* ob_size */
	"Curve",                            /* tp_name */
	sizeof( BPy_Curve ),                /* tp_basicsize */
	0,                                  /* tp_itemsize */
	/* methods */
	NULL, 								/* tp_dealloc */
	0,                                  /* tp_print */
	( getattrfunc ) NULL,	            /* tp_getattr */
	( setattrfunc ) NULL,	            /* tp_setattr */
	( cmpfunc ) Curve_compare,          /* tp_compare */
	( reprfunc ) Curve_repr,            /* tp_repr */
	/* Method suites for standard classes */

	NULL,                               /* PyNumberMethods *tp_as_number; */
	&Curve_as_sequence,                 /* PySequenceMethods *tp_as_sequence; */
	NULL,                               /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	( hashfunc ) GenericLib_hash,		/* hashfunc tp_hash; */
	NULL,                       	    /* ternaryfunc tp_call; */
	NULL,                       	    /* reprfunc tp_str; */
	NULL,                       	    /* getattrofunc tp_getattro; */
	NULL,                      	    /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       	    /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,                 /* long tp_flags; */

	NULL,                               /*  char *tp_doc;  */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                               /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       	    /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       	    /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          	    /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	( getiterfunc ) Curve_getIter,	    /* getiterfunc tp_iter; */
	( iternextfunc ) Curve_iterNext,	/* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_Curve_methods,           	    /* struct PyMethodDef *tp_methods; */
	NULL,                       	    /* struct PyMemberDef *tp_members; */
	Curve_getseters,                    /* struct PyGetSetDef *tp_getset; */
	NULL,                       	    /* struct _typeobject *tp_base; */
	NULL,                       	    /* PyObject *tp_dict; */
	NULL,                       	    /* descrgetfunc tp_descr_get; */
	NULL,                       	    /* descrsetfunc tp_descr_set; */
	0,                          	    /* long tp_dictoffset; */
	NULL,                       	    /* initproc tp_init; */
	NULL,                       	    /* allocfunc tp_alloc; */
	NULL,                       	    /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       	    /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       	    /* inquiry tp_is_gc;  */
	NULL,                       	    /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       	    /* PyObject *tp_mro;  */
	NULL,                       	    /* PyObject *tp_cache; */
	NULL,                       	    /* PyObject *tp_subclasses; */
	NULL,                       	    /* PyObject *tp_weaklist; */
	NULL
};


/*****************************************************************************/
/* Function:              Curve_Init                                         */
/*****************************************************************************/
PyObject *Curve_Init( void )
{
	PyObject *submodule;

	if( PyType_Ready( &Curve_Type) < 0) /* set exception.  -1 is failure */
		return NULL;

	submodule =
		Py_InitModule3( "Blender.Curve", M_Curve_methods,
				M_Curve_doc );
	return ( submodule );
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

struct Curve *Curve_FromPyObject( PyObject * py_obj )
{
	BPy_Curve *blen_obj;

	blen_obj = ( BPy_Curve * ) py_obj;
	return ( blen_obj->curve );

}

/* #####DEPRECATED###### */

PyObject *Curve_setName( BPy_Curve * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args, (setter)Curve_newsetName );
}

static PyObject *Curve_setPathLen( BPy_Curve * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args,
			(setter)Curve_newsetPathLen );
}

static PyObject *Curve_setTotcol( BPy_Curve * self, PyObject * args )
{
	if( !PyArg_ParseTuple( args, "i", &( self->curve->totcol ) ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected int argument" );
	Py_RETURN_NONE;
}

PyObject *Curve_setMode( BPy_Curve * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args,
			(setter)Curve_newsetMode );
}

PyObject *Curve_setBevresol( BPy_Curve * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args,
			(setter)Curve_newsetBevresol);
}

PyObject *Curve_setResolu( BPy_Curve * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args,
			(setter)Curve_newsetResolu );
}

PyObject *Curve_setResolv( BPy_Curve * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args,
			(setter)Curve_newsetResolv );
}

PyObject *Curve_setWidth( BPy_Curve * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args,
			(setter)Curve_newsetWidth );
}

PyObject *Curve_setExt1( BPy_Curve * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args,
			(setter)Curve_newsetExt1 );
}

PyObject *Curve_setExt2( BPy_Curve * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args,
			(setter)Curve_newsetExt2 );
}

static PyObject *Curve_setLoc( BPy_Curve * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args,
			(setter)Curve_newsetLoc );
}

static PyObject *Curve_setRot( BPy_Curve * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args,
			(setter)Curve_newsetRot );
}

static PyObject *Curve_setSize( BPy_Curve * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args,
			(setter)Curve_newsetSize );
}

PyObject *Curve_setBevOb( BPy_Curve * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args,
			(setter)Curve_newsetBevOb );
}

PyObject *Curve_setTaperOb( BPy_Curve * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args,
			(setter)Curve_newsetTaperOb );
}

