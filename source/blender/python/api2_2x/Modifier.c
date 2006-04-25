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
	/*Apply to all modifiers*/
	EXPP_MOD_RENDER = 0,
	EXPP_MOD_REALTIME,
	EXPP_MOD_EDITMODE,
	EXPP_MOD_ONCAGE,
	
	/*GENERIC*/
	EXPP_MOD_OBJECT, /*ARMATURE, LATTICE, CURVE, BOOLEAN, ARRAY*/
	EXPP_MOD_VERTGROUP, /*ARMATURE, LATTICE, CURVE*/
	EXPP_MOD_LIMIT, /*ARRAY, MIRROR*/
	EXPP_MOD_FLAG, /*MIRROR, WAVE*/
	EXPP_MOD_COUNT, /*DECIMATOR, ARRAY*/
	
	/*SUBSURF SPESIFIC*/
	EXPP_MOD_TYPES,
	EXPP_MOD_LEVELS,
	EXPP_MOD_RENDLEVELS,
	EXPP_MOD_OPTIMAL,
	EXPP_MOD_UV,
	
	/*ARMATURE SPESIFIC*/
	EXPP_MOD_ENVELOPES,
	
	/*BUILD SPESIFIC*/
	EXPP_MOD_START,
	EXPP_MOD_LENGTH,
	EXPP_MOD_SEED,
	EXPP_MOD_RANDOMIZE,

	/*MIRROR SPESIFIC*/
	EXPP_MOD_AXIS,

	/*DECIMATE SPESIFIC*/
	EXPP_MOD_RATIO,

	/*WAVE SPESIFIC*/
	EXPP_MOD_STARTX,
	EXPP_MOD_STARTY,
	EXPP_MOD_HEIGHT,
	EXPP_MOD_WIDTH,
	EXPP_MOD_NARROW,
	EXPP_MOD_SPEED,
	EXPP_MOD_DAMP,
	EXPP_MOD_LIFETIME,
	EXPP_MOD_TIMEOFFS,
	
	/*BOOLEAN SPESIFIC*/
	EXPP_MOD_OPERATION

	/* yet to be implemented */
	/* EXPP_MOD_HOOK_,*/
	/* EXPP_MOD_ARRAY_, */
};

/*****************************************************************************/
/* Python BPy_Modifier methods declarations:                                 */
/*****************************************************************************/
static PyObject *Modifier_getName( BPy_Modifier * self );
static int Modifier_setName( BPy_Modifier * self, PyObject *arg );
static PyObject *Modifier_getType( BPy_Modifier * self );

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
static void Modifier_dealloc( BPy_Modifier * self );
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

	( destructor ) Modifier_dealloc,/* destructor tp_dealloc; */
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
 * return the type of this modifier
 */

static PyObject *Modifier_getType( BPy_Modifier * self )
{
	if (self->md==NULL )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This modifier has been removed!" );
	
	return PyInt_FromLong( self->md->type );
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
	case EXPP_MOD_OBJECT: {
		Object *obj = (( BPy_Object * )value)->object;
		if( !BPy_Object_Check( value ) || obj->type != OB_ARMATURE )
			return EXPP_ReturnIntError( PyExc_TypeError, 
					"expected BPy armature object argument" );
		if(obj == self->obj )
			return EXPP_ReturnIntError( PyExc_TypeError,
					"Cannot lattice deform an object with its self" );
		md->object = obj;
		return 0;
		}
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
	case EXPP_MOD_OBJECT: {
		Object *obj = (( BPy_Object * )value)->object;
		if( !BPy_Object_Check( value ) || obj->type != OB_LATTICE )
			return EXPP_ReturnIntError( PyExc_TypeError, 
					"expected BPy lattice object argument" );
		if(obj == self->obj )
			return EXPP_ReturnIntError( PyExc_TypeError,
					"Cannot curve deform an object with its self" );
		md->object = obj;
		break;
		}
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
	case EXPP_MOD_OBJECT: {
		Object *obj = (( BPy_Object * )value)->object;
		if( !BPy_Object_Check( value ) || obj->type != OB_CURVE )
			return EXPP_ReturnIntError( PyExc_TypeError,
					"expected BPy lattice object argument" );
		if(obj == self->obj )
			return EXPP_ReturnIntError( PyExc_TypeError,
					"Cannot curve deform an object with its self" );
		md->object = obj;
		break;
		}
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
	case EXPP_MOD_AXIS:
		return PyInt_FromLong( (long)md->axis );
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
	case EXPP_MOD_AXIS:
		return EXPP_setIValueRange( value, &md->axis, 0, 2, 'h' );
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
		return EXPP_setFloatClamped( value, &md->damp, -1000.0, 1000.0 );
	case EXPP_MOD_LIFETIME:
		return EXPP_setFloatClamped( value, &md->lifetime, -1000.0, 1000.0 );
	case EXPP_MOD_TIMEOFFS:
		return EXPP_setFloatClamped( value, &md->timeoffs, -1000.0, 1000.0 );
	case EXPP_MOD_FLAG:
		return EXPP_setIValueRange( value, &md->flag, 0, 
				WAV_X+WAV_Y+WAV_CYCL, 'h' );
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

	if( type == EXPP_MOD_OBJECT ) {
		Object *obj = (( BPy_Object * )value)->object;
		if( !BPy_Object_Check( value ) || obj->type != OB_MESH )
			return EXPP_ReturnIntError( PyExc_TypeError,
					"expected BPy mesh object argument" );
		if(obj == self->obj )
			return EXPP_ReturnIntError( PyExc_TypeError,
					"Cannot boolean an object with its self" );
		md->object = obj;
		return 0;
	} else if( type == EXPP_MOD_OPERATION )
		return EXPP_setIValueRange( value, &md->operation, 
				eBooleanModifierOp_Intersect, eBooleanModifierOp_Difference,
				'h' );

	return EXPP_ReturnIntError( PyExc_KeyError, "key not found" );
}

