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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Jacques Guignot, Johnny Matthews
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

/**
 * \file World.c
 * \ingroup scripts
 * \brief Blender.World Module and World Data PyObject implementation.
 *
 * Note: Parameters between "<" and ">" are optional.  But if one of them is
 * given, all preceding ones must be given, too.  Of course, this only relates
 * to the Python functions and methods described here and only inside Python
 * code. [ This will go to another file later, probably the main exppython
 * doc file].  XXX Better: put optional args with their default value:
 * (self, name = "MyName")
 */

#include <BKE_main.h>
#include <BKE_global.h>
#include <BKE_object.h>
#include <BKE_library.h>
#include <BLI_blenlib.h>
#include <BSE_editipo.h>
#include <BIF_space.h>
#include <mydevice.h>

#include <DNA_scene_types.h>  /* for G.scene */

#include "World.h"
#include "Ipo.h"


#include "constant.h"
#include "rgbTuple.h"
#include "gen_utils.h"

#define IPOKEY_ZENITH   0
#define IPOKEY_HORIZON  1
#define IPOKEY_MIST     2
#define IPOKEY_STARS    3
#define IPOKEY_OFFSET   4
#define IPOKEY_SIZE     5
/*****************************************************************************/
/* Python BPy_World methods declarations:                                   */
/*****************************************************************************/
static PyObject *World_getRange( BPy_World * self );
static PyObject *World_setRange( BPy_World * self, PyObject * args );
static PyObject *World_getIpo( BPy_World * self );
static PyObject *World_setIpo( BPy_World * self, PyObject * args );
static PyObject *World_clearIpo( BPy_World * self );
static PyObject *World_insertIpoKey( BPy_World * self, PyObject * args );
static PyObject *World_getName( BPy_World * self );
static PyObject *World_setName( BPy_World * self, PyObject * args );
static PyObject *World_getMode( BPy_World * self );
static PyObject *World_setMode( BPy_World * self, PyObject * args );
static PyObject *World_getSkytype( BPy_World * self );
static PyObject *World_setSkytype( BPy_World * self, PyObject * args );
static PyObject *World_getMistype( BPy_World * self );
static PyObject *World_setMistype( BPy_World * self, PyObject * args );
static PyObject *World_getHor( BPy_World * self );
static PyObject *World_setHor( BPy_World * self, PyObject * args );
static PyObject *World_getZen( BPy_World * self );
static PyObject *World_setZen( BPy_World * self, PyObject * args );
static PyObject *World_getAmb( BPy_World * self );
static PyObject *World_setAmb( BPy_World * self, PyObject * args );
static PyObject *World_getStar( BPy_World * self );
static PyObject *World_setStar( BPy_World * self, PyObject * args );
static PyObject *World_getMist( BPy_World * self );
static PyObject *World_setMist( BPy_World * self, PyObject * args );
static PyObject *World_getScriptLinks( BPy_World * self, PyObject * args );
static PyObject *World_addScriptLink( BPy_World * self, PyObject * args );
static PyObject *World_clearScriptLinks( BPy_World * self );
static PyObject *World_setCurrent( BPy_World * self );


/*****************************************************************************/
/* Python API function prototypes for the World module.                     */
/*****************************************************************************/
static PyObject *M_World_New( PyObject * self, PyObject * args,
			      PyObject * keywords );
static PyObject *M_World_Get( PyObject * self, PyObject * args );
static PyObject *M_World_GetCurrent( PyObject * self );


/*****************************************************************************/
/* Python World_Type callback function prototypes:			*/
/*****************************************************************************/
static void World_DeAlloc( BPy_World * self );
//static int World_Print (BPy_World *self, FILE *fp, int flags);
static int World_SetAttr( BPy_World * self, char *name, PyObject * v );
static int World_Compare( BPy_World * a, BPy_World * b );
static PyObject *World_GetAttr( BPy_World * self, char *name );
static PyObject *World_Repr( BPy_World * self );



/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.World.__doc__                                                     */
/*****************************************************************************/
static char M_World_doc[] = "The Blender World module\n\n\
This module provides access to **World Data** objects in Blender\n\n";

static char M_World_New_doc[] = "() - return a new World object";

static char M_World_Get_doc[] =
	"(name) - return the world with the name 'name', \
returns None if not found.\n If 'name' is not specified, \
it returns a list of all worlds in the\ncurrent scene.";
static char M_World_GetCurrent_doc[] = "() - returns the current world, or \
None if the Scene has no world";



