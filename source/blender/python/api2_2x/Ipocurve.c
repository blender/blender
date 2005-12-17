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
 * Contributor(s): Jacques Guignot, Nathan Letwory, Ken Hughes, Johnny Matthews
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "Ipocurve.h" /*This must come first*/

#include "Object.h"
#include "BKE_global.h"
#include "BKE_depsgraph.h"
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
static PyObject *IpoCurve_evaluate( C_IpoCurve * self, PyObject * args );

static PyObject *IpoCurve_getDriver( C_IpoCurve * self );
static int IpoCurve_setDriver( C_IpoCurve * self, PyObject * args );

static PyObject *IpoCurve_getDriverObject( C_IpoCurve * self);
static int       IpoCurve_setDriverObject( C_IpoCurve * self, PyObject * args );

static PyObject *IpoCurve_getDriverChannel( C_IpoCurve * self);
static int       IpoCurve_setDriverChannel( C_IpoCurve * self, PyObject * args );
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


static PyGetSetDef C_IpoCurve_getseters[] = {
   {"name",
    (getter)IpoCurve_getName, (setter)NULL,
    "the IpoCurve name",
    NULL},
   {"bezierPoints",
    (getter)IpoCurve_getPoints, (setter)NULL,
    "list of all bezTriples of the curve",
    NULL},

	{"driver",
	 (getter)IpoCurve_getDriver, (setter)IpoCurve_setDriver,
	 "(int) The Status of the driver 1-on, 0-off",
	 NULL},
	{"driverObject",
	 (getter)IpoCurve_getDriverObject, (setter)IpoCurve_setDriverObject,
	 "(object) The Object Used to Drive the IpoCurve",
	 NULL},
	{"driverChannel",
	 (getter)IpoCurve_getDriverChannel, (setter)IpoCurve_setDriverChannel,
	 "(int) The Channel on the Driver Object Used to Drive the IpoCurve",
	 NULL},
	 {NULL,NULL,NULL,NULL,NULL}
};
/*****************************************************************************/
/* Python IpoCurve_Type callback function prototypes:                        */
/*****************************************************************************/
static void IpoCurveDeAlloc( C_IpoCurve * self );
//static int IpoCurvePrint (C_IpoCurve *self, FILE *fp, int flags);
static PyObject *IpoCurveRepr( C_IpoCurve * self );

