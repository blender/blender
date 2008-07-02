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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Joseph Gilbert
 *
 * ***** END GPL LICENSE BLOCK *****
*/

#include "NLA.h" /*This must come first*/

#include "DNA_curve_types.h"
#include "DNA_scene_types.h"
#include "BKE_action.h"
#include "BKE_nla.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_library.h"
#include "BLI_blenlib.h"
#include "Object.h"
#include "Ipo.h"
#include "gen_utils.h"
#include "gen_library.h"
#include "blendef.h"
#include "MEM_guardedalloc.h"

#define ACTSTRIP_STRIDEAXIS_X         0
#define ACTSTRIP_STRIDEAXIS_Y         1
#define ACTSTRIP_STRIDEAXIS_Z         2

/*****************************************************************************/
/* Python API function prototypes for the NLA module.			 */
/*****************************************************************************/
static PyObject *M_NLA_NewAction( PyObject * self, PyObject * args );
static PyObject *M_NLA_CopyAction( PyObject * self, PyObject * args );
static PyObject *M_NLA_GetActions( PyObject * self );

/*****************************************************************************/
/* The following string definitions are used for documentation strings.	   */
/* In Python these will be written to the console when doing a		 */
/* Blender.Armature.NLA.__doc__					 */
/*****************************************************************************/
char M_NLA_doc[] =
	"The Blender NLA module -This module provides control over  Armature keyframing in Blender.";
char M_NLA_NewAction_doc[] =
	"(name) - Create new action for linking to an object.";
char M_NLA_CopyAction_doc[] = "(name) - Copy action and return copy.";
char M_NLA_GetActions_doc[] = "(name) - Returns a dictionary of actions.";

/*****************************************************************************/
/* Python method structure definition for Blender.Armature.NLA module:	 */
/*****************************************************************************/
struct PyMethodDef M_NLA_methods[] = {
	{"NewAction", ( PyCFunction ) M_NLA_NewAction, METH_VARARGS,
	 M_NLA_NewAction_doc},
	{"CopyAction", ( PyCFunction ) M_NLA_CopyAction, METH_VARARGS,
	 M_NLA_CopyAction_doc},
	{"GetActions", ( PyCFunction ) M_NLA_GetActions, METH_NOARGS,
	 M_NLA_GetActions_doc},
	{NULL, NULL, 0, NULL}
};
/*****************************************************************************/
/* Python BPy_Action methods declarations:				*/
/*****************************************************************************/
static PyObject *Action_setActive( BPy_Action * self, PyObject * args );
static PyObject *Action_getFrameNumbers(BPy_Action *self);
static PyObject *Action_getChannelIpo( BPy_Action * self, PyObject * value );
static PyObject *Action_getChannelNames( BPy_Action * self );
static PyObject *Action_renameChannel( BPy_Action * self, PyObject * args );
static PyObject *Action_verifyChannel( BPy_Action * self, PyObject * value );
static PyObject *Action_removeChannel( BPy_Action * self, PyObject * value );
static PyObject *Action_getAllChannelIpos( BPy_Action * self );

