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
 * Contributor(s): Willian P. Germano
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include <Python.h>
#include <stdio.h>

#include <BKE_global.h>
#include <BKE_library.h>
#include <BKE_object.h> /* for during_script() */
#include <BIF_usiblender.h>
#include <BIF_mywindow.h>
#include <BSE_headerbuttons.h>
#include <BSE_filesel.h>
#include <BIF_screen.h>
#include <BIF_space.h>
#include <BIF_drawtext.h>
#include <mydevice.h>
#include <DNA_view3d_types.h>
#include <DNA_screen_types.h>
#include <DNA_space_types.h>
#include <DNA_text_types.h>

#include "gen_utils.h"
#include "modules.h"
#include "matrix.h"
#include "vector.h"


/* See Draw.c */
extern int EXPP_disable_force_draw;

/* Callback used by the file and image selector access functions */
static PyObject *(*EXPP_FS_PyCallback)(PyObject *arg) = NULL;

/*****************************************************************************/
/* Python API function prototypes for the Window module.										 */
/*****************************************************************************/
PyObject *M_Window_Redraw (PyObject *self, PyObject *args);
static PyObject *M_Window_RedrawAll (PyObject *self, PyObject *args);
static PyObject *M_Window_QRedrawAll (PyObject *self, PyObject *args);
static PyObject *M_Window_DrawProgressBar (PyObject *self, PyObject *args);
static PyObject *M_Window_GetCursorPos (PyObject *self);
static PyObject *M_Window_SetCursorPos (PyObject *self, PyObject *args);
static PyObject *M_Window_GetViewVector (PyObject *self);
static PyObject *M_Window_GetViewMatrix (PyObject *self);
static PyObject *M_Window_FileSelector (PyObject *self, PyObject *args);
static PyObject *M_Window_ImageSelector (PyObject *self, PyObject *args);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.			 */
/* In Python these will be written to the console when doing a							 */
/* Blender.Window.__doc__																										 */
/*****************************************************************************/
static char M_Window_doc[] =
"The Blender Window module\n\n";

static char M_Window_Redraw_doc[] =
"() - Force a redraw of a specific Window Type (see Window.Types)";

static char M_Window_RedrawAll_doc[] =
"() - Redraw all windows";

static char M_Window_QRedrawAll_doc[] =
"() - Redraw all windows by queue event";

static char M_Window_FileSelector_doc[] =
"(callback [, title, filename]) - Open a file selector window.\n\
The selected file name is used as argument to a function callback f(name)\n\
that you must provide. 'title' is optional and defaults to 'SELECT FILE'.\n\
'filename' is optional and defaults to Blender.Get('filename').\n\n\
Example:\n\n\
import Blender\n\n\
def my_function(filename):\n\
	print 'The selected file was: ', filename\n\n\
Blender.Window.FileSelector(my_function, 'SAVE FILE')\n";

static char M_Window_ImageSelector_doc[] =
"(callback [, title, filename]) - Open an image selector window.\n\
The selected file name is used as argument to a function callback f(name)\n\
that you must provide. 'title' is optional and defaults to 'SELECT IMAGE'.\n\
'filename' is optional and defaults to Blender.Get('filename').\n\n\
Example:\n\n\
import Blender\n\n\
def my_function(filename):\n\
	print 'The selected image file was: ', filename\n\n\
Blender.Window.ImageSelector(my_function, 'LOAD IMAGE')\n";

static char M_Window_DrawProgressBar_doc[] =
"(done, text) - Draw a progress bar.\n\
'done' is a float value <= 1.0, 'text' contains info about what is\n\
currently being done.";

static char M_Window_GetCursorPos_doc[] =
"() - Get the current 3d cursor position as a list of three floats.";

static char M_Window_SetCursorPos_doc[] =
"([f,f,f]) - Set the current 3d cursor position from a list of three floats.";

static char M_Window_GetViewVector_doc[] =
"() - Get the current 3d view vector as a list of three floats [x,y,z].";

static char M_Window_GetViewMatrix_doc[] =
"() - Get the current 3d view matrix.";

