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
 * Contributor(s): Jacques Guignot, Nathan Letwory, Ken Hughes
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "Ipocurve.h" /*This must come first*/

#include "BKE_global.h"
#include "BKE_ipo.h"
#include "BSE_editipo.h"
#include "MEM_guardedalloc.h"
#include "DNA_ipo_types.h"
#include "BezTriple.h"
#include "gen_utils.h"

/*****************************************************************************/
/* Python API function prototypes for the IpoCurve module.                   */
/*****************************************************************************/
static PyObject *M_IpoCurve_New( PyObject * self, PyObject * args );
static PyObject *M_IpoCurve_Get( PyObject * self, PyObject * args );

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.IpoCurve.__doc__                                                  */
/*****************************************************************************/
char M_IpoCurve_doc[] = "";
char M_IpoCurve_New_doc[] = "";
char M_IpoCurve_Get_doc[] = "";

/*****************************************************************************/
/* Python method structure definition for Blender.IpoCurve module:           */
/*****************************************************************************/

struct PyMethodDef M_IpoCurve_methods[] = {
	{"New", ( PyCFunction ) M_IpoCurve_New, METH_VARARGS | METH_KEYWORDS,
	 M_IpoCurve_New_doc},
	{"Get", M_IpoCurve_Get, METH_VARARGS, M_IpoCurve_Get_doc},
	{"get", M_IpoCurve_Get, METH_VARARGS, M_IpoCurve_Get_doc},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python C_IpoCurve methods declarations:                                   */
/*****************************************************************************/
static PyObject *IpoCurve_getName( C_IpoCurve * self );
static PyObject *IpoCurve_Recalc( C_IpoCurve * self );
static PyObject *IpoCurve_addBezier( C_IpoCurve * self, PyObject * args );
static PyObject *IpoCurve_delBezier( C_IpoCurve * self, PyObject * args );
static PyObject *IpoCurve_setInterpolation( C_IpoCurve * self,
					    PyObject * args );
static PyObject *IpoCurve_getInterpolation( C_IpoCurve * self );
static PyObject *IpoCurve_setExtrapolation( C_IpoCurve * self,
					    PyObject * args );
static PyObject *IpoCurve_getExtrapolation( C_IpoCurve * self );
static PyObject *IpoCurve_getPoints( C_IpoCurve * self );
static int IpoCurve_setPoints( C_IpoCurve * self, PyObject * value );
static PyObject *IpoCurve_evaluate( C_IpoCurve * self, PyObject * args );

/*****************************************************************************/
/* Python C_IpoCurve methods table:                                          */
/*****************************************************************************/
static PyMethodDef C_IpoCurve_methods[] = {
	/* name, method, flags, doc */
	{"getName", ( PyCFunction ) IpoCurve_getName, METH_NOARGS,
	 "() - Return IpoCurve name"},
	{"Recalc", ( PyCFunction ) IpoCurve_Recalc, METH_NOARGS,
	 "() - deprecated method.  use recalc() instead"},
	{"recalc", ( PyCFunction ) IpoCurve_Recalc, METH_NOARGS,
	 "() - Recomputes the curve after changes"},
	{"update", ( PyCFunction ) IpoCurve_Recalc, METH_NOARGS,
	 "() - obsolete: use recalc method instead."},
	{"addBezier", ( PyCFunction ) IpoCurve_addBezier, METH_VARARGS,
	 "(coordlist) -  Adds a Bezier point to a curve"},
	{"delBezier", ( PyCFunction ) IpoCurve_delBezier, METH_VARARGS,
	 "(int) - delete Bezier point at specified index"},
	{"setInterpolation", ( PyCFunction ) IpoCurve_setInterpolation,
	 METH_VARARGS, "(str) - Sets the interpolation type of the curve"},
	{"getInterpolation", ( PyCFunction ) IpoCurve_getInterpolation,
	 METH_NOARGS, "() - Gets the interpolation type of the curve"},
	{"setExtrapolation", ( PyCFunction ) IpoCurve_setExtrapolation,
	 METH_VARARGS, "(str) - Sets the extend mode of the curve"},
	{"getExtrapolation", ( PyCFunction ) IpoCurve_getExtrapolation,
	 METH_NOARGS, "() - Gets the extend mode of the curve"},
	{"getPoints", ( PyCFunction ) IpoCurve_getPoints, METH_NOARGS,
	 "() - Returns list of all bezTriples of the curve"},
	{"evaluate", ( PyCFunction ) IpoCurve_evaluate, METH_VARARGS,
	 "(float) - Evaluate curve at given time"},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python IpoCurve_Type callback function prototypes:                        */
/*****************************************************************************/
static void IpoCurveDeAlloc( C_IpoCurve * self );
//static int IpoCurvePrint (C_IpoCurve *self, FILE *fp, int flags);
static int IpoCurveSetAttr( C_IpoCurve * self, char *name, PyObject * v );
static PyObject *IpoCurveGetAttr( C_IpoCurve * self, char *name );
static PyObject *IpoCurveRepr( C_IpoCurve * self );

/*****************************************************************************/
/* Python IpoCurve_Type structure definition:                                */
/*****************************************************************************/
PyTypeObject IpoCurve_Type = {
	PyObject_HEAD_INIT( NULL )                  /* required macro */ 
	0,	/* ob_size */
	"IpoCurve",		/* tp_name */
	sizeof( C_IpoCurve ),	/* tp_basicsize */
	0,			/* tp_itemsize */
	/* methods */
	( destructor ) IpoCurveDeAlloc,	/* tp_dealloc */
	0,			/* tp_print */
	( getattrfunc ) IpoCurveGetAttr,	/* tp_getattr */
	( setattrfunc ) IpoCurveSetAttr,	/* tp_setattr */
	0,			/* tp_compare */
	( reprfunc ) IpoCurveRepr,	/* tp_repr */
	0,			/* tp_as_number */
	0,			/* tp_as_sequence */
	0,			/* tp_as_mapping */
	0,			/* tp_as_hash */
	0, 0, 0, 0, 0, 0,
	0,			/* tp_doc */
	0, 0, 0, 0, 0, 0,
	C_IpoCurve_methods,	/* tp_methods */
	0,			/* tp_members */
};

/*****************************************************************************/
/* Function:       M_IpoCurve_New                                          */
/* Python equivalent:     Blender.IpoCurve.New                   */
/*****************************************************************************/
static PyObject *M_IpoCurve_New( PyObject * self, PyObject * args )
{
	return 0;
}

/*****************************************************************************/
/* Function:              Ipo_Init                                           */
/*****************************************************************************/
PyObject *IpoCurve_Init( void )
{
	PyObject *submodule;

	IpoCurve_Type.ob_type = &PyType_Type;

	submodule =
		Py_InitModule3( "Blender.IpoCurve", M_IpoCurve_methods,
				M_IpoCurve_doc );

	return ( submodule );
}

/*****************************************************************************/
/* Function:              M_IpoCurve_Get                                     */
/* Python equivalent:     Blender.IpoCurve.Get                               */
/* Description:           Receives a string and returns the ipo data obj     */
/*                        whose name matches the string.  If no argument is  */
/*                           passed in, a list of all ipo data names in the  */
/*                        current scene is returned.                         */
/*****************************************************************************/
static PyObject *M_IpoCurve_Get( PyObject * self, PyObject * args )
{
	Py_INCREF( Py_None );
	return Py_None;
}

/*****************************************************************************/
/* Python C_IpoCurve methods:                                                */
/*****************************************************************************/

static PyObject *IpoCurve_setInterpolation( C_IpoCurve * self,
					    PyObject * args )
{
	char *interpolationtype = 0;
	int id = -1;
	if( !PyArg_ParseTuple( args, "s", &interpolationtype ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "expected string argument" ) );
	if( !strcmp( interpolationtype, "Bezier" ) )
		id = IPO_BEZ;
	if( !strcmp( interpolationtype, "Constant" ) )
		id = IPO_CONST;
	if( !strcmp( interpolationtype, "Linear" ) )
		id = IPO_LIN;
	if( id == -1 )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "bad interpolation type" ) );