/*****************************************************************************/
/* Python BPy_Action methods table:					 */
/*****************************************************************************/
static PyMethodDef BPy_Action_methods[] = {
	/* name, method, flags, doc */
	{"getName", ( PyCFunction ) GenericLib_getName, METH_NOARGS,
	 "() - return Action name"},
	{"setName", ( PyCFunction ) GenericLib_setName_with_method, METH_VARARGS,
	 "(str) - rename Action"},
	{"setActive", ( PyCFunction ) Action_setActive, METH_VARARGS,
	 "(str) -set this action as the active action for an object"},
	{"getFrameNumbers", (PyCFunction) Action_getFrameNumbers, METH_NOARGS,
	"() - get the frame numbers at which keys have been inserted"},
	{"getChannelIpo", ( PyCFunction ) Action_getChannelIpo, METH_O,
	 "(str) -get the Ipo from a named action channel in this action"},
	{"getChannelNames", ( PyCFunction ) Action_getChannelNames, METH_NOARGS,
	 "() -get the channel names for this action"},
	 {"renameChannel", ( PyCFunction ) Action_renameChannel, METH_VARARGS,
		 "(from, to) -rename the channel from string to string"},
	{"verifyChannel", ( PyCFunction ) Action_verifyChannel, METH_O,
	 "(str) -verify the channel in this action"},
	{"removeChannel", ( PyCFunction ) Action_removeChannel, METH_O,
	 "(str) -remove the channel from the action"},
	{"getAllChannelIpos", ( PyCFunction ) Action_getAllChannelIpos,
	 METH_NOARGS,
	 "() - Return a dict of (name:ipo)-keys containing each channel in the object's action"},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python TypeAction callback function prototypes:			    */
/*****************************************************************************/
static int Action_compare( BPy_Action * a, BPy_Action * b );
static PyObject *Action_repr( BPy_Action * bone );

/*-------------------------------------------------------------------------*/
static PyObject *M_NLA_NewAction( PyObject * self_unused, PyObject * args )
{
	char *name_str = "DefaultAction";
	BPy_Action *py_action = NULL;	/* for Action Data object wrapper in Python */
	bAction *bl_action = NULL;	/* for actual Action Data we create in Blender */

	if( !PyArg_ParseTuple( args, "|s", &name_str ) ) {
		EXPP_ReturnPyObjError( PyExc_AttributeError,
				       "expected string or nothing" );
		return NULL;
	}
	/* Create new action globally */
	bl_action = alloc_libblock( &G.main->action, ID_AC, name_str );
	bl_action->id.flag |= LIB_FAKEUSER; /* no need to assign a user because alloc_libblock alredy assigns one */
	

	/* now create the wrapper obj in Python */
	if( bl_action )
		py_action =
			( BPy_Action * ) PyObject_NEW( BPy_Action,
						       &Action_Type );
	else {
		EXPP_ReturnPyObjError( PyExc_RuntimeError,
				       "couldn't create Action Data in Blender" );
		return NULL;
	}

	if( py_action == NULL ) {
		EXPP_ReturnPyObjError( PyExc_MemoryError,
				       "couldn't create Action Data object" );
		return NULL;
	}

	py_action->action = bl_action;	/* link Python action wrapper with Blender Action */

	Py_INCREF( py_action );
	return ( PyObject * ) py_action;
}

static PyObject *M_NLA_CopyAction( PyObject * self_unused, PyObject * args )
{
	BPy_Action *py_action = NULL;
	bAction *copyAction = NULL;

	if( !PyArg_ParseTuple( args, "O!", &Action_Type, &py_action ) ) {
		EXPP_ReturnPyObjError( PyExc_AttributeError,
				       "expected python action type" );
		return NULL;
	}
	copyAction = copy_action( py_action->action );
	return Action_CreatePyObject( copyAction );
}

static PyObject *M_NLA_GetActions( PyObject * self_unused )
{
	PyObject *dict = PyDict_New(  );
	bAction *action = NULL;

	for( action = G.main->action.first; action; action = action->id.next ) {
		PyObject *py_action = Action_CreatePyObject( action );
		if( py_action ) {
			/* Insert dict entry using the bone name as key */
			if( PyDict_SetItemString
			    ( dict, action->id.name + 2, py_action ) != 0 ) {
				Py_DECREF( py_action );
				Py_DECREF( dict );

				return EXPP_ReturnPyObjError
					( PyExc_RuntimeError,
					  "NLA_GetActions: couldn't set dict item" );
			}
			Py_DECREF( py_action );
		} else {
			Py_DECREF( dict );
			return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
							"NLA_GetActions: could not create Action object" ) );
		}
	}
	return dict;
}

static PyObject *Action_getFrameNumbers(BPy_Action *self)
{
	bActionChannel *achan = NULL;
	IpoCurve *icu = NULL;
	BezTriple *bezt = NULL;
	int verts;
	PyObject *py_list = NULL;
	
	py_list = PyList_New(0);
	for(achan = self->action->chanbase.first; achan; achan = achan->next){
		if (achan->ipo) {
			for (icu = achan->ipo->curve.first; icu; icu = icu->next){
				bezt= icu->bezt;
				if(bezt) {
					verts = icu->totvert;
					while(verts--) {
						PyObject *value;
						value = PyInt_FromLong((int)bezt->vec[1][0]);
						if ( PySequence_Contains(py_list, value) == 0){
							PyList_Append(py_list, value);
						}
						Py_DECREF(value);
						bezt++;
					}
				}
			}
		}
	}
	PyList_Sort(py_list);
	return EXPP_incr_ret(py_list);
}

static PyObject *Action_setActive( BPy_Action * self, PyObject * args )
{
	BPy_Object *object;

	if( !self->action )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					 "couldn't get attribute from a NULL action" );

	if( !PyArg_ParseTuple( args, "O!", &Object_Type, &object ) )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected python object argument" );

	if( object->object->type != OB_ARMATURE )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
						"object not of type armature" );

	/* if object is already attached to an action, decrement user count */
	if( object->object->action )
		--object->object->action->id.us;

	/* set the active action to object */
	object->object->action = self->action;
	++object->object->action->id.us;

	Py_RETURN_NONE;
}

static PyObject *Action_getChannelIpo( BPy_Action * self, PyObject * value )
{
	char *chanName = PyString_AsString(value);
	bActionChannel *chan;

	if( !chanName )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
				       "string expected" );

	chan = get_action_channel( self->action, chanName );
	if( !chan )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				       "no channel with that name" );
	
	if( !chan->ipo ) {
		Py_RETURN_NONE;
	}

	return Ipo_CreatePyObject( chan->ipo );
}

static PyObject *Action_getChannelNames( BPy_Action * self )
{
	PyObject *list = PyList_New( BLI_countlist(&(self->action->chanbase)) );
	bActionChannel *chan = NULL;
	int index=0;
	for( chan = self->action->chanbase.first; chan; chan = chan->next ) {
		PyList_SetItem( list, index, PyString_FromString(chan->name) );
		index++;
	}
	return list;
}

static PyObject *Action_renameChannel( BPy_Action * self, PyObject * args )
{
	char *chanFrom, *chanTo;
	bActionChannel *chan;

	if( !PyArg_ParseTuple( args, "ss", &chanFrom, &chanTo ) )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
				       "2 strings expected" );
	
	chan = get_action_channel( self->action, chanFrom );
	if( !chan )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"no channel with that name" );
	if (strlen(chanTo) > 31)
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"new name greater then 31 characters long" );
	
	if (get_action_channel( self->action, chanTo ))
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"channel target name alredy exists" );
	
	strcpy(chan->name, chanTo);
	
	Py_RETURN_NONE;
}

