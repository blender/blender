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
 * Contributor(s): Willian P. Germano, Campbell Barton
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

/* This file is the Blender.Draw part of opy_draw.c, from the old
 * bpython/intern dir, with minor changes to adapt it to the new Python
 * implementation.	Non-trivial original comments are marked with an
 * @ symbol at their beginning. */

#include "Draw.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "BLI_blenlib.h"
#include "MEM_guardedalloc.h"

#include "BMF_Api.h"

#include "DNA_screen_types.h"

#include "DNA_text_types.h"

#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_object.h"

#include "BIF_gl.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_toolbox.h"

#include "BPI_script.h"	/* script struct */

#include "interface.h"
#include "mydevice.h"	/*@ for all the event constants */

/* This one was an extern in BPY_main.h, but only opy_draw.c was using it */
int g_window_redrawn;

/*@ hack to flag that window redraw has happened inside slider callback: */
int EXPP_disable_force_draw = 0;

/* forward declarations for internal functions */
static void Button_dealloc (PyObject *self);
static PyObject *Button_getattr (PyObject *self, char *name);
static PyObject *Button_repr (PyObject *self);
static int Button_setattr (PyObject *self, char *name, PyObject *v);

static Button *newbutton (void);

/* GUI interface routines */

static void exit_pydraw (SpaceScript *sc, short error);
static void exec_callback (SpaceScript *sc, PyObject *callback,
	PyObject *args);
static void spacescript_do_pywin_buttons (SpaceScript *sc,
	unsigned short event);

static PyObject *Method_Exit (PyObject * self, PyObject * args);
static PyObject *Method_Register (PyObject * self, PyObject * args);
static PyObject *Method_Redraw (PyObject * self, PyObject * args);
static PyObject *Method_Draw (PyObject * self, PyObject * args);
static PyObject *Method_Create (PyObject * self, PyObject * args);

static PyObject *Method_Button (PyObject * self, PyObject * args);
static PyObject *Method_Menu (PyObject * self, PyObject * args);
static PyObject *Method_Toggle (PyObject * self, PyObject * args);
static PyObject *Method_Slider (PyObject * self, PyObject * args);
static PyObject *Method_Scrollbar (PyObject * self, PyObject * args);
static PyObject *Method_Number (PyObject * self, PyObject * args);
static PyObject *Method_String (PyObject * self, PyObject * args);
static PyObject *Method_GetStringWidth (PyObject * self, PyObject * args);
static PyObject *Method_Text (PyObject * self, PyObject * args);
static PyObject *Method_PupMenu (PyObject * self, PyObject * args);
/* next three by Campbell: */
static PyObject *Method_PupIntInput (PyObject * self, PyObject * args);
static PyObject *Method_PupFloatInput (PyObject * self, PyObject * args);
static PyObject *Method_PupStrInput (PyObject * self, PyObject * args);

static uiBlock *Get_uiBlock (void);
static void py_slider_update (void *butv, void *data2_unused);

static char Draw_doc[] = "The Blender.Draw submodule";

static char Method_Register_doc[] =
"(draw, event, button) - Register callbacks for windowing\n\n\
(draw) A function to draw the screen, taking no arguments\n\
(event) A function to handle events, taking 2 arguments (evt, val)\n\
	(evt) The event number\n\
	(val) The value modifier (for key and mouse press/release)\n\
(button) A function to handle button events, taking 1 argument (evt)\n\
	(evt) The button number\n\n\
A None object can be passed if a callback is unused.";


static char Method_Redraw_doc[] = "([after]) - Queue a redraw event\n\n\
[after=0] Determines whether the redraw is processed before\n\
or after other input events.\n\n\
Redraw events are buffered so that regardless of how many events\n\
are queued the window only receives one redraw event.";

static char Method_Draw_doc[] = "() - Force an immediate redraw\n\n\
Forced redraws are not buffered, in other words the window is redrawn\n\
exactly once for everytime this function is called.";


static char Method_Create_doc[] =
"(value) - Create a default Button object\n\n\
 (value) - The value to store in the button\n\n\
 Valid values are ints, floats, and strings";

static char Method_Button_doc[] =
"(name, event, x, y, width, height, [tooltip]) - Create a new Button \
(push) button\n\n\
(name) A string to display on the button\n\
(event) The event number to pass to the button event function when activated\n\
(x, y) The lower left coordinate of the button\n\
(width, height) The button width and height\n\
[tooltip=] The button's tooltip\n\n\
This function can be called as Button() or PushButton().";

static char Method_Menu_doc[] =
"(name, event, x, y, width, height, default, [tooltip]) - Create a new Menu \
button\n\n\
(name) A string to display on the button\n\
(event) The event number to pass to the button event function when activated\n\
(x, y) The lower left coordinate of the button\n\
(width, height) The button width and height\n\
(default) The number of the option to be selected by default\n\
[tooltip=" "] The button's tooltip\n\n\
The menu options are specified through the name of the\n\
button. Options are followed by a format code and separated\n\
by the '|' (pipe) character.\n\
Valid format codes are\n\
	%t - The option should be used as the title\n\
	%xN - The option should set the integer N in the button value.";

static char Method_Toggle_doc[] =
"(name, event, x, y, width, height, default, [tooltip]) - Create a new Toggle \
button\n\n\
(name) A string to display on the button\n\
(event) The event number to pass to the button event function when activated\n\
(x, y) The lower left coordinate of the button\n\
(width, height) The button width and height\n\
(default) An integer (0 or 1) specifying the default state\n\
[tooltip=] The button's tooltip";


