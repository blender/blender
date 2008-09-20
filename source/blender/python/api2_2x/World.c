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
 * Contributor(s): Jacques Guignot, Johnny Matthews
 *
 * ***** END GPL LICENSE BLOCK *****
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

#include "World.h"  /*This must come first*/

#include "DNA_scene_types.h"  /* for G.scene */
#include "DNA_userdef_types.h"
#include "BKE_global.h"
#include "BKE_world.h"
#include "BKE_main.h"
#include "BKE_library.h"
#include "BKE_texture.h"
#include "BLI_blenlib.h"
#include "BSE_editipo.h"
#include "BIF_keyframing.h"
#include "BIF_space.h"
#include "mydevice.h"
#include "Ipo.h"
#include "MTex.h"
#include "gen_utils.h"
#include "gen_library.h"
#include "MEM_guardedalloc.h"

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
static PyObject *World_oldsetIpo( BPy_World * self, PyObject * args );
static int       World_setIpo( BPy_World * self, PyObject * args );
static PyObject *World_clearIpo( BPy_World * self );
static PyObject *World_insertIpoKey( BPy_World * self, PyObject * args );
static PyObject *World_getMode( BPy_World * self );
static PyObject *World_oldsetMode( BPy_World * self, PyObject * args );
static int       World_setMode( BPy_World * self, PyObject * args );
static PyObject *World_getSkytype( BPy_World * self );
static PyObject *World_oldsetSkytype( BPy_World * self, PyObject * args );
static int       World_setSkytype( BPy_World * self, PyObject * args );
static PyObject *World_getMistype( BPy_World * self );
static PyObject *World_oldsetMistype( BPy_World * self, PyObject * args );
static int       World_setMistype( BPy_World * self, PyObject * args );
static PyObject *World_getHor( BPy_World * self );
static PyObject *World_oldsetHor( BPy_World * self, PyObject * args );
static int       World_setHor( BPy_World * self, PyObject * args );
static PyObject *World_getZen( BPy_World * self );
static PyObject *World_oldsetZen( BPy_World * self, PyObject * args );
static int       World_setZen( BPy_World * self, PyObject * args );
static PyObject *World_getAmb( BPy_World * self );
static PyObject *World_oldsetAmb( BPy_World * self, PyObject * args );
static int       World_setAmb( BPy_World * self, PyObject * args );
static PyObject *World_getStar( BPy_World * self );
static PyObject *World_oldsetStar( BPy_World * self, PyObject * args );
static int       World_setStar( BPy_World * self, PyObject * args );
static PyObject *World_getMist( BPy_World * self );
static PyObject *World_oldsetMist( BPy_World * self, PyObject * args );
static int       World_setMist( BPy_World * self, PyObject * args );
static PyObject *World_getScriptLinks( BPy_World * self, PyObject * value );
static PyObject *World_addScriptLink( BPy_World * self, PyObject * args );
static PyObject *World_clearScriptLinks( BPy_World * self, PyObject * args );
static PyObject *World_setCurrent( BPy_World * self );
static PyObject *World_getTextures( BPy_World * self );
static int 		 World_setTextures( BPy_World * self, PyObject * value );
static PyObject *World_copy( BPy_World * self );


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
//static int World_Print (BPy_World *self, FILE *fp, int flags);
static int World_Compare( BPy_World * a, BPy_World * b );
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
	{"setIpo", ( PyCFunction ) World_oldsetIpo, METH_VARARGS,
	 "() - Change this World's ipo"},
	{"clearIpo", ( PyCFunction ) World_clearIpo, METH_VARARGS,
	 "() - Unlink Ipo from this World"},
	{"getName", ( PyCFunction ) GenericLib_getName, METH_NOARGS,
	 "() - Return World Data name"},
	{"setName", ( PyCFunction ) GenericLib_setName_with_method, METH_VARARGS,
	 "() - Set World Data name"},
	{"getMode", ( PyCFunction ) World_getMode, METH_NOARGS,
	 "() - Return World Data mode"},
	{"setMode", ( PyCFunction ) World_oldsetMode, METH_VARARGS,
	 "(i) - Set World Data mode"},
	{"getSkytype", ( PyCFunction ) World_getSkytype, METH_NOARGS,
	 "() - Return World Data skytype"},
	{"setSkytype", ( PyCFunction ) World_oldsetSkytype, METH_VARARGS,
	 "() - Return World Data skytype"},
	{"getMistype", ( PyCFunction ) World_getMistype, METH_NOARGS,
	 "() - Return World Data mistype"},
	{"setMistype", ( PyCFunction ) World_oldsetMistype, METH_VARARGS,
	 "() - Return World Data mistype"},
	{"getHor", ( PyCFunction ) World_getHor, METH_NOARGS,
	 "() - Return World Data hor"},
	{"setHor", ( PyCFunction ) World_oldsetHor, METH_VARARGS,
	 "() - Return World Data hor"},
	{"getZen", ( PyCFunction ) World_getZen, METH_NOARGS,
	 "() - Return World Data zen"},
	{"setZen", ( PyCFunction ) World_oldsetZen, METH_VARARGS,
	 "() - Return World Data zen"},
	{"getAmb", ( PyCFunction ) World_getAmb, METH_NOARGS,
	 "() - Return World Data amb"},
	{"setAmb", ( PyCFunction ) World_oldsetAmb, METH_VARARGS,
	 "() - Return World Data amb"},
	{"getStar", ( PyCFunction ) World_getStar, METH_NOARGS,
	 "() - Return World Data star"},
	{"setStar", ( PyCFunction ) World_oldsetStar, METH_VARARGS,
	 "() - Return World Data star"},
	{"getMist", ( PyCFunction ) World_getMist, METH_NOARGS,
	 "() - Return World Data mist"},
	{"setMist", ( PyCFunction ) World_oldsetMist, METH_VARARGS,
	 "() - Return World Data mist"},
	{"getScriptLinks", ( PyCFunction ) World_getScriptLinks, METH_O,
	 "(eventname) - Get a list of this world's scriptlinks (Text names) "
	 "of the given type\n"
	 "(eventname) - string: FrameChanged, Redraw or Render."},
	{"addScriptLink", ( PyCFunction ) World_addScriptLink, METH_VARARGS,
	 "(text, evt) - Add a new world scriptlink.\n"
	 "(text) - string: an existing Blender Text name;\n"
	 "(evt) string: FrameChanged, Redraw or Render."},
	{"clearScriptLinks", ( PyCFunction ) World_clearScriptLinks,
	 METH_VARARGS,
	 "() - Delete all scriptlinks from this world.\n"
	 "([s1<,s2,s3...>]) - Delete specified scriptlinks from this world."},
	{"setCurrent", ( PyCFunction ) World_setCurrent, METH_NOARGS,
	 "() - Makes this world the active world for the current scene."},
	{"makeActive", ( PyCFunction ) World_setCurrent, METH_NOARGS,
	 "please use setCurrent instead, this alias will be removed."},
	{"insertIpoKey", ( PyCFunction ) World_insertIpoKey, METH_VARARGS,
	 "( World IPO type ) - Inserts a key into the IPO"},
	{"__copy__", ( PyCFunction ) World_copy, METH_NOARGS,
	 "() - Makes a copy of this world."},
	{"copy", ( PyCFunction ) World_copy, METH_NOARGS,
	 "() - Makes a copy of this world."},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python attributes get/set structure:                                      */