/*----------------------------------------------------------------------*/
static PyObject *Action_verifyChannel( BPy_Action * self, PyObject * value )
{
	char *chanName = PyString_AsString(value);
	bActionChannel *chan;

	if( !self->action )
		( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					 "couldn't create channel for a NULL action" ) );

	if( !chanName )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected string argument" ) );

	chan = verify_action_channel(self->action, chanName);

	Py_RETURN_NONE;
}


static PyObject *Action_removeChannel( BPy_Action * self, PyObject * value )
{
	char *chanName = PyString_AsString(value);
	bActionChannel *chan;

	if( !chanName )
		return (EXPP_ReturnPyObjError( PyExc_AttributeError,
				       "string expected" ));


	chan = get_action_channel( self->action, chanName );
	if( chan == NULL ) {
		EXPP_ReturnPyObjError( PyExc_AttributeError,
				       "no channel with that name..." );
		return NULL;
	}
	/*release ipo*/
	if( chan->ipo )
		chan->ipo->id.us--;

	/*remove channel*/
	BLI_freelinkN( &self->action->chanbase, chan );

	Py_RETURN_NONE;
}

static PyObject *Action_getAllChannelIpos( BPy_Action * self )
{
	PyObject *dict = PyDict_New(  );
	bActionChannel *chan = NULL;

	for( chan = self->action->chanbase.first; chan; chan = chan->next ) {
		PyObject *ipo_attr;
	   	if( chan->ipo )
			ipo_attr = Ipo_CreatePyObject( chan->ipo );
		else {
			ipo_attr = Py_None;
			Py_INCREF( ipo_attr );
		}
		if( ipo_attr ) {
			/* Insert dict entry using the bone name as key*/
			if( PyDict_SetItemString( dict, chan->name, ipo_attr )
			    != 0 ) {
				Py_DECREF( ipo_attr );
				Py_DECREF( dict );

				return EXPP_ReturnPyObjError
					( PyExc_RuntimeError,
					  "Action_getAllChannelIpos: couldn't set dict item" );
			}
			Py_DECREF( ipo_attr );
		} else {
			Py_DECREF( dict );
			return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
							"Action_getAllChannelIpos: could not create Ipo object" ) );
		}
	}
	return dict;
}

/*----------------------------------------------------------------------*/
static int Action_compare( BPy_Action * a, BPy_Action * b )
{
	return ( a->action == b->action ) ? 0 : -1;
}

/*----------------------------------------------------------------------*/
static PyObject *Action_repr( BPy_Action * self )
{
	if( self->action )
		return PyString_FromFormat( "[Action \"%s\"]",
					    self->action->id.name + 2 );
	else
		return PyString_FromString( "NULL" );
}

/*----------------------------------------------------------------------*/
PyObject *Action_CreatePyObject( struct bAction * act )
{
	BPy_Action *blen_action;

	if(!act) Py_RETURN_NONE;

	blen_action =
		( BPy_Action * ) PyObject_NEW( BPy_Action, &Action_Type );

	if( !blen_action) {
		return ( EXPP_ReturnPyObjError
			 ( PyExc_RuntimeError, "failure to create object!" ) );
	}
	blen_action->action = act;
	return ( ( PyObject * ) blen_action );
}

/*----------------------------------------------------------------------*/
struct bAction *Action_FromPyObject( PyObject * py_obj )
{
	BPy_Action *blen_obj;

	blen_obj = ( BPy_Action * ) py_obj;
	return ( blen_obj->action );
}

/*****************************************************************************/
/* Python attributes get/set structure:                                      */
/*****************************************************************************/
static PyGetSetDef BPy_Action_getseters[] = {
	GENERIC_LIB_GETSETATTR,
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/*****************************************************************************/
/* Python TypeAction structure definition:			        				*/
/*****************************************************************************/
PyTypeObject Action_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,	/* ob_size */
	"Blender Action",	/* tp_name */
	sizeof( BPy_Action ),	/* tp_basicsize */
	0,			/* tp_itemsize */
	/* methods */
	NULL,	/* tp_dealloc */
	NULL,	/* tp_print */
	NULL,	/* tp_getattr */
	NULL,	/* tp_setattr */
	( cmpfunc ) Action_compare,		/* tp_compare */
	( reprfunc ) Action_repr,	/* tp_repr */
	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	( hashfunc ) GenericLib_hash,	/* hashfunc tp_hash; */
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
	BPy_Action_methods,           /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_Action_getseters,         /* struct PyGetSetDef *tp_getset; */
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
/* ActionStrip wrapper                                                       */
/*****************************************************************************/

/*****************************************************************************/
/* Python BPy_ActionStrip attributes:                                        */
/*****************************************************************************/

/*
 * return the action for the action strip
 */

static PyObject *ActionStrip_getAction( BPy_ActionStrip * self )
{
	if( !self->strip )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This strip has been removed!" );
	
	return Action_CreatePyObject( self->strip->act );
}

/*
 * return the start frame of the action strip
 */

static PyObject *ActionStrip_getStripStart( BPy_ActionStrip * self )
{
	if( !self->strip )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This strip has been removed!" );
	
	return PyFloat_FromDouble( self->strip->start );
}

/*
 * set the start frame of the action strip
 */

static int ActionStrip_setStripStart( BPy_ActionStrip * self, PyObject * value )
{
	int retval;

	if( !self->strip )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"This strip has been removed!" );

	retval = EXPP_setFloatClamped( value, &self->strip->start,
			-1000.0, self->strip->end-1 );
	if( !retval ) {
		float max = self->strip->end - self->strip->start;
		if( self->strip->blendin > max )
			self->strip->blendin = max;
		if( self->strip->blendout > max )
			self->strip->blendout = max;
	}
	return retval;
}