/*****************************************************************************/
/* Python method structure definition for Blender.World module:              */
/*****************************************************************************/
struct PyMethodDef M_World_methods[] = {
	{"New", ( PyCFunction ) M_World_New, METH_VARARGS | METH_KEYWORDS,
	 M_World_New_doc},
	{"Get", M_World_Get, METH_VARARGS, M_World_Get_doc},
	{"GetActive", ( PyCFunction ) M_World_GetCurrent, METH_NOARGS,
	 M_World_GetCurrent_doc},
	{"GetCurrent", ( PyCFunction ) M_World_GetCurrent, METH_NOARGS,
	 M_World_GetCurrent_doc},
	{"get", M_World_Get, METH_VARARGS, M_World_Get_doc},
	{NULL, NULL, 0, NULL}
};



/*****************************************************************************/
/* Python BPy_World methods table:                                          */
/*****************************************************************************/
static PyMethodDef BPy_World_methods[] = {
	{"getRange", ( PyCFunction ) World_getRange, METH_NOARGS,
	 "() - Return World Range"},
	{"setRange", ( PyCFunction ) World_setRange, METH_VARARGS,
	 "() - Change this World's range"},
	{"getIpo", ( PyCFunction ) World_getIpo, METH_NOARGS,
	 "() - Return World Ipo"},
	{"setIpo", ( PyCFunction ) World_setIpo, METH_VARARGS,
	 "() - Change this World's ipo"},
	{"clearIpo", ( PyCFunction ) World_clearIpo, METH_VARARGS,
	 "() - Unlink Ipo from this World"},
	{"getName", ( PyCFunction ) World_getName, METH_NOARGS,
	 "() - Return World Data name"},
	{"setName", ( PyCFunction ) World_setName, METH_VARARGS,
	 "() - Set World Data name"},
	{"getMode", ( PyCFunction ) World_getMode, METH_NOARGS,
	 "() - Return World Data mode"},
	{"setMode", ( PyCFunction ) World_setMode, METH_VARARGS,
	 "(i) - Set World Data mode"},
	{"getSkytype", ( PyCFunction ) World_getSkytype, METH_NOARGS,
	 "() - Return World Data skytype"},
	{"setSkytype", ( PyCFunction ) World_setSkytype, METH_VARARGS,
	 "() - Return World Data skytype"},
	{"getMistype", ( PyCFunction ) World_getMistype, METH_NOARGS,
	 "() - Return World Data mistype"},
	{"setMistype", ( PyCFunction ) World_setMistype, METH_VARARGS,
	 "() - Return World Data mistype"},
	{"getHor", ( PyCFunction ) World_getHor, METH_NOARGS,
	 "() - Return World Data hor"},
	{"setHor", ( PyCFunction ) World_setHor, METH_VARARGS,
	 "() - Return World Data hor"},
	{"getZen", ( PyCFunction ) World_getZen, METH_NOARGS,
	 "() - Return World Data zen"},
	{"setZen", ( PyCFunction ) World_setZen, METH_VARARGS,
	 "() - Return World Data zen"},
	{"getAmb", ( PyCFunction ) World_getAmb, METH_NOARGS,
	 "() - Return World Data amb"},
	{"setAmb", ( PyCFunction ) World_setAmb, METH_VARARGS,
	 "() - Return World Data amb"},
	{"getStar", ( PyCFunction ) World_getStar, METH_NOARGS,
	 "() - Return World Data star"},
	{"setStar", ( PyCFunction ) World_setStar, METH_VARARGS,
	 "() - Return World Data star"},
	{"getMist", ( PyCFunction ) World_getMist, METH_NOARGS,
	 "() - Return World Data mist"},
	{"setMist", ( PyCFunction ) World_setMist, METH_VARARGS,
	 "() - Return World Data mist"},
	{"getScriptLinks", ( PyCFunction ) World_getScriptLinks, METH_VARARGS,
	 "(eventname) - Get a list of this world's scriptlinks (Text names) "
	 "of the given type\n"
	 "(eventname) - string: FrameChanged or Redraw."},
	{"addScriptLink", ( PyCFunction ) World_addScriptLink, METH_VARARGS,
	 "(text, evt) - Add a new world scriptlink.\n"
	 "(text) - string: an existing Blender Text name;\n"
	 "(evt) string: FrameChanged or Redraw."},
	{"clearScriptLinks", ( PyCFunction ) World_clearScriptLinks,
	 METH_NOARGS,
	 "() - Delete all scriptlinks from this world :)."},
	{"setCurrent", ( PyCFunction ) World_setCurrent, METH_NOARGS,
	 "() - Makes this world the active world for the current scene."},
	{"makeActive", ( PyCFunction ) World_setCurrent, METH_NOARGS,
	 "please use setCurrent instead, this alias will be removed."},
	{"insertIpoKey", ( PyCFunction ) World_insertIpoKey, METH_VARARGS,
	 "( World IPO type ) - Inserts a key into the IPO"},
	{NULL, NULL, 0, NULL}
};


