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
 * Contributor(s): Willian P. Germano
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

/* The code in Draw.[ch] and BGL.[ch] comes from opy_draw.c in the old
 * bpython/intern dir, with minor modifications to suit the current
 * implementation.  Important original comments are marked with an @ symbol. */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "MEM_guardedalloc.h"

#include "BMF_Api.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_text_types.h"

#include "BKE_global.h"

#include "BIF_gl.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_interface.h"
#include "BIF_mywindow.h"

#include "interface.h"
#include "mydevice.h"  /*@ for all the event constants */

#include "Python.h"

#include "gen_utils.h"
#include "modules.h"

/*@ hack to flag that window redraw has happened inside slider callback: */
int EXPP_disable_force_draw = 0;

/* From Window.h, used here by py_slider_update() */
PyObject *M_Window_Redraw(PyObject *self, PyObject *args);

/* This one was an extern in BPY_main.h, but only opy_draw.c was using it */
int g_window_redrawn;

static char Draw_doc[] =
"Module Blender.Draw ... XXX improve this";

static void exit_pydraw (SpaceText *st);
static uiBlock *Get_uiBlock (void);
void initDraw (void);

/* Button Object */

typedef struct _Button {
	PyObject_VAR_HEAD

	int type; /*@ 1 == int, 2 == float, 3 == string */
	int slen; /*@ length of string (if type == 3) */
	union {
		int asint;
		float asfloat;
		char *asstr;
	} val;
} Button;

static void Button_dealloc(PyObject *self);
static PyObject *Button_getattr(PyObject *self, char *name);
static int Button_setattr(PyObject *self,  char *name, PyObject *v);
static PyObject *Button_repr(PyObject *self);

PyTypeObject Button_Type =
{
	PyObject_HEAD_INIT(NULL)
	0,								              /*ob_size*/
	"Button",						            /*tp_name*/
	sizeof(Button),					        /*tp_basicsize*/
	0,								              /*tp_itemsize*/
	(destructor) Button_dealloc,	/*tp_dealloc*/
	(printfunc)  0,					        /*tp_print*/
	(getattrfunc) Button_getattr,	/*tp_getattr*/
	(setattrfunc) Button_setattr,	/*tp_setattr*/
	(cmpfunc)  0,				          	/*tp_cmp*/
	(reprfunc)  Button_repr,		  /*tp_repr*/
};

static Button *newbutton (void);

/* GUI interface routines */

static void exit_pydraw(SpaceText *st);
static void exec_callback(SpaceText *st, PyObject *callback, PyObject *args);
void BPY_spacetext_do_pywin_draw(SpaceText *st);
static void spacetext_do_pywin_buttons(SpaceText *st, unsigned short event);
void BPY_spacetext_do_pywin_event(SpaceText *st, unsigned short event, short val);
int BPY_spacetext_is_pywin(SpaceText *st);

static char Method_Exit_doc[] = 
"() - Exit the windowing interface";

static PyObject *Method_Exit (PyObject *self, PyObject *args);

static char Method_Register_doc[] = 
"(draw, event, button) - Register callbacks for windowing\n\n\
(draw) A function to draw the screen, taking no arguments\n\
(event) A function to handle events, taking 2 arguments (evt, val)\n\
	(evt) The event number\n\
	(val) The value modifier (for key and mouse press/release)\n\
(button) A function to handle button events, taking 1 argument (evt)\n\
	(evt) The button number\n\n\
A None object can be passed if a callback is unused.";

static PyObject *Method_Register (PyObject *self, PyObject *args);

static char Method_Redraw_doc[] =
"([after]) - Queue a redraw event\n\n\
[after=0] Determines whether the redraw is processed before\n\
or after other input events.\n\n\
Redraw events are buffered so that regardless of how many events\n\
are queued the window only receives one redraw event.";

static PyObject *Method_Redraw (PyObject *self,  PyObject *args);

static char Method_Draw_doc[] =
"() - Force an immediate redraw\n\n\
Forced redraws are not buffered, in other words the window is redrawn\n\
exactly once for everytime this function is called.";

static PyObject *Method_Draw (PyObject *self,  PyObject *args);

static char Method_Create_doc[] =
"(value) - Create a default Button object\n\n\
(value) - The value to store in the button\n\n\
Valid values are ints, floats, and strings";

static PyObject *Method_Create (PyObject *self,  PyObject *args);

static uiBlock *Get_uiBlock(void);