/*****************************************************************************/
/* Python IpoCurve_Type structure definition:                                */
/*****************************************************************************/
PyTypeObject IpoCurve_Type = {
	PyObject_HEAD_INIT( NULL )          /* required macro */ 
	0,									/* ob_size */
	"IpoCurve",							/* tp_name */
	sizeof( C_IpoCurve ),				/* tp_basicsize */
	0,									/* tp_itemsize */
	/* methods */
	( destructor ) IpoCurveDeAlloc,		/* tp_dealloc */
	0,									/* tp_print */
	( getattrfunc ) NULL,	/* tp_getattr */
	( setattrfunc ) NULL,	/* tp_setattr */
	0,									/* tp_compare */
	( reprfunc ) IpoCurveRepr,			/* tp_repr */
	/* Method suites for standard classes */

	NULL,                       		/* PyNumberMethods *tp_as_number; */
	NULL,                       		/* PySequenceMethods *tp_as_sequence; */
	NULL,                       		/* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       		/* hashfunc tp_hash; */
	NULL,                       		/* ternaryfunc tp_call; */
	NULL,                       		/* reprfunc tp_str; */
	NULL,                       		/* getattrofunc tp_getattro; */
	NULL,                      			/* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       		/* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         		/* long tp_flags; */

	NULL,                       		/*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       		/* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       		/* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       		/* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          		/* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                      			/* getiterfunc tp_iter; */
	NULL,                       		/* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	C_IpoCurve_methods,           		/* struct PyMethodDef *tp_methods; */
	NULL,                       		/* struct PyMemberDef *tp_members; */
	C_IpoCurve_getseters,         		/* struct PyGetSetDef *tp_getset; */
	NULL,                       		/* struct _typeobject *tp_base; */
	NULL,                       		/* PyObject *tp_dict; */
	NULL,                       		/* descrgetfunc tp_descr_get; */
	NULL,                       		/* descrsetfunc tp_descr_set; */
	0,                          		/* long tp_dictoffset; */
	NULL,                       		/* initproc tp_init; */
	NULL,                       		/* allocfunc tp_alloc; */
	NULL,                       		/* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       		/* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       		/* inquiry tp_is_gc;  */
	NULL,                       		/* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       		/* PyObject *tp_mro;  */
	NULL,                       		/* PyObject *tp_cache; */
	NULL,                       		/* PyObject *tp_subclasses; */
	NULL,                       		/* PyObject *tp_weaklist; */
	NULL
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

	if( PyType_Ready( &IpoCurve_Type ) < 0)
		return NULL;

	submodule =
		Py_InitModule3( "Blender.IpoCurve", M_IpoCurve_methods,
				M_IpoCurve_doc );

	PyModule_AddIntConstant( submodule, "LOC_X", OB_LOC_X );
	PyModule_AddIntConstant( submodule, "LOC_Y", OB_LOC_Y );
	PyModule_AddIntConstant( submodule, "LOC_Z", OB_LOC_Z );	
	PyModule_AddIntConstant( submodule, "ROT_X", OB_ROT_X );
	PyModule_AddIntConstant( submodule, "ROT_Y", OB_ROT_Y );
	PyModule_AddIntConstant( submodule, "ROT_Z", OB_ROT_Z );	
	PyModule_AddIntConstant( submodule, "SIZE_X", OB_SIZE_X );
	PyModule_AddIntConstant( submodule, "SIZE_Y", OB_SIZE_Y );
	PyModule_AddIntConstant( submodule, "SIZE_Z", OB_SIZE_Z );	

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
	bzt->vec[0][2] = bzt->vec[1][2] = bzt->vec[2][2] = 0.0;
	/* set handle type to Auto */
	bzt->h1 = bzt->h2 = HD_AUTO;
	bzt->f1 = bzt->f2 = bzt->f3= 0;
	bzt->hide = IPO_BEZ;

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
	sort_time_ipocurve( icu );

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

/*****************************************************************************/
/* Function:    IpoCurveRepr                                             */
/* Description: This is a callback function for the C_IpoCurve type. It      */
/*              builds a meaninful string to represent ipo objects.          */
/*****************************************************************************/
static PyObject *IpoCurveRepr( C_IpoCurve * self )
{
	char s[100];
	sprintf( s, "[IpoCurve \"%s\"]", getIpoCurveName( self->ipocurve ) );
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


static PyObject *IpoCurve_getDriver( C_IpoCurve * self )
{
	if( self->ipocurve->driver == NULL ) {
		return PyInt_FromLong( 0 );	
	} else {
		return PyInt_FromLong( 1 );	
	}
}

static int IpoCurve_setDriver( C_IpoCurve * self, PyObject * args )
{
	IpoCurve *ipo = self->ipocurve;
	short mode;

	if( !PyInt_CheckExact( args ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected int argument 0 or 1" );

	mode = (short)PyInt_AS_LONG ( args );

	if(mode == 1){
		if(ipo->driver == NULL){
			ipo->driver				= MEM_callocN(sizeof(IpoDriver), "ipo driver");
			ipo->driver->blocktype	= ID_OB;
			ipo->driver->adrcode	= OB_LOC_X;
		}
	} else if(mode == 0){
		if(ipo->driver != NULL){
			MEM_freeN(ipo->driver);
			ipo->driver= NULL;			
		}
	} else
		return EXPP_ReturnIntError( PyExc_ValueError,
				"expected int argument: 0 or 1" );

	return 0;
}

static PyObject *IpoCurve_getDriverObject( C_IpoCurve * self )
{
	IpoCurve *ipo = self->ipocurve;
	
	if( ipo->driver )
		return Object_CreatePyObject( ipo->driver->ob );

	Py_RETURN_NONE;
}

static int IpoCurve_setDriverObject( C_IpoCurve * self, PyObject * arg )
{
	IpoCurve *ipo = self->ipocurve;

	if(ipo->driver == NULL)
		return EXPP_ReturnIntError( PyExc_RuntimeError,
					      "This IpoCurve does not have an active driver" );

	if(!BPy_Object_Check(arg) )
		return EXPP_ReturnIntError( PyExc_TypeError,
					      "expected an object argument" );

	ipo->driver->ob = ((BPy_Object *)arg)->object;
	DAG_scene_sort(G.scene);	
	
	return 0;
}

static PyObject *IpoCurve_getDriverChannel( C_IpoCurve * self )
{
	if( self->ipocurve->driver == NULL)
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "This IpoCurve does not have an active driver" );

	return PyInt_FromLong( self->ipocurve->driver->adrcode );	
}

static int IpoCurve_setDriverChannel( C_IpoCurve * self, PyObject * args )
{
	IpoCurve *ipo = self->ipocurve;

	if(ipo->driver == NULL)
		return EXPP_ReturnIntError( PyExc_RuntimeError,
					      "This IpoCurve does not have an active driver" );

	if( !PyInt_CheckExact( args ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected int argument 0 or 1" );

	ipo->driver->adrcode = (short)PyInt_AS_LONG( args );

	return 0;
}
