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
 * Contributor(s): Willian P. Germano
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include <Python.h>
#include <stdio.h>

#include <blendef.h>		/* OBACT */
#include <BDR_editobject.h>	/* enter / leave editmode */
#include <BKE_global.h>
#include <BKE_library.h>
#include <BKE_object.h>		/* for during_script() */
#include <BKE_scene.h>		/* scene_find_camera() */
#include <BIF_usiblender.h>
#include <BIF_mywindow.h>
#include <BSE_headerbuttons.h>
#include <BSE_filesel.h>
#include <BIF_editmesh.h>	/* for undo_push_mesh() */
#include <BIF_screen.h>
#include <BIF_space.h>
#include <BIF_drawtext.h>
#include <BIF_spacetypes.h>
#include <mydevice.h>
#include <DNA_view3d_types.h>
#include <DNA_screen_types.h>
#include <DNA_space_types.h>
#include <DNA_text_types.h>
#include <DNA_vec_types.h>	/* for rcti struct */

#include "gen_utils.h"
#include "modules.h"
#include "matrix.h"
#include "vector.h"
#include "constant.h"


/* See Draw.c */
extern int EXPP_disable_force_draw;

/* Callback used by the file and image selector access functions */
static PyObject *( *EXPP_FS_PyCallback ) ( PyObject * arg ) = NULL;

/*****************************************************************************/
/* Python API function prototypes for the Window module.		*/
/*****************************************************************************/
PyObject *M_Window_Redraw( PyObject * self, PyObject * args );
static PyObject *M_Window_RedrawAll( PyObject * self, PyObject * args );
static PyObject *M_Window_QRedrawAll( PyObject * self, PyObject * args );
static PyObject *M_Window_DrawProgressBar( PyObject * self, PyObject * args );
static PyObject *M_Window_GetCursorPos( PyObject * self );
static PyObject *M_Window_SetCursorPos( PyObject * self, PyObject * args );
static PyObject *M_Window_WaitCursor( PyObject * self, PyObject * args );
static PyObject *M_Window_GetViewVector( PyObject * self );
static PyObject *M_Window_GetViewQuat( PyObject * self );
static PyObject *M_Window_SetViewQuat( PyObject * self, PyObject * args );
static PyObject *M_Window_GetViewOffset( PyObject * self );
static PyObject *M_Window_SetViewOffset( PyObject * self, PyObject * args );
static PyObject *M_Window_GetViewMatrix( PyObject * self );
static PyObject *M_Window_FileSelector( PyObject * self, PyObject * args );
static PyObject *M_Window_ImageSelector( PyObject * self, PyObject * args );
static PyObject *M_Window_EditMode( PyObject * self, PyObject * args );
static PyObject *M_Window_ViewLayer( PyObject * self, PyObject * args );
static PyObject *M_Window_CameraView( PyObject * self, PyObject * args );
static PyObject *M_Window_QTest( PyObject * self );
static PyObject *M_Window_QRead( PyObject * self );
static PyObject *M_Window_QAdd( PyObject * self, PyObject * args );
static PyObject *M_Window_QHandle( PyObject * self, PyObject * args );
static PyObject *M_Window_GetMouseCoords( PyObject * self );
static PyObject *M_Window_SetMouseCoords( PyObject * self, PyObject * args );
static PyObject *M_Window_GetMouseButtons( PyObject * self );
static PyObject *M_Window_GetKeyQualifiers( PyObject * self );
static PyObject *M_Window_SetKeyQualifiers( PyObject * self, PyObject * args );
static PyObject *M_Window_GetAreaSize( PyObject * self );
static PyObject *M_Window_GetAreaID( PyObject * self );
static PyObject *M_Window_GetScreenSize( PyObject * self );
static PyObject *M_Window_GetScreens( PyObject * self );
static PyObject *M_Window_SetScreen( PyObject * self, PyObject * args );
static PyObject *M_Window_GetScreenInfo( PyObject * self, PyObject * args,
					 PyObject * kwords );

/*****************************************************************************/
/* The following string definitions are used for documentation strings.	    */
/* In Python these will be written to the console when doing a		    */
/* Blender.Window.__doc__						    */
/*****************************************************************************/
static char M_Window_doc[] = "The Blender Window module\n\n";

static char M_Window_Redraw_doc[] =
	"() - Force a redraw of a specific Window Type (see Window.Types)";

static char M_Window_RedrawAll_doc[] = "() - Redraw all windows";

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

static char M_Window_WaitCursor_doc[] =
	"(bool) - Set cursor to wait mode (nonzero bool) or normal mode (0).";

static char M_Window_GetViewVector_doc[] =
	"() - Get the current 3d view vector as a list of three floats [x,y,z].";

static char M_Window_GetViewMatrix_doc[] =
	"() - Get the current 3d view matrix.";

static char M_Window_EditMode_doc[] =
	"() - Get the current status -- 0: not in edit mode; 1: in edit mode.\n\
(status) - if 1: enter edit mode; if 0: leave edit mode.\n\
Returns the current status.  This function is mostly useful to leave\n\
edit mode before applying changes to a mesh (otherwise the changes will\n\
be lost) and then returning to it upon leaving.";

