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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
*/

/* python types */
#include "DNA_userdef_types.h"
#include "../api2_2x/gen_utils.h"
#include "bpy_config.h"
#include "BKE_utildefines.h"

enum conf_consts {
	/*string*/
	EXPP_CONF_ATTR_PATH_YF_EXPORT = 0,
	EXPP_CONF_ATTR_PATH_FONT,
	EXPP_CONF_ATTR_PATH_RENDER,
	EXPP_CONF_ATTR_PATH_TEXTURE,
	EXPP_CONF_ATTR_PATH_PYTHON,
	EXPP_CONF_ATTR_PATH_TEX_PLUGIN,
	EXPP_CONF_ATTR_PATH_SOUND,
	EXPP_CONF_ATTR_PATH_SEQ_PLUGIN,
	EXPP_CONF_ATTR_PATH_TEMP,
	
	/*int*/
	EXPP_CONF_ATTR_UNDOSTEPS,
	EXPP_CONF_ATTR_TEX_TIMEOUT,
	EXPP_CONF_ATTR_TEX_COLLECT_RATE,
	EXPP_CONF_ATTR_MEM_CACHE_LIMIT,
	EXPP_CONF_ATTR_FONT_SIZE
};

PyObject *Config_CreatePyObject( )
{
	BPy_Config *conf = PyObject_NEW( BPy_Config, &Config_Type);
	return (PyObject *)conf;
}

/*
 * repr function
 * callback functions building meaninful string to representations
 */
static PyObject *Config_repr( BPy_Config * self )
{
	return PyString_FromFormat( "[Blender Configuration Data]");
}


/*-----------------------Config module Init())-----------------------------*/
/* see Main.c */
/*
static struct PyMethodDef BPy_Config_methods[] = {
	{"new", (PyCFunction)MainSeq_new, METH_VARARGS,
		"(name) - Create a new object in this scene from the obdata given and return a new object"},
	{"load", (PyCFunction)MainSeq_load, METH_VARARGS,
		"(filename) - loads the given filename for image, font and sound types"},
	{"unlink", (PyCFunction)MainSeq_unlink, METH_VARARGS,
		"unlinks the object from the scene"},
	{NULL, NULL, 0, NULL}
};*/

/*
 * get integer attributes
 */
static PyObject *getStrAttr( BPy_Config *self, void *type )
{
	char *param = NULL;
	
	switch( GET_INT_FROM_POINTER(type) ) {
	case EXPP_CONF_ATTR_PATH_YF_EXPORT:
		param = U.yfexportdir;
		break;
	case EXPP_CONF_ATTR_PATH_FONT:
		param = U.fontdir;
		break;
	case EXPP_CONF_ATTR_PATH_RENDER:
		param = U.renderdir;
		break;	
	case EXPP_CONF_ATTR_PATH_TEXTURE:
		param = U.textudir;
		break;
	case EXPP_CONF_ATTR_PATH_PYTHON:
		param = U.pythondir;
		break;
	case EXPP_CONF_ATTR_PATH_TEX_PLUGIN:
		param = U.plugtexdir;
		break;
	case EXPP_CONF_ATTR_PATH_SOUND:
		param = U.sounddir;
		break;
	case EXPP_CONF_ATTR_PATH_SEQ_PLUGIN:
		param = U.plugseqdir;
		break;
	case EXPP_CONF_ATTR_PATH_TEMP:
		param = U.tempdir;
		break;	
	
	default:
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"undefined type in getStrAttr" );
	}

	return PyString_FromString( param );
}

/*
 * set integer attributes which require clamping
 */

static int setStrAttr( BPy_Config *self, PyObject *value, void *type )
{
	char *param;
	int len=160;
	char *str = PyString_AsString(value);
	
	if (!str)
		return EXPP_ReturnIntError( PyExc_TypeError,
			"error, must assign a python string for setStrAttr");
	
	switch( GET_INT_FROM_POINTER(type) ) {
	case EXPP_CONF_ATTR_PATH_YF_EXPORT:
		param = U.yfexportdir;
		break;
	case EXPP_CONF_ATTR_PATH_FONT:
		param = U.fontdir;
		break;
	case EXPP_CONF_ATTR_PATH_RENDER:
		param = U.renderdir;
		break;	
	case EXPP_CONF_ATTR_PATH_TEXTURE:
		param = U.textudir;
		break;
	case EXPP_CONF_ATTR_PATH_PYTHON:
		param = U.pythondir;
		break;
	case EXPP_CONF_ATTR_PATH_TEX_PLUGIN:
		param = U.plugtexdir;
		break;
	case EXPP_CONF_ATTR_PATH_SOUND:
		param = U.sounddir;
		break;
	case EXPP_CONF_ATTR_PATH_SEQ_PLUGIN:
		param = U.plugseqdir;
		break;
	case EXPP_CONF_ATTR_PATH_TEMP:
		param = U.tempdir;
		break;
		
	default:
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"undefined type in setStrAttr");
	}
	
	strncpy(param, str, len);
	return 0;
}


/*
 * get integer attributes
 */