/*****************************************************************************/
/* Python method structure definition for Blender.Window module:						 */
/*****************************************************************************/
struct PyMethodDef M_Window_methods[] = {
	{"Redraw",		 M_Window_Redraw,			METH_VARARGS, M_Window_Redraw_doc},
	{"RedrawAll",  M_Window_RedrawAll,	METH_VARARGS, M_Window_RedrawAll_doc},
	{"QRedrawAll", M_Window_QRedrawAll, METH_VARARGS, M_Window_QRedrawAll_doc},
	{"FileSelector", M_Window_FileSelector, METH_VARARGS,
		M_Window_FileSelector_doc},
	{"ImageSelector", (PyCFunction)M_Window_ImageSelector, METH_VARARGS,
		M_Window_ImageSelector_doc},
	{"DrawProgressBar", M_Window_DrawProgressBar,  METH_VARARGS,
		M_Window_DrawProgressBar_doc},
	{"drawProgressBar", M_Window_DrawProgressBar,  METH_VARARGS,
		M_Window_DrawProgressBar_doc},
	{"GetCursorPos", (PyCFunction)M_Window_GetCursorPos,	METH_NOARGS,
		M_Window_GetCursorPos_doc},
	{"SetCursorPos", M_Window_SetCursorPos,  METH_VARARGS,
		M_Window_SetCursorPos_doc},
	{"GetViewVector", (PyCFunction)M_Window_GetViewVector,	METH_NOARGS,
		M_Window_GetViewVector_doc},
	{"GetViewMatrix", (PyCFunction)M_Window_GetViewMatrix,	METH_NOARGS,
		M_Window_GetViewMatrix_doc},
	{NULL, NULL, 0, NULL}
};


/* Many parts of the code here come from the older bpython implementation
 * (file opy_window.c) */

/*****************************************************************************/
/* Function:							M_Window_Redraw																		 */
/* Python equivalent:			Blender.Window.Redraw															 */
/*****************************************************************************/
/* not static so py_slider_update in Draw.[ch] can use it */
PyObject *M_Window_Redraw(PyObject *self, PyObject *args)
{ 
	ScrArea *tempsa, *sa;
	SpaceText *st;
	int wintype = SPACE_VIEW3D;
	short redraw_all = 0;

	if (!PyArg_ParseTuple(args, "|i", &wintype))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected int argument (or nothing)"));

	if (wintype < 0)
		redraw_all = 1;

	if (!during_script()) { /* XXX check this */
		tempsa= curarea;
		sa = G.curscreen->areabase.first;

		while (sa) {

			if (sa->spacetype == wintype || redraw_all) {
				/* don't force-redraw Text window (Python GUI) when
					 redraw is called out of a slider update */
				if (sa->spacetype == SPACE_TEXT) {
					st = sa->spacedata.first;
					if (st->text->flags & TXT_FOLLOW) /* follow cursor display */
						pop_space_text(st);
					if (EXPP_disable_force_draw) { /* defined in Draw.[ch] ... */
						scrarea_queue_redraw(sa);
					}

				} else {
					scrarea_do_windraw(sa);
					if (sa->headwin) scrarea_do_headdraw(sa);
				}
			}

			sa= sa->next;
		}

		if (curarea != tempsa) areawinset (tempsa->win);

		if (curarea) { /* is null if Blender is in bg mode */
			if (curarea->headwin) scrarea_do_headdraw (curarea);
			screen_swapbuffers();
		}
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/*****************************************************************************/
/* Function:							M_Window_RedrawAll																 */
/* Python equivalent:			Blender.Window.RedrawAll													 */
/*****************************************************************************/
static PyObject *M_Window_RedrawAll(PyObject *self, PyObject *args)
{
	return M_Window_Redraw(self, Py_BuildValue("(i)", -1));
}

/*****************************************************************************/
/* Function:							M_Window_QRedrawAll																 */
/* Python equivalent:			Blender.Window.QRedrawAll													 */
/*****************************************************************************/
static PyObject *M_Window_QRedrawAll(PyObject *self, PyObject *args)
{
	allqueue(REDRAWALL, 0);

	Py_INCREF(Py_None);
	return Py_None;
}

/*****************************************************************************/
/* Function:							M_Window_FileSelector															 */
/* Python equivalent:			Blender.Window.FileSelector												 */
/*****************************************************************************/

/* This is the callback to "activate_fileselect" below.  It receives the
 * selected filename and (using it as argument) calls the Python callback
 * provided by the script writer and stored in EXPP_FS_PyCallback. */

static void getSelectedFile(char *name)
{
	if (!EXPP_FS_PyCallback) return;

	PyObject_CallFunction((PyObject *)EXPP_FS_PyCallback, "s", name);

	EXPP_FS_PyCallback = NULL;

	return;
}

static PyObject *M_Window_FileSelector(PyObject *self, PyObject *args)
{
	char *title = "SELECT FILE";
	char *filename = G.sce;
	SpaceScript *sc;
	Script *script = G.main->script.last;
	int startspace = 0;

	if (!PyArg_ParseTuple(args, "O!|ss",
			&PyFunction_Type, &EXPP_FS_PyCallback, &title, &filename))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
		"\nexpected a callback function (and optionally one or two strings) "
		"as argument(s)"));

/* trick: we move to a spacescript because then the fileselector will properly
 * unset our SCRIPT_FILESEL flag when the user chooses a file or cancels the
 * selection.  This is necessary because when a user cancels, the
 * getSelectedFile function above doesn't get called and so couldn't unset the
 * flag. */
	startspace = curarea->spacetype;
	if (startspace != SPACE_SCRIPT) newspace (curarea, SPACE_SCRIPT);

	sc = curarea->spacedata.first;

	/* did we get the right script? */
	if (!(script->flags & SCRIPT_RUNNING)) {
		/* if not running, then we were already on a SpaceScript space, executing
		 * a registered callback -- aka: this script has a gui */
		script = sc->script; /* this is the right script */
	}
	else { /* still running, use the trick */
		script->lastspace = startspace;
		sc->script = script;
	}

	script->flags |= SCRIPT_FILESEL;

	activate_fileselect(FILE_BLENDER, title, filename, getSelectedFile);

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *M_Window_ImageSelector(PyObject *self, PyObject *args)
{
	char *title = "SELECT IMAGE";
	char *filename = G.sce;
	SpaceScript *sc;
	Script *script = G.main->script.last;
	int startspace = 0;

	if (!PyArg_ParseTuple(args, "O!|ss",
			&PyFunction_Type, &EXPP_FS_PyCallback, &title, &filename))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
		"\nexpected a callback function (and optionally one or two strings) "
		"as argument(s)"));