/*****************************************************************************/
static PyGetSetDef BPy_World_getseters[] = {
	GENERIC_LIB_GETSETATTR,
	{"skytype", (getter)World_getSkytype, (setter)World_setSkytype,
	 "sky settings as a list", NULL},
	{"mode", (getter)World_getMode, (setter)World_setMode,
	 "world mode", NULL},
	{"mistype", (getter)World_getMistype, (setter)World_setMistype,
	 "world mist type", NULL},
	{"hor", (getter)World_getHor, (setter)World_setHor,
	 "world horizon color", NULL},
	{"amb", (getter)World_getAmb, (setter)World_setAmb,
	 "world ambient color", NULL},
	{"mist", (getter)World_getMist, (setter)World_setMist,
	 "world mist settings", NULL},
	{"ipo", (getter)World_getIpo, (setter)World_setIpo,
	 "world ipo", NULL},
    {"textures", (getter)World_getTextures, (setter)World_setTextures,
     "The World's texture list as a tuple",
     NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
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
	NULL,	/* tp_dealloc */
	0,		/* tp_print */
	NULL,	/* tp_getattr */
	NULL,	/* tp_setattr */
	( cmpfunc ) World_Compare,	/* tp_compare */
	( reprfunc ) World_Repr,	/* tp_repr */

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
	BPy_World_methods,           /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_World_getseters,         /* struct PyGetSetDef *tp_getset; */
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
	char error_msg[64];

	if( !PyArg_ParseTuple( args, "|s", &name ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected string argument (or nothing)" ) );

	if( name ) {		/* (name) - Search world by name */
		world_iter = ( World * ) GetIdFromList( &( G.main->world ), name );
		
		if( world_iter == NULL ) {	/* Requested world doesn't exist */
			PyOS_snprintf( error_msg, sizeof( error_msg ),
				       "World \"%s\" not found", name );
			return ( EXPP_ReturnPyObjError
				 ( PyExc_NameError, error_msg ) );
		}

		return ( PyObject * ) World_CreatePyObject(world_iter);
	}

	else {			/* return a list of all worlds in the scene */
		world_iter = G.main->world.first;
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
			Py_DECREF(found_world);
			
			world_iter = world_iter->id.next;
		}
		return ( worldlist );
	}

}

static PyObject *M_World_GetCurrent( PyObject * self )
{
	BPy_World *w = NULL;
#if 0	/* add back in when bpy becomes "official" */
	static char warning = 1;
	if( warning ) {
		printf("Blender.World.GetCurrent() deprecated!\n\tuse bpy.scenes.world instead\n");
		--warning;
	}
#endif

	if( !G.scene->world )
		Py_RETURN_NONE;
	
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

	if( PyType_Ready( &World_Type ) < 0 )
		return NULL;

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
	Py_RETURN_NONE;
}


static PyObject *World_getIpo( BPy_World * self )
{
	struct Ipo *ipo = self->world->ipo;

	if( !ipo )
		Py_RETURN_NONE;

	return Ipo_CreatePyObject( ipo );
}

static int World_setIpo( BPy_World * self, PyObject * value )
{
	return GenericLib_assignData(value, (void **) &self->world->ipo, 0, 1, ID_IP, ID_WO);
}

static PyObject *World_oldsetIpo( BPy_World * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args, (setter)World_setIpo );
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
 * \brief World PyMethod getSkytype
 *
 * \return int : The World Data skytype.
 */

static PyObject *World_getSkytype( BPy_World * self )
{
	return PyInt_FromLong( ( long ) self->world->skytype );
}


/**
 * \brief World PyMethod setSkytype
 *
 * \return int : The World Data skytype.
 */

static int World_setSkytype( BPy_World * self, PyObject * value )
{
	if( !PyInt_Check(value) )
		return ( EXPP_ReturnIntError( PyExc_TypeError,
						"expected int argument" ) );
	self->world->skytype = (short)PyInt_AsLong(value);
	return 0;
}

static PyObject *World_oldsetSkytype( BPy_World * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args, (setter)World_setSkytype );
}


/**
 * \brief World PyMethod getMode
 *
 * \return int : The World Data mode.
 */

static PyObject *World_getMode( BPy_World * self )
{
	return PyInt_FromLong( ( long ) self->world->mode );
}


/**
 * \brief World PyMethod setMode
 *
 * \return int : The World Data mode.
 */

static int World_setMode( BPy_World * self, PyObject * value )
{
	if( !PyInt_Check(value) )
		return ( EXPP_ReturnIntError( PyExc_TypeError,
						"expected int argument" ) );
	self->world->mode = (short)PyInt_AsLong(value);
	return 0;
}

static PyObject *World_oldsetMode( BPy_World * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args, (setter)World_setMode );
}



