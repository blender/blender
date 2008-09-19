/*
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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
 * ***** END GPL LICENSE BLOCK *****
 */

#include "Ipocurve.h" /*This must come first*/

#include "Object.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_depsgraph.h"
#include "BKE_ipo.h"
#include "BKE_utildefines.h"
#include "BIF_space.h"
#include "BSE_editipo.h"
#include "MEM_guardedalloc.h"
#include "DNA_ipo_types.h"
#include "DNA_key_types.h"
#include "BezTriple.h"
#include "gen_utils.h"

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
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python C_IpoCurve methods declarations:                                   */
/*****************************************************************************/
static PyObject *IpoCurve_getName( C_IpoCurve * self );
static PyObject *IpoCurve_Recalc( C_IpoCurve * self );
static PyObject *IpoCurve_append( C_IpoCurve * self, PyObject * value );
static PyObject *IpoCurve_addBezier( C_IpoCurve * self, PyObject * value );
static PyObject *IpoCurve_delBezier( C_IpoCurve * self, PyObject * args );
static PyObject *IpoCurve_setInterpolation( C_IpoCurve * self,
					    PyObject * value );
static PyObject *IpoCurve_getInterpolation( C_IpoCurve * self );
static PyObject *IpoCurve_newgetInterp( C_IpoCurve * self );
static int IpoCurve_newsetInterp( C_IpoCurve * self, PyObject * args );
static PyObject *IpoCurve_setExtrapolation( C_IpoCurve * self,
					    PyObject * value );
static PyObject *IpoCurve_getExtrapolation( C_IpoCurve * self );
static PyObject *IpoCurve_newgetExtend( C_IpoCurve * self );
static int IpoCurve_newsetExtend( C_IpoCurve * self, PyObject * args );
static PyObject *IpoCurve_getPoints( C_IpoCurve * self );
static PyObject *IpoCurve_evaluate( C_IpoCurve * self, PyObject * args );
static PyObject *IpoCurve_getDriver( C_IpoCurve * self );
static int IpoCurve_setDriver( C_IpoCurve * self, PyObject * args );
static PyObject *IpoCurve_getDriverObject( C_IpoCurve * self);
static int IpoCurve_setDriverObject( C_IpoCurve * self, PyObject * args );
static PyObject *IpoCurve_getDriverChannel( C_IpoCurve * self);
static int IpoCurve_setDriverChannel( C_IpoCurve * self, PyObject * args );
static PyObject *IpoCurve_getDriverExpression( C_IpoCurve * self);
static PyObject *IpoCurve_getFlag( C_IpoCurve * self, void *type);
static int IpoCurve_setFlag( C_IpoCurve * self, PyObject *value, void *type);

static int IpoCurve_setDriverExpression( C_IpoCurve * self, PyObject * args );
static PyObject *IpoCurve_getCurval( C_IpoCurve * self, PyObject * args );
static int IpoCurve_setCurval( C_IpoCurve * self, PyObject * key, 
		PyObject * value );

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
	 "() - deprecated method: use recalc method instead."},
	{"append", ( PyCFunction ) IpoCurve_append, METH_O,
	 "(coordlist) -  Adds a Bezier point to a curve"},
	{"addBezier", ( PyCFunction ) IpoCurve_addBezier, METH_O,
	 "() - deprecated method. use append() instead"},
	{"delBezier", ( PyCFunction ) IpoCurve_delBezier, METH_VARARGS,
	 "() - deprecated method. use \"del icu[index]\" instead"},
	{"setInterpolation", ( PyCFunction ) IpoCurve_setInterpolation,
	 METH_O, "(str) - Sets the interpolation type of the curve"},
	{"getInterpolation", ( PyCFunction ) IpoCurve_getInterpolation,
	 METH_NOARGS, "() - Gets the interpolation type of the curve"},
	{"setExtrapolation", ( PyCFunction ) IpoCurve_setExtrapolation,
	 METH_O, "(str) - Sets the extend mode of the curve"},
	{"getExtrapolation", ( PyCFunction ) IpoCurve_getExtrapolation,
	 METH_NOARGS, "() - Gets the extend mode of the curve"},
	{"getPoints", ( PyCFunction ) IpoCurve_getPoints, METH_NOARGS,
	 "() - Returns list of all bezTriples of the curve"},
	{"evaluate", ( PyCFunction ) IpoCurve_evaluate, METH_VARARGS,
	 "(float) - Evaluate curve at given time"},
	{NULL, NULL, 0, NULL}
};