static char M_Window_ViewLayer_doc[] =
	"(layers = []) - Get/set active layers in all 3d View windows.\n\
() - Make no changes, only return currently visible layers.\n\
(layers = []) - a list of integers, each in the range [1, 20].\n\
This function returns the currently visible layers as a list of ints.";

static char M_Window_GetViewQuat_doc[] =
	"() - Get the current VIEW3D view quaternion values.";

static char M_Window_SetViewQuat_doc[] =
	"(quat) - Set the current VIEW3D view quaternion values.\n\
(quat) - [f,f,f,f] or f,f,f,f: the new float values.";

static char M_Window_GetViewOffset_doc[] =
	"() - Get the current VIEW3D view offset values.";

static char M_Window_SetViewOffset_doc[] =
	"(ofs) - Set the current VIEW3D view offset values.\n\
(ofs) - [f,f,f] or f,f,f: the new float values.";

static char M_Window_CameraView_doc[] =
	"(camtov3d = 0) - Set the current VIEW3D view to the active camera's view.\n\
(camtov3d = 0) - bool: if nonzero it's the camera that gets positioned at the\n\
current view, instead of the view being changed to that of the camera.\n\n\
If no camera is the active object, the active camera for the current scene\n\
is used.";

static char M_Window_QTest_doc[] =
	"() - Check if there are pending events in the event queue.";

static char M_Window_QRead_doc[] =
	"() - Get the next pending event from the event queue.\n\
This function returns a list [event, val], where:\n\
event - int: the key or mouse event (see Blender.Draw module);\n\
val - int: if 1 it's a key or mouse button press, if 0 a release.  For\n\
	mouse movement events 'val' returns the new coordinates in x or y.";

static char M_Window_QAdd_doc[] =
	"(win, evt, val, after = 0) - Add an event to some window's event queue.\n\
(win) - int: the win id, see Blender.Window.GetScreenInfo();\n\
(evt) - int: the event number, see events in Blender.Draw;\n\
(val) - bool: 1 for a key press, 0 for a release;\n\
(after) - bool: if 1 the event is put after the current queue and added later.";

static char M_Window_QHandle_doc[] =
	"(win) - Process all events for the given window (area) now.\n\
(win) - int: the window id, see Blender.Window.GetScreenInfo().\n\n\
See Blender.Window.QAdd() for how to send events to a particular window.";

static char M_Window_GetMouseCoords_doc[] =
	"() - Get mouse pointer's current screen coordinates.";

static char M_Window_SetMouseCoords_doc[] =
	"(x, y) - Set mouse pointer's current screen coordinates.\n\
(x,y) - ints ([x, y] also accepted): the new x, y coordinates.";

static char M_Window_GetMouseButtons_doc[] =
	"() - Get the current mouse button state (see Blender.Draw.LEFTMOUSE, etc).";

static char M_Window_GetKeyQualifiers_doc[] =
	"() - Get the current qualifier keys state.\n\
An int is returned: or'ed combination of values in Blender.Window.Qual's dict.";

static char M_Window_SetKeyQualifiers_doc[] =
	"(qual) - Fake qualifier keys state.\n\
(qual) - int: an or'ed combination of the values in Blender.Window.Qual dict.\n\
Note: remember to reset to 0 after handling the related event (see QAdd()).";

static char M_Window_GetAreaID_doc[] =
	"() - Get the current window's (area) ID.";

static char M_Window_GetAreaSize_doc[] =
	"() - Get the current window's (area) size as [width, height].";

static char M_Window_GetScreenSize_doc[] =
	"() - Get the screen's size as [width, height].";

static char M_Window_GetScreens_doc[] =
	"() - Get a list with the names of all available screens.";

static char M_Window_SetScreen_doc[] =
	"(name) - Set current screen to the one with the given 'name'.";

static char M_Window_GetScreenInfo_doc[] =
	"(type = -1, rect = 'win', screen = None)\n\
- Get info about the the areas in the current screen setup.\n\
(type = -1) - int: the space type (Blender.Window.Types) to restrict the\n\
	results to, all if -1;\n\
(rect = 'win') - str: the rectangle of interest.  This defines if the corner\n\
	coordinates returned will refer to:\n\
	- the whole area: 'total';\n\
	- only the header: 'header';\n\
	- only the window content (default): 'win'.\n\
(screen = None) - str: the screen name, current if not given.\n\n\
A list of dictionaries (one for each area) is returned.\n\
Each dictionary has keys:\n\
'vertices': [xmin, ymin, xmax, ymax] area corners;\n\
'win': window type, see Blender.Window.Types dict;\n\
'id': area's id.";

