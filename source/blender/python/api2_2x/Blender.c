/* 
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
 * Contributor(s): Michel Selten
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include <Python.h>
#include <stdio.h>

#include <BKE_global.h>
#include <BSE_headerbuttons.h>
#include <DNA_ID.h>
#include <DNA_object_types.h>
#include <DNA_scene_types.h>
/* #include <DNA_screen_types.h> */
#include <DNA_userdef_types.h>
#include <BKE_ipo.h>

#include "gen_utils.h"
#include "modules.h"

/*****************************************************************************/
/* Python API function prototypes for the Blender module.                    */
/*****************************************************************************/
PyObject *Blender_Set (PyObject *self, PyObject *args);
PyObject *Blender_Get (PyObject *self, PyObject *args);
PyObject *Blender_Redraw(PyObject *self, PyObject *args);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.__doc__                                                           */
/*****************************************************************************/
char Blender_Set_doc[] =
"(request, data) - Update settings in Blender\n\
\n\
(request) A string indentifying the setting to change\n\
	'curframe'	- Sets the current frame using the number in data";

char Blender_Get_doc[] =
"(request) - Retrieve settings from Blender\n\
\n\
(request) A string indentifying the data to be returned\n\
	'curframe'	- Returns the current animation frame\n\
	'curtime'	- Returns the current animation time\n\
	'staframe'	- Returns the start frame of the animation\n\
	'endframe'	- Returns the end frame of the animation\n\
	'filename'	- Returns the name of the last file read or written\n\
	'version'	- Returns the Blender version number";

char Blender_Redraw_doc[] = "() - Redraw all 3D windows";

/*****************************************************************************/
/* Python method structure definition.                                       */
/*****************************************************************************/
struct PyMethodDef Blender_methods[] = {
	{"Set",    &Blender_Set, METH_VARARGS, Blender_Set_doc},
	{"Get",    &Blender_Get, METH_VARARGS, Blender_Get_doc},
	{"Redraw", &Blender_Redraw, METH_VARARGS, Blender_Redraw_doc},
	{NULL, NULL}
};

/*****************************************************************************/
/* Function:              Blender_Set                                        */
/* Python equivalent:     Blender.Set                                        */
/*****************************************************************************/
PyObject *Blender_Set (PyObject *self, PyObject *args)
{
	char      * name;
	PyObject  * arg;
	int         framenum;
	
	if (!PyArg_ParseTuple(args, "sO", &name, &arg))
	{
		/* TODO: Do we need to generate a nice error message here? */
		return (NULL);
	}

	if (StringEqual (name, "curframe"))
	{
		if (!PyArg_Parse(arg, "i", &framenum))
		{
			/* TODO: Do we need to generate a nice error message here? */
			return (NULL);
		}

		G.scene->r.cfra = framenum;

		update_for_newframe();
	}
	else
	{
		return (PythonReturnErrorObject (PyExc_AttributeError,
					"bad request identifier"));
	}
	return ( PythonIncRef (Py_None) );
}

/*****************************************************************************/
/* Function:              Blender_Get                                        */
/* Python equivalent:     Blender.Get                                        */
/*****************************************************************************/
PyObject *Blender_Get (PyObject *self, PyObject *args)
{
	PyObject  * object;
	PyObject  * dict;
	char      * str;
	
	printf ("In Blender_Get()\n");

	if (!PyArg_ParseTuple (args, "O", &object))
	{
		/* TODO: Do we need to generate a nice error message here? */
		return (NULL);
	}

	if (PyString_Check (object))
	{
		str = PyString_AsString (object);

		if (StringEqual (str, "curframe"))
		{
			return ( PyInt_FromLong (G.scene->r.cfra) );
		}
		if (StringEqual (str, "curtime"))
		{
			return ( PyFloat_FromDouble (frame_to_float (G.scene->r.cfra) ) );
		}
		if (StringEqual (str, "staframe"))
		{
			return ( PyInt_FromLong (G.scene->r.sfra) );
		}
		if (StringEqual (str, "endframe"))
		{
			return ( PyInt_FromLong (G.scene->r.efra) );
		}
		if (StringEqual (str, "filename"))
		{
			return ( PyString_FromString (G.sce) );
		}
		/* According to the old file (opy_blender.c), the following if
		   statement is a quick hack and needs some clean up. */
		if (StringEqual (str, "vrmloptions"))
		{
			dict = PyDict_New ();

			PyDict_SetItemString (dict, "twoside",
					PyInt_FromLong (U.vrmlflag & USERDEF_VRML_TWOSIDED));

			PyDict_SetItemString (dict, "layers",
					PyInt_FromLong (U.vrmlflag & USERDEF_VRML_LAYERS));

			PyDict_SetItemString (dict, "autoscale",
					PyInt_FromLong (U.vrmlflag & USERDEF_VRML_AUTOSCALE));

			return (dict);
		} /* End 'quick hack' part. */
		if (StringEqual (str, "version"))
		{
			return ( PyInt_FromLong (G.version) );
		}
		/* TODO: Do we want to display a usefull message here that the
		   requested data is unknown?
		else
		{
			return (PythonReturnErrorObject (..., "message") );
		}
		*/
	}
	else
	{
		return (PythonReturnErrorObject (PyExc_AttributeError,
					"expected string argument"));
	}

	return (PythonReturnErrorObject (PyExc_AttributeError,
				"bad request identifier"));
}

/*****************************************************************************/
/* Function:              Blender_Redraw                                     */
/* Python equivalent:     Blender.Redraw                                     */
/*****************************************************************************/
PyObject *Blender_Redraw(PyObject *self, PyObject *args)
{
	/*
	int wintype = SPACE_VIEW3D;
	
	printf ("In Blender_Redraw()\n");

	if (!PyArg_ParseTuple (args, "|i", &wintype))
	{
		 TODO: Do we need to generate a nice error message here?
		return (NULL);
	}

	return Windowmodule_Redraw(self, Py_BuildValue("(i)", wintype));
	*/
	return (Py_None);
}

/*****************************************************************************/
/* Function:              initBlender                                        */
/*****************************************************************************/
void initBlender (void)
{
	PyObject	* module;
	PyObject	* dict;

	printf ("In initBlender()\n");
	g_blenderdict = NULL;

	/* TODO: create a docstring for the Blender module */
	module = Py_InitModule3("Blender", Blender_methods, NULL);

	dict = PyModule_GetDict (module);
	g_blenderdict = dict;
	PyDict_SetItemString (dict, "Object", initObject());
}