	self->ipocurve->ipo = (short)id;
	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *IpoCurve_getInterpolation( C_IpoCurve * self )
{
	char *str = 0;
	IpoCurve *icu = self->ipocurve;
	if( icu->ipo == IPO_BEZ )
		str = "Bezier";
	if( icu->ipo == IPO_CONST )
		str = "Constant";
	if( icu->ipo == IPO_LIN )
		str = "Linear";

	if( !str )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "unknown interpolation type" ) );
	return PyString_FromString( str );
}

static PyObject *IpoCurve_setExtrapolation( C_IpoCurve * self,
					    PyObject * args )
{

	char *extrapolationtype = 0;
	int id = -1;
	if( !PyArg_ParseTuple( args, "s", &extrapolationtype ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "expected string argument" ) );
	if( !strcmp( extrapolationtype, "Constant" ) )
		id = 0;
	if( !strcmp( extrapolationtype, "Extrapolation" ) )
		id = 1;
	if( !strcmp( extrapolationtype, "Cyclic" ) )
		id = 2;
	if( !strcmp( extrapolationtype, "Cyclic_extrapolation" ) )
		id = 3;

	if( id == -1 )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "bad interpolation type" ) );
	self->ipocurve->extrap = (short)id;
	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *IpoCurve_getExtrapolation( C_IpoCurve * self )
{
	char *str = 0;
	IpoCurve *icu = self->ipocurve;
	if( icu->extrap == 0 )
		str = "Constant";
	if( icu->extrap == 1 )
		str = "Extrapolation";
	if( icu->extrap == 2 )
		str = "Cyclic";
	if( icu->extrap == 3 )
		str = "Cyclic_extrapolation";

	return PyString_FromString( str );
}