/*****************************************************************************/
/* Python World_Type structure definition:			          */
/*****************************************************************************/
PyTypeObject World_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,	/* ob_size */
	"World",		/* tp_name */
	sizeof( BPy_World ),	/* tp_basicsize */
	0,			/* tp_itemsize */
	/* methods */
	( destructor ) World_DeAlloc,	/* tp_dealloc */
	0,			/* tp_print */
	( getattrfunc ) World_GetAttr,	/* tp_getattr */
	( setattrfunc ) World_SetAttr,	/* tp_setattr */
	( cmpfunc ) World_Compare,	/* tp_compare */
	( reprfunc ) World_Repr,	/* tp_repr */
	0,			/* tp_as_number */
	0,			/* tp_as_sequence */
	0,			/* tp_as_mapping */
	0,			/* tp_as_hash */
	0, 0, 0, 0, 0, 0,
	0,			/* tp_doc */
	0, 0, 0, 0, 0, 0,
	BPy_World_methods,	/* tp_methods */
	0,			/* tp_members */
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

/**
 * \defgroup World_Module Blender.World module functions
 *
 */

/*@{*/

/**
 * \brief Python module function: Blender.World.New()
 *
 * This is the .New() function of the Blender.World submodule. It creates
 * new World Data in Blender and returns its Python wrapper object.  The
 * name parameter is mandatory.
 * \param <name> - string: The World Data name.
 * \return A new World PyObject.
 */

static PyObject *M_World_New( PyObject * self, PyObject * args,
			      PyObject * kwords )
{

	World *add_world( char *name );
	char *name = NULL;
	BPy_World *pyworld;
	World *blworld;

	if( !PyArg_ParseTuple( args, "s", &name ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected	int argument" ) );


	blworld = add_world( name );

	if( blworld ) {
		/* return user count to zero because add_world() inc'd it */
		blworld->id.us = 0;
		/* create python wrapper obj */
		pyworld =
			( BPy_World * ) PyObject_NEW( BPy_World, &World_Type );
	} else
		return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"couldn't create World Data in Blender" ) );

	if( pyworld == NULL )
		return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
						"couldn't create World Data object" ) );

	pyworld->world = blworld;

	return ( PyObject * ) pyworld;
}

/**
 * \brief Python module function: Blender.World.Get()
 *
 * This is the .Get() function of the Blender.World submodule.	It searches
 * the list of current World Data objects and returns a Python wrapper for
 * the one with the name provided by the user.	If called with no arguments,
 * it returns a list of all current World Data object names in Blender.
 * \param <name> - string: The name of an existing Blender World Data object.
 * \return () - A list with the names of all current World Data objects;\n
 * \return (name) - A Python wrapper for the World Data called 'name'
 * in Blender.
 */

static PyObject *M_World_Get( PyObject * self, PyObject * args )
{

	char *name = NULL;
	World *world_iter;
	PyObject *worldlist;
	BPy_World *wanted_world = NULL;
	char error_msg[64];

	if( !PyArg_ParseTuple( args, "|s", &name ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected string argument (or nothing)" ) );

	world_iter = G.main->world.first;

	if( name ) {		/* (name) - Search world by name */
		while( ( world_iter ) && ( wanted_world == NULL ) ) {
			if( strcmp( name, world_iter->id.name + 2 ) == 0 ) {
				wanted_world =
					( BPy_World * )
					PyObject_NEW( BPy_World, &World_Type );
				if( wanted_world )
					wanted_world->world = world_iter;
			}
			world_iter = world_iter->id.next;
		}

		if( wanted_world == NULL ) {	/* Requested world doesn't exist */
			PyOS_snprintf( error_msg, sizeof( error_msg ),
				       "World \"%s\" not found", name );
			return ( EXPP_ReturnPyObjError
				 ( PyExc_NameError, error_msg ) );
		}

		return ( PyObject * ) wanted_world;
	}

	else {			/* return a list of all worlds in the scene */
		worldlist = PyList_New( 0 );
		if( worldlist == NULL )
			return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
							"couldn't create PyList" ) );

		while( world_iter ) {
			BPy_World *found_world =
				( BPy_World * ) PyObject_NEW( BPy_World,
							      &World_Type );
			found_world->world = world_iter;
			PyList_Append( worldlist, ( PyObject * ) found_world );

			world_iter = world_iter->id.next;
		}
		return ( worldlist );
	}

}