/**
 * \brief World PyMethod getMistype
 *
 * \return int : The World Data mistype.
 */

static PyObject *World_getMistype( BPy_World * self )
{
	return PyInt_FromLong( ( long ) self->world->mistype );
}


/**
 * \brief World PyMethod setMistype
 *
 * \return int : The World Data mistype.
 */

static int World_setMistype( BPy_World * self, PyObject * value )
{
	if( !PyInt_Check(value) )
		return ( EXPP_ReturnIntError( PyExc_TypeError,
						"expected int argument" ) );
	self->world->mistype = (short)PyInt_AsLong(value);
	return 0;
}

static PyObject *World_oldsetMistype( BPy_World * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args, (setter)World_setMistype );
}



static PyObject *World_getHor( BPy_World * self )
{
	PyObject *attr = PyList_New( 3 );
	if( !attr )
		return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"couldn't create list" ) );
	PyList_SET_ITEM( attr, 0, PyFloat_FromDouble( self->world->horr ) );
	PyList_SET_ITEM( attr, 1, PyFloat_FromDouble( self->world->horg ) );
	PyList_SET_ITEM( attr, 2, PyFloat_FromDouble( self->world->horb ) );
	return attr;
}


static int World_setHor( BPy_World * self, PyObject * value )
{
	if( !PyList_Check( value ) )
		return ( EXPP_ReturnIntError( PyExc_TypeError,
						"expected list argument" ) );
	if( PyList_Size( value ) != 3 )
		return ( EXPP_ReturnIntError
			 ( PyExc_TypeError, "list size must be 3" ) );
	self->world->horr = (float)PyFloat_AsDouble( PyList_GetItem( value, 0 ) );
	self->world->horg = (float)PyFloat_AsDouble( PyList_GetItem( value, 1 ) );
	self->world->horb = (float)PyFloat_AsDouble( PyList_GetItem( value, 2 ) );
	return 0;
}

