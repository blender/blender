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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Michel Selten, Willian P. Germano, Joseph Gilbert
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include <Python.h>
#include <stdio.h>

#include <BIF_usiblender.h>
#include <BLI_blenlib.h>
#include <BKE_global.h>
#include <BPI_script.h>
#include <BSE_headerbuttons.h>
#include <DNA_ID.h>
#include <DNA_object_types.h>
#include <DNA_scene_types.h>
#include <DNA_screen_types.h> /* for SPACE_VIEW3D */
#include <DNA_space_types.h> /* for SPACE_VIEW3D */
#include <DNA_userdef_types.h>
#include <BKE_ipo.h>

#include "gen_utils.h"
#include "modules.h"
#include "../BPY_extern.h" /* for bpy_gethome() */

/* From Window.h, used here by Blender_Redraw */
PyObject *M_Window_Redraw(PyObject *self, PyObject *args);

/**********************************************************/
/* Python API function prototypes for the Blender module.	*/
/**********************************************************/
static PyObject *Blender_Set (PyObject *self, PyObject *args);
static PyObject *Blender_Get (PyObject *self, PyObject *args);
static PyObject *Blender_Redraw(PyObject *self, PyObject *args);
static PyObject *Blender_ReleaseGlobalDict(PyObject *self, PyObject *args);
static PyObject *Blender_Quit(PyObject *self);
static PyObject *Blender_Load(PyObject *self, PyObject *args);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.			 */
/* In Python these will be written to the console when doing a							 */
/* Blender.__doc__																													 */
/*****************************************************************************/
static char Blender_Set_doc[] =
"(request, data) - Update settings in Blender\n\
\n\
(request) A string identifying the setting to change\n\
	'curframe'	- Sets the current frame using the number in data";

static char Blender_Get_doc[] =
"(request) - Retrieve settings from Blender\n\
\n\
(request) A string indentifying the data to be returned\n\
	'curframe'	- Returns the current animation frame\n\
	'curtime'	- Returns the current animation time\n\
	'staframe'	- Returns the start frame of the animation\n\
	'endframe'	- Returns the end frame of the animation\n\
	'filename'	- Returns the name of the last file read or written\n\
	'datadir' - Returns the dir where scripts can save their data, if available\n\
	'version'	- Returns the Blender version number";

static char Blender_Redraw_doc[] = "() - Redraw all 3D windows";

static char Blender_ReleaseGlobalDict_doc[] =
"Deprecated, please use the Blender.Registry module solution instead.";

static char Blender_Quit_doc[] =
"() - Quit Blender.  The current data is saved as 'quit.blend' before leaving.";

static char Blender_Load_doc[] =
"(filename) - Load the given .blend file.  If succesful, the script is ended\n\
immediately.\n\
Notes:\n\
1 - () - an empty argument loads the default .B.blend file;\n\
2 - if the substring '.B.blend' occurs inside 'filename', the default\n\
.B.blend file is loaded;\n\
3 - The current data is always preserved as an autosave file, for safety;\n\
4 - This function only works if the script where it's executed is the\n\
only one running.";

/*****************************************************************************/
/* Python method structure definition.																			 */
/*****************************************************************************/
static struct PyMethodDef Blender_methods[] = {
	{"Set",		 Blender_Set, METH_VARARGS, Blender_Set_doc},
	{"Get",		 Blender_Get, METH_VARARGS, Blender_Get_doc},
	{"Redraw", Blender_Redraw, METH_VARARGS, Blender_Redraw_doc},
	{"Quit",	 (PyCFunction)Blender_Quit, METH_NOARGS, Blender_Quit_doc},
	{"Load", Blender_Load, METH_VARARGS, Blender_Load_doc},
	{"ReleaseGlobalDict", &Blender_ReleaseGlobalDict,
		METH_VARARGS, Blender_ReleaseGlobalDict_doc},
 	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Global variables																													 */
/*****************************************************************************/
PyObject *g_blenderdict;

/*****************************************************************************/
/* Function:							Blender_Set																				 */
/* Python equivalent:			Blender.Set																				 */
/*****************************************************************************/
static PyObject *Blender_Set (PyObject *self, PyObject *args)
{
	char			* name;
	PyObject	* arg;
	int					framenum;
			
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
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
																			"bad request identifier"));
	}
	return ( EXPP_incr_ret (Py_None) );
}

/*****************************************************************************/
/* Function:							Blender_Get																				 */
/* Python equivalent:			Blender.Get																				 */
/*****************************************************************************/
static PyObject *Blender_Get (PyObject *self, PyObject *args)
{
	PyObject	* object;
	PyObject	* dict;
	char			* str;
				
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
		if (StringEqual (str, "datadir"))
		{
			char datadir[FILE_MAXDIR];
			BLI_make_file_string("/", datadir, bpy_gethome(), "bpydata/");
			if (BLI_exists(datadir)) return PyString_FromString(datadir);
			else return EXPP_incr_ret (Py_None);
		}
		/* According to the old file (opy_blender.c), the following if
			 statement is a quick hack and needs some clean up. */
		if (StringEqual (str, "vrmloptions"))
		{
			dict = PyDict_New ();

			PyDict_SetItemString (dict, "twoside",
									PyInt_FromLong (U.vrmlflag & USER_VRML_TWOSIDED));

			PyDict_SetItemString (dict, "layers",
									PyInt_FromLong (U.vrmlflag & USER_VRML_LAYERS));

			PyDict_SetItemString (dict, "autoscale",
									PyInt_FromLong (U.vrmlflag & USER_VRML_AUTOSCALE));

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
			return (EXPP_ReturnPyObjError (..., "message") );
		}
		*/
	}
	else
	{
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
																		"expected string argument"));
	}

	return (EXPP_ReturnPyObjError (PyExc_AttributeError,
																"bad request identifier"));
}