static PyObject *M_World_GetCurrent( PyObject * self )
{
	BPy_World *w = NULL;
	if( !G.scene->world ) {
		Py_INCREF( Py_None );
		return Py_None;
	}
	w = ( BPy_World * ) PyObject_NEW( BPy_World, &World_Type );
	w->world = G.scene->world;
	return ( PyObject * ) w;
}

/*@}*/

/**
 * \brief Initializes the Blender.World submodule
 *
 * This function is used by Blender_Init() in Blender.c to register the
 * Blender.World submodule in the main Blender module.
 * \return PyObject*: The initialized submodule.
 */

PyObject *World_Init( void )
{
	PyObject *submodule;

	World_Type.ob_type = &PyType_Type;

	submodule = Py_InitModule3( "Blender.World",
				    M_World_methods, M_World_doc );

	PyModule_AddIntConstant( submodule, "ZENITH",      IPOKEY_ZENITH );
	PyModule_AddIntConstant( submodule, "HORIZON",     IPOKEY_HORIZON );
	PyModule_AddIntConstant( submodule, "MIST",        IPOKEY_MIST );
	PyModule_AddIntConstant( submodule, "STARS",       IPOKEY_STARS );
	PyModule_AddIntConstant( submodule, "OFFSET",      IPOKEY_OFFSET );
	PyModule_AddIntConstant( submodule, "SIZE",        IPOKEY_SIZE );

	return ( submodule );
}


/*****************************************************************************/
/* Python BPy_World methods:						*/
/*****************************************************************************/
static PyObject *World_getRange( BPy_World * self )
{
	return PyFloat_FromDouble( self->world->range );
}

static PyObject *World_setRange( BPy_World * self, PyObject * args )
{
	float range = 0.f;
	if( !PyArg_ParseTuple( args, "f", &range ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "expected a float argument" ) );
	self->world->range = range;
	Py_INCREF( Py_None );
	return Py_None;
}


static PyObject *World_getIpo( BPy_World * self )
{
	struct Ipo *ipo = self->world->ipo;

	if( !ipo ) {
		Py_INCREF( Py_None );
		return Py_None;
	}

	return Ipo_CreatePyObject( ipo );
}

