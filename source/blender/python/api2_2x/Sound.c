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
#include "BKE_sound.h"
#include "BKE_library.h"
#include "BIF_editsound.h"
#include "BKE_packedFile.h"
#include "mydevice.h"		/* redraw defines */
#include "gen_utils.h"
#include "gen_library.h"
#include "DNA_space_types.h" /* for FILE_MAXDIR only */

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
static int Sound_compare( BPy_Sound * a, BPy_Sound * b );
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
static PyObject *Sound_setName( BPy_Sound * self, PyObject * args );
static int 		 Sound_setFilename( BPy_Sound * self, PyObject * args );
static PyObject *Sound_oldsetFilename( BPy_Sound * self, PyObject * args );
static PyObject *Sound_setCurrent( BPy_Sound * self );
static PyObject *Sound_play( BPy_Sound * self );
static PyObject *Sound_unpack( BPy_Sound * self, PyObject * args);
static PyObject *Sound_pack( BPy_Sound * self );
/*static PyObject *Sound_reload ( BPy_Sound * self );*/
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
	{"setName", ( PyCFunction ) Sound_setName, METH_VARARGS,
	 "(name) - Set Sound object name"},
	{"setFilename", ( PyCFunction ) Sound_oldsetFilename, METH_VARARGS,
	 "(filename) - Set Sound object filename"},
	{"setCurrent", ( PyCFunction ) Sound_setCurrent, METH_NOARGS,
	 "() - make this the active sound in the sound buttons win (also redraws)"},
	{"play", ( PyCFunction ) Sound_play, METH_NOARGS,
				 "() - play this sound"},
	{"unpack", ( PyCFunction ) Sound_unpack, METH_VARARGS,
		         "(int) - Unpack sound. Uses one of the values defined in Blender.UnpackModes."},
	{"pack", ( PyCFunction ) Sound_pack, METH_NOARGS,
		         "() Pack the sound"},
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

			if( !pyobj ) {
				Py_DECREF(snd_list);
				return ( EXPP_ReturnPyObjError
					 ( PyExc_MemoryError,
					   "couldn't create PyObject" ) );
			}
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

	if( PyType_Ready( &Sound_Type ) < 0 )
		return NULL;

	submodule =
		Py_InitModule3( "Blender.Sound", M_Sound_methods,
				M_Sound_doc );

	return ( submodule );
}

/************************/
/*** The Sound PyType ***/
/************************/


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

static PyObject *Sound_getPacked( BPy_Sound * self )
{
	if (!sound_sample_is_null(self->sound))	{
		bSample *sample = sound_find_sample(self->sound);
		if (sample->packedfile)
			Py_RETURN_TRUE;
	}
	Py_RETURN_FALSE;
}

static PyObject *Sound_setName( BPy_Sound * self, PyObject * args )
{
	char *name;

	if( !PyArg_ParseTuple( args, "s", &name ) ) {
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected a String as argument" ) );
	}

	rename_id( &self->sound->id, name );

	Py_RETURN_NONE;
}

static int Sound_setFilename( BPy_Sound * self, PyObject * value )
{
	char *name;

	/* max len is FILE_MAXDIR = 160 chars like in DNA_image_types.h */
	name = PyString_AsString(value);
	if (!name || strlen(name) > FILE_MAXDIR)
		return ( EXPP_ReturnIntError( PyExc_ValueError,
						"string argument is limited to 160 chars at most" ) );

	strcpy( self->sound->name, name );
	return 0;
}

static PyObject *Sound_oldsetFilename( BPy_Sound * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args, (setter)Sound_setFilename );
}


static PyObject *Sound_play( BPy_Sound * self )
{
	sound_play_sound( self->sound );

	Py_RETURN_NONE;
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

	Py_RETURN_NONE;
}

/* unpack sound */

static PyObject *Sound_unpack( BPy_Sound * self, PyObject * args )
{
	bSound *sound = self->sound;
	int mode;
	if( !PyArg_ParseTuple( args, "i", &mode ) )
			return EXPP_ReturnPyObjError( PyExc_TypeError,
							"expected an integer from Blender.UnpackModes" );

	if (!sound_sample_is_null(sound)) {
	    bSample *sample = sound_find_sample(sound);
		if (sample->packedfile) {
			if (unpackSample(sample, mode) == RET_ERROR)
					return EXPP_ReturnPyObjError( PyExc_RuntimeError,
									"error unpacking sound");
		}
	} else {
		return EXPP_ReturnPyObjError( PyExc_RuntimeError, "sound has no samples" );
	}
	Py_RETURN_NONE;
}

/* pack sound */

static PyObject *Sound_pack( BPy_Sound * self )
{
	bSound *sound = self->sound;
	if (!sound_sample_is_null(sound))
	{
		bSample *sample = sound_find_sample(sound);
		if (sample->packedfile )
			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"sound alredy packed" );
		sound_set_packedfile(sample, newPackedFile(sample->name));
	}
	else
	{
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"sound has no samples" );
	}
	Py_RETURN_NONE;
}

/*
static PyObject *Sound_reload( BPy_Sound * self)
{
	sound_free_sample();

	if (sound->snd_sound) {
		SND_RemoveSound(ghSoundScene, sound->snd_sound);
		sound->snd_sound = NULL;
	}

	Py_RETURN_NONE;
}
*/



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
	return ( a->sound == b->sound ) ? 0 : -1;
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

/*****************************************************************************/
/* Python attributes get/set structure:                                      */
/*****************************************************************************/
static PyGetSetDef BPy_Sound_getseters[] = {
	GENERIC_LIB_GETSETATTR,
	{"filename", (getter)Sound_getFilename, (setter)Sound_setFilename,
	 "text filename", NULL},
	{"packed", (getter)Sound_getPacked, (setter)NULL,
	 "text filename", NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
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
	NULL,	/* tp_dealloc */
	0,		/* tp_print */
	NULL,	/* tp_getattr */
	NULL,	/* tp_setattr */
	( cmpfunc ) Sound_compare,	/* tp_compare */
	( reprfunc ) Sound_repr,	/* tp_repr */

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
	BPy_Sound_methods,           /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_Sound_getseters,         /* struct PyGetSetDef *tp_getset; */
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


