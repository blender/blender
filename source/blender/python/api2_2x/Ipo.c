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
 * Contributor(s): Jacques Guignot RIP 2005, Nathan Letwory, 
 *  Stephen Swaney, Ken Hughes
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "Ipo.h" /*This must come first*/

#include "BKE_main.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_object.h"
#include "BKE_ipo.h"
#include "BLI_blenlib.h"
#include "BIF_space.h"
#include "BSE_editipo.h"
#include "MEM_guardedalloc.h"
#include "DNA_key_types.h"
#include "mydevice.h"
#include "Ipocurve.h"
#include "gen_utils.h"
#include "gen_library.h"

extern int ob_ar[];
extern int la_ar[];
extern int ma_ar[];
extern int ac_ar[];
extern int cam_ar[];
extern int co_ar[];
extern int cu_ar[];
extern int seq_ar[];
extern int te_ar[];
extern int wo_ar[];

PyObject *submodule;

/*****************************************************************************/
/* Python API function prototypes for the Ipo module.                        */
/*****************************************************************************/
static PyObject *M_Ipo_New( PyObject * self, PyObject * args );
static PyObject *M_Ipo_Get( PyObject * self, PyObject * args );
static PyObject *M_Ipo_Recalc( PyObject * self, PyObject * args );

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.Ipo.__doc__                                                       */
/*****************************************************************************/
char M_Ipo_doc[] = "";
char M_Ipo_New_doc[] = "";
char M_Ipo_Get_doc[] = "";

/*****************************************************************************/
/* Python method structure definition for Blender.Ipo module:             */
/*****************************************************************************/