static PyObject *World_setIpo( BPy_World * self, PyObject * args )
{
	PyObject *pyipo = 0;
	Ipo *ipo = NULL;
	Ipo *oldipo;

	if( !PyArg_ParseTuple( args, "O!", &Ipo_Type, &pyipo ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected Ipo as argument" );

	ipo = Ipo_FromPyObject( pyipo );

	if( !ipo )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "null ipo!" );

	if( ipo->blocktype != ID_WO )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "this ipo is not a World type ipo" );

	oldipo = self->world->ipo;
	if( oldipo ) {
		ID *id = &oldipo->id;
		if( id->us > 0 )
			id->us--;
	}

	( ( ID * ) & ipo->id )->us++;

	self->world->ipo = ipo;

	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *World_clearIpo( BPy_World * self )
{
	World *world = self->world;
	Ipo *ipo = ( Ipo * ) world->ipo;

	if( ipo ) {
		ID *id = &ipo->id;
		if( id->us > 0 )
			id->us--;
		world->ipo = NULL;

		return EXPP_incr_ret_True();
	}

	return EXPP_incr_ret_False(); /* no ipo found */
}

/**
 * \brief World PyMethod getName
 *
 * \return string: The World Data name.
 */

static PyObject *World_getName( BPy_World * self )
{
	PyObject *attr = PyString_FromString( self->world->id.name + 2 );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get World.name attribute" ) );
}

/**
 * \brief World PyMethod setName
 * \param name - string: The new World Data name.
 */

static PyObject *World_setName( BPy_World * self, PyObject * args )
{
	char *name = 0;
	char buf[21];
	if( !PyArg_ParseTuple( args, "s", &name ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "expected string argument" ) );
	snprintf( buf, sizeof( buf ), "%s", name );
	rename_id( &self->world->id, buf );

	Py_INCREF( Py_None );
	return Py_None;
}





/**
 * \brief World PyMethod getSkytype
 *
 * \return int : The World Data skytype.
 */

static PyObject *World_getSkytype( BPy_World * self )
{
	PyObject *attr = PyInt_FromLong( ( long ) self->world->skytype );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get World.skytype attribute" ) );
}


/**
 * \brief World PyMethod setSkytype
 *
 * \return int : The World Data skytype.
 */

static PyObject *World_setSkytype( BPy_World * self, PyObject * args )
{
	int skytype;

	if( !PyArg_ParseTuple( args, "i", &skytype ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected int argument" ) );
	self->world->skytype = skytype;
	Py_INCREF( Py_None );
	return Py_None;
}


/**
 * \brief World PyMethod getMode
 *
 * \return int : The World Data mode.
 */

static PyObject *World_getMode( BPy_World * self )
{
	PyObject *attr = PyInt_FromLong( ( long ) self->world->mode );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get World.mode attribute" ) );
}


/**
 * \brief World PyMethod setMode
 *
 * \return int : The World Data mode.
 */

static PyObject *World_setMode( BPy_World * self, PyObject * args )
{
	int mode;

	if( !PyArg_ParseTuple( args, "i", &mode ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected int argument" ) );
	self->world->mode = mode;
	Py_INCREF( Py_None );
	return Py_None;
}














/**
 * \brief World PyMethod getMistype
 *
 * \return int : The World Data mistype.
 */

static PyObject *World_getMistype( BPy_World * self )
{
	PyObject *attr = PyInt_FromLong( ( long ) self->world->mistype );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get World.mistype attribute" ) );
}


/**
 * \brief World PyMethod setMistype
 *
 * \return int : The World Data mistype.
 */

static PyObject *World_setMistype( BPy_World * self, PyObject * args )
{
	int mistype;

	if( !PyArg_ParseTuple( args, "i", &mistype ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected int argument" ) );
	self->world->mistype = mistype;
	Py_INCREF( Py_None );
	return Py_None;
}





static PyObject *World_getHor( BPy_World * self )
{
	PyObject *attr = PyList_New( 0 );
	if( !attr )
		return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"couldn't create list" ) );
	PyList_Append( attr, PyFloat_FromDouble( self->world->horr ) );
	PyList_Append( attr, PyFloat_FromDouble( self->world->horg ) );
	PyList_Append( attr, PyFloat_FromDouble( self->world->horb ) );
	return attr;
}


static PyObject *World_setHor( BPy_World * self, PyObject * args )
{
	PyObject *listargs = 0;
	if( !PyArg_ParseTuple( args, "O", &listargs ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected list argument" ) );
	self->world->horr = PyFloat_AsDouble( PyList_GetItem( listargs, 0 ) );
	self->world->horg = PyFloat_AsDouble( PyList_GetItem( listargs, 1 ) );
	self->world->horb = PyFloat_AsDouble( PyList_GetItem( listargs, 2 ) );
	Py_INCREF( Py_None );
	return Py_None;
}


static PyObject *World_getZen( BPy_World * self )
{
	PyObject *attr = PyList_New( 0 );
	if( !attr )
		return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"couldn't create list" ) );
	PyList_Append( attr, PyFloat_FromDouble( self->world->zenr ) );
	PyList_Append( attr, PyFloat_FromDouble( self->world->zeng ) );
	PyList_Append( attr, PyFloat_FromDouble( self->world->zenb ) );
	return attr;
}


static PyObject *World_setZen( BPy_World * self, PyObject * args )
{
	PyObject *listargs = 0;
	if( !PyArg_ParseTuple( args, "O", &listargs ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected list argument" ) );
	self->world->zenr = PyFloat_AsDouble( PyList_GetItem( listargs, 0 ) );
	self->world->zeng = PyFloat_AsDouble( PyList_GetItem( listargs, 1 ) );
	self->world->zenb = PyFloat_AsDouble( PyList_GetItem( listargs, 2 ) );
	Py_INCREF( Py_None );
	return Py_None;
}




static PyObject *World_getAmb( BPy_World * self )
{
	PyObject *attr = PyList_New( 0 );
	if( !attr )
		return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"couldn't create list" ) );
	PyList_Append( attr, PyFloat_FromDouble( self->world->ambr ) );
	PyList_Append( attr, PyFloat_FromDouble( self->world->ambg ) );
	PyList_Append( attr, PyFloat_FromDouble( self->world->ambb ) );
	return attr;
}


static PyObject *World_setAmb( BPy_World * self, PyObject * args )
{
	PyObject *listargs = 0;
	if( !PyArg_ParseTuple( args, "O", &listargs ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "expected list argument" ) );
	if( !PyList_Check( listargs ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "expected list argument" ) );
	if( PyList_Size( listargs ) != 3 )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "wrong list size" ) );
	self->world->ambr = PyFloat_AsDouble( PyList_GetItem( listargs, 0 ) );
	self->world->ambg = PyFloat_AsDouble( PyList_GetItem( listargs, 1 ) );
	self->world->ambb = PyFloat_AsDouble( PyList_GetItem( listargs, 2 ) );
	Py_INCREF( Py_None );
	return Py_None;
}


static PyObject *World_getStar( BPy_World * self )
{
	PyObject *attr = PyList_New( 0 );
	if( !attr )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_RuntimeError, "couldn't create list" ) );
	PyList_Append( attr, PyFloat_FromDouble( self->world->starr ) );
	PyList_Append( attr, PyFloat_FromDouble( self->world->starg ) );
	PyList_Append( attr, PyFloat_FromDouble( self->world->starb ) );
	PyList_Append( attr, PyFloat_FromDouble( self->world->starsize ) );
	PyList_Append( attr, PyFloat_FromDouble( self->world->starmindist ) );
	PyList_Append( attr, PyFloat_FromDouble( self->world->stardist ) );
	PyList_Append( attr, PyFloat_FromDouble( self->world->starcolnoise ) );
	return attr;
}


static PyObject *World_setStar( BPy_World * self, PyObject * args )
{
	PyObject *listargs = 0;
	if( !PyArg_ParseTuple( args, "O", &listargs ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "expected list argument" ) );
	if( !PyList_Check( listargs ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "expected list argument" ) );
	if( PyList_Size( listargs ) != 7 )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "wrong list size" ) );
	self->world->starr = PyFloat_AsDouble( PyList_GetItem( listargs, 0 ) );
	self->world->starg = PyFloat_AsDouble( PyList_GetItem( listargs, 1 ) );
	self->world->starb = PyFloat_AsDouble( PyList_GetItem( listargs, 2 ) );
	self->world->starsize =
		PyFloat_AsDouble( PyList_GetItem( listargs, 3 ) );
	self->world->starmindist =
		PyFloat_AsDouble( PyList_GetItem( listargs, 4 ) );
	self->world->stardist =
		PyFloat_AsDouble( PyList_GetItem( listargs, 5 ) );
	self->world->starcolnoise =
		PyFloat_AsDouble( PyList_GetItem( listargs, 6 ) );
	Py_INCREF( Py_None );
	return Py_None;
}