static PyObject *getIntAttr( BPy_Config *self, void *type )
{
	int param;

	switch( GET_INT_FROM_POINTER(type) ) {
	case EXPP_CONF_ATTR_UNDOSTEPS:
		param = (int)U.undosteps;
		break;
	case EXPP_CONF_ATTR_TEX_TIMEOUT:
		param = U.textimeout;
		break;		
	case EXPP_CONF_ATTR_TEX_COLLECT_RATE:
		param = U.texcollectrate;
		break;
	case EXPP_CONF_ATTR_MEM_CACHE_LIMIT:
		param = U.memcachelimit;
		break;		
	case EXPP_CONF_ATTR_FONT_SIZE:
		param = U.fontsize;
		break;
	
	default:
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"undefined type in getIntAttr" );
	}

	return PyInt_FromLong( param );
}

/*
 * set integer attributes which require clamping
 */

static int setIntAttrClamp( BPy_Config *self, PyObject *value, void *type )
{
	void *param;
	int min, max, size;

	switch( GET_INT_FROM_POINTER(type) ) {
	case EXPP_CONF_ATTR_UNDOSTEPS:
		min = 0;
		max = 64;
		size = 'h';
		param = (void *)&U.undosteps;
		break;
	case EXPP_CONF_ATTR_TEX_TIMEOUT:
		min = 1;
		max = 3600;
		size = 'i';
		param = (void *)&U.textimeout;
		break;
	case EXPP_CONF_ATTR_TEX_COLLECT_RATE:
		min = 1;
		max = 3600;
		size = 'i';
		param = (void *)&U.texcollectrate;
		break;
	case EXPP_CONF_ATTR_MEM_CACHE_LIMIT:
		min = 1;
		max = 1024;
		size = 'i';
		param = (void *)&U.memcachelimit;
		break;	
	case EXPP_CONF_ATTR_FONT_SIZE:
		min = 8;
		max = 16;
		size = 'i';
		param = (void *)&U.fontsize;
		break;
		
	default:
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"undefined type in setIntAttrClamp");
	}
	return EXPP_setIValueClamped( value, param, min, max, size );
}

static PyGetSetDef Config_getseters[] = {
	
	/* ints & shorts */
	{"undoSteps",
	 (getter)getIntAttr, (setter)setIntAttrClamp,
	 "undo steps",
	 (void *)EXPP_CONF_ATTR_UNDOSTEPS},
	{"textureTimeout",
	 (getter)getIntAttr, (setter)setIntAttrClamp,
	 "time for textures to stay in openGL memory",
	 (void *)EXPP_CONF_ATTR_TEX_TIMEOUT},
	{"textureCollectRate",
	 (getter)getIntAttr, (setter)setIntAttrClamp,
	 "intervel for textures to be tagged as used",
	 (void *)EXPP_CONF_ATTR_TEX_COLLECT_RATE},
	{"sequenceMemCacheLimit",
	 (getter)getIntAttr, (setter)setIntAttrClamp,
	 "maximum memory for the sequencer to use as cache",
	 (void *)EXPP_CONF_ATTR_MEM_CACHE_LIMIT},
	{"fontSize",
	 (getter)getIntAttr, (setter)setIntAttrClamp,
	 "user interface font size",
	 (void *)EXPP_CONF_ATTR_FONT_SIZE},
	 
	/* Paths */
	{"yfExportDir",
	 (getter)getStrAttr, (setter)setStrAttr,
	 "yafray export path",
	 (void *)EXPP_CONF_ATTR_PATH_YF_EXPORT},	 
	{"fontDir",
	 (getter)getStrAttr, (setter)setStrAttr,
	 "default font path",
	 (void *)EXPP_CONF_ATTR_PATH_FONT},
	{"renderDir",
	 (getter)getStrAttr, (setter)setStrAttr,
	 "default render path",
	 (void *)EXPP_CONF_ATTR_PATH_RENDER},
	{"textureDir",
	 (getter)getStrAttr, (setter)setStrAttr,
	 "default texture path",
	 (void *)EXPP_CONF_ATTR_PATH_TEXTURE},
	{"userScriptsDir",
	 (getter)getStrAttr, (setter)setStrAttr,
	 "user scripts path",
	 (void *)EXPP_CONF_ATTR_PATH_PYTHON}, 
	{"texturePluginsDir",
	 (getter)getStrAttr, (setter)setStrAttr,
	 "default texture plugins path",
	 (void *)EXPP_CONF_ATTR_PATH_TEX_PLUGIN},
	{"soundDir",
	 (getter)getStrAttr, (setter)setStrAttr,
	 "default sound path",
	 (void *)EXPP_CONF_ATTR_PATH_SOUND},
	{"sequencePluginsDir",
	 (getter)getStrAttr, (setter)setStrAttr,
	 "sequencer plugins path",
	 (void *)EXPP_CONF_ATTR_PATH_SEQ_PLUGIN},
	{"tempDir",
	 (getter)getStrAttr, (setter)setStrAttr,
	 "temporary file path",
	 (void *)EXPP_CONF_ATTR_PATH_TEMP},
	
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};


/*
 *  Python Config_Type structure definition
 */
PyTypeObject Config_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender Config",           /* char *tp_name; */
	sizeof( BPy_Config ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	NULL,						/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL, /* cmpfunc tp_compare; */
	(reprfunc)Config_repr,   /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,	    /* PySequenceMethods *tp_as_sequence; */
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
	NULL, /* getiterfunc tp_iter; */
	NULL, /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	NULL,  /*BPy_Config_methods*/     /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	Config_getseters,       /* struct PyGetSetDef *tp_getset; */
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
