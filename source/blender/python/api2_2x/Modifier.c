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
 * This is a new part of Blender.
 *
 * Contributor(s): Ken Hughes, Campbell Barton
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/* TODO, accessing a modifier sequence of a deleted object will crash blender at the moment, not sure how to fix this. */


#include "Modifier.h" /*This must come first*/

#include "DNA_object_types.h"
#include "DNA_effect_types.h"
#include "DNA_armature_types.h"
#include "DNA_vec_types.h"

#include "BKE_main.h"
#include "BKE_global.h"
#include "BKE_modifier.h"
#include "BKE_library.h"
#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "MEM_guardedalloc.h"
#include "BDR_editobject.h"
#include "butspace.h"
#include "blendef.h"
#include "mydevice.h"

#include "Object.h"
#include "Texture.h"
#include "Mathutils.h"
#include "gen_utils.h"
#include "gen_library.h"

/* checks for the scene being removed */
#define MODIFIER_DEL_CHECK_PY(bpy_modifier) if (!(bpy_modifier->md)) return ( EXPP_ReturnPyObjError( PyExc_RuntimeError, "Modifier has been removed" ) )
#define MODIFIER_DEL_CHECK_INT(bpy_modifier) if (!(bpy_modifier->md)) return ( EXPP_ReturnIntError( PyExc_RuntimeError, "Modifier has been removed" ) )

enum mod_constants {
	/*Apply to all modifiers*/
	EXPP_MOD_RENDER = 0,
	EXPP_MOD_REALTIME,
	EXPP_MOD_EDITMODE,
	EXPP_MOD_ONCAGE,
	
	/*GENERIC*/
	EXPP_MOD_OBJECT, /*ARMATURE, LATTICE, CURVE, BOOLEAN, ARRAY*/
	EXPP_MOD_VERTGROUP, /*ARMATURE, LATTICE, CURVE, SMOOTH, CAST*/
	EXPP_MOD_LIMIT, /*ARRAY, MIRROR*/
	EXPP_MOD_FLAG, /*MIRROR, WAVE*/
	EXPP_MOD_COUNT, /*DECIMATOR, ARRAY*/
	EXPP_MOD_LENGTH, /*BUILD, ARRAY*/
	EXPP_MOD_FACTOR, /*SMOOTH, CAST*/
	EXPP_MOD_ENABLE_X, /*SMOOTH, CAST*/
	EXPP_MOD_ENABLE_Y, /*SMOOTH, CAST*/
	EXPP_MOD_ENABLE_Z, /*SMOOTH, CAST*/
	EXPP_MOD_TYPES, /*SUBSURF, CAST*/
	
	/*SUBSURF SPECIFIC*/
	EXPP_MOD_LEVELS,
	EXPP_MOD_RENDLEVELS,
	EXPP_MOD_OPTIMAL,
	EXPP_MOD_UV,
	
	/*ARMATURE SPECIFIC*/
	EXPP_MOD_ENVELOPES,
	
	/*ARRAY SPECIFIC*/
	EXPP_MOD_OBJECT_OFFSET,
	EXPP_MOD_OBJECT_CURVE,
	EXPP_MOD_OFFSET_VEC,
	EXPP_MOD_SCALE_VEC,
	EXPP_MOD_MERGE_DIST,
	
	/*BUILD SPECIFIC*/
	EXPP_MOD_START,
	EXPP_MOD_SEED,
	EXPP_MOD_RANDOMIZE,

	/*MIRROR SPECIFIC*/
	EXPP_MOD_AXIS_X,
	EXPP_MOD_AXIS_Y,
	EXPP_MOD_AXIS_Z,

	/*DECIMATE SPECIFIC*/
	EXPP_MOD_RATIO,

	/*WAVE SPECIFIC*/
	EXPP_MOD_STARTX,
	EXPP_MOD_STARTY,
	EXPP_MOD_HEIGHT,
	EXPP_MOD_WIDTH,
	EXPP_MOD_NARROW,
	EXPP_MOD_SPEED,
	EXPP_MOD_DAMP,
	EXPP_MOD_LIFETIME,
	EXPP_MOD_TIMEOFFS,
	
	/*BOOLEAN SPECIFIC*/
	EXPP_MOD_OPERATION,

	/*EDGE SPLIT SPECIFIC */
	EXPP_MOD_EDGESPLIT_ANGLE,
	EXPP_MOD_EDGESPLIT_FROM_ANGLE,
	EXPP_MOD_EDGESPLIT_FROM_SHARP,
	
	/* DISPLACE */
	EXPP_MOD_UVLAYER,
	EXPP_MOD_MID_LEVEL,
	EXPP_MOD_STRENGTH,
	EXPP_MOD_TEXTURE,
	EXPP_MOD_MAPPING,
	EXPP_MOD_DIRECTION,

	/* SMOOTH */
	EXPP_MOD_REPEAT,

	/* CAST */
	EXPP_MOD_RADIUS,
	EXPP_MOD_SIZE,
	EXPP_MOD_USE_OB_TRANSFORM,
	EXPP_MOD_SIZE_FROM_RADIUS
	
	/* yet to be implemented */
	/* EXPP_MOD_HOOK_,*/
	/* , */
};

/*****************************************************************************/
/* Python BPy_Modifier methods declarations:                                 */
/*****************************************************************************/
static PyObject *Modifier_getName( BPy_Modifier * self );
static int Modifier_setName( BPy_Modifier * self, PyObject *arg );
static PyObject *Modifier_getType( BPy_Modifier * self );
static PyObject *Modifier_reset( BPy_Modifier * self );

static PyObject *Modifier_getData( BPy_Modifier * self, PyObject * key );
static int Modifier_setData( BPy_Modifier * self, PyObject * key, 
		PyObject * value );

/*****************************************************************************/
/* Python BPy_Modifier methods table:                                        */
/*****************************************************************************/
static PyMethodDef BPy_Modifier_methods[] = {
	/* name, method, flags, doc */
	{"reset", (PyCFunction)Modifier_reset, METH_NOARGS,
		"resets a hook modifier location"},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python BPy_Modifier attributes get/set structure:                         */
/*****************************************************************************/
static PyGetSetDef BPy_Modifier_getseters[] = {
	{"name",
	(getter)Modifier_getName, (setter)Modifier_setName,
	 "Modifier name", NULL},
	{"type",
	(getter)Modifier_getType, (setter)NULL,
	 "Modifier type (read only)", NULL},
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

	NULL,						/* destructor tp_dealloc; */
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
	MODIFIER_DEL_CHECK_PY(self);
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

	MODIFIER_DEL_CHECK_INT(self);
	
	BLI_strncpy( self->md->name, name, sizeof( self->md->name ) );

	return 0;
}

/*
 * return the type of this modifier
 */

static PyObject *Modifier_getType( BPy_Modifier * self )
{
	MODIFIER_DEL_CHECK_PY(self);
	
	return PyInt_FromLong( self->md->type );
}

static PyObject *subsurf_getter( BPy_Modifier * self, int type )
{
	SubsurfModifierData *md = ( SubsurfModifierData *)(self->md);

	switch( type ) {
	case EXPP_MOD_TYPES:
		return PyInt_FromLong( ( long )md->subdivType );
	case EXPP_MOD_LEVELS:
		return PyInt_FromLong( ( long )md->levels );
	case EXPP_MOD_RENDLEVELS:
		return PyInt_FromLong( ( long )md->renderLevels );
	case EXPP_MOD_OPTIMAL:
		return PyBool_FromLong( ( long ) 
				( md->flags & eSubsurfModifierFlag_ControlEdges ) ) ;
	case EXPP_MOD_UV:
		return PyBool_FromLong( ( long ) 
				( md->flags & eSubsurfModifierFlag_SubsurfUv ) ) ;
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError,
				"key not found" );
	}
}