static char Method_Slider_doc[] =
"(name, event, x, y, width, height, initial, min, max, [update, tooltip]) - \
Create a new Slider button\n\n\
(name) A string to display on the button\n\
(event) The event number to pass to the button event function when activated\n\
(x, y) The lower left coordinate of the button\n\
(width, height) The button width and height\n\
(initial, min, max) Three values (int or float) specifying the initial \
				and limit values.\n\
[update=1] A value controlling whether the slider will emit events as it \
is edited.\n\
	A non-zero value (default) enables the events. A zero value supresses them.\n\
[tooltip=] The button's tooltip";


static char Method_Scrollbar_doc[] =
"(event, x, y, width, height, initial, min, max, [update, tooltip]) - Create a \
new Scrollbar\n\n\
(event) The event number to pass to the button event function when activated\n\
(x, y) The lower left coordinate of the button\n\
(width, height) The button width and height\n\
(initial, min, max) Three values (int or float) specifying the initial and limit values.\n\
[update=1] A value controlling whether the slider will emit events as it is edited.\n\
	A non-zero value (default) enables the events. A zero value supresses them.\n\
[tooltip=] The button's tooltip";

static char Method_Number_doc[] =
"(name, event, x, y, width, height, initial, min, max, [tooltip]) - Create a \
new Number button\n\n\
(name) A string to display on the button\n\
(event) The event number to pass to the button event function when activated\n\
(x, y) The lower left coordinate of the button\n\
(width, height) The button width and height\n\
(initial, min, max) Three values (int or float) specifying the initial and \
limit values.\n\
[tooltip=] The button's tooltip";

static char Method_String_doc[] =
"(name, event, x, y, width, height, initial, length, [tooltip]) - Create a \
new String button\n\n\
(name) A string to display on the button\n\
(event) The event number to pass to the button event function when activated\n\
(x, y) The lower left coordinate of the button\n\
(width, height) The button width and height\n\
(initial) The string to display initially\n\
(length) The maximum input length\n\
[tooltip=] The button's tooltip";

static char Method_GetStringWidth_doc[] =
"(text, font = 'normal') - Return the width in pixels of the given string\n\
(font) The font size: 'normal' (default), 'small' or 'tiny'.";

static char Method_Text_doc[] =
"(text, font = 'normal') - Draw text onscreen\n\n\
(text) The text to draw\n\
(font) The font size: 'normal' (default), 'small' or 'tiny'.\n\n\
This function returns the width of the drawn string.";

static char Method_PupMenu_doc[] =
"(string, maxrow = None) - Display a pop-up menu at the screen.\n\
The contents of the pop-up are specified through the 'string' argument,\n\
like with Draw.Menu.\n\
'maxrow' is an optional int to control how many rows the pop-up should have.\n\
Options are followed by a format code and separated\n\
by the '|' (pipe) character.\n\
Valid format codes are\n\
	%t - The option should be used as the title\n\
	%xN - The option should set the integer N in the button value.\n\n\
Ex: Draw.PupMenu('OK?%t|QUIT BLENDER') # should be familiar ...";

static char Method_PupIntInput_doc[] =
"(text, default, min, max) - Display an int pop-up input.\n\
(text) - text string to display on the button;\n\
(default, min, max) - the default, min and max int values for the button;\n\
Return the user input value or None on user exit";

static char Method_PupFloatInput_doc[] =
"(text, default, min, max, clickStep, floatLen) - Display a float pop-up input.\n\
(text) - text string to display on the button;\n\
(default, min, max) - the default, min and max float values for the button;\n\
(clickStep) - float increment/decrement for each click on the button arrows;\n\
(floatLen) - an integer defining the precision (number of decimal places) of \n\
the float value show.\n\
Return the user input value or None on user exit";

static char Method_PupStrInput_doc[] =
"(text, default, max = 20) - Display a float pop-up input.\n\
(text) - text string to display on the button;\n\
(default) - the initial string to display (truncated to 'max' chars);\n\
(max = 20) - The maximum number of chars the user can input;\n\
Return the user input value or None on user exit";

static char Method_Exit_doc[] = "() - Exit the windowing interface";

/*
* here we engage in some macro trickery to define the PyMethodDef table
*/