static PyObject *IpoCurve_addBezier( C_IpoCurve * self, PyObject * args )
{
	float x, y;
	int npoints;
	IpoCurve *icu;
	BezTriple *bzt, *tmp;
	static char name[10] = "mlml";
	PyObject *popo = 0;
	if( !PyArg_ParseTuple( args, "O", &popo ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "expected tuple argument" ) );

	x = (float)PyFloat_AsDouble( PyTuple_GetItem( popo, 0 ) );
	y = (float)PyFloat_AsDouble( PyTuple_GetItem( popo, 1 ) );
	icu = self->ipocurve;
	npoints = icu->totvert;
	tmp = icu->bezt;
	icu->bezt = MEM_mallocN( sizeof( BezTriple ) * ( npoints + 1 ), name );
	if( tmp ) {
		memmove( icu->bezt, tmp, sizeof( BezTriple ) * npoints );
		MEM_freeN( tmp );
	}
	memmove( icu->bezt + npoints, icu->bezt, sizeof( BezTriple ) );
	icu->totvert++;
	bzt = icu->bezt + npoints;
	bzt->vec[0][0] = x - 1;
	bzt->vec[1][0] = x;
	bzt->vec[2][0] = x + 1;
	bzt->vec[0][1] = y - 1;
	bzt->vec[1][1] = y;
	bzt->vec[2][1] = y + 1;
	/* set handle type to Auto */
	bzt->h1 = HD_AUTO;
	bzt->h2 = HD_AUTO;

	Py_INCREF( Py_None );
	return Py_None;
}

/*
   Function:  IpoCurve_delBezier
   Bpy:       Blender.Ipocurve.delBezier(0)

   Delete an BezTriple from an IPO curve.
   example:
       ipo = Blender.Ipo.Get('ObIpo')
       cu = ipo.getCurve('LocX')
       cu.delBezier(0)
*/

static PyObject *IpoCurve_delBezier( C_IpoCurve * self, PyObject * args )
{
	int npoints;
	int index;
	IpoCurve *icu;
	BezTriple *tmp;

	if( !PyArg_ParseTuple( args, "i", &index ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "expected int argument" ) );

	icu = self->ipocurve;
	npoints = icu->totvert - 1;

	/* if index is negative, count from end of list */
	if( index < 0 )
		index += icu->totvert;
	/* check range of index */
	if( index < 0 || index > npoints )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_ValueError, "index outside of list" ) );

	tmp = icu->bezt;

	/*
	   if delete empties list, then delete it, otherwise copy the remaining
	   points to a new list
	 */

	if( npoints == 0 ) {
		icu->bezt = NULL;
	} else {
		icu->bezt =
			MEM_mallocN( sizeof( BezTriple ) * npoints, "bezt" );
		if( index > 0 )
			memmove( icu->bezt, tmp, index * sizeof( BezTriple ) );
		if( index < npoints )
			memmove( icu->bezt + index, tmp + index + 1,
				 ( npoints - index ) * sizeof( BezTriple ) );
	}

	/* free old list, adjust vertex count */
	MEM_freeN( tmp );
	icu->totvert--;

	/* call calchandles_* instead of testhandles_*  */
	/* I'm not sure this is a complete solution but since we do not */
	/* deal with curve handles right now, it seems ok */
	calchandles_ipocurve( icu );

	Py_INCREF( Py_None );
	return Py_None;
}