/*
 * return the ending frame of the action strip
 */

static PyObject *ActionStrip_getStripEnd( BPy_ActionStrip * self )
{
	if( !self->strip )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This strip has been removed!" );
	
	return PyFloat_FromDouble( self->strip->end );
}

/*
 * set the ending frame of the action strip
 */

static int ActionStrip_setStripEnd( BPy_ActionStrip * self, PyObject * value )
{
	int retval;

	if( !self->strip )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"This strip has been removed!" );

	retval = EXPP_setFloatClamped( value, &self->strip->end,
			self->strip->start+1, MAXFRAMEF );
	if( !retval ) {
		float max = self->strip->end - self->strip->start;
		if( self->strip->blendin > max )
			self->strip->blendin = max;
		if( self->strip->blendout > max )
			self->strip->blendout = max;
	}
	return retval;
}

/*
 * return the start frame of the action
 */

static PyObject *ActionStrip_getActionStart( BPy_ActionStrip * self )
{
	if( !self->strip )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This strip has been removed!" );
	
	return PyFloat_FromDouble( self->strip->actstart );
}

/*
 * set the start frame of the action
 */

static int ActionStrip_setActionStart( BPy_ActionStrip * self, PyObject * value )
{
	if( !self->strip )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"This strip has been removed!" );

	return EXPP_setFloatClamped( value, &self->strip->actstart,
			-1000.0, self->strip->actend-1 );
}

/*
 * return the ending frame of the action
 */

static PyObject *ActionStrip_getActionEnd( BPy_ActionStrip * self )
{
	if( !self->strip )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This strip has been removed!" );
	
	return PyFloat_FromDouble( self->strip->actend );
}

/*
 * set the ending frame of the action
 */

static int ActionStrip_setActionEnd( BPy_ActionStrip * self, PyObject * value )
{
	if( !self->strip )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"This strip has been removed!" );

	return EXPP_setFloatClamped( value, &self->strip->actend,
			self->strip->actstart+1, MAXFRAMEF );
}

/*
 * return the repeat value of the action strip
 */

static PyObject *ActionStrip_getRepeat( BPy_ActionStrip * self )
{
	if( !self->strip )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This strip has been removed!" );
	
	return PyFloat_FromDouble( self->strip->repeat );
}

/*
 * set the repeat value of the action strip
 */

static int ActionStrip_setRepeat( BPy_ActionStrip * self, PyObject * value )
{
	if( !self->strip )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"This strip has been removed!" );

	return EXPP_setFloatClamped( value, &self->strip->repeat,
			0.001f, 1000.0f );
}

/*
 * return the blend in of the action strip
 */

static PyObject *ActionStrip_getBlendIn( BPy_ActionStrip * self )
{
	if( !self->strip )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This strip has been removed!" );
	
	return PyFloat_FromDouble( self->strip->blendin );
}

/*
 * set the blend in value of the action strip
 */

static int ActionStrip_setBlendIn( BPy_ActionStrip * self, PyObject * value )
{
	if( !self->strip )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"This strip has been removed!" );

	return EXPP_setFloatClamped( value, &self->strip->blendin,
			0.0, self->strip->end - self->strip->start );
}

/*
 * return the blend out of the action strip
 */

static PyObject *ActionStrip_getBlendOut( BPy_ActionStrip * self )
{
	if( !self->strip )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This strip has been removed!" );
	
	return PyFloat_FromDouble( self->strip->blendout );
}

/*
 * set the blend out value of the action strip
 */

static int ActionStrip_setBlendOut( BPy_ActionStrip * self, PyObject * value )
{
	if( !self->strip )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"This strip has been removed!" );

	return EXPP_setFloatClamped( value, &self->strip->blendout,
			0.0, self->strip->end - self->strip->start );
}

/*
 * return the blend mode of the action strip
 */

static PyObject *ActionStrip_getBlendMode( BPy_ActionStrip * self )
{
	if( !self->strip )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This strip has been removed!" );
	
	return PyInt_FromLong( (long)self->strip->mode ) ;
}

/*
 * set the blend mode value of the action strip
 */

static int ActionStrip_setBlendMode( BPy_ActionStrip * self, PyObject * value )
{
	if( !self->strip )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"This strip has been removed!" );

	return EXPP_setIValueRange( value, &self->strip->mode,
				0, ACTSTRIPMODE_ADD, 'h' );
}

/*
 * return the flag settings of the action strip
 */

