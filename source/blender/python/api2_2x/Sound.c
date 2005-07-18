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
 * Contributor(s): Chris Keith
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include "Sound.h" /*This must come first*/

#include "BKE_global.h"
#include "BKE_main.h"
#include "BLI_blenlib.h"
#include "BIF_editsound.h"
#include "mydevice.h"		/* redraw defines */
#include "gen_utils.h"

/*****************************************************************************/
/* Python BPy_Sound defaults:					*/
/*****************************************************************************/

#define EXPP_SND_volume_MIN   0.0
#define EXPP_SND_volume_MAX   1.0
#define EXPP_SND_pitch_MIN  -12.0
#define EXPP_SND_pitch_MAX   12.0
#define EXPP_SND_attenuation_MIN 0.0
#define EXPP_SND_attenuation_MAX 5.0

/*****************************************************************************/
/* Python API function prototypes for the Sound module.		*/
/*****************************************************************************/
static PyObject *M_Sound_Get( PyObject * self, PyObject * args );
static PyObject *M_Sound_Load( PyObject * self, PyObject * args );

/************************************************************************/
/* The following string definitions are used for documentation strings.	*/
/* In Python these will be written to the console when doing a		*/
/* Blender.Sound.__doc__						*/
/************************************************************************/
static char M_Sound_doc[] = "The Blender Sound module\n\n";

static char M_Sound_Get_doc[] =
	"(name) - return the sound with the name 'name', \
returns None if not found.\n If 'name' is not specified, \
it returns a list of all sounds in the\ncurrent scene.";

static char M_Sound_Load_doc[] =
	"(filename) - return sound from file filename as a Sound Object,\n\
returns None if not found.";