static PyObject *World_oldsetHor( BPy_World * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args, (setter)World_setHor );
}

static PyObject *World_getZen( BPy_World * self )
{
	PyObject *attr = PyList_New( 3 );
	if( !attr )
		return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"couldn't create list" ) );
	PyList_SET_ITEM( attr, 0, PyFloat_FromDouble( self->world->zenr ) );
	PyList_SET_ITEM( attr, 1, PyFloat_FromDouble( self->world->zeng ) );
	PyList_SET_ITEM( attr, 2, PyFloat_FromDouble( self->world->zenb ) );
	return attr;
}


static int World_setZen( BPy_World * self, PyObject * value )
{
	if( !PyList_Check( value ) )
		return ( EXPP_ReturnIntError( PyExc_TypeError,
						"expected list argument" ) );
	if( PyList_Size( value ) != 3 )
		return ( EXPP_ReturnIntError
			 ( PyExc_TypeError, "list size must be 3" ) );
	self->world->zenr = (float)PyFloat_AsDouble( PyList_GetItem( value, 0 ) );
	self->world->zeng = (float)PyFloat_AsDouble( PyList_GetItem( value, 1 ) );
	self->world->zenb = (float)PyFloat_AsDouble( PyList_GetItem( value, 2 ) );
	return 0;
}

static PyObject *World_oldsetZen( BPy_World * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args, (setter)World_setZen );
}


static PyObject *World_getAmb( BPy_World * self )
{
	PyObject *attr = PyList_New( 3 );
	if( !attr )
		return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"couldn't create list" ) );
	PyList_SET_ITEM( attr, 0, PyFloat_FromDouble( self->world->ambr ) );
	PyList_SET_ITEM( attr, 1, PyFloat_FromDouble( self->world->ambg ) );
	PyList_SET_ITEM( attr, 2, PyFloat_FromDouble( self->world->ambb ) );
	return attr;
}


static int World_setAmb( BPy_World * self, PyObject * value )
{
	if( !PyList_Check( value ) )
		return ( EXPP_ReturnIntError( PyExc_TypeError,
						"expected list argument" ) );
	if( PyList_Size( value ) != 3 )
		return ( EXPP_ReturnIntError
			 ( PyExc_TypeError, "wrong list size" ) );
	self->world->ambr = (float)PyFloat_AsDouble( PyList_GetItem( value, 0 ) );
	self->world->ambg = (float)PyFloat_AsDouble( PyList_GetItem( value, 1 ) );
	self->world->ambb = (float)PyFloat_AsDouble( PyList_GetItem( value, 2 ) );
	return 0;
}

