/* 
 * $Id$
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
 * Contributor(s): Willian P. Germano, Nathan Letwory
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/


#include <stdio.h>

#include <BKE_main.h>
#include <BKE_global.h>
#include <BKE_object.h>
#include <BKE_library.h>
#include <BLI_blenlib.h>
#include <BIF_space.h>
#include <BSE_editipo.h>
#include <mydevice.h>

#include "Lamp.h"
#include "Ipo.h"

#include "constant.h"
#include "rgbTuple.h"
#include "gen_utils.h"

/*****************************************************************************/
/* Python BPy_Lamp defaults:                                                 */
/*****************************************************************************/

/* Lamp types */

/* NOTE:
 these are the same values as LA_* from DNA_lamp_types.h
 is there some reason we are not simply using those #defines?
 s. swaney 8-oct-2004
*/

#define EXPP_LAMP_TYPE_LAMP 0
#define EXPP_LAMP_TYPE_SUN  1
#define EXPP_LAMP_TYPE_SPOT 2
#define EXPP_LAMP_TYPE_HEMI 3
#define EXPP_LAMP_TYPE_AREA 4
#define EXPP_LAMP_TYPE_YF_PHOTON 5
/*
  define a constant to keep magic numbers out of the code
  this value should be equal to the last EXPP_LAMP_TYPE_*
*/
#define EXPP_LAMP_TYPE_MAX  5

/* Lamp mode flags */

#define EXPP_LAMP_MODE_SHADOWS       1
#define EXPP_LAMP_MODE_HALO          2
#define EXPP_LAMP_MODE_LAYER         4
#define EXPP_LAMP_MODE_QUAD          8
#define EXPP_LAMP_MODE_NEGATIVE     16
#define EXPP_LAMP_MODE_ONLYSHADOW   32
#define EXPP_LAMP_MODE_SPHERE       64
#define EXPP_LAMP_MODE_SQUARE      128
#define EXPP_LAMP_MODE_TEXTURE     256
#define EXPP_LAMP_MODE_OSATEX      512
#define EXPP_LAMP_MODE_DEEPSHADOW 1024
#define EXPP_LAMP_MODE_NODIFFUSE  2048
#define EXPP_LAMP_MODE_NOSPECULAR 4096
/* Lamp MIN, MAX values */

#define EXPP_LAMP_SAMPLES_MIN 1
#define EXPP_LAMP_SAMPLES_MAX 16
#define EXPP_LAMP_BUFFERSIZE_MIN 512
#define EXPP_LAMP_BUFFERSIZE_MAX 5120
#define EXPP_LAMP_ENERGY_MIN  0.0
#define EXPP_LAMP_ENERGY_MAX 10.0
#define EXPP_LAMP_DIST_MIN    0.1
#define EXPP_LAMP_DIST_MAX 5000.0
#define EXPP_LAMP_SPOTSIZE_MIN   1.0
#define EXPP_LAMP_SPOTSIZE_MAX 180.0
#define EXPP_LAMP_SPOTBLEND_MIN 0.00
#define EXPP_LAMP_SPOTBLEND_MAX 1.00
#define EXPP_LAMP_CLIPSTART_MIN    0.1
#define EXPP_LAMP_CLIPSTART_MAX 1000.0
#define EXPP_LAMP_CLIPEND_MIN    1.0
#define EXPP_LAMP_CLIPEND_MAX 5000.0
#define EXPP_LAMP_BIAS_MIN 0.01
#define EXPP_LAMP_BIAS_MAX 5.00
#define EXPP_LAMP_SOFTNESS_MIN   1.0
#define EXPP_LAMP_SOFTNESS_MAX 100.0
#define EXPP_LAMP_HALOINT_MIN 0.0
#define EXPP_LAMP_HALOINT_MAX 5.0
#define EXPP_LAMP_HALOSTEP_MIN  0
#define EXPP_LAMP_HALOSTEP_MAX 12
#define EXPP_LAMP_QUAD1_MIN 0.0
#define EXPP_LAMP_QUAD1_MAX 1.0
#define EXPP_LAMP_QUAD2_MIN 0.0
#define EXPP_LAMP_QUAD2_MAX 1.0
#define EXPP_LAMP_COL_MIN 0.0
#define EXPP_LAMP_COL_MAX 1.0

#define IPOKEY_RGB       0
#define IPOKEY_ENERGY    1
#define IPOKEY_SPOTSIZE  2
#define IPOKEY_OFFSET    3
#define IPOKEY_SIZE      4

/*****************************************************************************/
/* Python API function prototypes for the Lamp module.                       */
/*****************************************************************************/
static PyObject *M_Lamp_New( PyObject * self, PyObject * args,
			     PyObject * keywords );
static PyObject *M_Lamp_Get( PyObject * self, PyObject * args );

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.Lamp.__doc__                                                      */
/*****************************************************************************/
static char M_Lamp_doc[] = "The Blender Lamp module\n\n\
This module provides control over **Lamp Data** objects in Blender.\n\n\
Example::\n\n\
  from Blender import Lamp\n\
  l = Lamp.New('Spot')            # create new 'Spot' lamp data\n\
  l.setMode('square', 'shadow')   # set these two lamp mode flags\n\
  ob = Object.New('Lamp')         # create new lamp object\n\
  ob.link(l)                      # link lamp obj with lamp data\n";

static char M_Lamp_New_doc[] = "Lamp.New (type = 'Lamp', name = 'LampData'):\n\
        Return a new Lamp Data object with the given type and name.";

static char M_Lamp_Get_doc[] = "Lamp.Get (name = None):\n\
        Return the Lamp Data with the given name, None if not found, or\n\
        Return a list with all Lamp Data objects in the current scene,\n\
        if no argument was given.";

