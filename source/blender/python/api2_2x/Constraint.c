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
 * Contributor(s): Joseph Gilbert, Ken Hughes, Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "Constraint.h" /*This must come first*/

#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_effect_types.h"
#include "DNA_vec_types.h"
#include "DNA_curve_types.h"
#include "DNA_text_types.h"

#include "BKE_main.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BLI_blenlib.h"
#include "BIF_editconstraint.h"
#include "BIF_poseobject.h"
#include "BSE_editipo.h"
#include "MEM_guardedalloc.h"
#include "butspace.h"
#include "blendef.h"
#include "mydevice.h"

#include "IDProp.h"
#include "Object.h"
#include "NLA.h"
#include "Text.h"
#include "gen_utils.h"

enum constraint_constants {
	EXPP_CONSTR_XROT = 0,
	EXPP_CONSTR_YROT = 1,
	EXPP_CONSTR_ZROT = 2,
	EXPP_CONSTR_XSIZE = 10,
	EXPP_CONSTR_YSIZE = 11,
	EXPP_CONSTR_ZSIZE = 12,
	EXPP_CONSTR_XLOC = 20,
	EXPP_CONSTR_YLOC = 21,
	EXPP_CONSTR_ZLOC = 22,

	EXPP_CONSTR_MAXX = TRACK_X,
	EXPP_CONSTR_MAXY = TRACK_Y,
	EXPP_CONSTR_MAXZ = TRACK_Z,
	EXPP_CONSTR_MINX = TRACK_nX,
	EXPP_CONSTR_MINY = TRACK_nY,
	EXPP_CONSTR_MINZ = TRACK_nZ,

	EXPP_CONSTR_TARGET = 100,
	EXPP_CONSTR_STRETCH,
	EXPP_CONSTR_ITERATIONS,
	EXPP_CONSTR_BONE,
	EXPP_CONSTR_CHAINLEN,
	EXPP_CONSTR_POSWEIGHT,
	EXPP_CONSTR_ROTWEIGHT,
	EXPP_CONSTR_ROTATE,
	EXPP_CONSTR_USETIP,

	EXPP_CONSTR_ACTION,
	EXPP_CONSTR_START,
	EXPP_CONSTR_END,
	EXPP_CONSTR_MIN,
	EXPP_CONSTR_MAX,
	EXPP_CONSTR_KEYON,

	EXPP_CONSTR_TRACK,
	EXPP_CONSTR_UP,

	EXPP_CONSTR_RESTLENGTH,
	EXPP_CONSTR_VOLVARIATION,
	EXPP_CONSTR_VOLUMEMODE,
	EXPP_CONSTR_PLANE,

	EXPP_CONSTR_FOLLOW,
	EXPP_CONSTR_OFFSET,
	EXPP_CONSTR_FORWARD,

	EXPP_CONSTR_LOCK,

	EXPP_CONSTR_MINMAX,
	EXPP_CONSTR_STICKY,

	EXPP_CONSTR_COPY,
	EXPP_CONSTR_LIMIT,
	EXPP_CONSTR_CLAMP,
	
	EXPP_CONSTR_MODE,
	
	EXPP_CONSTR_LIMXMIN = LIMIT_XMIN,
	EXPP_CONSTR_LIMXMAX = LIMIT_XMAX,
	EXPP_CONSTR_LIMYMIN = LIMIT_YMIN,
	EXPP_CONSTR_LIMYMAX = LIMIT_YMAX,
	EXPP_CONSTR_LIMZMIN = LIMIT_ZMIN,
	EXPP_CONSTR_LIMZMAX = LIMIT_ZMAX,
	
	EXPP_CONSTR_LIMXROT = LIMIT_XROT,
	EXPP_CONSTR_LIMYROT = LIMIT_YROT,
	EXPP_CONSTR_LIMZROT = LIMIT_ZROT,
	
	EXPP_CONSTR_CLAMPCYCLIC,
	
	EXPP_CONSTR_XMIN,
	EXPP_CONSTR_XMAX,
	EXPP_CONSTR_YMIN,
	EXPP_CONSTR_YMAX,
	EXPP_CONSTR_ZMIN,
	EXPP_CONSTR_ZMAX,
	
	EXPP_CONSTR_SCRIPT,
	EXPP_CONSTR_PROPS,
	
	EXPP_CONSTR_FROM,
	EXPP_CONSTR_TO,
	EXPP_CONSTR_EXPO,
	EXPP_CONSTR_FROMMINX,
	EXPP_CONSTR_FROMMAXX,
	EXPP_CONSTR_FROMMINY,
	EXPP_CONSTR_FROMMAXY,
	EXPP_CONSTR_FROMMINZ,
	EXPP_CONSTR_FROMMAXZ,
	EXPP_CONSTR_TOMINX,
	EXPP_CONSTR_TOMAXX,
	EXPP_CONSTR_TOMINY,
	EXPP_CONSTR_TOMAXY,
	EXPP_CONSTR_TOMINZ,
	EXPP_CONSTR_TOMAXZ,
	EXPP_CONSTR_MAPX,
	EXPP_CONSTR_MAPY,
	EXPP_CONSTR_MAPZ,
	
	EXPP_CONSTR_OWNSPACE,
	EXPP_CONSTR_TARSPACE,
		
	EXPP_CONSTR_RB_TYPE,
	EXPP_CONSTR_RB_BALL,
	EXPP_CONSTR_RB_HINGE,
	EXPP_CONSTR_RB_GENERIC6DOF,
	EXPP_CONSTR_RB_VEHICLE,
	EXPP_CONSTR_RB_PIVX,
	EXPP_CONSTR_RB_PIVY,
	EXPP_CONSTR_RB_PIVZ,
	EXPP_CONSTR_RB_AXX,
	EXPP_CONSTR_RB_AXY,
	EXPP_CONSTR_RB_AXZ,
	EXPP_CONSTR_RB_MINLIMIT0,
	EXPP_CONSTR_RB_MINLIMIT1,
	EXPP_CONSTR_RB_MINLIMIT2,
	EXPP_CONSTR_RB_MINLIMIT3,
	EXPP_CONSTR_RB_MINLIMIT4,
	EXPP_CONSTR_RB_MINLIMIT5,
	EXPP_CONSTR_RB_MAXLIMIT0,
	EXPP_CONSTR_RB_MAXLIMIT1,
	EXPP_CONSTR_RB_MAXLIMIT2,
	EXPP_CONSTR_RB_MAXLIMIT3,
	EXPP_CONSTR_RB_MAXLIMIT4,
	EXPP_CONSTR_RB_MAXLIMIT5,
	EXPP_CONSTR_RB_EXTRAFZ,
	EXPP_CONSTR_RB_FLAG,
	
};

/*****************************************************************************/
/* Python BPy_Constraint methods declarations:                               */
/*****************************************************************************/
static PyObject *Constraint_getName( BPy_Constraint * self );
static int Constraint_setName( BPy_Constraint * self, PyObject *arg );
static PyObject *Constraint_getType( BPy_Constraint * self );
static PyObject *Constraint_getInfluence( BPy_Constraint * self );
static int Constraint_setInfluence( BPy_Constraint * self, PyObject * arg );

static PyObject *Constraint_insertKey( BPy_Constraint * self, PyObject * arg );

static PyObject *Constraint_getData( BPy_Constraint * self, PyObject * key );
static int Constraint_setData( BPy_Constraint * self, PyObject * key, 
		PyObject * value );

/*****************************************************************************/
/* Python BPy_Constraint methods table:                                      */
/*****************************************************************************/
static PyMethodDef BPy_Constraint_methods[] = {
	/* name, method, flags, doc */
	{"insertKey", ( PyCFunction ) Constraint_insertKey, METH_O,
	 "Insert influence keyframe for constraint"},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python BPy_Constraint attributes get/set structure:                       */
/*****************************************************************************/
static PyGetSetDef BPy_Constraint_getseters[] = {
	{"name",
	(getter)Constraint_getName, (setter)Constraint_setName,
	 "Constraint name", NULL},
	{"type",
	(getter)Constraint_getType, (setter)NULL,
	 "Constraint type (read only)", NULL},
	{"influence",
	(getter)Constraint_getInfluence, (setter)Constraint_setInfluence,
	 "Constraint influence", NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/*****************************************************************************/
/* Python Constraint_Type Mapping Methods table:                             */
/*****************************************************************************/
static PyMappingMethods Constraint_as_mapping = {
	NULL,                               /* mp_length        */
	( binaryfunc ) Constraint_getData,	/* mp_subscript     */
	( objobjargproc ) Constraint_setData,	/* mp_ass_subscript */
};

/*****************************************************************************/
/* Python Constraint_Type callback function prototypes:                      */
/*****************************************************************************/
static PyObject *Constraint_repr( BPy_Constraint * self );
static int Constraint_compare( BPy_Constraint * a, BPy_Constraint * b );

/*****************************************************************************/
/* Python Constraint_Type structure definition:                              */
/*****************************************************************************/
PyTypeObject Constraint_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender Constraint",         /* char *tp_name; */
	sizeof( BPy_Constraint ),     /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	NULL,						/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) Constraint_compare, /* cmpfunc tp_compare; */
	( reprfunc ) Constraint_repr, /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	&Constraint_as_mapping,       /* PyMappingMethods *tp_as_mapping; */

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
	BPy_Constraint_methods,       /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_Constraint_getseters,     /* struct PyGetSetDef *tp_getset; */
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
/* Python BPy_Constraint methods:                                            */
/*****************************************************************************/

/*
 * return the name of this constraint
 */

static PyObject *Constraint_getName( BPy_Constraint * self )
{
	if( !self->con )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This constraint has been removed!" );
	
	return PyString_FromString( self->con->name );
}

/*
 * set the name of this constraint
 */

static int Constraint_setName( BPy_Constraint * self, PyObject * attr )
{
	char *name = PyString_AsString( attr );
	if( !name )
		return EXPP_ReturnIntError( PyExc_TypeError, "expected string arg" );

	if( !self->con )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"This constraint has been removed!" );
	
	BLI_strncpy( self->con->name, name, sizeof( self->con->name ) );

	return 0;
}

/*
 * return the influence of this constraint
 */

static PyObject *Constraint_getInfluence( BPy_Constraint * self )
{
	if( !self->con )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This constraint has been removed!" );
	
	return PyFloat_FromDouble( (double)self->con->enforce );
}

/*
 * set the influence of this constraint
 */

static int Constraint_setInfluence( BPy_Constraint * self, PyObject * value )
{
	if( !self->con )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"This constraint has been removed!" );

	return EXPP_setFloatClamped( value, &self->con->enforce, 0.0, 1.0 );
}

/*
 * return the type of this constraint
 */

static PyObject *Constraint_getType( BPy_Constraint * self )
{
	if( !self->con )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This constraint has been removed!" );
	
	return PyInt_FromLong( self->con->type );
}

/*
 * add keyframe for influence
	base on code in add_influence_key_to_constraint_func()
 */
static PyObject *Constraint_insertKey( BPy_Constraint * self, PyObject * value )
{
	bConstraint *con = self->con;
	Object *ob = self->obj;
	bPoseChannel *pchan = self->pchan;
	IpoCurve *icu;
	float cfra = (float)PyFloat_AsDouble(value);
	char actname[32] = "";
	
	if( !self->con )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This constraint has been removed!" );

	/* get frame for inserting key */
	if( PyFloat_Check(value) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a float argument" );
	
	/* find actname for locating that action-channel that a constraint channel should be added to */
	if (ob) {
		if (pchan) {
			/* actname is the name of the pchan that this constraint belongs to */
			BLI_strncpy(actname, pchan->name, 32);
		}
		else {
			/* hardcoded achan name -> "Object" (this may change in future) */
			strcpy(actname, "Object");
		}
	}
	else {
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"constraint doesn't belong to anything" );
	}
	icu= verify_ipocurve((ID *)ob, ID_CO, actname, con->name, NULL, CO_ENFORCE);
	
	if (!icu)
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"cannot get a curve from this IPO, may be using libdata" );
	
	if( ob->action )
		insert_vert_icu( icu, get_action_frame(ob, cfra), con->enforce, 0);
	else
		insert_vert_icu( icu, cfra, con->enforce, 0);

	Py_RETURN_NONE;
}

/******************************************************************************/
/* Constraint Space Conversion get/set procedures							  */
/* 		- These are called before/instead of individual constraint 			  */
/*		  get/set procedures when OWNERSPACE or TARGETSPACE are chosen		  */
/*		- They are only called from Constraint_g/setData					  */
/******************************************************************************/

