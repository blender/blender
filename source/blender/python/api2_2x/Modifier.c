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
 * Contributor(s): Ken Hughes
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "Modifier.h" /*This must come first*/

#include "DNA_object_types.h"
#include "DNA_effect_types.h"
#include "DNA_vec_types.h"

#include "BKE_main.h"
#include "BKE_global.h"
#include "BKE_modifier.h"
#include "BKE_library.h"
#include "BLI_blenlib.h"
#include "MEM_guardedalloc.h"
#include "butspace.h"
#include "blendef.h"
#include "mydevice.h"

#include "Object.h"
#include "gen_utils.h"

enum mod_constants {
	EXPP_MOD_RENDER = 0,
	EXPP_MOD_REALTIME,
	EXPP_MOD_EDITMODE,
	EXPP_MOD_ONCAGE,

	EXPP_MOD_SUBSURF_TYPES,
	EXPP_MOD_SUBSURF_LEVELS,
	EXPP_MOD_SUBSURF_RENDLEVELS,
	EXPP_MOD_SUBSURF_OPTIMAL,
	EXPP_MOD_SUBSURF_UV,

	EXPP_MOD_ARMATURE_OBJECT,
	EXPP_MOD_ARMATURE_VERTGROUPS,
	EXPP_MOD_ARMATURE_ENVELOPES,

	EXPP_MOD_LATTICE_OBJECT,
	EXPP_MOD_LATTICE_VERTGROUP,

	EXPP_MOD_CURVE_OBJECT,
	EXPP_MOD_CURVE_VERTGROUP,

	EXPP_MOD_BUILD_START,
	EXPP_MOD_BUILD_LENGTH,
	EXPP_MOD_BUILD_SEED,
	EXPP_MOD_BUILD_RANDOMIZE,

	EXPP_MOD_MIRROR_LIMIT,
	EXPP_MOD_MIRROR_FLAG,
	EXPP_MOD_MIRROR_AXIS,

	EXPP_MOD_DECIMATE_RATIO,
	EXPP_MOD_DECIMATE_COUNT,

	EXPP_MOD_WAVE_STARTX,
	EXPP_MOD_WAVE_STARTY,
	EXPP_MOD_WAVE_HEIGHT,
	EXPP_MOD_WAVE_WIDTH,
	EXPP_MOD_WAVE_NARROW,
	EXPP_MOD_WAVE_SPEED,
	EXPP_MOD_WAVE_DAMP,
	EXPP_MOD_WAVE_LIFETIME,
	EXPP_MOD_WAVE_TIMEOFFS,
	EXPP_MOD_WAVE_FLAG,

	EXPP_MOD_BOOLEAN_OPERATION,
	EXPP_MOD_BOOLEAN_OBJECT,

	/* yet to be implemented */
	/* EXPP_MOD_HOOK_,
	EXPP_MOD_ARRAY_, */
};

/*****************************************************************************/
/* Python BPy_Modifier methods declarations:                                 */
/*****************************************************************************/
static PyObject *Modifier_getName( BPy_Modifier * self );
static int Modifier_setName( BPy_Modifier * self, PyObject *arg );

static PyObject *Modifier_getKeys( BPy_Modifier * self );
static PyObject *Modifier_moveUp( BPy_Modifier * self );
static PyObject *Modifier_moveDown( BPy_Modifier * self );

static PyObject *Modifier_getData( BPy_Modifier * self, PyObject * key );
static int Modifier_setData( BPy_Modifier * self, PyObject * key, 
		PyObject * value );