/* trick: we move to a spacescript because then the fileselector will properly
 * unset our SCRIPT_FILESEL flag when the user chooses a file or cancels the
 * selection.  This is necessary because when a user cancels, the
 * getSelectedFile function above doesn't get called and so couldn't unset the
 * flag. */
	startspace = curarea->spacetype;
	if (startspace != SPACE_SCRIPT) newspace (curarea, SPACE_SCRIPT);

	sc = curarea->spacedata.first;

	/* did we get the right script? */
	if (!(script->flags & SCRIPT_RUNNING)) {
		/* if not running, then we're on a SpaceScript space, executing a
		 * registered callback -- aka: this script has a gui */
		SpaceScript *sc = curarea->spacedata.first;
		script = sc->script; /* this is the right script */
	}
	else { /* still running, use the trick */
		script->lastspace = startspace;
		sc->script = script;
	}

	script->flags |= SCRIPT_FILESEL; /* same flag as filesel */

	activate_imageselect(FILE_BLENDER, title, filename, getSelectedFile);

	Py_INCREF(Py_None);
	return Py_None;
}

/*****************************************************************************/
/* Function:							M_Window_DrawProgressBar													 */
/* Python equivalent:			Blender.Window.DrawProgressBar										 */
/*****************************************************************************/
static PyObject *M_Window_DrawProgressBar(PyObject *self, PyObject *args)
{
	float done;
	char *info = NULL;
	int retval = 0;

	if(!PyArg_ParseTuple(args, "fs", &done, &info))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected a float and a string as arguments"));

	if (!G.background) retval = progress_bar(done, info);

	return Py_BuildValue("i", retval);
}