/*
 * IpoCurve methods
 */

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
	 "The status of the driver 1-object, 2-py expression, 0-off",
	 NULL},
	{"driverObject",
	 (getter)IpoCurve_getDriverObject, (setter)IpoCurve_setDriverObject,
	 "The object used to drive the IpoCurve",
	 NULL},
	{"driverChannel",
	 (getter)IpoCurve_getDriverChannel, (setter)IpoCurve_setDriverChannel,
	 "The channel on the driver object used to drive the IpoCurve",
	 NULL},
	{"driverExpression",
	 (getter)IpoCurve_getDriverExpression, (setter)IpoCurve_setDriverExpression,
	 "The python expression on the driver used to drive the IpoCurve",
	 NULL},
	{"interpolation",
	 (getter)IpoCurve_newgetInterp, (setter)IpoCurve_newsetInterp,
	 "The interpolation mode of the curve",
	 NULL},
	{"extend",
	 (getter)IpoCurve_newgetExtend, (setter)IpoCurve_newsetExtend,
	 "The extend mode of the curve",
	 NULL},
	
	{"sel",
	 (getter)IpoCurve_getFlag, (setter)IpoCurve_setFlag,
	 "the selection state of the curve",
	 (void *)IPO_SELECT},
	 
	 {NULL,NULL,NULL,NULL,NULL}
};

/*****************************************************************************/
/* Python IpoCurve_Type Mapping Methods table:                               */
/*****************************************************************************/

static PyMappingMethods IpoCurve_as_mapping = {
	( inquiry ) 0,	/* mp_length */
	( binaryfunc ) IpoCurve_getCurval,	/* mp_subscript */
	( objobjargproc ) IpoCurve_setCurval,	/* mp_ass_subscript */
};

/*****************************************************************************/
/* Python IpoCurve_Type callback function prototypes:                        */
/*****************************************************************************/
static int IpoCurve_compare( C_IpoCurve * a, C_IpoCurve * b );
static PyObject *IpoCurve_repr( C_IpoCurve * self );

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
	NULL,							    /* tp_dealloc */
	0,									/* tp_print */
	( getattrfunc ) NULL,	            /* tp_getattr */
	( setattrfunc ) NULL,	            /* tp_setattr */
	( cmpfunc ) IpoCurve_compare,		/* tp_compare */
	( reprfunc ) IpoCurve_repr,			/* tp_repr */
	/* Method suites for standard classes */

	NULL,                               /* PyNumberMethods *tp_as_number; */
	NULL,                               /* PySequenceMethods *tp_as_sequence; */
	&IpoCurve_as_mapping,               /* PyMappingMethods *tp_as_mapping; */

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

	NULL,                       		/*  char *tp_doc;  */
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
/* local utility functions                                                   */
/*****************************************************************************/

/*
 * Keys are handled differently than other Ipos, so go through contortions
 * to find their names.
 */

static char *get_key_curvename( IpoCurve *ipocurve )
{
	Key *key_iter;
	char *empty = "";

	/* search for keys with an Ipo */

	for( key_iter = G.main->key.first; key_iter; key_iter=key_iter->id.next) {
		if( key_iter->ipo ) {
			IpoCurve *icu = key_iter->ipo->curve.first;
			/* search curves for a match */
			while( icu ) {
				if( icu == ipocurve ) {
					KeyBlock *block = key_iter->block.first;
					/* search for matching adrcode */
					while( block ) {
						if( block->adrcode == ipocurve->adrcode )
							return block->name;
						block = block->next;
					}
				}
				icu = icu->next;
			}
		}
	}

	/* shouldn't get here unless deleted in UI while BPy object alive */
	return empty;
}

/*
 * internal bpy func to get Ipo Curve Name, used by Ipo.c and
 * KX_BlenderSceneConverter.cpp.
 *
 * We are returning a pointer to string constants so there are
 * no issues with who owns pointers.
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
		/* return "Key";	*/
		/* ipo curves have no names... that was only meant for drawing the buttons... (ton) */
		return get_key_curvename( icu );
	case ID_SEQ:
		return getname_seq_ei( icu->adrcode );
	case ID_CO:
		return getname_co_ei( icu->adrcode );
	}
	return NULL;
}