static PyObject *World_oldsetAmb( BPy_World * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args, (setter)World_setAmb );
}

static PyObject *World_getStar( BPy_World * self )
{
	PyObject *attr = PyList_New( 7 );
	if( !attr )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_RuntimeError, "couldn't create list" ) );
	PyList_SET_ITEM( attr, 0, PyFloat_FromDouble( self->world->starr ) );
	PyList_SET_ITEM( attr, 1, PyFloat_FromDouble( self->world->starg ) );
	PyList_SET_ITEM( attr, 2, PyFloat_FromDouble( self->world->starb ) );
	PyList_SET_ITEM( attr, 3, PyFloat_FromDouble( self->world->starsize ) );
	PyList_SET_ITEM( attr, 4, PyFloat_FromDouble( self->world->starmindist ) );
	PyList_SET_ITEM( attr, 5, PyFloat_FromDouble( self->world->stardist ) );
	PyList_SET_ITEM( attr, 6, PyFloat_FromDouble( self->world->starcolnoise ) );
	return attr;
}


static int World_setStar( BPy_World * self, PyObject * value )
{
	if( !PyList_Check( value ) )
		return ( EXPP_ReturnIntError
			 ( PyExc_TypeError, "expected list argument" ) );
	if( PyList_Size( value ) != 7 )
		return ( EXPP_ReturnIntError
			 ( PyExc_TypeError, "wrong list size" ) );
	self->world->starr = (float)PyFloat_AsDouble( PyList_GetItem( value, 0 ) );
	self->world->starg = (float)PyFloat_AsDouble( PyList_GetItem( value, 1 ) );
	self->world->starb = (float)PyFloat_AsDouble( PyList_GetItem( value, 2 ) );
	self->world->starsize =
		(float)PyFloat_AsDouble( PyList_GetItem( value, 3 ) );
	self->world->starmindist =
		(float)PyFloat_AsDouble( PyList_GetItem( value, 4 ) );
	self->world->stardist =
		(float)PyFloat_AsDouble( PyList_GetItem( value, 5 ) );
	self->world->starcolnoise =
		(float)PyFloat_AsDouble( PyList_GetItem( value, 6 ) );
	return 0;
}

static PyObject *World_oldsetStar( BPy_World * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args, (setter)World_setStar );
}


static PyObject *World_getMist( BPy_World * self )
{
	PyObject *attr = PyList_New( 4 );
	if( !attr )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_RuntimeError, "couldn't create list" ) );
	PyList_SET_ITEM( attr, 0, PyFloat_FromDouble( self->world->misi ) );
	PyList_SET_ITEM( attr, 1, PyFloat_FromDouble( self->world->miststa ) );
	PyList_SET_ITEM( attr, 2, PyFloat_FromDouble( self->world->mistdist ) );
	PyList_SET_ITEM( attr, 3, PyFloat_FromDouble( self->world->misthi ) );
	return attr;
}

static int World_setMist( BPy_World * self, PyObject * value )
{
	if( !PyList_Check( value ) )
		return ( EXPP_ReturnIntError
			 ( PyExc_TypeError, "expected list argument" ) );
	if( PyList_Size( value ) != 4 )
		return ( EXPP_ReturnIntError
			 ( PyExc_TypeError, "wrong list size" ) );
	self->world->misi = (float)PyFloat_AsDouble( PyList_GetItem( value, 0 ) );
	self->world->miststa =
		(float)PyFloat_AsDouble( PyList_GetItem( value, 1 ) );
	self->world->mistdist =
		(float)PyFloat_AsDouble( PyList_GetItem( value, 2 ) );
	self->world->misthi =
		(float)PyFloat_AsDouble( PyList_GetItem( value, 3 ) );
	return 0;
}

static PyObject *World_oldsetMist( BPy_World * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args, (setter)World_setMist );
}


/* world.addScriptLink */
static PyObject *World_addScriptLink( BPy_World * self, PyObject * args )
{
	World *world = self->world;
	ScriptLink *slink = NULL;

	slink = &( world )->scriptlink;

	return EXPP_addScriptLink( slink, args, 0 );
}