struct PyMethodDef M_Ipo_methods[] = {
	{"New", ( PyCFunction ) M_Ipo_New, METH_VARARGS | METH_KEYWORDS,
	 M_Ipo_New_doc},
	{"Get", M_Ipo_Get, METH_VARARGS, M_Ipo_Get_doc},
	{"get", M_Ipo_Get, METH_VARARGS, M_Ipo_Get_doc},
	{"Recalc", M_Ipo_Recalc, METH_O, M_Ipo_Get_doc},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python BPy_Ipo methods declarations:                                     */
/*****************************************************************************/
static PyObject *Ipo_getBlocktype( BPy_Ipo * self );
static PyObject *Ipo_oldsetBlocktype( BPy_Ipo * self, PyObject * args );
static int Ipo_setBlocktype( BPy_Ipo * self, PyObject * args );
static PyObject *Ipo_getRctf( BPy_Ipo * self );
static PyObject *Ipo_oldsetRctf( BPy_Ipo * self, PyObject * args );
static int Ipo_setRctf( BPy_Ipo * self, PyObject * args );

static PyObject *Ipo_getCurve( BPy_Ipo * self, PyObject * args );
static PyObject *Ipo_getCurves( BPy_Ipo * self );
static PyObject *Ipo_getCurveNames( BPy_Ipo * self );
static PyObject *Ipo_addCurve( BPy_Ipo * self, PyObject * value );
static PyObject *Ipo_delCurve( BPy_Ipo * self, PyObject * args );
static PyObject *Ipo_getNcurves( BPy_Ipo * self );
static PyObject *Ipo_getNBezPoints( BPy_Ipo * self, PyObject * args );
static PyObject *Ipo_DeleteBezPoints( BPy_Ipo * self, PyObject * args );
static PyObject *Ipo_getCurveBP( BPy_Ipo * self, PyObject * args );
static PyObject *Ipo_getCurvecurval( BPy_Ipo * self, PyObject * args );
static PyObject *Ipo_EvaluateCurveOn( BPy_Ipo * self, PyObject * args );

static PyObject *Ipo_setCurveBeztriple( BPy_Ipo * self, PyObject * args );
static PyObject *Ipo_getCurveBeztriple( BPy_Ipo * self, PyObject * args );

static PyObject *Ipo_getChannel( BPy_Ipo * self );
static PyObject *Ipo_copy( BPy_Ipo * self );
static int Ipo_setChannel( BPy_Ipo * self, PyObject * args );

static int Ipo_length( BPy_Ipo * inst );
static PyObject *Ipo_getIpoCurveByName( BPy_Ipo * self, PyObject * key );
static int Ipo_setIpoCurveByName( BPy_Ipo * self, PyObject * key, 
		PyObject * value );
static int Ipo_contains( BPy_Ipo * self, PyObject * key );

/*****************************************************************************/
/* Python BPy_Ipo methods table:                                            */
/*****************************************************************************/
static PyMethodDef BPy_Ipo_methods[] = {
	/* name, method, flags, doc */
	{"getName", ( PyCFunction ) GenericLib_getName, METH_NOARGS,
	 "() - Return Ipo Data name"},
	{"setName", ( PyCFunction ) GenericLib_setName_with_method, METH_VARARGS,
	 "(str) - Change Ipo Data name"},
	{"getBlocktype", ( PyCFunction ) Ipo_getBlocktype, METH_NOARGS,
	 "() - Return Ipo blocktype"},
	{"setBlocktype", ( PyCFunction ) Ipo_oldsetBlocktype, METH_VARARGS,
	 "(str) - Change Ipo blocktype"},
	{"getRctf", ( PyCFunction ) Ipo_getRctf, METH_NOARGS,
	 "() - Return Ipo rctf"},
	{"setRctf", ( PyCFunction ) Ipo_oldsetRctf, METH_VARARGS,
	 "(flt,flt,flt,flt) - Change Ipo rctf"},
	{"addCurve", ( PyCFunction ) Ipo_addCurve, METH_O,
	 "() - Add a curve to Ipo"},
	{"delCurve", ( PyCFunction ) Ipo_delCurve, METH_VARARGS,
	 "(str) - Delete curve from Ipo"},
	{"getNcurves", ( PyCFunction ) Ipo_getNcurves, METH_NOARGS,
	 "() - Return number of Ipo curves"},
	{"getCurves", ( PyCFunction ) Ipo_getCurves, METH_NOARGS,
	 "() - Return list of all defined Ipo curves"},
	{"getCurve", ( PyCFunction ) Ipo_getCurve, METH_VARARGS,
	 "(str|int) - Returns specified Ipo curve"},
	{"getNBezPoints", ( PyCFunction ) Ipo_getNBezPoints, METH_VARARGS,
	 "(int) - Return number of Bez points on an Ipo curve"},
	{"delBezPoint", ( PyCFunction ) Ipo_DeleteBezPoints, METH_VARARGS,
	 "(int) - deprecated: use ipocurve.delBezier()"},
	{"getCurveBP", ( PyCFunction ) Ipo_getCurveBP, METH_VARARGS,
	 "() - unsupported"},
	{"EvaluateCurveOn", ( PyCFunction ) Ipo_EvaluateCurveOn, METH_VARARGS,
	 "(int,flt) - deprecated: see ipocurve.evaluate()"},
	{"getCurveCurval", ( PyCFunction ) Ipo_getCurvecurval, METH_VARARGS,
	 "(int) - deprecated: see ipocurve.evaluate()"},
	{"getCurveBeztriple", ( PyCFunction ) Ipo_getCurveBeztriple, METH_VARARGS,
	 "(int,int) - deprecated: see ipocurve.bezierPoints[]"},
	{"setCurveBeztriple", ( PyCFunction ) Ipo_setCurveBeztriple, METH_VARARGS,
	 "(int,int,list) - set a BezTriple"},

	{"__copy__", ( PyCFunction ) Ipo_copy, METH_NOARGS,
	 "() - copy the ipo"},
	{"copy", ( PyCFunction ) Ipo_copy, METH_NOARGS,
	 "() - copy the ipo"},
	 
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python BPy_Ipo attributes get/set structure:                              */
/*****************************************************************************/
static PyGetSetDef BPy_Ipo_getseters[] = {
	GENERIC_LIB_GETSETATTR,
	{"curves",
	 (getter)Ipo_getCurves, (setter)NULL,
	 "Ipo curves",
	 NULL},
	{"curveConsts",
	 (getter)Ipo_getCurveNames, (setter)NULL,
	 "Ipo curve constants (values depend on Ipo type)",
	 NULL},
	{"channel",
	 (getter)Ipo_getChannel, (setter)Ipo_setChannel,
	 "Ipo texture channel (world, lamp, material Ipos only)",
	 NULL},

	{"blocktype",
	 (getter)Ipo_getBlocktype, (setter)NULL,
	 "Ipo block type",
	 NULL},
	{"rctf",
	 (getter)Ipo_getRctf, (setter)Ipo_setRctf,
	 "Ipo type",
	 NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/*****************************************************************************/
/* Python Ipo_Type Mapping Methods table:                                    */
/*****************************************************************************/
static PyMappingMethods Ipo_as_mapping = {
	( inquiry ) Ipo_length,	/* mp_length        */
	( binaryfunc ) Ipo_getIpoCurveByName,	/* mp_subscript     */
	( objobjargproc ) Ipo_setIpoCurveByName,	/* mp_ass_subscript */
};

static PySequenceMethods Ipo_as_sequence = {
	( inquiry ) 0,	/* sq_length */
	( binaryfunc ) 0,	/* sq_concat */
	( intargfunc ) 0,	/* sq_repeat */
	( intargfunc ) 0,	/* sq_item */
	( intintargfunc ) 0,	/* sq_slice */
	( intobjargproc ) 0,	/* sq_ass_item */
	( intintobjargproc ) 0,	/* sq_ass_slice */
	( objobjproc ) Ipo_contains,	/* sq_contains */
	( binaryfunc ) 0,		/* sq_inplace_concat */
	( intargfunc ) 0,		/* sq_inplace_repeat */
};

/*****************************************************************************/
/* Python Ipo_Type callback function prototypes:                             */
/*****************************************************************************/
/*static int IpoPrint (BPy_Ipo *self, FILE *fp, int flags);*/
static int Ipo_compare( BPy_Ipo * a, BPy_Ipo * b );
static PyObject *Ipo_repr( BPy_Ipo * self );
static PyObject *Ipo_getIter( BPy_Ipo * self );
static PyObject *Ipo_nextIter( BPy_Ipo * self );

/* #define CURVEATTRS */ /* uncomment to enable curves as Ipo attributes */ 

#ifdef CURVEATTRS
static PyGetSetDef BPy_Ipocurve_getseter = {
	"noname",
	 (getter)NULL, (setter)NULL,
	 "Ipo curve name",
	 NULL
};

void generate_curveattrs( PyObject* dict, int blocktype )
{
	typedef char * (*namefunc)(int, ... );
	namefunc lookup_name;
	int size;
	int *vals = NULL;
	char name[32];
	PyObject*desc;

	switch ( blocktype ) {
	case ID_OB:
		lookup_name = (namefunc)getname_ob_ei;
		vals = ob_ar;
		size = OB_TOTIPO;
		break;
	case ID_MA:
		lookup_name = (namefunc)getname_mat_ei;
		vals = ma_ar;
		size = MA_TOTIPO;
		break;
	case ID_CA:
		lookup_name = (namefunc)getname_cam_ei;
		vals = cam_ar;
		size = CAM_TOTIPO;
		break;
	case ID_LA:
		lookup_name = (namefunc)getname_la_ei;
		vals = la_ar;
		size = LA_TOTIPO;
		break;
	case ID_TE:
		lookup_name = (namefunc)getname_tex_ei;
		vals = te_ar;
		size = TE_TOTIPO;
		break;
	case ID_WO:
		lookup_name = (namefunc)getname_world_ei;
		vals = wo_ar;
		size = WO_TOTIPO;
		break;
	case ID_PO:
		lookup_name = (namefunc)getname_ac_ei;
		vals = ac_ar;
		size = AC_TOTIPO;
		break;
	case ID_CO:
		lookup_name = (namefunc)getname_co_ei;
		vals = co_ar;
		size = CO_TOTIPO;
		break;
	case ID_CU:
		lookup_name = (namefunc)getname_cu_ei;
		vals = cu_ar;
		size = CU_TOTIPO;
		break;
	case ID_SEQ:
		lookup_name = (namefunc)getname_seq_ei;
		vals = seq_ar;
		size = SEQ_TOTIPO;
		break;
	}

	desc = PyDescr_NewGetSet( &Ipo_Type, &BPy_Ipocurve_getseter );
	while( size-- ) {
		strcpy( name, lookup_name( *vals ) ); 
		*name = tolower( *name );
		PyDict_SetItemString( dict, name, desc );
		++vals;
	}
	Py_DECREF( desc );
}

static short lookup_curve_name( char *, int , int );

static PyObject *getattro( PyObject *self, PyObject *value )
{
	char *name = PyString_AS_STRING( value );
	Ipo *ipo = ((BPy_Ipo *)self)->ipo;
	int adrcode;
	IpoCurve *icu;

	if( !strcmp(name, "__class__") )
		return PyObject_GenericGetAttr( self, value );

	if( !strcmp(name, "__dict__") ) /* no effect */
	{
		PyObject *dict;
		dict = PyDict_Copy( self->ob_type->tp_dict );
		generate_curveattrs( dict, ipo->blocktype );
		return dict;
	}

	adrcode = lookup_curve_name( name, ipo->blocktype, 
			((BPy_Ipo *)self)->mtex );

	if( adrcode != -1 ) {
		for( icu = ipo->curve.first; icu; icu = icu->next )
			if( icu->adrcode == adrcode )
				return IpoCurve_CreatePyObject( icu );
		Py_RETURN_NONE;
	}

	return PyObject_GenericGetAttr( self, value );
}
#endif

/*****************************************************************************/
/* Python Ipo_Type structure definition:                                     */
/*****************************************************************************/
PyTypeObject Ipo_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender Ipo",              /* char *tp_name; */
	sizeof( BPy_Ipo ),          /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	NULL,						/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) Ipo_compare,   /* cmpfunc tp_compare; */
	( reprfunc ) Ipo_repr,     /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	&Ipo_as_sequence,           /* PySequenceMethods *tp_as_sequence; */
	&Ipo_as_mapping, 	        /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	( hashfunc ) GenericLib_hash,	/* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
#ifdef CURVEATTRS
	(getattrofunc)getattro,
#else
	NULL,                       /* getattrofunc tp_getattro; */
#endif
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
	( getiterfunc) Ipo_getIter, /* getiterfunc tp_iter; */
	( iternextfunc ) Ipo_nextIter, /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_Ipo_methods,            /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_Ipo_getseters,          /* struct PyGetSetDef *tp_getset; */
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
/* internal utility routines                                                 */
/*****************************************************************************/

/*
 * Search through list of known Ipocurves for a particular name.
 *
 * str: name of curve we are searching for
 * blocktype: type of Ipo
 * channel: texture channel number, for World/Lamp/Material curves
 *
 * returns the adrcode for the named curve if it exists, -1 otherwise
 */

/* this is needed since getname_ob_ei() is different from the rest */

typedef char * (*namefunc)(int, ... );

static short lookup_curve_name( char *str, int blocktype, int channel )
{
	namefunc lookup_name;
	int *adrcodes = NULL;
	int size = 0;

	/* make sure channel type is ignored when it should be */
	if( blocktype != ID_WO && blocktype != ID_LA && blocktype != ID_MA )
		channel = -1;

	switch ( blocktype ) {
	case ID_OB:
		lookup_name = (namefunc)getname_ob_ei;
		adrcodes = ob_ar;
		size = OB_TOTIPO;
		break;
	case ID_MA:
		lookup_name = (namefunc)getname_mat_ei;
		adrcodes = ma_ar;
		size = MA_TOTIPO;
		break;
	case ID_CA:
		lookup_name = (namefunc)getname_cam_ei;
		adrcodes = cam_ar;
		size = CAM_TOTIPO;
		break;
	case ID_LA:
		lookup_name = (namefunc)getname_la_ei;
		adrcodes = la_ar;
		size = LA_TOTIPO;
		break;
	case ID_TE:
		lookup_name = (namefunc)getname_tex_ei;
		adrcodes = te_ar;
		size = TE_TOTIPO;
		break;
	case ID_WO:
		lookup_name = (namefunc)getname_world_ei;
		adrcodes = wo_ar;
		size = WO_TOTIPO;
		break;
	case ID_PO:
		lookup_name = (namefunc)getname_ac_ei;
		adrcodes = ac_ar;
		size = AC_TOTIPO;
		break;
	case ID_CO:
		lookup_name = (namefunc)getname_co_ei;
		adrcodes = co_ar;
		size = CO_TOTIPO;
		break;
	case ID_CU:
		lookup_name = (namefunc)getname_cu_ei;
		adrcodes = cu_ar;
		size = CU_TOTIPO;
		break;
	case ID_SEQ:
		lookup_name = (namefunc)getname_seq_ei;
		adrcodes = seq_ar;
		size = SEQ_TOTIPO;
		break;
	case ID_KE:	/* shouldn't happen */
	default:
		return -1;
	}

	while ( size-- ) {
		char *name = lookup_name ( *adrcodes );

		/* if not a texture channel, just return the adrcode */
		if( !strcmp( str, name ) ) {
			if( channel == -1 || *adrcodes < MA_MAP1 )
				return (short)*adrcodes;

		/* otherwise adjust adrcode to include current channel */
			else {
				int param = (short)*adrcodes & ~MA_MAP1;
				param |= texchannel_to_adrcode( channel );
				return (short)param;
			}
		}
		++adrcodes;
	}
	return -1;
}

static short lookup_curve_key( char *str, Ipo *ipo )
{
	Key *keyiter;

	/* find the ipo in the keylist */
	for( keyiter = G.main->key.first; keyiter; keyiter = keyiter->id.next ) {
		if( keyiter->ipo == ipo ) {
			KeyBlock *block = keyiter->block.first;

			/* look for a matching string, get the adrcode */
			for( block = keyiter->block.first; block; block = block->next )
				if( !strcmp( str, block->name ) )
					return block->adrcode;

			/* no match; no addr code */
			return -1;
		}
	}

	/* error if the ipo isn't in the list */
	return -2;
}

/*
 * Search through list of known Ipocurves for a particular adrcode.
 *
 * code: adrcode of curve we are searching for
 * blocktype: type of Ipo
 * channel: texture channel number, for World/Lamp/Material curves
 *
 * returns the adrcode for the named curve if it exists, -1 otherwise
 */

static short lookup_curve_adrcode( int code, int blocktype, int channel )
{
	int *adrcodes = NULL;
	int size = 0;

	switch ( blocktype ) {
	case ID_OB:
		adrcodes = ob_ar;
		size = OB_TOTIPO;
		break;
	case ID_MA:
		adrcodes = ma_ar;
		size = MA_TOTIPO;
		break;
	case ID_CA:
		adrcodes = cam_ar;
		size = CAM_TOTIPO;
		break;
	case ID_LA:
		adrcodes = la_ar;
		size = LA_TOTIPO;
		break;
	case ID_TE:
		adrcodes = te_ar;
		size = TE_TOTIPO;
		break;
	case ID_WO:
		adrcodes = wo_ar;
		size = WO_TOTIPO;
		break;
	case ID_PO:
		adrcodes = ac_ar;
		size = AC_TOTIPO;
		break;
	case ID_CO:
		adrcodes = co_ar;
		size = CO_TOTIPO;
		break;
	case ID_CU:
		adrcodes = cu_ar;
		size = CU_TOTIPO;
		break;
	case ID_SEQ:
		adrcodes = seq_ar;
		size = SEQ_TOTIPO;
		break;
	case ID_KE:
	default:
		return -1;
	}

	while ( size-- ) {
		if( *adrcodes == code ) {

		/* if not a texture channel, just return the adrcode */
			if( channel == -1 || *adrcodes < MA_MAP1 )
				return (short)*adrcodes;

		/* otherwise adjust adrcode to include current channel */
			else {
				int param = *adrcodes & ~MA_MAP1;
				param |= texchannel_to_adrcode( channel );
				return (short)param;
			}
		}
		++adrcodes;
	}
	return -1;
}

/*
 * Delete an IpoCurve from an Ipo
 */

static void del_ipocurve( Ipo * ipo, IpoCurve * icu ) {
	BLI_remlink( &( ipo->curve ), icu );
	if( icu->bezt )
		MEM_freeN( icu->bezt );
	if( icu->driver )
		MEM_freeN( icu->driver );
	MEM_freeN( icu );

	/* have to do this to avoid crashes in the IPO window */
	allspace( REMAKEIPO, 0 );
	EXPP_allqueue( REDRAWIPO, 0 );
}

/*****************************************************************************/
/* Python BPy_Ipo functions:                                                 */
/*****************************************************************************/

/*****************************************************************************/
/* Function:              M_Ipo_New                                          */
/* Python equivalent:     Blender.Ipo.New                                    */
/*****************************************************************************/

static PyObject *M_Ipo_New( PyObject * self_unused, PyObject * args )
{
	char *name = NULL, *code = NULL;
	int idcode = -1;
	Ipo *blipo;

	if( !PyArg_ParseTuple( args, "ss", &code, &name ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected two string arguments" );

	if( !strcmp( code, "Object" ) )
		idcode = ID_OB;
	else if( !strcmp( code, "Camera" ) )
		idcode = ID_CA;
	else if( !strcmp( code, "World" ) )
		idcode = ID_WO;
	else if( !strcmp( code, "Material" ) )
		idcode = ID_MA;
	else if( !strcmp( code, "Texture" ) )
		idcode = ID_TE;
	else if( !strcmp( code, "Lamp" ) )
		idcode = ID_LA;
	else if( !strcmp( code, "Action" ) )
		idcode = ID_PO;
	else if( !strcmp( code, "Constraint" ) )
		idcode = ID_CO;
	else if( !strcmp( code, "Sequence" ) )
		idcode = ID_SEQ;
	else if( !strcmp( code, "Curve" ) )
		idcode = ID_CU;
	else if( !strcmp( code, "Key" ) )
		idcode = ID_KE;
	else return EXPP_ReturnPyObjError( PyExc_ValueError,
			"unknown Ipo code" );

	blipo = add_ipo( name, idcode );

	if( blipo ) {
		/* return user count to zero because add_ipo() inc'd it */
		blipo->id.us = 0;
		/* create python wrapper obj */
		return Ipo_CreatePyObject( blipo );
	} else
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"couldn't create Ipo Data in Blender" );
}

/*****************************************************************************/
/* Function:              M_Ipo_Get                                       */
/* Python equivalent:     Blender.Ipo.Get                                 */
/* Description:           Receives a string and returns the ipo data obj  */
/*                        whose name matches the string.  If no argument is  */
/*                        passed in, a list of all ipo data names in the  */
/*                        current scene is returned.                         */
/*****************************************************************************/
static PyObject *M_Ipo_Get( PyObject * self_unused, PyObject * args )
{
	char *name = NULL;
	Ipo *ipo_iter;
	PyObject *ipolist, *pyobj;
	char error_msg[64];

	if( !PyArg_ParseTuple( args, "|s", &name ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected string argument (or nothing)" ) );

	ipo_iter = G.main->ipo.first;

	if( name ) {		/* (name) - Search ipo by name */
		while( ipo_iter ) {
			if( !strcmp( name, ipo_iter->id.name + 2 ) ) {
				return Ipo_CreatePyObject( ipo_iter );
			}
			ipo_iter = ipo_iter->id.next;
		}

		PyOS_snprintf( error_msg, sizeof( error_msg ),
				   "Ipo \"%s\" not found", name );
		return EXPP_ReturnPyObjError( PyExc_NameError, error_msg );
	}

	else {			/* () - return a list with all ipos in the scene */
		int index = 0;

		ipolist = PyList_New( BLI_countlist( &( G.main->ipo ) ) );

		if( !ipolist )
			return EXPP_ReturnPyObjError( PyExc_MemoryError,
					"couldn't create PyList" );

		while( ipo_iter ) {
			pyobj = Ipo_CreatePyObject( ipo_iter );

			if( !pyobj )
				return NULL;

			PyList_SET_ITEM( ipolist, index, pyobj );

			ipo_iter = ipo_iter->id.next;
			index++;
		}

		return ipolist;
	}
}

/*
 * This should probably be deprecated too?  Or else documented in epydocs.
 * Seems very similar to Ipocurve.recalc().
 */

/*****************************************************************************/
/* Function:              M_Ipo_Recalc                                       */
/* Python equivalent:     Blender.Ipo.Recalc                                 */
/* Description:           Receives (presumably) an IpoCurve object and       */
/*                        updates the curve after changes to control points. */
/*****************************************************************************/
static PyObject *M_Ipo_Recalc( PyObject * self_unused, PyObject * value )
{
	if( !BPy_IpoCurve_Check(value) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected Ipo curve argument" );

	testhandles_ipocurve( IpoCurve_FromPyObject( value ) );

	Py_RETURN_NONE;
}

/*****************************************************************************/
/* Python BPy_Ipo methods:                                                   */
/*****************************************************************************/
static PyObject *Ipo_getBlocktype( BPy_Ipo * self )
{
	return PyInt_FromLong( self->ipo->blocktype );
}

static int Ipo_setBlocktype( BPy_Ipo * self, PyObject * args )
{
	if( !PyInt_Check( args ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected int argument" );

	self->ipo->blocktype = (short)PyInt_AS_LONG( args );

	return 0;
}

static PyObject *Ipo_getRctf( BPy_Ipo * self )
{
	PyObject *l = PyList_New( 4 );
	PyList_SET_ITEM( l, 0, PyFloat_FromDouble( self->ipo->cur.xmin ) );
	PyList_SET_ITEM( l, 1, PyFloat_FromDouble( self->ipo->cur.xmax ) );
	PyList_SET_ITEM( l, 2, PyFloat_FromDouble( self->ipo->cur.ymin ) );
	PyList_SET_ITEM( l, 3, PyFloat_FromDouble( self->ipo->cur.ymax ) );
	return l;
}

static int Ipo_setRctf( BPy_Ipo * self, PyObject * args )
{
	float v[4];

	if( !PyArg_ParseTuple( args, "ffff", v, v + 1, v + 2, v + 3 ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected a tuple of 4 floats" );

	self->ipo->cur.xmin = v[0];
	self->ipo->cur.xmax = v[1];
	self->ipo->cur.ymin = v[2];
	self->ipo->cur.ymax = v[3];

	return 0;
}

/*
 * Get total number of Ipo curves for this Ipo.  NOTE:  this function
 * returns all curves for Ipos which have texture channels, unlike
 * Ipo_length().
 */

static PyObject *Ipo_getNcurves( BPy_Ipo * self )
{
	IpoCurve *icu;
	int i = 0;

	for( icu = self->ipo->curve.first; icu; icu = icu->next ) {
		i++;
	}

	return PyInt_FromLong( (long)i );
}

/* 
   Function:  Ipo_addCurve
   Bpy:       Blender.Ipo.addCurve( 'curname')

   add a new curve to an existing IPO.
   example:
   ipo = Blender.Ipo.New('Object','ObIpo')
   cu = ipo.addCurve('LocX')
*/

static PyObject *Ipo_addCurve( BPy_Ipo * self, PyObject * value )
{
	short param;		/* numeric curve name constant */
	char *cur_name = PyString_AsString(value);	/* input arg: curve name */
	Ipo *ipo = 0;
	IpoCurve *icu = 0;
	Link *link;

	if( !cur_name )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "expected string argument" ) );


	/* chase down the ipo list looking for ours */
	link = G.main->ipo.first;

	while( link ) {
		ipo = ( Ipo * ) link;
		if( ipo == self->ipo )
			break;
		link = link->next;
	}

	if( !link )
		return EXPP_ReturnPyObjError
				( PyExc_RuntimeError, "Ipo not found" );

	/*
	 * Check if the input arg curve name is valid depending on the block
	 * type, and set param to numeric value.  Invalid names will return
	 * param = -1.
	 */

	if( ipo->blocktype != ID_KE ) {
		param = lookup_curve_name( cur_name, ipo->blocktype, self->mtex );
	} else {
		param = lookup_curve_key( cur_name, ipo );
		if( param == -2 )
			return EXPP_ReturnPyObjError( PyExc_RuntimeError, 
					"unable to find matching key data for Ipo" );
	}

	if( param == -1 )
		return EXPP_ReturnPyObjError( PyExc_NameError,
				"curve name is not valid" );

	/* see if the curve already exists */
	for( icu = ipo->curve.first; icu; icu = icu->next )
		if( icu->adrcode == param )
			return EXPP_ReturnPyObjError( PyExc_ValueError,
					"Ipo curve already exists" );

	/* create the new ipo curve */
	icu = MEM_callocN( sizeof(IpoCurve), "Python added ipocurve");
	icu->blocktype = ipo->blocktype;
	icu->adrcode = param;
	icu->flag |= IPO_VISIBLE|IPO_AUTO_HORIZ;
	set_icu_vars( icu );
	BLI_addtail( &(ipo->curve), icu);

	allspace( REMAKEIPO, 0 );
	EXPP_allqueue( REDRAWIPO, 0 );

	/* create a bpy wrapper for the new ipo curve */
	return IpoCurve_CreatePyObject( icu );
}

/* 
   Function:  Ipo_delCurve
   Bpy:       Blender.Ipo.delCurve(curtype)

   delete an existing curve from IPO.
   example:
       ipo = Blender.Ipo.New('Object','ObIpo')
       cu = ipo.delCurve('LocX')
*/

static PyObject *Ipo_delCurve( BPy_Ipo * self, PyObject * value )
{
	IpoCurve *icu;
	char *strname = PyString_AsString(value);

	if( !strname )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected string argument" );

	for( icu = self->ipo->curve.first; icu; icu = icu->next ) {
		if( !strcmp( strname, getIpoCurveName( icu ) ) ) {
			del_ipocurve( self->ipo, icu );
    		Py_RETURN_NONE;
		}
	}

	return EXPP_ReturnPyObjError( PyExc_ValueError, "IpoCurve not found" );
}
/*
 */

static PyObject *Ipo_getCurve( BPy_Ipo * self, PyObject * args )
{
    IpoCurve *icu = NULL;
    short adrcode;
    PyObject *value = NULL;

    if( !PyArg_ParseTuple( args, "|O", &value ) )
		goto typeError;
 
    /* if no name give, get all the Ipocurves */
	if( !value )
		return Ipo_getCurves( self );

	/* if arg is a string or int, look up the adrcode */
    if( PyString_Check( value ) ) {
		char *str = PyString_AsString( value );
        for( icu = self->ipo->curve.first; icu; icu = icu->next ) {
            if( !strcmp( str, getIpoCurveName( icu ) ) )
                return IpoCurve_CreatePyObject( icu );
        }
    	Py_RETURN_NONE;
	}
    else if( PyInt_Check( value ) ) {
        adrcode = ( short )PyInt_AsLong( value );
        for( icu = self->ipo->curve.first; icu; icu = icu->next ) {
            if( icu->adrcode == adrcode )
                 return IpoCurve_CreatePyObject( icu );
		}
    	Py_RETURN_NONE;
	}

typeError:
	return EXPP_ReturnPyObjError(PyExc_TypeError,
			"expected string or int argument" );
} 

static PyObject *Ipo_getCurves( BPy_Ipo * self )
{
	PyObject *attr = PyList_New( 0 ), *pyipo;
	IpoCurve *icu;

	for( icu = self->ipo->curve.first; icu; icu = icu->next ) {
		pyipo = IpoCurve_CreatePyObject( icu );
		PyList_Append( attr, pyipo );
		Py_DECREF(pyipo);
	}
	return attr;
}

/*
 * return a list of valid curve name constants for the Ipo
 */

static PyObject *Ipo_getCurveNames( BPy_Ipo * self )
{
	namefunc lookup_name;
	int size;
	PyObject *dict;
	int *vals = NULL;
	char name[32];
	PyObject *attr = Py_None;

	/* determine what type of Ipo we are */

	switch ( self->ipo->blocktype ) {
	case ID_OB:
		lookup_name = (namefunc)getname_ob_ei;
		vals = ob_ar;
		size = OB_TOTIPO;
		strcpy( name, "OB_" );
		break;
	case ID_MA:
		lookup_name = (namefunc)getname_mat_ei;
		vals = ma_ar;
		size = MA_TOTIPO;
		strcpy( name, "MA_" );
		break;
	case ID_CA:
		lookup_name = (namefunc)getname_cam_ei;
		vals = cam_ar;
		size = CAM_TOTIPO;
		strcpy( name, "CA_" );
		break;
	case ID_LA:
		lookup_name = (namefunc)getname_la_ei;
		vals = la_ar;
		size = LA_TOTIPO;
		strcpy( name, "LA_" );
		break;
	case ID_TE:
		lookup_name = (namefunc)getname_tex_ei;
		vals = te_ar;
		size = TE_TOTIPO;
		strcpy( name, "TE_" );
		break;
	case ID_WO:
		lookup_name = (namefunc)getname_world_ei;
		vals = wo_ar;
		size = WO_TOTIPO;
		strcpy( name, "WO_" );
		break;
	case ID_PO:
		lookup_name = (namefunc)getname_ac_ei;
		vals = ac_ar;
		size = AC_TOTIPO;
		strcpy( name, "PO_" );
		break;
	case ID_CO:
		lookup_name = (namefunc)getname_co_ei;
		vals = co_ar;
		size = CO_TOTIPO;
		strcpy( name, "CO_" );
		break;
	case ID_CU:
		lookup_name = (namefunc)getname_cu_ei;
		vals = cu_ar;
		size = CU_TOTIPO;
		strcpy( name, "CU_" );
		break;
	case ID_SEQ:
		lookup_name = (namefunc)getname_seq_ei;
		vals = seq_ar;
		size = SEQ_TOTIPO;
		strcpy( name, "SQ_" );
		break;
	case ID_KE:
		{
			Key *key;

			/* find the ipo in the keylist */
			for( key = G.main->key.first; key; key = key->id.next ) {
				if( key->ipo == self->ipo ) {
					PyObject *tmpstr;
					KeyBlock *block = key->block.first;
					attr = PyList_New( 0 );
					
					/* add each name to the list */
					for( block = key->block.first; block; block = block->next ) {
						tmpstr = PyString_FromString( block->name );
						PyList_Append( attr, tmpstr);
						Py_DECREF(tmpstr);
					}
					return attr;
				}
			}

			/* error if the ipo isn't in the list */
        	return EXPP_ReturnPyObjError( PyExc_RuntimeError, 
					"unable to find matching key data for Ipo" );
		}
	default:
		Py_DECREF( attr );
        return EXPP_ReturnPyObjError( PyExc_RuntimeError, 
			   				"unknown Ipo type" );
	}

	/*
	 * go through the list of adrcodes to find names, then add to dictionary
	 * with string as key and adrcode as value
	 */

	dict = PyModule_GetDict( submodule );
	attr = PyConstant_New();

	while( size-- ) {
		char *ptr = name+3;
		strcpy( name+3, lookup_name( *vals ) ); 
		while( *ptr ) {
			*ptr = (char)toupper( *ptr );
			++ptr;
		}
		PyConstant_Insert( (BPy_constant *)attr, name, 
				PyInt_FromLong( *vals ) );
		++vals;
	}
	return attr;
}

void generate_curveconsts( PyObject* module )
{
	namefunc lookup_name = NULL;
	int size = 0;
	int *vals = NULL;
	char name[32];

	unsigned int i = 0;
	static short curvelist[] = {
		ID_OB, ID_MA, ID_CA, ID_LA, ID_TE, ID_WO, ID_PO, ID_CO, ID_CU, ID_SEQ
	};

	for( i = 0; i < sizeof(curvelist)/sizeof(short); ++i ) {
		switch ( curvelist[i] ) {
		case ID_OB:
			lookup_name = (namefunc)getname_ob_ei;
			vals = ob_ar;
			size = OB_TOTIPO;
			strcpy( name, "OB_" );
			break;
		case ID_MA:
			lookup_name = (namefunc)getname_mat_ei;
			vals = ma_ar;
			size = MA_TOTIPO;
			strcpy( name, "MA_" );
			break;
		case ID_CA:
			lookup_name = (namefunc)getname_cam_ei;
			vals = cam_ar;
			size = CAM_TOTIPO;
			strcpy( name, "CA_" );
			break;
		case ID_LA:
			lookup_name = (namefunc)getname_la_ei;
			vals = la_ar;
			size = LA_TOTIPO;
			strcpy( name, "LA_" );
			break;
		case ID_TE:
			lookup_name = (namefunc)getname_tex_ei;
			vals = te_ar;
			size = TE_TOTIPO;
			strcpy( name, "TE_" );
			break;
		case ID_WO:
			lookup_name = (namefunc)getname_world_ei;
			vals = wo_ar;
			size = WO_TOTIPO;
			strcpy( name, "WO_" );
			break;
		case ID_PO:
			lookup_name = (namefunc)getname_ac_ei;
			vals = ac_ar;
			size = AC_TOTIPO;
			strcpy( name, "PO_" );
			break;
		case ID_CO:
			lookup_name = (namefunc)getname_co_ei;
			vals = co_ar;
			size = CO_TOTIPO;
			strcpy( name, "CO_" );
			break;
		case ID_CU:
			lookup_name = (namefunc)getname_cu_ei;
			vals = cu_ar;
			size = CU_TOTIPO;
			strcpy( name, "CU_" );
			break;
		case ID_SEQ:
			lookup_name = (namefunc)getname_seq_ei;
			vals = seq_ar;
			size = SEQ_TOTIPO;
			strcpy( name, "SQ_" );
			break;
		}

		while( size-- ) {
			char *ptr = name+3;
			strcpy( name+3, lookup_name( *vals ) ); 
			while( *ptr ) {
				*ptr = (char)toupper( *ptr );
				++ptr;
			}
			PyModule_AddIntConstant( module, name, *vals );
			++vals;
		}
	}
}


/*
 * get the current texture channel number, if defined
 */

static PyObject *Ipo_getChannel( BPy_Ipo * self )
{
	if( self->mtex != -1 )
		return PyInt_FromLong( (long)self->mtex );
	Py_RETURN_NONE;
}

/*
 * set the current texture channel number, if defined
 */

static int Ipo_setChannel( BPy_Ipo * self, PyObject * value )
{
	if( self->mtex != -1 )
		return EXPP_setIValueRange( value, &self->mtex, 0, 9, 'h' );
	return 0;
}

/*****************************************************************************/
/* Function:    Ipo_compare			                                         */
/* Description: This compares 2 ipo python types, == or != only.             */
/*****************************************************************************/
static int Ipo_compare( BPy_Ipo * a, BPy_Ipo * b )
{
	return ( a->ipo == b->ipo ) ? 0 : -1;
}

/*****************************************************************************/
/* Function:    Ipo_repr                                                     */
/* Description: This is a callback function for the BPy_Ipo type. It         */
/*              builds a meaningful string to represent ipo objects.         */
/*****************************************************************************/
static PyObject *Ipo_repr( BPy_Ipo * self )
{
	char *param;

	switch ( self->ipo->blocktype ) {
	case ID_OB:
		param = "Object"; break;
	case ID_CA:
		param = "Camera"; break;
	case ID_LA:
		param = "Lamp"; break;
	case ID_TE:
		param = "Texture"; break;
	case ID_WO:
		param = "World"; break;
	case ID_MA:
		param = "Material"; break;
	case ID_PO:
		param = "Action"; break;
	case ID_CO:
		param = "Constriant"; break;
	case ID_CU:
		param = "Curve"; break;
	case ID_SEQ:
		param = "Sequence"; break;
	case ID_KE:
		param = "Key"; break;
	default:
        return EXPP_ReturnPyObjError( PyExc_RuntimeError, 
			   				"unknown Ipo type" );
	}
	return PyString_FromFormat( "[Ipo \"%s\" (%s)]", self->ipo->id.name + 2,
				    param );
}

/* Three Python Ipo_Type helper functions needed by the Object module: */

/*****************************************************************************/
/* Function:    Ipo_CreatePyObject                                           */
/* Description: This function will create a new BPy_Ipo from an existing     */
/*              Blender ipo structure.                                       */
/*****************************************************************************/
PyObject *Ipo_CreatePyObject( Ipo * ipo )
{
	BPy_Ipo *pyipo;
	pyipo = ( BPy_Ipo * ) PyObject_NEW( BPy_Ipo, &Ipo_Type );
	if( !pyipo )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create BPy_Ipo object" );
	pyipo->ipo = ipo;
	pyipo->iter = 0;
	if( pyipo->ipo->blocktype == ID_WO || pyipo->ipo->blocktype == ID_LA ||
			pyipo->ipo->blocktype == ID_MA )
		pyipo->mtex = 0;
	else
		pyipo->mtex = -1;
	return ( PyObject * ) pyipo;
}

/*****************************************************************************/
/* Function:    Ipo_FromPyObject                                             */
/* Description: This function returns the Blender ipo from the given         */
/*              PyObject.                                                    */
/*****************************************************************************/
Ipo *Ipo_FromPyObject( PyObject * pyobj )
{
	return ( ( BPy_Ipo * ) pyobj )->ipo;
}

/*****************************************************************************/
/* Function:    Ipo_length                                                   */
/* Description: This function counts the number of curves accessible for the */
/*              PyObject.                                                    */
/*****************************************************************************/
static int Ipo_length( BPy_Ipo * self )
{
	IpoCurve *icu;
	int len = 0;

	for( icu = self->ipo->curve.first; icu; icu = icu->next ) {
		if( self->mtex == -1 || icu->adrcode < MA_MAP1 ||
				icu->adrcode & texchannel_to_adrcode( self->mtex ) )
			++len;
	}
	return len;
}

/*
 * "mapping" operator getter: return an IpoCurve it we can find it
 */

static PyObject *Ipo_getIpoCurveByName( BPy_Ipo * self, PyObject * key )
{
    IpoCurve *icu = NULL;
    int adrcode;
 
	/* if Ipo is not ShapeKey and arg is an int, look up the adrcode */
	if( self->ipo->blocktype != ID_KE && PyNumber_Check( key ) )
		adrcode = lookup_curve_adrcode( PyInt_AsLong( key ),
				self->ipo->blocktype, self->mtex );
	/* if Ipo is ShapeKey and arg is string, look up the adrcode */
	else if( self->ipo->blocktype == ID_KE && PyString_Check( key ) ) {
		adrcode = lookup_curve_key( PyString_AS_STRING( key ), self->ipo );
		if( adrcode == -2 )
			return EXPP_ReturnPyObjError( PyExc_RuntimeError, 
					"unable to find matching key data for Ipo" );
	}
	else
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected int or string key" );

	/* if no adrcode found, value error */
	if( adrcode == -1 )
		return EXPP_ReturnPyObjError( PyExc_KeyError, "invalid curve key" );

	/* search for a matching adrcode */
	for( icu = self->ipo->curve.first; icu; icu = icu->next )
		if( icu->adrcode == adrcode )
			return IpoCurve_CreatePyObject( icu );

	/* no curve found */
    Py_RETURN_NONE;
}

/*
 * "mapping" operator setter: create or delete an IpoCurve it we can find it
 */

static int Ipo_setIpoCurveByName( BPy_Ipo * self, PyObject * key, 
		PyObject * arg )
{
	IpoCurve *icu;
	Ipo *ipo = self->ipo;
	short adrcode;

	/* "del ipo[const]" will send NULL here; give an error */
	if( !arg )
		return EXPP_ReturnIntError( PyExc_NotImplementedError,
				"del operator not supported" );

	/* "del ipo[const]" will send NULL here; give an error */
	if( self->ipo->blocktype == ID_KE )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"creation or deletion of Shape Keys not supported" );

	/* check for int argument */
	if( !PyNumber_Check( key ) )
		return EXPP_ReturnIntError( PyExc_TypeError, "expected int key" );

	/* look up the key, return error if not found */
	adrcode = lookup_curve_adrcode( PyInt_AsLong( key ),
			self->ipo->blocktype, self->mtex );

	if( adrcode == -1 )
		return EXPP_ReturnIntError( PyExc_KeyError,
				"invalid curve specified" );

	/* if arg is None, delete the curve */
	if( arg == Py_None ) {
		for( icu = self->ipo->curve.first; icu; icu = icu->next ) {
			if( icu->adrcode == adrcode ) {
				del_ipocurve( ipo, icu );
				return 0;
			}
		}

		return EXPP_ReturnIntError( PyExc_ValueError, "IpoCurve not found" );
	} else {

	/* create the new ipo curve */
		float time, curval;
		PyObject *tmp, *flt=NULL, *val=NULL;

		/* error if not a sequence or sequence with other than 2 values */
		if( PySequence_Size( arg ) != 2 )
			goto AttrError;

		/* get the time and curval */
		tmp = PySequence_ITEM( arg, 0 );
		flt = PyNumber_Float( tmp );
		Py_DECREF( tmp );
		tmp = PySequence_ITEM( arg, 1 );
		val = PyNumber_Float( tmp );
		Py_DECREF( tmp );

		if( !flt || !val )
			goto AttrError;

		time = (float)PyFloat_AS_DOUBLE( flt );
		curval = (float)PyFloat_AS_DOUBLE( val );
		Py_DECREF( flt );
		Py_DECREF( val );

		/* if curve already exist, delete the original */
		for( icu = ipo->curve.first; icu; icu = icu->next )
			if( icu->adrcode == adrcode ) {
				del_ipocurve( ipo, icu );
				break;
			}

		/* create the new curve, then add the key */
		icu = MEM_callocN( sizeof(IpoCurve), "Python added ipocurve");
		icu->blocktype = ipo->blocktype;
		icu->adrcode = adrcode;
		icu->flag |= IPO_VISIBLE|IPO_AUTO_HORIZ;
		set_icu_vars( icu );
		BLI_addtail( &(ipo->curve), icu);
		insert_vert_icu( icu, time, curval );

		allspace( REMAKEIPO, 0 );
		EXPP_allqueue( REDRAWIPO, 0 );

		return 0; 

AttrError:
		Py_XDECREF( val );
		Py_XDECREF( flt );
		return EXPP_ReturnIntError( PyExc_AttributeError,
				"expected sequence of two floats" );
	}
} 

/*
 * sequence __contains__ method (implements "x in ipo")
 */

static int Ipo_contains( BPy_Ipo *self, PyObject *key )
{
    IpoCurve *icu = NULL;
    int adrcode;

	/* take a Ipo curve name: key must be a int */ 

	if( self->ipo->blocktype != ID_KE && PyNumber_Check( key ) ) {
		adrcode = lookup_curve_adrcode( PyInt_AsLong( key ),
			self->ipo->blocktype, self->mtex );

		/* if we found an adrcode for the key, search the ipo's curve */
		if( adrcode != -1 ) {
			for( icu = self->ipo->curve.first; icu; icu = icu->next )
				if( icu->adrcode == adrcode )
					return 1;
		}
	} else if( self->ipo->blocktype == ID_KE && PyString_Check( key ) ) {
		adrcode = lookup_curve_key( PyString_AS_STRING( key ), self->ipo );

		/* if we found an adrcode for the key, search the ipo's curve */
		if( adrcode >= 0 ) {
			for( icu = self->ipo->curve.first; icu; icu = icu->next )
				if( icu->adrcode == adrcode )
					return 1;
		}
	} 

	/* no curve found */
    return 0;
}

/*
 * Initialize the interator index
 */

static PyObject *Ipo_getIter( BPy_Ipo * self )
{
	/* return a new IPO object if we are looping on the existing one
	   This allows nested loops */
	if (self->iter==0) {
		return EXPP_incr_ret ( (PyObject *) self );
	} else {
		return Ipo_CreatePyObject(self->ipo);
	}
}

/*
 * Get the next Ipo curve
 */

static PyObject *Ipo_nextIter( BPy_Ipo * self )
{
	int i;
	IpoCurve *icu = self->ipo->curve.first;

	++self->iter;

		/*
		 * count curves only if 
		 * (a) Ipo has no texture channels
		 * (b) Ipo has texture channels, but curve is not that type
		 * (c) Ipo has texture channels, and curve is that type, and it is
		 *    in the active texture channel
		 */
	for( i = 0; icu; icu = icu->next ) {
		if( self->mtex == -1 || icu->adrcode < MA_MAP1 ||
				icu->adrcode & texchannel_to_adrcode( self->mtex ) ) {
			++i;

			/* if indices match, return the curve */
			if( i == self->iter )
				return IpoCurve_CreatePyObject( icu );
		}
	}

	self->iter = 0; /* allow iter use again */
	/* ran out of curves */
	return EXPP_ReturnPyObjError( PyExc_StopIteration,
			"iterator at end" );
}

/*****************************************************************************/
/* Function:              Ipo_Init                                           */
/*****************************************************************************/
PyObject *Ipo_Init( void )
{
	/* PyObject *submodule; */

	if( PyType_Ready( &Ipo_Type ) < 0 )
		return NULL;

	submodule = Py_InitModule3( "Blender.Ipo", M_Ipo_methods, M_Ipo_doc );
	generate_curveconsts( submodule );

	return submodule;
}

/*
 * The following methods should be deprecated when there are equivalent
 * methods in Ipocurve (if there aren't already).
 */

static PyObject *Ipo_getNBezPoints( BPy_Ipo * self, PyObject * args )
{
	int num = 0;
	IpoCurve *icu = NULL;

	if( !PyArg_ParseTuple( args, "i", &num ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected int argument" );

	icu = self->ipo->curve.first;
	while( icu && num > 0 ) {
		icu = icu->next;
		--num;
	}

	if( num < 0 || !icu )
		return EXPP_ReturnPyObjError( PyExc_IndexError,
				"index out of range" );

	return PyInt_FromLong( icu->totvert );
}

static PyObject *Ipo_DeleteBezPoints( BPy_Ipo * self, PyObject * args )
{
	int num = 0, i = 0;
	IpoCurve *icu = 0;
	if( !PyArg_ParseTuple( args, "i", &num ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "expected int argument" ) );
	icu = self->ipo->curve.first;
	if( !icu )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "No IPO curve" ) );
	for( i = 0; i < num; i++ ) {
		if( !icu )
			return ( EXPP_ReturnPyObjError
				 ( PyExc_TypeError, "Bad curve number" ) );
		icu = icu->next;

	}
	icu->totvert--;
	return ( PyInt_FromLong( icu->totvert ) );
}

/*
 * Ipo_getCurveBP()
 * this method is UNSUPPORTED.
 * Calling this method throws a TypeError Exception.
 *
 * it looks like the original intent was to return the first point
 * of a BPoint Ipo curve.  However, BPoint ipos are not currently
 * implemented.
 */

static PyObject *Ipo_getCurveBP( BPy_Ipo * self_unused, PyObject * args_unused )
{
	return EXPP_ReturnPyObjError( PyExc_NotImplementedError,
				      "bpoint ipos are not supported" );
}

static PyObject *Ipo_getCurveBeztriple( BPy_Ipo * self, PyObject * args )
{
	struct BezTriple *ptrbt;
	int num = 0, pos, i, j;
	IpoCurve *icu;
	PyObject *l, *pyfloat;

	if( !PyArg_ParseTuple( args, "ii", &num, &pos ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "expected int argument" ) );
	icu = self->ipo->curve.first;
	if( !icu )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "No IPO curve" ) );
	for( i = 0; i < num; i++ ) {
		if( !icu )
			return ( EXPP_ReturnPyObjError
				 ( PyExc_TypeError, "Bad ipo number" ) );
		icu = icu->next;
	}
	if( pos >= icu->totvert )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "Bad bezt number" );

	ptrbt = icu->bezt + pos;
	if( !ptrbt )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "No bez triple" );

	l = PyList_New( 0 );
	for( i = 0; i < 3; i++ ) {
		for( j = 0; j < 3; j++ ) {
			pyfloat = PyFloat_FromDouble( ptrbt->vec[i][j] );
			PyList_Append( l, pyfloat );
			Py_DECREF(pyfloat);
		}
	}
	return l;
}

static PyObject *Ipo_setCurveBeztriple( BPy_Ipo * self, PyObject * args )
{
	struct BezTriple *ptrbt;
	int num = 0, pos, i;
	IpoCurve *icu;
	PyObject *listargs = 0;

	if( !PyArg_ParseTuple( args, "iiO", &num, &pos, &listargs ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError,
			   "expected int int object argument" ) );
	if( !PyTuple_Check( listargs ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "3rd arg should be a tuple" ) );
	icu = self->ipo->curve.first;
	if( !icu )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "No IPO curve" ) );
	for( i = 0; i < num; i++ ) {
		if( !icu )
			return ( EXPP_ReturnPyObjError
				 ( PyExc_TypeError, "Bad ipo number" ) );
		icu = icu->next;
	}
	if( pos >= icu->totvert )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "Bad bezt number" );

	ptrbt = icu->bezt + pos;
	if( !ptrbt )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "No bez triple" );