static PyObject *IpoCurve_Recalc( C_IpoCurve * self )
{
	IpoCurve *icu = self->ipocurve;

	/* testhandles_ipocurve (icu); */
	/* call calchandles_* instead of testhandles_*  */
	/* I'm not sure this is a complete solution but since we do not */
	/* deal with curve handles right now, it seems ok */
	calchandles_ipocurve( icu );

	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *IpoCurve_getName( C_IpoCurve * self )
{
	switch ( self->ipocurve->blocktype ) {
	case ID_OB:
		return PyString_FromString( getname_ob_ei( self->ipocurve->adrcode, 1 ) );	/* solve: what if EffX/Y/Z are wanted? */
	case ID_TE:
		return PyString_FromString( getname_tex_ei
					    ( self->ipocurve->adrcode ) );
	case ID_LA:
		return PyString_FromString( getname_la_ei
					    ( self->ipocurve->adrcode ) );
	case ID_MA:
		return PyString_FromString( getname_mat_ei
					    ( self->ipocurve->adrcode ) );
	case ID_CA:
		return PyString_FromString( getname_cam_ei
					    ( self->ipocurve->adrcode ) );
	case ID_WO:
		return PyString_FromString( getname_world_ei
					    ( self->ipocurve->adrcode ) );
	case ID_PO:
		return PyString_FromString( getname_ac_ei
					    ( self->ipocurve->adrcode ) );
	case ID_CU:
		return PyString_FromString( getname_cu_ei
					    ( self->ipocurve->adrcode ) );
	case ID_KE:
		return PyString_FromString("Key"); /* ipo curves have no names... that was only meant for drawing the buttons... (ton) */
	case ID_SEQ:
		return PyString_FromString( getname_seq_ei
					    ( self->ipocurve->adrcode ) );
	case ID_CO:
		return PyString_FromString( getname_co_ei
					    ( self->ipocurve->adrcode ) );
	default:
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "This function doesn't support this ipocurve type yet" );
	}
}

static void IpoCurveDeAlloc( C_IpoCurve * self )
{
	PyObject_DEL( self );
}

static PyObject *IpoCurve_getPoints( C_IpoCurve * self )
{
	struct BezTriple *bezt;
	PyObject *po;

	PyObject *list = PyList_New( 0 );
	int i;

	for( i = 0; i < self->ipocurve->totvert; i++ ) {
		bezt = self->ipocurve->bezt + i;
		po = BezTriple_CreatePyObject( bezt );
#if 0
		if( BezTriple_CheckPyObject( po ) )
			printf( "po is ok\n" );
		else
			printf( "po is hosed\n" );
#endif
		PyList_Append( list, po );
		/*
		   PyList_Append( list, BezTriple_CreatePyObject(bezt));
		 */
	}
	return list;
}


int IpoCurve_setPoints( C_IpoCurve * self, PyObject * value )
{
	struct BezTriple *bezt;
	PyObject *l = PyList_New( 0 );
	int i;
	for( i = 0; i < self->ipocurve->totvert; i++ ) {
		bezt = self->ipocurve->bezt + i;
		PyList_Append( l, BezTriple_CreatePyObject( bezt ) );
	}
	return 0;
}


/*****************************************************************************/
/* Function:    IpoCurveGetAttr                                         */
/* Description: This is a callback function for the C_IpoCurve type. It is   */
/*              the function that accesses C_IpoCurve "member variables" and */
/*              methods.                                                     */
/*****************************************************************************/
static PyObject *IpoCurveGetAttr( C_IpoCurve * self, char *name )
{
	if( strcmp( name, "bezierPoints" ) == 0 )
		return IpoCurve_getPoints( self );
	if( strcmp( name, "name" ) == 0 )
		return IpoCurve_getName( self );
	return Py_FindMethod( C_IpoCurve_methods, ( PyObject * ) self, name );
}