/* world.clearScriptLinks */
static PyObject *World_clearScriptLinks( BPy_World * self, PyObject * args )
{
	World *world = self->world;
	ScriptLink *slink = NULL;

	slink = &( world )->scriptlink;

	return EXPP_clearScriptLinks( slink, args );
}

/* world.getScriptLinks */
static PyObject *World_getScriptLinks( BPy_World * self, PyObject * value )
{
	World *world = self->world;
	ScriptLink *slink = NULL;
	PyObject *ret = NULL;

	slink = &( world )->scriptlink;

	ret = EXPP_getScriptLinks( slink, value, 0 );

	if( ret )
		return ret;
	else
		return NULL;
}


/* world.setCurrent */
static PyObject *World_setCurrent( BPy_World * self )
{
	World *world = self->world;
#if 0	/* add back in when bpy becomes "official" */
	static char warning = 1;
	if( warning ) {
		printf("world.setCurrent() deprecated!\n\tuse bpy.scenes.world=world instead\n");
		--warning;
	}
#endif

	/* If there is a world then it now has one less user */
	if( G.scene->world )
		G.scene->world->id.us--;
	world->id.us++;
	G.scene->world = world;
	Py_RETURN_NONE;
}

/* world.__copy__ */
static PyObject *World_copy( BPy_World * self )
{
	World *world = copy_world(self->world );
	world->id.us = 0;
	return World_CreatePyObject(world);
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
	return ( a->world == b->world ) ? 0 : -1;
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

World *World_FromPyObject( PyObject * py_obj )
{
	BPy_World *blen_obj;

	blen_obj = ( BPy_World * ) py_obj;
	return ( blen_obj->world );

}

/*
 * World_insertIpoKey()
 *  inserts World IPO key for ZENITH,HORIZON,MIST,STARS,OFFSET,SIZE
 */

static PyObject *World_insertIpoKey( BPy_World * self, PyObject * args )
{
	int key = 0, flag = 0, map;

	if( !PyArg_ParseTuple( args, "i", &( key ) ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
										"expected int argument" ) );

	map = texchannel_to_adrcode(self->world->texact);

	/* flag should be initialised with the 'autokeying' flags like for normal keying */
	if (IS_AUTOKEY_FLAG(INSERTNEEDED)) flag |= INSERTKEY_NEEDED;
	
	if(key == IPOKEY_ZENITH) {
		insertkey((ID *)self->world, ID_WO, NULL, NULL, WO_ZEN_R, flag);
		insertkey((ID *)self->world, ID_WO, NULL, NULL, WO_ZEN_G, flag);
		insertkey((ID *)self->world, ID_WO, NULL, NULL, WO_ZEN_B, flag);
	}
	if(key == IPOKEY_HORIZON) {
		insertkey((ID *)self->world, ID_WO, NULL, NULL, WO_HOR_R, flag);
		insertkey((ID *)self->world, ID_WO, NULL, NULL, WO_HOR_G, flag);
		insertkey((ID *)self->world, ID_WO, NULL, NULL, WO_HOR_B, flag);
	}
	if(key == IPOKEY_MIST) {
		insertkey((ID *)self->world, ID_WO, NULL, NULL, WO_MISI, flag);
		insertkey((ID *)self->world, ID_WO, NULL, NULL, WO_MISTDI, flag);
		insertkey((ID *)self->world, ID_WO, NULL, NULL, WO_MISTSTA, flag);
		insertkey((ID *)self->world, ID_WO, NULL, NULL, WO_MISTHI, flag);
	}
	if(key == IPOKEY_STARS) {
		insertkey((ID *)self->world, ID_WO, NULL, NULL, WO_STAR_R, flag);
		insertkey((ID *)self->world, ID_WO, NULL, NULL, WO_STAR_G, flag);
		insertkey((ID *)self->world, ID_WO, NULL, NULL, WO_STAR_B, flag);
		insertkey((ID *)self->world, ID_WO, NULL, NULL, WO_STARDIST, flag);
		insertkey((ID *)self->world, ID_WO, NULL, NULL, WO_STARSIZE, flag);
	}
	if(key == IPOKEY_OFFSET) {
		insertkey((ID *)self->world, ID_WO, NULL, NULL, map+MAP_OFS_X, flag);
		insertkey((ID *)self->world, ID_WO, NULL, NULL, map+MAP_OFS_Y, flag);
		insertkey((ID *)self->world, ID_WO, NULL, NULL, map+MAP_OFS_Z, flag);
	}
	if(key == IPOKEY_SIZE) {
		insertkey((ID *)self->world, ID_WO, NULL, NULL, map+MAP_SIZE_X, flag);
		insertkey((ID *)self->world, ID_WO, NULL, NULL, map+MAP_SIZE_Y, flag);
		insertkey((ID *)self->world, ID_WO, NULL, NULL, map+MAP_SIZE_Z, flag);
	}

	allspace(REMAKEIPO, 0);
	EXPP_allqueue(REDRAWIPO, 0);
	EXPP_allqueue(REDRAWVIEW3D, 0);
	EXPP_allqueue(REDRAWACTION, 0);
	EXPP_allqueue(REDRAWNLA, 0);

	Py_RETURN_NONE;
}

static PyObject *World_getTextures( BPy_World * self )
{
	int i;
	PyObject *tuple;

	/* build a texture list */
	tuple = PyTuple_New( MAX_MTEX );
	if( !tuple )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create PyTuple" );

	for( i = 0; i < MAX_MTEX; ++i ) {
		struct MTex *mtex = self->world->mtex[i];
		if( mtex ) {
			PyTuple_SET_ITEM( tuple, i, MTex_CreatePyObject( mtex, ID_WO ) );
		} else {
			Py_INCREF( Py_None );
			PyTuple_SET_ITEM( tuple, i, Py_None );
		}
	}

	return tuple;
}

static int World_setTextures( BPy_World * self, PyObject * value )
{
	int i;

	if( !PyList_Check( value ) && !PyTuple_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
						"expected tuple or list of integers" );

	/* don't allow more than MAX_MTEX items */
	if( PySequence_Size(value) > MAX_MTEX )
		return EXPP_ReturnIntError( PyExc_AttributeError,
						"size of sequence greater than number of allowed textures" );

	/* get a fast sequence; in Python 2.5, this just return the original
	 * list or tuple and INCREFs it, so we must DECREF */
	value = PySequence_Fast( value, "" );

	/* check the list for valid entries */
	for( i= 0; i < PySequence_Size(value) ; ++i ) {
		PyObject *item = PySequence_Fast_GET_ITEM( value, i );
		if( item == Py_None || ( BPy_MTex_Check( item ) &&
						((BPy_MTex *)item)->type == ID_WO ) ) {
			continue;
		} else {
			Py_DECREF(value);
			return EXPP_ReturnIntError( PyExc_TypeError,
					"expected tuple or list containing world MTex objects and NONE" );
		}
	}

	/* for each MTex object, copy to this structure */
	for( i= 0; i < PySequence_Size(value) ; ++i ) {
		PyObject *item = PySequence_Fast_GET_ITEM( value, i );
		struct MTex *mtex = self->world->mtex[i];
		if( item != Py_None ) {
			BPy_MTex *obj = (BPy_MTex *)item;

			/* if MTex is already at this location, just skip it */
			if( obj->mtex == mtex )	continue;

			/* create a new entry if needed, otherwise update reference count
			 * for texture that is being replaced */
			if( !mtex )
				mtex = self->world->mtex[i] = add_mtex(  );
			else
				mtex->tex->id.us--;

			/* copy the data */
			mtex->tex = obj->mtex->tex;
			id_us_plus( &mtex->tex->id );
			mtex->texco = obj->mtex->texco;
			mtex->mapto = obj->mtex->mapto;
		}
	}

	/* now go back and free any entries now marked as None */
	for( i= 0; i < PySequence_Size(value) ; ++i ) {
		PyObject *item = PySequence_Fast_GET_ITEM( value, i );
		struct MTex *mtex = self->world->mtex[i];
		if( item == Py_None && mtex ) {
			mtex->tex->id.us--;
			MEM_freeN( mtex );
			self->world->mtex[i] = NULL;
		} 
	}

	Py_DECREF(value);
	return 0;
}