	for( i = 0; i < 9; i++ ) {
		PyObject *xx = PyTuple_GetItem( listargs, i );
		ptrbt->vec[i / 3][i % 3] = (float)PyFloat_AsDouble( xx );
	}

	Py_INCREF( Py_None );
	return Py_None;
}

/* Ipo.__copy__ */
static PyObject *Ipo_copy( BPy_Ipo * self )
{
	Ipo *ipo = copy_ipo(self->ipo );
	ipo->id.us = 0;
	return Ipo_CreatePyObject(ipo);
}

static PyObject *Ipo_EvaluateCurveOn( BPy_Ipo * self, PyObject * args )
{
	int num = 0, i;
	IpoCurve *icu;
	float time = 0;

	if( !PyArg_ParseTuple( args, "if", &num, &time ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "expected int argument" ) );

	icu = self->ipo->curve.first;
	if( !icu )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "No IPO curve" ) );

	for( i = 0; i < num; i++ ) {
		if( !icu )
			return ( EXPP_ReturnPyObjError
				 ( PyExc_TypeError, "Bad ipo number" ) );
		icu = icu->next;

	}
	return PyFloat_FromDouble( eval_icu( icu, time ) );
}

static PyObject *Ipo_getCurvecurval( BPy_Ipo * self, PyObject * args )
{
	int numcurve = 0, i;
	IpoCurve *icu;
	char *stringname = 0, *str1 = 0;

	icu = self->ipo->curve.first;
	if( !icu )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "No IPO curve" ) );

	if( PyNumber_Check( PyTuple_GetItem( args, 0 ) ) )	/* args is an integer */
	{
		if( !PyArg_ParseTuple( args, "i", &numcurve ) )
			return ( EXPP_ReturnPyObjError
				 ( PyExc_TypeError,
				   "expected int or string argument" ) );
		for( i = 0; i < numcurve; i++ ) {
			if( !icu )
				return ( EXPP_ReturnPyObjError
					 ( PyExc_TypeError,
					   "Bad ipo number" ) );
			icu = icu->next;
		}
	}

	else			/* args is a string */
	{
		if( !PyArg_ParseTuple( args, "s", &stringname ) )
			return ( EXPP_ReturnPyObjError
				 ( PyExc_TypeError,
				   "expected int or string argument" ) );
		while( icu ) {
			str1 = getIpoCurveName( icu );
			if( !strcmp( str1, stringname ) )
				break;
			icu = icu->next;
		}
	}

	if( icu )
		return PyFloat_FromDouble( icu->curval );
	Py_RETURN_NONE;
}

/*
 * The following methods should be deprecated when methods are pruned out.
 */

static PyObject *Ipo_oldsetRctf( BPy_Ipo * self, PyObject * args )
{
	return EXPP_setterWrapperTuple( (void *)self, args,
			(setter)Ipo_setRctf );
}

static PyObject *Ipo_oldsetBlocktype( BPy_Ipo * self, PyObject * args )
{
	return EXPP_setterWrapperTuple( (void *)self, args,
			(setter)Ipo_setBlocktype );
}