/*
 * delete a bezTriple from a curve
 */

static void del_beztriple( IpoCurve *icu, int index )
{
	int npoints = icu->totvert - 1;
	BezTriple * tmp = icu->bezt;

	/*
	 * if delete empties list, then delete it, otherwise copy the remaining
	 * points to a new list
	 */

	if( !npoints ) {
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
}

/*****************************************************************************/
/* Python C_IpoCurve methods:                                                */
/*****************************************************************************/

static PyObject *IpoCurve_setInterpolation( C_IpoCurve * self,
					    PyObject * value )
{
	char *interpolationtype = PyString_AsString(value);
	short id;

	if( !interpolationtype )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected string argument" );

	if( !strcmp( interpolationtype, "Bezier" ) )
		id = IPO_BEZ;
	else if( !strcmp( interpolationtype, "Constant" ) )
		id = IPO_CONST;
	else if( !strcmp( interpolationtype, "Linear" ) )
		id = IPO_LIN;
	else
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"bad interpolation type" );

	self->ipocurve->ipo = id;
	Py_RETURN_NONE;
}

static PyObject *IpoCurve_getInterpolation( C_IpoCurve * self )
{
	char *str = 0;
	IpoCurve *icu = self->ipocurve;

	switch( icu->ipo ) {
	case IPO_BEZ:
		str = "Bezier";
		break;
	case IPO_CONST:
		str = "Constant";
		break;
	case IPO_LIN:
		str = "Linear";
		break;
	default:
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"unknown interpolation type" );
	}

	return PyString_FromString( str );
}

static PyObject * IpoCurve_setExtrapolation( C_IpoCurve * self,
		PyObject * value )
{
	char *extrapolationtype = PyString_AsString(value);
	short id;

	if( !extrapolationtype )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected string argument" );

	if( !strcmp( extrapolationtype, "Constant" ) )
		id = 0;
	else if( !strcmp( extrapolationtype, "Extrapolation" ) )
		id = 1;
	else if( !strcmp( extrapolationtype, "Cyclic" ) )
		id = 2;
	else if( !strcmp( extrapolationtype, "Cyclic_extrapolation" ) )
		id = 3;
	else
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"bad interpolation type" );

	self->ipocurve->extrap = id;
	Py_RETURN_NONE;
}

static PyObject *IpoCurve_getExtrapolation( C_IpoCurve * self )
{
	char *str;
	IpoCurve *icu = self->ipocurve;

	switch( icu->extrap ) {
	case 0:
		str = "Constant";
		break;
	case 1:
		str = "Extrapolation";
		break;
	case 2:
		str = "Cyclic";
		break;
	case 3:
		str = "Cyclic_extrapolation";
		break;
	default:
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"bad extrapolation type" );
	}

	return PyString_FromString( str );
}

/*
 * append a new BezTriple to curve
 */