static PyObject *World_getMist( BPy_World * self )
{
	PyObject *attr = PyList_New( 0 );
	if( !attr )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_RuntimeError, "couldn't create list" ) );
	PyList_Append( attr, PyFloat_FromDouble( self->world->misi ) );
	PyList_Append( attr, PyFloat_FromDouble( self->world->miststa ) );
	PyList_Append( attr, PyFloat_FromDouble( self->world->mistdist ) );
	PyList_Append( attr, PyFloat_FromDouble( self->world->misthi ) );
	return attr;
}


static PyObject *World_setMist( BPy_World * self, PyObject * args )
{
	PyObject *listargs = 0;
	if( !PyArg_ParseTuple( args, "O", &listargs ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "expected list argument" ) );
	if( !PyList_Check( listargs ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "expected list argument" ) );
	if( PyList_Size( listargs ) != 4 )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "wrong list size" ) );
	self->world->misi = PyFloat_AsDouble( PyList_GetItem( listargs, 0 ) );
	self->world->miststa =
		PyFloat_AsDouble( PyList_GetItem( listargs, 1 ) );
	self->world->mistdist =
		PyFloat_AsDouble( PyList_GetItem( listargs, 2 ) );
	self->world->misthi =
		PyFloat_AsDouble( PyList_GetItem( listargs, 3 ) );
	Py_INCREF( Py_None );
	return Py_None;
}

/* world.addScriptLink */
static PyObject *World_addScriptLink( BPy_World * self, PyObject * args )
{
	World *world = self->world;
	ScriptLink *slink = NULL;

	slink = &( world )->scriptlink;

	if( !EXPP_addScriptLink( slink, args, 0 ) )
		return EXPP_incr_ret( Py_None );
	else
		return NULL;
}