#define _MethodDef(func, prefix) \
	{#func, prefix##_##func, METH_VARARGS, prefix##_##func##_doc}

/* So that _MethodDef(delete, Scene) expands to:
 * {"delete", Scene_delete, METH_VARARGS, Scene_delete_doc} */

#undef MethodDef
#define MethodDef(func) _MethodDef(func, Method)

static struct PyMethodDef Draw_methods[] = {
	MethodDef (Create),
	MethodDef (Button),
	MethodDef (Toggle),
	MethodDef (Menu),
	MethodDef (Slider),
	MethodDef (Scrollbar),
	MethodDef (Number),
	MethodDef (String),
	MethodDef (GetStringWidth),
	MethodDef (Text),
	MethodDef (PupMenu),
	MethodDef (PupIntInput),
	MethodDef (PupFloatInput),
	MethodDef (PupStrInput),
	MethodDef (Exit),
	MethodDef (Redraw),
	MethodDef (Draw),
	MethodDef (Register),
  {"PushButton", Method_Button, METH_VARARGS, Method_Button_doc},
	{NULL, NULL,0,NULL}
};

PyTypeObject Button_Type = {
	PyObject_HEAD_INIT (NULL) 0,	/*ob_size */
	"Button",			/*tp_name */
	sizeof (Button),		/*tp_basicsize */
	0,				/*tp_itemsize */
	(destructor) Button_dealloc,	/*tp_dealloc */
	(printfunc) 0,		/*tp_print */
	(getattrfunc) Button_getattr,	/*tp_getattr */
	(setattrfunc) Button_setattr,	/*tp_setattr */
	(cmpfunc) 0,			/*tp_cmp */
	(reprfunc) Button_repr,	/*tp_repr */
};


static void Button_dealloc (PyObject *self)
{
	Button *but = (Button *) self;

	if (but->type == 3) MEM_freeN (but->val.asstr);

	PyObject_DEL (self);
}

static PyObject *Button_getattr (PyObject *self, char *name)
{
	Button *but = (Button *) self;

	if (strcmp (name, "val") == 0) {
		if (but->type == 1)
			return Py_BuildValue ("i", but->val.asint);
		else if (but->type == 2)
			return Py_BuildValue ("f", but->val.asfloat);
		else if (but->type == 3)
			return Py_BuildValue ("s", but->val.asstr);
	}

	PyErr_SetString (PyExc_AttributeError, name);
	return NULL;
}

static int Button_setattr (PyObject *self, char *name, PyObject *v)
{
	Button *but = (Button *) self;

	if (strcmp (name, "val") == 0) {
		if (but->type == 1)
			PyArg_Parse (v, "i", &but->val.asint);
		else if (but->type == 2)
			PyArg_Parse (v, "f", &but->val.asfloat);
		else if (but->type == 3) {
			char *newstr;
			PyArg_Parse (v, "s", &newstr);

			/* if the length of the new string is the same as */
			/* the old one, just copy, else delete and realloc. */
			if (but->slen == strlen (newstr)) {
				BLI_strncpy (but->val.asstr, newstr, but->slen);
			}
			else {
				MEM_freeN (but->val.asstr);
				but->slen = strlen (newstr);
				but->val.asstr = MEM_mallocN (but->slen + 1, "button setattr");
				strcpy (but->val.asstr, newstr);
			}
		}
	}
	else {
		PyErr_SetString (PyExc_AttributeError, name);
		return -1;
	}

	return 0;
}

static PyObject *Button_repr (PyObject * self)
{
	return PyObject_Repr (Button_getattr (self, "val"));
}

static Button *newbutton (void)
{
	Button *but = (Button *) PyObject_NEW (Button, &Button_Type);

	return but;
}

/* GUI interface routines */

static void exit_pydraw (SpaceScript *sc, short err)
{
	Script *script = NULL;

	if (!sc || !sc->script) return;

	script = sc->script;

	if (err) {
		PyErr_Print ();
		script->flags = 0;	/* mark script struct for deletion */
		error ("Python script error: check console");
		scrarea_queue_redraw (sc->area);
	}

	Py_XDECREF ((PyObject *) script->py_draw);
	Py_XDECREF ((PyObject *) script->py_event);
	Py_XDECREF ((PyObject *) script->py_button);

	script->py_draw = script->py_event = script->py_button = NULL;
}

static void exec_callback (SpaceScript *sc, PyObject *callback, PyObject *args)
{
	PyObject *result = PyObject_CallObject (callback, args);

	if (result == NULL && sc->script) {/* errors in the script */

		if (sc->script->lastspace == SPACE_TEXT) {/*if it can be an ALT+P script */
			Text *text = G.main->text.first;

			while (text) {/* find it and free its compiled code */

				if (!strcmp (text->id.name + 2, sc->script->id.name + 2)) {
					BPY_free_compiled_text (text);
					break;
				}

				text = text->id.next;
			}
		}
		exit_pydraw (sc, 1);
	}

	Py_XDECREF (result);
	Py_DECREF (args);
}

/* BPY_spacescript_do_pywin_draw, the static spacescript_do_pywin_buttons and
 * BPY_spacescript_do_pywin_event are the three functions responsible for
 * calling the draw, buttons and event callbacks registered with Draw.Register
 * (see Method_Register below).  They are called (only the two BPY_ ones)
 * from blender/src/drawscript.c */

void BPY_spacescript_do_pywin_draw (SpaceScript *sc)
{
	uiBlock *block;
	char butblock[20];
	Script *script = sc->script;

	sprintf (butblock, "win %d", curarea->win);
	block = uiNewBlock (&curarea->uiblocks, butblock, UI_EMBOSSX,
		UI_HELV, curarea->win);

	if (script->py_draw) {
		glPushAttrib (GL_ALL_ATTRIB_BITS);
		exec_callback (sc, script->py_draw, Py_BuildValue ("()"));
		glPopAttrib ();
	}
	else {
		glClearColor (0.4375, 0.4375, 0.4375, 0.0);
		glClear (GL_COLOR_BUFFER_BIT);
	}

	uiDrawBlock (block);

	curarea->win_swap = WIN_BACK_OK;
}

static void spacescript_do_pywin_buttons (SpaceScript *sc, unsigned short event)
{
	if (sc->script->py_button)
		exec_callback (sc, sc->script->py_button, Py_BuildValue ("(i)", event));
}

void BPY_spacescript_do_pywin_event (SpaceScript *sc, unsigned short event,
	short val)
{
	static int menu_hack = 0;

	/* about menu_hack above: when a menu returns after an entry is chosen,
	 * two events are generated, the second one with val = 4.  We don't want
	 * this second one to be passed to Python, because it can be confused with
	 * some event with same number defined by the script.
	 * What we do is set menu_hack to 1 if a button event occurs.
	 * Then if the next one is also a button event, w/ val = 4, we discard it. */

	if (event != UI_BUT_EVENT || !val) menu_hack = 0;

	if (event == QKEY && G.qual & (LR_ALTKEY | LR_CTRLKEY)) {
		/* finish script: user pressed ALT+Q or CONTROL+Q */
		Script *script = sc->script;

		exit_pydraw (sc, 0);

		script->flags &= ~SCRIPT_GUI;	/* we're done with this script */

		return;
	}

	if (val) {
		if (uiDoBlocks (&curarea->uiblocks, event) != UI_NOTHING) event = 0;

		if (event == UI_BUT_EVENT) {

			if (menu_hack && (val == 4)) { /* "false" event? */
				menu_hack = 0; /* if so, discard it and clear menu_hack */
			}
			else {
				menu_hack = 1;
				spacescript_do_pywin_buttons (sc, val);
			}

		}
	}

	if (sc->script->py_event)
		exec_callback (sc, sc->script->py_event, Py_BuildValue("(ii)", event, val));
}

static PyObject *Method_Exit (PyObject *self, PyObject *args)
{
	SpaceScript *sc;
	Script *script;

	/* if users call Draw.Exit when we are already out of the SPACE_SCRIPT, we
	 * simply return, for compatibility */
	if (curarea->spacetype == SPACE_SCRIPT)
		sc = curarea->spacedata.first;
	else
		return EXPP_incr_ret (Py_None);

	if (!PyArg_ParseTuple (args, ""))
		return EXPP_ReturnPyObjError (PyExc_AttributeError,
			"expected empty argument list");

	exit_pydraw (sc, 0);

	script = sc->script;

	/* remove our lock to the current namespace */
	script->flags &= ~SCRIPT_GUI;

	return EXPP_incr_ret (Py_None);
}

/* Method_Register (Draw.Register) registers callbacks for drawing, events
 * and gui button events, so a script can continue executing after the
 * interpreter reached its end and returned control to Blender.  Everytime
 * the SPACE_SCRIPT window with this script is redrawn, the registered
 * callbacks are executed. */
static PyObject *Method_Register (PyObject *self, PyObject *args)
{
	PyObject *newdrawc = NULL, *neweventc = NULL, *newbuttonc = NULL;
	SpaceScript *sc;
	Script *script;
	int startspace = 0;

	if (!PyArg_ParseTuple (args, "O|OO", &newdrawc, &neweventc, &newbuttonc))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected one or three PyObjects");

	if (!PyCallable_Check (newdrawc))
		newdrawc = NULL;
	if (!PyCallable_Check (neweventc))
		neweventc = NULL;
	if (!PyCallable_Check (newbuttonc))
		newbuttonc = NULL;

	if (!(newdrawc || neweventc || newbuttonc))
		return EXPP_incr_ret (Py_None);

	startspace = curarea->spacetype;

	/* first make sure the current area is of type SPACE_SCRIPT */
	if (startspace != SPACE_SCRIPT)
		newspace (curarea, SPACE_SCRIPT);

	sc = curarea->spacedata.first;

	/* this is a little confusing: we need to know which script is being executed
	 * now, so we can preserve its namespace from being deleted.
	 * There are two possibilities:
	 * a) One new script was created and the interpreter still hasn't returned
	 * from executing it.
	 * b) Any number of scripts were executed but left registered callbacks and
	 * so were not deleted yet. */

	/* To find out if we're dealing with a) or b), we start with the last
	 * created one: */
	script = G.main->script.last;

	if (!script) {
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
			"Draw.Register: couldn't get pointer to script struct");
	}

	/* if the flag SCRIPT_RUNNING is set, this script is case a): */
	if (!(script->flags & SCRIPT_RUNNING)) {
		script = sc->script;
	}
	/* otherwise it's case b) and the script we want is here: */
	else
		sc->script = script;

	/* Now we have the right script and can set a lock so its namespace can't be
	 * deleted for as long as we need it */
	script->flags |= SCRIPT_GUI;

	/* save the last space so we can go back to it upon finishing */
	if (!script->lastspace)
		script->lastspace = startspace;

	/* clean the old callbacks */
	exit_pydraw (sc, 0);

	/* prepare the new ones and insert them */
	Py_XINCREF (newdrawc);
	Py_XINCREF (neweventc);
	Py_XINCREF (newbuttonc);

	script->py_draw = newdrawc;
	script->py_event = neweventc;
	script->py_button = newbuttonc;

	scrarea_queue_redraw (sc->area);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Method_Redraw (PyObject *self, PyObject *args)
{
	int after = 0;

	if (!PyArg_ParseTuple (args, "|i", &after))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected int argument (or nothing)");

	/* XXX shouldn't we redraw all spacescript wins with this script on ? */
	if (after)
		addafterqueue (curarea->win, REDRAW, 1);
	else
		scrarea_queue_winredraw (curarea);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Method_Draw (PyObject * self, PyObject * args)
{
	/*@ If forced drawing is disable queue a redraw event instead */
	if (EXPP_disable_force_draw) {
		scrarea_queue_winredraw (curarea);
		return EXPP_incr_ret (Py_None);
	}

	if (!PyArg_ParseTuple (args, ""))
		return EXPP_ReturnPyObjError (PyExc_AttributeError,
			"expected empty argument list");

	scrarea_do_windraw (curarea);

	screen_swapbuffers ();

	return EXPP_incr_ret (Py_None);
}

static PyObject *Method_Create (PyObject *self, PyObject *args)
{
	Button *but;
	PyObject *in;

	if (!PyArg_ParseTuple (args, "O", &in))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected PyObject argument");

	but = newbutton ();
	if (PyFloat_Check (in))	{
		but->type = 2;
		but->val.asfloat = PyFloat_AsDouble (in);
	}
	else if (PyInt_Check (in)) {
		but->type = 1;
		but->val.asint = PyInt_AsLong (in);
	}
	else if (PyString_Check (in)) {
		char *newstr = PyString_AsString (in);

		but->type = 3;
		but->slen = strlen (newstr);
		but->val.asstr = MEM_mallocN (but->slen + 1, "button string");

		strcpy (but->val.asstr, newstr);
	}

	return (PyObject *) but;
}

static uiBlock *Get_uiBlock (void)
{
	char butblock[32];

	sprintf (butblock, "win %d", curarea->win);

	return uiGetBlock (butblock, curarea);
}

static PyObject *Method_Button (PyObject *self, PyObject *args)
{
	uiBlock *block;
	char *name, *tip = NULL;
	int event;
	int x, y, w, h;

	if (!PyArg_ParseTuple (args, "siiiii|s", &name, &event,
			&x, &y, &w, &h, &tip))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
		"expected a string, five ints and optionally another string as arguments");

	block = Get_uiBlock ();

	if (block)
		uiDefBut (block, BUT, event, name, x, y, w, h, 0, 0, 0, 0, 0, tip);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Method_Menu (PyObject *self, PyObject *args)
{
	uiBlock *block;
	char *name, *tip = NULL;
	int event, def;
	int x, y, w, h;
	Button *but;

	if (!PyArg_ParseTuple (args, "siiiiii|s", &name, &event,
			 &x, &y, &w, &h, &def, &tip))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected a string, six ints and optionally another string as arguments");

	but = newbutton ();
	but->type = 1;
	but->val.asint = def;

	block = Get_uiBlock ();
	if (block)
		uiDefButI (block, MENU, event, name, x, y, w, h,
			&but->val.asint, 0, 0, 0, 0, tip);

	return (PyObject *) but;
}

static PyObject *Method_Toggle (PyObject *self, PyObject *args)
{
	uiBlock *block;
	char *name, *tip = NULL;
	int event;
	int x, y, w, h, def;
	Button *but;

	if (!PyArg_ParseTuple (args, "siiiiii|s", &name, &event,
			 &x, &y, &w, &h, &def, &tip))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected a string, six ints and optionally another string as arguments");

	but = newbutton ();
	but->type = 1;
	but->val.asint = def;

	block = Get_uiBlock ();
	if (block)
		uiDefButI (block, TOG, event, name, x, y, w, h,
			&but->val.asint, 0, 0, 0, 0, tip);

	return (PyObject *) but;
}

/*@DO NOT TOUCH THIS FUNCTION !
	 Redrawing a slider inside its own callback routine is actually forbidden
	 with the current toolkit architecture (button routines are not reentrant).
	 But it works anyway.
	 XXX This is condemned to be dinosource in future - it's a hack.
	 */

static void	py_slider_update (void *butv, void *data2_unused)
{
	uiBut *but = butv;

	EXPP_disable_force_draw = 1;
	/*@
		 Disable forced drawing, otherwise the button object which
		 is still being used might be deleted 
	 */

	/*@  
		 spacetext_do_pywin_buttons(curarea->spacedata.first, but->retval); */

	g_window_redrawn = 0;
	curarea->win_swap = WIN_BACK_OK;
	/* removed global uiFrontBuf (contact ton when this goes wrong here) */
	spacescript_do_pywin_buttons (curarea->spacedata.first,
				uiButGetRetVal (but));

	if (!g_window_redrawn) { /*@ if Redraw already called */
		disable_where_script (1);
		M_Window_Redraw (0, Py_BuildValue ("(i)", SPACE_VIEW3D));
		disable_where_script (0);
	}

	EXPP_disable_force_draw = 0;
}

static PyObject *Method_Slider (PyObject *self, PyObject *args)
{
	uiBlock *block;
	char *name, *tip = NULL;
	int event;
	int x, y, w, h, realtime = 1;
	Button *but;
	PyObject *mino, *maxo, *inio;

	if (!PyArg_ParseTuple (args, "siiiiiOOO|is", &name, &event,
			&x, &y, &w, &h, &inio, &mino, &maxo, &realtime, &tip))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected a string, five ints, three PyObjects\n\
			and optionally another int and string as arguments");

	but = newbutton ();

	if (PyFloat_Check (inio)) {
		float ini, min, max;

		ini = PyFloat_AsDouble (inio);
		min = PyFloat_AsDouble (mino);
		max = PyFloat_AsDouble (maxo);

		but->type = 2;
		but->val.asfloat = ini;

		block = Get_uiBlock ();
		if (block) {
			uiBut *ubut;
			ubut = uiDefButF (block, NUMSLI, event, name, x, y, w, h,
				&but->val.asfloat, min, max, 0, 0, tip);
			if (realtime)
				uiButSetFunc (ubut, py_slider_update, ubut, NULL);
		}
	}
	else {
		int ini, min, max;

		ini = PyInt_AsLong (inio);
		min = PyInt_AsLong (mino);
		max = PyInt_AsLong (maxo);

		but->type = 1;
		but->val.asint = ini;

		block = Get_uiBlock ();
		if (block) {
			uiBut *ubut;
			ubut = uiDefButI (block, NUMSLI, event, name, x, y, w, h,
				&but->val.asint, min, max, 0, 0, tip);
			if (realtime)
				uiButSetFunc (ubut, py_slider_update, ubut, NULL);
		}
	}
	return (PyObject *) but;
}

static PyObject *Method_Scrollbar (PyObject *self, PyObject *args)
{
	char *tip = NULL;
	uiBlock *block;
	int event;
	int x, y, w, h, realtime = 1;
	Button *but;
	PyObject *mino, *maxo, *inio;
	float ini, min, max;

	if (!PyArg_ParseTuple (args, "iiiiiOOO|is", &event, &x, &y, &w, &h,
			 &inio, &mino, &maxo, &realtime, &tip))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected five ints, three PyObjects and optionally\n\
			another int and string as arguments");

	if (!PyNumber_Check (inio) || !PyNumber_Check (inio)
			|| !PyNumber_Check (inio))
		return EXPP_ReturnPyObjError (PyExc_AttributeError,
			"expected numbers for initial, min, and max");

	but = newbutton ();

	if (PyFloat_Check (inio))
		but->type = 2;
	else
		but->type = 1;

	ini = PyFloat_AsDouble (inio);
	min = PyFloat_AsDouble (mino);
	max = PyFloat_AsDouble (maxo);

	if (but->type == 2) {
		but->val.asfloat = ini;
		block = Get_uiBlock ();
		if (block) {
			uiBut *ubut;
			ubut = uiDefButF (block, SCROLL, event, "", x, y, w, h,
				&but->val.asfloat, min, max, 0, 0, tip);
			if (realtime)
				uiButSetFunc (ubut, py_slider_update, ubut, NULL);
		}
	}
	else {
		but->val.asint = ini;
		block = Get_uiBlock ();
		if (block) {
			uiBut *ubut;
			ubut = uiDefButI (block, SCROLL, event, "", x, y, w, h,
				&but->val.asint, min, max, 0, 0, tip);
			if (realtime)
				uiButSetFunc (ubut, py_slider_update, ubut, NULL);
		}
	}

	return (PyObject *) but;
}

static PyObject *Method_Number (PyObject *self, PyObject *args)
{
	uiBlock *block;
	char *name, *tip = NULL;
	int event;
	int x, y, w, h;
	Button *but;
	PyObject *mino, *maxo, *inio;

	if (!PyArg_ParseTuple (args, "siiiiiOOO|s", &name, &event,
			&x, &y, &w, &h, &inio, &mino, &maxo, &tip))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected a string, five ints, three PyObjects and\n\
			optionally another string as arguments");

	but = newbutton ();

	if (PyFloat_Check (inio)) {
		float ini, min, max;

		ini = PyFloat_AsDouble (inio);
		min = PyFloat_AsDouble (mino);
		max = PyFloat_AsDouble (maxo);

		but->type = 2;
		but->val.asfloat = ini;

		block = Get_uiBlock ();
		if (block)
			uiDefButF (block, NUM, event, name, x, y, w, h,
				&but->val.asfloat, min, max, 0, 0, tip);
	}
	else {
		int ini, min, max;

		ini = PyInt_AsLong (inio);
		min = PyInt_AsLong (mino);
		max = PyInt_AsLong (maxo);

		but->type = 1;
		but->val.asint = ini;

		block = Get_uiBlock ();
		if (block)
			uiDefButI (block, NUM, event, name, x, y, w, h,
				&but->val.asint, min, max, 0, 0, tip);
	}

	return (PyObject *) but;
}

static PyObject *Method_String (PyObject *self, PyObject *args)
{
	uiBlock *block;
	char *name, *tip = NULL, *newstr;
	int event;
	int x, y, w, h, len;
	Button *but;

	if (!PyArg_ParseTuple (args, "siiiiisi|s", &name, &event,
			&x, &y, &w, &h, &newstr, &len, &tip))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected a string, five ints, a string, an int and\n\
			optionally another string as arguments");

	but = newbutton ();
	but->type = 3;
	but->slen = len;
	but->val.asstr = MEM_mallocN (len + 1, "button string");

	strncpy (but->val.asstr, newstr, len);
	but->val.asstr[len] = 0;

	block = Get_uiBlock ();
	if (block)
		uiDefBut (block, TEX, event, name, x, y, w, h,
			but->val.asstr, 0, len, 0, 0, tip);

	return (PyObject *) but;
}

static PyObject *Method_GetStringWidth (PyObject *self, PyObject *args)
{
	char *text;
	char *font_str = "normal";
	struct BMF_Font *font;
	PyObject *width;

	if (!PyArg_ParseTuple (args, "s|s", &text, &font_str))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
					"expected one or two string arguments");

	if (!strcmp (font_str, "normal"))
		font = (&G)->font;
	else if (!strcmp (font_str, "small"))
		font = (&G)->fonts;
	else if (!strcmp (font_str, "tiny"))
		font = (&G)->fontss;
	else
		return EXPP_ReturnPyObjError (PyExc_AttributeError,
					"\"font\" must be: 'normal' (default), 'small' or 'tiny'.");

	width = PyInt_FromLong (BMF_GetStringWidth (font, text));

	if (!width)
		return EXPP_ReturnPyObjError (PyExc_MemoryError, "couldn't create PyInt");

	return width;
}

static PyObject *Method_Text (PyObject *self, PyObject *args)
{
	char *text;
	char *font_str = NULL;
	struct BMF_Font *font;

	if (!PyArg_ParseTuple (args, "s|s", &text, &font_str))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected one or two string arguments");

	if (!font_str)
		font = (&G)->font;
	else if (!strcmp (font_str, "normal"))
		font = (&G)->font;
	else if (!strcmp (font_str, "small"))
		font = (&G)->fonts;
	else if (!strcmp (font_str, "tiny"))
		font = (&G)->fontss;
	else
		return EXPP_ReturnPyObjError (PyExc_AttributeError,
			"\"font\" must be: 'normal' (default), 'small' or 'tiny'.");

	BMF_DrawString (font, text);

	return PyInt_FromLong (BMF_GetStringWidth (font, text));
}

static PyObject *Method_PupMenu (PyObject *self, PyObject *args)
{
	char *text;
	int maxrow = -1;
	PyObject *ret;

	if (!PyArg_ParseTuple (args, "s|i", &text, &maxrow))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected a string and optionally an int as arguments");

	if (maxrow >= 0)
		ret = PyInt_FromLong (pupmenu_col (text, maxrow));
	else
		ret = PyInt_FromLong (pupmenu (text));

	if (ret) return ret;

	return EXPP_ReturnPyObjError (PyExc_MemoryError, "couldn't create a PyInt");
}

static PyObject *Method_PupIntInput (PyObject *self, PyObject *args)
{
	char *text = NULL;
	int min = 0, max = 1;
	short var = 0;
	PyObject *ret = NULL;
	
	if (!PyArg_ParseTuple (args, "s|hii", &text, &var, &min, &max))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected 1 string and 3 int arguments");
	
	if (button(&var, min, max, text) == 0) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	ret = PyInt_FromLong (var);
	if (ret) return ret;
		
	return EXPP_ReturnPyObjError (PyExc_MemoryError, "couldn't create a PyInt");
}

static PyObject *Method_PupFloatInput (PyObject *self, PyObject *args)
{
	char *text = NULL;
	float min = 0, max = 1, var = 0, a1 = 10, a2 = 2;
	PyObject *ret = NULL;

	if (!PyArg_ParseTuple (args, "s|fffff", &text, &var, &min, &max, &a1, &a2))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected 1 string and 5 float arguments");

	if(fbutton(&var, min, max, a1, a2, text)==0) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	ret = PyFloat_FromDouble (var);
	if (ret) return ret;

	return EXPP_ReturnPyObjError (PyExc_MemoryError, "couldn't create a PyFloat");
}

static PyObject *Method_PupStrInput (PyObject *self, PyObject *args)
{
	char *text = NULL, *textMsg = NULL;
	char tmp[101];
	char max = 20;
	PyObject *ret = NULL;

	if (!PyArg_ParseTuple (args, "ss|b", &textMsg, &text, &max))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected 2 strings and 1 int");

	if ((max <= 0) || (max > 100))
		return EXPP_ReturnPyObjError (PyExc_AttributeError,
			"max string length value must be in the range [1, 100].");

	/* copying the text string handles both cases:
	 * max < strlen(text) (by truncating) and
	 * max > strlen(text) (by expanding to strlen(tmp)) */
	BLI_strncpy(tmp, text, max);

	if (sbutton (tmp, 0, max, textMsg) == 0) {
		Py_INCREF (Py_None);
		return Py_None;
	}

	ret = Py_BuildValue ("s", tmp);

	if (ret) return ret;
	
	return EXPP_ReturnPyObjError(PyExc_MemoryError, "couldn't create a PyString");
}


PyObject *Draw_Init (void)
{
	PyObject *submodule, *dict;

	Button_Type.ob_type = &PyType_Type;

	submodule = Py_InitModule3 ("Blender.Draw", Draw_methods, Draw_doc);

	dict = PyModule_GetDict (submodule);

#define EXPP_ADDCONST(x) \
	PyDict_SetItemString(dict, #x, PyInt_FromLong(x))

	/* So, for example:
	 * EXPP_ADDCONST(LEFTMOUSE) becomes
	 * PyDict_SetItemString(dict, "LEFTMOUSE", PyInt_FromLong(LEFTMOUSE)) 
	 */

	EXPP_ADDCONST (LEFTMOUSE);
	EXPP_ADDCONST (MIDDLEMOUSE);
	EXPP_ADDCONST (RIGHTMOUSE);
	EXPP_ADDCONST (MOUSEX);
	EXPP_ADDCONST (MOUSEY);
	EXPP_ADDCONST (TIMER0);
	EXPP_ADDCONST (TIMER1);
	EXPP_ADDCONST (TIMER2);
	EXPP_ADDCONST (TIMER3);
	EXPP_ADDCONST (KEYBD);
	EXPP_ADDCONST (RAWKEYBD);
	EXPP_ADDCONST (REDRAW);
	EXPP_ADDCONST (INPUTCHANGE);
	EXPP_ADDCONST (QFULL);
	EXPP_ADDCONST (WINFREEZE);
	EXPP_ADDCONST (WINTHAW);
	EXPP_ADDCONST (WINCLOSE);
	EXPP_ADDCONST (WINQUIT);
#ifndef IRISGL
	EXPP_ADDCONST (Q_FIRSTTIME);
#endif
	EXPP_ADDCONST (AKEY);
	EXPP_ADDCONST (BKEY);
	EXPP_ADDCONST (CKEY);
	EXPP_ADDCONST (DKEY);
	EXPP_ADDCONST (EKEY);
	EXPP_ADDCONST (FKEY);
	EXPP_ADDCONST (GKEY);
	EXPP_ADDCONST (HKEY);
	EXPP_ADDCONST (IKEY);
	EXPP_ADDCONST (JKEY);
	EXPP_ADDCONST (KKEY);
	EXPP_ADDCONST (LKEY);
	EXPP_ADDCONST (MKEY);
	EXPP_ADDCONST (NKEY);
	EXPP_ADDCONST (OKEY);
	EXPP_ADDCONST (PKEY);
	EXPP_ADDCONST (QKEY);
	EXPP_ADDCONST (RKEY);
	EXPP_ADDCONST (SKEY);
	EXPP_ADDCONST (TKEY);
	EXPP_ADDCONST (UKEY);
	EXPP_ADDCONST (VKEY);
	EXPP_ADDCONST (WKEY);
	EXPP_ADDCONST (XKEY);
	EXPP_ADDCONST (YKEY);
	EXPP_ADDCONST (ZKEY);
	EXPP_ADDCONST (ZEROKEY);
	EXPP_ADDCONST (ONEKEY);
	EXPP_ADDCONST (TWOKEY);
	EXPP_ADDCONST (THREEKEY);
	EXPP_ADDCONST (FOURKEY);
	EXPP_ADDCONST (FIVEKEY);
	EXPP_ADDCONST (SIXKEY);
	EXPP_ADDCONST (SEVENKEY);
	EXPP_ADDCONST (EIGHTKEY);
	EXPP_ADDCONST (NINEKEY);
	EXPP_ADDCONST (CAPSLOCKKEY);
	EXPP_ADDCONST (LEFTCTRLKEY);
	EXPP_ADDCONST (LEFTALTKEY);
	EXPP_ADDCONST (RIGHTALTKEY);
	EXPP_ADDCONST (RIGHTCTRLKEY);
	EXPP_ADDCONST (RIGHTSHIFTKEY);
	EXPP_ADDCONST (LEFTSHIFTKEY);
	EXPP_ADDCONST (ESCKEY);
	EXPP_ADDCONST (TABKEY);
	EXPP_ADDCONST (RETKEY);
	EXPP_ADDCONST (SPACEKEY);
	EXPP_ADDCONST (LINEFEEDKEY);
	EXPP_ADDCONST (BACKSPACEKEY);
	EXPP_ADDCONST (DELKEY);
	EXPP_ADDCONST (SEMICOLONKEY);
	EXPP_ADDCONST (PERIODKEY);
	EXPP_ADDCONST (COMMAKEY);
	EXPP_ADDCONST (QUOTEKEY);
	EXPP_ADDCONST (ACCENTGRAVEKEY);
	EXPP_ADDCONST (MINUSKEY);
	EXPP_ADDCONST (SLASHKEY);
	EXPP_ADDCONST (BACKSLASHKEY);
	EXPP_ADDCONST (EQUALKEY);
	EXPP_ADDCONST (LEFTBRACKETKEY);
	EXPP_ADDCONST (RIGHTBRACKETKEY);
	EXPP_ADDCONST (LEFTARROWKEY);
	EXPP_ADDCONST (DOWNARROWKEY);
	EXPP_ADDCONST (RIGHTARROWKEY);
	EXPP_ADDCONST (UPARROWKEY);
	EXPP_ADDCONST (PAD2);
	EXPP_ADDCONST (PAD4);
	EXPP_ADDCONST (PAD6);
	EXPP_ADDCONST (PAD8);
	EXPP_ADDCONST (PAD1);
	EXPP_ADDCONST (PAD3);
	EXPP_ADDCONST (PAD5);
	EXPP_ADDCONST (PAD7);
	EXPP_ADDCONST (PAD9);
	EXPP_ADDCONST (PADPERIOD);
	EXPP_ADDCONST (PADSLASHKEY);
	EXPP_ADDCONST (PADASTERKEY);
	EXPP_ADDCONST (PAD0);
	EXPP_ADDCONST (PADMINUS);
	EXPP_ADDCONST (PADENTER);
	EXPP_ADDCONST (PADPLUSKEY);
	EXPP_ADDCONST (F1KEY);
	EXPP_ADDCONST (F2KEY);
	EXPP_ADDCONST (F3KEY);
	EXPP_ADDCONST (F4KEY);
	EXPP_ADDCONST (F5KEY);
	EXPP_ADDCONST (F6KEY);
	EXPP_ADDCONST (F7KEY);
	EXPP_ADDCONST (F8KEY);
	EXPP_ADDCONST (F9KEY);
	EXPP_ADDCONST (F10KEY);
	EXPP_ADDCONST (F11KEY);
	EXPP_ADDCONST (F12KEY);
	EXPP_ADDCONST (PAUSEKEY);
	EXPP_ADDCONST (INSERTKEY);
	EXPP_ADDCONST (HOMEKEY);
	EXPP_ADDCONST (PAGEUPKEY);
	EXPP_ADDCONST (PAGEDOWNKEY);
	EXPP_ADDCONST (ENDKEY);

	return submodule;
}