static char Method_Button_doc[] =
"(name, event, x, y, width, height, [tooltip]) - Create a new Button \
(push) button\n\n\
(name) A string to display on the button\n\
(event) The event number to pass to the button event function when activated\n\
(x, y) The lower left coordinate of the button\n\
(width, height) The button width and height\n\
[tooltip=""] The button's tooltip";

static PyObject *Method_Button (PyObject *self,  PyObject *args);

static char Method_Menu_doc[] =
"(name, event, x, y, width, height, default, [tooltip]) - Create a new Menu \
button\n\n\
(name) A string to display on the button\n\
(event) The event number to pass to the button event function when activated\n\
(x, y) The lower left coordinate of the button\n\
(width, height) The button width and height\n\
(default) The number of the option to be selected by default\n\
[tooltip=""] The button's tooltip\n\n\
The menu options are specified through the name of the\n\
button. Options are followed by a format code and seperated\n\
by the '|' (pipe) character.\n\
Valid format codes are\n\
	%t - The option should be used as the title\n\
	%xN - The option should set the integer N in the button value.";

static PyObject *Method_Menu (PyObject *self,  PyObject *args);

static char Method_Toggle_doc[] =
"(name, event, x, y, width, height, default, [tooltip]) - Create a new Toggle \
button\n\n\
(name) A string to display on the button\n\
(event) The event number to pass to the button event function when activated\n\
(x, y) The lower left coordinate of the button\n\
(width, height) The button width and height\n\
(default) An integer (0 or 1) specifying the default state\n\
[tooltip=""] The button's tooltip";

static PyObject *Method_Toggle (PyObject *self,  PyObject *args);
static void py_slider_update(void *butv, void *data2_unused);

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
[tooltip=""] The button's tooltip";

static PyObject *Method_Slider (PyObject *self,  PyObject *args);

static char Method_Scrollbar_doc[] =
"(event, x, y, width, height, initial, min, max, [update, tooltip]) - Create a \
new Scrollbar\n\n\
(event) The event number to pass to the button event function when activated\n\
(x, y) The lower left coordinate of the button\n\
(width, height) The button width and height\n\
(initial, min, max) Three values (int or float) specifying the initial and limit values.\n\
[update=1] A value controlling whether the slider will emit events as it is edited.\n\
			A non-zero value (default) enables the events. A zero value supresses them.\n\
[tooltip=""] The button's tooltip";

static PyObject *Method_Scrollbar (PyObject *self,  PyObject *args);

static char Method_Number_doc[] =
"(name, event, x, y, width, height, initial, min, max, [tooltip]) - Create a \
new Number button\n\n\
(name) A string to display on the button\n\
(event) The event number to pass to the button event function when activated\n\
(x, y) The lower left coordinate of the button\n\
(width, height) The button width and height\n\
(initial, min, max) Three values (int or float) specifying the initial and \
limit values.\n\
[tooltip=""] The button's tooltip";

static PyObject *Method_Number (PyObject *self,  PyObject *args);

static char Method_String_doc[] =
"(name, event, x, y, width, height, initial, length, [tooltip]) - Create a \
new String button\n\n\
(name) A string to display on the button\n\
(event) The event number to pass to the button event function when activated\n\
(x, y) The lower left coordinate of the button\n\
(width, height) The button width and height\n\
(initial) The string to display initially\n\
(length) The maximum input length\n\
[tooltip=""] The button's tooltip";

static PyObject *Method_String (PyObject *self,  PyObject *args);

static char Method_Text_doc[] =
"(text) - Draw text onscreen\n\n\
(text) The text to draw\n";

static PyObject *Method_Text (PyObject *self, PyObject *args);

#define _MethodDef(func, prefix) \
	{#func, prefix##_##func, METH_VARARGS, prefix##_##func##_doc}

/* So that _MethodDef(delete, Scene) expands to:
 * {"delete", Scene_delete, METH_VARARGS, Scene_delete_doc} */

#undef MethodDef
#define MethodDef(func) _MethodDef(func, Method)

static struct PyMethodDef Draw_methods[] = {
	MethodDef(Create),
	MethodDef(Button),
	MethodDef(Toggle),
	MethodDef(Menu),
	MethodDef(Slider),
	MethodDef(Scrollbar),
	MethodDef(Number),
	MethodDef(String),

	MethodDef(Text),

	MethodDef(Exit),
	MethodDef(Redraw),
	MethodDef(Draw),
	MethodDef(Register),

	{NULL, NULL}
};

PyObject *M_Draw_Init (void); 

