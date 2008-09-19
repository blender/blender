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
 * Contributor(s): Willian P. Germano, Tom Musgrove, Michael Reimpell,
 * Yann Vernier, Ken Hughes
 *
 * ***** END GPL LICENSE BLOCK *****
*/

#include <Python.h>

#include "BDR_editobject.h"	/* enter / leave editmode */
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_object.h"		/* for during_script() and during_scriptlink() */
#include "BKE_scene.h"		/* scene_find_camera() */
#include "BIF_mywindow.h"
#include "BIF_imasel.h"
#include "BSE_headerbuttons.h"
#include "BSE_filesel.h"
#include "BIF_editmesh.h"	/* for undo_push_mesh() */
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_drawtext.h"
#include "BIF_poseobject.h"
#include "BIF_toolbox.h"	/* for error() */
#include "DNA_view3d_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_text_types.h"
#include "DNA_object_types.h"
#include "mydevice.h"
#include "blendef.h"		/* OBACT */
#include "windowTheme.h"
#include "Mathutils.h"
#include "constant.h"
#include "gen_utils.h"
#include "Armature.h"

/* Pivot Types 
-0 for Bounding Box Center; \n\
-1 for 3D Cursor\n\
-2 for Individual Centers\n\
-3 for Median Point\n\
-4 for Active Object"; */

#define		PIVOT_BOUNDBOX		0
#define		PIVOT_CURSOR		1
#define		PIVOT_INDIVIDUAL	2
#define		PIVOT_MEDIAN		3
#define		PIVOT_ACTIVE		4

/* See Draw.c */
extern int EXPP_disable_force_draw;
extern void setcameratoview3d(void);

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
static PyObject *M_Window_GetActiveLayer( PyObject * self );
static PyObject *M_Window_SetActiveLayer( PyObject * self, PyObject * args );
static PyObject *M_Window_GetViewQuat( PyObject * self );
static PyObject *M_Window_SetViewQuat( PyObject * self, PyObject * args );
static PyObject *M_Window_GetViewOffset( PyObject * self );
static PyObject *M_Window_SetViewOffset( PyObject * self, PyObject * args );
static PyObject *M_Window_GetViewMatrix( PyObject * self );
static PyObject *M_Window_GetPerspMatrix( PyObject * self );
static PyObject *M_Window_FileSelector( PyObject * self, PyObject * args );
static PyObject *M_Window_ImageSelector( PyObject * self, PyObject * args );
static PyObject *M_Window_EditMode( PyObject * self, PyObject * args );
static PyObject *M_Window_PoseMode( PyObject * self, PyObject * args );
static PyObject *M_Window_ViewLayers( PyObject * self, PyObject * args );
static PyObject *M_Window_CameraView( PyObject * self, PyObject * args );
static PyObject *M_Window_QTest( PyObject * self );
static PyObject *M_Window_QRead( PyObject * self );
static PyObject *M_Window_QAdd( PyObject * self, PyObject * args );
static PyObject *M_Window_QHandle( PyObject * self, PyObject * args );
static PyObject *M_Window_TestBreak( PyObject * self );
static PyObject *M_Window_GetMouseCoords( PyObject * self );
static PyObject *M_Window_SetMouseCoords( PyObject * self, PyObject * args );
static PyObject *M_Window_GetMouseButtons( PyObject * self );
static PyObject *M_Window_GetKeyQualifiers( PyObject * self );
static PyObject *M_Window_SetKeyQualifiers( PyObject * self, PyObject * args );
static PyObject *M_Window_GetAreaSize( PyObject * self );
static PyObject *M_Window_GetAreaID( PyObject * self );
static PyObject *M_Window_GetScreenSize( PyObject * self );
static PyObject *M_Window_GetScreens( PyObject * self );
static PyObject *M_Window_SetScreen( PyObject * self, PyObject * value );
static PyObject *M_Window_GetScreenInfo( PyObject * self, PyObject * args,
					 PyObject * kwords );
static PyObject *M_Window_GetPivot( PyObject * self );
static PyObject *M_Window_SetPivot( PyObject * self, PyObject * value );