static int subsurf_setter( BPy_Modifier * self, int type,
		PyObject *value )
{
	SubsurfModifierData *md = (SubsurfModifierData *)(self->md);

	switch( type ) {
	case EXPP_MOD_TYPES:
		return EXPP_setIValueRange( value, &md->subdivType, 0, 1, 'h' );
	case EXPP_MOD_LEVELS:
		return EXPP_setIValueClamped( value, &md->levels, 1, 6, 'h' );
	case EXPP_MOD_RENDLEVELS:
		return EXPP_setIValueClamped( value, &md->renderLevels, 1, 6, 'h' );
	case EXPP_MOD_OPTIMAL:
		return EXPP_setBitfield( value, &md->flags,
				eSubsurfModifierFlag_ControlEdges, 'h' );
	case EXPP_MOD_UV:
		return EXPP_setBitfield( value, &md->flags,
				eSubsurfModifierFlag_SubsurfUv, 'h' );
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}

static PyObject *armature_getter( BPy_Modifier * self, int type )
{
	ArmatureModifierData *md = (ArmatureModifierData *)(self->md);

	switch( type ) {
	case EXPP_MOD_OBJECT:
		return Object_CreatePyObject( md->object );
	case EXPP_MOD_VERTGROUP:
		return PyBool_FromLong( ( long )( md->deformflag & 1 ) ) ;
	case EXPP_MOD_ENVELOPES:
		return PyBool_FromLong( ( long )( md->deformflag & 2 ) ) ;
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
	}
}

static int armature_setter( BPy_Modifier *self, int type, PyObject *value )
{
	ArmatureModifierData *md = (ArmatureModifierData *)(self->md);

	switch( type ) {
	case EXPP_MOD_OBJECT: 
		return GenericLib_assignData(value, (void **) &md->object, 0, 0, ID_OB, OB_ARMATURE);
	case EXPP_MOD_VERTGROUP:
		return EXPP_setBitfield( value, &md->deformflag, 1, 'h' );
	case EXPP_MOD_ENVELOPES:
		return EXPP_setBitfield( value, &md->deformflag, 2, 'h' );
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}

static PyObject *lattice_getter( BPy_Modifier * self, int type )
{
	LatticeModifierData *md = (LatticeModifierData *)(self->md);

	switch( type ) {
	case EXPP_MOD_OBJECT:
		return Object_CreatePyObject( md->object );
	case EXPP_MOD_VERTGROUP:
		return PyString_FromString( md->name ) ;
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
	}
}

static int lattice_setter( BPy_Modifier *self, int type, PyObject *value )
{
	LatticeModifierData *md = (LatticeModifierData *)(self->md);

	switch( type ) {
	case EXPP_MOD_OBJECT:
		return GenericLib_assignData(value, (void **) &md->object, (void **) &self->object, 0, ID_OB, OB_LATTICE);
	case EXPP_MOD_VERTGROUP: {
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

static PyObject *curve_getter( BPy_Modifier * self, int type )
{
	CurveModifierData *md = (CurveModifierData *)(self->md);

	switch( type ) {
	case EXPP_MOD_OBJECT:
		return Object_CreatePyObject( md->object );
	case EXPP_MOD_VERTGROUP:
		return PyString_FromString( md->name ) ;
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
	}
}

static int curve_setter( BPy_Modifier *self, int type, PyObject *value )
{
	CurveModifierData *md = (CurveModifierData *)(self->md);

	switch( type ) {
	case EXPP_MOD_OBJECT:
		return GenericLib_assignData(value, (void **) &md->object, (void **) &self->object, 0, ID_OB, OB_CURVE);
	case EXPP_MOD_VERTGROUP: {
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

static PyObject *build_getter( BPy_Modifier * self, int type )
{
	BuildModifierData *md = (BuildModifierData *)(self->md);

	switch( type ) {
	case EXPP_MOD_START:
		return PyFloat_FromDouble( ( float )md->start );
	case EXPP_MOD_LENGTH:
		return PyFloat_FromDouble( ( float )md->length );
	case EXPP_MOD_SEED:
		return PyInt_FromLong( ( long )md->seed );
	case EXPP_MOD_RANDOMIZE:
		return PyBool_FromLong( ( long )md->randomize ) ;
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
	}
}

static int build_setter( BPy_Modifier *self, int type, PyObject *value )
{
	BuildModifierData *md = (BuildModifierData *)(self->md);

	switch( type ) {
	case EXPP_MOD_START:
		return EXPP_setFloatClamped( value, &md->start, 1.0, MAXFRAMEF );
	case EXPP_MOD_LENGTH:
		return EXPP_setFloatClamped( value, &md->length, 1.0, MAXFRAMEF );
	case EXPP_MOD_SEED:
		return EXPP_setIValueClamped( value, &md->seed, 1, MAXFRAME, 'i' );
	case EXPP_MOD_RANDOMIZE:
		return EXPP_setBitfield( value, &md->randomize, 1, 'i' );
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}

static PyObject *mirror_getter( BPy_Modifier * self, int type )
{
	MirrorModifierData *md = (MirrorModifierData *)(self->md);

	switch( type ) {
	case EXPP_MOD_LIMIT:
		return PyFloat_FromDouble( (double)md->tolerance );
	case EXPP_MOD_FLAG:
		return PyBool_FromLong( (long)( md->flag & MOD_MIR_CLIPPING ) ) ;
	case EXPP_MOD_AXIS_X:
		return PyBool_FromLong( ( long ) 
				( md->flag & MOD_MIR_AXIS_X ) ) ;
	case EXPP_MOD_AXIS_Y:
		return PyBool_FromLong( ( long ) 
				( md->flag & MOD_MIR_AXIS_Y ) ) ;
	case EXPP_MOD_AXIS_Z:
		return PyBool_FromLong( ( long ) 
				( md->flag & MOD_MIR_AXIS_Z ) ) ;
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
	}
}

static int mirror_setter( BPy_Modifier *self, int type, PyObject *value )
{
	MirrorModifierData *md = (MirrorModifierData *)(self->md);

	switch( type ) {
	case EXPP_MOD_LIMIT:
		return EXPP_setFloatClamped( value, &md->tolerance, 0.0, 1.0 );
	case EXPP_MOD_FLAG:
		return EXPP_setBitfield( value, &md->flag, MOD_MIR_CLIPPING, 'i' );
	case EXPP_MOD_AXIS_X:
		return EXPP_setBitfield( value, &md->flag, MOD_MIR_AXIS_X, 'h' );
	case EXPP_MOD_AXIS_Y:
		return EXPP_setBitfield( value, &md->flag, MOD_MIR_AXIS_Y, 'h' );
	case EXPP_MOD_AXIS_Z:
		return EXPP_setBitfield( value, &md->flag, MOD_MIR_AXIS_Z, 'h' );
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}

static PyObject *decimate_getter( BPy_Modifier * self, int type )
{
	DecimateModifierData *md = (DecimateModifierData *)(self->md);

	if( type == EXPP_MOD_RATIO )
		return PyFloat_FromDouble( (double)md->percent );
	else if( type == EXPP_MOD_COUNT )
		return PyInt_FromLong( (long)md->faceCount );
	return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
}

static int decimate_setter( BPy_Modifier *self, int type, PyObject *value )
{
	DecimateModifierData *md = (DecimateModifierData *)(self->md);

	if( type == EXPP_MOD_RATIO )
		return EXPP_setFloatClamped( value, &md->percent, 0.0, 1.0 );
	else if( type == EXPP_MOD_COUNT )
		return EXPP_ReturnIntError( PyExc_AttributeError,
				"value is read-only" );
	return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
}

static PyObject *smooth_getter( BPy_Modifier * self, int type )
{
	SmoothModifierData *md = (SmoothModifierData *)(self->md);

	switch( type ) {
	case EXPP_MOD_FACTOR:
		return PyFloat_FromDouble( (double)md->fac );
	case EXPP_MOD_REPEAT:
		return PyInt_FromLong( (long)md->repeat );
	case EXPP_MOD_VERTGROUP:
		return PyString_FromString( md->defgrp_name ) ;
	case EXPP_MOD_ENABLE_X:
		return EXPP_getBitfield( &md->flag, MOD_SMOOTH_X, 'h' );
	case EXPP_MOD_ENABLE_Y:
		return EXPP_getBitfield( &md->flag, MOD_SMOOTH_Y, 'h' );
	case EXPP_MOD_ENABLE_Z:
		return EXPP_getBitfield( &md->flag, MOD_SMOOTH_Z, 'h' );
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
	}
}

static int smooth_setter( BPy_Modifier *self, int type, PyObject *value )
{
	SmoothModifierData *md = (SmoothModifierData *)(self->md);

	switch( type ) {
	case EXPP_MOD_FACTOR:
		return EXPP_setFloatClamped( value, &md->fac, -10.0, 10.0 );
	case EXPP_MOD_REPEAT:
		return EXPP_setIValueRange( value, &md->repeat, 0, 30, 'h' );
	case EXPP_MOD_VERTGROUP: {
		char *name = PyString_AsString( value );
		if( !name ) return EXPP_ReturnIntError( PyExc_TypeError, "expected string arg" );
		BLI_strncpy( md->defgrp_name, name, sizeof( md->defgrp_name ) );
		return 0;
	}
	case EXPP_MOD_ENABLE_X:
		return EXPP_setBitfield( value, &md->flag, MOD_SMOOTH_X, 'h' ); 
	case EXPP_MOD_ENABLE_Y:
		return EXPP_setBitfield( value, &md->flag, MOD_SMOOTH_Y, 'h' ); 
	case EXPP_MOD_ENABLE_Z:
		return EXPP_setBitfield( value, &md->flag, MOD_SMOOTH_Z, 'h' ); 
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}

static PyObject *cast_getter( BPy_Modifier * self, int type )
{
	CastModifierData *md = (CastModifierData *)(self->md);

	switch( type ) {
	case EXPP_MOD_TYPES:
		return PyInt_FromLong( (long)md->type );
	case EXPP_MOD_FACTOR:
		return PyFloat_FromDouble( (double)md->fac );
	case EXPP_MOD_RADIUS:
		return PyFloat_FromDouble( (double)md->radius );
	case EXPP_MOD_SIZE:
		return PyFloat_FromDouble( (double)md->size );
	case EXPP_MOD_OBJECT:
		return Object_CreatePyObject( md->object );
	case EXPP_MOD_VERTGROUP:
		return PyString_FromString( md->defgrp_name ) ;
	case EXPP_MOD_ENABLE_X:
		return EXPP_getBitfield( &md->flag, MOD_CAST_X, 'h' );
	case EXPP_MOD_ENABLE_Y:
		return EXPP_getBitfield( &md->flag, MOD_CAST_Y, 'h' );
	case EXPP_MOD_ENABLE_Z:
		return EXPP_getBitfield( &md->flag, MOD_CAST_Z, 'h' );
	case EXPP_MOD_USE_OB_TRANSFORM:
		return EXPP_getBitfield( &md->flag, MOD_CAST_USE_OB_TRANSFORM, 'h' );
	case EXPP_MOD_SIZE_FROM_RADIUS:
		return EXPP_getBitfield( &md->flag, MOD_CAST_SIZE_FROM_RADIUS, 'h' );
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
	}
}

static int cast_setter( BPy_Modifier *self, int type, PyObject *value )
{
	CastModifierData *md = (CastModifierData *)(self->md);

	switch( type ) {
	case EXPP_MOD_TYPES:
		return EXPP_setIValueRange( value, &md->type, 0, MOD_CAST_TYPE_CUBOID, 'h' );
	case EXPP_MOD_FACTOR:
		return EXPP_setFloatClamped( value, &md->fac, -10.0, 10.0 );
	case EXPP_MOD_RADIUS:
		return EXPP_setFloatClamped( value, &md->radius, 0.0, 100.0 );
	case EXPP_MOD_SIZE:
		return EXPP_setFloatClamped( value, &md->size, 0.0, 100.0 );
	case EXPP_MOD_OBJECT: {
		Object *ob_new=NULL;
		if (value == Py_None) {
			md->object = NULL;
		} else if (BPy_Object_Check( value )) {
			ob_new = ((( BPy_Object * )value)->object);
			md->object = ob_new;
		} else {
			return EXPP_ReturnIntError( PyExc_TypeError,
				"Expected an Object or None value" );
		}
		return 0;
	}
	case EXPP_MOD_VERTGROUP: {
		char *name = PyString_AsString( value );
		if( !name ) return EXPP_ReturnIntError( PyExc_TypeError, "expected string arg" );
		BLI_strncpy( md->defgrp_name, name, sizeof( md->defgrp_name ) );
		return 0;
	}
	case EXPP_MOD_ENABLE_X:
		return EXPP_setBitfield( value, &md->flag, MOD_CAST_X, 'h' ); 
	case EXPP_MOD_ENABLE_Y:
		return EXPP_setBitfield( value, &md->flag, MOD_CAST_Y, 'h' ); 
	case EXPP_MOD_ENABLE_Z:
		return EXPP_setBitfield( value, &md->flag, MOD_CAST_Z, 'h' ); 
	case EXPP_MOD_USE_OB_TRANSFORM:
		return EXPP_setBitfield( value, &md->flag, MOD_CAST_USE_OB_TRANSFORM, 'h' ); 
	case EXPP_MOD_SIZE_FROM_RADIUS:
		return EXPP_setBitfield( value, &md->flag, MOD_CAST_SIZE_FROM_RADIUS, 'h' ); 
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}

static PyObject *wave_getter( BPy_Modifier * self, int type )
{
	WaveModifierData *md = (WaveModifierData *)(self->md);

	switch( type ) {
	case EXPP_MOD_STARTX:
		return PyFloat_FromDouble( (double)md->startx );
	case EXPP_MOD_STARTY:
		return PyFloat_FromDouble( (double)md->starty );
	case EXPP_MOD_HEIGHT:
		return PyFloat_FromDouble( (double)md->height );
	case EXPP_MOD_WIDTH:
		return PyFloat_FromDouble( (double)md->width );
	case EXPP_MOD_NARROW:
		return PyFloat_FromDouble( (double)md->narrow );
	case EXPP_MOD_SPEED:
		return PyFloat_FromDouble( (double)md->speed );
	case EXPP_MOD_DAMP:
		return PyFloat_FromDouble( (double)md->damp );
	case EXPP_MOD_LIFETIME:
		return PyFloat_FromDouble( (double)md->lifetime );
	case EXPP_MOD_TIMEOFFS:
		return PyFloat_FromDouble( (double)md->timeoffs );
	case EXPP_MOD_FLAG:
		return PyInt_FromLong( (long)md->flag );
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
	}
}

static int wave_setter( BPy_Modifier *self, int type, PyObject *value )
{
	WaveModifierData *md = (WaveModifierData *)(self->md);

	switch( type ) {
	case EXPP_MOD_STARTX:
		return EXPP_setFloatClamped( value, &md->startx, -100.0, 100.0 );
	case EXPP_MOD_STARTY:
		return EXPP_setFloatClamped( value, &md->starty, -100.0, 100.0 );
	case EXPP_MOD_HEIGHT:
		return EXPP_setFloatClamped( value, &md->height, -2.0, 2.0 );
	case EXPP_MOD_WIDTH:
		return EXPP_setFloatClamped( value, &md->width, 0.0, 5.0 );
	case EXPP_MOD_NARROW:
		return EXPP_setFloatClamped( value, &md->width, 0.0, 5.0 );
	case EXPP_MOD_SPEED:
		return EXPP_setFloatClamped( value, &md->speed, -2.0, 2.0 );
	case EXPP_MOD_DAMP:
		return EXPP_setFloatClamped( value, &md->damp, -MAXFRAMEF, MAXFRAMEF );
	case EXPP_MOD_LIFETIME:
		return EXPP_setFloatClamped( value, &md->lifetime, -MAXFRAMEF, MAXFRAMEF );
	case EXPP_MOD_TIMEOFFS:
		return EXPP_setFloatClamped( value, &md->timeoffs, -MAXFRAMEF, MAXFRAMEF );
	case EXPP_MOD_FLAG:
		return EXPP_setIValueRange( value, &md->flag, 0, 
				MOD_WAVE_X | MOD_WAVE_Y | MOD_WAVE_CYCL, 'h' );
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}

static PyObject *array_getter( BPy_Modifier * self, int type )
{
	ArrayModifierData *md = (ArrayModifierData *)(self->md);

	if( type == EXPP_MOD_OBJECT_OFFSET )
		return Object_CreatePyObject( md->offset_ob );
	else if( type == EXPP_MOD_OBJECT_CURVE )
		return Object_CreatePyObject( md->curve_ob );	
	else if( type == EXPP_MOD_COUNT )
		return PyInt_FromLong( (long)md->count );
	else if( type == EXPP_MOD_LENGTH )
		return PyFloat_FromDouble( md->length );
	else if( type == EXPP_MOD_MERGE_DIST )
		return PyFloat_FromDouble( md->merge_dist );
	else if( type == EXPP_MOD_OFFSET_VEC)
		return newVectorObject( md->offset, 3, Py_NEW );
	else if( type == EXPP_MOD_SCALE_VEC)
		return newVectorObject( md->scale, 3, Py_NEW );
	
	return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
}

static int array_setter( BPy_Modifier *self, int type, PyObject *value )
{
	ArrayModifierData *md = (ArrayModifierData *)(self->md);
	switch( type ) {
	case EXPP_MOD_OBJECT_OFFSET:
		return GenericLib_assignData(value, (void **) &md->offset_ob, (void **) &self->object, 0, ID_OB, 0);
	case EXPP_MOD_OBJECT_CURVE:
		return GenericLib_assignData(value, (void **) &md->curve_ob, 0, 0, ID_OB, OB_CURVE);
	case EXPP_MOD_COUNT:
		return EXPP_setIValueClamped( value, &md->count, 1, 1000, 'i' );
	case EXPP_MOD_LENGTH:
		return EXPP_setFloatClamped( value, &md->length, 0.0, 1000.0 );
	case EXPP_MOD_MERGE_DIST:
		return EXPP_setFloatClamped( value, &md->merge_dist, 0.0, 1000.0 );
	case EXPP_MOD_OFFSET_VEC:
		return EXPP_setVec3Clamped( value, md->offset, -10000.0, 10000.0 );
	case EXPP_MOD_SCALE_VEC:
		return EXPP_setVec3Clamped( value, md->scale, -10000.0, 10000.0 );
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}

static PyObject *boolean_getter( BPy_Modifier * self, int type )
{
	BooleanModifierData *md = (BooleanModifierData *)(self->md);

	if( type == EXPP_MOD_OBJECT )
		return Object_CreatePyObject( md->object );
	else if( type == EXPP_MOD_OPERATION )
		return PyInt_FromLong( ( long )md->operation ) ;

	return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
}

static int boolean_setter( BPy_Modifier *self, int type, PyObject *value )
{
	BooleanModifierData *md = (BooleanModifierData *)(self->md);

	if( type == EXPP_MOD_OBJECT )
		return GenericLib_assignData(value, (void **) &md->object, (void **) &self->object, 0, ID_OB, OB_MESH);
	else if( type == EXPP_MOD_OPERATION )
		return EXPP_setIValueRange( value, &md->operation, 
				eBooleanModifierOp_Intersect, eBooleanModifierOp_Difference,
				'h' );

	return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
}


static PyObject *edgesplit_getter( BPy_Modifier * self, int type )
{
	EdgeSplitModifierData *md = (EdgeSplitModifierData *)(self->md);

	switch( type ) {
	case EXPP_MOD_EDGESPLIT_ANGLE:
		return PyFloat_FromDouble( (double)md->split_angle );
	case EXPP_MOD_EDGESPLIT_FROM_ANGLE:
		return PyBool_FromLong( ( long ) 
				( md->flags & MOD_EDGESPLIT_FROMANGLE ) ) ;
	case EXPP_MOD_EDGESPLIT_FROM_SHARP:
		return PyBool_FromLong( ( long ) 
				( md->flags & MOD_EDGESPLIT_FROMFLAG ) ) ;
	
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
	}
}

static int edgesplit_setter( BPy_Modifier *self, int type, PyObject *value )
{
	EdgeSplitModifierData *md = (EdgeSplitModifierData *)(self->md);

	switch( type ) {
	case EXPP_MOD_EDGESPLIT_ANGLE:
		return EXPP_setFloatClamped( value, &md->split_angle, 0.0, 180.0 );
	case EXPP_MOD_EDGESPLIT_FROM_ANGLE:
		return EXPP_setBitfield( value, &md->flags,
				MOD_EDGESPLIT_FROMANGLE, 'h' );
	case EXPP_MOD_EDGESPLIT_FROM_SHARP:
		return EXPP_setBitfield( value, &md->flags,
				MOD_EDGESPLIT_FROMFLAG, 'h' );
	
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}

static PyObject *displace_getter( BPy_Modifier * self, int type )
{
	DisplaceModifierData *md = (DisplaceModifierData *)(self->md);

	switch( type ) {
	case EXPP_MOD_TEXTURE:
		if (md->texture)	Texture_CreatePyObject( md->texture );
		else				Py_RETURN_NONE;
	case EXPP_MOD_STRENGTH:
		return PyFloat_FromDouble( (double)md->strength );
	case EXPP_MOD_DIRECTION:
		PyInt_FromLong( md->direction );
	case EXPP_MOD_VERTGROUP:
		return PyString_FromString( md->defgrp_name ) ;
	case EXPP_MOD_MID_LEVEL:
		return PyFloat_FromDouble( (double)md->midlevel );
	case EXPP_MOD_MAPPING:
		PyInt_FromLong( md->texmapping );
	case EXPP_MOD_OBJECT:
		return Object_CreatePyObject( md->map_object );
	case EXPP_MOD_UVLAYER:
		return PyString_FromString( md->uvlayer_name );
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
	}
}

static int displace_setter( BPy_Modifier *self, int type, PyObject *value )
{
	DisplaceModifierData *md = (DisplaceModifierData *)(self->md);

	switch( type ) {
	case EXPP_MOD_TEXTURE:
		return GenericLib_assignData(value, (void **) &md->texture, 0, 1, ID_TE, 0);
	case EXPP_MOD_STRENGTH:
		return EXPP_setFloatClamped( value, &md->strength, -1000.0, 1000.0 );
	
	case EXPP_MOD_DIRECTION:
		return EXPP_setIValueClamped( value, &md->direction,
				MOD_DISP_DIR_X, MOD_DISP_DIR_RGB_XYZ, 'i' );
	
	case EXPP_MOD_VERTGROUP: {
		char *name = PyString_AsString( value );
		if( !name ) return EXPP_ReturnIntError( PyExc_TypeError, "expected string arg" );
		BLI_strncpy( md->defgrp_name, name, sizeof( md->defgrp_name ) );
		return 0;
	}
	case EXPP_MOD_MID_LEVEL:
		return EXPP_setFloatClamped( value, &md->midlevel, 0.0, 1.0 );
	
	case EXPP_MOD_MAPPING:
		return EXPP_setIValueClamped( value, &md->texmapping,
				MOD_DISP_MAP_LOCAL, MOD_DISP_MAP_UV, 'i' );
	
	case EXPP_MOD_OBJECT: {
		Object *ob_new=NULL;
		if (value == Py_None) {
			md->map_object = NULL;
		} else if (BPy_Object_Check( value )) {
			ob_new = ((( BPy_Object * )value)->object);
			md->map_object = ob_new;
		} else {
			return EXPP_ReturnIntError( PyExc_TypeError,
				"Expected an Object or None value" );
		}
		return 0;
	}
	
	case EXPP_MOD_UVLAYER: {
		char *name = PyString_AsString( value );
		if( !name ) return EXPP_ReturnIntError( PyExc_TypeError, "expected string arg" );
		BLI_strncpy( md->uvlayer_name, name, sizeof( md->uvlayer_name ) );
		return 0;
	}
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
}


/* static PyObject *uvproject_getter( BPy_Modifier * self, int type )
{
	DisplaceModifierData *md = (DisplaceModifierData *)(self->md);

	switch( type ) {
	case EXPP_MOD_MID_LEVEL:
		return PyFloat_FromDouble( (double)md->midlevel );
	default:
		return EXPP_ReturnPyObjError( PyExc_KeyError, "key not found" );
	}
}

static int uvproject_setter( BPy_Modifier *self, int type, PyObject *value )
{
	DisplaceModifierData *md = (DisplaceModifierData *)(self->md);

	switch( type ) {
	case EXPP_MOD_TEXTURE:
		return 0;
	default:
		return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
	}
} */


/*
 * get data from a modifier
 */

static PyObject *Modifier_getData( BPy_Modifier * self, PyObject * key )
{
	int setting;

	if( !PyInt_Check( key ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected an int arg as stored in Blender.Modifier.Settings" );

	MODIFIER_DEL_CHECK_PY(self);
	
	setting = PyInt_AsLong( key );
	switch( setting ) {
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
				return subsurf_getter( self, setting );
			case eModifierType_Armature:
				return armature_getter( self, setting );
			case eModifierType_Lattice:
				return lattice_getter( self, setting );
			case eModifierType_Curve:
				return curve_getter( self, setting );
			case eModifierType_Build:
				return build_getter( self, setting );
			case eModifierType_Mirror:
				return mirror_getter( self, setting );
			case eModifierType_Decimate:
				return decimate_getter( self, setting );
			case eModifierType_Smooth:
				return smooth_getter( self, setting );
			case eModifierType_Cast:
				return cast_getter( self, setting );
			case eModifierType_Wave:
				return wave_getter( self, setting );
			case eModifierType_Boolean:
				return boolean_getter( self, setting );
			case eModifierType_Array:
				return array_getter( self, setting );
			case eModifierType_EdgeSplit:
				return edgesplit_getter( self, setting );
			case eModifierType_Displace:
				return displace_getter( self, setting );
			/*case eModifierType_UVProject:
				return uvproject_getter( self, setting );*/
			case eModifierType_Hook:
			case eModifierType_Softbody:
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
	int key_int;

	if( !PyNumber_Check( key ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected an int arg as stored in Blender.Modifier.Settings" );
	
	MODIFIER_DEL_CHECK_INT(self);
	
	key_int = PyInt_AsLong( key );
	
	/* Chach for standard modifier settings */
	switch( key_int ) {
	case EXPP_MOD_RENDER:
		return EXPP_setBitfield( arg, &self->md->mode,
				eModifierMode_Render, 'h' );
	case EXPP_MOD_REALTIME:
		return EXPP_setBitfield( arg, &self->md->mode,
				eModifierMode_Realtime, 'h' );
	case EXPP_MOD_EDITMODE:
		return EXPP_setBitfield( arg, &self->md->mode,
				eModifierMode_Editmode, 'h' );
	case EXPP_MOD_ONCAGE:
		return EXPP_setBitfield( arg, &self->md->mode,
				eModifierMode_OnCage, 'h' );
	}
	
	switch( self->md->type ) {
    	case eModifierType_Subsurf:
			return subsurf_setter( self, key_int, arg );
    	case eModifierType_Armature:
			return armature_setter( self, key_int, arg );
    	case eModifierType_Lattice:
			return lattice_setter( self, key_int, arg );
    	case eModifierType_Curve:
			return curve_setter( self, key_int, arg );
    	case eModifierType_Build:
			return build_setter( self, key_int, arg );
    	case eModifierType_Mirror:
			return mirror_setter( self, key_int, arg );
		case eModifierType_Array:
			return array_setter( self, key_int, arg );
		case eModifierType_Decimate:
			return decimate_setter( self, key_int, arg );
		case eModifierType_Smooth:
			return smooth_setter( self, key_int, arg );
		case eModifierType_Cast:
			return cast_setter( self, key_int, arg );
		case eModifierType_Wave:
			return wave_setter( self, key_int, arg );
		case eModifierType_Boolean:
			return boolean_setter( self, key_int, arg );
		case eModifierType_EdgeSplit:
			return edgesplit_setter( self, key_int, arg );
		case eModifierType_Displace:
			return displace_setter( self, key_int, arg );
		/*case eModifierType_UVProject:
			return uvproject_setter( self, key_int, arg );*/
		case eModifierType_Hook:
		case eModifierType_Softbody:
		case eModifierType_None:
			return 0;
	}
	return EXPP_ReturnIntError( PyExc_RuntimeError,
			"unsupported modifier setting" );
}


static PyObject *Modifier_reset( BPy_Modifier * self )
{
	Object *ob = self->object;
	ModifierData *md = self->md;
	HookModifierData *hmd = (HookModifierData*) md;
	
	MODIFIER_DEL_CHECK_PY(self);
	
	if (md->type != eModifierType_Hook)
		return EXPP_ReturnPyObjError( PyExc_TypeError,
			"can only reset hooks" );
	
	if (hmd->object) {
		Mat4Invert(hmd->object->imat, hmd->object->obmat);
		Mat4MulSerie(hmd->parentinv, hmd->object->imat, ob->obmat, NULL, NULL, NULL, NULL, NULL, NULL);
	}
	Py_RETURN_NONE;
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
PyObject *Modifier_CreatePyObject( Object *ob, ModifierData * md )
{
	BPy_Modifier *pymod;
	pymod = ( BPy_Modifier * ) PyObject_NEW( BPy_Modifier, &Modifier_Type );
	if( !pymod )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create BPy_Modifier object" );
	pymod->md = md;
	pymod->object = ob;
	return ( PyObject * ) pymod;
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
	if (!self->iter) {
		self->iter = (ModifierData *)self->object->modifiers.first;
		return EXPP_incr_ret ( (PyObject *) self );
	} else {
		return ModSeq_CreatePyObject(self->object, (ModifierData *)self->object->modifiers.first);
	}
}

/*
 * Get the next Modifier
 */

static PyObject *ModSeq_nextIter( BPy_ModSeq * self )
{
	ModifierData *iter = self->iter;
	if( iter ) {
		self->iter = iter->next;
		return Modifier_CreatePyObject( self->object, iter );
	}
	
	self->iter= NULL; /* mark as not iterating */
	return EXPP_ReturnPyObjError( PyExc_StopIteration,
			"iterator at end" );
}

/* return the number of modifiers */

static int ModSeq_length( BPy_ModSeq * self )
{
	return BLI_countlist( &self->object->modifiers );
}

/* return a modifier */

static PyObject *ModSeq_item( BPy_ModSeq * self, int i )
{
	ModifierData *md = NULL;

	/* if index is negative, start counting from the end of the list */
	if( i < 0 )
		i += ModSeq_length( self );

	/* skip through the list until we get the modifier or end of list */

	for( md = self->object->modifiers.first; i && md; --i ) md = md->next;

	if( md )
		return Modifier_CreatePyObject( self->object, md );
	else
		return EXPP_ReturnPyObjError( PyExc_IndexError,
				"array index out of range" );
}

/*****************************************************************************/
/* Python BPy_ModSeq sequence table:                                      */
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

/*
 * helper function to check for a valid modifier argument
 */

static ModifierData *locate_modifier( BPy_ModSeq *self, BPy_Modifier * value )
{
	ModifierData *md;

	/* check that argument is a modifier */
	if( !BPy_Modifier_Check(value) )
		return (ModifierData *)EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected an modifier as an argument" );

	/* check whether modifier has been removed */
	if( !value->md )
		return (ModifierData *)EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This modifier has been removed!" );

	/* find the modifier in the object's list */
	for( md = self->object->modifiers.first; md; md = md->next )
		if( md == value->md )
			return md;

	/* return exception if we can't find the modifier */
	return (ModifierData *)EXPP_ReturnPyObjError( PyExc_AttributeError,
			"This modifier is not in the object's stack" );
}

/* create a new modifier at the end of the list */

static PyObject *ModSeq_append( BPy_ModSeq *self, PyObject *value )
{
	int type = PyInt_AsLong(value);
	
	/* type 0 is eModifierType_None, should we be able to add one of these? */
	if( type <= 0 || type >= NUM_MODIFIER_TYPES )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"Not an int or argument out of range, expected an int from Blender.Modifier.Type" );
	
	BLI_addtail( &self->object->modifiers, modifier_new( type ) );
	return Modifier_CreatePyObject( self->object, self->object->modifiers.last );
}

/* remove an existing modifier */

static PyObject *ModSeq_remove( BPy_ModSeq *self, BPy_Modifier *value )
{
	ModifierData *md = locate_modifier( self, value );

	/* if we can't locate the modifier, return (exception already set) */
	if( !md )
		return (PyObject *)NULL;

	/* do the actual removal */
	BLI_remlink( &self->object->modifiers, md );
	modifier_free( md );

	/* erase the link to the modifier */
	value->md = NULL;

	Py_RETURN_NONE;
}

/* move the modifier up in the stack */

static PyObject *ModSeq_moveUp( BPy_ModSeq * self, BPy_Modifier * value )
{
	ModifierData *md = locate_modifier( self, value );

	/* if we can't locate the modifier, return (exception already set) */
	if( !md )
		return (PyObject *)NULL;
	
	if( mod_moveUp( self->object, md ) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"cannot move above a modifier requiring original data" );

	Py_RETURN_NONE;
}

/* move the modifier down in the stack */

static PyObject *ModSeq_moveDown( BPy_ModSeq * self, BPy_Modifier *value )
{
	ModifierData *md = locate_modifier( self, value );

	/* if we can't locate the modifier, return (exception already set) */
	if( !md )
		return (PyObject *)NULL;
	
	if( mod_moveDown( self->object, md ) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"cannot move beyond a non-deforming modifier" );

	Py_RETURN_NONE;
}


/* quick hack for ZanQdo: add new hook modifier for selected verts */
static PyObject *ModSeq_ZanQdoHack(BPy_ModSeq *self)
{
	/* this should add the hook (assumes that modifier stack is on same ob!) */
	if ((self) && (G.obedit) && (self->object==G.obedit)) {
		add_hook(1);
	}
	
	Py_RETURN_NONE;
}

/*****************************************************************************/
/* Python BPy_ModSeq methods table:                                       */
/*****************************************************************************/
static PyMethodDef BPy_ModSeq_methods[] = {
	/* name, method, flags, doc */
	{"append", ( PyCFunction ) ModSeq_append, METH_O,
	 "(type) - add a new modifier, where type is the type of modifier"},
	{"remove", ( PyCFunction ) ModSeq_remove, METH_O,
	 "(modifier) - remove an existing modifier, where modifier is a modifier from this object."},
	{"moveUp", ( PyCFunction ) ModSeq_moveUp, METH_O,
	 "(modifier) - Move a modifier up in stack"},
	{"moveDown", ( PyCFunction ) ModSeq_moveDown, METH_O,
	 "(modifier) - Move a modifier down in stack"},
	{"ZanQdoHack", (PyCFunction)ModSeq_ZanQdoHack, METH_NOARGS,
	 "while in editmode, adds a hook for the selected verts (adds new modifier, and deselects object)"},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python ModSeq_Type structure definition:                               */
/*****************************************************************************/
PyTypeObject ModSeq_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender.Modifiers",        /* char *tp_name; */
	sizeof( BPy_ModSeq ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	NULL,						/* destructor tp_dealloc; */
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
/* Function:    ModSeq_CreatePyObject                                     */
/* Description: This function will create a new BPy_ModSeq from an        */
/*              existing  ListBase structure.                                */
/*****************************************************************************/
PyObject *ModSeq_CreatePyObject( Object *ob, ModifierData *iter )
{
	BPy_ModSeq *pymod;
	pymod = ( BPy_ModSeq * ) PyObject_NEW( BPy_ModSeq, &ModSeq_Type );
	if( !pymod )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
				"couldn't create BPy_ModSeq object" );
	pymod->object = ob;
	pymod->iter = iter;
	return ( PyObject * ) pymod;
}

static PyObject *M_Modifier_TypeDict( void )
{
	PyObject *S = PyConstant_New(  );

	if( S ) {
		BPy_constant *d = ( BPy_constant * ) S;

		PyConstant_Insert( d, "SUBSURF", 
				PyInt_FromLong( eModifierType_Subsurf ) );
		PyConstant_Insert( d, "ARMATURE",
				PyInt_FromLong( eModifierType_Armature ) );
		PyConstant_Insert( d, "LATTICE",
				PyInt_FromLong( eModifierType_Lattice ) );
		PyConstant_Insert( d, "CURVE",
				PyInt_FromLong( eModifierType_Curve ) );
		PyConstant_Insert( d, "BUILD",
				PyInt_FromLong( eModifierType_Build ) );
		PyConstant_Insert( d, "MIRROR",
				PyInt_FromLong( eModifierType_Mirror ) );
		PyConstant_Insert( d, "DECIMATE",
				PyInt_FromLong( eModifierType_Decimate ) );
		PyConstant_Insert( d, "WAVE",
				PyInt_FromLong( eModifierType_Wave ) );
		PyConstant_Insert( d, "BOOLEAN",
				PyInt_FromLong( eModifierType_Boolean ) );
		PyConstant_Insert( d, "ARRAY",
				PyInt_FromLong( eModifierType_Array ) );
		PyConstant_Insert( d, "EDGESPLIT",
				PyInt_FromLong( eModifierType_EdgeSplit ) );
		PyConstant_Insert( d, "SMOOTH",
				PyInt_FromLong( eModifierType_Smooth ) );
		PyConstant_Insert( d, "CAST",
				PyInt_FromLong( eModifierType_Cast ) );
		PyConstant_Insert( d, "DISPLACE",
				PyInt_FromLong( eModifierType_Displace ) );
	}
	return S;
}


static PyObject *M_Modifier_SettingsDict( void )
{
	PyObject *S = PyConstant_New(  );
	
	if( S ) {
		BPy_constant *d = ( BPy_constant * ) S;

/*
# The lines below are a python script that uses the enum variables to create 
# the lines below
# START PYSCRIPT
st='''
	EXPP_MOD_RENDER = 0,
	EXPP_MOD_REALTIME,
	EXPP_MOD_EDITMODE,
	etc.. copy from above
'''

base= '''
			PyConstant_Insert( d, "%s", 
				PyInt_FromLong( EXPP_MOD_%s ) );
'''
for var in st.replace(',','').split('\n'):
	
	var= var.split()
	if not var: continue
	var= var[0]
	if (not var) or var.startswith('/'): continue
	
	var='_'.join(var.split('_')[2:])
	print base % (var, var),
# END PYSCRIPT
*/
			
			/*Auto generated from the above script*/
			PyConstant_Insert( d, "RENDER", 
				PyInt_FromLong( EXPP_MOD_RENDER ) );
			PyConstant_Insert( d, "REALTIME", 
				PyInt_FromLong( EXPP_MOD_REALTIME ) );
			PyConstant_Insert( d, "EDITMODE", 
				PyInt_FromLong( EXPP_MOD_EDITMODE ) );
			PyConstant_Insert( d, "ONCAGE", 
				PyInt_FromLong( EXPP_MOD_ONCAGE ) );
			PyConstant_Insert( d, "OBJECT", 
				PyInt_FromLong( EXPP_MOD_OBJECT ) );
			PyConstant_Insert( d, "VERTGROUP", 
				PyInt_FromLong( EXPP_MOD_VERTGROUP ) );
			PyConstant_Insert( d, "LIMIT", 
				PyInt_FromLong( EXPP_MOD_LIMIT ) );
			PyConstant_Insert( d, "FLAG", 
				PyInt_FromLong( EXPP_MOD_FLAG ) );
			PyConstant_Insert( d, "COUNT", 
				PyInt_FromLong( EXPP_MOD_COUNT ) );
			PyConstant_Insert( d, "LENGTH", 
				PyInt_FromLong( EXPP_MOD_LENGTH ) );
			PyConstant_Insert( d, "FACTOR", 
				PyInt_FromLong( EXPP_MOD_FACTOR ) );
			PyConstant_Insert( d, "ENABLE_X", 
				PyInt_FromLong( EXPP_MOD_ENABLE_X ) );
			PyConstant_Insert( d, "ENABLE_Y", 
				PyInt_FromLong( EXPP_MOD_ENABLE_Y ) );
			PyConstant_Insert( d, "ENABLE_Z", 
				PyInt_FromLong( EXPP_MOD_ENABLE_Z ) );
			PyConstant_Insert( d, "TYPES", 
				PyInt_FromLong( EXPP_MOD_TYPES ) );
			PyConstant_Insert( d, "LEVELS", 
				PyInt_FromLong( EXPP_MOD_LEVELS ) );
			PyConstant_Insert( d, "RENDLEVELS", 
				PyInt_FromLong( EXPP_MOD_RENDLEVELS ) );
			PyConstant_Insert( d, "OPTIMAL", 
				PyInt_FromLong( EXPP_MOD_OPTIMAL ) );
			PyConstant_Insert( d, "UV", 
				PyInt_FromLong( EXPP_MOD_UV ) );
			PyConstant_Insert( d, "ENVELOPES", 
				PyInt_FromLong( EXPP_MOD_ENVELOPES ) );
			PyConstant_Insert( d, "OBJECT_OFFSET", 
				PyInt_FromLong( EXPP_MOD_OBJECT_OFFSET ) );
			PyConstant_Insert( d, "OBJECT_CURVE", 
				PyInt_FromLong( EXPP_MOD_OBJECT_CURVE ) );
			PyConstant_Insert( d, "OFFSET_VEC", 
				PyInt_FromLong( EXPP_MOD_OFFSET_VEC ) );
			PyConstant_Insert( d, "SCALE_VEC", 
				PyInt_FromLong( EXPP_MOD_SCALE_VEC ) );
			PyConstant_Insert( d, "MERGE_DIST", 
				PyInt_FromLong( EXPP_MOD_MERGE_DIST ) );
			PyConstant_Insert( d, "START", 
				PyInt_FromLong( EXPP_MOD_START ) );
			PyConstant_Insert( d, "SEED", 
				PyInt_FromLong( EXPP_MOD_SEED ) );
			PyConstant_Insert( d, "RANDOMIZE", 
				PyInt_FromLong( EXPP_MOD_RANDOMIZE ) );
			PyConstant_Insert( d, "AXIS_X", 
				PyInt_FromLong( EXPP_MOD_AXIS_X ) );
			PyConstant_Insert( d, "AXIS_Y", 
				PyInt_FromLong( EXPP_MOD_AXIS_Y ) );
			PyConstant_Insert( d, "AXIS_Z", 
				PyInt_FromLong( EXPP_MOD_AXIS_Z ) );
			PyConstant_Insert( d, "RATIO", 
				PyInt_FromLong( EXPP_MOD_RATIO ) );
			PyConstant_Insert( d, "STARTX", 
				PyInt_FromLong( EXPP_MOD_STARTX ) );
			PyConstant_Insert( d, "STARTY", 
				PyInt_FromLong( EXPP_MOD_STARTY ) );
			PyConstant_Insert( d, "HEIGHT", 
				PyInt_FromLong( EXPP_MOD_HEIGHT ) );
			PyConstant_Insert( d, "WIDTH", 
				PyInt_FromLong( EXPP_MOD_WIDTH ) );
			PyConstant_Insert( d, "NARROW", 
				PyInt_FromLong( EXPP_MOD_NARROW ) );
			PyConstant_Insert( d, "SPEED", 
				PyInt_FromLong( EXPP_MOD_SPEED ) );
			PyConstant_Insert( d, "DAMP", 
				PyInt_FromLong( EXPP_MOD_DAMP ) );
			PyConstant_Insert( d, "LIFETIME", 
				PyInt_FromLong( EXPP_MOD_LIFETIME ) );
			PyConstant_Insert( d, "TIMEOFFS", 
				PyInt_FromLong( EXPP_MOD_TIMEOFFS ) );
			PyConstant_Insert( d, "OPERATION", 
				PyInt_FromLong( EXPP_MOD_OPERATION ) );
			PyConstant_Insert( d, "EDGESPLIT_ANGLE", 
				PyInt_FromLong( EXPP_MOD_EDGESPLIT_ANGLE ) );
			PyConstant_Insert( d, "EDGESPLIT_FROM_ANGLE", 
				PyInt_FromLong( EXPP_MOD_EDGESPLIT_FROM_ANGLE ) );
			PyConstant_Insert( d, "EDGESPLIT_FROM_SHARP", 
				PyInt_FromLong( EXPP_MOD_EDGESPLIT_FROM_SHARP ) );
			PyConstant_Insert( d, "UVLAYER",
				PyInt_FromLong( EXPP_MOD_UVLAYER ) );
			PyConstant_Insert( d, "MID_LEVEL",
				PyInt_FromLong( EXPP_MOD_MID_LEVEL ) );
			PyConstant_Insert( d, "STRENGTH",
				PyInt_FromLong( EXPP_MOD_STRENGTH ) );
			PyConstant_Insert( d, "TEXTURE", 
				PyInt_FromLong( EXPP_MOD_TEXTURE ) );
			PyConstant_Insert( d, "MAPPING", 
				PyInt_FromLong( EXPP_MOD_MAPPING ) );
			PyConstant_Insert( d, "DIRECTION", 
				PyInt_FromLong( EXPP_MOD_DIRECTION ) );
			PyConstant_Insert( d, "REPEAT", 
				PyInt_FromLong( EXPP_MOD_REPEAT ) );
			PyConstant_Insert( d, "RADIUS", 
				PyInt_FromLong( EXPP_MOD_RADIUS ) );
			PyConstant_Insert( d, "SIZE", 
				PyInt_FromLong( EXPP_MOD_SIZE ) );
			PyConstant_Insert( d, "USE_OB_TRANSFORM", 
				PyInt_FromLong( EXPP_MOD_USE_OB_TRANSFORM ) );
			PyConstant_Insert( d, "SIZE_FROM_RADIUS", 
				PyInt_FromLong( EXPP_MOD_SIZE_FROM_RADIUS ) );
			/*End Auto generated code*/
	}
	return S;
}

/*****************************************************************************/
/* Function:              Modifier_Init                                      */
/*****************************************************************************/
PyObject *Modifier_Init( void )
{
	PyObject *submodule;
	PyObject *TypeDict = M_Modifier_TypeDict( );
	PyObject *SettingsDict = M_Modifier_SettingsDict( );

	if( PyType_Ready( &ModSeq_Type ) < 0 ||
			PyType_Ready( &Modifier_Type ) < 0 )
		return NULL;

	submodule = Py_InitModule3( "Blender.Modifier", NULL,
			"Modifer module for accessing and creating object modifier data" );

	if( TypeDict ) {
		PyModule_AddObject( submodule, "Type", TypeDict ); /* deprecated */
		/* since PyModule_AddObject() steals a reference, we need to
		   incref TypeDict to use it again */
		Py_INCREF( TypeDict);
		PyModule_AddObject( submodule, "Types", TypeDict );
	}
	
	if( SettingsDict )
		PyModule_AddObject( submodule, "Settings", SettingsDict );
	
	return submodule;
}