/*****************************************************************************/
/* Function:							M_Window_GetCursorPos															 */
/* Python equivalent:			Blender.Window.GetCursorPos												 */
/*****************************************************************************/
static PyObject *M_Window_GetCursorPos(PyObject *self)
{
	float *cursor = NULL;
	PyObject *pylist;

	if (G.vd && G.vd->localview)
		cursor = G.vd->cursor;
	else cursor = G.scene->cursor;

	pylist = Py_BuildValue("[fff]", cursor[0], cursor[1], cursor[2]);

	if (!pylist)
		return (EXPP_ReturnPyObjError (PyExc_MemoryError,
						"GetCursorPos: couldn't create pylist"));

	return pylist;
}

/*****************************************************************************/
/* Function:							M_Window_SetCursorPos															 */
/* Python equivalent:			Blender.Window.SetCursorPos												 */
/*****************************************************************************/
static PyObject *M_Window_SetCursorPos(PyObject *self, PyObject *args)
{
	int ok = 0;
	float val[3];

	if (PyObject_Length (args) == 3)
		ok = PyArg_ParseTuple (args, "fff", &val[0], &val[1], &val[2]);
	else
		ok = PyArg_ParseTuple(args, "(fff)", &val[0], &val[1], &val[2]);

	if (!ok)
		return EXPP_ReturnPyObjError (PyExc_TypeError,
									"expected [f,f,f] or f,f,f as arguments");

	if (G.vd && G.vd->localview) {
		G.vd->cursor[0] = val[0];
		G.vd->cursor[1] = val[1];
		G.vd->cursor[2] = val[2];
	}
	else {
		G.scene->cursor[0] = val[0];
		G.scene->cursor[1] = val[1];
		G.scene->cursor[2] = val[2];
	}

	Py_INCREF (Py_None);
	return Py_None;
}

/*****************************************************************************/
/* Function:							M_Window_GetViewVector														 */
/* Python equivalent:			Blender.Window.GetViewVector											 */
/*****************************************************************************/
static PyObject *M_Window_GetViewVector(PyObject *self)
{
	float *vec = NULL;
	PyObject *pylist;

	if (!G.vd) {
		Py_INCREF (Py_None);
		return Py_None;
	}

	vec = G.vd->viewinv[2];

	pylist = Py_BuildValue("[fff]", vec[0], vec[1], vec[2]);

	if (!pylist)
		return (EXPP_ReturnPyObjError (PyExc_MemoryError,
						"GetViewVector: couldn't create pylist"));

	return pylist;
}

/*****************************************************************************/
/* Function:							M_Window_GetViewMatrix														 */
/* Python equivalent:			Blender.Window.GetViewMatrix											 */
/*****************************************************************************/
static PyObject *M_Window_GetViewMatrix(PyObject *self)
{
	PyObject *viewmat;

	if (!G.vd) {
		Py_INCREF (Py_None);
		return Py_None;
	}

	viewmat = (PyObject*)newMatrixObject((float*)G.vd->viewmat, 4, 4);

	if (!viewmat)
		return (EXPP_ReturnPyObjError (PyExc_MemoryError,
						"GetViewMatrix: couldn't create matrix pyobject"));

	return viewmat;
}

/*****************************************************************************/
/* Function:							Window_Init																				 */
/*****************************************************************************/
PyObject *Window_Init (void)
{
	PyObject	*submodule, *Types;

	submodule = Py_InitModule3("Blender.Window", M_Window_methods, M_Window_doc);

	Types = Py_BuildValue("{s:h,s:h,s:h,s:h,s:h,s:h,s:h,s:h,s:h,s:h,s:h,s:h,s:h,s:h}",
		"VIEW3D", SPACE_VIEW3D, "IPO", SPACE_IPO, "OOPS", SPACE_OOPS,
		"BUTS", SPACE_BUTS, "FILE", SPACE_FILE, "IMAGE", SPACE_IMAGE,
		"INFO", SPACE_INFO, "SEQ", SPACE_SEQ, "IMASEL", SPACE_IMASEL,
		"SOUND", SPACE_SOUND, "ACTION", SPACE_ACTION,
		"TEXT", SPACE_TEXT, "NLA", SPACE_NLA, "SCRIPT", SPACE_SCRIPT);

	if (Types) PyModule_AddObject(submodule, "Types", Types);

	return submodule;
}