#define ACTIONSTRIP_MASK (ACTSTRIP_SELECT | ACTSTRIP_USESTRIDE \
		| ACTSTRIP_HOLDLASTFRAME | ACTSTRIP_ACTIVE | ACTSTRIP_LOCK_ACTION \
		| ACTSTRIP_MUTE | ACTSTRIP_CYCLIC_USEX | ACTSTRIP_CYCLIC_USEY | ACTSTRIP_CYCLIC_USEZ | ACTSTRIP_AUTO_BLENDS)

static PyObject *ActionStrip_getFlag( BPy_ActionStrip * self )
{
	if( !self->strip )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This strip has been removed!" );
	
	return PyInt_FromLong( (long)( self->strip->flag & ACTIONSTRIP_MASK ) ) ;
}

/*
 * set the flag settings out value of the action strip
 */

static int ActionStrip_setFlag( BPy_ActionStrip * self, PyObject * arg )
{
	PyObject *num = PyNumber_Int( arg );
	int value;

	if( !self->strip )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"This strip has been removed!" );
	if( !num )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected int argument" );
	value = PyInt_AS_LONG( num );
	Py_DECREF( num );

	if( ( value & ACTIONSTRIP_MASK ) != value ) {
		char errstr[128];
		sprintf ( errstr , "expected int bitmask of 0x%04x", ACTIONSTRIP_MASK );
		return EXPP_ReturnIntError( PyExc_TypeError, errstr );
	}

	self->strip->flag = (short)value;
	return 0;
}

/*
 * return the stride axis of the action strip
 */

static PyObject *ActionStrip_getStrideAxis( BPy_ActionStrip * self )
{
	if( !self->strip )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This strip has been removed!" );
	
	return PyInt_FromLong( (long)self->strip->stride_axis ) ;
}

/*
 * set the stride axis of the action strip
 */

static int ActionStrip_setStrideAxis( BPy_ActionStrip * self, PyObject * value )
{
	if( !self->strip )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"This strip has been removed!" );

	return EXPP_setIValueRange( value, &self->strip->stride_axis,
				ACTSTRIP_STRIDEAXIS_X, ACTSTRIP_STRIDEAXIS_Z, 'h' );
}

/*
 * return the stride length of the action strip
 */

static PyObject *ActionStrip_getStrideLength( BPy_ActionStrip * self )
{
	if( !self->strip )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This strip has been removed!" );
	
	return PyFloat_FromDouble( (double)self->strip->stridelen ) ;
}

/*
 * set the stride length of the action strip
 */

static int ActionStrip_setStrideLength( BPy_ActionStrip * self, PyObject * value )
{
	if( !self->strip )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"This strip has been removed!" );

	return EXPP_setFloatClamped( value, &self->strip->stridelen,
			0.0001f, 1000.0 );
}

/*
 * return the stride bone name
 */

static PyObject *ActionStrip_getStrideBone( BPy_ActionStrip * self )
{
	if( !self->strip )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This strip has been removed!" );
	
	return PyString_FromString( self->strip->stridechannel );
}

static PyObject *ActionStrip_getGroupTarget( BPy_ActionStrip * self )
{
	if( !self->strip )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This strip has been removed!" );
				
	if (self->strip->object) {
		return Object_CreatePyObject( self->strip->object );
	} else {
		Py_RETURN_NONE;
	}
}

/*
 * set the stride bone name
 */

static int ActionStrip_setStrideBone( BPy_ActionStrip * self, PyObject * attr )
{
	char *name = PyString_AsString( attr );
	if( !name )
		return EXPP_ReturnIntError( PyExc_TypeError, "expected string arg" );

	if( !self->strip )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"This strip has been removed!" );
	
	BLI_strncpy( self->strip->stridechannel, name, 32 );

	return 0;
}

static int ActionStrip_setGroupTarget( BPy_ActionStrip * self, PyObject * args )
{
	if( !self->strip )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"This strip has been removed!" );

	if( (PyObject *)args == Py_None )
		self->strip->object = NULL;
	else if( BPy_Object_Check( args ) )
		self->strip->object = ((BPy_Object *)args)->object;
	else
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected an object or None" );
	return 0;
}