/* world.clearScriptLinks */
static PyObject *World_clearScriptLinks( BPy_World * self )
{
	World *world = self->world;
	ScriptLink *slink = NULL;

	slink = &( world )->scriptlink;

	return EXPP_incr_ret( Py_BuildValue
			      ( "i", EXPP_clearScriptLinks( slink ) ) );
}

/* world.getScriptLinks */
static PyObject *World_getScriptLinks( BPy_World * self, PyObject * args )
{
	World *world = self->world;
	ScriptLink *slink = NULL;
	PyObject *ret = NULL;

	slink = &( world )->scriptlink;

	ret = EXPP_getScriptLinks( slink, args, 0 );

	if( ret )
		return ret;
	else
		return NULL;
}



/* world.setCurrent */
static PyObject *World_setCurrent( BPy_World * self )
{
	World *world = self->world;
	/* If there is a world then it now has one less user */
	if( G.scene->world )
		G.scene->world->id.us--;
	world->id.us++;
	G.scene->world = world;
	Py_INCREF( Py_None );
	return Py_None;
}


/*@{*/

/**
 * \brief The World PyType destructor
 */

static void World_DeAlloc( BPy_World * self )
{
	PyObject_DEL( self );
}

/**
 * \brief The World PyType attribute getter
 *
 * This is the callback called when a user tries to retrieve the contents of
 * World PyObject data members.  Ex. in Python: "print myworld.lens".
 */

static PyObject *World_GetAttr( BPy_World * self, char *name )
{

	if( strcmp( name, "name" ) == 0 )
		return World_getName( self );
	if( strcmp( name, "skytype" ) == 0 )
		return World_getSkytype( self );
	if( strcmp( name, "mode" ) == 0 )
		return World_getMode( self );
	if( strcmp( name, "mistype" ) == 0 )
		return World_getMistype( self );
	if( strcmp( name, "hor" ) == 0 )
		return World_getHor( self );
	if( strcmp( name, "zen" ) == 0 )
		return World_getZen( self );
	if( strcmp( name, "amb" ) == 0 )
		return World_getAmb( self );
	if( strcmp( name, "star" ) == 0 )
		return World_getStar( self );
	if( strcmp( name, "mist" ) == 0 )
		return World_getMist( self );
	if( strcmp( name, "users" ) == 0 )
		return PyInt_FromLong( self->world->id.us );
	return Py_FindMethod( BPy_World_methods, ( PyObject * ) self, name );
}

/**
 * \brief The World PyType attribute setter
 *
 * This is the callback called when the user tries to change the value of some
 * World data member.  Ex. in Python: "myworld.lens = 45.0".
 */

static int World_SetAttr( BPy_World * self, char *name, PyObject * value )
{
	PyObject *valtuple = Py_BuildValue( "(O)", value );

	if( !valtuple )
		return EXPP_ReturnIntError( PyExc_MemoryError,
					    "WorldSetAttr: couldn't parse args" );
	if( strcmp( name, "name" ) == 0 )
		World_setName( self, valtuple );
	if( strcmp( name, "skytype" ) == 0 )
		World_setSkytype( self, valtuple );
	if( strcmp( name, "mode" ) == 0 )
		World_setMode( self, valtuple );
	if( strcmp( name, "mistype" ) == 0 )
		World_setMistype( self, valtuple );
	if( strcmp( name, "hor" ) == 0 )
		World_setHor( self, valtuple );
	if( strcmp( name, "zen" ) == 0 )
		World_setZen( self, valtuple );
	if( strcmp( name, "amb" ) == 0 )
		World_setAmb( self, valtuple );
	if( strcmp( name, "star" ) == 0 )
		World_setStar( self, valtuple );
	if( strcmp( name, "mist" ) == 0 )
		World_setMist( self, valtuple );
	return 0;		/* normal exit */
}

/**
 * \brief The World PyType compare function
 *
 * This function compares two given World PyObjects, returning 0 for equality
 * and -1 otherwise.	In Python it becomes 1 if they are equal and 0 case not.
 * The comparison is done with their pointers to Blender World Data objects,
 * so any two wrappers pointing to the same Blender World Data will be
 * considered the same World PyObject.	Currently, only the "==" and "!="
 * comparisons are meaninful -- the "<", "<=", ">" or ">=" are not.
 */

static int World_Compare( BPy_World * a, BPy_World * b )
{
	World *pa = a->world, *pb = b->world;
	return ( pa == pb ) ? 0 : -1;
}

/**
 * \brief The World PyType print callback
 *
 * This function is called when the user tries to print a PyObject of type
 * World.  It builds a string with the name of the wrapped Blender World.
 */