static PyObject *constspace_getter( BPy_Constraint * self, int type )
{
	bConstraint *con= (bConstraint *)(self->con);
	
	/* depends on type being asked for
	 * NOTE: not all constraints support all space types 
	 */
	if (type == EXPP_CONSTR_OWNSPACE) {
		switch (con->type) {
			/* all of these support this... */
			case CONSTRAINT_TYPE_PYTHON:
			case CONSTRAINT_TYPE_LOCLIKE:
			case CONSTRAINT_TYPE_ROTLIKE:
			case CONSTRAINT_TYPE_SIZELIKE:
			case CONSTRAINT_TYPE_TRACKTO:
			case CONSTRAINT_TYPE_LOCLIMIT:
			case CONSTRAINT_TYPE_ROTLIMIT:
			case CONSTRAINT_TYPE_SIZELIMIT:
			case CONSTRAINT_TYPE_TRANSFORM:
				return PyInt_FromLong( (long)con->ownspace );
		}
	}
	else if (type == EXPP_CONSTR_TARSPACE) {
		switch (con->type) {
			/* all of these support this... */
			case CONSTRAINT_TYPE_PYTHON:
			case CONSTRAINT_TYPE_ACTION:
			case CONSTRAINT_TYPE_LOCLIKE:
			case CONSTRAINT_TYPE_ROTLIKE:
			case CONSTRAINT_TYPE_SIZELIKE:
			case CONSTRAINT_TYPE_TRACKTO:
			case CONSTRAINT_TYPE_TRANSFORM:
			{	
				bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
				bConstraintTarget *ct;
				PyObject *tlist=NULL, *val;
				
				if (cti) {
					/* change space of targets */
					if (cti->get_constraint_targets) {
						ListBase targets = {NULL, NULL};
						int num_tars=0, i=0;
						
						/* get targets, and create py-list for use temporarily */
						num_tars= cti->get_constraint_targets(con, &targets);
						if (num_tars) {
							tlist= PyList_New(num_tars);
							
							for (ct=targets.first; ct; ct=ct->next, i++) {
								val= PyInt_FromLong((long)ct->space);
								PyList_SET_ITEM(tlist, i, val);
							}
						}
						
						if (cti->flush_constraint_targets)
							cti->flush_constraint_targets(con, &targets, 1);
					}
				}
				
				return tlist;
			}
		}
	}
	
	/* raise error if failed */
	return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
}

static int constspace_setter( BPy_Constraint *self, int type, PyObject *value )
{
	bConstraint *con= (bConstraint *)(self->con);
	
	/* depends on type being asked for
	 * NOTE: not all constraints support all space types 
	 */
	if (type == EXPP_CONSTR_OWNSPACE) {
		switch (con->type) {
			/* all of these support this... */
			case CONSTRAINT_TYPE_PYTHON:
			case CONSTRAINT_TYPE_LOCLIKE:
			case CONSTRAINT_TYPE_ROTLIKE:
			case CONSTRAINT_TYPE_SIZELIKE:
			case CONSTRAINT_TYPE_TRACKTO:
			case CONSTRAINT_TYPE_LOCLIMIT:
			case CONSTRAINT_TYPE_ROTLIMIT:
			case CONSTRAINT_TYPE_SIZELIMIT:
			case CONSTRAINT_TYPE_TRANSFORM:
			{
				/* only copy depending on ownertype */
				if (self->pchan) {
					return EXPP_setIValueClamped( value, &con->ownspace, 
							CONSTRAINT_SPACE_WORLD, CONSTRAINT_SPACE_PARLOCAL, 'h' );
				}
				else {
					return EXPP_setIValueClamped( value, &con->ownspace, 
							CONSTRAINT_SPACE_WORLD, CONSTRAINT_SPACE_LOCAL, 'h' );
				}
			}
				break;
		}
	}
	else if (type == EXPP_CONSTR_TARSPACE) {
		switch (con->type) {
			/* all of these support this... */
			case CONSTRAINT_TYPE_PYTHON:
			case CONSTRAINT_TYPE_ACTION:
			case CONSTRAINT_TYPE_LOCLIKE:
			case CONSTRAINT_TYPE_ROTLIKE:
			case CONSTRAINT_TYPE_SIZELIKE:
			case CONSTRAINT_TYPE_TRACKTO:
			case CONSTRAINT_TYPE_TRANSFORM:
			{
				bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
				bConstraintTarget *ct;
				int ok= 0;
				
				if (cti) {
					/* change space of targets */
					if (cti->get_constraint_targets) {
						ListBase targets = {NULL, NULL};
						int num_tars=0, i=0;
						
						/* get targets, and extract values from the given list */
						num_tars= cti->get_constraint_targets(con, &targets);
						if (num_tars) {
							if ((PySequence_Check(value) == 0) || (PySequence_Size(value) != num_tars)) {
								char errorstr[64];
								sprintf(errorstr, "expected sequence of %d integer(s)", num_tars);
								return EXPP_ReturnIntError(PyExc_TypeError, errorstr);
							}
							
							for (ct=targets.first; ct; ct=ct->next, i++) {
								if (ct->tar && ct->subtarget[0]) {
									PyObject *val= PySequence_ITEM(value, i);
									
									ok += EXPP_setIValueClamped(val, &ct->space, 
											CONSTRAINT_SPACE_WORLD, CONSTRAINT_SPACE_PARLOCAL, 'h' );
										
									Py_DECREF(val);
								}
								else if (ct->tar) {
									PyObject *val= PySequence_ITEM(value, i);
									
									ok += EXPP_setIValueClamped(val, &ct->space, 
											CONSTRAINT_SPACE_WORLD, CONSTRAINT_SPACE_LOCAL, 'h' );
									
									Py_DECREF(val);
								}
							}
						}
						
						if (cti->flush_constraint_targets)
							cti->flush_constraint_targets(con, &targets, 0);
					}
				}
				
				return ok;
			}
				break;
		}
	}
	
	/* raise error if failed */
	return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
}

/*****************************************************************************/
/* Specific constraint get/set procedures                                    */
/*****************************************************************************/

static PyObject *kinematic_getter( BPy_Constraint * self, int type )
{
	bKinematicConstraint *con = (bKinematicConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_TARGET:
		return Object_CreatePyObject( con->tar );
	case EXPP_CONSTR_BONE:
		return PyString_FromString( con->subtarget );
	case EXPP_CONSTR_STRETCH:
		return PyBool_FromLong( (long)( con->flag & CONSTRAINT_IK_STRETCH ) ) ;
	case EXPP_CONSTR_ITERATIONS:
		return PyInt_FromLong( (long)con->iterations );
	case EXPP_CONSTR_CHAINLEN:
		return PyInt_FromLong( (long)con->rootbone );
	case EXPP_CONSTR_POSWEIGHT:
		return PyFloat_FromDouble( (double)con->weight );
	case EXPP_CONSTR_ROTWEIGHT:
		return PyFloat_FromDouble( (double)con->orientweight );
	case EXPP_CONSTR_ROTATE:
		return PyBool_FromLong( (long)( con->flag & CONSTRAINT_IK_ROT ) ) ;
	case EXPP_CONSTR_USETIP:
		return PyBool_FromLong( (long)( con->flag & CONSTRAINT_IK_TIP ) ) ;
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
	}
}