static PyObject *IpoCurve_append( C_IpoCurve * self, PyObject * value )
{
	float x, y;
	IpoCurve *icu = self->ipocurve;

	/* if args is a already a beztriple, tack onto end of list */
	if( BPy_BezTriple_Check ( value ) ) {
		BPy_BezTriple *bobj = (BPy_BezTriple *)value;

		BezTriple *newb = MEM_callocN( (icu->totvert+1)*sizeof(BezTriple),
				"BPyBeztriple" );
		if( icu->bezt ) {
			memcpy( newb, icu->bezt, ( icu->totvert )*sizeof( BezTriple ) );
			MEM_freeN( icu->bezt );
		}
		icu->bezt = newb;
		memcpy( &icu->bezt[icu->totvert], bobj->beztriple,
				sizeof( BezTriple ) );
		icu->totvert++;
		calchandles_ipocurve( icu );
	
	/* otherwise try to get two floats and add to list */
	} else {
		PyObject *xobj, *yobj;
		xobj = PyNumber_Float( PyTuple_GetItem( value, 0 ) );
		yobj = PyNumber_Float( PyTuple_GetItem( value, 1 ) );

		if( !xobj || !yobj )
			return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected tuple of floats" );

		x = (float)PyFloat_AsDouble( xobj );
		Py_DECREF( xobj );
		y = (float)PyFloat_AsDouble( yobj );
		Py_DECREF( yobj );
		insert_vert_icu( icu, x, y, 0);
	}

	Py_RETURN_NONE;
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
	int index;

	if( !PyArg_ParseTuple( args, "i", &index ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected int argument" );

	/* if index is negative, count from end of list */
	if( index < 0 )
		index += self->ipocurve->totvert;
	/* check range of index */
	if( index < 0 || index > self->ipocurve->totvert - 1 )
		return EXPP_ReturnPyObjError( PyExc_IndexError,
				"index outside of list" );

	del_beztriple( self->ipocurve, index );

	Py_RETURN_NONE;
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
		return PyString_FromString( get_key_curvename( self->ipocurve ) );
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

static PyObject *IpoCurve_getPoints( C_IpoCurve * self )
{
	BezTriple *bezt;
	PyObject *po;
	int i;
	PyObject *list = PyList_New( self->ipocurve->totvert );

	if( !list )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
				"PyList_New() failed" );

	for( bezt = self->ipocurve->bezt, i = 0;
			i < self->ipocurve->totvert; i++, bezt++ ) {
		po = BezTriple_CreatePyObject( bezt );
		if( !po ) {
			Py_DECREF( list );
			return NULL; /* This is okay since the error is alredy set */
		}
		PyList_SET_ITEM( list, i, po );
	}
	return list;
}

/*****************************************************************************/
/* Function:    IpoCurve_compare                                     		 */
/* Description: This compares 2 python types, == or != only.			     */
/*****************************************************************************/
static int IpoCurve_compare( C_IpoCurve * a, C_IpoCurve * b )
{
	return ( a->ipocurve == b->ipocurve ) ? 0 : -1;
}

/*****************************************************************************/
/* Function:    IpoCurve_repr                                                */
/* Description: This is a callback function for the C_IpoCurve type. It      */
/*              builds a meaningful string to represent ipocurve objects.    */
/*****************************************************************************/
static PyObject *IpoCurve_repr( C_IpoCurve * self )
{
	return PyString_FromFormat( "[IpoCurve \"%s\"]",
			getIpoCurveName( self->ipocurve ) );
}

/* Three Python IpoCurve_Type helper functions needed by the Object module: */

/*****************************************************************************/
/* Function:    IpoCurve_CreatePyObject                                     */
/* Description: This function will create a new C_IpoCurve from an existing  */
/*              Blender ipo structure.                                       */
/*****************************************************************************/
PyObject *IpoCurve_CreatePyObject( IpoCurve * icu )
{
	C_IpoCurve *pyipo;

	pyipo = ( C_IpoCurve * ) PyObject_NEW( C_IpoCurve, &IpoCurve_Type );

	if( !pyipo )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create C_IpoCurve object" );

	pyipo->ipocurve = icu;

	return ( PyObject * ) pyipo;
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

/*
 * get the value of an Ipocurve at a particular time
 */

static PyObject *IpoCurve_getCurval( C_IpoCurve * self, PyObject * args )
{
	float time;
	PyObject *pyfloat = PyNumber_Float( args );

	if( !pyfloat )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected float argument" );
	time = ( float )PyFloat_AS_DOUBLE( pyfloat );
	Py_DECREF( pyfloat );

	return PyFloat_FromDouble( ( double ) eval_icu( self->ipocurve, time ) );
}

/*
 * set the value of an Ipocurve at a particular time
 */

static int IpoCurve_setCurval( C_IpoCurve * self, PyObject * key, 
		PyObject * value )
{
	float time, curval;
	PyObject *pyfloat;

	/* make sure time, curval are both floats */

	pyfloat = PyNumber_Float( key );
	if( !pyfloat )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected float key" );
	time = ( float )PyFloat_AS_DOUBLE( pyfloat );
	Py_DECREF( pyfloat );

	pyfloat = PyNumber_Float( value );
	if( !pyfloat )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected float argument" );
	curval = ( float )PyFloat_AS_DOUBLE( pyfloat );
	Py_DECREF( pyfloat );

	/* insert a key at the specified time */

	insert_vert_icu( self->ipocurve, time, curval, 0);
	allspace(REMAKEIPO, 0);
	return 0;
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

static PyObject *IpoCurve_getDriver( C_IpoCurve * self )
{
	if( !self->ipocurve->driver )
		return PyInt_FromLong( 0 );	
	else {
		if (self->ipocurve->driver->type == IPO_DRIVER_TYPE_NORMAL)
			return PyInt_FromLong( 1 );
		if (self->ipocurve->driver->type == IPO_DRIVER_TYPE_PYTHON)
			return PyInt_FromLong( 2 );
	}
	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"unknown driver type, internal error" );
	
}

/*
	sets the driver to
	0: disabled
	1: enabled (object)
	2: enabled (python expression)
*/
static int IpoCurve_setDriver( C_IpoCurve * self, PyObject * args )
{
	IpoCurve *ipo = self->ipocurve;
	int type;
	if( !PyInt_Check( args ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected int argument 0 or 1 " );
	
	type = PyInt_AS_LONG( args );
	
	if (type < 0 || type > 2)
		return EXPP_ReturnIntError( PyExc_ValueError,
				"expected int argument 0, 1 or 2" );
	
	if (type==0) { /* disable driver */
		if( ipo->driver ) {
			MEM_freeN( ipo->driver );
			ipo->driver = NULL;			
		}
	} else {
		if( !ipo->driver ) { /*add driver if its not there */
			ipo->driver = MEM_callocN( sizeof(IpoDriver), "ipo driver" );
			ipo->driver->blocktype = ID_OB;
			ipo->driver->adrcode = OB_LOC_X;
		}
		
		if (type==1 && ipo->driver->type != IPO_DRIVER_TYPE_NORMAL) {
			ipo->driver->type = IPO_DRIVER_TYPE_NORMAL;
			ipo->driver->ob = NULL;
			ipo->driver->flag &= ~IPO_DRIVER_FLAG_INVALID;
			
		} else if (type==2 && ipo->driver->type != IPO_DRIVER_TYPE_PYTHON) {
			ipo->driver->type = IPO_DRIVER_TYPE_PYTHON;
			/* we should probably set ipo->driver->ob, but theres no way to do it properly */
			ipo->driver->ob = NULL;
		}
	}
	
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

	if( !ipo->driver )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
					      "This IpoCurve does not have an active driver" );

	if(!BPy_Object_Check(arg) )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
					      "expected an object argument" );
	ipo->driver->ob = ((BPy_Object *)arg)->object;

	DAG_scene_sort(G.scene);	
	
	return 0;
}

static PyObject *IpoCurve_getDriverChannel( C_IpoCurve * self )
{
	if( !self->ipocurve->driver )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "This IpoCurve does not have an active driver" );

	return PyInt_FromLong( self->ipocurve->driver->adrcode );	
}

static int IpoCurve_setDriverChannel( C_IpoCurve * self, PyObject * args )
{
	IpoCurve *ipo = self->ipocurve;
	short param;

	if( !ipo->driver )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"This IpoCurve does not have an active driver" );

	if( !PyInt_Check( args ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected int argument" );

	param  = (short)PyInt_AS_LONG ( args );
	if( ( param >= OB_LOC_X && param <= OB_LOC_Z )
			|| ( param >= OB_ROT_X && param <= OB_ROT_Z )
			|| ( param >= OB_SIZE_X && param <= OB_SIZE_Z ) ) {
		ipo->driver->adrcode = (short)PyInt_AS_LONG ( args );
		return 0;
	}

	return EXPP_ReturnIntError( PyExc_ValueError, "invalid int argument" );
}

static PyObject *IpoCurve_getDriverExpression( C_IpoCurve * self )
{
	IpoCurve *ipo = self->ipocurve;
	
	if( ipo->driver && ipo->driver->type == IPO_DRIVER_TYPE_PYTHON )
		return PyString_FromString( ipo->driver->name );

	Py_RETURN_NONE;
}

static int IpoCurve_setDriverExpression( C_IpoCurve * self, PyObject * arg )
{
	IpoCurve *ipo = self->ipocurve;
	char *exp; /* python expression */
	
	if( !ipo->driver )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
			"This IpoCurve does not have an active driver" );

	if (ipo->driver->type != IPO_DRIVER_TYPE_PYTHON)
		return EXPP_ReturnIntError( PyExc_RuntimeError,
			"This IpoCurve is not a python expression set the driver attribute to 2" );
	
	if(!PyString_Check(arg) )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
					      "expected a string argument" );
	
	exp = PyString_AsString(arg);
	if (strlen(exp)>127)
		return EXPP_ReturnIntError( PyExc_ValueError,
					      "string is too long, use 127 characters or less" );

	strcpy(ipo->driver->name, exp);
	return 0;
}

static PyObject *M_IpoCurve_ExtendDict( void )
{
	PyObject *EM = PyConstant_New(  );

	if( EM ) {
		BPy_constant *d = ( BPy_constant * ) EM;

		PyConstant_Insert( d, "CONST", PyInt_FromLong( IPO_HORIZ ) );
		PyConstant_Insert( d, "EXTRAP", PyInt_FromLong( IPO_DIR ) );
		PyConstant_Insert( d, "CYCLIC", PyInt_FromLong( IPO_CYCL ) );
		PyConstant_Insert( d, "CYCLIC_EXTRAP", PyInt_FromLong( IPO_CYCLX ) );
	}
	return EM;
}

static PyObject *M_IpoCurve_InterpDict( void )
{
	PyObject *IM = PyConstant_New(  );

	if( IM ) {
		BPy_constant *d = ( BPy_constant * ) IM;

		PyConstant_Insert( d, "CONST", PyInt_FromLong( IPO_CONST ) );
		PyConstant_Insert( d, "LINEAR", PyInt_FromLong( IPO_LIN ) );
		PyConstant_Insert( d, "BEZIER", PyInt_FromLong( IPO_BEZ ) );
	}
	return IM;
}

/*****************************************************************************/
/* Function:              IpoCurve_Init                                      */
/*****************************************************************************/
PyObject *IpoCurve_Init( void )
{
	PyObject *submodule;
	PyObject *ExtendTypes = M_IpoCurve_ExtendDict( );
	PyObject *InterpTypes = M_IpoCurve_InterpDict( );

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

	if( ExtendTypes )
		PyModule_AddObject( submodule, "ExtendTypes", ExtendTypes );
	if( InterpTypes )
		PyModule_AddObject( submodule, "InterpTypes", InterpTypes );

	return submodule;
}

/*
 */

static PyObject *IpoCurve_newgetInterp( C_IpoCurve * self )
{
	return PyInt_FromLong( self->ipocurve->ipo );	
}

static int IpoCurve_newsetInterp( C_IpoCurve * self, PyObject * value )
{
	return EXPP_setIValueRange( value, &self->ipocurve->ipo,
			IPO_CONST, IPO_BEZ, 'h' );
}

static PyObject *IpoCurve_newgetExtend( C_IpoCurve * self )
{
	return PyInt_FromLong( self->ipocurve->extrap );	
}

static int IpoCurve_newsetExtend( C_IpoCurve * self, PyObject * value )
{
	return EXPP_setIValueRange( value, &self->ipocurve->extrap,
			IPO_HORIZ, IPO_CYCLX, 'h' );
}

static PyObject *IpoCurve_getFlag( C_IpoCurve * self, void *type )
{
	if (self->ipocurve->flag & GET_INT_FROM_POINTER(type))
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}

static int IpoCurve_setFlag( C_IpoCurve * self, PyObject *value, void *type )
{
	int param = PyObject_IsTrue( value );
	if( param == -1 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected True/False or 0/1" );
	
	if (param)
		self->ipocurve->flag |= GET_INT_FROM_POINTER(type);
	else
		self->ipocurve->flag &= ~GET_INT_FROM_POINTER(type);
	
	return 0;
}


/* #####DEPRECATED###### */

static PyObject *IpoCurve_addBezier( C_IpoCurve * self, PyObject * value )
{
	float x, y;
	int npoints;
	IpoCurve *icu;
	BezTriple *bzt, *tmp;
	static char name[10] = "mlml";
	if( !PyArg_ParseTuple( value, "ff", &x, &y ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "expected a tuple of 2 floats" ) );

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

	Py_RETURN_NONE;
}