/*
static int World_Print(BPy_World *self, FILE *fp, int flags)
{ 
	fprintf(fp, "[World \"%s\"]", self->world->id.name+2);
	return 0;
}
*/

/**
 * \brief The World PyType repr callback
 *
 * This function is called when the statement "repr(myworld)" is executed in
 * Python.	Repr gives a string representation of a PyObject.
 */

static PyObject *World_Repr( BPy_World * self )
{
	return PyString_FromFormat( "[World \"%s\"]",
				    self->world->id.name + 2 );
}

/*@}*/
/*
static int World_compare (BPy_World *a, BPy_World *b)
{
	World *pa = a->world, *pb = b->world;
	return (pa == pb) ? 0:-1;
}
*/
PyObject *World_CreatePyObject( struct World * world )
{
	BPy_World *blen_object;

	blen_object = ( BPy_World * ) PyObject_NEW( BPy_World, &World_Type );

	if( blen_object == NULL ) {
		return ( NULL );
	}
	blen_object->world = world;
	return ( ( PyObject * ) blen_object );

}

int World_CheckPyObject( PyObject * py_obj )
{
	return ( py_obj->ob_type == &World_Type );
}


World *World_FromPyObject( PyObject * py_obj )
{
	BPy_World *blen_obj;

	blen_obj = ( BPy_World * ) py_obj;
	return ( blen_obj->world );

}

/*****************************************************************************/
/* Description: Returns the object with the name specified by the argument   */
/*		name. Note that the calling function has to remove the first */
/*		two characters of the object name. These two characters	     */
/*		specify the type of the object (OB, ME, WO, ...)           */
/*		The function will return NULL when no object with the given  */
/*		 name is found.						*/
/*****************************************************************************/
World *GetWorldByName( char *name )
{
	World *world_iter;

	world_iter = G.main->world.first;
	while( world_iter ) {
		if( StringEqual( name, GetIdName( &( world_iter->id ) ) ) ) {
			return ( world_iter );
		}
		world_iter = world_iter->id.next;
	}

	/* There is no object with the given name */
	return ( NULL );
}
/*
 * World_insertIpoKey()
 *  inserts World IPO key for ZENITH,HORIZON,MIST,STARS,OFFSET,SIZE
 */

static PyObject *World_insertIpoKey( BPy_World * self, PyObject * args )
{
	int key = 0, map;

	if( !PyArg_ParseTuple( args, "i", &( key ) ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
										"expected int argument" ) );

	map = texchannel_to_adrcode(self->world->texact);

	if(key == IPOKEY_ZENITH) {
		insertkey((ID *)self->world, WO_ZEN_R);
		insertkey((ID *)self->world, WO_ZEN_G);
		insertkey((ID *)self->world, WO_ZEN_B);
	}
	if(key == IPOKEY_HORIZON) {
		insertkey((ID *)self->world, WO_HOR_R);
		insertkey((ID *)self->world, WO_HOR_G);
		insertkey((ID *)self->world, WO_HOR_B);
	}
	if(key == IPOKEY_MIST) {
		insertkey((ID *)self->world, WO_MISI);
		insertkey((ID *)self->world, WO_MISTDI);
		insertkey((ID *)self->world, WO_MISTSTA);
		insertkey((ID *)self->world, WO_MISTHI);
	}
	if(key == IPOKEY_STARS) {
		insertkey((ID *)self->world, WO_STAR_R);
		insertkey((ID *)self->world, WO_STAR_G);
		insertkey((ID *)self->world, WO_STAR_B);
		insertkey((ID *)self->world, WO_STARDIST);
		insertkey((ID *)self->world, WO_STARSIZE);
	}
	if(key == IPOKEY_OFFSET) {
		insertkey((ID *)self->world, map+MAP_OFS_X);
		insertkey((ID *)self->world, map+MAP_OFS_Y);
		insertkey((ID *)self->world, map+MAP_OFS_Z);
	}
	if(key == IPOKEY_SIZE) {
		insertkey((ID *)self->world, map+MAP_SIZE_X);
		insertkey((ID *)self->world, map+MAP_SIZE_Y);
		insertkey((ID *)self->world, map+MAP_SIZE_Z);
	}

	allspace(REMAKEIPO, 0);
	EXPP_allqueue(REDRAWIPO, 0);
	EXPP_allqueue(REDRAWVIEW3D, 0);
	EXPP_allqueue(REDRAWACTION, 0);
	EXPP_allqueue(REDRAWNLA, 0);

	return EXPP_incr_ret( Py_None );
}