/*
 * get data from a modifier
 */

static PyObject *Modifier_getData( BPy_Modifier * self, PyObject * key )
{
	int setting;

	if( !PyInt_CheckExact( key ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected an int arg as stored in Blender.Modifier.Settings" );

	if (self->md==NULL )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This modifier has been removed!" );
	
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
			case eModifierType_Wave:
				return wave_getter( self, setting );
			case eModifierType_Boolean:
				return boolean_getter( self, setting );
			case eModifierType_Hook:
			case eModifierType_Softbody:
			case eModifierType_Array:
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
	
	if (self->md==NULL )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"This modifier has been removed!" );
	
	key_int = PyInt_AsLong( key );
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
		case eModifierType_Decimate:
			return decimate_setter( self, key_int, arg );
		case eModifierType_Wave:
			return wave_setter( self, key_int, arg );
		case eModifierType_Boolean:
			return boolean_setter( self, key_int, arg );
		case eModifierType_Hook:
		case eModifierType_Softbody:
		case eModifierType_Array:
		case eModifierType_None:
			return 0;
	}
	return EXPP_ReturnIntError( PyExc_RuntimeError,
			"unsupported modifier setting" );
}

/*****************************************************************************/
/* Function:    Modifier_dealloc                                             */
/* Description: This is a callback function for the BPy_Modifier type. It    */
/*              destroys data when the object is deleted.                    */
/*****************************************************************************/
static void Modifier_dealloc( BPy_Modifier * self )
{
	PyObject_DEL( self );
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
	if (type==0 || type >= NUM_MODIFIER_TYPES) /* type 0 is eModifierType_None, should we be able to add one of these? */
		EXPP_ReturnPyObjError( PyExc_TypeError, "int argument out of range, expected an int from Blender.Modifier.Type" );
	
	BLI_addtail( &self->obj->modifiers, modifier_new( type ) );
	return Modifier_CreatePyObject( self->obj, self->obj->modifiers.last );
}

/* remove an existing modifier */

static PyObject *ModSeq_remove( BPy_ModSeq *self, PyObject *args )
{
	PyObject *pyobj;
	Object *obj;
	ModifierData *md_v, *md;

	/* check that argument is a modifier */
	if( !PyArg_ParseTuple( args, "O!", &Modifier_Type, &pyobj ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a modifier as an argument" );

	/* 
	 * check that modseq and modifier refer to the same object (this is
	 * more for user sanity than anything else)
	 */

	if( self->obj != ( ( BPy_Modifier * ) pyobj )->obj )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
				"modifier does not belong to this object" );

	md_v = ( ( BPy_Modifier * ) pyobj )->md;

	if (md_v==NULL)
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This modifier has already been removed!" );

	/* verify the modifier is still in the object's modifier */
	obj = self->obj;
	for (md=obj->modifiers.first; md; md=md->next)
		if (md==md_v)
			break;
	if (!md)
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This modifier is no longer in the object's stack" );

	/* do the actual removal */
	BLI_remlink(&(obj->modifiers), md_v);
	modifier_free(md_v);
	( ( BPy_Modifier * ) pyobj )->md= NULL;
	Py_RETURN_NONE;
}

/*****************************************************************************/
/* Function:    ModSeq_dealloc                                               */
/* Description: This is a callback function for the BPy_Modifier type. It    */
/*              destroys data when the object is deleted.                    */
/*****************************************************************************/
static void ModSeq_dealloc( BPy_Modifier * self )
{
	PyObject_DEL( self );
}

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

	( destructor ) ModSeq_dealloc,/* destructor tp_dealloc; */
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
	........ and so on, copy from above
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
	
	var=var.split('_')[-1]
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
			PyConstant_Insert( d, "START", 
				PyInt_FromLong( EXPP_MOD_START ) );
			PyConstant_Insert( d, "LENGTH", 
				PyInt_FromLong( EXPP_MOD_LENGTH ) );
			PyConstant_Insert( d, "SEED", 
				PyInt_FromLong( EXPP_MOD_SEED ) );
			PyConstant_Insert( d, "RANDOMIZE", 
				PyInt_FromLong( EXPP_MOD_RANDOMIZE ) );
			PyConstant_Insert( d, "AXIS", 
				PyInt_FromLong( EXPP_MOD_AXIS ) );
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

	if( PyType_Ready( &ModSeq_Type ) < 0 || PyType_Ready( &Modifier_Type ) < 0 )
		return NULL;

	submodule = Py_InitModule3( "Blender.Modifier", NULL, "Modifer module for accessing and creating object modifier data" );

	if( TypeDict )
		PyModule_AddObject( submodule, "Type", TypeDict );
	
	if( SettingsDict )
		PyModule_AddObject( submodule, "Settings", SettingsDict );
	
	return submodule;
}
