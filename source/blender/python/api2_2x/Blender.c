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
 * Contributor(s): Michel Selten, Willian P. Germano
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include "Blender.h"

/*****************************************************************************/
/* Global variables                                                          */
/*****************************************************************************/
PyObject *g_blenderdict;

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

	int wintype = SPACE_VIEW3D;

	printf ("In Blender_Redraw()\n");

	if (!PyArg_ParseTuple (args, "|i", &wintype))
	{
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected int argument (or nothing)");
	}

	return M_Window_Redraw(self, Py_BuildValue("(i)", wintype));
}

/*****************************************************************************/
/* Function:              Blender_ReleaseGlobalDict                          */
/* Python equivalent:     Blender.ReleaseGlobalDict                          */
/* Description:           Receives an int (treated as boolean) to define     */
/*                        whether the global Python dictionary should be     */
/*                        cleared after the script is run or not.  Default   */
/*                        is to clear (to release).  To change this, call    */
/*                        Blender.ReleaseGlobalDict with a non-zero int      */
/*                        argument.  If called with an empty arg list, it    */
/*                        doesn't change anything.                           */
/*                        Returns the current behavior.                      */
/*****************************************************************************/
PyObject *Blender_ReleaseGlobalDict(PyObject *self, PyObject *args)
{
	printf ("In Blender_ReleaseGlobalDict()\n");

	if (!PyArg_ParseTuple (args, "|i", &EXPP_releaseGlobalDict))
	{
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected int argument (or nothing)");
	}

	return Py_BuildValue("i", (EXPP_releaseGlobalDict?1:0));
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
	PyDict_SetItemString (dict, "Camera", M_Camera_Init());
	PyDict_SetItemString (dict, "Lamp",   M_Lamp_Init());
	PyDict_SetItemString (dict, "Image",  M_Image_Init());
	PyDict_SetItemString (dict, "Window", M_Window_Init());
	PyDict_SetItemString (dict, "Draw",   M_Draw_Init());
	PyDict_SetItemString (dict, "BGL",    M_BGL_Init());
	PyDict_SetItemString (dict, "Text",   M_Text_Init());
}