/*****************************************************************************/
/* Python method structure definition for Blender.Sound module:							 */
/*****************************************************************************/
struct PyMethodDef M_Sound_methods[] = {
	{"Get", M_Sound_Get, METH_VARARGS, M_Sound_Get_doc},
	{"Load", M_Sound_Load, METH_VARARGS, M_Sound_Load_doc},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python Sound_Type callback function prototypes:			*/
/*****************************************************************************/
static void Sound_dealloc( BPy_Sound * self );
static int Sound_setAttr( BPy_Sound * self, char *name, PyObject * v );
static int Sound_compare( BPy_Sound * a, BPy_Sound * b );
static PyObject *Sound_getAttr( BPy_Sound * self, char *name );
static PyObject *Sound_repr( BPy_Sound * self );

#define SOUND_FLOAT_METHODS(funcname, varname)			\
static PyObject *Sound_get ## funcname(BPy_Sound *self) {	\
    char e[256];						\
    PyObject *attr = PyFloat_FromDouble(self->sound->varname);	\
    if (attr) return attr;					\
    sprintf(e, "couldn't get Sound.%s attribute", #varname);	\
    return EXPP_ReturnPyObjError (PyExc_RuntimeError, e);	\
}								\
static PyObject *Sound_set ## funcname(BPy_Sound *self, PyObject *args) { \
    float	f = 0;						\
    if (!PyArg_ParseTuple(args, "f", &f))			\
	    return (EXPP_ReturnPyObjError (PyExc_TypeError,	\
		    "expected float argument"));		\
    self->sound->varname = EXPP_ClampFloat(f,\
			EXPP_SND_##varname##_MIN, EXPP_SND_##varname##_MAX);\
    Py_INCREF(Py_None);						\
    return Py_None;						\
}

#define SOUND_FLOAT_METHOD_FUNCS(varname)			\
{"get"#varname, (PyCFunction)Sound_get ## varname, METH_NOARGS,	\
"() - Return Sound object "#varname},				\
{"set"#varname, (PyCFunction)Sound_set ## varname, METH_VARARGS, \
"(float) - Change Sound object "#varname},


/*****************************************************************************/
/* Python BPy_Sound methods declarations:				*/
/*****************************************************************************/
static PyObject *Sound_getName( BPy_Sound * self );
static PyObject *Sound_getFilename( BPy_Sound * self );
static PyObject *Sound_play( BPy_Sound * self );
static PyObject *Sound_setCurrent( BPy_Sound * self );
//static PyObject *Sound_reload ( BPy_Sound * self );
SOUND_FLOAT_METHODS( Volume, volume )
SOUND_FLOAT_METHODS( Attenuation, attenuation )
SOUND_FLOAT_METHODS( Pitch, pitch )
/* these can't be set via interface, removed for now */
/*
SOUND_FLOAT_METHODS( Panning, panning )
SOUND_FLOAT_METHODS( MinGain, min_gain )
SOUND_FLOAT_METHODS( MaxGain, max_gain )
SOUND_FLOAT_METHODS( Distance, distance )
*/

/*****************************************************************************/
/* Python BPy_Sound methods table:				         */
/*****************************************************************************/
static PyMethodDef BPy_Sound_methods[] = {
	/* name, method, flags, doc */
	{"getName", ( PyCFunction ) Sound_getName, METH_NOARGS,
	 "() - Return Sound object name"},
	{"getFilename", ( PyCFunction ) Sound_getFilename, METH_NOARGS,
	 "() - Return Sound object filename"},
	{"play", ( PyCFunction ) Sound_play, METH_NOARGS,
	 "() - play this sound"},
	{"setCurrent", ( PyCFunction ) Sound_setCurrent, METH_NOARGS,
	 "() - make this the active sound in the sound buttons win (also redraws)"},

/*
	{"reload", ( PyCFunction ) Sound_setCurrent, METH_NOARGS,
	 "() - reload this Sound object's sample.\n\
    This is only useful if the original sound file has changed."},
*/
	SOUND_FLOAT_METHOD_FUNCS( Volume )
	SOUND_FLOAT_METHOD_FUNCS( Attenuation )
	SOUND_FLOAT_METHOD_FUNCS( Pitch )
	/*
	SOUND_FLOAT_METHOD_FUNCS( Panning )
	SOUND_FLOAT_METHOD_FUNCS( MinGain )
	SOUND_FLOAT_METHOD_FUNCS( MaxGain )
	SOUND_FLOAT_METHOD_FUNCS( Distance )
	*/
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python Sound_Type structure definition:				*/
/*****************************************************************************/
PyTypeObject Sound_Type = {
	PyObject_HEAD_INIT( NULL )
	0,		/* ob_size */
	"Blender Sound",	/* tp_name */
	sizeof( BPy_Sound ),	/* tp_basicsize */
	0,			/* tp_itemsize */
	/* methods */
	( destructor ) Sound_dealloc,	/* tp_dealloc */
	0,			/* tp_print */
	( getattrfunc ) Sound_getAttr,	/* tp_getattr */
	( setattrfunc ) Sound_setAttr,	/* tp_setattr */
	( cmpfunc ) Sound_compare,	/* tp_compare */
	( reprfunc ) Sound_repr,	/* tp_repr */
	0,			/* tp_as_number */
	0,			/* tp_as_sequence */
	0,			/* tp_as_mapping */
	0,			/* tp_as_hash */
	0, 0, 0, 0, 0, 0,
	0,			/* tp_doc */
	0, 0, 0, 0, 0, 0,
	BPy_Sound_methods,	/* tp_methods */
	0,			/* tp_members */
};

/* NOTE: these were copied and modified from image.h.  To Be Done TBD:
 * macro-ize them, or C++ templates eventually?
 */
/****************************************************************************/
/* Function:		M_Sound_Get				*/
/* Python equivalent:	Blender.Sound.Get			 */
/* Description:		Receives a string and returns the Sound object	 */
/*			whose name matches the string.	If no argument is  */
/*			passed in, a list of all Sound names in the	 */
/*			current scene is returned.			 */
/****************************************************************************/
static PyObject *M_Sound_Get( PyObject * self, PyObject * args )
{
	char *name = NULL;
	bSound *snd_iter;

	if( !PyArg_ParseTuple( args, "|s", &name ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected string argument (or nothing)" ) );

	snd_iter = G.main->sound.first;

	if( name ) {		/* (name) - Search Sound by name */

		BPy_Sound *wanted_Sound = NULL;

		while( ( snd_iter ) && ( wanted_Sound == NULL ) ) {
			if( strcmp( name, snd_iter->id.name + 2 ) == 0 ) {
				wanted_Sound =
					( BPy_Sound * )
					PyObject_NEW( BPy_Sound, &Sound_Type );
				if( wanted_Sound ) {
					wanted_Sound->sound = snd_iter;
					break;
				}
			}
			snd_iter = snd_iter->id.next;
		}

		if( wanted_Sound == NULL ) {	/* Requested Sound doesn't exist */
			char error_msg[64];
			PyOS_snprintf( error_msg, sizeof( error_msg ),
				       "Sound \"%s\" not found", name );
			return ( EXPP_ReturnPyObjError
				 ( PyExc_NameError, error_msg ) );
		}

		return ( PyObject * ) wanted_Sound;
	}

	else {			/* () - return a list of all Sounds in the scene */
		int index = 0;
		PyObject *snd_list, *pyobj;

		snd_list = PyList_New( BLI_countlist( &( G.main->sound ) ) );

		if( snd_list == NULL )
			return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
							"couldn't create PyList" ) );

		while( snd_iter ) {
			pyobj = Sound_CreatePyObject( snd_iter );

			if( !pyobj )
				return ( EXPP_ReturnPyObjError
					 ( PyExc_MemoryError,
					   "couldn't create PyObject" ) );

			PyList_SET_ITEM( snd_list, index, pyobj );

			snd_iter = snd_iter->id.next;
			index++;
		}

		return ( snd_list );
	}
}

/*****************************************************************************/
/* Function:	M_Sound_Load						*/
/* Python equivalent:	Blender.Sound.Load				*/
/* Description:		Receives a string and returns the Sound object	 */
/*			whose filename matches the string.		 */
/*****************************************************************************/
static PyObject *M_Sound_Load( PyObject * self, PyObject * args )
{
	char *fname;
	bSound *snd_ptr;
	BPy_Sound *snd;

	if( !PyArg_ParseTuple( args, "s", &fname ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected string argument" ) );

	snd = ( BPy_Sound * ) PyObject_NEW( BPy_Sound, &Sound_Type );

	if( !snd )
		return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
						"couldn't create PyObject Sound_Type" ) );

	snd_ptr = sound_new_sound( fname );

	if( snd_ptr ) {
		if( G.ssound ) {
			G.ssound->sound = snd_ptr;
		}
	}

	if( !snd_ptr )
		return ( EXPP_ReturnPyObjError( PyExc_IOError,
						"not a valid sound sample" ) );

	snd->sound = snd_ptr;

	return ( PyObject * ) snd;
}

/*****************************************************************************/
/* Function:	Sound_Init					*/
/*****************************************************************************/
PyObject *Sound_Init( void )
{
	PyObject *submodule;

	Sound_Type.ob_type = &PyType_Type;

	submodule =
		Py_InitModule3( "Blender.Sound", M_Sound_methods,
				M_Sound_doc );

	return ( submodule );
}

/************************/
/*** The Sound PyType ***/
/************************/

/*****************************************************************************/
/* Function:		Sound_dealloc			         */
/* Description: This is a callback function for the BPy_Sound type. It is  */
/*	       	the destructor function.				*/
/*****************************************************************************/
static void Sound_dealloc( BPy_Sound * self )
{
	PyObject_DEL( self );
}

/*****************************************************************************/
/* Function:	Sound_CreatePyObject					*/
/* Description: This function will create a new BPy_Sound from an existing  */
/*		Blender Sound structure.				*/
/*****************************************************************************/
PyObject *Sound_CreatePyObject( bSound * snd )
{
	BPy_Sound *py_snd;

	py_snd = ( BPy_Sound * ) PyObject_NEW( BPy_Sound, &Sound_Type );

	if( !py_snd )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create BPy_Sound object" );

	py_snd->sound = snd;

	return ( PyObject * ) py_snd;
}

/*****************************************************************************/
/* Function:	Sound_CheckPyObject					*/
/* Description: This function returns true when the given PyObject is of the */
/*			type Sound. Otherwise it will return false.	*/
/*****************************************************************************/
int Sound_CheckPyObject( PyObject * pyobj )
{
	return ( pyobj->ob_type == &Sound_Type );
}

/*****************************************************************************/
/* Function:	Sound_FromPyObject				*/
/* Description: Returns the Blender Sound associated with this object	 */
/*****************************************************************************/
bSound *Sound_FromPyObject( PyObject * pyobj )
{
	return ( ( BPy_Sound * ) pyobj )->sound;
}

/*****************************************************************************/
/* Python BPy_Sound methods:	*/
/*****************************************************************************/
static PyObject *Sound_getName( BPy_Sound * self )
{
	PyObject *attr = PyString_FromString( self->sound->id.name + 2 );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Sound.name attribute" ) );
}

static PyObject *Sound_getFilename( BPy_Sound * self )
{
	PyObject *attr = PyString_FromString( self->sound->name );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Sound.filename attribute" ) );
}

static PyObject *Sound_play( BPy_Sound * self )
{
	sound_play_sound( self->sound );

	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *Sound_setCurrent( BPy_Sound * self )
{
	bSound *snd_ptr = self->sound;

	if( snd_ptr ) {
		if( G.ssound ) {
			G.ssound->sound = snd_ptr;
		}
	}

	EXPP_allqueue( REDRAWSOUND, 0 );
	EXPP_allqueue( REDRAWBUTSLOGIC, 0 );

	Py_INCREF( Py_None );
	return Py_None;
}
/*
static PyObject *Sound_reload( BPy_Sound * self)
{
	sound_free_sample();

	if (sound->snd_sound) {
		SND_RemoveSound(ghSoundScene, sound->snd_sound);
		sound->snd_sound = NULL;
	}

	return EXPP_incr_ret( Py_None );
}
*/

/*****************************************************************************/
/* Function:	Sound_getAttr					*/
/* Description: This is a callback function for the BPy_Sound type. It is  */
/*		the function that accesses BPy_Sound member variables and  */
/*		methods.						 */
/*****************************************************************************/
static PyObject *Sound_getAttr( BPy_Sound * self, char *name )
{
	PyObject *attr = Py_None;

	if( strcmp( name, "name" ) == 0 )
		attr = PyString_FromString( self->sound->id.name + 2 );
	else if( strcmp( name, "filename" ) == 0 )
		attr = PyString_FromString( self->sound->name );

	else if( strcmp( name, "__members__" ) == 0 )
		attr = Py_BuildValue( "[s,s]", "name", "filename" );

	if( !attr )
		return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
						"couldn't create PyObject" ) );

	if( attr != Py_None )
		return attr;	/* attribute found, return its value */

	/* not an attribute, search the methods table */
	return Py_FindMethod( BPy_Sound_methods, ( PyObject * ) self, name );
}

/*****************************************************************************/
/* Function:	Sound_setAttr						*/
/* Description: This is a callback function for the BPy_Sound type. It is the*/
/*		function that changes Sound object members values. If this  */
/*		data is linked to a Blender Sound, it also gets updated.    */
/*****************************************************************************/
static int Sound_setAttr( BPy_Sound * self, char *name, PyObject * value )
{
	PyObject *valtuple;
//	PyObject *error = NULL;

/* We're playing a trick on the Python API users here.	Even if they use
 * Sound.member = val instead of Sound.setMember(value), we end up using the
 * function anyway, since it already has error checking, clamps to the right
 * interval and updates the Blender Sound structure when necessary. */

	valtuple = Py_BuildValue( "(O)", value );	/*the set* functions expect a tuple */

	if( !valtuple )
		return EXPP_ReturnIntError( PyExc_MemoryError,
					    "SoundSetAttr: couldn't create PyTuple" );

/*	if (strcmp (name, "name") == 0)
		error = Sound_setName (self, valtuple);
	else */  {
		/* Error: no such member in the Sound object structure */
		Py_DECREF( value );
		Py_DECREF( valtuple );
		return ( EXPP_ReturnIntError( PyExc_KeyError,
					      "attribute not found or immutable" ) );
	}

/*	===This code is unreachable===
	Py_DECREF( valtuple );

	if( error != Py_None )
		return -1;

	Py_DECREF( Py_None );	// incref'ed by the called set* function /
	return 0;		// normal exit 
	*/
}

/*****************************************************************************/
/* Function:	Sound_compare					*/
/* Description: This is a callback function for the BPy_Sound type. It	 */
/*		compares two Sound_Type objects. Only the "==" and "!="	  */
/*		comparisons are meaninful. Returns 0 for equality and -1 if  */
/*	 	they don't point to the same Blender Sound struct.	 */
/*		In Python it becomes 1 if they are equal, 0 otherwise.	 */
/*****************************************************************************/
static int Sound_compare( BPy_Sound * a, BPy_Sound * b )
{
	bSound *pa = a->sound, *pb = b->sound;
	return ( pa == pb ) ? 0 : -1;
}

/*****************************************************************************/
/* Function:	Sound_repr						*/
/* Description: This is a callback function for the BPy_Sound type. It	*/
/*		builds a meaninful string to represent Sound objects.	 */
/*****************************************************************************/
static PyObject *Sound_repr( BPy_Sound * self )
{
	return PyString_FromFormat( "[Sound \"%s\"]",
				    self->sound->id.name + 2 );
}