/*****************************************************************************/
/* Python method structure definition for Blender.Window module:	*/
/*****************************************************************************/
struct PyMethodDef M_Window_methods[] = {
	{"Redraw", M_Window_Redraw, METH_VARARGS, M_Window_Redraw_doc},
	{"RedrawAll", M_Window_RedrawAll, METH_VARARGS,
	 M_Window_RedrawAll_doc},
	{"QRedrawAll", M_Window_QRedrawAll, METH_VARARGS,
	 M_Window_QRedrawAll_doc},
	{"FileSelector", M_Window_FileSelector, METH_VARARGS,
	 M_Window_FileSelector_doc},
	{"ImageSelector", ( PyCFunction ) M_Window_ImageSelector, METH_VARARGS,
	 M_Window_ImageSelector_doc},
	{"DrawProgressBar", M_Window_DrawProgressBar, METH_VARARGS,
	 M_Window_DrawProgressBar_doc},
	{"drawProgressBar", M_Window_DrawProgressBar, METH_VARARGS,
	 M_Window_DrawProgressBar_doc},
	{"GetCursorPos", ( PyCFunction ) M_Window_GetCursorPos, METH_NOARGS,
	 M_Window_GetCursorPos_doc},
	{"SetCursorPos", M_Window_SetCursorPos, METH_VARARGS,
	 M_Window_SetCursorPos_doc},
	{"WaitCursor", M_Window_WaitCursor, METH_VARARGS,
	 M_Window_WaitCursor_doc},
	{"GetViewVector", ( PyCFunction ) M_Window_GetViewVector, METH_NOARGS,
	 M_Window_GetViewVector_doc},
	{"GetViewQuat", ( PyCFunction ) M_Window_GetViewQuat, METH_NOARGS,
	 M_Window_GetViewQuat_doc},
	{"SetViewQuat", ( PyCFunction ) M_Window_SetViewQuat, METH_VARARGS,
	 M_Window_SetViewQuat_doc},
	{"GetViewOffset", ( PyCFunction ) M_Window_GetViewOffset, METH_NOARGS,
	 M_Window_GetViewOffset_doc},
	{"SetViewOffset", ( PyCFunction ) M_Window_SetViewOffset, METH_VARARGS,
	 M_Window_SetViewOffset_doc},
	{"GetViewMatrix", ( PyCFunction ) M_Window_GetViewMatrix, METH_NOARGS,
	 M_Window_GetViewMatrix_doc},
	{"EditMode", ( PyCFunction ) M_Window_EditMode, METH_VARARGS,
	 M_Window_EditMode_doc},
	{"ViewLayer", ( PyCFunction ) M_Window_ViewLayer, METH_VARARGS,
	 M_Window_ViewLayer_doc},
	{"CameraView", ( PyCFunction ) M_Window_CameraView, METH_VARARGS,
	 M_Window_CameraView_doc},
	{"QTest", ( PyCFunction ) M_Window_QTest, METH_NOARGS,
	 M_Window_QTest_doc},
	{"QRead", ( PyCFunction ) M_Window_QRead, METH_NOARGS,
	 M_Window_QRead_doc},
	{"QAdd", ( PyCFunction ) M_Window_QAdd, METH_VARARGS,
	 M_Window_QAdd_doc},
	{"QHandle", ( PyCFunction ) M_Window_QHandle, METH_VARARGS,
	 M_Window_QHandle_doc},
	{"GetMouseCoords", ( PyCFunction ) M_Window_GetMouseCoords,
	 METH_NOARGS,
	 M_Window_GetMouseCoords_doc},
	{"SetMouseCoords", ( PyCFunction ) M_Window_SetMouseCoords,
	 METH_VARARGS,
	 M_Window_SetMouseCoords_doc},
	{"GetMouseButtons", ( PyCFunction ) M_Window_GetMouseButtons,
	 METH_NOARGS,
	 M_Window_GetMouseButtons_doc},
	{"GetKeyQualifiers", ( PyCFunction ) M_Window_GetKeyQualifiers,
	 METH_NOARGS,
	 M_Window_GetKeyQualifiers_doc},
	{"SetKeyQualifiers", ( PyCFunction ) M_Window_SetKeyQualifiers,
	 METH_VARARGS,
	 M_Window_SetKeyQualifiers_doc},
	{"GetAreaSize", ( PyCFunction ) M_Window_GetAreaSize, METH_NOARGS,
	 M_Window_GetAreaSize_doc},
	{"GetAreaID", ( PyCFunction ) M_Window_GetAreaID, METH_NOARGS,
	 M_Window_GetAreaID_doc},
	{"GetScreenSize", ( PyCFunction ) M_Window_GetScreenSize, METH_NOARGS,
	 M_Window_GetScreenSize_doc},
	{"GetScreens", ( PyCFunction ) M_Window_GetScreens, METH_NOARGS,
	 M_Window_GetScreens_doc},
	{"SetScreen", ( PyCFunction ) M_Window_SetScreen, METH_VARARGS,
	 M_Window_SetScreen_doc},
	{"GetScreenInfo", ( PyCFunction ) M_Window_GetScreenInfo,
	 METH_VARARGS | METH_KEYWORDS, M_Window_GetScreenInfo_doc},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Function:	M_Window_Redraw						*/
/* Python equivalent:	Blender.Window.Redraw				*/
/*****************************************************************************/
/* not static so py_slider_update in Draw.[ch] can use it */
PyObject *M_Window_Redraw( PyObject * self, PyObject * args )
{
	ScrArea *tempsa, *sa;
	SpaceText *st;
	int wintype = SPACE_VIEW3D;
	short redraw_all = 0;

	if( !PyArg_ParseTuple( args, "|i", &wintype ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected int argument (or nothing)" ) );

	if( wintype < 0 )
		redraw_all = 1;

	if( !during_script(  ) ) {
		tempsa = curarea;
		sa = G.curscreen->areabase.first;

		while( sa ) {

			if( sa->spacetype == wintype || redraw_all ) {
				if( sa->spacetype == SPACE_TEXT ) {
					st = sa->spacedata.first;
					if( st->text ) {
						if( st->text->flags & TXT_FOLLOW )	/* follow cursor display */
							pop_space_text( st );

						// XXX making a test: Jul 07, 2004.
						// we don't need to prevent text win redraws anymore,
						// since now there's a scripts space instead.
						//if (EXPP_disable_force_draw) { /* defined in Draw.[ch] ... */
						//      scrarea_queue_redraw(sa);
					}
				}
				//} else {
				scrarea_do_windraw( sa );
				if( sa->headwin )
					scrarea_do_headdraw( sa );
				//}
			}

			sa = sa->next;
		}

		if( curarea != tempsa )
			areawinset( tempsa->win );

		if( curarea ) {	/* is null if Blender is in bg mode */
			if( curarea->headwin )
				scrarea_do_headdraw( curarea );
			screen_swapbuffers(  );
		}
	}

	Py_INCREF( Py_None );
	return Py_None;
}

/*****************************************************************************/
/* Function: M_Window_RedrawAll						*/
/* Python equivalent:	Blender.Window.RedrawAll			*/
/*****************************************************************************/
static PyObject *M_Window_RedrawAll( PyObject * self, PyObject * args )
{
	return M_Window_Redraw( self, Py_BuildValue( "(i)", -1 ) );
}

/*****************************************************************************/
/* Function:	M_Window_QRedrawAll					*/
/* Python equivalent:			Blender.Window.QRedrawAll	*/
/*****************************************************************************/
static PyObject *M_Window_QRedrawAll( PyObject * self, PyObject * args )
{
	allqueue( REDRAWALL, 0 );

	Py_INCREF( Py_None );
	return Py_None;
}

/*****************************************************************************/
/* Function:	M_Window_FileSelector					*/
/* Python equivalent:	Blender.Window.FileSelector			*/
/*****************************************************************************/

/* This is the callback to "activate_fileselect" below.  It receives the
 * selected filename and (using it as argument) calls the Python callback
 * provided by the script writer and stored in EXPP_FS_PyCallback. */

static void getSelectedFile( char *name )
{
	if( !EXPP_FS_PyCallback )
		return;

	PyObject_CallFunction( ( PyObject * ) EXPP_FS_PyCallback, "s", name );

	EXPP_FS_PyCallback = NULL;

	return;
}

static PyObject *M_Window_FileSelector( PyObject * self, PyObject * args )
{
	char *title = "SELECT FILE";
	char *filename = G.sce;
	SpaceScript *sc;
	Script *script = G.main->script.last;
	int startspace = 0;

	if( !PyArg_ParseTuple( args, "O!|ss",
			       &PyFunction_Type, &EXPP_FS_PyCallback, &title,
			       &filename ) )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "\nexpected a callback function (and optionally one or two strings) "
					      "as argument(s)" );

/* trick: we move to a spacescript because then the fileselector will properly
 * unset our SCRIPT_FILESEL flag when the user chooses a file or cancels the
 * selection.  This is necessary because when a user cancels, the
 * getSelectedFile function above doesn't get called and so couldn't unset the
 * flag. */
	startspace = curarea->spacetype;
	if( startspace != SPACE_SCRIPT )
		newspace( curarea, SPACE_SCRIPT );

	sc = curarea->spacedata.first;

	/* did we get the right script? */
	if( !( script->flags & SCRIPT_RUNNING ) ) {
		/* if not running, then we were already on a SpaceScript space, executing
		 * a registered callback -- aka: this script has a gui */
		script = sc->script;	/* this is the right script */
	} else {		/* still running, use the trick */
		script->lastspace = startspace;
		sc->script = script;
	}

	script->flags |= SCRIPT_FILESEL;

	activate_fileselect( FILE_BLENDER, title, filename, getSelectedFile );

	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *M_Window_ImageSelector( PyObject * self, PyObject * args )
{
	char *title = "SELECT IMAGE";
	char *filename = G.sce;
	SpaceScript *sc;
	Script *script = G.main->script.last;
	int startspace = 0;

	if( !PyArg_ParseTuple( args, "O!|ss",
			       &PyFunction_Type, &EXPP_FS_PyCallback, &title,
			       &filename ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_AttributeError,
			   "\nexpected a callback function (and optionally one or two strings) "
			   "as argument(s)" ) );

/* trick: we move to a spacescript because then the fileselector will properly
 * unset our SCRIPT_FILESEL flag when the user chooses a file or cancels the
 * selection.  This is necessary because when a user cancels, the
 * getSelectedFile function above doesn't get called and so couldn't unset the
 * flag. */
	startspace = curarea->spacetype;
	if( startspace != SPACE_SCRIPT )
		newspace( curarea, SPACE_SCRIPT );

	sc = curarea->spacedata.first;

	/* did we get the right script? */
	if( !( script->flags & SCRIPT_RUNNING ) ) {
		/* if not running, then we're on a SpaceScript space, executing a
		 * registered callback -- aka: this script has a gui */
		SpaceScript *sc = curarea->spacedata.first;
		script = sc->script;	/* this is the right script */
	} else {		/* still running, use the trick */
		script->lastspace = startspace;
		sc->script = script;
	}

	script->flags |= SCRIPT_FILESEL;	/* same flag as filesel */

	activate_imageselect( FILE_BLENDER, title, filename, getSelectedFile );

	Py_INCREF( Py_None );
	return Py_None;
}

/*****************************************************************************/
/* Function:	M_Window_DrawProgressBar		          	*/
/* Python equivalent:	Blender.Window.DrawProgressBar			*/
/*****************************************************************************/
static PyObject *M_Window_DrawProgressBar( PyObject * self, PyObject * args )
{
	float done;
	char *info = NULL;
	int retval = 0;

	if( !PyArg_ParseTuple( args, "fs", &done, &info ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected a float and a string as arguments" ) );

	if( !G.background )
		retval = progress_bar( done, info );

	return Py_BuildValue( "i", retval );
}

/*****************************************************************************/
/* Function:   M_Window_GetCursorPos					*/
/* Python equivalent:	Blender.Window.GetCursorPos			*/
/*****************************************************************************/
static PyObject *M_Window_GetCursorPos( PyObject * self )
{
	float *cursor = NULL;
	PyObject *pylist;

	if( G.vd && G.vd->localview )
		cursor = G.vd->cursor;
	else
		cursor = G.scene->cursor;

	pylist = Py_BuildValue( "[fff]", cursor[0], cursor[1], cursor[2] );

	if( !pylist )
		return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
						"GetCursorPos: couldn't create pylist" ) );

	return pylist;
}

/*****************************************************************************/
/* Function:	M_Window_SetCursorPos					*/
/* Python equivalent:	Blender.Window.SetCursorPos			*/
/*****************************************************************************/
static PyObject *M_Window_SetCursorPos( PyObject * self, PyObject * args )
{
	int ok = 0;
	float val[3];

	if( PyObject_Length( args ) == 3 )
		ok = PyArg_ParseTuple( args, "fff", &val[0], &val[1],
				       &val[2] );
	else
		ok = PyArg_ParseTuple( args, "(fff)", &val[0], &val[1],
				       &val[2] );

	if( !ok )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected [f,f,f] or f,f,f as arguments" );

	if( G.vd && G.vd->localview ) {
		G.vd->cursor[0] = val[0];
		G.vd->cursor[1] = val[1];
		G.vd->cursor[2] = val[2];
	} else {
		G.scene->cursor[0] = val[0];
		G.scene->cursor[1] = val[1];
		G.scene->cursor[2] = val[2];
	}

	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *M_Window_WaitCursor( PyObject * self, PyObject * args )
{
	int bool;

	if( !PyArg_ParseTuple( args, "i", &bool ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected bool (0 or 1) or nothing as argument" );

	waitcursor( bool );	/* nonzero bool sets, zero unsets */

	return EXPP_incr_ret( Py_None );
}

/*****************************************************************************/
/* Function:	M_Window_GetViewVector					*/
/* Python equivalent:	Blender.Window.GetViewVector			*/
/*****************************************************************************/
static PyObject *M_Window_GetViewVector( PyObject * self )
{
	float *vec = NULL;
	PyObject *pylist;

	if( !G.vd ) {
		Py_INCREF( Py_None );
		return Py_None;
	}

	vec = G.vd->viewinv[2];

	pylist = Py_BuildValue( "[fff]", vec[0], vec[1], vec[2] );

	if( !pylist )
		return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
						"GetViewVector: couldn't create pylist" ) );

	return pylist;
}

static PyObject *M_Window_GetViewQuat( PyObject * self )
{
	float *vec = NULL;
	PyObject *pylist;

	if( !G.vd ) {
		Py_INCREF( Py_None );
		return Py_None;
	}

	vec = G.vd->viewquat;

	pylist = Py_BuildValue( "[ffff]", vec[0], vec[1], vec[2], vec[3] );

	if( !pylist )
		return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
						"GetViewQuat: couldn't create pylist" ) );

	return pylist;
}

static PyObject *M_Window_SetViewQuat( PyObject * self, PyObject * args )
{
	int ok = 0;
	float val[4];

	if( !G.vd ) {
		Py_INCREF( Py_None );
		return Py_None;
	}

	if( PyObject_Length( args ) == 4 )
		ok = PyArg_ParseTuple( args, "ffff", &val[0], &val[1], &val[2],
				       &val[3] );
	else
		ok = PyArg_ParseTuple( args, "(ffff)", &val[0], &val[1],
				       &val[2], &val[3] );

	if( !ok )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected [f,f,f,f] or f,f,f,f as arguments" );

	G.vd->viewquat[0] = val[0];
	G.vd->viewquat[1] = val[1];
	G.vd->viewquat[2] = val[2];
	G.vd->viewquat[3] = val[3];

	return EXPP_incr_ret( Py_None );
}

static PyObject *M_Window_GetViewOffset( PyObject * self )
{
	float *vec = NULL;
	PyObject *pylist;

	if( !G.vd ) {
		Py_INCREF( Py_None );
		return Py_None;
	}

	vec = G.vd->ofs;

	pylist = Py_BuildValue( "[fff]", vec[0], vec[1], vec[2] );

	if( !pylist )
		return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
						"GetViewQuat: couldn't create pylist" ) );