PyObject *Window_Init( void );


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

static char M_Window_GetActiveLayer_doc[] =
	"() - Get the current 3d views active layer where new objects are created.";

static char M_Window_SetActiveLayer_doc[] =
	"(int) - Set the current 3d views active layer where new objects are created.";

static char M_Window_GetViewMatrix_doc[] =
	"() - Get the current 3d view matrix.";

static char M_Window_GetPerspMatrix_doc[] =
	"() - Get the current 3d Persp matrix.";

static char M_Window_EditMode_doc[] =
	"() - Get the current status -- 0: not in edit mode; 1: in edit mode.\n\
(status) - if 1: enter edit mode; if 0: leave edit mode.\n\
Returns the current status.  This function is mostly useful to leave\n\
edit mode before applying changes to a mesh (otherwise the changes will\n\
be lost) and then returning to it upon leaving.";
static char M_Window_PoseMode_doc[] =
		"() - Get the current status -- 0: not in pose mode; 1: in edit mode";

static char M_Window_ViewLayers_doc[] =
	"(layers = [], winid = None) - Get/set active layers in all 3d View windows.\n\
() - Make no changes, only return currently visible layers.\n\
(layers = []) - a list of integers, each in the range [1, 20].\n\
(layers = [], winid = int) - layers as above, winid is an optional.\n\
arg that makes the function only set layers for that view.\n\
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

static char M_Window_TestBreak_doc[] =
	"() - Returns true if the user has pressed escape.";

static char M_Window_GetMouseCoords_doc[] =
	"() - Get mouse pointer's current screen coordinates.";

static char M_Window_SetMouseCoords_doc[] =
	"(x, y) - Set mouse pointer's current screen coordinates.\n\
(x,y) - ints ([x, y] also accepted): the new x, y coordinates.";

static char M_Window_GetMouseButtons_doc[] =
	"() - Get the current mouse button state (see Blender.Window.MButs dict).";

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

static char M_Window_SetPivot_doc[] = 
	"(Pivot) - Set Pivot Mode for 3D Viewport:\n\
Options are: \n\
-PivotTypes.BOUNDBOX for Bounding Box Center; \n\
-PivotTypes.CURSOR for 3D Cursor\n\
-PivotTypes.INDIVIDUAL for Individual Centers\n\
-PivotTypes.MEDIAN for Median Point\n\
-PivotTypes.ACTIVE for Active Object";