/*****************************************************************************/
/* Python method structure definition for Blender.Lamp module:               */
/*****************************************************************************/
struct PyMethodDef M_Lamp_methods[] = {
	{"New", ( PyCFunction ) M_Lamp_New, METH_VARARGS | METH_KEYWORDS,
	 M_Lamp_New_doc},
	{"Get", M_Lamp_Get, METH_VARARGS, M_Lamp_Get_doc},
	{"get", M_Lamp_Get, METH_VARARGS, M_Lamp_Get_doc},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python BPy_Lamp methods declarations:                                     */
/*****************************************************************************/
static PyObject *Lamp_getName( BPy_Lamp * self );
static PyObject *Lamp_getType( BPy_Lamp * self );
static PyObject *Lamp_getMode( BPy_Lamp * self );
static PyObject *Lamp_getSamples( BPy_Lamp * self );
static PyObject *Lamp_getBufferSize( BPy_Lamp * self );
static PyObject *Lamp_getHaloStep( BPy_Lamp * self );
static PyObject *Lamp_getEnergy( BPy_Lamp * self );
static PyObject *Lamp_getDist( BPy_Lamp * self );
static PyObject *Lamp_getSpotSize( BPy_Lamp * self );
static PyObject *Lamp_getSpotBlend( BPy_Lamp * self );
static PyObject *Lamp_getClipStart( BPy_Lamp * self );
static PyObject *Lamp_getClipEnd( BPy_Lamp * self );
static PyObject *Lamp_getBias( BPy_Lamp * self );
static PyObject *Lamp_getSoftness( BPy_Lamp * self );
static PyObject *Lamp_getHaloInt( BPy_Lamp * self );
static PyObject *Lamp_getQuad1( BPy_Lamp * self );
static PyObject *Lamp_getQuad2( BPy_Lamp * self );
static PyObject *Lamp_getCol( BPy_Lamp * self );
static PyObject *Lamp_getIpo( BPy_Lamp * self );
static PyObject *Lamp_clearIpo( BPy_Lamp * self );
static PyObject *Lamp_setIpo( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_insertIpoKey( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_setName( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_setType( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_setIntType( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_setMode( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_setIntMode( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_setSamples( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_setBufferSize( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_setHaloStep( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_setEnergy( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_setDist( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_setSpotSize( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_setSpotBlend( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_setClipStart( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_setClipEnd( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_setBias( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_setSoftness( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_setHaloInt( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_setQuad1( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_setQuad2( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_setCol( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_setColorComponent( BPy_Lamp * self, char *key,
					 PyObject * args );
static PyObject *Lamp_getScriptLinks( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_addScriptLink( BPy_Lamp * self, PyObject * args );
static PyObject *Lamp_clearScriptLinks( BPy_Lamp * self );

/*****************************************************************************/
/* Python BPy_Lamp methods table:                                            */
/*****************************************************************************/
static PyMethodDef BPy_Lamp_methods[] = {
	/* name, method, flags, doc */
	{"getName", ( PyCFunction ) Lamp_getName, METH_NOARGS,
	 "() - return Lamp name"},
	{"getType", ( PyCFunction ) Lamp_getType, METH_NOARGS,
	 "() - return Lamp type - 'Lamp':0, 'Sun':1, 'Spot':2, 'Hemi':3, 'Area':4, 'Photon':5"},
	{"getMode", ( PyCFunction ) Lamp_getMode, METH_NOARGS,
	 "() - return Lamp mode flags (or'ed value)"},
	{"getSamples", ( PyCFunction ) Lamp_getSamples, METH_NOARGS,
	 "() - return Lamp samples value"},
	{"getBufferSize", ( PyCFunction ) Lamp_getBufferSize, METH_NOARGS,
	 "() - return Lamp buffer size value"},
	{"getHaloStep", ( PyCFunction ) Lamp_getHaloStep, METH_NOARGS,
	 "() - return Lamp halo step value"},
	{"getEnergy", ( PyCFunction ) Lamp_getEnergy, METH_NOARGS,
	 "() - return Lamp energy value"},
	{"getDist", ( PyCFunction ) Lamp_getDist, METH_NOARGS,
	 "() - return Lamp clipping distance value"},
	{"getSpotSize", ( PyCFunction ) Lamp_getSpotSize, METH_NOARGS,
	 "() - return Lamp spot size value"},
	{"getSpotBlend", ( PyCFunction ) Lamp_getSpotBlend, METH_NOARGS,
	 "() - return Lamp spot blend value"},
	{"getClipStart", ( PyCFunction ) Lamp_getClipStart, METH_NOARGS,
	 "() - return Lamp clip start value"},
	{"getClipEnd", ( PyCFunction ) Lamp_getClipEnd, METH_NOARGS,
	 "() - return Lamp clip end value"},
	{"getBias", ( PyCFunction ) Lamp_getBias, METH_NOARGS,
	 "() - return Lamp bias value"},
	{"getSoftness", ( PyCFunction ) Lamp_getSoftness, METH_NOARGS,
	 "() - return Lamp softness value"},
	{"getHaloInt", ( PyCFunction ) Lamp_getHaloInt, METH_NOARGS,
	 "() - return Lamp halo intensity value"},
	{"getQuad1", ( PyCFunction ) Lamp_getQuad1, METH_NOARGS,
	 "() - return light intensity value #1 for a Quad Lamp"},
	{"getQuad2", ( PyCFunction ) Lamp_getQuad2, METH_NOARGS,
	 "() - return light intensity value #2 for a Quad Lamp"},
	{"getCol", ( PyCFunction ) Lamp_getCol, METH_NOARGS,
	 "() - return light rgb color triplet"},
	{"setName", ( PyCFunction ) Lamp_setName, METH_VARARGS,
	 "(str) - rename Lamp"},
	{"setType", ( PyCFunction ) Lamp_setType, METH_VARARGS,
	 "(str) - change Lamp type, which can be 'Lamp', 'Sun', 'Spot', 'Hemi', 'Area', 'Photon'"},
	{"setMode", ( PyCFunction ) Lamp_setMode, METH_VARARGS,
	 "([up to eight str's]) - Set Lamp mode flag(s)"},
	{"setSamples", ( PyCFunction ) Lamp_setSamples, METH_VARARGS,
	 "(int) - change Lamp samples value"},
	{"setBufferSize", ( PyCFunction ) Lamp_setBufferSize, METH_VARARGS,
	 "(int) - change Lamp buffer size value"},
	{"setHaloStep", ( PyCFunction ) Lamp_setHaloStep, METH_VARARGS,
	 "(int) - change Lamp halo step value"},
	{"setEnergy", ( PyCFunction ) Lamp_setEnergy, METH_VARARGS,
	 "(float) - change Lamp energy value"},
	{"setDist", ( PyCFunction ) Lamp_setDist, METH_VARARGS,
	 "(float) - change Lamp clipping distance value"},
	{"setSpotSize", ( PyCFunction ) Lamp_setSpotSize, METH_VARARGS,
	 "(float) - change Lamp spot size value"},
	{"setSpotBlend", ( PyCFunction ) Lamp_setSpotBlend, METH_VARARGS,
	 "(float) - change Lamp spot blend value"},
	{"setClipStart", ( PyCFunction ) Lamp_setClipStart, METH_VARARGS,
	 "(float) - change Lamp clip start value"},
	{"setClipEnd", ( PyCFunction ) Lamp_setClipEnd, METH_VARARGS,
	 "(float) - change Lamp clip end value"},
	{"setBias", ( PyCFunction ) Lamp_setBias, METH_VARARGS,
	 "(float) - change Lamp draw size value"},
	{"setSoftness", ( PyCFunction ) Lamp_setSoftness, METH_VARARGS,
	 "(float) - change Lamp softness value"},
	{"setHaloInt", ( PyCFunction ) Lamp_setHaloInt, METH_VARARGS,
	 "(float) - change Lamp halo intensity value"},
	{"setQuad1", ( PyCFunction ) Lamp_setQuad1, METH_VARARGS,
	 "(float) - change light intensity value #1 for a Quad Lamp"},
	{"setQuad2", ( PyCFunction ) Lamp_setQuad2, METH_VARARGS,
	 "(float) - change light intensity value #2 for a Quad Lamp"},
	{"setCol", ( PyCFunction ) Lamp_setCol, METH_VARARGS,
	 "(f,f,f) or ([f,f,f]) - change light's rgb color triplet"},
	{"getScriptLinks", ( PyCFunction ) Lamp_getScriptLinks, METH_VARARGS,
	 "(eventname) - Get a list of this lamp's scriptlinks (Text names) "
	 "of the given type\n"
	 "(eventname) - string: FrameChanged or Redraw."},
	{"addScriptLink", ( PyCFunction ) Lamp_addScriptLink, METH_VARARGS,
	 "(text, evt) - Add a new lamp scriptlink.\n"
	 "(text) - string: an existing Blender Text name;\n"
	 "(evt) string: FrameChanged or Redraw."},
	{"clearScriptLinks", ( PyCFunction ) Lamp_clearScriptLinks,
	 METH_NOARGS,
	 "() - Delete all scriptlinks from this lamp."},
	{"getIpo", ( PyCFunction ) Lamp_getIpo, METH_NOARGS,
	 "() - get IPO for this lamp"},
	{"clearIpo", ( PyCFunction ) Lamp_clearIpo, METH_NOARGS,
	 "() - unlink the IPO for this lamp"},
	{"setIpo", ( PyCFunction ) Lamp_setIpo, METH_VARARGS,
	 "( lamp-ipo ) - link an IPO to this lamp"},
	 {"insertIpoKey", ( PyCFunction ) Lamp_insertIpoKey, METH_VARARGS,
	 "( Lamp IPO type ) - Inserts a key into IPO"},

	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python TypeLamp callback function prototypes:                             */
/*****************************************************************************/
static void Lamp_dealloc( BPy_Lamp * lamp );
static PyObject *Lamp_getAttr( BPy_Lamp * lamp, char *name );
static int Lamp_setAttr( BPy_Lamp * lamp, char *name, PyObject * v );
static int Lamp_compare( BPy_Lamp * a, BPy_Lamp * b );
static PyObject *Lamp_repr( BPy_Lamp * lamp );


/*****************************************************************************/
/* Python TypeLamp structure definition:                                     */
/*****************************************************************************/
PyTypeObject Lamp_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,	/* ob_size */
	"Blender Lamp",		/* tp_name */
	sizeof( BPy_Lamp ),	/* tp_basicsize */
	0,			/* tp_itemsize */
	/* methods */
	( destructor ) Lamp_dealloc,	/* tp_dealloc */
	0,			/* tp_print */
	( getattrfunc ) Lamp_getAttr,	/* tp_getattr */
	( setattrfunc ) Lamp_setAttr,	/* tp_setattr */
	( cmpfunc ) Lamp_compare,	/* tp_compare */
	( reprfunc ) Lamp_repr,	/* tp_repr */
	0,			/* tp_as_number */
	0,			/* tp_as_sequence */
	0,			/* tp_as_mapping */
	0,			/* tp_as_hash */
	0, 0, 0, 0, 0, 0,
	0,			/* tp_doc */
	0, 0, 0, 0, 0, 0,
	BPy_Lamp_methods,	/* tp_methods */
	0,			/* tp_members */
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

/*****************************************************************************/
/* Function:              M_Lamp_New                                         */
/* Python equivalent:     Blender.Lamp.New                                   */
/*****************************************************************************/
static PyObject *M_Lamp_New( PyObject * self, PyObject * args,
			     PyObject * keywords )
{
	char *type_str = "Lamp";
	char *name_str = "LampData";
	static char *kwlist[] = { "type_str", "name_str", NULL };
	BPy_Lamp *py_lamp;	/* for Lamp Data object wrapper in Python */
	Lamp *bl_lamp;		/* for actual Lamp Data we create in Blender */
	char buf[21];

	if( !PyArg_ParseTupleAndKeywords( args, keywords, "|ss", kwlist,
					  &type_str, &name_str ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected string(s) or empty argument" ) );

	bl_lamp = add_lamp(  );	/* first create in Blender */
	if( bl_lamp )		/* now create the wrapper obj in Python */
		py_lamp = ( BPy_Lamp * ) Lamp_CreatePyObject( bl_lamp );
	else
		return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"couldn't create Lamp Data in Blender" ) );

	/* let's return user count to zero, because ... */
	bl_lamp->id.us = 0;	/* ... add_lamp() incref'ed it */

	if( py_lamp == NULL )
		return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
						"couldn't create Lamp Data object" ) );

	if( strcmp( type_str, "Lamp" ) == 0 )
		bl_lamp->type = ( short ) EXPP_LAMP_TYPE_LAMP;
	else if( strcmp( type_str, "Sun" ) == 0 )
		bl_lamp->type = ( short ) EXPP_LAMP_TYPE_SUN;
	else if( strcmp( type_str, "Spot" ) == 0 )
		bl_lamp->type = ( short ) EXPP_LAMP_TYPE_SPOT;
	else if( strcmp( type_str, "Hemi" ) == 0 )
		bl_lamp->type = ( short ) EXPP_LAMP_TYPE_HEMI;
	else if( strcmp( type_str, "Area" ) == 0 )
		bl_lamp->type = ( short ) EXPP_LAMP_TYPE_AREA;
	else if( strcmp( type_str, "Photon" ) == 0 )
		bl_lamp->type = ( short ) EXPP_LAMP_TYPE_YF_PHOTON;
	else
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"unknown lamp type" ) );

	if( strcmp( name_str, "LampData" ) == 0 )
		return ( PyObject * ) py_lamp;
	else {			/* user gave us a name for the lamp, use it */
		PyOS_snprintf( buf, sizeof( buf ), "%s", name_str );
		rename_id( &bl_lamp->id, buf );
	}

	return ( PyObject * ) py_lamp;
}

/*****************************************************************************/
/* Function:              M_Lamp_Get                                         */
/* Python equivalent:     Blender.Lamp.Get                                   */
/* Description:           Receives a string and returns the lamp data obj    */
/*                        whose name matches the string.  If no argument is  */
/*                        passed in, a list of all lamp data names in the    */
/*                        current scene is returned.                         */
/*****************************************************************************/
static PyObject *M_Lamp_Get( PyObject * self, PyObject * args )
{
	char *name = NULL;
	Lamp *lamp_iter;

	if( !PyArg_ParseTuple( args, "|s", &name ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected string argument (or nothing)" ) );

	lamp_iter = G.main->lamp.first;

	if( name ) {		/* (name) - Search lamp by name */

		BPy_Lamp *wanted_lamp = NULL;

		while( ( lamp_iter ) && ( wanted_lamp == NULL ) ) {

			if( strcmp( name, lamp_iter->id.name + 2 ) == 0 )
				wanted_lamp =
					( BPy_Lamp * )
					Lamp_CreatePyObject( lamp_iter );

			lamp_iter = lamp_iter->id.next;
		}

		if( wanted_lamp == NULL ) { /* Requested lamp doesn't exist */
			char error_msg[64];
			PyOS_snprintf( error_msg, sizeof( error_msg ),
				       "Lamp \"%s\" not found", name );
			return ( EXPP_ReturnPyObjError
				 ( PyExc_NameError, error_msg ) );
		}

		return ( PyObject * ) wanted_lamp;
	}

	else {		/* () - return a list of all lamps in the scene */
		int index = 0;
		PyObject *lamplist, *pyobj;

		lamplist = PyList_New( BLI_countlist( &( G.main->lamp ) ) );

		if( lamplist == NULL )
			return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
							"couldn't create PyList" ) );

		while( lamp_iter ) {
			pyobj = Lamp_CreatePyObject( lamp_iter );

			if( !pyobj )
				return ( EXPP_ReturnPyObjError
					 ( PyExc_MemoryError,
					   "couldn't create PyString" ) );

			PyList_SET_ITEM( lamplist, index, pyobj );

			lamp_iter = lamp_iter->id.next;
			index++;
		}

		return lamplist;
	}
}

static PyObject *Lamp_TypesDict( void )
{	/* create the Blender.Lamp.Types constant dict */
	PyObject *Types = M_constant_New(  );

	if( Types ) {
		BPy_constant *c = ( BPy_constant * ) Types;

		constant_insert( c, "Lamp",
				 PyInt_FromLong( EXPP_LAMP_TYPE_LAMP ) );
		constant_insert( c, "Sun",
				 PyInt_FromLong( EXPP_LAMP_TYPE_SUN ) );
		constant_insert( c, "Spot",
				 PyInt_FromLong( EXPP_LAMP_TYPE_SPOT ) );
		constant_insert( c, "Hemi",
				 PyInt_FromLong( EXPP_LAMP_TYPE_HEMI ) );
		constant_insert( c, "Area",
				 PyInt_FromLong( EXPP_LAMP_TYPE_AREA ) );
		constant_insert( c, "Photon",
				 PyInt_FromLong( EXPP_LAMP_TYPE_YF_PHOTON ) );
	}

	return Types;
}

static PyObject *Lamp_ModesDict( void )
{			/* create the Blender.Lamp.Modes constant dict */
	PyObject *Modes = M_constant_New(  );

	if( Modes ) {
		BPy_constant *c = ( BPy_constant * ) Modes;

		constant_insert( c, "Shadows",
				 PyInt_FromLong( EXPP_LAMP_MODE_SHADOWS ) );
		constant_insert( c, "Halo",
				 PyInt_FromLong( EXPP_LAMP_MODE_HALO ) );
		constant_insert( c, "Layer",
				 PyInt_FromLong( EXPP_LAMP_MODE_LAYER ) );
		constant_insert( c, "Quad",
				 PyInt_FromLong( EXPP_LAMP_MODE_QUAD ) );
		constant_insert( c, "Negative",
				 PyInt_FromLong( EXPP_LAMP_MODE_NEGATIVE ) );
		constant_insert( c, "Sphere",
				 PyInt_FromLong( EXPP_LAMP_MODE_SPHERE ) );
		constant_insert( c, "Square",
				 PyInt_FromLong( EXPP_LAMP_MODE_SQUARE ) );
		constant_insert( c, "OnlyShadow",
				 PyInt_FromLong( EXPP_LAMP_MODE_ONLYSHADOW ) );
		constant_insert( c, "NoDiffuse",
				 PyInt_FromLong( EXPP_LAMP_MODE_NODIFFUSE ) );
		constant_insert( c, "NoSpecular",
				 PyInt_FromLong( EXPP_LAMP_MODE_NOSPECULAR ) );
	}

	return Modes;
}

/*****************************************************************************/
/* Function:              Lamp_Init                                          */
/*****************************************************************************/
/* Needed by the Blender module, to register the Blender.Lamp submodule */
PyObject *Lamp_Init( void )
{
	PyObject *submodule, *Types, *Modes;

	Lamp_Type.ob_type = &PyType_Type;

	Types = Lamp_TypesDict(  );
	Modes = Lamp_ModesDict(  );

	submodule =
		Py_InitModule3( "Blender.Lamp", M_Lamp_methods, M_Lamp_doc );

	if( Types )
		PyModule_AddObject( submodule, "Types", Types );
	if( Modes )
		PyModule_AddObject( submodule, "Modes", Modes );

	PyModule_AddIntConstant( submodule, "RGB",      IPOKEY_RGB );
	PyModule_AddIntConstant( submodule, "ENERGY",   IPOKEY_ENERGY );
	PyModule_AddIntConstant( submodule, "SPOTSIZE", IPOKEY_SPOTSIZE );
	PyModule_AddIntConstant( submodule, "OFFSET",   IPOKEY_OFFSET );
	PyModule_AddIntConstant( submodule, "SIZE",     IPOKEY_SIZE );
	
	return submodule;
}

/* Three Python Lamp_Type helper functions needed by the Object module: */

/*****************************************************************************/
/* Function:    Lamp_CreatePyObject                                          */
/* Description: This function will create a new BPy_Lamp from an existing    */
/*              Blender lamp structure.                                      */
/*****************************************************************************/
PyObject *Lamp_CreatePyObject( Lamp * lamp )
{
	BPy_Lamp *pylamp;
	float *rgb[3];

	pylamp = ( BPy_Lamp * ) PyObject_NEW( BPy_Lamp, &Lamp_Type );

	if( !pylamp )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create BPy_Lamp object" );

	pylamp->lamp = lamp;

	rgb[0] = &lamp->r;
	rgb[1] = &lamp->g;
	rgb[2] = &lamp->b;

	pylamp->color = ( BPy_rgbTuple * ) rgbTuple_New( rgb );

	return ( PyObject * ) pylamp;
}

/*****************************************************************************/
/* Function:    Lamp_CheckPyObject                                           */
/* Description: This function returns true when the given PyObject is of the */
/*              type Lamp. Otherwise it will return false.                   */
/*****************************************************************************/
int Lamp_CheckPyObject( PyObject * pyobj )
{
	return ( pyobj->ob_type == &Lamp_Type );
}

/*****************************************************************************/
/* Function:    Lamp_FromPyObject                                            */
/* Description: This function returns the Blender lamp from the given        */
/*              PyObject.                                                    */
/*****************************************************************************/
Lamp *Lamp_FromPyObject( PyObject * pyobj )
{
	return ( ( BPy_Lamp * ) pyobj )->lamp;
}

/*****************************************************************************/
/* Description: Returns the lamp with the name specified by the argument     */
/*              name. Note that the calling function has to remove the first */
/*              two characters of the lamp name. These two characters        */
/*              specify the type of the object (OB, ME, WO, ...)             */
/*              The function will return NULL when no lamp with the given    */
/*              name is found.                                               */
/*****************************************************************************/
Lamp *GetLampByName( char *name )
{
	Lamp *lamp_iter;

	lamp_iter = G.main->lamp.first;
	while( lamp_iter ) {
		if( StringEqual( name, GetIdName( &( lamp_iter->id ) ) ) ) {
			return lamp_iter;
		}
		lamp_iter = lamp_iter->id.next;
	}

	/* There is no lamp with the given name */
	return NULL;
}

/*****************************************************************************/
/* Python BPy_Lamp methods:                                                  */
/*****************************************************************************/
static PyObject *Lamp_getName( BPy_Lamp * self )
{
	PyObject *attr = PyString_FromString( self->lamp->id.name + 2 );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Lamp.name attribute" ) );
}

static PyObject *Lamp_getType( BPy_Lamp * self )
{
	PyObject *attr = PyInt_FromLong( self->lamp->type );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Lamp.type attribute" ) );
}

static PyObject *Lamp_getMode( BPy_Lamp * self )
{
	PyObject *attr = PyInt_FromLong( self->lamp->mode );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Lamp.mode attribute" ) );
}

static PyObject *Lamp_getSamples( BPy_Lamp * self )
{
	PyObject *attr = PyInt_FromLong( self->lamp->samp );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Lamp.samples attribute" ) );
}

static PyObject *Lamp_getBufferSize( BPy_Lamp * self )
{
	PyObject *attr = PyInt_FromLong( self->lamp->bufsize );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Lamp.bufferSize attribute" ) );
}

static PyObject *Lamp_getHaloStep( BPy_Lamp * self )
{
	PyObject *attr = PyInt_FromLong( self->lamp->shadhalostep );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Lamp.haloStep attribute" ) );
}

static PyObject *Lamp_getEnergy( BPy_Lamp * self )
{
	PyObject *attr = PyFloat_FromDouble( self->lamp->energy );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Lamp.energy attribute" ) );
}

static PyObject *Lamp_getDist( BPy_Lamp * self )
{
	PyObject *attr = PyFloat_FromDouble( self->lamp->dist );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Lamp.dist attribute" ) );
}

static PyObject *Lamp_getSpotSize( BPy_Lamp * self )
{
	PyObject *attr = PyFloat_FromDouble( self->lamp->spotsize );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Lamp.spotSize attribute" ) );
}

static PyObject *Lamp_getSpotBlend( BPy_Lamp * self )
{
	PyObject *attr = PyFloat_FromDouble( self->lamp->spotblend );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Lamp.spotBlend attribute" ) );
}

static PyObject *Lamp_getClipStart( BPy_Lamp * self )
{
	PyObject *attr = PyFloat_FromDouble( self->lamp->clipsta );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Lamp.clipStart attribute" ) );
}

static PyObject *Lamp_getClipEnd( BPy_Lamp * self )
{
	PyObject *attr = PyFloat_FromDouble( self->lamp->clipend );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Lamp.clipEnd attribute" ) );
}

static PyObject *Lamp_getBias( BPy_Lamp * self )
{
	PyObject *attr = PyFloat_FromDouble( self->lamp->bias );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Lamp.bias attribute" ) );
}

static PyObject *Lamp_getSoftness( BPy_Lamp * self )
{
	PyObject *attr = PyFloat_FromDouble( self->lamp->soft );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Lamp.softness attribute" ) );
}

static PyObject *Lamp_getHaloInt( BPy_Lamp * self )
{
	PyObject *attr = PyFloat_FromDouble( self->lamp->haint );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Lamp.haloInt attribute" ) );
}

static PyObject *Lamp_getQuad1( BPy_Lamp * self )
{				/* should we complain if Lamp is not of type Quad? */
	PyObject *attr = PyFloat_FromDouble( self->lamp->att1 );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Lamp.quad1 attribute" ) );
}

static PyObject *Lamp_getQuad2( BPy_Lamp * self )
{			/* should we complain if Lamp is not of type Quad? */
	PyObject *attr = PyFloat_FromDouble( self->lamp->att2 );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Lamp.quad2 attribute" ) );
}

static PyObject *Lamp_getCol( BPy_Lamp * self )
{
	return rgbTuple_getCol( self->color );
}

static PyObject *Lamp_setName( BPy_Lamp * self, PyObject * args )
{
	char *name = NULL;
	char buf[21];

	if( !PyArg_ParseTuple( args, "s", &name ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected string argument" ) );

	PyOS_snprintf( buf, sizeof( buf ), "%s", name );

	rename_id( &self->lamp->id, buf );

	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *Lamp_setType( BPy_Lamp * self, PyObject * args )
{
	char *type;

	if( !PyArg_ParseTuple( args, "s", &type ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected string argument" ) );

	if( strcmp( type, "Lamp" ) == 0 )
		self->lamp->type = ( short ) EXPP_LAMP_TYPE_LAMP;
	else if( strcmp( type, "Sun" ) == 0 )
		self->lamp->type = ( short ) EXPP_LAMP_TYPE_SUN;
	else if( strcmp( type, "Spot" ) == 0 )
		self->lamp->type = ( short ) EXPP_LAMP_TYPE_SPOT;
	else if( strcmp( type, "Hemi" ) == 0 )
		self->lamp->type = ( short ) EXPP_LAMP_TYPE_HEMI;
	else if( strcmp( type, "Area" ) == 0 )
		self->lamp->type = ( short ) EXPP_LAMP_TYPE_AREA;
	else if( strcmp( type, "Photon" ) == 0 )
		self->lamp->type = ( short ) EXPP_LAMP_TYPE_YF_PHOTON;
	else
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"unknown lamp type" ) );

	Py_INCREF( Py_None );
	return Py_None;
}

/* This one is 'private'. It is not really a method, just a helper function for
 * when script writers use Lamp.type = t instead of Lamp.setType(t), since in
 * the first case t shoud be an int and in the second it should be a string. So
 * while the method setType expects a string  or an empty
 * argument, this function should receive an int (0 or 1). */
static PyObject *Lamp_setIntType( BPy_Lamp * self, PyObject * args )
{
	short value;

	if( !PyArg_ParseTuple( args, "h", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected int argument in [0,5]" ) );

	if( value >= 0 && value <= EXPP_LAMP_TYPE_MAX )
		self->lamp->type = value;
	else
		return ( EXPP_ReturnPyObjError( PyExc_ValueError,
						"expected int argument in [0,5]" ) );

	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *Lamp_setMode( BPy_Lamp * self, PyObject * args )
{
	char *m[10] =
		{ NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
	short i, flag = 0;

	if( !PyArg_ParseTuple( args, "|ssssssss", &m[0], &m[1], &m[2],
			       &m[3], &m[4], &m[5], &m[6], &m[7], &m[8],
			       &m[9] ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_AttributeError,
			   "expected from none to 10 string argument(s)" ) );

	for( i = 0; i < 10; i++ ) {
		if( m[i] == NULL )
			break;
		if( strcmp( m[i], "Shadows" ) == 0 )
			flag |= ( short ) EXPP_LAMP_MODE_SHADOWS;
		else if( strcmp( m[i], "Halo" ) == 0 )
			flag |= ( short ) EXPP_LAMP_MODE_HALO;
		else if( strcmp( m[i], "Layer" ) == 0 )
			flag |= ( short ) EXPP_LAMP_MODE_LAYER;
		else if( strcmp( m[i], "Quad" ) == 0 )
			flag |= ( short ) EXPP_LAMP_MODE_QUAD;
		else if( strcmp( m[i], "Negative" ) == 0 )
			flag |= ( short ) EXPP_LAMP_MODE_NEGATIVE;
		else if( strcmp( m[i], "OnlyShadow" ) == 0 )
			flag |= ( short ) EXPP_LAMP_MODE_ONLYSHADOW;
		else if( strcmp( m[i], "Sphere" ) == 0 )
			flag |= ( short ) EXPP_LAMP_MODE_SPHERE;
		else if( strcmp( m[i], "Square" ) == 0 )
			flag |= ( short ) EXPP_LAMP_MODE_SQUARE;
		else if( strcmp( m[i], "NoDiffuse" ) == 0 )
			flag |= ( short ) EXPP_LAMP_MODE_NODIFFUSE;
		else if( strcmp( m[i], "NoSpecular" ) == 0 )
			flag |= ( short ) EXPP_LAMP_MODE_NOSPECULAR;
		else
			return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
							"unknown lamp flag argument" ) );
	}

	self->lamp->mode = flag;

	Py_INCREF( Py_None );
	return Py_None;
}

/* Another helper function, for the same reason.
 * (See comment before Lamp_setIntType above). */
static PyObject *Lamp_setIntMode( BPy_Lamp * self, PyObject * args )
{
	short value;

	if( !PyArg_ParseTuple( args, "h", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected int argument" ) );

/* well, with so many flag bits, we just accept any short int, no checking */
	self->lamp->mode = value;

	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *Lamp_setSamples( BPy_Lamp * self, PyObject * args )
{
	short value;

	if( !PyArg_ParseTuple( args, "h", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected int argument in [1,16]" ) );

	self->lamp->samp = EXPP_ClampInt( value,
					  EXPP_LAMP_SAMPLES_MIN,
					  EXPP_LAMP_SAMPLES_MAX );

	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *Lamp_setBufferSize( BPy_Lamp * self, PyObject * args )
{
	short value;

	if( !PyArg_ParseTuple( args, "h", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected int argument in [512, 5120]" ) );

	self->lamp->bufsize = EXPP_ClampInt( value,
					     EXPP_LAMP_BUFFERSIZE_MIN,
					     EXPP_LAMP_BUFFERSIZE_MAX );

	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *Lamp_setHaloStep( BPy_Lamp * self, PyObject * args )
{
	short value;

	if( !PyArg_ParseTuple( args, "h", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected int argument in [0,12]" ) );

	self->lamp->shadhalostep = EXPP_ClampInt( value,
						  EXPP_LAMP_HALOSTEP_MIN,
						  EXPP_LAMP_HALOSTEP_MAX );

	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *Lamp_setEnergy( BPy_Lamp * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument" ) );

	self->lamp->energy = EXPP_ClampFloat( value,
					      EXPP_LAMP_ENERGY_MIN,
					      EXPP_LAMP_ENERGY_MAX );

	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *Lamp_setDist( BPy_Lamp * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument" ) );

	self->lamp->dist = EXPP_ClampFloat( value,
					    EXPP_LAMP_DIST_MIN,
					    EXPP_LAMP_DIST_MAX );

	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *Lamp_setSpotSize( BPy_Lamp * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument" ) );

	self->lamp->spotsize = EXPP_ClampFloat( value,
						EXPP_LAMP_SPOTSIZE_MIN,
						EXPP_LAMP_SPOTSIZE_MAX );

	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *Lamp_setSpotBlend( BPy_Lamp * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument" ) );

	self->lamp->spotblend = EXPP_ClampFloat( value,
						 EXPP_LAMP_SPOTBLEND_MIN,
						 EXPP_LAMP_SPOTBLEND_MAX );

	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *Lamp_setClipStart( BPy_Lamp * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument" ) );

	self->lamp->clipsta = EXPP_ClampFloat( value,
					       EXPP_LAMP_CLIPSTART_MIN,
					       EXPP_LAMP_CLIPSTART_MAX );

	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *Lamp_setClipEnd( BPy_Lamp * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument" ) );

	self->lamp->clipend = EXPP_ClampFloat( value,
					       EXPP_LAMP_CLIPEND_MIN,
					       EXPP_LAMP_CLIPEND_MAX );

	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *Lamp_setBias( BPy_Lamp * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument" ) );

	self->lamp->bias = EXPP_ClampFloat( value,
					    EXPP_LAMP_BIAS_MIN,
					    EXPP_LAMP_BIAS_MAX );

	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *Lamp_setSoftness( BPy_Lamp * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument" ) );

	self->lamp->soft = EXPP_ClampFloat( value,
					    EXPP_LAMP_SOFTNESS_MIN,
					    EXPP_LAMP_SOFTNESS_MAX );

	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *Lamp_setHaloInt( BPy_Lamp * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument" ) );

	self->lamp->haint = EXPP_ClampFloat( value,
					     EXPP_LAMP_HALOINT_MIN,
					     EXPP_LAMP_HALOINT_MAX );

	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *Lamp_setQuad1( BPy_Lamp * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument" ) );

	self->lamp->att1 = EXPP_ClampFloat( value,
					    EXPP_LAMP_QUAD1_MIN,
					    EXPP_LAMP_QUAD1_MAX );

	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *Lamp_setQuad2( BPy_Lamp * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument" ) );

	self->lamp->att2 = EXPP_ClampFloat( value,
					    EXPP_LAMP_QUAD2_MIN,
					    EXPP_LAMP_QUAD2_MAX );

	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *Lamp_setColorComponent( BPy_Lamp * self, char *key,
					 PyObject * args )
{				/* for compatibility with old bpython */
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument in [0.0, 1.0]" ) );

	value = EXPP_ClampFloat( value, EXPP_LAMP_COL_MIN, EXPP_LAMP_COL_MAX );

	if( !strcmp( key, "R" ) )
		self->lamp->r = value;
	else if( !strcmp( key, "G" ) )
		self->lamp->g = value;
	else if( !strcmp( key, "B" ) )
		self->lamp->b = value;

	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *Lamp_setCol( BPy_Lamp * self, PyObject * args )
{
	return rgbTuple_setCol( self->color, args );
}

/* lamp.addScriptLink */
static PyObject *Lamp_addScriptLink( BPy_Lamp * self, PyObject * args )
{
	Lamp *lamp = self->lamp;
	ScriptLink *slink = NULL;

	slink = &( lamp )->scriptlink;

	if( !EXPP_addScriptLink( slink, args, 0 ) )
		return EXPP_incr_ret( Py_None );
	else
		return NULL;
}

/* lamp.clearScriptLinks */
static PyObject *Lamp_clearScriptLinks( BPy_Lamp * self )
{
	Lamp *lamp = self->lamp;
	ScriptLink *slink = NULL;

	slink = &( lamp )->scriptlink;

	return EXPP_incr_ret( Py_BuildValue
			      ( "i", EXPP_clearScriptLinks( slink ) ) );
}

/* mat.getScriptLinks */
static PyObject *Lamp_getScriptLinks( BPy_Lamp * self, PyObject * args )
{
	Lamp *lamp = self->lamp;
	ScriptLink *slink = NULL;
	PyObject *ret = NULL;

	slink = &( lamp )->scriptlink;

	ret = EXPP_getScriptLinks( slink, args, 0 );

	if( ret )
		return ret;
	else
		return NULL;
}

/*****************************************************************************/
/* Function:    Lamp_dealloc                                                 */
/* Description: This is a callback function for the BPy_Lamp type. It is     */
/*              the destructor function.                                     */
/*****************************************************************************/
static void Lamp_dealloc( BPy_Lamp * self )
{
	Py_DECREF( self->color );
	PyObject_DEL( self );
}

/*****************************************************************************/
/* Function:    Lamp_getAttr                                                 */
/* Description: This is a callback function for the BPy_Lamp type. It is     */
/*              the function that accesses BPy_Lamp member variables and     */
/*              methods.                                                     */
/*****************************************************************************/
static PyObject *Lamp_getAttr( BPy_Lamp * self, char *name )
{
	PyObject *attr = Py_None;

	if( strcmp( name, "name" ) == 0 )
		attr = PyString_FromString( self->lamp->id.name + 2 );
	else if( strcmp( name, "type" ) == 0 )
		attr = PyInt_FromLong( self->lamp->type );
	else if( strcmp( name, "mode" ) == 0 )
		attr = PyInt_FromLong( self->lamp->mode );
	else if( strcmp( name, "samples" ) == 0 )
		attr = PyInt_FromLong( self->lamp->samp );
	else if( strcmp( name, "bufferSize" ) == 0 )
		attr = PyInt_FromLong( self->lamp->bufsize );
	else if( strcmp( name, "haloStep" ) == 0 )
		attr = PyInt_FromLong( self->lamp->shadhalostep );
	else if( strcmp( name, "R" ) == 0 )
		attr = PyFloat_FromDouble( self->lamp->r );
	else if( strcmp( name, "G" ) == 0 )
		attr = PyFloat_FromDouble( self->lamp->g );
	else if( strcmp( name, "B" ) == 0 )
		attr = PyFloat_FromDouble( self->lamp->b );
	else if( strcmp( name, "col" ) == 0 )
		attr = Lamp_getCol( self );
	else if( strcmp( name, "energy" ) == 0 )
		attr = PyFloat_FromDouble( self->lamp->energy );
	else if( strcmp( name, "dist" ) == 0 )
		attr = PyFloat_FromDouble( self->lamp->dist );
	else if( strcmp( name, "spotSize" ) == 0 )
		attr = PyFloat_FromDouble( self->lamp->spotsize );
	else if( strcmp( name, "spotBlend" ) == 0 )
		attr = PyFloat_FromDouble( self->lamp->spotblend );
	else if( strcmp( name, "clipStart" ) == 0 )
		attr = PyFloat_FromDouble( self->lamp->clipsta );
	else if( strcmp( name, "clipEnd" ) == 0 )
		attr = PyFloat_FromDouble( self->lamp->clipend );
	else if( strcmp( name, "bias" ) == 0 )
		attr = PyFloat_FromDouble( self->lamp->bias );
	else if( strcmp( name, "softness" ) == 0 )
		attr = PyFloat_FromDouble( self->lamp->soft );
	else if( strcmp( name, "haloInt" ) == 0 )
		attr = PyFloat_FromDouble( self->lamp->haint );
	else if( strcmp( name, "quad1" ) == 0 )
		attr = PyFloat_FromDouble( self->lamp->att1 );
	else if( strcmp( name, "quad2" ) == 0 )
		attr = PyFloat_FromDouble( self->lamp->att2 );
	else if( strcmp( name, "users" ) == 0 )
		attr = PyInt_FromLong( self->lamp->id.us );
	
	else if( strcmp( name, "Types" ) == 0 ) {
		attr = Py_BuildValue( "{s:h,s:h,s:h,s:h,s:h,s:h}",
				      "Lamp", EXPP_LAMP_TYPE_LAMP,
				      "Sun", EXPP_LAMP_TYPE_SUN,
				      "Spot", EXPP_LAMP_TYPE_SPOT,
				      "Hemi", EXPP_LAMP_TYPE_HEMI, 
				      "Area", EXPP_LAMP_TYPE_AREA, 
				      "Photon", EXPP_LAMP_TYPE_YF_PHOTON 
			);
	}

	else if( strcmp( name, "Modes" ) == 0 ) {
		attr = Py_BuildValue
			( "{s:h,s:h,s:h,s:h,s:h,s:h,s:h,s:h,s:h,s:h}",
			  "Shadows", EXPP_LAMP_MODE_SHADOWS, "Halo",
			  EXPP_LAMP_MODE_HALO, "Layer", EXPP_LAMP_MODE_LAYER,
			  "Quad", EXPP_LAMP_MODE_QUAD, "Negative",
			  EXPP_LAMP_MODE_NEGATIVE, "OnlyShadow",
			  EXPP_LAMP_MODE_ONLYSHADOW, "Sphere",
			  EXPP_LAMP_MODE_SPHERE, "Square",
			  EXPP_LAMP_MODE_SQUARE, "NoDiffuse",
			  EXPP_LAMP_MODE_NODIFFUSE, "NoSpecular",
			  EXPP_LAMP_MODE_NOSPECULAR );
	}
	
	else if( strcmp( name, "__members__" ) == 0 ) {
		/* 23 entries */
		attr = Py_BuildValue
			( "[s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s]",
			  "name", "type", "mode", "samples", "bufferSize",
			  "haloStep", "R", "G", "B", "energy", "dist",
			  "spotSize", "spotBlend", "clipStart", "clipEnd",
			  "bias", "softness", "haloInt", "quad1", "quad2",
			  "Types", "Modes", "col", "users" );
	}

	if( !attr )
		return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
						"couldn't create PyObject" ) );

	if( attr != Py_None )
		return attr;	/* member attribute found, return it */

	/* not an attribute, search the methods table */
	return Py_FindMethod( BPy_Lamp_methods, ( PyObject * ) self, name );
}

/*****************************************************************************/
/* Function:    Lamp_setAttr                                                 */
/* Description: This is a callback function for the BPy_Lamp type. It is the */
/*              function that changes Lamp Data members values. If this      */
/*              data is linked to a Blender Lamp, it also gets updated.      */
/*****************************************************************************/
static int Lamp_setAttr( BPy_Lamp * self, char *name, PyObject * value )
{
	PyObject *valtuple;
	PyObject *error = NULL;

	valtuple = Py_BuildValue( "(O)", value );	/*the set* functions expect a tuple */

	if( !valtuple )
		return EXPP_ReturnIntError( PyExc_MemoryError,
					    "LampSetAttr: couldn't create tuple" );

	if( strcmp( name, "name" ) == 0 )
		error = Lamp_setName( self, valtuple );
	else if( strcmp( name, "type" ) == 0 )
		error = Lamp_setIntType( self, valtuple );	/* special case */
	else if( strcmp( name, "mode" ) == 0 )
		error = Lamp_setIntMode( self, valtuple );	/* special case */
	else if( strcmp( name, "samples" ) == 0 )
		error = Lamp_setSamples( self, valtuple );
	else if( strcmp( name, "bufferSize" ) == 0 )
		error = Lamp_setBufferSize( self, valtuple );
	else if( strcmp( name, "haloStep" ) == 0 )
		error = Lamp_setHaloStep( self, valtuple );
	else if( strcmp( name, "R" ) == 0 )
		error = Lamp_setColorComponent( self, "R", valtuple );
	else if( strcmp( name, "G" ) == 0 )
		error = Lamp_setColorComponent( self, "G", valtuple );
	else if( strcmp( name, "B" ) == 0 )
		error = Lamp_setColorComponent( self, "B", valtuple );
	else if( strcmp( name, "energy" ) == 0 )
		error = Lamp_setEnergy( self, valtuple );
	else if( strcmp( name, "dist" ) == 0 )
		error = Lamp_setDist( self, valtuple );
	else if( strcmp( name, "spotSize" ) == 0 )
		error = Lamp_setSpotSize( self, valtuple );
	else if( strcmp( name, "spotBlend" ) == 0 )
		error = Lamp_setSpotBlend( self, valtuple );
	else if( strcmp( name, "clipStart" ) == 0 )
		error = Lamp_setClipStart( self, valtuple );
	else if( strcmp( name, "clipEnd" ) == 0 )
		error = Lamp_setClipEnd( self, valtuple );
	else if( strcmp( name, "bias" ) == 0 )
		error = Lamp_setBias( self, valtuple );
	else if( strcmp( name, "softness" ) == 0 )
		error = Lamp_setSoftness( self, valtuple );
	else if( strcmp( name, "haloInt" ) == 0 )
		error = Lamp_setHaloInt( self, valtuple );
	else if( strcmp( name, "quad1" ) == 0 )
		error = Lamp_setQuad1( self, valtuple );
	else if( strcmp( name, "quad2" ) == 0 )
		error = Lamp_setQuad2( self, valtuple );
	else if( strcmp( name, "col" ) == 0 )
		error = Lamp_setCol( self, valtuple );

	else {			/* Error */
		Py_DECREF( valtuple );

		if( ( strcmp( name, "Types" ) == 0 ) ||	/* user tried to change a */
		    ( strcmp( name, "Modes" ) == 0 ) )	/* constant dict type ... */
			return ( EXPP_ReturnIntError( PyExc_AttributeError,
						      "constant dictionary -- cannot be changed" ) );

		else	/* ... or no member with the given name was found */
			return ( EXPP_ReturnIntError( PyExc_AttributeError,
						      "attribute not found" ) );
	}

	Py_DECREF( valtuple );

	if( error != Py_None )
		return -1;

	Py_DECREF( Py_None );	/* was incref'ed by the called Lamp_set* function */
	return 0;		/* normal exit */
}

/*****************************************************************************/
/* Function:    Lamp_compare                                                 */
/* Description: This is a callback function for the BPy_Lamp type. It        */
/*              compares two Lamp_Type objects. Only the "==" and "!="       */
/*              comparisons are meaninful. Returns 0 for equality and -1 if  */
/*              they don't point to the same Blender Lamp struct.            */
/*              In Python it becomes 1 if they are equal, 0 otherwise.       */
/*****************************************************************************/
static int Lamp_compare( BPy_Lamp * a, BPy_Lamp * b )
{
	Lamp *pa = a->lamp, *pb = b->lamp;
	return ( pa == pb ) ? 0 : -1;
}

/*****************************************************************************/
/* Function:    Lamp_repr                                                    */
/* Description: This is a callback function for the BPy_Lamp type. It        */
/*              builds a meaninful string to represent lamp objects.         */
/*****************************************************************************/
static PyObject *Lamp_repr( BPy_Lamp * self )
{
	return PyString_FromFormat( "[Lamp \"%s\"]", self->lamp->id.name + 2 );
}

static PyObject *Lamp_getIpo( BPy_Lamp * self )
{
	struct Ipo *ipo = self->lamp->ipo;

	if( !ipo ) {
		Py_INCREF( Py_None );
		return Py_None;
	}

	return Ipo_CreatePyObject( ipo );
}

extern PyTypeObject Ipo_Type;

static PyObject *Lamp_setIpo( BPy_Lamp * self, PyObject * args )
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

	if( ipo->blocktype != ID_TE )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "this ipo is not a lamp data ipo" );

	oldipo = self->lamp->ipo;
	if( oldipo ) {
		ID *id = &oldipo->id;
		if( id->us > 0 )
			id->us--;
	}

	( ( ID * ) & ipo->id )->us++;

	self->lamp->ipo = ipo;

	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *Lamp_clearIpo( BPy_Lamp * self )
{
	Lamp *lamp = self->lamp;
	Ipo *ipo = ( Ipo * ) lamp->ipo;

	if( ipo ) {
		ID *id = &ipo->id;
		if( id->us > 0 )
			id->us--;
		lamp->ipo = NULL;

		return EXPP_incr_ret_True();
	}

	return EXPP_incr_ret_False(); /* no ipo found */
}

/*
 * Lamp_insertIpoKey()
 *  inserts Lamp IPO key for RGB,ENERGY,SPOTSIZE,OFFSET,SIZE
 */

static PyObject *Lamp_insertIpoKey( BPy_Lamp * self, PyObject * args )
{
	int key = 0, map;

	if( !PyArg_ParseTuple( args, "i", &( key ) ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
										"expected int argument" ) );

	map = texchannel_to_adrcode(self->lamp->texact);

	if (key == IPOKEY_RGB ) {
		insertkey((ID *)self->lamp,LA_COL_R);
		insertkey((ID *)self->lamp,LA_COL_G);
		insertkey((ID *)self->lamp,LA_COL_B);      
	}
	if (key == IPOKEY_ENERGY ) {
		insertkey((ID *)self->lamp,LA_ENERGY);    
	}	
	if (key == IPOKEY_SPOTSIZE ) {
		insertkey((ID *)self->lamp,LA_SPOTSI);    
	}
	if (key == IPOKEY_OFFSET ) {
		insertkey((ID *)self->lamp, map+MAP_OFS_X);
		insertkey((ID *)self->lamp, map+MAP_OFS_Y);
		insertkey((ID *)self->lamp, map+MAP_OFS_Z);  
	}
	if (key == IPOKEY_SIZE ) {
		insertkey((ID *)self->lamp, map+MAP_SIZE_X);
		insertkey((ID *)self->lamp, map+MAP_SIZE_Y);
		insertkey((ID *)self->lamp, map+MAP_SIZE_Z);  
	}

	allspace(REMAKEIPO, 0);
	EXPP_allqueue(REDRAWIPO, 0);
	EXPP_allqueue(REDRAWVIEW3D, 0);
	EXPP_allqueue(REDRAWACTION, 0);
	EXPP_allqueue(REDRAWNLA, 0);

	return EXPP_incr_ret( Py_None );
}