	return pylist;
}

static PyObject *M_Window_SetViewOffset( PyObject * self, PyObject * args )
{
	int ok = 0;
	float val[3];

	if( !G.vd ) {
		Py_INCREF( Py_None );
		return Py_None;
	}

	if( PyObject_Length( args ) == 3 )
		ok = PyArg_ParseTuple( args, "fff", &val[0], &val[1],
				       &val[2] );
	else
		ok = PyArg_ParseTuple( args, "(fff)", &val[0], &val[1],
				       &val[2] );

	if( !ok )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected [f,f,f] or f,f,f as arguments" );

	G.vd->ofs[0] = val[0];
	G.vd->ofs[1] = val[1];
	G.vd->ofs[2] = val[2];

	return EXPP_incr_ret( Py_None );
}


/*****************************************************************************/
/* Function:	M_Window_GetViewMatrix				*/
/* Python equivalent:	Blender.Window.GetViewMatrix		*/
/*****************************************************************************/
static PyObject *M_Window_GetViewMatrix( PyObject * self )
{
	PyObject *viewmat;

	if( !G.vd ) {
		Py_INCREF( Py_None );
		return Py_None;
	}

	viewmat =
		( PyObject * ) newMatrixObject( ( float * ) G.vd->viewmat, 4,
						4 );

	if( !viewmat )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "GetViewMatrix: couldn't create matrix pyobject" );

	return viewmat;
}