/*****************************************************************************/
/* Function:							Blender_Redraw																		 */
/* Python equivalent:			Blender.Redraw																		 */
/*****************************************************************************/
static PyObject *Blender_Redraw(PyObject *self, PyObject *args)
{
	int wintype = SPACE_VIEW3D;

	if (!PyArg_ParseTuple (args, "|i", &wintype))
	{
		return EXPP_ReturnPyObjError (PyExc_TypeError,
												"expected int argument (or nothing)");
	}

	return M_Window_Redraw(self, Py_BuildValue("(i)", wintype));
}

/*****************************************************************************/
/* Function:							Blender_ReleaseGlobalDict													 */
/* Python equivalent:			Blender.ReleaseGlobalDict													 */
/* Description:						Deprecated function.															 */
/*****************************************************************************/
static PyObject *Blender_ReleaseGlobalDict(PyObject *self, PyObject *args)
{
	Py_INCREF(Py_None);
	return Py_None;
}

/*****************************************************************************/
/* Function:							Blender_Quit																			 */
/* Python equivalent:			Blender.Quit																			 */
/*****************************************************************************/
static PyObject *Blender_Quit(PyObject *self)
{
	BIF_write_autosave(); /* save the current data first */

	exit_usiblender(); /* renames last autosave to quit.blend */

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *Blender_Load(PyObject *self, PyObject *args)
{
	char *fname = NULL;
	Script *script = NULL;

	if (!PyArg_ParseTuple(args, "|s", &fname))
		return EXPP_ReturnPyObjError(PyExc_TypeError,
			"expected filename string or nothing (for default file) as argument");

	if (fname && !BLI_exists(fname))
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"requested file doesn't exist!");

	/* We won't let a new .blend file be loaded if there are still other
	 * scripts running, since loading a new file will close and remove them. */

	if (G.main->script.first != G.main->script.last)
		return EXPP_ReturnPyObjError(PyExc_RuntimeError,
			"there are other scripts running at the Scripts win, close them first!");

	/* trick: mark the script so that its script struct won't be freed after
	 * the script is executed (to avoid a double free warning on exit): */
	script = G.main->script.first;
	script->flags |= SCRIPT_GUI;

	BIF_write_autosave(); /* for safety let's preserve the current data */

	/* for safety, any filename with .B.blend is considered the default one.
	 * It doesn't seem necessary to compare file attributes (like st_ino and
	 * st_dev, according to the glibc info pages) to find out if the given
	 * filename, that may have been given with a twisted misgiving path, is the
	 * default one for sure.  Taking any .B.blend file as the default is good
	 * enough here.  Note: the default file requires extra clean-up done by
	 * BIF_read_homefile: freeing the user theme data. */
	if (!fname || strstr(fname, ".B.blend"))
		BIF_read_homefile();
	else
		BIF_read_file(fname);

	Py_INCREF(Py_None);
	return Py_None;
}

/*****************************************************************************/
/* Function:							initBlender																				 */
/*****************************************************************************/
void M_Blender_Init (void)
{
	PyObject				* module;
	PyObject				* dict;

	g_blenderdict = NULL;

	/* TODO: create a docstring for the Blender module */
	module = Py_InitModule3("Blender", Blender_methods, NULL);

	types_InitAll(); /* set all our pytypes to &PyType_Type*/

	dict = PyModule_GetDict (module);
	g_blenderdict = dict;

	Py_INCREF(Py_False);
	PyDict_SetItemString(dict, "bylink", Py_False);
	Py_INCREF(Py_None);
	PyDict_SetItemString(dict, "link", Py_None);
	PyDict_SetItemString(dict, "event", PyString_FromString(""));

	PyDict_SetItemString (dict, "Types",		Types_Init());
	PyDict_SetItemString (dict, "sys",			sys_Init());
	PyDict_SetItemString (dict, "Registry", Registry_Init());
	PyDict_SetItemString (dict, "Scene",		Scene_Init());
	PyDict_SetItemString (dict, "Object",		Object_Init());
	PyDict_SetItemString (dict, "Material", Material_Init());
	PyDict_SetItemString (dict, "Camera",		Camera_Init());
	PyDict_SetItemString (dict, "Lamp",			Lamp_Init());
	PyDict_SetItemString (dict, "Lattice",	Lattice_Init());
	PyDict_SetItemString (dict, "Curve",		Curve_Init());
	PyDict_SetItemString (dict, "Armature", Armature_Init());
	PyDict_SetItemString (dict, "Ipo",			Ipo_Init());
	PyDict_SetItemString (dict, "IpoCurve", IpoCurve_Init());
	PyDict_SetItemString (dict, "Metaball", Metaball_Init());
	PyDict_SetItemString (dict, "Image",		Image_Init());
	PyDict_SetItemString (dict, "Window",		Window_Init());
	PyDict_SetItemString (dict, "Draw",			Draw_Init());
	PyDict_SetItemString (dict, "BGL",			BGL_Init());
	PyDict_SetItemString (dict, "Effect",		Effect_Init());
	PyDict_SetItemString (dict, "Text",			Text_Init());
	PyDict_SetItemString (dict, "World",		World_Init());
	PyDict_SetItemString (dict, "Texture",	Texture_Init());
	PyDict_SetItemString (dict, "NMesh",		NMesh_Init());
	PyDict_SetItemString (dict, "Noise",		Noise_Init());
	PyDict_SetItemString (dict, "Mathutils",Mathutils_Init());
	PyDict_SetItemString (dict, "Library",  Library_Init());

	PyModule_AddIntConstant(module, "TRUE",  1);
	PyModule_AddIntConstant(module, "FALSE",  0);
}