/*****************************************************************************/
/* Function:    IpoCurveSetAttr                                    */
/* Description: This is a callback function for the C_IpoCurve type. It  */
/*               sets IpoCurve Data attributes (member variables).*/
/*****************************************************************************/
static int IpoCurveSetAttr( C_IpoCurve * self, char *name, PyObject * value )
{
	if( strcmp( name, "bezierPoints" ) == 0 )
		return IpoCurve_setPoints( self, value );
	return 0;		/* normal exit */
}

/*****************************************************************************/
/* Function:    IpoCurveRepr                                             */
/* Description: This is a callback function for the C_IpoCurve type. It      */
/*              builds a meaninful string to represent ipo objects.          */
/*****************************************************************************/
static PyObject *IpoCurveRepr( C_IpoCurve * self )
{
	char s[100];
	sprintf( s, "[IpoCurve \"%s\"]\n", getIpoCurveName( self->ipocurve ) );
	return PyString_FromString( s );
}

/* Three Python IpoCurve_Type helper functions needed by the Object module: */

/*****************************************************************************/
/* Function:    IpoCurve_CreatePyObject                                     */
/* Description: This function will create a new C_IpoCurve from an existing  */
/*              Blender ipo structure.                                       */
/*****************************************************************************/
PyObject *IpoCurve_CreatePyObject( IpoCurve * ipo )
{
	C_IpoCurve *pyipo;

	pyipo = ( C_IpoCurve * ) PyObject_NEW( C_IpoCurve, &IpoCurve_Type );

	if( !pyipo )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create C_IpoCurve object" );

	pyipo->ipocurve = ipo;

	return ( PyObject * ) pyipo;
}

/*****************************************************************************/
/* Function:    IpoCurve_CheckPyObject                                      */
/* Description: This function returns true when the given PyObject is of the */
/*              type IpoCurve. Otherwise it will return false.               */
/*****************************************************************************/
int IpoCurve_CheckPyObject( PyObject * pyobj )
{
	return ( pyobj->ob_type == &IpoCurve_Type );
}

/*****************************************************************************/
/* Function:    IpoCurve_FromPyObject                                       */
/* Description: This function returns the Blender ipo from the given         */
/*              PyObject.                                                    */
/*****************************************************************************/
IpoCurve *IpoCurve_FromPyObject( PyObject * pyobj )
{
	return ( ( C_IpoCurve * ) pyobj )->ipocurve;
}

/***************************************************************************/
/* Function:      IpoCurve_evaluate( time )                                */
/* Description:   Evaluates IPO curve at the given time.                   */
/***************************************************************************/

static PyObject *IpoCurve_evaluate( C_IpoCurve * self, PyObject * args )
{

	float time = 0;
	double eval = 0;

	/* expecting float */
	if( !PyArg_ParseTuple( args, "f", &time ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "expected float argument" ) );

	eval = ( double ) eval_icu( self->ipocurve, time );

	return PyFloat_FromDouble( eval );

}

/*
  internal bpy func to get Ipo Curve Name.
  We are returning a pointer to string constants so there are
  no issues with who owns pointers.
*/

char *getIpoCurveName( IpoCurve * icu )
{
	switch ( icu->blocktype ) {
	case ID_MA:
		return getname_mat_ei( icu->adrcode );
	case ID_WO:
		return getname_world_ei( icu->adrcode );
	case ID_CA:
		return getname_cam_ei( icu->adrcode );
	case ID_OB:
		return getname_ob_ei( icu->adrcode, 1 );
		/* solve: what if EffX/Y/Z are wanted? */
	case ID_TE:
		return getname_tex_ei( icu->adrcode );
	case ID_LA:
		return getname_la_ei( icu->adrcode );
	case ID_PO:
		return getname_ac_ei( icu->adrcode );
	case ID_CU:
		return getname_cu_ei( icu->adrcode );
	case ID_KE:
		return "Key";	/* ipo curves have no names... that was only meant for drawing the buttons... (ton) */
	case ID_SEQ:
		return getname_seq_ei( icu->adrcode );
	case ID_CO:
		return getname_co_ei( icu->adrcode );
	}
	return NULL;
}