/*****************************************************************************/
/* Python BPy_Modifier methods table:                                        */
/*****************************************************************************/
static PyMethodDef BPy_Modifier_methods[] = {
	/* name, method, flags, doc */
	{"up", ( PyCFunction ) Modifier_moveUp, METH_NOARGS,
	 "Move modifier up in stack"},
	{"down", ( PyCFunction ) Modifier_moveDown, METH_NOARGS,
	 "Move modifier down in stack"},
	{"keys", ( PyCFunction )Modifier_getKeys, METH_NOARGS,
	 "Modifier keys"},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python BPy_Modifier attributes get/set structure:                         */
/*****************************************************************************/
static PyGetSetDef BPy_Modifier_getseters[] = {
	{"name",
	(getter)Modifier_getName, (setter)Modifier_setName,
	 "Modifier name", NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/*****************************************************************************/
/* Python Modifier_Type Mapping Methods table:                               */
/*****************************************************************************/
static PyMappingMethods Modifier_as_mapping = {
	NULL,                               /* mp_length        */
	( binaryfunc ) Modifier_getData,	/* mp_subscript     */
	( objobjargproc ) Modifier_setData,	/* mp_ass_subscript */
};

/*****************************************************************************/
/* Python Modifier_Type callback function prototypes:                        */
/*****************************************************************************/
static PyObject *Modifier_repr( BPy_Modifier * self );

/*****************************************************************************/
/* Python Modifier_Type structure definition:                                */
/*****************************************************************************/
PyTypeObject Modifier_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender Modifier",         /* char *tp_name; */
	sizeof( BPy_Modifier ),     /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) PyObject_Del,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	( reprfunc ) Modifier_repr, /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	&Modifier_as_mapping,       /* PyMappingMethods *tp_as_mapping; */

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
	BPy_Modifier_methods,       /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_Modifier_getseters,     /* struct PyGetSetDef *tp_getset; */
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
/* Python BPy_Modifier methods:                                              */
/*****************************************************************************/

/*
 * return the name of this modifier
 */

static PyObject *Modifier_getName( BPy_Modifier * self )
{
	if (self->md==NULL)
		return (EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This modifier has been removed!" ));
	
	return PyString_FromString( self->md->name );
}

/*
 * set the name of this modifier
 */

static int Modifier_setName( BPy_Modifier * self, PyObject * attr )
{
	char *name = PyString_AsString( attr );
	if( !name )
		return EXPP_ReturnIntError( PyExc_TypeError, "expected string arg" );

	if (self->md==NULL)
		return (EXPP_ReturnIntError( PyExc_RuntimeError,
				"This modifier has been removed!" ));
	
	BLI_strncpy( self->md->name, name, sizeof( self->md->name ) );

	return 0;
}

/*
 * move the modifier up in the stack
 */

static PyObject *Modifier_moveUp( BPy_Modifier * self )
{
	if (self->md==NULL)
		return (EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This modifier has been removed!" ));
	
	if( mod_moveUp( self->obj, self->md ) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"cannot move above a modifier requiring original data" );

	Py_RETURN_NONE;
}

/*
 * move the modifier down in the stack
 */

static PyObject *Modifier_moveDown( BPy_Modifier * self )
{
	if (self->md==NULL)
		return (EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This modifier has been removed!" ));
	
	if( mod_moveDown( self->obj, self->md ) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"cannot move beyond a non-deforming modifier" );

	Py_RETURN_NONE;
}

/*
 * return a constant object which contains all the data attributes which
 * can be accessed for each modifier type
 */

static PyObject *Modifier_getKeys( BPy_Modifier * self )
{
	BPy_constant *attr = (BPy_constant *)PyConstant_New();

	PyConstant_Insert( attr, "RENDER", PyInt_FromLong( EXPP_MOD_RENDER ) );
	PyConstant_Insert( attr, "REALTIME", PyInt_FromLong( EXPP_MOD_REALTIME ) );
	PyConstant_Insert( attr, "EDITMODE", PyInt_FromLong( EXPP_MOD_EDITMODE ) );
	PyConstant_Insert( attr, "ONCAGE", PyInt_FromLong( EXPP_MOD_ONCAGE ) );
	switch( self->md->type ) {
    	case eModifierType_Subsurf:
			PyConstant_Insert( attr, "TYPE",
					PyInt_FromLong( EXPP_MOD_SUBSURF_TYPES ) );
			PyConstant_Insert( attr, "LEVELS",
					PyInt_FromLong( EXPP_MOD_SUBSURF_LEVELS ) );
			PyConstant_Insert( attr, "RENDER_LEVELS",
					PyInt_FromLong( EXPP_MOD_SUBSURF_RENDLEVELS ) );
			PyConstant_Insert( attr, "OPTIMAL",
					PyInt_FromLong( EXPP_MOD_SUBSURF_OPTIMAL ) );
			PyConstant_Insert( attr, "UV",
					PyInt_FromLong( EXPP_MOD_SUBSURF_UV ) );
			break;

    	case eModifierType_Armature:
			PyConstant_Insert( attr, "OBJECT",
					PyInt_FromLong( EXPP_MOD_ARMATURE_OBJECT ) );
			PyConstant_Insert( attr, "VERTGROUPS",
					PyInt_FromLong( EXPP_MOD_ARMATURE_VERTGROUPS ) );
			PyConstant_Insert( attr, "ENVELOPES",
					PyInt_FromLong( EXPP_MOD_ARMATURE_ENVELOPES ) );
			break;

    	case eModifierType_Lattice:
			PyConstant_Insert( attr, "OBJECT",
					PyInt_FromLong( EXPP_MOD_LATTICE_OBJECT) );
			PyConstant_Insert( attr, "VERTGROUP",
					PyInt_FromLong( EXPP_MOD_LATTICE_VERTGROUP) );
			break;

    	case eModifierType_Curve:
			PyConstant_Insert( attr, "OBJECT",
					PyInt_FromLong( EXPP_MOD_CURVE_OBJECT ) );
			PyConstant_Insert( attr, "VERTGROUP",
					PyInt_FromLong( EXPP_MOD_CURVE_VERTGROUP ) );
			break;

    	case eModifierType_Build:
			PyConstant_Insert( attr, "START",
					PyInt_FromLong( EXPP_MOD_BUILD_START ) );
			PyConstant_Insert( attr, "LENGTH",
					PyInt_FromLong( EXPP_MOD_BUILD_LENGTH ) );
			PyConstant_Insert( attr, "SEED",
					PyInt_FromLong( EXPP_MOD_BUILD_SEED ) );
			PyConstant_Insert( attr, "RANDOMIZE",
					PyInt_FromLong( EXPP_MOD_BUILD_RANDOMIZE ) );
			break;

    	case eModifierType_Mirror:
			PyConstant_Insert( attr, "LIMIT",
					PyInt_FromLong( EXPP_MOD_MIRROR_LIMIT ) );
			PyConstant_Insert( attr, "FLAG",
					PyInt_FromLong( EXPP_MOD_MIRROR_FLAG ) );
			PyConstant_Insert( attr, "AXIS",
					PyInt_FromLong( EXPP_MOD_MIRROR_AXIS ) );
			break;

    	case eModifierType_Decimate:
			PyConstant_Insert( attr, "RATIO",
					PyInt_FromLong( EXPP_MOD_DECIMATE_RATIO ) );
			PyConstant_Insert( attr, "FACE_COUNT",
					PyInt_FromLong( EXPP_MOD_DECIMATE_COUNT ) );
			break;

    	case eModifierType_Wave:
			PyConstant_Insert( attr, "START_X",
					PyInt_FromLong( EXPP_MOD_WAVE_STARTX ) );
			PyConstant_Insert( attr, "START_Y",
					PyInt_FromLong( EXPP_MOD_WAVE_STARTY ) );
			PyConstant_Insert( attr, "HEIGHT",
					PyInt_FromLong( EXPP_MOD_WAVE_HEIGHT ) );
			PyConstant_Insert( attr, "WIDTH",
					PyInt_FromLong( EXPP_MOD_WAVE_WIDTH ) );
			PyConstant_Insert( attr, "NARROW",
					PyInt_FromLong( EXPP_MOD_WAVE_NARROW ) );
			PyConstant_Insert( attr, "SPEED",
					PyInt_FromLong( EXPP_MOD_WAVE_SPEED ) );
			PyConstant_Insert( attr, "DAMP",
					PyInt_FromLong( EXPP_MOD_WAVE_DAMP ) );
			PyConstant_Insert( attr, "LIFETIME",
					PyInt_FromLong( EXPP_MOD_WAVE_LIFETIME ) );
			PyConstant_Insert( attr, "TIME_OFFS",
					PyInt_FromLong( EXPP_MOD_WAVE_TIMEOFFS ) );
			PyConstant_Insert( attr, "FLAG",
					PyInt_FromLong( EXPP_MOD_WAVE_FLAG ) );
			break;

    	case eModifierType_Boolean:
			PyConstant_Insert( attr, "OPERATION",
					PyInt_FromLong( EXPP_MOD_BOOLEAN_OPERATION ) );
			PyConstant_Insert( attr, "OBJECT",
					PyInt_FromLong( EXPP_MOD_BOOLEAN_OBJECT ) );

		default:
			break;
	}

	return (PyObject *)attr;
}

static PyObject *subsurf_getter( ModifierData *ptr, int type )
{
	SubsurfModifierData *md = ( SubsurfModifierData *)ptr;

	switch( type ) {
	case EXPP_MOD_SUBSURF_TYPES:
		return PyInt_FromLong( ( long )md->subdivType );
	case EXPP_MOD_SUBSURF_LEVELS:
		return PyInt_FromLong( ( long )md->levels );
	case EXPP_MOD_SUBSURF_RENDLEVELS:
		return PyInt_FromLong( ( long )md->renderLevels );
	case EXPP_MOD_SUBSURF_OPTIMAL:
		return PyBool_FromLong( ( long ) 
				( md->flags & eSubsurfModifierFlag_ControlEdges ) ) ;
	case EXPP_MOD_SUBSURF_UV:
		return PyBool_FromLong( ( long ) 
				( md->flags & eSubsurfModifierFlag_SubsurfUv ) ) ;
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError,
				"key not found" );
	}
}

static int subsurf_setter( ModifierData *ptr, int type,
		PyObject *value )
{
	SubsurfModifierData *md = (SubsurfModifierData *)ptr;

	switch( type ) {
	case EXPP_MOD_SUBSURF_TYPES:
		return EXPP_setIValueRange( value, &md->subdivType, 0, 1, 'h' );
	case EXPP_MOD_SUBSURF_LEVELS:
		return EXPP_setIValueClamped( value, &md->levels, 1, 6, 'h' );
	case EXPP_MOD_SUBSURF_RENDLEVELS:
		return EXPP_setIValueClamped( value, &md->renderLevels, 1, 6, 'h' );
	case EXPP_MOD_SUBSURF_OPTIMAL:
		return EXPP_setBitfield( value, &md->flags,
				eSubsurfModifierFlag_ControlEdges, 'h' );
	case EXPP_MOD_SUBSURF_UV:
		return EXPP_setBitfield( value, &md->flags,
				eSubsurfModifierFlag_SubsurfUv, 'h' );
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}

static PyObject *armature_getter( ModifierData *ptr, int type )
{
	ArmatureModifierData *md = (ArmatureModifierData *)ptr;

	switch( type ) {
	case EXPP_MOD_ARMATURE_OBJECT:
		return Object_CreatePyObject( md->object );
	case EXPP_MOD_ARMATURE_VERTGROUPS:
		return PyBool_FromLong( ( long )( md->deformflag & 1 ) ) ;
	case EXPP_MOD_ARMATURE_ENVELOPES:
		return PyBool_FromLong( ( long )( md->deformflag & 2 ) ) ;
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
	}
}

static int armature_setter( ModifierData *ptr, int type, PyObject *value )
{
	ArmatureModifierData *md = (ArmatureModifierData *)ptr;

	switch( type ) {
	case EXPP_MOD_ARMATURE_OBJECT: {
		Object *obj = (( BPy_Object * )value)->object;
		if( !BPy_Object_Check( value ) || obj->type != OB_ARMATURE )
			return EXPP_ReturnIntError( PyExc_TypeError, 
					"expected BPy armature object argument" );
		md->object = obj;
		return 0;
		}
	case EXPP_MOD_ARMATURE_VERTGROUPS:
		return EXPP_setBitfield( value, &md->deformflag, 1, 'h' );
	case EXPP_MOD_ARMATURE_ENVELOPES:
		return EXPP_setBitfield( value, &md->deformflag, 2, 'h' );
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}

static PyObject *lattice_getter( ModifierData *ptr, int type )
{
	LatticeModifierData *md = (LatticeModifierData *)ptr;

	switch( type ) {
	case EXPP_MOD_LATTICE_OBJECT:
		return Object_CreatePyObject( md->object );
	case EXPP_MOD_LATTICE_VERTGROUP:
		return PyString_FromString( md->name ) ;
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
	}
}

static int lattice_setter( ModifierData *ptr, int type, PyObject *value )
{
	LatticeModifierData *md = (LatticeModifierData *)ptr;

	switch( type ) {
	case EXPP_MOD_LATTICE_OBJECT: {
		Object *obj = (( BPy_Object * )value)->object;
		if( !BPy_Object_Check( value ) || obj->type != OB_LATTICE )
			return EXPP_ReturnIntError( PyExc_TypeError, 
					"expected BPy lattice object argument" );
		md->object = obj;
		break;
		}
	case EXPP_MOD_LATTICE_VERTGROUP: {
		char *name = PyString_AsString( value );
		if( !name )
			return EXPP_ReturnIntError( PyExc_TypeError,
					"expected string arg" );
		BLI_strncpy( md->name, name, sizeof( md->name ) );
		break;
		}
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
	return 0;
}

static PyObject *curve_getter( ModifierData *ptr, int type )
{
	CurveModifierData *md = (CurveModifierData *)ptr;

	switch( type ) {
	case EXPP_MOD_CURVE_OBJECT:
		return Object_CreatePyObject( md->object );
	case EXPP_MOD_CURVE_VERTGROUP:
		return PyString_FromString( md->name ) ;
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
	}
}

static int curve_setter( ModifierData *ptr, int type, PyObject *value )
{
	CurveModifierData *md = (CurveModifierData *)ptr;

	switch( type ) {
	case EXPP_MOD_CURVE_OBJECT: {
		Object *obj = (( BPy_Object * )value)->object;
		if( !BPy_Object_Check( value ) || obj->type != OB_CURVE )
			return EXPP_ReturnIntError( PyExc_TypeError,
					"expected BPy lattice object argument" );
		md->object = obj;
		break;
		}
	case EXPP_MOD_CURVE_VERTGROUP: {
		char *name = PyString_AsString( value );
		if( !name )
			return EXPP_ReturnIntError( PyExc_TypeError,
					"expected string arg" );
		BLI_strncpy( md->name, name, sizeof( md->name ) );
		break;
		}
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
	return 0;
}

static PyObject *build_getter( ModifierData *ptr, int type )
{
	BuildModifierData *md = (BuildModifierData *)ptr;

	switch( type ) {
	case EXPP_MOD_BUILD_START:
		return PyFloat_FromDouble( ( float )md->start );
	case EXPP_MOD_BUILD_LENGTH:
		return PyFloat_FromDouble( ( float )md->length );
	case EXPP_MOD_BUILD_SEED:
		return PyInt_FromLong( ( long )md->seed );
	case EXPP_MOD_BUILD_RANDOMIZE:
		return PyBool_FromLong( ( long )md->randomize ) ;
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
	}
}

static int build_setter( ModifierData *ptr, int type, PyObject *value )
{
	BuildModifierData *md = (BuildModifierData *)ptr;

	switch( type ) {
	case EXPP_MOD_BUILD_START:
		return EXPP_setFloatClamped( value, &md->start, 1.0, MAXFRAMEF );
	case EXPP_MOD_BUILD_LENGTH:
		return EXPP_setFloatClamped( value, &md->length, 1.0, MAXFRAMEF );
	case EXPP_MOD_BUILD_SEED:
		return EXPP_setIValueClamped( value, &md->seed, 1, MAXFRAME, 'i' );
	case EXPP_MOD_BUILD_RANDOMIZE:
		return EXPP_setBitfield( value, &md->randomize, 1, 'i' );
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}

static PyObject *mirror_getter( ModifierData *ptr, int type )
{
	MirrorModifierData *md = (MirrorModifierData *)ptr;

	switch( type ) {
	case EXPP_MOD_MIRROR_LIMIT:
		return PyFloat_FromDouble( (double)md->tolerance );
	case EXPP_MOD_MIRROR_FLAG:
		return PyBool_FromLong( (long)( md->flag & MOD_MIR_CLIPPING ) ) ;
	case EXPP_MOD_MIRROR_AXIS:
		return PyInt_FromLong( (long)md->axis );
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
	}
}

static int mirror_setter( ModifierData *ptr, int type, PyObject *value )
{
	MirrorModifierData *md = (MirrorModifierData *)ptr;

	switch( type ) {
	case EXPP_MOD_MIRROR_LIMIT:
		return EXPP_setFloatClamped( value, &md->tolerance, 0.0, 1.0 );
	case EXPP_MOD_MIRROR_FLAG:
		return EXPP_setBitfield( value, &md->flag, MOD_MIR_CLIPPING, 'i' );
	case EXPP_MOD_MIRROR_AXIS:
		return EXPP_setIValueRange( value, &md->axis, 0, 2, 'h' );
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}

static PyObject *decimate_getter( ModifierData *ptr, int type )
{
	DecimateModifierData *md = (DecimateModifierData *)ptr;

	if( type == EXPP_MOD_DECIMATE_RATIO )
		return PyFloat_FromDouble( (double)md->percent );
	else if( type == EXPP_MOD_DECIMATE_COUNT )
		return PyInt_FromLong( (long)md->faceCount );
	return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
}

static int decimate_setter( ModifierData *ptr, int type, PyObject *value )
{
	DecimateModifierData *md = (DecimateModifierData *)ptr;

	if( type == EXPP_MOD_DECIMATE_RATIO )
		return EXPP_setFloatClamped( value, &md->percent, 0.0, 1.0 );
	else if( type == EXPP_MOD_DECIMATE_COUNT )
		return EXPP_ReturnIntError( PyExc_AttributeError,
				"value is read-only" );
	return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
}

static PyObject *wave_getter( ModifierData *ptr, int type )
{
	WaveModifierData *md = (WaveModifierData *)ptr;

	switch( type ) {
	case EXPP_MOD_WAVE_STARTX:
		return PyFloat_FromDouble( (double)md->startx );
	case EXPP_MOD_WAVE_STARTY:
		return PyFloat_FromDouble( (double)md->starty );
	case EXPP_MOD_WAVE_HEIGHT:
		return PyFloat_FromDouble( (double)md->height );
	case EXPP_MOD_WAVE_WIDTH:
		return PyFloat_FromDouble( (double)md->width );
	case EXPP_MOD_WAVE_NARROW:
		return PyFloat_FromDouble( (double)md->narrow );
	case EXPP_MOD_WAVE_SPEED:
		return PyFloat_FromDouble( (double)md->speed );
	case EXPP_MOD_WAVE_DAMP:
		return PyFloat_FromDouble( (double)md->damp );
	case EXPP_MOD_WAVE_LIFETIME:
		return PyFloat_FromDouble( (double)md->lifetime );
	case EXPP_MOD_WAVE_TIMEOFFS:
		return PyFloat_FromDouble( (double)md->timeoffs );
	case EXPP_MOD_WAVE_FLAG:
		return PyInt_FromLong( (long)md->flag );
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
	}
}

static int wave_setter( ModifierData *ptr, int type, PyObject *value )
{
	WaveModifierData *md = (WaveModifierData *)ptr;

	switch( type ) {
	case EXPP_MOD_WAVE_STARTX:
		return EXPP_setFloatClamped( value, &md->startx, -100.0, 100.0 );
	case EXPP_MOD_WAVE_STARTY:
		return EXPP_setFloatClamped( value, &md->starty, -100.0, 100.0 );
	case EXPP_MOD_WAVE_HEIGHT:
		return EXPP_setFloatClamped( value, &md->height, -2.0, 2.0 );
	case EXPP_MOD_WAVE_WIDTH:
		return EXPP_setFloatClamped( value, &md->width, 0.0, 5.0 );
	case EXPP_MOD_WAVE_NARROW:
		return EXPP_setFloatClamped( value, &md->width, 0.0, 5.0 );
	case EXPP_MOD_WAVE_SPEED:
		return EXPP_setFloatClamped( value, &md->speed, -2.0, 2.0 );
	case EXPP_MOD_WAVE_DAMP:
		return EXPP_setFloatClamped( value, &md->damp, -1000.0, 1000.0 );
	case EXPP_MOD_WAVE_LIFETIME:
		return EXPP_setFloatClamped( value, &md->lifetime, -1000.0, 1000.0 );
	case EXPP_MOD_WAVE_TIMEOFFS:
		return EXPP_setFloatClamped( value, &md->timeoffs, -1000.0, 1000.0 );
	case EXPP_MOD_WAVE_FLAG:
		return EXPP_setIValueRange( value, &md->flag, 0, 
				WAV_X+WAV_Y+WAV_CYCL, 'h' );
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}

static PyObject *boolean_getter( ModifierData *ptr, int type )
{
	BooleanModifierData *md = (BooleanModifierData *)ptr;

	if( type == EXPP_MOD_BOOLEAN_OBJECT )
		return Object_CreatePyObject( md->object );
	else if( type == EXPP_MOD_BOOLEAN_OPERATION )
		return PyInt_FromLong( ( long )md->operation ) ;

	return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
}

static int boolean_setter( ModifierData *ptr, int type, PyObject *value )
{
	BooleanModifierData *md = (BooleanModifierData *)ptr;

	if( type == EXPP_MOD_BOOLEAN_OBJECT ) {
		Object *obj = (( BPy_Object * )value)->object;
		if( !BPy_Object_Check( value ) || obj->type != OB_MESH )
			return EXPP_ReturnIntError( PyExc_TypeError,
					"expected BPy mesh object argument" );
		md->object = obj;
		return 0;
	} else if( type == EXPP_MOD_BOOLEAN_OPERATION )
		return EXPP_setIValueRange( value, &md->operation, 
				eBooleanModifierOp_Intersect, eBooleanModifierOp_Difference,
				'h' );

	return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
}

static PyObject *hook_getter( ModifierData *ptr, int type )
{
	Py_RETURN_NONE;
}

static int hook_setter( ModifierData *ptr, int type, PyObject *value )
{
	return 0;
}

static PyObject *softbody_getter( ModifierData *ptr, int type )
{
	Py_RETURN_NONE;
}

static int softbody_setter( ModifierData *ptr, int type, PyObject *value )
{
	return 0;
}

static PyObject *array_getter( ModifierData *ptr, int type )
{
	Py_RETURN_NONE;
}

static int array_setter( ModifierData *ptr, int type, PyObject *value )
{
	return 0;
}

static PyObject *Modifier_getData( BPy_Modifier * self, PyObject * key )
{
	int type;

	if( !PyInt_CheckExact( key ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected string arg" );

	type = PyInt_AsLong( key );
	switch( type ) {
	case EXPP_MOD_RENDER:
		return EXPP_getBitfield( &self->md->mode, eModifierMode_Render, 'h' );
	case EXPP_MOD_REALTIME:
		return EXPP_getBitfield( &self->md->mode, eModifierMode_Realtime, 'h' );
	case EXPP_MOD_EDITMODE:
		return EXPP_getBitfield( &self->md->mode, eModifierMode_Editmode, 'h' );
	case EXPP_MOD_ONCAGE:
		return EXPP_getBitfield( &self->md->mode, eModifierMode_OnCage, 'h' );
	default:
		switch( self->md->type ) {
			case eModifierType_Subsurf:
				return subsurf_getter( self->md, type );
			case eModifierType_Armature:
				return armature_getter( self->md, type );
			case eModifierType_Lattice:
				return lattice_getter( self->md, type );
			case eModifierType_Curve:
				return curve_getter( self->md, type );
			case eModifierType_Build:
				return build_getter( self->md, type );
			case eModifierType_Mirror:
				return mirror_getter( self->md, type );
			case eModifierType_Decimate:
				return decimate_getter( self->md, type );
			case eModifierType_Wave:
				return wave_getter( self->md, type );
			case eModifierType_Hook:
				return hook_getter( self->md, type );
			case eModifierType_Softbody:
				return softbody_getter( self->md, type );
			case eModifierType_Boolean:
				return boolean_getter( self->md, type );
			case eModifierType_Array:
				return array_getter( self->md, type );
			case eModifierType_None:
				Py_RETURN_NONE;
		}
	}
	return EXPP_ReturnPyObjError( PyExc_KeyError,
			"unknown key or modifier type" );
}

static int Modifier_setData( BPy_Modifier * self, PyObject * key, 
		PyObject * arg )
{
	int type;

	if( !PyNumber_Check( key ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected string arg" );

	type = PyInt_AsLong( key );
	switch( self->md->type ) {
    	case eModifierType_Subsurf:
			return subsurf_setter( self->md, type, arg );
    	case eModifierType_Armature:
			return armature_setter( self->md, type, arg );
    	case eModifierType_Lattice:
			return lattice_setter( self->md, type, arg );
    	case eModifierType_Curve:
			return curve_setter( self->md, type, arg );
    	case eModifierType_Build:
			return build_setter( self->md, type, arg );
    	case eModifierType_Mirror:
			return mirror_setter( self->md, type, arg );
		case eModifierType_Decimate:
			return decimate_setter( self->md, type, arg );
		case eModifierType_Wave:
			return wave_setter( self->md, type, arg );
		case eModifierType_Hook:
			return hook_setter( self->md, type, arg );
		case eModifierType_Softbody:
			return softbody_setter( self->md, type, arg );
		case eModifierType_Boolean:
			return boolean_setter( self->md, type, arg );
		case eModifierType_Array:
			return array_setter( self->md, type, arg );
		case eModifierType_None:
			return 0;
	}
	return EXPP_ReturnIntError( PyExc_RuntimeError,
			"unsupported modifier type" );
} 

/*****************************************************************************/
/* Function:    Modifier_repr                                                */
/* Description: This is a callback function for the BPy_Modifier type. It    */
/*              builds a meaningful string to represent modifier objects.    */
/*****************************************************************************/
static PyObject *Modifier_repr( BPy_Modifier * self )
{
	ModifierTypeInfo *mti;
	if (self->md==NULL)
		return PyString_FromString( "[Modifier - Removed");
	
	mti= modifierType_getInfo(self->md->type);
	return PyString_FromFormat( "[Modifier \"%s\", Type \"%s\"]", self->md->name, mti->name );
}

/* Three Python Modifier_Type helper functions needed by the Object module: */

/*****************************************************************************/
/* Function:    Modifier_CreatePyObject                                      */
/* Description: This function will create a new BPy_Modifier from an         */
/*              existing Blender modifier structure.                         */
/*****************************************************************************/
PyObject *Modifier_CreatePyObject( Object *obj, ModifierData * md )
{
	BPy_Modifier *pymod;
	pymod = ( BPy_Modifier * ) PyObject_NEW( BPy_Modifier, &Modifier_Type );
	if( !pymod )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create BPy_Modifier object" );
	pymod->md = md;
	pymod->obj = obj;
	return ( PyObject * ) pymod;
}

/*****************************************************************************/
/* Function:    Modifier_CheckPyObject                                       */
/* Description: This function returns true when the given PyObject is of the */
/*              type Modifier. Otherwise it will return false.               */
/*****************************************************************************/
int Modifier_CheckPyObject( PyObject * pyobj )
{
	return ( pyobj->ob_type == &Modifier_Type );
}

/*****************************************************************************/
/* Function:    Modifier_FromPyObject                                        */
/* Description: This function returns the Blender modifier from the given    */
/*              PyObject.                                                    */
/*****************************************************************************/
ModifierData *Modifier_FromPyObject( PyObject * pyobj )
{
	return ( ( BPy_Modifier * ) pyobj )->md;
}

/*****************************************************************************/
/* Modifier Sequence wrapper                                                 */
/*****************************************************************************/

/*
 * Initialize the interator
 */

static PyObject *ModSeq_getIter( BPy_ModSeq * self )
{
	self->iter = (ModifierData *)self->obj->modifiers.first;
	return EXPP_incr_ret ( (PyObject *) self );
}

/*
 * Get the next Modifier
 */

static PyObject *ModSeq_nextIter( BPy_ModSeq * self )
{
	ModifierData *this = self->iter;
	if( this ) {
		self->iter = this->next;
		return Modifier_CreatePyObject( self->obj, this );
	}

	return EXPP_ReturnPyObjError( PyExc_StopIteration,
			"iterator at end" );
}

/* return the number of modifiers */

static int ModSeq_length( BPy_ModSeq * self )
{
	return BLI_countlist( &self->obj->modifiers );
}

/* return a modifier */

static PyObject *ModSeq_item( BPy_ModSeq * self, int i )
{
	ModifierData *md = NULL;

	/* if index is negative, start counting from the end of the list */
	if( i < 0 )
		i += ModSeq_length( self );

	/* skip through the list until we get the modifier or end of list */

	for( md = self->obj->modifiers.first; i && md; --i ) md = md->next;

	if( md )
		return Modifier_CreatePyObject( self->obj, md );
	else
		return EXPP_ReturnPyObjError( PyExc_IndexError,
				"array index out of range" );
}

/*****************************************************************************/
/* Python BPy_ModSeq sequence table:                                         */
/*****************************************************************************/
static PySequenceMethods ModSeq_as_sequence = {
	( inquiry ) ModSeq_length,	/* sq_length */
	( binaryfunc ) 0,	/* sq_concat */
	( intargfunc ) 0,	/* sq_repeat */
	( intargfunc ) ModSeq_item,	/* sq_item */
	( intintargfunc ) 0,	/* sq_slice */
	( intobjargproc ) 0,	/* sq_ass_item */
	( intintobjargproc ) 0,	/* sq_ass_slice */
	( objobjproc ) 0,	/* sq_contains */
	( binaryfunc ) 0,		/* sq_inplace_concat */
	( intargfunc ) 0,		/* sq_inplace_repeat */
};

/* create a new modifier at the end of the list */

static PyObject *ModSeq_append( BPy_ModSeq *self, PyObject *args )
{
	int type;

	if( !PyArg_ParseTuple( args, "i", &type ) )
		EXPP_ReturnPyObjError( PyExc_TypeError, "expected int argument" );
	
	BLI_addtail(&self->obj->modifiers, modifier_new(type));
	return Modifier_CreatePyObject( self->obj, self->obj->modifiers.last );
}

/* remove an existing modifier a new modifier at the end of the list */
static PyObject *ModSeq_remove( BPy_ModSeq *self, PyObject *args )
{
	PyObject *pyobj;
	Object *obj;
	ModifierData *md_v, *md;
	if( !PyArg_ParseTuple( args, "O!", &Modifier_Type, &pyobj ) ) {
		return ( EXPP_ReturnPyObjError( PyExc_TypeError, "expected a modifier as an argument" ) );
	}
	obj = ( ( BPy_Modifier * ) pyobj )->obj;
	md_v = ( ( BPy_Modifier * ) pyobj )->md;
	
	
	if (md_v==NULL)
		return (EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "This modifier has alredy been removed!" ));
	
	for (md=obj->modifiers.first; md; md=md->next)
		if (md==md_v)
			break;
	
	if (!md)
		return (EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "This modifier is not in its object list, this should never happen!" ));
	
	BLI_remlink(&(obj->modifiers), md_v);
	modifier_free(md_v);
	( ( BPy_Modifier * ) pyobj )->md= NULL;
	return EXPP_incr_ret( Py_None );
}



/*
 * simple method to implement pseudo module constants
 */

static PyObject *ModSeq_typeConst( BPy_Modifier *self_unused, void *type )
{
	return PyInt_FromLong( (long)type );
}

/*****************************************************************************/
/* Python BPy_ModSeq attributes get/set structure:                           */
/*****************************************************************************/
static PyGetSetDef BPy_ModSeq_getseters[] = {
	{"SUBSURF",
	 (getter)ModSeq_typeConst, (setter)NULL,
	 NULL, (void *)eModifierType_Subsurf},
	{"ARMATURE",
	 (getter)ModSeq_typeConst, (setter)NULL,
	 NULL, (void *)eModifierType_Armature},
	{"LATTICE",
	 (getter)ModSeq_typeConst, (setter)NULL,
	 NULL, (void *)eModifierType_Lattice},
	{"CURVE",
	 (getter)ModSeq_typeConst, (setter)NULL,
	 NULL, (void *)eModifierType_Curve},
	{"BUILD",
	 (getter)ModSeq_typeConst, (setter)NULL,
	 NULL, (void *)eModifierType_Build},
	{"MIRROR",
	 (getter)ModSeq_typeConst, (setter)NULL,
	 NULL, (void *)eModifierType_Mirror},
	{"DECIMATE",
	 (getter)ModSeq_typeConst, (setter)NULL,
	 NULL, (void *)eModifierType_Decimate},
	{"WAVE",
	 (getter)ModSeq_typeConst, (setter)NULL,
	 NULL, (void *)eModifierType_Wave},
	{"BOOLEAN",
	 (getter)ModSeq_typeConst, (setter)NULL,
	 NULL, (void *)eModifierType_Boolean},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/*****************************************************************************/
/* Python BPy_ModSeq methods table:                                          */
/*****************************************************************************/
static PyMethodDef BPy_ModSeq_methods[] = {
	/* name, method, flags, doc */
	{"append", ( PyCFunction ) ModSeq_append, METH_VARARGS,
	 "(type) - add a new modifier, where type is the type of modifier"},
	{"remove", ( PyCFunction ) ModSeq_remove, METH_VARARGS,
	 "(modifier) - remove an existing modifier, where modifier is a modifier from this object."},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python ModSeq_Type structure definition:                                  */
/*****************************************************************************/
PyTypeObject ModSeq_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender Modifier Sequence",/* char *tp_name; */
	sizeof( BPy_ModSeq ),     /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) PyObject_Del,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	( reprfunc ) NULL,          /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	&ModSeq_as_sequence,        /* PySequenceMethods *tp_as_sequence; */
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
	( getiterfunc )ModSeq_getIter, /* getiterfunc tp_iter; */
    ( iternextfunc )ModSeq_nextIter, /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_ModSeq_methods,         /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_ModSeq_getseters,       /* struct PyGetSetDef *tp_getset; */
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
/* Function:    ModSeq_CreatePyObject                                        */
/* Description: This function will create a new BPy_ModSeq from an existing  */
/*              ListBase structure.                                          */
/*****************************************************************************/
PyObject *ModSeq_CreatePyObject( Object *obj )
{
	BPy_ModSeq *pymod;
	pymod = ( BPy_ModSeq * ) PyObject_NEW( BPy_ModSeq, &ModSeq_Type );
	if( !pymod )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create BPy_ModSeq object" );
	pymod->obj = obj;
	return ( PyObject * ) pymod;
}
