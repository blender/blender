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

#ifndef EXPP_BLENDER_H
#define EXPP_BLENDER_H

#include <Python.h>
#include <stdio.h>

#include <BKE_global.h>
#include <BSE_headerbuttons.h>
#include <DNA_ID.h>
#include <DNA_object_types.h>
#include <DNA_scene_types.h>
#include <DNA_screen_types.h> /* for SPACE_VIEW3D */
#include <DNA_userdef_types.h>
#include <BKE_ipo.h>

#include "gen_utils.h"
#include "modules.h"

/* From Window.h, used here by Blender_Redraw */
PyObject *M_Window_Redraw(PyObject *self, PyObject *args);

/*****************************************************************************/
/* Python API function prototypes for the Blender module.                    */
/*****************************************************************************/
PyObject *Blender_Set (PyObject *self, PyObject *args);
PyObject *Blender_Get (PyObject *self, PyObject *args);
PyObject *Blender_Redraw(PyObject *self, PyObject *args);
PyObject *Blender_ReleaseGlobalDict(PyObject *self, PyObject *args);
PyObject *Blender_Quit(PyObject *self);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.__doc__                                                           */
/*****************************************************************************/
char Blender_Set_doc[] =
"(request, data) - Update settings in Blender\n\
\n\
(request) A string identifying the setting to change\n\
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

char Blender_ReleaseGlobalDict_doc[] =
"Deprecated, please use the Blender.Registry module solution instead.";

char Blender_Quit_doc[] =
"() - Quit Blender. Experimental, please use with caution.";

/*****************************************************************************/
/* Python method structure definition.                                       */
/*****************************************************************************/
struct PyMethodDef Blender_methods[] = {
	{"Set",    &Blender_Set, METH_VARARGS, Blender_Set_doc},
	{"Get",    &Blender_Get, METH_VARARGS, Blender_Get_doc},
	{"Redraw", &Blender_Redraw, METH_VARARGS, Blender_Redraw_doc},
	{"Quit",   &Blender_Quit, METH_NOARGS, Blender_Quit_doc},
	{"ReleaseGlobalDict", &Blender_ReleaseGlobalDict,
					METH_VARARGS, Blender_ReleaseGlobalDict_doc},
	{NULL, NULL}
};

#endif /* EXPP_BLENDER_H */