/*****************************************************************************/
/* Python BPy_Constraint attributes get/set structure:                       */
/*****************************************************************************/
static PyGetSetDef BPy_ActionStrip_getseters[] = {
	{"action",
	(getter)ActionStrip_getAction, (setter)NULL,
	 "Action associated with the strip", NULL},
	{"stripStart",
	(getter)ActionStrip_getStripStart, (setter)ActionStrip_setStripStart,
	 "Starting frame of the strip", NULL},
	{"stripEnd",
	(getter)ActionStrip_getStripEnd, (setter)ActionStrip_setStripEnd,
	 "Ending frame of the strip", NULL},
	{"actionStart",
	(getter)ActionStrip_getActionStart, (setter)ActionStrip_setActionStart,
	 "Starting frame of the action", NULL},
	{"actionEnd",
	(getter)ActionStrip_getActionEnd, (setter)ActionStrip_setActionEnd,
	 "Ending frame of the action", NULL},
	{"repeat",
	(getter)ActionStrip_getRepeat, (setter)ActionStrip_setRepeat,
	 "The number of times to repeat the action range", NULL},
	{"blendIn",
	(getter)ActionStrip_getBlendIn, (setter)ActionStrip_setBlendIn,
	 "Number of frames of motion blending", NULL},
	{"blendOut",
	(getter)ActionStrip_getBlendOut, (setter)ActionStrip_setBlendOut,
	 "Number of frames of ease-out", NULL},
	{"mode",
	(getter)ActionStrip_getBlendMode, (setter)ActionStrip_setBlendMode,
	 "Setting of blending mode", NULL},
	{"flag",
	(getter)ActionStrip_getFlag, (setter)ActionStrip_setFlag,
	 "Setting of blending flags", NULL},
	{"strideAxis",
	(getter)ActionStrip_getStrideAxis, (setter)ActionStrip_setStrideAxis,
	 "Dominant axis for stride bone", NULL},
	{"strideLength",
	(getter)ActionStrip_getStrideLength, (setter)ActionStrip_setStrideLength,
  	 "Distance covered by one complete cycle of the action", NULL},
	{"strideBone",
	(getter)ActionStrip_getStrideBone, (setter)ActionStrip_setStrideBone,
  	 "Name of Bone used for stride", NULL},
	{"groupTarget",
	(getter)ActionStrip_getGroupTarget, (setter)ActionStrip_setGroupTarget,
	 "Name of target armature within group", NULL},	
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/*****************************************************************************/
/* Python BPy_ActionStrip methods:                                           */
/*****************************************************************************/

/*
 * restore the values of ActionStart and ActionEnd to their defaults
 */

static PyObject *ActionStrip_resetLimits( BPy_ActionStrip *self )
{
	bActionStrip *strip = self->strip;

	if( !strip )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This strip has been removed!" );
	
	calc_action_range( strip->act, &strip->actstart, &strip->actend, 1 );

	Py_RETURN_NONE;
}

/*
 * reset the strip size
 */

static PyObject *ActionStrip_resetStripSize( BPy_ActionStrip *self )
{
	float mapping;
	bActionStrip *strip = self->strip;

	if( !strip )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This strip has been removed!" );
	
	mapping = (strip->actend - strip->actstart) / (strip->end - strip->start);
	strip->end = strip->start + mapping*(strip->end - strip->start);

	Py_RETURN_NONE;
}

/*
 * snap to start and end to nearest frames
 */

static PyObject *ActionStrip_snapToFrame( BPy_ActionStrip *self )
{
	bActionStrip *strip = self->strip;

	if( !strip )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This strip has been removed!" );
	
	strip->start= (float)floor(strip->start+0.5);
	strip->end= (float)floor(strip->end+0.5);

	Py_RETURN_NONE;
}

/*****************************************************************************/
/* Python BPy_ActionStrip methods table:                                    */
/*****************************************************************************/
static PyMethodDef BPy_ActionStrip_methods[] = {
	/* name, method, flags, doc */
	{"resetActionLimits", ( PyCFunction ) ActionStrip_resetLimits, METH_NOARGS,
	 "Restores the values of ActionStart and ActionEnd to their defaults"},
	{"resetStripSize", ( PyCFunction ) ActionStrip_resetStripSize, METH_NOARGS,
	 "Resets the Action Strip size to its creation values"},
	{"snapToFrame", ( PyCFunction ) ActionStrip_snapToFrame, METH_NOARGS,
	 "Snaps the ends of the action strip to the nearest whole numbered frame"},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python ActionStrip_Type structure definition:                            */
/*****************************************************************************/
PyTypeObject ActionStrip_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender.ActionStrip",     /* char *tp_name; */
	sizeof( BPy_ActionStrip ), /* int tp_basicsize; */
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
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
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
	NULL,                        /* getiterfunc tp_iter; */
    NULL,                        /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_ActionStrip_methods,    /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_ActionStrip_getseters,  /* struct PyGetSetDef *tp_getset; */
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

static PyObject *M_ActionStrip_FlagsDict( void )
{
	PyObject *S = PyConstant_New(  );
	
	if( S ) {
		BPy_constant *d = ( BPy_constant * ) S;
		PyConstant_Insert( d, "SELECT",			PyInt_FromLong( ACTSTRIP_SELECT ) );
		PyConstant_Insert( d, "STRIDE_PATH",	PyInt_FromLong( ACTSTRIP_USESTRIDE ) );
		PyConstant_Insert( d, "HOLD",			PyInt_FromLong( ACTSTRIP_HOLDLASTFRAME ) );
		PyConstant_Insert( d, "ACTIVE",			PyInt_FromLong( ACTSTRIP_ACTIVE ) );
		PyConstant_Insert( d, "LOCK_ACTION",	PyInt_FromLong( ACTSTRIP_LOCK_ACTION ) );
		PyConstant_Insert( d, "MUTE", 			PyInt_FromLong( ACTSTRIP_MUTE ) );
		PyConstant_Insert( d, "USEX", 			PyInt_FromLong( ACTSTRIP_CYCLIC_USEX ) );
		PyConstant_Insert( d, "USEY", 			PyInt_FromLong( ACTSTRIP_CYCLIC_USEY ) );
		PyConstant_Insert( d, "USEZ", 			PyInt_FromLong( ACTSTRIP_CYCLIC_USEZ ) );
		PyConstant_Insert( d, "AUTO_BLEND", 	PyInt_FromLong( ACTSTRIP_AUTO_BLENDS ) );
	}
	return S;
}

static PyObject *M_ActionStrip_AxisDict( void )
{
	PyObject *S = PyConstant_New(  );
	
	if( S ) {
		BPy_constant *d = ( BPy_constant * ) S;
		PyConstant_Insert( d, "STRIDEAXIS_X",
				PyInt_FromLong( ACTSTRIP_STRIDEAXIS_X ) );
		PyConstant_Insert( d, "STRIDEAXIS_Y",
				PyInt_FromLong( ACTSTRIP_STRIDEAXIS_Y ) );
		PyConstant_Insert( d, "STRIDEAXIS_Z",
				PyInt_FromLong( ACTSTRIP_STRIDEAXIS_Z ) );
	}
	return S;
}

static PyObject *M_ActionStrip_ModeDict( void )
{
	PyObject *S = PyConstant_New(  );
	
	if( S ) {
		BPy_constant *d = ( BPy_constant * ) S;
		PyConstant_Insert( d, "MODE_ADD",
				PyInt_FromLong( ACTSTRIPMODE_ADD ) );
	}
	return S;
}

PyObject *ActionStrip_CreatePyObject( struct bActionStrip *strip )
{
	BPy_ActionStrip *pyobj;
	pyobj = ( BPy_ActionStrip * ) PyObject_NEW( BPy_ActionStrip,
			&ActionStrip_Type );
	if( !pyobj )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create BPy_ActionStrip object" );
	pyobj->strip = strip;
	return ( PyObject * ) pyobj;
}

/*****************************************************************************/
/* ActionStrip Sequence wrapper                                              */
/*****************************************************************************/

/*
 * Initialize the iterator
 */

static PyObject *ActionStrips_getIter( BPy_ActionStrips * self )
{
	self->iter = (bActionStrip *)self->ob->nlastrips.first;
	return EXPP_incr_ret ( (PyObject *) self );
}

/*
 * Get the next action strip
 */

static PyObject *ActionStrips_nextIter( BPy_ActionStrips * self )
{
	bActionStrip *strip = self->iter;
	if( strip ) {
		self->iter = strip->next;
		return ActionStrip_CreatePyObject( strip );
	}

	return EXPP_ReturnPyObjError( PyExc_StopIteration,
			"iterator at end" );
}

/* return the number of action strips */

static int ActionStrips_length( BPy_ActionStrips * self )
{
	return BLI_countlist( &self->ob->nlastrips );
}

/* return an action strip */

static PyObject *ActionStrips_item( BPy_ActionStrips * self, int i )
{
	bActionStrip *strip = NULL;

	/* if index is negative, start counting from the end of the list */
	if( i < 0 )
		i += ActionStrips_length( self );

	/* skip through the list until we get the strip or end of list */

	strip = self->ob->nlastrips.first;

	while( i && strip ) {
		--i;
		strip = strip->next;
	}

	if( strip )
		return ActionStrip_CreatePyObject( strip );
	else
		return EXPP_ReturnPyObjError( PyExc_IndexError,
				"array index out of range" );
}

/*****************************************************************************/
/* Python BPy_ActionStrips sequence table:                                  */
/*****************************************************************************/
static PySequenceMethods ActionStrips_as_sequence = {
	( inquiry ) ActionStrips_length,	/* sq_length */
	( binaryfunc ) 0,	/* sq_concat */
	( intargfunc ) 0,	/* sq_repeat */
	( intargfunc ) ActionStrips_item,	/* sq_item */
	( intintargfunc ) 0,	/* sq_slice */
	( intobjargproc ) 0,	/* sq_ass_item */
	( intintobjargproc ) 0,	/* sq_ass_slice */
	( objobjproc ) 0,	/* sq_contains */
	( binaryfunc ) 0,		/* sq_inplace_concat */
	( intargfunc ) 0,		/* sq_inplace_repeat */
};


/*****************************************************************************/
/* Python BPy_ActionStrip methods:                                           */
/*****************************************************************************/

/*
 * helper function to check for a valid action strip argument
 */

static bActionStrip *locate_strip( BPy_ActionStrips *self, 
		PyObject *args, BPy_ActionStrip **stripobj )
{
	bActionStrip *strip = NULL;
	BPy_ActionStrip *pyobj;

	/* check that argument is a constraint */
	if( !PyArg_ParseTuple( args, "O!", &ActionStrip_Type, &pyobj ) ) {
		EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected an action strip as an argument" );
		return NULL;
	}

	if( !pyobj->strip ) {
		EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"This strip has been removed!" );
		return NULL;
	}

	/* if caller needs the object, return it */
	if( stripobj )
		*stripobj = pyobj;

	/* find the action strip in the NLA */
	for( strip = self->ob->nlastrips.first; strip; strip = strip->next )
		if( strip == pyobj->strip )
			return strip;

	/* return exception if we can't find the strip */
	EXPP_ReturnPyObjError( PyExc_AttributeError,
			"action strip does not belong to this object" );
	return NULL;
}

/*
 * remove an action strip from the NLA
 */

static PyObject *ActionStrips_remove( BPy_ActionStrips *self, PyObject * args )
{
	BPy_ActionStrip *pyobj;
	bActionStrip *strip = locate_strip( self, args, &pyobj );

	/* return exception if we can't find the strip */
	if( !strip )
		return (PyObject *)NULL;

	/* do the actual removal */
	free_actionstrip(strip);
	BLI_remlink(&self->ob->nlastrips, strip);
	MEM_freeN(strip);

	pyobj->strip = NULL;
	Py_RETURN_NONE;
}

/*
 * move an action strip up in the strip list
 */

static PyObject *ActionStrips_moveUp( BPy_ActionStrips *self, PyObject * args )
{
	bActionStrip *strip = locate_strip( self, args, NULL );

	/* return exception if we can't find the strip */
	if( !strip )
		return (PyObject *)NULL;

	/* if strip is not already the first, move it up */
	if( strip != self->ob->nlastrips.first ) {
		BLI_remlink(&self->ob->nlastrips, strip);
		BLI_insertlink(&self->ob->nlastrips, strip->prev->prev, strip);
	}

	Py_RETURN_NONE;
}

/*
 * move an action strip down in the strip list
 */

static PyObject *ActionStrips_moveDown( BPy_ActionStrips *self, PyObject * args )
{
	bActionStrip *strip = locate_strip( self, args, NULL );

	/* return exception if we can't find the strip */
	if( !strip )
		return (PyObject *)NULL;

	/* if strip is not already the last, move it down */
	if( strip != self->ob->nlastrips.last ) {
		BLI_remlink(&self->ob->nlastrips, strip);
		BLI_insertlink(&self->ob->nlastrips, strip->next, strip);
	}

	Py_RETURN_NONE;
}

static PyObject *ActionStrips_append( BPy_ActionStrips *self, PyObject * args )
{
	BPy_Action *pyobj;
	Object *ob;
	bActionStrip *strip;
	bAction *act;

	/* check that argument is an action */
	if( !PyArg_ParseTuple( args, "O!", &Action_Type, &pyobj ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected an action as an argument" );

	ob = self->ob;
	act = pyobj->action;

	/* Initialize the new action block */
	strip = MEM_callocN( sizeof(bActionStrip), "bActionStrip" );

    strip->act = act;
    calc_action_range( strip->act, &strip->actstart, &strip->actend, 1 );
    strip->start = (float)G.scene->r.cfra;
    strip->end = strip->start + ( strip->actend - strip->actstart );
        /* simple prevention of zero strips */
    if( strip->start > strip->end-2 )
        strip->end = strip->start+100;

    strip->flag = ACTSTRIP_LOCK_ACTION;
    find_stridechannel(ob, strip);

    strip->repeat = 1.0;
    act->id.us++;

    BLI_addtail(&ob->nlastrips, strip);

	Py_RETURN_NONE;
}

/*****************************************************************************/
/* Python BPy_ActionStrips methods table:                                    */
/*****************************************************************************/
static PyMethodDef BPy_ActionStrips_methods[] = {
	/* name, method, flags, doc */
	{"append", ( PyCFunction ) ActionStrips_append, METH_VARARGS,
	 "(action) - append a new actionstrip using existing action"},
	{"remove", ( PyCFunction ) ActionStrips_remove, METH_VARARGS,
	 "(strip) - remove an existing strip from this actionstrips"},
	{"moveUp", ( PyCFunction ) ActionStrips_moveUp, METH_VARARGS,
	 "(strip) - move an existing strip up in the actionstrips"},
	{"moveDown", ( PyCFunction ) ActionStrips_moveDown, METH_VARARGS,
	 "(strip) - move an existing strip down in the actionstrips"},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python ActionStrips_Type structure definition:                            */
/*****************************************************************************/
PyTypeObject ActionStrips_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender.ActionStrips",     /* char *tp_name; */
	sizeof( BPy_ActionStrips ), /* int tp_basicsize; */
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
	&ActionStrips_as_sequence,  /* PySequenceMethods *tp_as_sequence; */
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
	( getiterfunc )ActionStrips_getIter, /* getiterfunc tp_iter; */
    ( iternextfunc )ActionStrips_nextIter, /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_ActionStrips_methods,   /* struct PyMethodDef *tp_methods; */
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

PyObject *ActionStrips_CreatePyObject( Object *ob )
{
	BPy_ActionStrips *pyseq;
	pyseq = ( BPy_ActionStrips * ) PyObject_NEW( BPy_ActionStrips,
			&ActionStrips_Type );
	if( !pyseq )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create BPy_ActionStrips object" );
	pyseq->ob = ob;
	return ( PyObject * ) pyseq;
}

/*****************************************************************************/
/* Function:    NLA_Init                                                     */
/*****************************************************************************/
PyObject *NLA_Init( void )
{
	PyObject *FlagsDict = M_ActionStrip_FlagsDict( );
	PyObject *AxisDict = M_ActionStrip_AxisDict( );
	PyObject *ModeDict = M_ActionStrip_ModeDict( );
	PyObject *submodule;

	if( PyType_Ready( &Action_Type ) < 0
			|| PyType_Ready( &ActionStrip_Type ) < 0
			|| PyType_Ready( &ActionStrips_Type ) < 0 )
		return NULL;

	submodule = Py_InitModule3( "Blender.Armature.NLA",
				    M_NLA_methods, M_NLA_doc );

	if( FlagsDict )
		PyModule_AddObject( submodule, "Flags", FlagsDict );
	if( AxisDict )
		PyModule_AddObject( submodule, "StrideAxes", AxisDict );
	if( ModeDict )
		PyModule_AddObject( submodule, "Modes", ModeDict );

	return submodule;
}