static int kinematic_setter( BPy_Constraint *self, int type, PyObject *value )
{
	bKinematicConstraint *con = (bKinematicConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_TARGET: {
		Object *obj = (( BPy_Object * )value)->object;
		if( !BPy_Object_Check( value ) )
			return EXPP_ReturnIntError( PyExc_TypeError, 
					"expected BPy object argument" );
		con->tar = obj;
		return 0;
		}
	case EXPP_CONSTR_BONE: {
		char *name = PyString_AsString( value );
		if( !name )
			return EXPP_ReturnIntError( PyExc_TypeError,
					"expected string arg" );

		BLI_strncpy( con->subtarget, name, sizeof( con->subtarget ) );

		return 0;
		}
	case EXPP_CONSTR_STRETCH:
		return EXPP_setBitfield( value, &con->flag, CONSTRAINT_IK_STRETCH, 'h' );
	case EXPP_CONSTR_ITERATIONS:
		return EXPP_setIValueClamped( value, &con->iterations, 1, 10000, 'h' );
	case EXPP_CONSTR_CHAINLEN:
		return EXPP_setIValueClamped( value, &con->rootbone, 0, 255, 'i' );
	case EXPP_CONSTR_POSWEIGHT:
		return EXPP_setFloatClamped( value, &con->weight, 0.01f, 1.0 );
	case EXPP_CONSTR_ROTWEIGHT:
		return EXPP_setFloatClamped( value, &con->orientweight, 0.01f, 1.0 );
	case EXPP_CONSTR_ROTATE:
		return EXPP_setBitfield( value, &con->flag, CONSTRAINT_IK_ROT, 'h' );
	case EXPP_CONSTR_USETIP:
		return EXPP_setBitfield( value, &con->flag, CONSTRAINT_IK_TIP, 'h' );
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}

static PyObject *action_getter( BPy_Constraint * self, int type )
{
	bActionConstraint *con = (bActionConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_TARGET:
		return Object_CreatePyObject( con->tar );
	case EXPP_CONSTR_BONE:
		return PyString_FromString( con->subtarget );
	case EXPP_CONSTR_ACTION:
		return Action_CreatePyObject( con->act );
	case EXPP_CONSTR_START:
		return PyInt_FromLong( (long)con->start );
	case EXPP_CONSTR_END:
		return PyInt_FromLong( (long)con->end );
	case EXPP_CONSTR_MIN:
		return PyFloat_FromDouble( (double)con->min );
	case EXPP_CONSTR_MAX:
		return PyFloat_FromDouble( (double)con->max );
	case EXPP_CONSTR_KEYON:
		return PyInt_FromLong( (long)con->type );
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
	}
}

static int action_setter( BPy_Constraint *self, int type, PyObject *value )
{
	bActionConstraint *con = (bActionConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_TARGET: {
		Object *obj = (( BPy_Object * )value)->object;
		if( !BPy_Object_Check( value ) )
			return EXPP_ReturnIntError( PyExc_TypeError, 
					"expected BPy object argument" );
		con->tar = obj;
		return 0;
		}
	case EXPP_CONSTR_BONE: {
		char *name = PyString_AsString( value );
		if( !name )
			return EXPP_ReturnIntError( PyExc_TypeError,
					"expected string arg" );

		BLI_strncpy( con->subtarget, name, sizeof( con->subtarget ) );

		return 0;
		}
	case EXPP_CONSTR_ACTION: {
		bAction *act = (( BPy_Action * )value)->action;
		if( !BPy_Action_Check( value ) )
			return EXPP_ReturnIntError( PyExc_TypeError, 
					"expected BPy action argument" );
		con->act = act;
		return 0;
		}
	case EXPP_CONSTR_START:
		return EXPP_setIValueClamped( value, &con->start, 1, MAXFRAME, 'h' );
	case EXPP_CONSTR_END:
		return EXPP_setIValueClamped( value, &con->end, 1, MAXFRAME, 'h' );
	case EXPP_CONSTR_MIN:
		if (con->type < 10)
			return EXPP_setFloatClamped( value, &con->min, -180.0, 180.0 );
		else if (con->type < 20)
			return EXPP_setFloatClamped( value, &con->min, 0.0001, 1000.0 );
		else 
			return EXPP_setFloatClamped( value, &con->min, -1000.0, 1000.0 );
	case EXPP_CONSTR_MAX:
		if (con->type < 10)
			return EXPP_setFloatClamped( value, &con->max, -180.0, 180.0 );
		else if (con->type < 20)
			return EXPP_setFloatClamped( value, &con->max, 0.0001, 1000.0 );
		else 
			return EXPP_setFloatClamped( value, &con->max, -1000.0, 1000.0 );
	case EXPP_CONSTR_KEYON:
		return EXPP_setIValueRange( value, &con->type,
				EXPP_CONSTR_XROT, EXPP_CONSTR_ZLOC, 'h' );
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}

static PyObject *trackto_getter( BPy_Constraint * self, int type )
{
	bTrackToConstraint *con = (bTrackToConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_TARGET:
		return Object_CreatePyObject( con->tar );
	case EXPP_CONSTR_BONE:
		return PyString_FromString( con->subtarget );
	case EXPP_CONSTR_TRACK:
		return PyInt_FromLong( (long)con->reserved1 );
	case EXPP_CONSTR_UP:
		return PyInt_FromLong( (long)con->reserved2 );
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
	}
}

static int trackto_setter( BPy_Constraint *self, int type, PyObject *value )
{
	bTrackToConstraint *con = (bTrackToConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_TARGET: {
		Object *obj = (( BPy_Object * )value)->object;
		if( !BPy_Object_Check( value ) )
			return EXPP_ReturnIntError( PyExc_TypeError, 
					"expected BPy object argument" );
		con->tar = obj;
		return 0;
		}
	case EXPP_CONSTR_BONE: {
		char *name = PyString_AsString( value );
		if( !name )
			return EXPP_ReturnIntError( PyExc_TypeError,
					"expected string arg" );

		BLI_strncpy( con->subtarget, name, sizeof( con->subtarget ) );

		return 0;
		}
	case EXPP_CONSTR_TRACK:
		return EXPP_setIValueRange( value, &con->reserved1,
				TRACK_X, TRACK_nZ, 'i' );
	case EXPP_CONSTR_UP:
		return EXPP_setIValueRange( value, &con->reserved2,
				UP_X, UP_Z, 'i' );
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}

static PyObject *stretchto_getter( BPy_Constraint * self, int type )
{
	bStretchToConstraint *con = (bStretchToConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_TARGET:
		return Object_CreatePyObject( con->tar );
	case EXPP_CONSTR_BONE:
		return PyString_FromString( con->subtarget );
	case EXPP_CONSTR_RESTLENGTH:
		return PyFloat_FromDouble( (double)con->orglength );
	case EXPP_CONSTR_VOLVARIATION:
		return PyFloat_FromDouble( (double)con->bulge );
	case EXPP_CONSTR_VOLUMEMODE:
		return PyInt_FromLong( (long)con->volmode );
	case EXPP_CONSTR_PLANE:
		return PyInt_FromLong( (long)con->plane );
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
	}
}

static int stretchto_setter( BPy_Constraint *self, int type, PyObject *value )
{
	bStretchToConstraint *con = (bStretchToConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_TARGET: {
		Object *obj = (( BPy_Object * )value)->object;
		if( !BPy_Object_Check( value ) )
			return EXPP_ReturnIntError( PyExc_TypeError, 
					"expected BPy object argument" );
		con->tar = obj;
		return 0;
		}
	case EXPP_CONSTR_BONE: {
		char *name = PyString_AsString( value );
		if( !name )
			return EXPP_ReturnIntError( PyExc_TypeError,
					"expected string arg" );

		BLI_strncpy( con->subtarget, name, sizeof( con->subtarget ) );

		return 0;
		}
	case EXPP_CONSTR_RESTLENGTH:
		return EXPP_setFloatClamped( value, &con->orglength, 0.0, 100.0 );
	case EXPP_CONSTR_VOLVARIATION:
		return EXPP_setFloatClamped( value, &con->bulge, 0.0, 100.0 );
	case EXPP_CONSTR_VOLUMEMODE:
		return EXPP_setIValueRange( value, &con->volmode,
				VOLUME_XZ, NO_VOLUME, 'h' );
	case EXPP_CONSTR_PLANE: {
		int status, oldcode = con->plane;
		status = EXPP_setIValueRange( value, &con->plane,
				PLANE_X, PLANE_Z, 'h' );
		if( !status && con->plane == PLANE_Y ) {
			con->plane = oldcode;
			return EXPP_ReturnIntError( PyExc_ValueError,
					"value must be either PLANEX or PLANEZ" );
		}
		return status;
		}
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}

static PyObject *distlimit_getter( BPy_Constraint * self, int type )
{
	bDistLimitConstraint *con = (bDistLimitConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_TARGET:
		return Object_CreatePyObject( con->tar );
	case EXPP_CONSTR_BONE:
		return PyString_FromString( con->subtarget );
	case EXPP_CONSTR_RESTLENGTH:
		return PyFloat_FromDouble( (double)con->dist );
	case EXPP_CONSTR_MODE:
		return PyInt_FromLong( (long)con->mode );
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
	}
}

static int distlimit_setter( BPy_Constraint *self, int type, PyObject *value )
{
	bDistLimitConstraint *con = (bDistLimitConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_TARGET: {
		Object *obj = (( BPy_Object * )value)->object;
		if( !BPy_Object_Check( value ) )
			return EXPP_ReturnIntError( PyExc_TypeError, 
					"expected BPy object argument" );
		con->tar = obj;
		return 0;
		}
	case EXPP_CONSTR_BONE: {
		char *name = PyString_AsString( value );
		if( !name )
			return EXPP_ReturnIntError( PyExc_TypeError,
					"expected string arg" );

		BLI_strncpy( con->subtarget, name, sizeof( con->subtarget ) );

		return 0;
		}
	case EXPP_CONSTR_RESTLENGTH:
		return EXPP_setFloatClamped( value, &con->dist, 0.0, 100.0 );
	case EXPP_CONSTR_MODE:
		return EXPP_setIValueRange( value, &con->mode, LIMITDIST_INSIDE, LIMITDIST_ONSURFACE, 'i' );
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}

static PyObject *followpath_getter( BPy_Constraint * self, int type )
{
	bFollowPathConstraint *con = (bFollowPathConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_TARGET:
		return Object_CreatePyObject( con->tar );
	case EXPP_CONSTR_FOLLOW:
		return PyBool_FromLong( (long)( con->followflag & SELECT ) );
	case EXPP_CONSTR_OFFSET:
		return PyFloat_FromDouble( (double)con->offset );
	case EXPP_CONSTR_FORWARD:
		return PyInt_FromLong( (long)con->trackflag );
	case EXPP_CONSTR_UP:
		return PyInt_FromLong( (long)con->upflag );
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
	}
}

static int followpath_setter( BPy_Constraint *self, int type, PyObject *value )
{
	bFollowPathConstraint *con = (bFollowPathConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_TARGET: {
		Object *obj = (( BPy_Object * )value)->object;
		if( !BPy_Object_Check( value ) )
			return EXPP_ReturnIntError( PyExc_TypeError, 
					"expected BPy object argument" );
		con->tar = obj;
		return 0;
		}
	case EXPP_CONSTR_FOLLOW:
		return EXPP_setBitfield( value, &con->followflag, SELECT, 'i' );
	case EXPP_CONSTR_OFFSET:
		return EXPP_setFloatClamped( value, &con->offset,
				-MAXFRAMEF, MAXFRAMEF );
	case EXPP_CONSTR_FORWARD:
		return EXPP_setIValueRange( value, &con->trackflag,
				TRACK_X, TRACK_nZ, 'i' );
	case EXPP_CONSTR_UP:
		return EXPP_setIValueRange( value, &con->upflag,
				UP_X, UP_Z, 'i' );
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}

static PyObject *clampto_getter( BPy_Constraint * self, int type )
{
	bClampToConstraint *con = (bClampToConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_TARGET:
		return Object_CreatePyObject( con->tar );
	case EXPP_CONSTR_CLAMP:
		return PyInt_FromLong( (long)con->flag );
	case EXPP_CONSTR_CLAMPCYCLIC:
		return PyBool_FromLong( (long)(con->flag2 & CLAMPTO_CYCLIC) );
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
	}
}

static int clampto_setter( BPy_Constraint *self, int type, PyObject *value )
{
	bClampToConstraint *con = (bClampToConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_TARGET: {
		Object *obj = (( BPy_Object * )value)->object;
		if( !BPy_Object_Check( value ) )
			return EXPP_ReturnIntError( PyExc_TypeError, 
					"expected BPy object argument" );
		con->tar = obj;
		return 0;
		}
	case EXPP_CONSTR_CLAMP:
		return EXPP_setIValueRange( value, &con->flag,
				CLAMPTO_AUTO, CLAMPTO_Z, 'i' );
	case EXPP_CONSTR_CLAMPCYCLIC:
		return EXPP_setBitfield( value, &con->flag2, CLAMPTO_CYCLIC, 'i' );
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}
static PyObject *locktrack_getter( BPy_Constraint * self, int type )
{
	bLockTrackConstraint *con = (bLockTrackConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_TARGET:
		return Object_CreatePyObject( con->tar );
	case EXPP_CONSTR_BONE:
		return PyString_FromString( con->subtarget );
	case EXPP_CONSTR_TRACK:
		return PyInt_FromLong( (long)con->trackflag );
	case EXPP_CONSTR_LOCK:
		return PyInt_FromLong( (long)con->lockflag );
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
	}
}

static int locktrack_setter( BPy_Constraint *self, int type, PyObject *value )
{
	bLockTrackConstraint *con = (bLockTrackConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_TARGET: {
		Object *obj = (( BPy_Object * )value)->object;
		if( !BPy_Object_Check( value ) )
			return EXPP_ReturnIntError( PyExc_TypeError, 
					"expected BPy object argument" );
		con->tar = obj;
		return 0;
		}
	case EXPP_CONSTR_BONE: {
		char *name = PyString_AsString( value );
		if( !name )
			return EXPP_ReturnIntError( PyExc_TypeError,
					"expected string arg" );

		BLI_strncpy( con->subtarget, name, sizeof( con->subtarget ) );

		return 0;
		}
	case EXPP_CONSTR_TRACK:
		return EXPP_setIValueRange( value, &con->trackflag,
				TRACK_X, TRACK_nZ, 'i' );
	case EXPP_CONSTR_LOCK:
		return EXPP_setIValueRange( value, &con->lockflag,
				LOCK_X, LOCK_Z, 'i' );
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}

static PyObject *floor_getter( BPy_Constraint * self, int type )
{
	bMinMaxConstraint *con = (bMinMaxConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_TARGET:
		return Object_CreatePyObject( con->tar );
	case EXPP_CONSTR_BONE:
		return PyString_FromString( con->subtarget );
	case EXPP_CONSTR_MINMAX:
		return PyInt_FromLong( (long)con->minmaxflag );
	case EXPP_CONSTR_OFFSET:
		return PyFloat_FromDouble( (double)con->offset );
	case EXPP_CONSTR_STICKY:
		return PyBool_FromLong( (long)( con->flag & MINMAX_STICKY ) ) ;
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
	}
}

static int floor_setter( BPy_Constraint *self, int type, PyObject *value )
{
	bMinMaxConstraint *con = (bMinMaxConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_TARGET: {
		Object *obj = (( BPy_Object * )value)->object;
		if( !BPy_Object_Check( value ) )
			return EXPP_ReturnIntError( PyExc_TypeError, 
					"expected BPy object argument" );
		con->tar = obj;
		return 0;
		}
	case EXPP_CONSTR_BONE: {
		char *name = PyString_AsString( value );
		if( !name )
			return EXPP_ReturnIntError( PyExc_TypeError,
					"expected string arg" );

		BLI_strncpy( con->subtarget, name, sizeof( con->subtarget ) );

		return 0;
		}
	case EXPP_CONSTR_MINMAX:
		return EXPP_setIValueRange( value, &con->minmaxflag,
				EXPP_CONSTR_MAXX, EXPP_CONSTR_MINZ, 'i' );
	case EXPP_CONSTR_OFFSET:
		return EXPP_setFloatClamped( value, &con->offset, -100.0, 100.0 );
	case EXPP_CONSTR_STICKY:
		return EXPP_setBitfield( value, &con->flag, MINMAX_STICKY, 'h' );
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}

static PyObject *locatelike_getter( BPy_Constraint * self, int type )
{
	bLocateLikeConstraint *con = (bLocateLikeConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_TARGET:
		return Object_CreatePyObject( con->tar );
	case EXPP_CONSTR_BONE:
		return PyString_FromString( con->subtarget );
	case EXPP_CONSTR_COPY:
		return PyInt_FromLong( (long)con->flag );
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
	}
}

static int locatelike_setter( BPy_Constraint *self, int type, PyObject *value )
{
	bLocateLikeConstraint *con = (bLocateLikeConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_TARGET: {
		Object *obj = (( BPy_Object * )value)->object;
		if( !BPy_Object_Check( value ) )
			return EXPP_ReturnIntError( PyExc_TypeError, 
					"expected BPy object argument" );
		con->tar = obj;
		return 0;
		}
	case EXPP_CONSTR_BONE: {
		char *name = PyString_AsString( value );
		if( !name )
			return EXPP_ReturnIntError( PyExc_TypeError,
					"expected string arg" );

		BLI_strncpy( con->subtarget, name, sizeof( con->subtarget ) );

		return 0;
		}
	case EXPP_CONSTR_COPY:
		return EXPP_setIValueRange( value, &con->flag,
				0, LOCLIKE_X | LOCLIKE_Y | LOCLIKE_Z | LOCLIKE_X_INVERT | LOCLIKE_Y_INVERT | LOCLIKE_Z_INVERT, 'i' );
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}

static PyObject *rotatelike_getter( BPy_Constraint * self, int type )
{
	bRotateLikeConstraint *con = (bRotateLikeConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_TARGET:
		return Object_CreatePyObject( con->tar );
	case EXPP_CONSTR_BONE:
		return PyString_FromString( con->subtarget );
	case EXPP_CONSTR_COPY:
		return PyInt_FromLong( (long)con->flag );
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
	}
}

static int rotatelike_setter( BPy_Constraint *self, int type, PyObject *value )
{
	bRotateLikeConstraint *con = (bRotateLikeConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_TARGET: {
		Object *obj = (( BPy_Object * )value)->object;
		if( !BPy_Object_Check( value ) )
			return EXPP_ReturnIntError( PyExc_TypeError, 
					"expected BPy object argument" );
		con->tar = obj;
		return 0;
		}
	case EXPP_CONSTR_BONE: {
		char *name = PyString_AsString( value );
		if( !name )
			return EXPP_ReturnIntError( PyExc_TypeError,
					"expected string arg" );

		BLI_strncpy( con->subtarget, name, sizeof( con->subtarget ) );

		return 0;
		}
	case EXPP_CONSTR_COPY:
		return EXPP_setIValueRange( value, &con->flag,
				0, ROTLIKE_X | ROTLIKE_Y | ROTLIKE_Z | ROTLIKE_X_INVERT | ROTLIKE_Y_INVERT | ROTLIKE_Z_INVERT, 'i' );
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}

static PyObject *sizelike_getter( BPy_Constraint * self, int type )
{
	bSizeLikeConstraint *con = (bSizeLikeConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_TARGET:
		return Object_CreatePyObject( con->tar );
	case EXPP_CONSTR_BONE:
		return PyString_FromString( con->subtarget );
	case EXPP_CONSTR_COPY:
		return PyInt_FromLong( (long)con->flag );
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
	}
}

static int sizelike_setter( BPy_Constraint *self, int type, PyObject *value )
{
	bSizeLikeConstraint *con = (bSizeLikeConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_TARGET: {
		Object *obj = (( BPy_Object * )value)->object;
		if( !BPy_Object_Check( value ) )
			return EXPP_ReturnIntError( PyExc_TypeError, 
					"expected BPy object argument" );
		con->tar = obj;
		return 0;
		}
	case EXPP_CONSTR_BONE: {
		char *name = PyString_AsString( value );
		if( !name )
			return EXPP_ReturnIntError( PyExc_TypeError,
					"expected string arg" );

		BLI_strncpy( con->subtarget, name, sizeof( con->subtarget ) );

		return 0;
		}
	case EXPP_CONSTR_COPY:
		return EXPP_setIValueRange( value, &con->flag,
				0, SIZELIKE_X | SIZELIKE_Y | SIZELIKE_Z, 'i' );
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}

static PyObject *loclimit_getter( BPy_Constraint * self, int type)
{
	bLocLimitConstraint *con = (bLocLimitConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_LIMIT:
		return PyInt_FromLong( (long)con->flag );
	case EXPP_CONSTR_XMIN:
		return PyFloat_FromDouble( (double)con->xmin );
	case EXPP_CONSTR_XMAX:
		return PyFloat_FromDouble( (double)con->xmax );
	case EXPP_CONSTR_YMIN:
		return PyFloat_FromDouble( (double)con->ymin );
	case EXPP_CONSTR_YMAX:
		return PyFloat_FromDouble( (double)con->ymax );
	case EXPP_CONSTR_ZMIN:
		return PyFloat_FromDouble( (double)con->zmin );
	case EXPP_CONSTR_ZMAX:
		return PyFloat_FromDouble( (double)con->zmax );
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );	
	}
}

static int loclimit_setter( BPy_Constraint *self, int type, PyObject *value )
{
	bLocLimitConstraint *con = (bLocLimitConstraint *)(self->con->data);	
	
	switch( type ) {
	case EXPP_CONSTR_LIMIT:
		return EXPP_setIValueRange( value, &con->flag, 0, 
			LIMIT_XMIN | LIMIT_XMAX | LIMIT_YMIN | LIMIT_YMAX | LIMIT_ZMIN | LIMIT_ZMAX , 'i' );
	case EXPP_CONSTR_XMIN:
		return EXPP_setFloatClamped( value, &con->xmin, -1000.0, 1000.0 );
	case EXPP_CONSTR_XMAX:
		return EXPP_setFloatClamped( value, &con->xmax, -1000.0, 1000.0 );
	case EXPP_CONSTR_YMIN:
		return EXPP_setFloatClamped( value, &con->ymin, -1000.0, 1000.0 );
	case EXPP_CONSTR_YMAX:
		return EXPP_setFloatClamped( value, &con->ymax, -1000.0, 1000.0 );
	case EXPP_CONSTR_ZMIN:
		return EXPP_setFloatClamped( value, &con->zmin, -1000.0, 1000.0 );
	case EXPP_CONSTR_ZMAX:
		return EXPP_setFloatClamped( value, &con->zmax, -1000.0, 1000.0 );
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}

static PyObject *rotlimit_getter( BPy_Constraint * self, int type )
{
	bRotLimitConstraint *con = (bRotLimitConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_LIMIT:
		return PyInt_FromLong( (long)con->flag );
	case EXPP_CONSTR_XMIN:
		return PyFloat_FromDouble( (double)con->xmin );
	case EXPP_CONSTR_XMAX:
		return PyFloat_FromDouble( (double)con->xmax );
	case EXPP_CONSTR_YMIN:
		return PyFloat_FromDouble( (double)con->ymin );
	case EXPP_CONSTR_YMAX:
		return PyFloat_FromDouble( (double)con->ymax );
	case EXPP_CONSTR_ZMIN:
		return PyFloat_FromDouble( (double)con->zmin );
	case EXPP_CONSTR_ZMAX:
		return PyFloat_FromDouble( (double)con->zmax );
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );	
	}
}

static int rotlimit_setter( BPy_Constraint *self, int type, PyObject *value )
{
	bRotLimitConstraint *con = (bRotLimitConstraint *)(self->con->data);	
	
	switch( type ) {
	case EXPP_CONSTR_LIMIT:
		return EXPP_setIValueRange( value, &con->flag, 0, 
			LIMIT_XROT | LIMIT_YROT | LIMIT_ZROT, 'i' );
	case EXPP_CONSTR_XMIN:
		return EXPP_setFloatClamped( value, &con->xmin, -360.0, 360.0 );
	case EXPP_CONSTR_XMAX:
		return EXPP_setFloatClamped( value, &con->xmax, -360.0, 360.0 );
	case EXPP_CONSTR_YMIN:
		return EXPP_setFloatClamped( value, &con->ymin, -360.0, 360.0 );
	case EXPP_CONSTR_YMAX:
		return EXPP_setFloatClamped( value, &con->ymax, -360.0, 360.0 );
	case EXPP_CONSTR_ZMIN:
		return EXPP_setFloatClamped( value, &con->zmin, -360.0, 360.0 );
	case EXPP_CONSTR_ZMAX:
		return EXPP_setFloatClamped( value, &con->zmax, -360.0, 360.0 );
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}

static PyObject *sizelimit_getter( BPy_Constraint * self, int type)
{
	bSizeLimitConstraint *con = (bSizeLimitConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_LIMIT:
		return PyInt_FromLong( (long)con->flag );
	case EXPP_CONSTR_XMIN:
		return PyFloat_FromDouble( (double)con->xmin );
	case EXPP_CONSTR_XMAX:
		return PyFloat_FromDouble( (double)con->xmax );
	case EXPP_CONSTR_YMIN:
		return PyFloat_FromDouble( (double)con->ymin );
	case EXPP_CONSTR_YMAX:
		return PyFloat_FromDouble( (double)con->ymax );
	case EXPP_CONSTR_ZMIN:
		return PyFloat_FromDouble( (double)con->zmin );
	case EXPP_CONSTR_ZMAX:
		return PyFloat_FromDouble( (double)con->zmax );
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );	
	}
}

static int sizelimit_setter( BPy_Constraint *self, int type, PyObject *value )
{
	bSizeLimitConstraint *con = (bSizeLimitConstraint *)(self->con->data);	
	
	switch( type ) {
	case EXPP_CONSTR_LIMIT:
		return EXPP_setIValueRange( value, &con->flag, 0, 
			LIMIT_XMIN | LIMIT_XMAX | LIMIT_YMIN | LIMIT_YMAX | LIMIT_ZMIN | LIMIT_ZMAX, 'i' );
	case EXPP_CONSTR_XMIN:
		return EXPP_setFloatClamped( value, &con->xmin, -1000.0, 1000.0 );
	case EXPP_CONSTR_XMAX:
		return EXPP_setFloatClamped( value, &con->xmax, -1000.0, 1000.0 );
	case EXPP_CONSTR_YMIN:
		return EXPP_setFloatClamped( value, &con->ymin, -1000.0, 1000.0 );
	case EXPP_CONSTR_YMAX:
		return EXPP_setFloatClamped( value, &con->ymax, -1000.0, 1000.0 );
	case EXPP_CONSTR_ZMIN:
		return EXPP_setFloatClamped( value, &con->zmin, -1000.0, 1000.0 );
	case EXPP_CONSTR_ZMAX:
		return EXPP_setFloatClamped( value, &con->zmax, -1000.0, 1000.0 );
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}

static PyObject *script_getter( BPy_Constraint * self, int type )
{
	bPythonConstraint *con = (bPythonConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_TARGET:
	case EXPP_CONSTR_BONE:
		{
			bConstraintTypeInfo *cti= get_constraint_typeinfo(CONSTRAINT_TYPE_PYTHON);
			bConstraintTarget *ct;
			PyObject *tlist=NULL, *val;
			
			if (cti) {
				/* change space of targets */
				if (cti->get_constraint_targets) {
					ListBase targets = {NULL, NULL};
					int num_tars=0, i=0;
					
					/* get targets, and create py-list for use temporarily */
					num_tars= cti->get_constraint_targets(self->con, &targets);
					if (num_tars) {
						tlist= PyList_New(num_tars);
						
						for (ct=targets.first; ct; ct=ct->next, i++) {
							if (type == EXPP_CONSTR_BONE)
								val= PyString_FromString(ct->subtarget);
							else
								val= Object_CreatePyObject(ct->tar);
							PyList_SET_ITEM(tlist, i, val);
						}
					}
					
					if (cti->flush_constraint_targets)
						cti->flush_constraint_targets(self->con, &targets, 1);
				}
			}
			
			return tlist;
		}
	case EXPP_CONSTR_SCRIPT:
		return Text_CreatePyObject( con->text );
	case EXPP_CONSTR_PROPS:
		return BPy_Wrap_IDProperty( NULL, con->prop, NULL);
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
	}
}

static int script_setter( BPy_Constraint *self, int type, PyObject *value )
{
	bPythonConstraint *con = (bPythonConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_TARGET:
	case EXPP_CONSTR_BONE:
		{
			bConstraintTypeInfo *cti= get_constraint_typeinfo(CONSTRAINT_TYPE_PYTHON);
			bConstraintTarget *ct;
			int ok= 0;
			
			if (cti) {
				/* change space of targets */
				if (cti->get_constraint_targets) {
					ListBase targets = {NULL, NULL};
					int num_tars=0, i=0;
					
					/* get targets, and extract values from the given list */
					num_tars= cti->get_constraint_targets(self->con, &targets);
					if (num_tars) {
						if ((PySequence_Check(value) == 0) || (PySequence_Size(value) != num_tars)) {
							char errorstr[64];
							sprintf(errorstr, "expected sequence of %d integer(s)", num_tars);
							return EXPP_ReturnIntError(PyExc_TypeError, errorstr);
						}
						
						for (ct=targets.first; ct; ct=ct->next, i++) {
							PyObject *val= PySequence_ITEM(value, i);
								
							if (type == EXPP_CONSTR_BONE) {
								char *name = PyString_AsString(val);
								
								if (name == NULL) {
									// hrm... should we break here instead?
									ok = EXPP_ReturnIntError(PyExc_TypeError, 
												"expected string arg as member of list");
									Py_DECREF(val);
									
									break;
								}
								
								BLI_strncpy(ct->subtarget, name, sizeof(ct->subtarget));
							}
							else {
								Object *obj = (( BPy_Object * )val)->object;
								
								if ( !BPy_Object_Check(value) ) {
									// hrm... should we break here instead?
									ok = EXPP_ReturnIntError(PyExc_TypeError, 
												"expected BPy object argument as member of list");
									Py_DECREF(val);
									
									break;
								}
								
								ct->tar = obj;
							}
							
							Py_DECREF(val);
						}
					}
					
					/* only flush changes to real constraints if all were successful */
					if ((cti->flush_constraint_targets) && (ok == 0))
						cti->flush_constraint_targets(self->con, &targets, 0);
				}
			}
			
			return ok;
		}
			break;
	case EXPP_CONSTR_SCRIPT: {
		Text *text = (( BPy_Text * )value)->text;
		if( !BPy_Object_Check( value ) )
			return EXPP_ReturnIntError( PyExc_TypeError, 
					"expected BPy text argument" );
		con->text = text;
		return 0;
		}
	case EXPP_CONSTR_PROPS:
		return EXPP_ReturnIntError( PyExc_RuntimeError,
					"setting ID-Properties of PyConstraints this way is not supported" );
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}


static PyObject *rigidbody_getter( BPy_Constraint * self, int type)
{
	bRigidBodyJointConstraint *con = (bRigidBodyJointConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_TARGET:
		return Object_CreatePyObject( con->tar );
	case EXPP_CONSTR_RB_PIVX:
		return PyFloat_FromDouble( (double)con->pivX );
	case EXPP_CONSTR_RB_PIVY:
		return PyFloat_FromDouble( (double)con->pivY );
	case EXPP_CONSTR_RB_PIVZ:
		return PyFloat_FromDouble( (double)con->pivZ );
	case EXPP_CONSTR_RB_AXX:
		return PyFloat_FromDouble( (double)con->axX );
	case EXPP_CONSTR_RB_AXY:
		return PyFloat_FromDouble( (double)con->axY );
	case EXPP_CONSTR_RB_AXZ:
		return PyFloat_FromDouble( (double)con->axZ );
	case EXPP_CONSTR_RB_MINLIMIT0:
		return PyFloat_FromDouble( (double)con->minLimit[0] );
	case EXPP_CONSTR_RB_MINLIMIT1:
		return PyFloat_FromDouble( (double)con->minLimit[1] );
	case EXPP_CONSTR_RB_MINLIMIT2:
		return PyFloat_FromDouble( (double)con->minLimit[2] );
	case EXPP_CONSTR_RB_MINLIMIT3:
		return PyFloat_FromDouble( (double)con->minLimit[3] );
	case EXPP_CONSTR_RB_MINLIMIT4:
		return PyFloat_FromDouble( (double)con->minLimit[4] );
	case EXPP_CONSTR_RB_MINLIMIT5:
		return PyFloat_FromDouble( (double)con->minLimit[5] );
	case EXPP_CONSTR_RB_MAXLIMIT0:
		return PyFloat_FromDouble( (double)con->maxLimit[0] );
	case EXPP_CONSTR_RB_MAXLIMIT1:
		return PyFloat_FromDouble( (double)con->maxLimit[1] );
	case EXPP_CONSTR_RB_MAXLIMIT2:
		return PyFloat_FromDouble( (double)con->maxLimit[2] );
	case EXPP_CONSTR_RB_MAXLIMIT3:
		return PyFloat_FromDouble( (double)con->maxLimit[3] );
	case EXPP_CONSTR_RB_MAXLIMIT4:
		return PyFloat_FromDouble( (double)con->maxLimit[4] );
	case EXPP_CONSTR_RB_MAXLIMIT5:
		return PyFloat_FromDouble( (double)con->maxLimit[5] );
	case EXPP_CONSTR_RB_EXTRAFZ:
		return PyFloat_FromDouble( (double)con->extraFz );		
	case EXPP_CONSTR_LIMIT:
		return PyInt_FromLong( (int)con->flag );
		
	case EXPP_CONSTR_RB_TYPE:
		return PyInt_FromLong( (int)con->type );
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );	
	}
}


static int rigidbody_setter( BPy_Constraint *self, int type, PyObject *value )
{
	bRigidBodyJointConstraint *con = (bRigidBodyJointConstraint *)(self->con->data);	
	
	switch( type ) {
	case EXPP_CONSTR_TARGET: {
		Object *obj = (( BPy_Object * )value)->object;
		if( !BPy_Object_Check( value ) )
			return EXPP_ReturnIntError( PyExc_TypeError, 
					"expected BPy object argument" );
		con->tar = obj;
		return 0;
		}
	case EXPP_CONSTR_RB_PIVX:
		return EXPP_setFloatClamped( value, &con->pivX , -1000.0, 1000.0 );
	case EXPP_CONSTR_RB_PIVY:
		return EXPP_setFloatClamped( value, &con->pivY , -1000.0, 1000.0 );
	case EXPP_CONSTR_RB_PIVZ:
		return EXPP_setFloatClamped( value, &con->pivZ , -1000.0, 1000.0 );
	case EXPP_CONSTR_RB_AXX:
		return EXPP_setFloatClamped( value, &con->axX , -1000.0, 1000.0 );
	case EXPP_CONSTR_RB_AXY:
		return EXPP_setFloatClamped( value, &con->axY , -1000.0, 1000.0 );
	case EXPP_CONSTR_RB_AXZ:
		return EXPP_setFloatClamped( value, &con->axZ , -1000.0, 1000.0 );
	case EXPP_CONSTR_RB_MINLIMIT0:
		return EXPP_setFloatClamped( value, &con->minLimit[0] , -1000.0, 1000.0 );
	case EXPP_CONSTR_RB_MINLIMIT1:
		return EXPP_setFloatClamped( value, &con->minLimit[1] , -1000.0, 1000.0 );
	case EXPP_CONSTR_RB_MINLIMIT2:
		return EXPP_setFloatClamped( value, &con->minLimit[2] , -1000.0, 1000.0 );
	case EXPP_CONSTR_RB_MINLIMIT3:
		return EXPP_setFloatClamped( value, &con->minLimit[3] , -1000.0, 1000.0 );
	case EXPP_CONSTR_RB_MINLIMIT4:
		return EXPP_setFloatClamped( value, &con->minLimit[4] , -1000.0, 1000.0 );
	case EXPP_CONSTR_RB_MINLIMIT5:
		return EXPP_setFloatClamped( value, &con->minLimit[5] , -1000.0, 1000.0 );
	case EXPP_CONSTR_RB_MAXLIMIT0:
		return EXPP_setFloatClamped( value, &con->maxLimit[0] , -1000.0, 1000.0 );
	case EXPP_CONSTR_RB_MAXLIMIT1:
		return EXPP_setFloatClamped( value, &con->maxLimit[1] , -1000.0, 1000.0 );
	case EXPP_CONSTR_RB_MAXLIMIT2:
		return EXPP_setFloatClamped( value, &con->maxLimit[2] , -1000.0, 1000.0 );
	case EXPP_CONSTR_RB_MAXLIMIT3:
		return EXPP_setFloatClamped( value, &con->maxLimit[3] , -1000.0, 1000.0 );
	case EXPP_CONSTR_RB_MAXLIMIT4:
		return EXPP_setFloatClamped( value, &con->maxLimit[4] , -1000.0, 1000.0 );
	case EXPP_CONSTR_RB_MAXLIMIT5:
		return EXPP_setFloatClamped( value, &con->maxLimit[5] , -1000.0, 1000.0 );
	case EXPP_CONSTR_RB_EXTRAFZ:
		return EXPP_setFloatClamped( value, &con->extraFz , -1000.0, 1000.0 );						
	case EXPP_CONSTR_LIMIT:
		return EXPP_setIValueRange( value, &con->flag, 0, 
			LIMIT_XMIN | LIMIT_XMAX | LIMIT_YMIN | LIMIT_YMAX | LIMIT_ZMIN | LIMIT_ZMAX, 'i' );
	case EXPP_CONSTR_RB_TYPE:
		return EXPP_setIValueRange( value, &con->type, 0, 
			EXPP_CONSTR_RB_BALL | EXPP_CONSTR_RB_HINGE | EXPP_CONSTR_RB_GENERIC6DOF | EXPP_CONSTR_RB_VEHICLE, 'i' );
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}

static PyObject *childof_getter( BPy_Constraint * self, int type )
{
	bChildOfConstraint *con = (bChildOfConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_TARGET:
		return Object_CreatePyObject( con->tar );
	case EXPP_CONSTR_BONE:
		return PyString_FromString( con->subtarget );
	case EXPP_CONSTR_COPY:
		return PyInt_FromLong( (long)con->flag );
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
	}
}

static int childof_setter( BPy_Constraint *self, int type, PyObject *value )
{
	bChildOfConstraint *con = (bChildOfConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_TARGET: {
		Object *obj = (( BPy_Object * )value)->object;
		if( !BPy_Object_Check( value ) )
			return EXPP_ReturnIntError( PyExc_TypeError, 
					"expected BPy object argument" );
		con->tar = obj;
		return 0;
		}
	case EXPP_CONSTR_BONE: {
		char *name = PyString_AsString( value );
		if( !name )
			return EXPP_ReturnIntError( PyExc_TypeError,
					"expected string arg" );

		BLI_strncpy( con->subtarget, name, sizeof( con->subtarget ) );

		return 0;
		}
	case EXPP_CONSTR_COPY:
		return EXPP_setIValueRange( value, &con->flag,
				0, CHILDOF_LOCX| CHILDOF_LOCY | CHILDOF_LOCZ | CHILDOF_ROTX | CHILDOF_ROTY | CHILDOF_ROTZ |
					CHILDOF_SIZEX |CHILDOF_SIZEY| CHILDOF_SIZEZ, 'i' );
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}

static PyObject *transf_getter( BPy_Constraint * self, int type )
{
	bTransformConstraint *con = (bTransformConstraint *)(self->con->data);

	switch( type ) {
	case EXPP_CONSTR_TARGET:
		return Object_CreatePyObject( con->tar );
	case EXPP_CONSTR_BONE:
		return PyString_FromString( con->subtarget );
	case EXPP_CONSTR_FROM:
		return PyInt_FromLong( (long)con->from );
	case EXPP_CONSTR_TO:
		return PyInt_FromLong( (long)con->to );
	case EXPP_CONSTR_MAPX:
		return PyInt_FromLong( (long)con->map[0] );
	case EXPP_CONSTR_MAPY:
		return PyInt_FromLong( (long)con->map[1] );
	case EXPP_CONSTR_MAPZ:
		return PyInt_FromLong( (long)con->map[2] );
	case EXPP_CONSTR_FROMMINX:
		return PyFloat_FromDouble( (double)con->from_min[0] );
	case EXPP_CONSTR_FROMMAXX:
		return PyFloat_FromDouble( (double)con->from_max[0] );
	case EXPP_CONSTR_FROMMINY:
		return PyFloat_FromDouble( (double)con->from_min[1] );
	case EXPP_CONSTR_FROMMAXY:
		return PyFloat_FromDouble( (double)con->from_max[1] );
	case EXPP_CONSTR_FROMMINZ:
		return PyFloat_FromDouble( (double)con->from_min[2] );
	case EXPP_CONSTR_FROMMAXZ:
		return PyFloat_FromDouble( (double)con->from_max[2] );
	case EXPP_CONSTR_TOMINX:
		return PyFloat_FromDouble( (double)con->to_min[0] );
	case EXPP_CONSTR_TOMAXX:
		return PyFloat_FromDouble( (double)con->to_max[0] );
	case EXPP_CONSTR_TOMINY:
		return PyFloat_FromDouble( (double)con->to_min[1] );
	case EXPP_CONSTR_TOMAXY:
		return PyFloat_FromDouble( (double)con->to_max[1] );
	case EXPP_CONSTR_TOMINZ:
		return PyFloat_FromDouble( (double)con->to_min[2] );
	case EXPP_CONSTR_TOMAXZ:
		return PyFloat_FromDouble( (double)con->to_max[2] );
	case EXPP_CONSTR_EXPO:
		return PyBool_FromLong( (long)con->expo );
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
	}
}

static int transf_setter( BPy_Constraint *self, int type, PyObject *value )
{
	bTransformConstraint *con = (bTransformConstraint *)(self->con->data);
	float fmin, fmax, tmin, tmax;
	
	if (con->from == 2) {
		fmin = 0.0001;
		fmax = 1000.0;
	}
	else if (con->from == 1) {
		fmin = -360.0;
		fmax = 360.0;
	}
	else {
		fmin = -1000.0;
		fmax = 1000.0;
	}
	
	if (con->to == 2) {
		tmin = 0.0001;
		tmax = 1000.0;
	}
	else if (con->to == 1) {
		tmin = -360.0;
		tmax = 360.0;
	}
	else {
		tmin = -1000.0;
		tmax = 1000.0;
	}
	
	switch( type ) {
	case EXPP_CONSTR_TARGET: {
		Object *obj = (( BPy_Object * )value)->object;
		if( !BPy_Object_Check( value ) )
			return EXPP_ReturnIntError( PyExc_TypeError, 
					"expected BPy object argument" );
		con->tar = obj;
		return 0;
		}
	case EXPP_CONSTR_BONE: {
		char *name = PyString_AsString( value );
		if( !name )
			return EXPP_ReturnIntError( PyExc_TypeError,
					"expected string arg" );

		BLI_strncpy( con->subtarget, name, sizeof( con->subtarget ) );

		return 0;
		}
	case EXPP_CONSTR_FROM:
		return EXPP_setIValueClamped( value, &con->from, 0, 3, 'h' );
	case EXPP_CONSTR_TO:
		return EXPP_setIValueClamped( value, &con->to, 0, 3, 'h' );
	case EXPP_CONSTR_MAPX:
		return EXPP_setIValueClamped( value, &con->map[0], 0, 3, 'h' );
	case EXPP_CONSTR_MAPY:
		return EXPP_setIValueClamped( value, &con->map[1], 0, 3, 'h' );
	case EXPP_CONSTR_MAPZ:
		return EXPP_setIValueClamped( value, &con->map[2], 0, 3, 'h' );
	case EXPP_CONSTR_FROMMINX:
		return EXPP_setFloatClamped( value, &con->from_min[0], fmin, fmax );
	case EXPP_CONSTR_FROMMAXX:
		return EXPP_setFloatClamped( value, &con->from_max[0], fmin, fmax );
	case EXPP_CONSTR_FROMMINY:
		return EXPP_setFloatClamped( value, &con->from_min[1], fmin, fmax );
	case EXPP_CONSTR_FROMMAXY:
		return EXPP_setFloatClamped( value, &con->from_max[1], fmin, fmax );
	case EXPP_CONSTR_FROMMINZ:
		return EXPP_setFloatClamped( value, &con->from_min[2], fmin, fmax );
	case EXPP_CONSTR_FROMMAXZ:
		return EXPP_setFloatClamped( value, &con->from_max[2], fmin, fmax );
	case EXPP_CONSTR_TOMINX:
		return EXPP_setFloatClamped( value, &con->to_min[0], tmin, tmax );
	case EXPP_CONSTR_TOMAXX:
		return EXPP_setFloatClamped( value, &con->to_max[0], tmin, tmax );
	case EXPP_CONSTR_TOMINY:
		return EXPP_setFloatClamped( value, &con->to_min[1], tmin, tmax );
	case EXPP_CONSTR_TOMAXY:
		return EXPP_setFloatClamped( value, &con->to_max[1], tmin, tmax );
	case EXPP_CONSTR_TOMINZ:
		return EXPP_setFloatClamped( value, &con->to_min[2], tmin, tmax );
	case EXPP_CONSTR_TOMAXZ:
		return EXPP_setFloatClamped( value, &con->to_max[2], tmin, tmax );
	case EXPP_CONSTR_EXPO:
		return EXPP_setBitfield( value, &con->expo, 1, 'h' );
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}

/*
 * get data from a constraint
 */

static PyObject *Constraint_getData( BPy_Constraint * self, PyObject * key )
{
	int setting;

	if( !PyInt_Check( key ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected an int arg" );

	if( !self->con )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This constraint has been removed!" );
	
	setting = PyInt_AsLong( key );
	
	/* bypass doing settings of individual constraints, if we're just doing
	 * constraint space access-stuff 
	 */
	if ((setting==EXPP_CONSTR_OWNSPACE) || (setting==EXPP_CONSTR_TARSPACE)) {
		return constspace_getter( self, setting );
	}
	switch( self->con->type ) {
		case CONSTRAINT_TYPE_NULL:
			Py_RETURN_NONE;
		case CONSTRAINT_TYPE_TRACKTO:
			return trackto_getter( self, setting );
		case CONSTRAINT_TYPE_KINEMATIC:
			return kinematic_getter( self, setting );
		case CONSTRAINT_TYPE_FOLLOWPATH:
			return followpath_getter( self, setting );
		case CONSTRAINT_TYPE_ACTION:
			return action_getter( self, setting );
		case CONSTRAINT_TYPE_LOCKTRACK:
			return locktrack_getter( self, setting );
		case CONSTRAINT_TYPE_STRETCHTO:
			return stretchto_getter( self, setting );
		case CONSTRAINT_TYPE_MINMAX:
			return floor_getter( self, setting );
		case CONSTRAINT_TYPE_LOCLIKE:
			return locatelike_getter( self, setting );
		case CONSTRAINT_TYPE_ROTLIKE:
			return rotatelike_getter( self, setting );
		case CONSTRAINT_TYPE_SIZELIKE:
			return sizelike_getter( self, setting );
		case CONSTRAINT_TYPE_ROTLIMIT:
			return rotlimit_getter( self, setting );
		case CONSTRAINT_TYPE_LOCLIMIT:
			return loclimit_getter( self, setting );
		case CONSTRAINT_TYPE_SIZELIMIT:
			return sizelimit_getter( self, setting );
		case CONSTRAINT_TYPE_DISTLIMIT:
			return distlimit_getter( self, setting );
		case CONSTRAINT_TYPE_RIGIDBODYJOINT:
			return rigidbody_getter( self, setting );
		case CONSTRAINT_TYPE_CLAMPTO:
			return clampto_getter( self, setting );
		case CONSTRAINT_TYPE_PYTHON:
			return script_getter( self, setting );
		case CONSTRAINT_TYPE_CHILDOF:
			return childof_getter( self, setting );
		case CONSTRAINT_TYPE_TRANSFORM:
			return transf_getter( self, setting );
		default:
			return EXPP_ReturnPyObjError( PyExc_KeyError,
					"unknown constraint type" );
	}
}

static int Constraint_setData( BPy_Constraint * self, PyObject * key, 
		PyObject * arg )
{
	int key_int, result;

	if( !PyNumber_Check( key ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected an int arg" );
	if( !self->con )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"This constraint has been removed!" );
	
	key_int = PyInt_AsLong( key );
	
	/* bypass doing settings of individual constraints, if we're just doing
	 * constraint space access-stuff 
	 */
	if ((key_int==EXPP_CONSTR_OWNSPACE) || (key_int==EXPP_CONSTR_TARSPACE)) {
		result = constspace_setter( self, key_int, arg );
	}
	else {
		switch( self->con->type ) {
		case CONSTRAINT_TYPE_KINEMATIC:
			result = kinematic_setter( self, key_int, arg );
			break;
		case CONSTRAINT_TYPE_ACTION:
			result = action_setter( self, key_int, arg );
			break;
		case CONSTRAINT_TYPE_TRACKTO:
			result = trackto_setter( self, key_int, arg );
			break;
		case CONSTRAINT_TYPE_STRETCHTO:
			result = stretchto_setter( self, key_int, arg );
			break;
		case CONSTRAINT_TYPE_FOLLOWPATH:
			result = followpath_setter( self, key_int, arg );
			break;
		case CONSTRAINT_TYPE_LOCKTRACK:
			result = locktrack_setter( self, key_int, arg );
			break;
		case CONSTRAINT_TYPE_MINMAX:
			result = floor_setter( self, key_int, arg );
			break;
		case CONSTRAINT_TYPE_LOCLIKE:
			result = locatelike_setter( self, key_int, arg );
			break;
		case CONSTRAINT_TYPE_ROTLIKE:
			result = rotatelike_setter( self, key_int, arg );
			break;
		case CONSTRAINT_TYPE_SIZELIKE:
			result = sizelike_setter( self, key_int, arg );
			break;
		case CONSTRAINT_TYPE_ROTLIMIT:
			result = rotlimit_setter( self, key_int, arg );
			break;
		case CONSTRAINT_TYPE_LOCLIMIT:
			result = loclimit_setter( self, key_int, arg );
			break;
		case CONSTRAINT_TYPE_SIZELIMIT:
			result = sizelimit_setter( self, key_int, arg);
			break;
		case CONSTRAINT_TYPE_DISTLIMIT:
			result = distlimit_setter( self, key_int, arg);
			break;
		case CONSTRAINT_TYPE_RIGIDBODYJOINT:
			result = rigidbody_setter( self, key_int, arg);
			break;
		case CONSTRAINT_TYPE_CLAMPTO:
			result = clampto_setter( self, key_int, arg);
			break;
		case CONSTRAINT_TYPE_PYTHON:
			result = script_setter( self, key_int, arg);
			break;
		case CONSTRAINT_TYPE_CHILDOF:
			result = childof_setter( self, key_int, arg);
			break;
		case CONSTRAINT_TYPE_TRANSFORM:
			result = transf_setter( self, key_int, arg);
			break;
		case CONSTRAINT_TYPE_NULL:
			return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
		default:
			return EXPP_ReturnIntError( PyExc_RuntimeError,
					"unsupported constraint setting" );
		}
	}
	if( !result && self->pchan )
		update_pose_constraint_flags( self->obj->pose );
	return result;
}

/*****************************************************************************/
/* Function:    Constraint_compare                                           */
/* Description: This compares 2 constraint python types, == or != only.      */
/*****************************************************************************/
static int Constraint_compare( BPy_Constraint * a, BPy_Constraint * b )
{
	return ( a->con == b->con ) ? 0 : -1;
}

/*****************************************************************************/
/* Function:    Constraint_repr                                              */
/* Description: This is a callback function for the BPy_Constraint type. It  */
/*              builds a meaningful string to represent constraint objects.  */
/*****************************************************************************/

static PyObject *Constraint_repr( BPy_Constraint * self )
{
	bConstraintTypeInfo *cti;

	if (!self->con)
		return PyString_FromString("[Constraint - Removed]");
	else
		cti= constraint_get_typeinfo(self->con);
	
	if (cti) {
		return PyString_FromFormat("[Constraint \"%s\", Type \"%s\"]",
				self->con->name, cti->name);
	}
	else {
		return PyString_FromString("[Constraint \"%s\", Type \"Unknown\"]");
	}
}

/* Three Python Constraint_Type helper functions needed by the Object module: */

/*****************************************************************************/
/* Function:    Constraint_CreatePyObject                                    */
/* Description: This function will create a new BPy_Constraint from an       */
/*              existing Blender constraint structure.                       */
/*****************************************************************************/
PyObject *Constraint_CreatePyObject( bPoseChannel *pchan, Object *obj,
		bConstraint *con )
{
	BPy_Constraint *pycon;
	pycon = ( BPy_Constraint * ) PyObject_NEW( BPy_Constraint,
			&Constraint_Type );
	if( !pycon )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create BPy_Constraint object" );

	pycon->con = con;

	/* one of these two will be NULL */
	pycon->obj = obj;	
	pycon->pchan = pchan;
	return ( PyObject * ) pycon;
}

/*****************************************************************************/
/* Function:    Constraint_FromPyObject                                      */
/* Description: This function returns the Blender constraint from the given  */
/*              PyObject.                                                    */
/*****************************************************************************/
bConstraint *Constraint_FromPyObject( BPy_Constraint * self )
{
	return self->con;
}

/*****************************************************************************/
/* Constraint Sequence wrapper                                               */
/*****************************************************************************/

/*
 * Initialize the interator
 */

static PyObject *ConstraintSeq_getIter( BPy_ConstraintSeq * self )
{
	if( self->pchan )
		self->iter = (bConstraint *)self->pchan->constraints.first;
	else
		self->iter = (bConstraint *)self->obj->constraints.first;
	return EXPP_incr_ret ( (PyObject *) self );
}

/*
 * Get the next Constraint
 */

static PyObject *ConstraintSeq_nextIter( BPy_ConstraintSeq * self )
{
	bConstraint *this = self->iter;
	if( this ) {
		self->iter = this->next;
		return Constraint_CreatePyObject( self->pchan, self->obj, this );
	}

	return EXPP_ReturnPyObjError( PyExc_StopIteration,
			"iterator at end" );
}

/* return the number of constraints */

static int ConstraintSeq_length( BPy_ConstraintSeq * self )
{
	return BLI_countlist( self->pchan ?
		&self->pchan->constraints : &self->obj->constraints );
}

/* return a constraint */

static PyObject *ConstraintSeq_item( BPy_ConstraintSeq * self, int i )
{
	bConstraint *con = NULL;

	/* if index is negative, start counting from the end of the list */
	if( i < 0 )
		i += ConstraintSeq_length( self );

	/* skip through the list until we get the constraint or end of list */

	if( self->pchan )
		con = self->pchan->constraints.first;
	else
		con = self->obj->constraints.first;

	while( i && con ) {
		--i;
		con = con->next;
	}

	if( con )
		return Constraint_CreatePyObject( self->pchan, self->obj, con );
	else
		return EXPP_ReturnPyObjError( PyExc_IndexError,
				"array index out of range" );
}

/*****************************************************************************/
/* Python BPy_ConstraintSeq sequence table:                                  */
/*****************************************************************************/
static PySequenceMethods ConstraintSeq_as_sequence = {
	( inquiry ) ConstraintSeq_length,	/* sq_length */
	( binaryfunc ) 0,	/* sq_concat */
	( intargfunc ) 0,	/* sq_repeat */
	( intargfunc ) ConstraintSeq_item,	/* sq_item */
	( intintargfunc ) 0,	/* sq_slice */
	( intobjargproc ) 0,	/* sq_ass_item */
	( intintobjargproc ) 0,	/* sq_ass_slice */
	( objobjproc ) 0,	/* sq_contains */
	( binaryfunc ) 0,		/* sq_inplace_concat */
	( intargfunc ) 0,		/* sq_inplace_repeat */
};

/*
 * helper function to check for a valid constraint argument
 */

static bConstraint *locate_constr( BPy_ConstraintSeq *self, BPy_Constraint * value )
{
	bConstraint *con;

	/* check that argument is a modifier */
	if (!BPy_Constraint_Check(value))
		return (bConstraint *)EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a constraint as an argument" );

	/* check whether constraint has been removed */
	if( !value->con )
		return (bConstraint *)EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This constraint has been removed!" );

	/* verify the constraint is still exists in the stack */
	if( self->pchan )
		con = self->pchan->constraints.first;
	else
		con = self->obj->constraints.first;
	while( con && con != value->con )
	   	con = con->next;

	/* if we didn't find it, exception */
	if( !con )
		return (bConstraint *)EXPP_ReturnPyObjError( PyExc_AttributeError,
				"This constraint is no longer in the object's stack" );

	return con;
}


/* create a new constraint at the end of the list */

static PyObject *ConstraintSeq_append( BPy_ConstraintSeq *self, PyObject *value )
{
	int type = (int)PyInt_AsLong(value);
	bConstraint *con;

	/* type 0 is CONSTRAINT_TYPE_NULL, should we be able to add one of these?
	 * if the value is not an int it will be -1 */
	if( type < CONSTRAINT_TYPE_NULL || type > CONSTRAINT_TYPE_RIGIDBODYJOINT ) 
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"arg not in int or out of range" );

	con = add_new_constraint( type );
	if( self->pchan ) {
		BLI_addtail( &self->pchan->constraints, con );
		update_pose_constraint_flags( self->obj->pose );
	}
	else
		BLI_addtail( &self->obj->constraints, con );

	return Constraint_CreatePyObject( self->pchan, self->obj, con );
}

/* move the constraint up in the stack */

static PyObject *ConstraintSeq_moveUp( BPy_ConstraintSeq *self, BPy_Constraint *value )
{
	bConstraint *con = locate_constr( self,  value );

	/* if we can't locate the constraint, return (exception already set) */
	if( !con )
		return (PyObject *)NULL;

	const_moveUp( self->obj, con );
	Py_RETURN_NONE;
}

/* move the constraint down in the stack */

static PyObject *ConstraintSeq_moveDown( BPy_ConstraintSeq *self, BPy_Constraint *value )
{
	bConstraint *con = locate_constr( self,  value );

	/* if we can't locate the constraint, return (exception already set) */
	if( !con )
		return (PyObject *)NULL;

	const_moveDown( self->obj, con );
	Py_RETURN_NONE;
}

/* remove an existing constraint */

static PyObject *ConstraintSeq_remove( BPy_ConstraintSeq *self, BPy_Constraint *value )
{
	bConstraint *con = locate_constr(self, value);
	bPoseChannel *active= NULL;

	/* if we can't locate the constraint, return (exception already set) */
	if (!con)
		return (PyObject *)NULL;

	/* check if we need to set temporary 'active' flag for pchan */
	if (self->pchan) {
		active= get_active_posechannel(self->obj);
		
		if (active != self->pchan) {
			if (active) active->bone->flag &= ~BONE_ACTIVE;
			self->pchan->bone->flag |= BONE_ACTIVE;
		}
	}
	
	/* del_constr_func() frees constraint + its data */
	del_constr_func( self->obj, con );

	/* reset active pchan (if applicable) */
	if (self->pchan && self->pchan!=active) {
		if (active) active->bone->flag |= BONE_ACTIVE;
		self->pchan->bone->flag &= ~BONE_ACTIVE;
	}
	
	/* erase the link to the constraint */
	value->con = NULL;

	Py_RETURN_NONE;
}

/*****************************************************************************/
/* Function:    ConstraintSeq_dealloc                                        */
/* Description: This is a callback function for the BPy_ConstraintSeq type.  */
/*              It destroys data when the object is deleted.                 */
/*****************************************************************************/
static void ConstraintSeq_dealloc( BPy_Constraint * self )
{
	PyObject_DEL( self );
}

/*****************************************************************************/
/* Python BPy_ConstraintSeq methods table:                                   */
/*****************************************************************************/
static PyMethodDef BPy_ConstraintSeq_methods[] = {
	/* name, method, flags, doc */
	{"append", ( PyCFunction ) ConstraintSeq_append, METH_O,
	 "(type) - add a new constraint, where type is the constraint type"},
	{"remove", ( PyCFunction ) ConstraintSeq_remove, METH_O,
	 "(con) - remove an existing constraint, where con is a constraint from this object."},
	{"moveUp", ( PyCFunction ) ConstraintSeq_moveUp, METH_O,
	 "(con) - Move constraint up in stack"},
	{"moveDown", ( PyCFunction ) ConstraintSeq_moveDown, METH_O,
	 "(con) - Move constraint down in stack"},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python ConstraintSeq_Type structure definition:                           */
/*****************************************************************************/
PyTypeObject ConstraintSeq_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender.Constraints",      /* char *tp_name; */
	sizeof( BPy_ConstraintSeq ),     /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) ConstraintSeq_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	( reprfunc ) NULL,          /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	&ConstraintSeq_as_sequence,        /* PySequenceMethods *tp_as_sequence; */
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
	( getiterfunc )ConstraintSeq_getIter, /* getiterfunc tp_iter; */
    ( iternextfunc )ConstraintSeq_nextIter, /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_ConstraintSeq_methods,         /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	NULL,                       /* struct PyGetSetDef *tp_getset; */
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
/* Function:    PoseConstraintSeq_CreatePyObject                             */
/* Description: This function will create a new BPy_ConstraintSeq from an    */
/*              existing ListBase structure.                                 */
/*****************************************************************************/
PyObject *PoseConstraintSeq_CreatePyObject( bPoseChannel *pchan )
{
	BPy_ConstraintSeq *pyseq;
	Object *ob;

	for( ob = G.main->object.first; ob; ob = ob->id.next ) {
		if( ob->type == OB_ARMATURE ) {
			bPoseChannel *p = ob->pose->chanbase.first;
			while( p ) {
				if( p == pchan ) {
					pyseq = ( BPy_ConstraintSeq * ) PyObject_NEW( 
							BPy_ConstraintSeq, &ConstraintSeq_Type );
					if( !pyseq )
						return EXPP_ReturnPyObjError( PyExc_MemoryError,
								"couldn't create BPy_ConstraintSeq object" );
					pyseq->pchan = pchan;
					pyseq->obj = ob;
					return ( PyObject * ) pyseq;
				} else
					p = p->next;
			}
		}
	}
	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"couldn't find ANY armature with the pose!" );

}

/*****************************************************************************/
/* Function:    ObConstraintSeq_CreatePyObject                               */
/* Description: This function will create a new BPy_ConstraintSeq from an    */
/*              existing ListBase structure.                                 */
/*****************************************************************************/
PyObject *ObConstraintSeq_CreatePyObject( Object *obj )
{
	BPy_ConstraintSeq *pyseq;
	pyseq = ( BPy_ConstraintSeq * ) PyObject_NEW( BPy_ConstraintSeq,
			&ConstraintSeq_Type );
	if( !pyseq )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create BPy_ConstraintSeq object" );
	pyseq->obj = obj;
	pyseq->pchan = NULL;
	return ( PyObject * ) pyseq;
}

static PyObject *M_Constraint_TypeDict( void )
{
	PyObject *S = PyConstant_New(  );

	if( S ) {
		BPy_constant *d = ( BPy_constant * ) S;
		PyConstant_Insert( d, "NULL", 
				PyInt_FromLong( CONSTRAINT_TYPE_NULL ) );
		PyConstant_Insert( d, "TRACKTO",
				PyInt_FromLong( CONSTRAINT_TYPE_TRACKTO ) );
		PyConstant_Insert( d, "IKSOLVER", 
				PyInt_FromLong( CONSTRAINT_TYPE_KINEMATIC ) );
		PyConstant_Insert( d, "FOLLOWPATH", 
				PyInt_FromLong( CONSTRAINT_TYPE_FOLLOWPATH ) );
		PyConstant_Insert( d, "COPYROT", 
				PyInt_FromLong( CONSTRAINT_TYPE_ROTLIKE ) );
		PyConstant_Insert( d, "COPYLOC", 
				PyInt_FromLong( CONSTRAINT_TYPE_LOCLIKE ) );
		PyConstant_Insert( d, "COPYSIZE", 
				PyInt_FromLong( CONSTRAINT_TYPE_SIZELIKE ) );
		PyConstant_Insert( d, "ACTION", 
				PyInt_FromLong( CONSTRAINT_TYPE_ACTION ) );
		PyConstant_Insert( d, "LOCKTRACK", 
				PyInt_FromLong( CONSTRAINT_TYPE_LOCKTRACK ) );
		PyConstant_Insert( d, "STRETCHTO", 
				PyInt_FromLong( CONSTRAINT_TYPE_STRETCHTO ) );
		PyConstant_Insert( d, "FLOOR", 
				PyInt_FromLong( CONSTRAINT_TYPE_MINMAX ) );
		PyConstant_Insert( d, "LIMITLOC", 
				PyInt_FromLong( CONSTRAINT_TYPE_LOCLIMIT ) );
		PyConstant_Insert( d, "LIMITROT", 
				PyInt_FromLong( CONSTRAINT_TYPE_ROTLIMIT ) );
		PyConstant_Insert( d, "LIMITSIZE", 
				PyInt_FromLong( CONSTRAINT_TYPE_SIZELIMIT ) );
		PyConstant_Insert( d, "LIMITDIST",
				PyInt_FromLong( CONSTRAINT_TYPE_DISTLIMIT ) );
		PyConstant_Insert( d, "RIGIDBODYJOINT", 
				PyInt_FromLong( CONSTRAINT_TYPE_RIGIDBODYJOINT ) );
		PyConstant_Insert( d, "CLAMPTO", 
				PyInt_FromLong( CONSTRAINT_TYPE_CLAMPTO ) );
		PyConstant_Insert( d, "PYTHON",
				PyInt_FromLong( CONSTRAINT_TYPE_PYTHON ) );
		PyConstant_Insert( d, "CHILDOF",
				PyInt_FromLong( CONSTRAINT_TYPE_CHILDOF ) );
		PyConstant_Insert( d, "TRANSFORM",
				PyInt_FromLong( CONSTRAINT_TYPE_TRANSFORM ) );
	}
	return S;
}

static PyObject *M_Constraint_SettingsDict( void )
{
	PyObject *S = PyConstant_New(  );
	
	if( S ) {
		BPy_constant *d = ( BPy_constant * ) S;
		PyConstant_Insert( d, "XROT",
				PyInt_FromLong( EXPP_CONSTR_XROT ) );
		PyConstant_Insert( d, "YROT",
				PyInt_FromLong( EXPP_CONSTR_YROT ) );
		PyConstant_Insert( d, "ZROT",
				PyInt_FromLong( EXPP_CONSTR_ZROT ) );
		PyConstant_Insert( d, "XSIZE",
				PyInt_FromLong( EXPP_CONSTR_XSIZE ) );
		PyConstant_Insert( d, "YSIZE",
				PyInt_FromLong( EXPP_CONSTR_YSIZE ) );
		PyConstant_Insert( d, "ZSIZE",
				PyInt_FromLong( EXPP_CONSTR_ZSIZE ) );
		PyConstant_Insert( d, "XLOC",
				PyInt_FromLong( EXPP_CONSTR_XLOC ) );
		PyConstant_Insert( d, "YLOC",
				PyInt_FromLong( EXPP_CONSTR_YLOC ) );
		PyConstant_Insert( d, "ZLOC",
				PyInt_FromLong( EXPP_CONSTR_ZLOC ) );

		PyConstant_Insert( d, "UPX",
				PyInt_FromLong( UP_X ) );
		PyConstant_Insert( d, "UPY",
				PyInt_FromLong( UP_Y ) );
		PyConstant_Insert( d, "UPZ",
				PyInt_FromLong( UP_Z ) );

		PyConstant_Insert( d, "TRACKX",
				PyInt_FromLong( TRACK_X ) );
		PyConstant_Insert( d, "TRACKY",
				PyInt_FromLong( TRACK_Y ) );
		PyConstant_Insert( d, "TRACKZ",
				PyInt_FromLong( TRACK_Z ) );
		PyConstant_Insert( d, "TRACKNEGX",
				PyInt_FromLong( TRACK_nX ) );
		PyConstant_Insert( d, "TRACKNEGY",
				PyInt_FromLong( TRACK_nY ) );
		PyConstant_Insert( d, "TRACKNEGZ",
				PyInt_FromLong( TRACK_nZ ) );

		PyConstant_Insert( d, "VOLUMEXZ",
				PyInt_FromLong( VOLUME_XZ ) );
		PyConstant_Insert( d, "VOLUMEX",
				PyInt_FromLong( VOLUME_X ) );
		PyConstant_Insert( d, "VOLUMEZ",
				PyInt_FromLong( VOLUME_Z ) );
		PyConstant_Insert( d, "VOLUMENONE",
				PyInt_FromLong( NO_VOLUME ) );

		PyConstant_Insert( d, "PLANEX",
				PyInt_FromLong( PLANE_X ) );
		PyConstant_Insert( d, "PLANEY",
				PyInt_FromLong( PLANE_Y ) );
		PyConstant_Insert( d, "PLANEZ",
				PyInt_FromLong( PLANE_Z ) );

		PyConstant_Insert( d, "LOCKX",
				PyInt_FromLong( LOCK_X ) );
		PyConstant_Insert( d, "LOCKY",
				PyInt_FromLong( LOCK_Y ) );
		PyConstant_Insert( d, "LOCKZ",
				PyInt_FromLong( LOCK_Z ) );

		PyConstant_Insert( d, "MAXX",
				PyInt_FromLong( EXPP_CONSTR_MAXX ) );
		PyConstant_Insert( d, "MAXY",
				PyInt_FromLong( EXPP_CONSTR_MAXY ) );
		PyConstant_Insert( d, "MAXZ",
				PyInt_FromLong( EXPP_CONSTR_MAXZ ) );
		PyConstant_Insert( d, "MINX",
				PyInt_FromLong( EXPP_CONSTR_MINX ) );
		PyConstant_Insert( d, "MINY",
				PyInt_FromLong( EXPP_CONSTR_MINY ) );
		PyConstant_Insert( d, "MINZ",
				PyInt_FromLong( EXPP_CONSTR_MINZ ) );

		PyConstant_Insert( d, "COPYX",
				PyInt_FromLong( LOCLIKE_X ) );
		PyConstant_Insert( d, "COPYY",
				PyInt_FromLong( LOCLIKE_Y ) );
		PyConstant_Insert( d, "COPYZ",
				PyInt_FromLong( LOCLIKE_Z ) );
		PyConstant_Insert( d, "COPYXINVERT",
				PyInt_FromLong( LOCLIKE_X_INVERT ) );
		PyConstant_Insert( d, "COPYYINVERT",
				PyInt_FromLong( LOCLIKE_Y_INVERT ) );
		PyConstant_Insert( d, "COPYZINVERT",
				PyInt_FromLong( LOCLIKE_Z_INVERT ) );
				
		PyConstant_Insert( d, "PARLOCX",
				PyInt_FromLong( CHILDOF_LOCX ) );
		PyConstant_Insert( d, "PARLOCY",
				PyInt_FromLong( CHILDOF_LOCY ) );
		PyConstant_Insert( d, "PARLOCZ",
				PyInt_FromLong( CHILDOF_LOCZ ) );
		PyConstant_Insert( d, "PARROTX",
				PyInt_FromLong( CHILDOF_ROTX ) );
		PyConstant_Insert( d, "PARROTY",
				PyInt_FromLong( CHILDOF_ROTY ) );
		PyConstant_Insert( d, "PARROTZ",
				PyInt_FromLong( CHILDOF_ROTZ ) );
		PyConstant_Insert( d, "PARSIZEX",
				PyInt_FromLong( CHILDOF_LOCX ) );
		PyConstant_Insert( d, "PARSIZEY",
				PyInt_FromLong( CHILDOF_SIZEY ) );
		PyConstant_Insert( d, "PARSIZEZ",
				PyInt_FromLong( CHILDOF_SIZEZ ) );
				
		PyConstant_Insert( d, "CLAMPAUTO",
				PyInt_FromLong( CLAMPTO_AUTO ) );
		PyConstant_Insert( d, "CLAMPX",
				PyInt_FromLong( CLAMPTO_X ) );
		PyConstant_Insert( d, "CLAMPY",
				PyInt_FromLong( CLAMPTO_Y ) );
		PyConstant_Insert( d, "CLAMPZ",
				PyInt_FromLong( CLAMPTO_Z ) );
		PyConstant_Insert( d, "CLAMPCYCLIC",
				PyInt_FromLong( EXPP_CONSTR_CLAMPCYCLIC ));

		PyConstant_Insert( d, "TARGET",
				PyInt_FromLong( EXPP_CONSTR_TARGET ) );
		PyConstant_Insert( d, "STRETCH", 
				PyInt_FromLong( EXPP_CONSTR_STRETCH ) );
		PyConstant_Insert( d, "ITERATIONS", 
				PyInt_FromLong( EXPP_CONSTR_ITERATIONS ) );
		PyConstant_Insert( d, "BONE", 
				PyInt_FromLong( EXPP_CONSTR_BONE ) );
		PyConstant_Insert( d, "CHAINLEN", 
				PyInt_FromLong( EXPP_CONSTR_CHAINLEN ) );
		PyConstant_Insert( d, "POSWEIGHT", 
				PyInt_FromLong( EXPP_CONSTR_POSWEIGHT ) );
		PyConstant_Insert( d, "ROTWEIGHT", 
				PyInt_FromLong( EXPP_CONSTR_ROTWEIGHT ) );
		PyConstant_Insert( d, "ROTATE", 
				PyInt_FromLong( EXPP_CONSTR_ROTATE ) );
		PyConstant_Insert( d, "USETIP", 
				PyInt_FromLong( EXPP_CONSTR_USETIP ) );

		PyConstant_Insert( d, "ACTION", 
				PyInt_FromLong( EXPP_CONSTR_ACTION ) );
		PyConstant_Insert( d, "START", 
				PyInt_FromLong( EXPP_CONSTR_START ) );
		PyConstant_Insert( d, "END", 
				PyInt_FromLong( EXPP_CONSTR_END ) );
		PyConstant_Insert( d, "MIN", 
				PyInt_FromLong( EXPP_CONSTR_MIN ) );
		PyConstant_Insert( d, "MAX", 
				PyInt_FromLong( EXPP_CONSTR_MAX ) );
		PyConstant_Insert( d, "KEYON", 
				PyInt_FromLong( EXPP_CONSTR_KEYON ) );

		PyConstant_Insert( d, "TRACK", 
				PyInt_FromLong( EXPP_CONSTR_TRACK ) );
		PyConstant_Insert( d, "UP", 
				PyInt_FromLong( EXPP_CONSTR_UP ) );

		PyConstant_Insert( d, "RESTLENGTH",
				PyInt_FromLong( EXPP_CONSTR_RESTLENGTH ) );
		PyConstant_Insert( d, "VOLVARIATION",
				PyInt_FromLong( EXPP_CONSTR_VOLVARIATION ) );
		PyConstant_Insert( d, "VOLUMEMODE",
				PyInt_FromLong( EXPP_CONSTR_VOLUMEMODE ) );
		PyConstant_Insert( d, "PLANE",
				PyInt_FromLong( EXPP_CONSTR_PLANE ) );

		PyConstant_Insert( d, "FOLLOW",
				PyInt_FromLong( EXPP_CONSTR_FOLLOW ) );
		PyConstant_Insert( d, "OFFSET",
				PyInt_FromLong( EXPP_CONSTR_OFFSET ) );
		PyConstant_Insert( d, "FORWARD",
				PyInt_FromLong( EXPP_CONSTR_FORWARD ) );

		PyConstant_Insert( d, "LOCK",
				PyInt_FromLong( EXPP_CONSTR_LOCK ) );
				
		PyConstant_Insert( d, "LIMITMODE",
				PyInt_FromLong( EXPP_CONSTR_MODE ) );
		PyConstant_Insert( d, "LIMIT_INSIDE",
				PyInt_FromLong( LIMITDIST_INSIDE ) );
		PyConstant_Insert( d, "LIMIT_OUTSIDE",
				PyInt_FromLong( LIMITDIST_OUTSIDE ) );
		PyConstant_Insert( d, "LIMIT_ONSURFACE",
				PyInt_FromLong( LIMITDIST_ONSURFACE ) );

		PyConstant_Insert( d, "COPY",
				PyInt_FromLong( EXPP_CONSTR_COPY ) );
		PyConstant_Insert( d, "LIMIT",
				PyInt_FromLong( EXPP_CONSTR_LIMIT ) );
		PyConstant_Insert( d, "CLAMP",
				PyInt_FromLong( EXPP_CONSTR_CLAMP ) );
		
		PyConstant_Insert( d, "LIMIT_XMIN",
				PyInt_FromLong( EXPP_CONSTR_LIMXMIN ) );
		PyConstant_Insert( d, "LIMIT_XMAX",
				PyInt_FromLong( EXPP_CONSTR_LIMXMAX ) );
		PyConstant_Insert( d, "LIMIT_YMIN",
				PyInt_FromLong( EXPP_CONSTR_LIMYMIN ) );
		PyConstant_Insert( d, "LIMIT_YMAX",
				PyInt_FromLong( EXPP_CONSTR_LIMYMAX ) );
		PyConstant_Insert( d, "LIMIT_ZMIN",
				PyInt_FromLong( EXPP_CONSTR_LIMZMIN ) );
		PyConstant_Insert( d, "LIMIT_ZMAX",
				PyInt_FromLong( EXPP_CONSTR_LIMZMAX ) );
		
		PyConstant_Insert( d, "LIMIT_XROT",
				PyInt_FromLong( EXPP_CONSTR_LIMXROT ) );
		PyConstant_Insert( d, "LIMIT_YROT",
				PyInt_FromLong( EXPP_CONSTR_LIMYROT ) );
		PyConstant_Insert( d, "LIMIT_ZROT",
				PyInt_FromLong( EXPP_CONSTR_LIMZROT ) );
		
		PyConstant_Insert( d, "XMIN",
				PyInt_FromLong( EXPP_CONSTR_XMIN ) );
		PyConstant_Insert( d, "XMAX",
				PyInt_FromLong( EXPP_CONSTR_XMAX ) );
		PyConstant_Insert( d, "YMIN",
				PyInt_FromLong( EXPP_CONSTR_YMIN ) );
		PyConstant_Insert( d, "YMAX",
				PyInt_FromLong( EXPP_CONSTR_YMAX ) );
		PyConstant_Insert( d, "ZMIN",
				PyInt_FromLong( EXPP_CONSTR_ZMIN ) );
		PyConstant_Insert( d, "ZMAX",
				PyInt_FromLong( EXPP_CONSTR_ZMAX ) );
				
		PyConstant_Insert( d, "SCRIPT",
				PyInt_FromLong( EXPP_CONSTR_SCRIPT ) );
		PyConstant_Insert( d, "PROPERTIES",
				PyInt_FromLong( EXPP_CONSTR_PROPS ) );
				
		PyConstant_Insert( d, "FROM",
				PyInt_FromLong( EXPP_CONSTR_FROM ) );
		PyConstant_Insert( d, "TO",
				PyInt_FromLong( EXPP_CONSTR_TO ) );
		PyConstant_Insert( d, "EXTRAPOLATE",
				PyInt_FromLong( EXPP_CONSTR_EXPO ) );
		PyConstant_Insert( d, "MAPX",
				PyInt_FromLong( EXPP_CONSTR_MAPX ) );
		PyConstant_Insert( d, "MAPY",
				PyInt_FromLong( EXPP_CONSTR_MAPY ) );
		PyConstant_Insert( d, "MAPZ",
				PyInt_FromLong( EXPP_CONSTR_MAPZ ) );
		PyConstant_Insert( d, "FROM_MINX",
				PyInt_FromLong( EXPP_CONSTR_FROMMINX ) );
		PyConstant_Insert( d, "FROM_MAXX",
				PyInt_FromLong( EXPP_CONSTR_FROMMAXX ) );
		PyConstant_Insert( d, "FROM_MINY",
				PyInt_FromLong( EXPP_CONSTR_FROMMINY ) );
		PyConstant_Insert( d, "FROM_MAXY",
				PyInt_FromLong( EXPP_CONSTR_FROMMAXY ) );
		PyConstant_Insert( d, "FROM_MINZ",
				PyInt_FromLong( EXPP_CONSTR_FROMMINZ ) );
		PyConstant_Insert( d, "FROM_MAXZ",
				PyInt_FromLong( EXPP_CONSTR_FROMMAXZ ) );
		PyConstant_Insert( d, "TO_MINX",
				PyInt_FromLong( EXPP_CONSTR_TOMINX ) );
		PyConstant_Insert( d, "TO_MAXX",
				PyInt_FromLong( EXPP_CONSTR_TOMAXX ) );
		PyConstant_Insert( d, "TO_MINY",
				PyInt_FromLong( EXPP_CONSTR_TOMINY ) );
		PyConstant_Insert( d, "TO_MAXY",
				PyInt_FromLong( EXPP_CONSTR_TOMAXY ) );
		PyConstant_Insert( d, "TO_MINZ",
				PyInt_FromLong( EXPP_CONSTR_TOMINZ ) );
		PyConstant_Insert( d, "TO_MAXZ",
				PyInt_FromLong( EXPP_CONSTR_TOMAXZ ) );
				
		PyConstant_Insert( d, "LOC",
				PyInt_FromLong( 0 ) );
		PyConstant_Insert( d, "ROT",
				PyInt_FromLong( 1 ) );
		PyConstant_Insert( d, "SCALE",
				PyInt_FromLong( 2 ) );
				
		PyConstant_Insert( d, "CONSTR_RB_TYPE",
				PyInt_FromLong( EXPP_CONSTR_RB_TYPE ) );
		PyConstant_Insert( d, "CONSTR_RB_BALL",
				PyInt_FromLong( EXPP_CONSTR_RB_BALL ) );
		PyConstant_Insert( d, "CONSTR_RB_HINGE",
				PyInt_FromLong( EXPP_CONSTR_RB_HINGE ) );
		PyConstant_Insert( d, "CONSTR_RB_GENERIC6DOF",
				PyInt_FromLong( EXPP_CONSTR_RB_GENERIC6DOF ) );
		PyConstant_Insert( d, "CONSTR_RB_VEHICLE",
				PyInt_FromLong( EXPP_CONSTR_RB_VEHICLE ) );
		PyConstant_Insert( d, "CONSTR_RB_PIVX",
				PyInt_FromLong( EXPP_CONSTR_RB_PIVX ) );
		PyConstant_Insert( d, "CONSTR_RB_PIVY",
				PyInt_FromLong( EXPP_CONSTR_RB_PIVY ) );
		PyConstant_Insert( d, "CONSTR_RB_PIVZ",
				PyInt_FromLong( EXPP_CONSTR_RB_PIVZ ) );
		PyConstant_Insert( d, "CONSTR_RB_AXX",
				PyInt_FromLong( EXPP_CONSTR_RB_AXX ) );
		PyConstant_Insert( d, "CONSTR_RB_AXY",
				PyInt_FromLong( EXPP_CONSTR_RB_AXY ) );
		PyConstant_Insert( d, "CONSTR_RB_AXZ",
				PyInt_FromLong( EXPP_CONSTR_RB_AXZ ) );
		PyConstant_Insert( d, "CONSTR_RB_MINLIMIT0",
				PyInt_FromLong( EXPP_CONSTR_RB_MINLIMIT0 ) );
		PyConstant_Insert( d, "CONSTR_RB_MINLIMIT1",
				PyInt_FromLong( EXPP_CONSTR_RB_MINLIMIT1 ) );
		PyConstant_Insert( d, "CONSTR_RB_MINLIMIT2",
				PyInt_FromLong( EXPP_CONSTR_RB_MINLIMIT2 ) );
		PyConstant_Insert( d, "CONSTR_RB_MINLIMIT3",
				PyInt_FromLong( EXPP_CONSTR_RB_MINLIMIT3 ) );
		PyConstant_Insert( d, "CONSTR_RB_MINLIMIT4",
				PyInt_FromLong( EXPP_CONSTR_RB_MINLIMIT4 ) );
		PyConstant_Insert( d, "CONSTR_RB_MINLIMIT5",
				PyInt_FromLong( EXPP_CONSTR_RB_MINLIMIT5 ) );
		PyConstant_Insert( d, "CONSTR_RB_MAXLIMIT0",
				PyInt_FromLong( EXPP_CONSTR_RB_MAXLIMIT0 ) );
		PyConstant_Insert( d, "CONSTR_RB_MAXLIMIT1",
				PyInt_FromLong( EXPP_CONSTR_RB_MAXLIMIT1 ) );
		PyConstant_Insert( d, "CONSTR_RB_MAXLIMIT2",
				PyInt_FromLong( EXPP_CONSTR_RB_MAXLIMIT2 ) );
		PyConstant_Insert( d, "CONSTR_RB_MAXLIMIT3",
				PyInt_FromLong( EXPP_CONSTR_RB_MAXLIMIT3 ) );
		PyConstant_Insert( d, "CONSTR_RB_MAXLIMIT4",
				PyInt_FromLong( EXPP_CONSTR_RB_MAXLIMIT4 ) );
		PyConstant_Insert( d, "CONSTR_RB_MAXLIMIT5",
				PyInt_FromLong( EXPP_CONSTR_RB_MAXLIMIT5 ) );				
		PyConstant_Insert( d, "CONSTR_RB_EXTRAFZ",
				PyInt_FromLong( EXPP_CONSTR_RB_EXTRAFZ ) );
		PyConstant_Insert( d, "CONSTR_RB_FLAG",
				PyInt_FromLong( EXPP_CONSTR_RB_FLAG ) );
				
				
		PyConstant_Insert( d, "OWNERSPACE",
				PyInt_FromLong( EXPP_CONSTR_OWNSPACE ) );
		PyConstant_Insert( d, "TARGETSPACE",
				PyInt_FromLong( EXPP_CONSTR_TARSPACE ) );
				
		PyConstant_Insert( d, "SPACE_WORLD",
				PyInt_FromLong( CONSTRAINT_SPACE_WORLD) );
		PyConstant_Insert( d, "SPACE_LOCAL",
				PyInt_FromLong( CONSTRAINT_SPACE_LOCAL ) );
		PyConstant_Insert( d, "SPACE_POSE",
				PyInt_FromLong( CONSTRAINT_SPACE_POSE) );
		PyConstant_Insert( d, "SPACE_PARLOCAL",
				PyInt_FromLong( CONSTRAINT_SPACE_PARLOCAL ) );
	}
	return S;
}

/*****************************************************************************/
/* Function:              Constraint_Init                                    */
/*****************************************************************************/
PyObject *Constraint_Init( void )
{
	PyObject *submodule;
	PyObject *TypeDict = M_Constraint_TypeDict( );
	PyObject *SettingsDict = M_Constraint_SettingsDict( );

	if( PyType_Ready( &ConstraintSeq_Type ) < 0
			|| PyType_Ready( &Constraint_Type ) < 0 )
		return NULL;

	submodule = Py_InitModule3( "Blender.Constraint", NULL,
			"Constraint module for accessing and creating constraint data" );

	if( TypeDict )
		PyModule_AddObject( submodule, "Type", TypeDict );
	
	if( SettingsDict )
		PyModule_AddObject( submodule, "Settings", SettingsDict );
	
	return submodule;
}
