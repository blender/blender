/* 
 * $Id: Registry.c 4803 2005-07-18 03:50:37Z ascotan $
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
 * Contributor(s): Willian P. Germano
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include "Registry.h" /*This must come first */

#include "BKE_global.h"
#include "gen_utils.h"


/* the Registry dictionary */
PyObject *bpy_registryDict = NULL;

/*****************************************************************************/
/* Python API function prototypes for the Registry module.                   */
/*****************************************************************************/
static PyObject *M_Registry_Keys( PyObject * self );
static PyObject *M_Registry_GetKey( PyObject * self, PyObject * args );
static PyObject *M_Registry_SetKey( PyObject * self, PyObject * args );
static PyObject *M_Registry_RemoveKey( PyObject * self, PyObject * args );

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.Registry.__doc__                                                  */
/*****************************************************************************/
char M_Registry_doc[] =
	"The Blender Registry module (persistent data cache)\n\n\
    Use this module to store configuration data that a script can reload\n\
    when it is executed again.\n";

char M_Registry_Keys_doc[] =
	"() - Get all keys in the Registry dictionary.\n\n\
    Each key references another dict with saved data from a specific script.\n";

char M_Registry_GetKey_doc[] =
	"(name, disk = False) - Get an entry (a dict) from the Registry dictionary\n\
 (name) - a string that references a specific script;\n\
 (disk = False) - search on the user (if available) or default scripts config\n\
data dir.\n";

char M_Registry_SetKey_doc[] =
	"(key, dict, disk = False) - Store an entry in the Registry dictionary.\n\
    If an entry with the same 'key' already exists, it is substituted.\n\
 (key) - the string to use as a key for the dict being saved.\n\
 (dict) - a dictionary with the data to be stored.\n\
 (disk = False) - also write data as a config file inside the user (if\n\
available) or default scripts config data dir.\n";

char M_Registry_RemoveKey_doc[] =
	"(key, disk = False) - Remove the dict with key 'key' from the Registry.\n\
 (key) - the name of the key to delete;\n\
 (disk = False) - if True the respective config file is also deleted.\n";

/*****************************************************************************/
/* Python method structure definition for Blender.Registry module:           */
/*****************************************************************************/
struct PyMethodDef M_Registry_methods[] = {
	{"Keys", ( PyCFunction ) M_Registry_Keys, METH_VARARGS,
	 M_Registry_Keys_doc},
	{"GetKey", M_Registry_GetKey, METH_VARARGS, M_Registry_GetKey_doc},
	{"SetKey", M_Registry_SetKey, METH_VARARGS, M_Registry_SetKey_doc},
	{"RemoveKey", M_Registry_RemoveKey, METH_VARARGS,
	 M_Registry_RemoveKey_doc},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Function:              M_Registry_Keys                                    */
/* Python equivalent:     Blender.Registry.Keys                              */
/*****************************************************************************/
PyObject *M_Registry_Keys( PyObject * self )
{
	PyObject *pydict = NULL;

	if( !bpy_registryDict )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "No Registry dictionary found!" );

	pydict = PyDict_Keys( bpy_registryDict );

	if( !pydict )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "Registry_Keys: couldn't get keys" );

	return pydict;
}

/*****************************************************************************/
/* Function:              M_Registry_GetKey                                  */
/* Python equivalent:     Blender.Registry.GetKey                            */
/*****************************************************************************/
static PyObject *M_Registry_GetKey( PyObject * self, PyObject * args )
{
	PyObject *pyentry = NULL;
	PyObject *pydict = NULL;
	int disk = 0;

	if( !bpy_registryDict )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"No Registry dictionary found!" );

	if( !PyArg_ParseTuple( args, "O!|i", &PyString_Type, &pyentry, &disk ) )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
			"expected a string and optionally a bool" );

	pydict = PyDict_GetItem( bpy_registryDict, pyentry );	/* borrowed ... */

	if (!pydict) {
		if (disk > 0) {
			/* try to get data from disk */
			char buf[256];
			PyOS_snprintf(buf, sizeof(buf),
				"import Blender, BPyRegistry; BPyRegistry.LoadConfigData('%s')",
				PyString_AsString(pyentry));
			if (!PyRun_SimpleString(buf))
				pydict = PyDict_GetItem(bpy_registryDict, pyentry);
			else PyErr_Clear();
		}

		if (!pydict) /* no need to return a KeyError, since without doubt */
			pydict = Py_None; /* Py_None means no key (all valid keys are dicts) */
	}

	return EXPP_incr_ret (pydict); /* ... so we incref it */
}

/*****************************************************************************/
/* Function:              M_Registry_SetKey                                  */
/* Python equivalent:     Blender.Registry.SetKey                            */
/*****************************************************************************/
static PyObject *M_Registry_SetKey( PyObject * self, PyObject * args )
{
	PyObject *pystr = NULL;
	PyObject *pydict = NULL;
	int disk = 0;

	if( !bpy_registryDict )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "No Registry dictionary found!" );

	if( !PyArg_ParseTuple( args, "O!O!|i",
			       &PyString_Type, &pystr, &PyDict_Type,
			       &pydict, &disk ) )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "expected a string and a dictionary" );

	if( PyDict_SetItem( bpy_registryDict, pystr, pydict ) )	/* 0 on success */
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "Registry_SetKey: couldn't update the Registry dict" );

	if (disk) {
		/* try to save data to disk */
		char buf[256];
		PyOS_snprintf(buf, sizeof(buf),
			"import Blender, BPyRegistry; BPyRegistry.SaveConfigData('%s')",
			PyString_AsString(pystr));
		if (PyRun_SimpleString(buf) != 0) {
			PyErr_Clear();
			if (G.f & G_DEBUG)
				fprintf(stderr, "\nCan't save script configuration data!\n");
		}
	}

	Py_INCREF( Py_None );
	return Py_None;
}

/*****************************************************************************/
/* Function:              M_Registry_RemoveKey                               */
/* Python equivalent:     Blender.Registry.RemoveKey                         */
/*****************************************************************************/
static PyObject *M_Registry_RemoveKey( PyObject * self, PyObject * args )
{
	PyObject *pystr = NULL;
	int disk = 0;

	if( !bpy_registryDict )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "No Registry dictionary found!" );

	if( !PyArg_ParseTuple( args, "O!|i", &PyString_Type, &pystr, &disk ) )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "expected a string and optionally a bool" );

	if( PyDict_DelItem( bpy_registryDict, pystr ) )	/* returns 0 on success */
		return EXPP_ReturnPyObjError( PyExc_KeyError,
					      "no such key in the Registry" );
	else if (disk) {
		/* try to delete from disk too */
		char buf[256];
		PyOS_snprintf(buf, sizeof(buf),
			"import Blender, BPyRegistry; BPyRegistry.RemoveConfigData('%s')",
			PyString_AsString(pystr));
		if (PyRun_SimpleString(buf) != 0) {
			PyErr_Clear();
			if (G.f & G_DEBUG)
				fprintf(stderr, "\nCan't remove script configuration data file!\n");
		}
	}

	Py_INCREF( Py_None );
	return Py_None;
}

/*****************************************************************************/
/* Function:              Registry_Init                                      */
/*****************************************************************************/
PyObject *Registry_Init( void )
{
	PyObject *submodule;

	submodule = Py_InitModule3( "Blender.Registry", M_Registry_methods,
				    M_Registry_doc );

	return submodule;
}