static char M_Window_GetPivot_doc[] = 
	"Return the pivot for the active 3d window";

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
	{"GetActiveLayer", ( PyCFunction ) M_Window_GetActiveLayer, METH_NOARGS,
	 M_Window_GetActiveLayer_doc},
	{"SetActiveLayer", ( PyCFunction ) M_Window_SetActiveLayer, METH_VARARGS,
	 M_Window_SetActiveLayer_doc},
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
	{"GetPerspMatrix", ( PyCFunction ) M_Window_GetPerspMatrix, METH_NOARGS,
	 M_Window_GetPerspMatrix_doc},
	{"EditMode", ( PyCFunction ) M_Window_EditMode, METH_VARARGS,
	 M_Window_EditMode_doc},
	{"PoseMode", ( PyCFunction ) M_Window_PoseMode, METH_VARARGS,
	 M_Window_PoseMode_doc},
	{"ViewLayers", ( PyCFunction ) M_Window_ViewLayers, METH_VARARGS,
	 M_Window_ViewLayers_doc},
	 /* typo, deprecate someday: */
	{"ViewLayer", ( PyCFunction ) M_Window_ViewLayers, METH_VARARGS,
	 M_Window_ViewLayers_doc},
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
	{"TestBreak", ( PyCFunction ) M_Window_TestBreak, METH_NOARGS,
	 M_Window_TestBreak_doc},
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
	{"SetScreen", ( PyCFunction ) M_Window_SetScreen, METH_O,
	 M_Window_SetScreen_doc},
	{"GetScreenInfo", ( PyCFunction ) M_Window_GetScreenInfo,
	 METH_VARARGS | METH_KEYWORDS, M_Window_GetScreenInfo_doc},
	{"GetPivot", ( PyCFunction ) M_Window_GetPivot, METH_NOARGS, 
	 M_Window_GetPivot_doc},
	{"SetPivot", ( PyCFunction ) M_Window_SetPivot, METH_O, 
	 M_Window_SetPivot_doc},
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
	int wintype = SPACE_VIEW3D;
	short redraw_all = 0;

	if( !PyArg_ParseTuple( args, "|i", &wintype ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected int argument (or nothing)" ) );

	if( wintype < 0 )
		redraw_all = 1;

	if( !during_script( ) && !G.background ) {
		tempsa = curarea;
		sa = G.curscreen->areabase.first;

		while( sa ) {

			if( sa->spacetype == wintype || redraw_all ) {
				if (sa->spacetype == SPACE_SCRIPT && EXPP_disable_force_draw) {
						scrarea_queue_redraw(sa);
				}
				else {
					/* do not call fancy hacks here like pop_space_text(st); (ton) */
					scrarea_do_windraw( sa );
					if( sa->headwin ) scrarea_do_headdraw( sa );
				}
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

	Py_RETURN_NONE;
}

/*****************************************************************************/
/* Function: M_Window_RedrawAll						*/
/* Python equivalent:	Blender.Window.RedrawAll			*/
/*****************************************************************************/
static PyObject *M_Window_RedrawAll( PyObject * self, PyObject * args )
{
	PyObject *arg = Py_BuildValue( "(i)", -1 );
	PyObject *ret = M_Window_Redraw( self, arg );
	Py_DECREF(arg);
	return ret;
}

/*****************************************************************************/
/* Function:	M_Window_QRedrawAll					*/
/* Python equivalent:			Blender.Window.QRedrawAll	*/
/*****************************************************************************/
static PyObject *M_Window_QRedrawAll( PyObject * self, PyObject * args )
{
	EXPP_allqueue( REDRAWALL, 0 );
	Py_RETURN_NONE;
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
	PyObject *pycallback;
	PyObject *result;
	Script *script;

	/* let's find the script that owns this callback */
	script = G.main->script.first;
	while (script) {
		if (script->flags & SCRIPT_RUNNING) break;
		script = script->id.next;
	}

	if (!script) {
		if (curarea->spacetype == SPACE_SCRIPT) {
			SpaceScript *sc = curarea->spacedata.first;
			script = sc->script;
		}
	}
	/* If 'script' is null,
	 * The script must have had an error and closed,
	 * but the fileselector was left open, show an error and exit */
	if (!script) {
		error("Python script error: script quit, cannot run callback");
		return;
	}
		

	pycallback = script->py_browsercallback;

	if (pycallback) {
		PyGILState_STATE gilstate = PyGILState_Ensure();

		result = PyObject_CallFunction( pycallback, "s", name );

		if (!result) {
			if (G.f & G_DEBUG)
				fprintf(stderr, "BPy error: Callback call failed!\n");
		}
		else Py_DECREF(result);
		
		
			
		if (script->py_browsercallback == pycallback) {
			if (script->flags & SCRIPT_GUI) {
				script->py_browsercallback = NULL;
			} else {
				SCRIPT_SET_NULL(script);
			}
		}
		
		/* else another call to selector was made inside pycallback */

		Py_DECREF(pycallback);

		PyGILState_Release(gilstate);
	}

	return;
}

/* Use for file and image selector */
static PyObject * FileAndImageSelector(PyObject * self, PyObject * args, int type)
{
	char *title = (type==0 ? "SELECT FILE" : "SELECT IMAGE");
	char *filename = G.sce;
	SpaceScript *sc;
	Script *script = NULL;
	PyObject *pycallback = NULL;
	int startspace = 0;
	
	if (during_scriptlink())
		return EXPP_ReturnPyObjError(PyExc_RuntimeError,
			"script links can't call the file/image selector");

	if (G.background)
		return EXPP_ReturnPyObjError(PyExc_RuntimeError,
			"the file/image selector is not available in background mode");

	if((!PyArg_ParseTuple( args, "O|ss", &pycallback, &title, &filename))
		|| (!PyCallable_Check(pycallback)))
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
			"\nexpected a callback function (and optionally one or two strings) "
			"as argument(s)" );

	Py_INCREF(pycallback);

/* trick: we move to a spacescript because then the fileselector will properly
 * unset our SCRIPT_FILESEL flag when the user chooses a file or cancels the
 * selection.  This is necessary because when a user cancels, the
 * getSelectedFile function above doesn't get called and so couldn't unset the
 * flag. */
	startspace = curarea->spacetype;
	if( startspace != SPACE_SCRIPT )
		newspace( curarea, SPACE_SCRIPT );

	sc = curarea->spacedata.first;

	/* let's find the script that called us */
	script = G.main->script.first;
	while (script) {
		if (script->flags & SCRIPT_RUNNING) break;
		script = script->id.next;
	}

	if( !script ) {
		/* if not running, then we were already on a SpaceScript space, executing
		 * a registered callback -- aka: this script has a gui */
		script = sc->script;	/* this is the right script */
	} else {		/* still running, use the trick */
		script->lastspace = startspace;
		sc->script = script;
	}

	script->flags |= SCRIPT_FILESEL;

	/* clear any previous callback (nested calls to selector) */
	if (script->py_browsercallback) {
		Py_DECREF((PyObject *)script->py_browsercallback);
	}
	script->py_browsercallback = pycallback;

	/* if were not running a script GUI here alredy, then dont make this script persistant */
	if ((script->flags & SCRIPT_GUI)==0) {
		script->scriptname[0] = '\0';
		script->scriptarg[0] = '\0';
	}
	if (type==0) {
		activate_fileselect( FILE_BLENDER, title, filename, getSelectedFile );
	} else {
		activate_imageselect( FILE_BLENDER, title, filename, getSelectedFile );
	}
	Py_RETURN_NONE;
}

static PyObject *M_Window_FileSelector( PyObject * self, PyObject * args )
{
	return FileAndImageSelector( self, args, 0 );
}

static PyObject *M_Window_ImageSelector( PyObject * self, PyObject * args )
{
	return FileAndImageSelector( self, args, 1 );
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
	ScrArea *sa = curarea;

	if (G.background)
		return EXPP_ReturnPyObjError(PyExc_RuntimeError,
			"the progress bar is not available in background mode");

	if( !PyArg_ParseTuple( args, "fs", &done, &info ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
			"expected a float and a string as arguments" ) );

	retval = progress_bar( done, info );

	areawinset(sa->win);

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

	Py_RETURN_NONE;
}

static PyObject *M_Window_WaitCursor( PyObject * self, PyObject * args )
{
	int bool;

	if( !PyArg_ParseTuple( args, "i", &bool ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected bool (0 or 1) or nothing as argument" );

	waitcursor( bool );	/* nonzero bool sets, zero unsets */

	Py_RETURN_NONE;
}

/*****************************************************************************/
/* Function:	M_Window_GetViewVector					*/
/* Python equivalent:	Blender.Window.GetViewVector			*/
/*****************************************************************************/
static PyObject *M_Window_GetViewVector( PyObject * self )
{
	float *vec = NULL;

	if( !G.vd )
		Py_RETURN_NONE;

	vec = G.vd->viewinv[2];

	return Py_BuildValue( "[fff]", vec[0], vec[1], vec[2] );
}

/*****************************************************************************/
/* Function:	M_Window_GetActiveLayer					*/
/* Python equivalent:	Blender.Window.GetActiveLayer			*/
/*****************************************************************************/
static PyObject *M_Window_GetActiveLayer( PyObject * self )
{
	if (!G.vd) {
		return PyInt_FromLong(0);
	} else {
		return PyInt_FromLong( G.vd->layact );
	}
}

static PyObject *M_Window_SetActiveLayer( PyObject * self, PyObject * args )
{
	int layer, bit=1;
	if( !PyArg_ParseTuple( args, "i", &layer ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected an int" ) );
	
	if (!G.vd)
		Py_RETURN_FALSE;
	
	bit= 0;
	while(bit<32) {
		if(layer & (1<<bit)) {
			G.vd->layact= 1<<bit;
			G.vd->lay |= G.vd->layact;
			
			if (G.vd->scenelock) {
				G.scene->lay |= G.vd->layact; 
			}
			bit = -1; /* no error */
			break;
		}
		bit++;
	}
	
	if (bit != -1)
		return ( EXPP_ReturnPyObjError( PyExc_ValueError,
					"The flag could not be used for the active layer" ) );
	
	Py_RETURN_NONE;
}

static PyObject *M_Window_GetViewQuat( PyObject * self )
{
	float *vec = NULL;

	if( !G.vd )
		Py_RETURN_NONE;

	vec = G.vd->viewquat;

	return Py_BuildValue( "[ffff]", vec[0], vec[1], vec[2], vec[3] );
}

static PyObject *M_Window_SetViewQuat( PyObject * self, PyObject * args )
{
	int ok = 0;
	float val[4];

	if( !G.vd )
		Py_RETURN_NONE;

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

	Py_RETURN_NONE;
}

static PyObject *M_Window_GetViewOffset( PyObject * self )
{
	if( !G.vd )
		Py_RETURN_NONE;
	return Py_BuildValue( "[fff]", G.vd->ofs[0], G.vd->ofs[1], G.vd->ofs[2] );
}

static PyObject *M_Window_SetViewOffset( PyObject * self, PyObject * args )
{
	int ok = 0;
	float val[3];

	if( !G.vd )
		Py_RETURN_NONE;

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

	Py_RETURN_NONE;
}


/*****************************************************************************/
/* Function:	M_Window_GetViewMatrix				*/
/* Python equivalent:	Blender.Window.GetViewMatrix		*/
/*****************************************************************************/
static PyObject *M_Window_GetViewMatrix( PyObject * self )
{
	if( !G.vd )
		Py_RETURN_NONE;

	return newMatrixObject( ( float * ) G.vd->viewmat, 4, 4, Py_WRAP );
}

/*****************************************************************************/
/* Function:	M_Window_GetPerspMatrix				*/
/* Python equivalent:	Blender.Window.GetPerspMatrix		*/
/*****************************************************************************/
static PyObject *M_Window_GetPerspMatrix( PyObject * self )
{
	if( !G.vd )
		Py_RETURN_NONE;

	return newMatrixObject( ( float * ) G.vd->persmat, 4, 4, Py_WRAP );
}


/* update_armature_weakrefs()
 * helper function used in M_Window_EditMode.
 * rebuilds list of Armature weakrefs in __main__
 */

static int update_armature_weakrefs()
{
	/* stuff for armature weak refs */
	char *list_name = ARM_WEAKREF_LIST_NAME;
	PyObject *maindict = NULL, *armlist = NULL;
	PyObject *pyarmature = NULL;
	int x;

	maindict= PyModule_GetDict(PyImport_AddModule(	"__main__"));
	armlist = PyDict_GetItemString(maindict, list_name);
	if( !armlist){
		printf("Oops - update_armature_weakrefs()\n");
		return 0;
	}

	for (x = 0; x < PySequence_Size(armlist); x++){
		pyarmature = PyWeakref_GetObject(PySequence_GetItem(	armlist, x));
		if (pyarmature != Py_None)
			Armature_RebuildEditbones(pyarmature);
	}
	return 1;
}


static PyObject *M_Window_EditMode( PyObject * self, PyObject * args )
{
	short status = -1;
	char *undo_str = "From script";
	int undo_str_len = 11;
	int do_undo = 1;

	if( !PyArg_ParseTuple( args,
				"|hs#i", &status, &undo_str, &undo_str_len, &do_undo ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected optional int (bool), string and int (bool) as arguments" );

	if( status >= 0 ) {
		if( status ) {
			if( !G.obedit ){

				//update armatures
				if(! update_armature_weakrefs()){
					return EXPP_ReturnPyObjError( 
						PyExc_RuntimeError,
						"internal error -  update_armature_weakrefs");
				}

				//enter editmode
				enter_editmode(0);
			}
		} else if( G.obedit ) {
			if( undo_str_len > 63 )
				undo_str[63] = '\0';	/* 64 is max */
			BIF_undo_push( undo_str ); /* This checks user undo settings */
			exit_editmode( EM_FREEDATA );

			//update armatures
			if(! update_armature_weakrefs()){
				return EXPP_ReturnPyObjError( 
					PyExc_RuntimeError,
					"internal error -  update_armature_weakrefs");
			}

		}
	}

	return Py_BuildValue( "h", G.obedit ? 1 : 0 );
}

static PyObject *M_Window_PoseMode( PyObject * self, PyObject * args )
{
	short status = -1;
	short is_posemode = 0;
	Base *base;
	
	if( !PyArg_ParseTuple( args, "|h", &status ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
									  "expected optional int (bool) as argument" );

	if( status >= 0 ) {
		if( status ) {
			enter_posemode();
		} else if( G.obedit ) {
			exit_posemode();
		}
	}

	base= BASACT;
	if (base && base->object->flag & OB_POSEMODE) {
		is_posemode = 1;
	}
	
	return Py_BuildValue( "h", is_posemode );
}

static PyObject *M_Window_ViewLayers( PyObject * self, PyObject * args )
{
	PyObject *item = NULL;
	PyObject *list = NULL, *resl = NULL;
	int val, i, bit = 0, layer = 0, len_list;
	short winid = -1;

	if( !G.scene ) {
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"can't get pointer to global scene" );
	}

	/* Pase Args, Nothing, One list, Or a list and an int */
	if (PyTuple_GET_SIZE(args)!=0) {
		if( !PyArg_ParseTuple ( args, "O!|h", &PyList_Type, &list, &winid) ) {
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"nothing or a list and optionaly a window ID argument" );	
		}
	}
	
	if( list ) {
		len_list = PyList_Size(list);

		if (len_list == 0)
			return EXPP_ReturnPyObjError( PyExc_AttributeError,
		  	"list can't be empty, at list one layer must be set" );

		for( i = 0; i < len_list; i++ ) {
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
		
		if (winid==-1) {
			/* set scene and viewport */
			G.scene->lay = layer;
			if (G.vd) {
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
		} else {
			/* only set the windows layer */
			ScrArea *sa;
			SpaceLink *sl;
			View3D *vd;
			 
			if (G.curscreen) { /* can never be too careful */
	            for (sa=G.curscreen->areabase.first; sa; sa= sa->next) {
	            	if (winid == sa->win) {
	            		
	            		for (sl= sa->spacedata.first; sl; sl= sl->next)
	            			if(sl->spacetype==SPACE_VIEW3D)
	            				break;
	            		
	            		if (!sl)
	            			return EXPP_ReturnPyObjError( PyExc_ValueError,
	            				"The window matching the winid has no 3d viewport" );
	            		
	            		vd= (View3D *) sl;
	            		vd->lay = layer;
	            		
	            		for(bit= 0; bit < 20; bit++) {
	            			val = 1 << bit;
	            			if( layer & val ) {
	            				vd->layact = val;
	            				 break;
	            			}
	            		}
	            		
	            		winid = -1; /* so we know its done */
	            		break;
	            	}
	            }
				if (winid!=-1)
					return EXPP_ReturnPyObjError( PyExc_TypeError,
							"The winid argument did not match any window" );
			}
		}
	}

	resl = PyList_New( 0 );
	if( !resl )
		return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
						"couldn't create pylist!" ) );

	layer = G.scene->lay;

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

	if( !PyArg_ParseTuple( args, "|i", &camtov3d ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected an int (from Window.Views) as argument" );

	if( !G.vd )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"this function can only be used after a 3d View has been initialized" );

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

	Py_RETURN_NONE;
}

static PyObject *M_Window_QTest( PyObject * self )
{
	return Py_BuildValue( "h", qtest(  ) );
}

static PyObject *M_Window_QRead( PyObject * self )
{
	short val = 0;
	unsigned short event;

	if (G.background)
		return EXPP_ReturnPyObjError(PyExc_RuntimeError,
			"QRead is not available in background mode");

	event = extern_qread( &val );

	return Py_BuildValue( "ii", event, val );
}

static PyObject *M_Window_QAdd( PyObject * self, PyObject * args )
{
	short win;
	short evt;		/* unsigned, we check below */
	short val;
	short after = 0;

	if (G.background)
		return EXPP_ReturnPyObjError(PyExc_RuntimeError,
			"QAdd is not available in background mode");

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

	Py_RETURN_NONE;
}

static PyObject *M_Window_QHandle( PyObject * self, PyObject * args )
{
	short win;
	ScrArea *sa;
	ScrArea *oldsa = NULL;

	if (G.background)
		return EXPP_ReturnPyObjError(PyExc_RuntimeError,
			"QHandle is not available in background mode");

	if (!G.curscreen)
		return EXPP_ReturnPyObjError(PyExc_RuntimeError,
			"No screens available");
	
	if( !PyArg_ParseTuple( args, "h", &win ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected an int as argument" );
	
	for (sa= G.curscreen->areabase.first; sa; sa= sa->next)
		if( sa->win == win )
			break;

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

	Py_RETURN_NONE;
}

static PyObject *M_Window_TestBreak( PyObject * self )
{
	if (blender_test_break()) {
		G.afbreek= 0;
		Py_RETURN_TRUE;
	} else {
		Py_RETURN_FALSE;
	}
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

	Py_RETURN_NONE;
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
		Py_RETURN_NONE;

	return Py_BuildValue( "hh", sa->winx, sa->winy );
}

static PyObject *M_Window_GetAreaID( PyObject * self )
{
	ScrArea *sa = curarea;

	if( !sa )
		Py_RETURN_NONE;

	return Py_BuildValue( "h", sa->win );
}

static PyObject *M_Window_GetScreenSize( PyObject * self )
{
	bScreen *scr = G.curscreen;

	if( !scr )
		Py_RETURN_NONE;

	return Py_BuildValue( "hh", scr->sizex, scr->sizey );
}


static PyObject *M_Window_SetScreen( PyObject * self, PyObject * value )
{
	bScreen *scr = G.main->screen.first;
	char *name = PyString_AsString(value);

	if( !name )
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
					      "no such screen, check Window.GetScreens() for valid names" );

	Py_RETURN_NONE;
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

static PyObject *M_Window_GetPivot( PyObject * self )
{
	if (G.vd) {
		return PyInt_FromLong( G.vd->around );
	}
	Py_RETURN_NONE;
}

static PyObject *M_Window_SetPivot( PyObject * self, PyObject * value)

{
	short pivot;
	if (G.vd) {
		pivot = (short)PyInt_AsLong( value );

		if ( pivot > 4 || pivot < 0 )
			return EXPP_ReturnPyObjError( PyExc_AttributeError,
							  "Expected a constant from Window.PivotTypes" );
		
		G.vd->around = pivot;
	}
	Py_RETURN_NONE;
}	


/*****************************************************************************/
/* Function:	Window_Init						*/
/*****************************************************************************/
PyObject *Window_Init( void )
{
	PyObject *submodule, *Types, *Qual, *MButs, *PivotTypes, *dict;

	submodule =
		Py_InitModule3( "Blender.Window", M_Window_methods,
				M_Window_doc );

	dict = PyModule_GetDict( submodule );
	if( dict )
		PyDict_SetItemString( dict, "Theme", Theme_Init(  ) );

	Types = PyConstant_New(  );
	Qual = PyConstant_New(  );
	MButs = PyConstant_New(  );
	PivotTypes = PyConstant_New(  );

	if( Types ) {
		BPy_constant *d = ( BPy_constant * ) Types;

		PyConstant_Insert( d, "VIEW3D", PyInt_FromLong( SPACE_VIEW3D ) );
		PyConstant_Insert( d, "IPO", PyInt_FromLong( SPACE_IPO ) );
		PyConstant_Insert( d, "OOPS", PyInt_FromLong( SPACE_OOPS ) );
		PyConstant_Insert( d, "BUTS", PyInt_FromLong( SPACE_BUTS ) );
		PyConstant_Insert( d, "FILE", PyInt_FromLong( SPACE_FILE ) );
		PyConstant_Insert( d, "IMAGE", PyInt_FromLong( SPACE_IMAGE ) );
		PyConstant_Insert( d, "INFO", PyInt_FromLong( SPACE_INFO ) );
		PyConstant_Insert( d, "SEQ", PyInt_FromLong( SPACE_SEQ ) );
		PyConstant_Insert( d, "IMASEL", PyInt_FromLong( SPACE_IMASEL ) );
		PyConstant_Insert( d, "SOUND", PyInt_FromLong( SPACE_SOUND ) );
		PyConstant_Insert( d, "ACTION", PyInt_FromLong( SPACE_ACTION ) );
		PyConstant_Insert( d, "TEXT", PyInt_FromLong( SPACE_TEXT ) );
		PyConstant_Insert( d, "NLA", PyInt_FromLong( SPACE_NLA ) );
		PyConstant_Insert( d, "SCRIPT", PyInt_FromLong( SPACE_SCRIPT ) );
		PyConstant_Insert( d, "TIME", PyInt_FromLong( SPACE_TIME ) );
		PyConstant_Insert( d, "NODE", PyInt_FromLong( SPACE_NODE ) );

		PyModule_AddObject( submodule, "Types", Types );
	}

	if( Qual ) {
		BPy_constant *d = ( BPy_constant * ) Qual;

		PyConstant_Insert( d, "LALT", PyInt_FromLong( L_ALTKEY ) );
		PyConstant_Insert( d, "RALT", PyInt_FromLong( R_ALTKEY ) );
		PyConstant_Insert( d, "ALT", PyInt_FromLong( LR_ALTKEY ) );
		PyConstant_Insert( d, "LCTRL", PyInt_FromLong( L_CTRLKEY ) );
		PyConstant_Insert( d, "RCTRL", PyInt_FromLong( R_CTRLKEY ) );
		PyConstant_Insert( d, "CTRL", PyInt_FromLong( LR_CTRLKEY ) );
		PyConstant_Insert( d, "LSHIFT", PyInt_FromLong( L_SHIFTKEY ) );
		PyConstant_Insert( d, "RSHIFT", PyInt_FromLong( R_SHIFTKEY ) );
		PyConstant_Insert( d, "SHIFT", PyInt_FromLong( LR_SHIFTKEY ) );

		PyModule_AddObject( submodule, "Qual", Qual );
	}

	if( MButs ) {
		BPy_constant *d = ( BPy_constant * ) MButs;

		PyConstant_Insert( d, "L", PyInt_FromLong( L_MOUSE ) );
		PyConstant_Insert( d, "M", PyInt_FromLong( M_MOUSE ) );
		PyConstant_Insert( d, "R", PyInt_FromLong( R_MOUSE ) );

		PyModule_AddObject( submodule, "MButs", MButs );
	}

	if( PivotTypes ) {
		BPy_constant *d = ( BPy_constant * ) PivotTypes;

		PyConstant_Insert(d, "BOUNDBOX", PyInt_FromLong( PIVOT_BOUNDBOX ) );
		PyConstant_Insert(d, "CURSOR", PyInt_FromLong( PIVOT_CURSOR ) );
		PyConstant_Insert(d, "MEDIAN", PyInt_FromLong( PIVOT_MEDIAN ) );
		PyConstant_Insert(d, "ACTIVE", PyInt_FromLong( PIVOT_ACTIVE ) );
		PyConstant_Insert(d, "INDIVIDUAL", PyInt_FromLong( PIVOT_INDIVIDUAL ) );

		PyModule_AddObject( submodule, "PivotTypes", PivotTypes );
	}
	return submodule;
}