static PyObject *M_Window_EditMode( PyObject * self, PyObject * args )
{
	short status = -1;
	char *undo_str = "From script";
	int undo_str_len = 11;

	if( !PyArg_ParseTuple
	    ( args, "|hs#", &status, &undo_str, &undo_str_len ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected nothing or an int (bool) and a string as arguments" );

	if( status >= 0 ) {
		if( status ) {
			if( !G.obedit )
				enter_editmode(  );
		} else if( G.obedit ) {
			if( undo_str_len > 63 )
				undo_str[63] = '\0';	/* 64 is max */
			undo_push_mesh( undo_str );	/* use better solution after 2.34 */
			exit_editmode( 1 );
		}
	}

	return Py_BuildValue( "h", G.obedit ? 1 : 0 );
}

static PyObject *M_Window_ViewLayer( PyObject * self, PyObject * args )
{
	PyObject *item = NULL;
	PyObject *list = NULL, *resl = NULL;
	int val, i, bit = 0, layer = 0;

	if( !PyArg_ParseTuple( args, "|O!", &PyList_Type, &list ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected nothing or a list of ints as argument" );

	if( list ) {
		for( i = 0; i < PyList_Size( list ); i++ ) {
			item = PyList_GetItem( list, i );
			if( !PyInt_Check( item ) )
				return EXPP_ReturnPyObjError
					( PyExc_AttributeError,
					  "list must contain only integer numbers" );

			val = ( int ) PyInt_AsLong( item );
			if( val < 1 || val > 20 )
				return EXPP_ReturnPyObjError
					( PyExc_AttributeError,
					  "layer values must be in the range [1, 20]" );

			layer |= 1 << ( val - 1 );
		}
		G.vd->lay = layer;

		while( bit < 20 ) {
			val = 1 << bit;
			if( layer & val ) {
				G.vd->layact = val;
				break;
			}
			bit++;
		}
	}

	resl = PyList_New( 0 );
	if( !resl )
		return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
						"couldn't create pylist!" ) );

	layer = G.vd->lay;

	bit = 0;
	while( bit < 20 ) {
		val = 1 << bit;
		if( layer & val ) {
			item = Py_BuildValue( "i", bit + 1 );
			PyList_Append( resl, item );
			Py_DECREF( item );
		}
		bit++;
	}

	return resl;
}

static PyObject *M_Window_CameraView( PyObject * self, PyObject * args )
{
	short camtov3d = 0;
	void setcameratoview3d( void );	/* view.c, used in toets.c */

	if( !PyArg_ParseTuple( args, "|i", &camtov3d ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected an int (from Window.Views) as argument" );

	if( !G.vd )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "View3d not available!" );

	if( !G.vd->camera ) {
		if( BASACT && OBACT->type == OB_CAMERA )
			G.vd->camera = OBACT;
		else
			G.vd->camera = scene_find_camera( G.scene );
		handle_view3d_lock(  );
	}

	G.vd->persp = 2;
	G.vd->view = 0;

	if( camtov3d )
		setcameratoview3d(  );

	return EXPP_incr_ret( Py_None );
}

static PyObject *M_Window_QTest( PyObject * self )
{
	return Py_BuildValue( "h", qtest(  ) );
}

static PyObject *M_Window_QRead( PyObject * self )
{
	short val = 0;
	unsigned short event;

	event = extern_qread( &val );

	return Py_BuildValue( "ii", event, val );
}

static PyObject *M_Window_QAdd( PyObject * self, PyObject * args )
{
	short win;
	short evt;		/* unsigned, we check below */
	short val;
	short after = 0;

	if( !PyArg_ParseTuple( args, "hhh|h", &win, &evt, &val, &after ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected three or four ints as arguments" );

	if( evt < 0 )		/* evt is unsigned short */
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "event value must be a positive int, check events in Blender.Draw" );

	if( after )
		addafterqueue( win, evt, val );
	else
		addqueue( win, evt, val );

	return EXPP_incr_ret( Py_None );
}

static PyObject *M_Window_QHandle( PyObject * self, PyObject * args )
{
	short win;
	ScrArea *sa = curarea;
	ScrArea *oldsa = NULL;

	if( !PyArg_ParseTuple( args, "h", &win ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected an int as argument" );

	while( sa ) {
		if( sa->win == win )
			break;
		sa = sa->next;
	}

	if( sa ) {
		BWinEvent evt;
		short do_redraw = 0, do_change = 0;

		if( sa != curarea || sa->win != mywinget(  ) ) {
			oldsa = curarea;
			areawinset( sa->win );
			set_g_activearea( sa );
		}
		while( bwin_qread( sa->win, &evt ) ) {
			if( evt.event == REDRAW ) {
				do_redraw = 1;
			} else if( evt.event == CHANGED ) {
				sa->win_swap = 0;
				do_change = 1;
				do_redraw = 1;
			} else {
				scrarea_do_winhandle( sa, &evt );
			}
		}
	}

	if( oldsa ) {
		areawinset( oldsa->win );
		set_g_activearea( oldsa );
	}

	return EXPP_incr_ret( Py_None );
}

static PyObject *M_Window_GetMouseCoords( PyObject * self )
{
	short mval[2];

	getmouse( mval );

	return Py_BuildValue( "hh", mval[0], mval[1] );
}

static PyObject *M_Window_SetMouseCoords( PyObject * self, PyObject * args )
{
	int ok, x, y;

	if( !G.curscreen )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "no current screen to retrieve info from!" );

	x = G.curscreen->sizex / 2;
	y = G.curscreen->sizey / 2;

	if( PyObject_Length( args ) == 2 )
		ok = PyArg_ParseTuple( args, "hh", &x, &y );
	else
		ok = PyArg_ParseTuple( args, "|(hh)", &x, &y );

	if( !ok )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected [i, i] or i,i as arguments (or nothing)." );

	warp_pointer( x, y );

	return EXPP_incr_ret( Py_None );
}

static PyObject *M_Window_GetMouseButtons( PyObject * self )
{
	short mbut = get_mbut(  );

	return Py_BuildValue( "h", mbut );
}

static PyObject *M_Window_GetKeyQualifiers( PyObject * self )
{
	short qual = get_qual(  );

	return Py_BuildValue( "h", qual );
}

static PyObject *M_Window_SetKeyQualifiers( PyObject * self, PyObject * args )
{
	short qual = 0;

	if( !PyArg_ParseTuple( args, "|h", &qual ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected nothing or an int (or'ed flags) as argument" );

	if( qual < 0 )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "value must be a positive int, check Blender.Window.Qual" );

	G.qual = qual;

	return Py_BuildValue( "h", qual );
}

static PyObject *M_Window_GetAreaSize( PyObject * self )
{
	ScrArea *sa = curarea;

	if( !sa )
		return EXPP_incr_ret( Py_None );

	return Py_BuildValue( "hh", sa->winx, sa->winy );
}

static PyObject *M_Window_GetAreaID( PyObject * self )
{
	ScrArea *sa = curarea;

	if( !sa )
		return EXPP_incr_ret( Py_None );

	return Py_BuildValue( "h", sa->win );
}

static PyObject *M_Window_GetScreenSize( PyObject * self )
{
	bScreen *scr = G.curscreen;

	if( !scr )
		return EXPP_incr_ret( Py_None );

	return Py_BuildValue( "hh", scr->sizex, scr->sizey );
}


static PyObject *M_Window_SetScreen( PyObject * self, PyObject * args )
{
	bScreen *scr = G.main->screen.first;
	char *name = NULL;

	if( !PyArg_ParseTuple( args, "s", &name ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected string as argument" );

	while( scr ) {
		if( !strcmp( scr->id.name + 2, name ) ) {
			setscreen( scr );
			break;
		}
		scr = scr->id.next;
	}

	if( !scr )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "no such screen, check Window.GetScreens() for valid names." );

	return EXPP_incr_ret( Py_None );
}

static PyObject *M_Window_GetScreens( PyObject * self )
{
	bScreen *scr = G.main->screen.first;
	PyObject *list = PyList_New( 0 );
	PyObject *str = NULL;

	if( !list )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create py list!" );

	while( scr ) {
		str = PyString_FromString( scr->id.name + 2 );

		if( !str ) {
			Py_DECREF( list );
			return EXPP_ReturnPyObjError( PyExc_MemoryError,
						      "couldn't create py string!" );
		}

		PyList_Append( list, str );	/* incref's str */
		Py_DECREF( str );

		scr = scr->id.next;
	}

	return list;
}

static PyObject *M_Window_GetScreenInfo( PyObject * self, PyObject * args,
					 PyObject * kwords )
{
	ScrArea *sa = G.curscreen->areabase.first;
	bScreen *scr = G.main->screen.first;
	PyObject *item, *list;
	rcti *rct;
	int type = -1;
	char *rect = "win";
	char *screen = "";
	static char *kwlist[] = { "type", "rect", "screen", NULL };
	int rctype = 0;

	if( !PyArg_ParseTupleAndKeywords( args, kwords, "|iss", kwlist, &type,
					  &rect, &screen ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected nothing or an int and two strings as arguments" );

	if( !strcmp( rect, "win" ) )
		rctype = 0;
	else if( !strcmp( rect, "total" ) )
		rctype = 1;
	else if( !strcmp( rect, "header" ) )
		rctype = 2;
	else
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "requested invalid type for area rectangle coordinates." );

	list = PyList_New( 0 );

	if( screen && screen[0] != '\0' ) {
		while( scr ) {
			if( !strcmp( scr->id.name + 2, screen ) ) {
				sa = scr->areabase.first;
				break;
			}
			scr = scr->id.next;
		}
	}

	if( !scr ) {
		Py_DECREF( list );
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "no such screen, see existing ones with Window.GetScreens." );
	}

	while( sa ) {
		if( type != -1 && sa->spacetype != type ) {
			sa = sa->next;
			continue;
		}

		switch ( rctype ) {
		case 0:
			rct = &sa->winrct;
			break;
		case 1:
			rct = &sa->totrct;
			break;
		case 2:
		default:
			rct = &sa->headrct;
		}

		item = Py_BuildValue( "{s:[h,h,h,h],s:h,s:h}",
				      "vertices", rct->xmin, rct->ymin,
				      rct->xmax, rct->ymax, "type",
				      ( short ) sa->spacetype, "id",
				      ( short ) sa->win );
		PyList_Append( list, item );
		Py_DECREF( item );

		sa = sa->next;
	}

	return list;
}

/*****************************************************************************/
/* Function:	Window_Init						*/
/*****************************************************************************/
PyObject *Window_Init( void )
{
	PyObject *submodule, *Types, *Qual, *dict;

	submodule =
		Py_InitModule3( "Blender.Window", M_Window_methods,
				M_Window_doc );

	dict = PyModule_GetDict( submodule );
	if( dict )
		PyDict_SetItemString( dict, "Theme", Theme_Init(  ) );

	Types = M_constant_New(  );
	Qual = M_constant_New(  );

	if( Types ) {
		BPy_constant *d = ( BPy_constant * ) Types;

		constant_insert( d, "VIEW3D", PyInt_FromLong( SPACE_VIEW3D ) );
		constant_insert( d, "IPO", PyInt_FromLong( SPACE_IPO ) );
		constant_insert( d, "OOPS", PyInt_FromLong( SPACE_OOPS ) );
		constant_insert( d, "BUTS", PyInt_FromLong( SPACE_BUTS ) );
		constant_insert( d, "FILE", PyInt_FromLong( SPACE_FILE ) );
		constant_insert( d, "IMAGE", PyInt_FromLong( SPACE_IMAGE ) );
		constant_insert( d, "INFO", PyInt_FromLong( SPACE_INFO ) );
		constant_insert( d, "SEQ", PyInt_FromLong( SPACE_SEQ ) );
		constant_insert( d, "IMASEL", PyInt_FromLong( SPACE_IMASEL ) );
		constant_insert( d, "SOUND", PyInt_FromLong( SPACE_SOUND ) );
		constant_insert( d, "ACTION", PyInt_FromLong( SPACE_ACTION ) );
		constant_insert( d, "TEXT", PyInt_FromLong( SPACE_TEXT ) );
		constant_insert( d, "NLA", PyInt_FromLong( SPACE_NLA ) );
		constant_insert( d, "SCRIPT", PyInt_FromLong( SPACE_SCRIPT ) );

		PyModule_AddObject( submodule, "Types", Types );
	}

	if( Qual ) {
		BPy_constant *d = ( BPy_constant * ) Qual;

		constant_insert( d, "LALT", PyInt_FromLong( L_ALTKEY ) );
		constant_insert( d, "RALT", PyInt_FromLong( R_ALTKEY ) );
		constant_insert( d, "ALT", PyInt_FromLong( LR_ALTKEY ) );
		constant_insert( d, "LCTRL", PyInt_FromLong( L_CTRLKEY ) );
		constant_insert( d, "RCTRL", PyInt_FromLong( R_CTRLKEY ) );
		constant_insert( d, "CTRL", PyInt_FromLong( LR_CTRLKEY ) );
		constant_insert( d, "LSHIFT", PyInt_FromLong( L_SHIFTKEY ) );
		constant_insert( d, "RSHIFT", PyInt_FromLong( R_SHIFTKEY ) );
		constant_insert( d, "SHIFT", PyInt_FromLong( LR_SHIFTKEY ) );

		PyModule_AddObject( submodule, "Qual", Qual );
	}

	return submodule;
}
