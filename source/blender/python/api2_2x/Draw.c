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
 * Contributor(s): Willian P. Germano, Campbell Barton, Ken Hughes
 *
 * ***** END GPL LICENSE BLOCK *****
*/

/* This file is the Blender.Draw part of opy_draw.c, from the old
 * bpython/intern dir, with minor changes to adapt it to the new Python
 * implementation.	Non-trivial original comments are marked with an
 * @ symbol at their beginning. */

#include "Draw.h" /*This must come first*/

#include "BLI_blenlib.h"
#include "MEM_guardedalloc.h"
#include "BMF_Api.h"
#include "DNA_screen_types.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_object.h"
#include "BKE_main.h"
#include "BKE_utildefines.h"
#include "BIF_gl.h"
#include "BIF_mywindow.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_interface.h"
#include "BIF_toolbox.h"
#include "DNA_space_types.h"		/* script struct */
#include "Image.h"              /* for accessing Blender.Image objects */
#include "IMB_imbuf_types.h"    /* for the IB_rect define */
#include "interface.h"
#include "mydevice.h"		/*@ for all the event constants */
#include "gen_utils.h"
#include "Window.h"
#include "../BPY_extern.h"

/* used so we can get G.scene->r.cfra for getting the
current image frame, some images change frame if they are a sequence */
#include "DNA_scene_types.h"

/* these delimit the free range for button events */
#define EXPP_BUTTON_EVENTS_OFFSET 1001
#define EXPP_BUTTON_EVENTS_MIN 0
#define EXPP_BUTTON_EVENTS_MAX 15382 /* 16384 - 1 - OFFSET */

#define ButtonObject_Check(v) ((v)->ob_type == &Button_Type)

#define UI_METHOD_ERRORCHECK \
	if (check_button_event(&event) == -1)\
		return EXPP_ReturnPyObjError( PyExc_AttributeError,\
			"button event argument must be in the range [0, 16382]");\
	if (callback && !PyCallable_Check(callback))\
		return EXPP_ReturnPyObjError( PyExc_ValueError,\
			"callback is not a python function");\

/* pointer to main dictionary defined in Blender.c */
extern PyObject *g_blenderdict;

/*@ hack to flag that window redraw has happened inside slider callback: */
int EXPP_disable_force_draw = 0;

/* forward declarations for internal functions */
static void Button_dealloc( PyObject * self );
static PyObject *Button_getattr( PyObject * self, char *name );
static PyObject *Button_repr( PyObject * self );
static PyObject *Button_richcmpr(PyObject *objectA, PyObject *objectB, int comparison_type);
static int Button_setattr( PyObject * self, char *name, PyObject * v );

static Button *newbutton( void );

/* GUI interface routines */

static void exit_pydraw( SpaceScript * sc, short error );
static void exec_callback( SpaceScript * sc, PyObject * callback,
			   PyObject * args );
static void spacescript_do_pywin_buttons( SpaceScript * sc,
					  unsigned short event );

static PyObject *Method_Exit( PyObject * self );
static PyObject *Method_Register( PyObject * self, PyObject * args );
static PyObject *Method_Redraw( PyObject * self, PyObject * args );
static PyObject *Method_Draw( PyObject * self );
static PyObject *Method_Create( PyObject * self, PyObject * args );
static PyObject *Method_UIBlock( PyObject * self, PyObject * args );

static PyObject *Method_Button( PyObject * self, PyObject * args );
static PyObject *Method_Menu( PyObject * self, PyObject * args );
static PyObject *Method_Toggle( PyObject * self, PyObject * args );
static PyObject *Method_Slider( PyObject * self, PyObject * args );
static PyObject *Method_Scrollbar( PyObject * self, PyObject * args );
static PyObject *Method_ColorPicker( PyObject * self, PyObject * args );
static PyObject *Method_Normal( PyObject * self, PyObject * args );
static PyObject *Method_Number( PyObject * self, PyObject * args );
static PyObject *Method_String( PyObject * self, PyObject * args );
static PyObject *Method_GetStringWidth( PyObject * self, PyObject * args );
static PyObject *Method_Text( PyObject * self, PyObject * args );
static PyObject *Method_Label( PyObject * self, PyObject * args );
/* by Campbell: */
static PyObject *Method_PupMenu( PyObject * self, PyObject * args );
static PyObject *Method_PupTreeMenu( PyObject * self, PyObject * args );
static PyObject *Method_PupIntInput( PyObject * self, PyObject * args );
static PyObject *Method_PupFloatInput( PyObject * self, PyObject * args );
static PyObject *Method_PupStrInput( PyObject * self, PyObject * args );
static PyObject *Method_BeginAlign( PyObject * self, PyObject * args  );
static PyObject *Method_EndAlign( PyObject * self, PyObject * args  );
/* next by Jonathan Merritt (lancelet): */
static PyObject *Method_Image( PyObject * self, PyObject * args);
/* CLEVER NUMBUT */
static PyObject *Method_PupBlock( PyObject * self, PyObject * args );

static uiBlock *Get_uiBlock( void );

static void py_slider_update( void *butv, void *data2_unused );

/* hack to get 1 block for the UIBlock, only ever 1 at a time */
static uiBlock *uiblock=NULL;

static char Draw_doc[] = "The Blender.Draw submodule";

static char Method_UIBlock_doc[] = "(drawfunc, x,y) - Popup dialog where buttons can be drawn (expemental)";

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

static char Method_BeginAlign_doc[] =
	"Buttons after this function will draw aligned (button layout only)";

static char Method_EndAlign_doc[] =
	"Use after BeginAlign() to stop aligning the buttons (button layout only).";

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

static char Method_ColorPicker_doc[] = 
	"(event, x, y, width, height, initial, [tooltip]) - Create a new Button \
Color picker button\n\n\
(event) The event number to pass to the button event function when the color changes\n\
(x, y) The lower left coordinate of the button\n\
(width, height) The button width and height\n\
(initial) 3-Float tuple of the color (values between 0 and 1)\
[tooltip=] The button's tooltip";

static char Method_Normal_doc[] = 
	"(event, x, y, width, height, initial, [tooltip]) - Create a new Button \
Normal button (a sphere that you can roll to change the normal)\n\n\
(event) The event number to pass to the button event function when the color changes\n\
(x, y) The lower left coordinate of the button\n\
(width, height) The button width and height - non square will gave odd results\n\
(initial) 3-Float tuple of the normal vector (values between -1 and 1)\
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
(font) The font size: 'large','normal' (default), 'normalfix', 'small' or 'tiny'.";

static char Method_Text_doc[] =
	"(text, font = 'normal') - Draw text onscreen\n\n\
(text) The text to draw\n\
(font) The font size: 'large','normal' (default), 'normalfix', 'small' or 'tiny'.\n\n\
This function returns the width of the drawn string.";

static char Method_Label_doc[] =
	"(text, x, y) - Draw a text label onscreen\n\n\
(text) The text to draw\n\
(x, y) The lower left coordinate of the lable";

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

static char Method_PupTreeMenu_doc[] =
"each item in the menu list should be - (str, event), separator - None or submenu - (str, [...]).";

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

static char Method_Image_doc[] =
	"(image, x, y, zoomx = 1.0, zoomy = 1.0, [clipx, clipy, clipw, cliph])) \n\
    - Draw an image.\n\
(image) - Blender.Image to draw.\n\
(x, y) - floats specifying the location of the image.\n\
(zoomx, zoomy) - float zoom factors in horizontal and vertical directions.\n\
(clipx, clipy, clipw, cliph) - integers specifying a clipping rectangle within the original image.";

static char Method_PupStrInput_doc[] =
	"(text, default, max = 20) - Display a float pop-up input.\n\
(text) - text string to display on the button;\n\
(default) - the initial string to display (truncated to 'max' chars);\n\
(max = 20) - The maximum number of chars the user can input;\n\
Return the user input value or None on user exit";

static char Method_PupBlock_doc[] =
	"(title, sequence) - Display a pop-up block.\n\
(title) - The title of the block.\n\
(sequence) - A sequence defining what the block contains. \
The order of the list is the order of appearance, from top down.\n\
Possible format for sequence items:\n\
[value is an object created with Create]\n\
\ttext: Defines a label in the block\n\
\t(text, value, tooltip = ''): Defines a toggle button \n\
\t(text, value, min, max, tooltip = ''): Defines a num or string button \n\
\t\t\tdepending on the value.\n\
\t\tFor string, max is the maximum length of the text and min is unused.\n\
Return 1 if the pop-up is confirmed, 0 otherwise. \n\
Warning: On cancel, the value objects are brought back to there previous values, \
\texcept for string values which will still contain the modified values.\n";

static char Method_Exit_doc[] = "() - Exit the windowing interface";

/*This is needed for button callbacks.  Any button that uses a callback gets added to this list.
  On the C side of drawing begin, this list should be cleared.
  Each entry is a tuple of the form (button, callback py object)
*/
PyObject *M_Button_List = NULL;

static struct PyMethodDef Draw_methods[] = {
	{"Create", (PyCFunction)Method_Create, METH_VARARGS, Method_Create_doc},
	{"UIBlock", (PyCFunction)Method_UIBlock, METH_VARARGS, Method_UIBlock_doc},
	{"Button", (PyCFunction)Method_Button, METH_VARARGS, Method_Button_doc},
	{"Toggle", (PyCFunction)Method_Toggle, METH_VARARGS, Method_Toggle_doc},
	{"Menu", (PyCFunction)Method_Menu, METH_VARARGS, Method_Menu_doc},
	{"Slider", (PyCFunction)Method_Slider, METH_VARARGS, Method_Slider_doc},
	{"Scrollbar", (PyCFunction)Method_Scrollbar, METH_VARARGS, Method_Scrollbar_doc},
	{"ColorPicker", (PyCFunction)Method_ColorPicker, METH_VARARGS, Method_ColorPicker_doc},
	{"Normal", (PyCFunction)Method_Normal, METH_VARARGS, Method_Normal_doc},
	{"Number", (PyCFunction)Method_Number, METH_VARARGS, Method_Number_doc},
	{"String", (PyCFunction)Method_String, METH_VARARGS, Method_String_doc},
	{"GetStringWidth", (PyCFunction)Method_GetStringWidth, METH_VARARGS, Method_GetStringWidth_doc},
	{"Text", (PyCFunction)Method_Text, METH_VARARGS, Method_Text_doc},
	{"Label", (PyCFunction)Method_Label, METH_VARARGS, Method_Label_doc},
	{"PupMenu", (PyCFunction)Method_PupMenu, METH_VARARGS, Method_PupMenu_doc},
	{"PupTreeMenu", (PyCFunction)Method_PupTreeMenu, METH_VARARGS, Method_PupTreeMenu_doc},
	{"PupIntInput", (PyCFunction)Method_PupIntInput, METH_VARARGS, Method_PupIntInput_doc},
	{"PupFloatInput", (PyCFunction)Method_PupFloatInput, METH_VARARGS, Method_PupFloatInput_doc},
	{"PupStrInput", (PyCFunction)Method_PupStrInput, METH_VARARGS, Method_PupStrInput_doc},
	{"PupBlock", (PyCFunction)Method_PupBlock, METH_VARARGS, Method_PupBlock_doc},
	{"Image", (PyCFunction)Method_Image, METH_VARARGS, Method_Image_doc},
	{"Exit", (PyCFunction)Method_Exit, METH_NOARGS, Method_Exit_doc},
	{"Redraw", (PyCFunction)Method_Redraw, METH_VARARGS, Method_Redraw_doc},
	{"Draw", (PyCFunction)Method_Draw, METH_NOARGS, Method_Draw_doc},
	{"Register", (PyCFunction)Method_Register, METH_VARARGS, Method_Register_doc},
	{"PushButton", (PyCFunction)Method_Button, METH_VARARGS, Method_Button_doc},
	{"BeginAlign", (PyCFunction)Method_BeginAlign, METH_VARARGS, Method_BeginAlign_doc},
	{"EndAlign", (PyCFunction)Method_EndAlign, METH_VARARGS, Method_EndAlign_doc},
	{NULL, NULL, 0, NULL}
};

PyTypeObject Button_Type = {
	PyObject_HEAD_INIT( NULL ) 0,	/*ob_size */
	"Button",		/*tp_name */
	sizeof( Button ),	/*tp_basicsize */
	0,			/*tp_itemsize */
	( destructor ) Button_dealloc,	/*tp_dealloc */
	( printfunc ) 0,	/*tp_print */
	( getattrfunc ) Button_getattr,	/*tp_getattr */
	( setattrfunc ) Button_setattr,	/*tp_setattr */
	NULL,		/*tp_cmp */
	( reprfunc ) Button_repr,	/*tp_repr */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	(richcmpfunc)Button_richcmpr,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	NULL,           /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	NULL,         /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};

static void Button_dealloc( PyObject * self )
{
	Button *but = ( Button * ) self;

	if( but->type == BSTRING_TYPE ) {
		if( but->val.asstr )
			MEM_freeN( but->val.asstr );
	}

	PyObject_DEL( self );
}

static PyObject *Button_getattr( PyObject * self, char *name )
{
	Button *but = ( Button * ) self;
	
	if( strcmp( name, "val" ) == 0 ) {
		if( but->type == BINT_TYPE )
			return PyInt_FromLong( but->val.asint );
		else if( but->type == BFLOAT_TYPE )
			return PyFloat_FromDouble( but->val.asfloat );
		else if( but->type == BSTRING_TYPE )
			return PyString_FromString( but->val.asstr );
		else if( but->type == BVECTOR_TYPE )
			return Py_BuildValue( "fff", but->val.asvec[0], but->val.asvec[1], but->val.asvec[2] );
	}

	PyErr_SetString( PyExc_AttributeError, name );
	return NULL;
}

static int Button_setattr( PyObject * self, char *name, PyObject * v )
{
	Button *but = ( Button * ) self;

	if( strcmp( name, "val" ) == 0 ) {
		if( but->type == BINT_TYPE && PyNumber_Check(v) ) {
			PyObject *pyVal = PyNumber_Int( v );
			if (pyVal) {
				but->val.asint = (int)PyInt_AS_LONG( pyVal );
				Py_DECREF(pyVal);
				return 0;
			}
		}
		else if( but->type == BFLOAT_TYPE && PyNumber_Check(v) ) {
			PyObject *pyVal = PyNumber_Float( v );
			if (pyVal) {
				but->val.asfloat = (float)PyFloat_AS_DOUBLE( pyVal );
				Py_DECREF(pyVal);
				return 0;
			}
		}
		else if( but->type == BVECTOR_TYPE ) {
			if ( PyArg_ParseTuple( v, "fff", but->val.asvec, but->val.asvec+1, but->val.asvec+2 ) )
				return 0;
		}
		else if( but->type == BSTRING_TYPE && PyString_Check(v) ) {
			char *newstr = PyString_AsString(v);
			size_t newlen = strlen(newstr);
			
			if (newlen+1> UI_MAX_DRAW_STR)
				return EXPP_ReturnIntError( PyExc_ValueError, "Error: button string length exceeded max limit (399 chars).");

			/* if the length of the new string is the same as */
			/* the old one, just copy, else delete and realloc. */
			if( but->slen == newlen ) {
				BLI_strncpy( but->val.asstr, newstr,
					     but->slen + 1 );

				return 0;

			} else {
				MEM_freeN( but->val.asstr );
				but->slen = newlen;
				but->val.asstr =
					MEM_mallocN( but->slen + 1,
						     "button setattr" );
				BLI_strncpy( but->val.asstr, newstr,
					     but->slen + 1 );

				return 0;
			}
		}
	} else {
		/*
		 * Accessing the wrong attribute.
		 */
		return EXPP_ReturnIntError( PyExc_AttributeError, name );
	}

	/*
	 * Correct attribute but value is incompatible with current button value.
	 */
	return EXPP_ReturnIntError( PyExc_ValueError, "value incompatible with current button type" );
}

static PyObject *Button_repr( PyObject * self )
{
	return PyObject_Repr( Button_getattr( self, "val" ) );
}

static PyObject *Button_richcmpr(PyObject *objectA, PyObject *objectB, int comparison_type)
{
	PyObject *ret, *valA=NULL, *valB=NULL;
	if (ButtonObject_Check(objectA))
		objectA = valA = Button_getattr( objectA, "val" );
	if (ButtonObject_Check(objectB))
		objectB = valB = Button_getattr( objectB, "val" );
	ret = PyObject_RichCompare(objectA, objectB, comparison_type);
	Py_XDECREF(valA); /* Button_getattr created with 1 ref, we dont care about them now */
	Py_XDECREF(valB);
	return ret;
}


static Button *newbutton( void )
{
	Button *but = NULL;
	
	but = ( Button * ) PyObject_NEW( Button, &Button_Type );
	but->tooltip[0] = 0; /*NULL-terminate tooltip string*/
	but->tooltip[255] = 0; /*necassary to insure we always have a NULL-terminated string, as
	                         according to the docs strncpy doesn't do this for us.*/
	return but;
}

/* GUI interface routines */

static void exit_pydraw( SpaceScript * sc, short err )
{
	Script *script = NULL;

	if( !sc || !sc->script )
		return;

	script = sc->script;

	if( err ) {
		PyErr_Print(  );
		script->flags = 0;	/* mark script struct for deletion */
		SCRIPT_SET_NULL(script);
		script->scriptname[0] = '\0';
		script->scriptarg[0] = '\0';
		error_pyscript();
		scrarea_queue_redraw( sc->area );
	}

	BPy_Set_DrawButtonsList(sc->but_refs);
	BPy_Free_DrawButtonsList(); /*clear all temp button references*/
	sc->but_refs = NULL;
	
	Py_XDECREF( ( PyObject * ) script->py_draw );
	Py_XDECREF( ( PyObject * ) script->py_event );
	Py_XDECREF( ( PyObject * ) script->py_button );

	script->py_draw = script->py_event = script->py_button = NULL;
}

static void exec_callback( SpaceScript * sc, PyObject * callback,
			   PyObject * args )
{
	PyObject *result = PyObject_CallObject( callback, args );

	if( result == NULL && sc->script ) {	/* errors in the script */

		if( sc->script->lastspace == SPACE_TEXT ) {	/*if it can be an ALT+P script */
			Text *text = G.main->text.first;

			while( text ) {	/* find it and free its compiled code */

				if( !strcmp
				    ( text->id.name + 2,
				      sc->script->id.name + 2 ) ) {
					BPY_free_compiled_text( text );
					break;
				}

				text = text->id.next;
			}
		}
		exit_pydraw( sc, 1 );
	}

	Py_XDECREF( result );
	Py_DECREF( args );
}

/* BPY_spacescript_do_pywin_draw, the static spacescript_do_pywin_buttons and
 * BPY_spacescript_do_pywin_event are the three functions responsible for
 * calling the draw, buttons and event callbacks registered with Draw.Register
 * (see Method_Register below).  They are called (only the two BPY_ ones)
 * from blender/src/drawscript.c */

void BPY_spacescript_do_pywin_draw( SpaceScript * sc )
{
	uiBlock *block;
	char butblock[20];
	Script *script = sc->script;
	PyGILState_STATE gilstate = PyGILState_Ensure();

	sprintf( butblock, "win %d", curarea->win );
	block = uiNewBlock( &curarea->uiblocks, butblock, UI_EMBOSSX,
			    UI_HELV, curarea->win );

	if( script->py_draw ) {
		if (sc->but_refs) {
			BPy_Set_DrawButtonsList(sc->but_refs);
			BPy_Free_DrawButtonsList(); /*clear all temp button references*/
		}
		sc->but_refs = PyList_New(0);
		BPy_Set_DrawButtonsList(sc->but_refs);
		
		glPushAttrib( GL_ALL_ATTRIB_BITS );
		exec_callback( sc, script->py_draw, Py_BuildValue( "()" ) );
		glPopAttrib(  );
	} else {
		glClearColor( 0.4375, 0.4375, 0.4375, 0.0 );
		glClear( GL_COLOR_BUFFER_BIT );
	}

	uiDrawBlock( block );

	curarea->win_swap = WIN_BACK_OK;

	PyGILState_Release(gilstate);
}

static void spacescript_do_pywin_buttons( SpaceScript * sc,
					  unsigned short event )
{
	if( sc->script->py_button )
		exec_callback( sc, sc->script->py_button,
			       Py_BuildValue( "(i)", event ) );
}

void BPY_spacescript_do_pywin_event( SpaceScript * sc, unsigned short event,
	short val, char ascii )
{
	PyGILState_STATE gilstate = PyGILState_Ensure();

	if( event == QKEY && G.qual & ( LR_ALTKEY | LR_CTRLKEY ) ) {
		/* finish script: user pressed ALT+Q or CONTROL+Q */
		Script *script = sc->script;

		exit_pydraw( sc, 0 );

		script->flags &= ~SCRIPT_GUI;	/* we're done with this script */

		PyGILState_Release(gilstate);

		return;
	}

	if (val) {

		if (uiDoBlocks( &curarea->uiblocks, event, 1 ) != UI_NOTHING) event = 0;

		if (event == UI_BUT_EVENT) {
			/* check that event is in free range for script button events;
			 * read the comment before check_button_event() below to understand */
			if (val >= EXPP_BUTTON_EVENTS_OFFSET && val < 0x4000)
				spacescript_do_pywin_buttons(sc, val - EXPP_BUTTON_EVENTS_OFFSET);

			PyGILState_Release(gilstate);

			return;
		}
	}

	/* We use the "event" main module var, used by scriptlinks, to pass the ascii
	 * value to event callbacks (gui/event/button callbacks are not allowed
	 * inside scriptlinks, so this is ok) */
	if( sc->script->py_event ) {
		int pass_ascii = 0;
		if (ascii > 31 && ascii != 127) {
			pass_ascii = 1;
			EXPP_dict_set_item_str(g_blenderdict, "event",
					PyInt_FromLong((long)ascii));
		}
		exec_callback( sc, sc->script->py_event,
			Py_BuildValue( "(ii)", event, val ) );
		if (pass_ascii)
			EXPP_dict_set_item_str(g_blenderdict, "event",
					PyString_FromString(""));
	}

	PyGILState_Release(gilstate);
}

static void exec_but_callback(void *pyobj, void *data)
{
	PyObject *result;
	PyObject *pyvalue = NULL;
	uiBut *but = (uiBut *)data;
	PyObject *arg;
	PyObject *callback = (PyObject *)pyobj;
	
	double value = ui_get_but_val(but);
	
	if (callback==NULL || callback == Py_None)
		return;
	
	/* Button types support
	case MENU:	
	case TEX:
	case TOG:
	case NUMSLI:
	case NUM:
	case COL:
	case BUT_NORMAL:
	case BUT */
	switch (but->type) {
	case TEX:
		/*printf("TEX\n");*/
		pyvalue = PyString_FromString( (char *)but->poin );
		break;
	case NUM:
	case NUMSLI:
	case TOG:
	case MENU:
		if (but->pointype==FLO) {
			/*printf("FLO\n");*/
			pyvalue = PyFloat_FromDouble( (float)value );
		} else if (but->pointype==INT) {
			/*printf("INT\n");*/
			pyvalue = PyInt_FromLong( (int)value );
		} else if (but->pointype==SHO) {
			/*printf("SHO\n");*/
			pyvalue = PyInt_FromLong( (short)value );
		}	
		break;
	case COL:
	case BUT_NORMAL:
	{
		float vec[3];
		VECCOPY(vec, (float *)but->poin);
		pyvalue = Py_BuildValue("(fff)", vec[0], vec[1], vec[2]);
		break;
	}
	case BUT:
		pyvalue = Py_None;
		Py_INCREF(pyvalue);
		break;
	default:
		pyvalue = Py_None;
		Py_INCREF(pyvalue);
		printf("Error, no button type matched.");
	}
	
	arg = PyTuple_New( 2 );
	if (uiblock==NULL)
		PyTuple_SetItem( arg, 0, PyInt_FromLong(but->retval - EXPP_BUTTON_EVENTS_OFFSET) );
	else
		PyTuple_SetItem( arg, 0, PyInt_FromLong(but->retval) );
	
	PyTuple_SetItem( arg, 1, pyvalue );
	
	result = PyObject_CallObject( callback, arg );
	Py_DECREF(arg);
	
	if (!result) {
		Py_DECREF(pyvalue);
		PyErr_Print(  );
		error_pyscript(  );
	}
	Py_XDECREF( result );
}

/*note that this function populates the drawbutton ref lists.*/
static void set_pycallback(uiBut *ubut, PyObject *callback, Button *but)
{
	PyObject *tuple;
	if (!callback || !PyCallable_Check(callback)) {
		if (M_Button_List && but) {
			PyList_Append(M_Button_List, (PyObject*)but);
		}
		return;
	}
	
	if (M_Button_List) {
		if (but) tuple = PyTuple_New(2);
		else tuple = PyTuple_New(1);
		
		/*the tuple API mandates this*/
		Py_XINCREF(callback);
		Py_XINCREF(but); /*this checks for NULL*/
		
		PyTuple_SET_ITEM(tuple, 0, callback);
		if (but) PyTuple_SET_ITEM(tuple, 1, (PyObject*)but);
		
		PyList_Append(M_Button_List, tuple);
		Py_DECREF(tuple); /*we have to do this to aovid double references.*/
		
		uiButSetFunc(ubut, exec_but_callback, callback, ubut);
	}
}

void BPy_Set_DrawButtonsList(void *list)
{
	M_Button_List = list;
}

/*this MUST be called after doing UI stuff.*/
void BPy_Free_DrawButtonsList(void)
{
	/*Clear the list.*/
	if (M_Button_List) {
		PyGILState_STATE gilstate = {0};
		int py_is_on = Py_IsInitialized();

		if (py_is_on) gilstate = PyGILState_Ensure();

		PyList_SetSlice(M_Button_List, 0, PyList_Size(M_Button_List), NULL);
		Py_DECREF(M_Button_List);
		M_Button_List = NULL;

		if (py_is_on) PyGILState_Release(gilstate);
	}
}

static PyObject *Method_Exit( PyObject * self )
{
	SpaceScript *sc;
	Script *script;

	/* if users call Draw.Exit when we are already out of the SPACE_SCRIPT, we
	 * simply return, for compatibility */
	if( curarea->spacetype == SPACE_SCRIPT )
		sc = curarea->spacedata.first;
	else
		Py_RETURN_NONE;

	exit_pydraw( sc, 0 );

	script = sc->script;

	/* remove our lock to the current namespace */
	script->flags &= ~SCRIPT_GUI;
	script->scriptname[0] = '\0';
	script->scriptarg[0] = '\0';

	Py_RETURN_NONE;
}

/* Method_Register (Draw.Register) registers callbacks for drawing, events
 * and gui button events, so a script can continue executing after the
 * interpreter reached its end and returned control to Blender.  Everytime
 * the SPACE_SCRIPT window with this script is redrawn, the registered
 * callbacks are executed. */
static PyObject *Method_Register( PyObject * self, PyObject * args )
{
	PyObject *newdrawc = NULL, *neweventc = NULL, *newbuttonc = NULL;
	SpaceScript *sc;
	Script *script;
	int startspace = 0;

	if( !PyArg_ParseTuple
	    ( args, "O|OO", &newdrawc, &neweventc, &newbuttonc ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected one or three PyObjects" );

	if( !PyCallable_Check( newdrawc ) )
		newdrawc = NULL;
	if( !PyCallable_Check( neweventc ) )
		neweventc = NULL;
	if( !PyCallable_Check( newbuttonc ) )
		newbuttonc = NULL;

	if( !( newdrawc || neweventc || newbuttonc ) )
		Py_RETURN_NONE;

	startspace = curarea->spacetype;

	/* first make sure the current area is of type SPACE_SCRIPT */
	if( startspace != SPACE_SCRIPT )
		newspace( curarea, SPACE_SCRIPT );

	sc = curarea->spacedata.first;

	/* There are two kinds of scripts:
	 * a) those that simply run, finish and return control to Blender;
	 * b) those that do like 'a)' above but leave callbacks for drawing,
	 * events and button events, with this Method_Register (Draw.Register
	 * in Python).  These callbacks are called by scriptspaces (Scripts windows).
	 *
	 * We need to flag scripts that leave callbacks so their namespaces are
	 * not deleted when they 'finish' execution, because the callbacks will
	 * still need the namespace.
	 */

	/* Let's see if this is a new script */
	script = G.main->script.first;
	while (script) {
		if (script->flags & SCRIPT_RUNNING) break;
		script = script->id.next;
	}

	if( !script ) {
		/* not new, it's a left callback calling Register again */
 		script = sc->script;
		if( !script ) {
			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"Draw.Register can't be used inside script links" );
		}
	}
	else sc->script = script;

	/* Now we have the right script and can set a lock so its namespace can't be
	 * deleted for as long as we need it */
	script->flags |= SCRIPT_GUI;

	/* save the last space so we can go back to it upon finishing */
	if( !script->lastspace )
		script->lastspace = startspace;

	/* clean the old callbacks */
	exit_pydraw( sc, 0 );

	/* prepare the new ones and insert them */
	Py_XINCREF( newdrawc );
	Py_XINCREF( neweventc );
	Py_XINCREF( newbuttonc );

	script->py_draw = newdrawc;
	script->py_event = neweventc;
	script->py_button = newbuttonc;

	scrarea_queue_redraw( sc->area );

	Py_RETURN_NONE;
}

static PyObject *Method_Redraw( PyObject * self, PyObject * args )
{
	int after = 0;

	if( !PyArg_ParseTuple( args, "|i", &after ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected int argument (or nothing)" );

	if( after )
		addafterqueue( curarea->win, REDRAW, 1 );
	else
		scrarea_queue_winredraw( curarea );

	Py_RETURN_NONE;
}

static PyObject *Method_Draw( PyObject * self )
{
	/*@ If forced drawing is disable queue a redraw event instead */
	if( EXPP_disable_force_draw ) {
		scrarea_queue_winredraw( curarea );
		Py_RETURN_NONE;
	}

	scrarea_do_windraw( curarea );

	screen_swapbuffers(  );

	Py_RETURN_NONE;
}

static PyObject *Method_Create( PyObject * self, PyObject * args )
{
	Button *but = NULL;
	PyObject *val;
	char *newstr;

	but = newbutton();
	/* If this function dosnt sucseed this will need to be deallocated,
	 * make sure the type is NOT BSTRING_TYPE before deallocing -1 is ok.
	 * so we dont dealloc with an uninitialized value wich would be bad! */
	if ( PyArg_ParseTuple( args, "fff", but->val.asvec, but->val.asvec+1, but->val.asvec+2 ) ) {
		but->type = BVECTOR_TYPE;
	
	} else if ( PyArg_ParseTuple( args, "O!", &PyFloat_Type, &val ) ) {
		but->val.asfloat = (float)PyFloat_AS_DOUBLE(val);
		but->type = BFLOAT_TYPE;
	
	} else if ( PyArg_ParseTuple( args, "O!", &PyInt_Type, &val ) ) {
		but->val.asint = (int)PyInt_AS_LONG(val);
		but->type = BINT_TYPE;
	
	} else if ( PyArg_ParseTuple( args, "s#", &newstr, &but->slen ) ) {
		if (but->slen + 1 > UI_MAX_DRAW_STR) {
			but->type = -1;
			Py_DECREF((PyObject *)but); /* will remove */
			but = NULL;
			PyErr_SetString( PyExc_TypeError, "string is longer then 399 chars");
		} else {
			but->type = BSTRING_TYPE;
			but->val.asstr = MEM_mallocN( but->slen + 1, "button string" );
			BLI_strncpy( but->val.asstr, newstr, but->slen+1 );
		}
	
	} else {
		but->type = -1;
		Py_DECREF((PyObject *)but); /* will remove */
		but = NULL;
		PyErr_SetString( PyExc_TypeError, "expected string, float, int or 3-float tuple argument" );
	}
	
	if (but != NULL) {
		PyErr_Clear();
	}

	return (PyObject*) but;
}


static PyObject *Method_UIBlock( PyObject * self, PyObject * args )
{
	PyObject *val = NULL;
	PyObject *result = NULL;
	ListBase listb= {NULL, NULL};

	if ( !PyArg_ParseTuple( args, "O", &val ) || !PyCallable_Check( val ) ) 
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "expected 1 python function and 2 ints" );

	if (uiblock)
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
	      "cannot run more then 1 UIBlock at a time" );

	BPy_Set_DrawButtonsList(PyList_New(0));
	
	mywinset(G.curscreen->mainwin);
	uiblock= uiNewBlock(&listb, "numbuts", UI_EMBOSS, UI_HELV, G.curscreen->mainwin);
	
	uiBlockSetFlag(uiblock, UI_BLOCK_LOOP|UI_BLOCK_REDRAW);
	result = PyObject_CallObject( val, Py_BuildValue( "()" ) );
	
	if (!result) {
		PyErr_Print(  );
		error_pyscript(  );
	} else {
		/* copied from do_clever_numbuts in toolbox.c */
		
		/* Clear all events so tooltips work, this is not ideal and
		only needed because calls from the menu still have some events
		left over when do_clever_numbuts is called.
		Calls from keyshortcuts do not have this problem.*/
		ScrArea *sa;
		BWinEvent temp_bevt;
		for (sa= G.curscreen->areabase.first; sa; sa= sa->next) {
			if(sa->win) {
				while( bwin_qread( sa->win, &temp_bevt ) ) {}
			}
			if(sa->headwin) {
				while( bwin_qread( sa->headwin, &temp_bevt ) ) {}
			}
		}
		/* Done clearing events */
		
		uiBoundsBlock(uiblock, 5);
		uiDoBlocks(&listb, 0, 1);
	}
	uiFreeBlocks(&listb);
	uiblock = NULL;
	BPy_Free_DrawButtonsList(); /*clear all temp button references*/
	
	Py_XDECREF( result );
	Py_RETURN_NONE;
}

void Set_uiBlock(uiBlock *block)
{
	uiblock = block;
}

static uiBlock *Get_uiBlock( void )
{
	char butblock[32];
	/* Global, used now for UIBlock */
	if (uiblock) {
		return uiblock;
	}
	/* Local */
	sprintf( butblock, "win %d", curarea->win );

	return uiGetBlock( butblock, curarea );
}


/* We restrict the acceptable event numbers to a proper "free" range
 * according to other spaces in Blender.
 * winqread***space() (space events callbacks) use short for events
 * (called 'val' there) and we also translate by EXPP_BUTTON_EVENTS_OFFSET
 * to get rid of unwanted events (check BPY_do_pywin_events above for
 * explanation). This function takes care of that and proper checking: */
static int check_button_event(int *event) {
	if ((*event < EXPP_BUTTON_EVENTS_MIN) ||
			(*event > EXPP_BUTTON_EVENTS_MAX)) {
		return -1;
	}
	if (uiblock==NULL) /* For UIBlock we need non offset UI elements */
		*event += EXPP_BUTTON_EVENTS_OFFSET;
	return 0;
}

static PyObject *Method_BeginAlign( PyObject * self, PyObject * args )
{
	uiBlock *block = Get_uiBlock(  );
	
	if (block)
		uiBlockBeginAlign(block);
	
	Py_RETURN_NONE;
}

static PyObject *Method_EndAlign( PyObject * self, PyObject * args )
{
	uiBlock *block = Get_uiBlock(  );
	
	if (block)
		uiBlockEndAlign(block);
	
	Py_RETURN_NONE;
}

static PyObject *Method_Button( PyObject * self, PyObject * args )
{
	uiBlock *block;
	char *name, *tip = NULL;
	int event;
	int x, y, w, h;
	PyObject *callback=NULL;

	if( !PyArg_ParseTuple( args, "siiiii|sO", &name, &event,
			       &x, &y, &w, &h, &tip, &callback ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected a string, five ints and optionally string and callback arguments" );

	UI_METHOD_ERRORCHECK;

	block = Get_uiBlock(  );
	if( block ) {
		uiBut *ubut = uiDefBut( block, BUT, event, name, (short)x, (short)y, (short)w, (short)h, 0, 0, 0, 0, 0, tip );
		set_pycallback(ubut, callback, NULL);
	}
	Py_RETURN_NONE;
}

static PyObject *Method_Menu( PyObject * self, PyObject * args )
{
	uiBlock *block;
	char *name, *tip = NULL;
	int event, def;
	int x, y, w, h;
	Button *but;
	PyObject *callback=NULL;

	if( !PyArg_ParseTuple( args, "siiiiii|sO", &name, &event,
			       &x, &y, &w, &h, &def, &tip, &callback ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected a string, six ints and optionally string and callback arguments" );

	UI_METHOD_ERRORCHECK;
	
	but = newbutton(  );
	but->type = BINT_TYPE;
	but->val.asint = def;
	if (tip) strncpy(but->tooltip, tip, BPY_MAX_TOOLTIP);
	
	block = Get_uiBlock(  );
	if( block ) {
		uiBut *ubut = uiDefButI( block, MENU, event, name, (short)x, (short)y, (short)w, (short)h,
			   &but->val.asint, 0, 0, 0, 0, but->tooltip );
		set_pycallback(ubut, callback, but);
	}
	return ( PyObject * ) but;
}

static PyObject *Method_Toggle( PyObject * self, PyObject * args )
{
	uiBlock *block;
	char *name, *tip = NULL;
	int event;
	int x, y, w, h, def;
	Button *but;
	PyObject *callback=NULL;

	if( !PyArg_ParseTuple( args, "siiiiii|sO", &name, &event,
			       &x, &y, &w, &h, &def, &tip, &callback ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected a string, six ints and optionally string and callback arguments" );

	UI_METHOD_ERRORCHECK;
	
	but = newbutton(  );
	but->type = BINT_TYPE;
	but->val.asint = def;
	if (tip) strncpy(but->tooltip, tip, BPY_MAX_TOOLTIP);
	
	block = Get_uiBlock(  );
	if( block ) {
		uiBut *ubut = uiDefButI( block, TOG, event, name, (short)x, (short)y, (short)w, (short)h,
			   &but->val.asint, 0, 0, 0, 0, but->tooltip );
		set_pycallback(ubut, callback, but);
	}
	return ( PyObject * ) but;
}

/*@DO NOT TOUCH THIS FUNCTION !
	 Redrawing a slider inside its own callback routine is actually forbidden
	 with the current toolkit architecture (button routines are not reentrant).
	 But it works anyway.
	 XXX This is condemned to be dinosource in future - it's a hack.
	 */

static void py_slider_update( void *butv, void *data2_unused )
{
	uiBut *but = butv;
	PyObject *ref = Py_BuildValue( "(i)", SPACE_VIEW3D );
	PyObject *ret = NULL;

	EXPP_disable_force_draw = 1;
	/*@ Disable forced drawing, otherwise the button object which
	 * is still being used might be deleted */

	curarea->win_swap = WIN_BACK_OK;
	/* removed global uiFrontBuf (contact ton when this goes wrong here) */

	disable_where_script( 1 );

	spacescript_do_pywin_buttons( curarea->spacedata.first,
		(unsigned short)uiButGetRetVal( but ) -  EXPP_BUTTON_EVENTS_OFFSET );

	/* XXX useless right now, investigate better before a bcon 5 */
	ret = M_Window_Redraw( 0, ref );

	Py_XDECREF(ref);
	Py_XDECREF(ret);

	disable_where_script( 0 );

	EXPP_disable_force_draw = 0;
}

static PyObject *Method_Slider( PyObject * self, PyObject * args )
{
	uiBlock *block;
	char *name, *tip = NULL;
	int event;
	int x, y, w, h, realtime = 1;
	Button *but;
	PyObject *mino, *maxo, *inio;
	PyObject *callback=NULL;

	if( !PyArg_ParseTuple( args, "siiiiiOOO|isO", &name, &event,
			       &x, &y, &w, &h, &inio, &mino, &maxo, &realtime,
			       &tip, &callback ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected a string, five ints, three PyObjects\n\
			and optionally int, string and callback arguments" );

	if(realtime && uiblock)
		realtime = 0; /* realtime dosnt work with UIBlock */

	UI_METHOD_ERRORCHECK;
	
	but = newbutton(  );

	if( PyFloat_Check( inio ) ) {
		float ini, min, max;

		ini = (float)PyFloat_AsDouble( inio );
		min = (float)PyFloat_AsDouble( mino );
		max = (float)PyFloat_AsDouble( maxo );

		but->type = BFLOAT_TYPE;
		but->val.asfloat = ini;
		if (tip) strncpy(but->tooltip, tip, BPY_MAX_TOOLTIP);
		
		block = Get_uiBlock(  );
		if( block ) {
			uiBut *ubut;
			ubut = uiDefButF( block, NUMSLI, event, name, (short)x, (short)y, (short)w,
					  (short)h, &but->val.asfloat, min, max, 0, 0,
					  but->tooltip );
			if( realtime )
				uiButSetFunc( ubut, py_slider_update, ubut, NULL );
			else
				set_pycallback(ubut, callback, but);
		}
	} else {
		int ini, min, max;

		ini = PyInt_AsLong( inio );
		min = PyInt_AsLong( mino );
		max = PyInt_AsLong( maxo );

		but->type = BINT_TYPE;
		but->val.asint = ini;
		if (tip) strncpy(but->tooltip, tip, BPY_MAX_TOOLTIP);
		
		block = Get_uiBlock(  );
		if( block ) {
			uiBut *ubut;
			ubut = uiDefButI( block, NUMSLI, event, name, (short)x, (short)y, (short)w,
					  (short)h, &but->val.asint, (float)min, (float)max, 0, 0,
					  but->tooltip );
			if( realtime )
				uiButSetFunc( ubut, py_slider_update, ubut, NULL );
			else
				set_pycallback(ubut, callback, but);
		}
	}
	return ( PyObject * ) but;
}

static PyObject *Method_Scrollbar( PyObject * self, PyObject * args )
{
	char *tip = NULL;
	uiBlock *block;
	int event;
	int x, y, w, h, realtime = 1;
	Button *but;
	PyObject *mino, *maxo, *inio;
	float ini, min, max;
	uiBut *ubut;
	
	if( !PyArg_ParseTuple( args, "iiiiiOOO|isO", &event, &x, &y, &w, &h,
			       &inio, &mino, &maxo, &realtime, &tip ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
			"expected five ints, three PyObjects and optionally\n\
another int and string as arguments" );

	if( !PyNumber_Check( inio ) || !PyNumber_Check( inio )
	    || !PyNumber_Check( inio ) )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "expected numbers for initial, min, and max" );

	if (check_button_event(&event) == -1)
	return EXPP_ReturnPyObjError( PyExc_AttributeError,
		"button event argument must be in the range [0, 16382]");

	but = newbutton(  );
	if (tip) strncpy(but->tooltip, tip, BPY_MAX_TOOLTIP);
	
	if( PyFloat_Check( inio ) )
		but->type = BFLOAT_TYPE;
	else
		but->type = BINT_TYPE;

	ini = (float)PyFloat_AsDouble( inio );
	min = (float)PyFloat_AsDouble( mino );
	max = (float)PyFloat_AsDouble( maxo );
	
	block = Get_uiBlock(  );

	if( block ) {
		if( but->type == BFLOAT_TYPE ) {
			but->val.asfloat = ini;
			ubut = uiDefButF( block, SCROLL, event, "", (short)x, (short)y, (short)w, (short)h,
					  &but->val.asfloat, min, max, 0, 0, but->tooltip );
			if( realtime )
				uiButSetFunc( ubut, py_slider_update, ubut, NULL );
		} else {
			but->val.asint = (int)ini;
			ubut = uiDefButI( block, SCROLL, event, "", (short)x, (short)y, (short)w, (short)h,
					  &but->val.asint, min, max, 0, 0, but->tooltip );
			if( realtime )
				uiButSetFunc( ubut, py_slider_update, ubut, NULL );
		}
	}
	return ( PyObject * ) but;
}

static PyObject *Method_ColorPicker( PyObject * self, PyObject * args )
{
	char USAGE_ERROR[] = "expected a 3-float tuple of values between 0 and 1";
	Button *but;
	PyObject *inio;
	uiBlock *block;
	char *tip = NULL;
	float col[3];
	int event;
	short x, y, w, h;
	PyObject *callback=NULL;
	
	if( !PyArg_ParseTuple( args, "ihhhhO!|sO", &event,
			       &x, &y, &w, &h, &PyTuple_Type, &inio, &tip, &callback ) )
 		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected five ints, one tuple and optionally string and callback arguments" );
 
	UI_METHOD_ERRORCHECK;
 
	if ( !PyArg_ParseTuple( inio, "fff", col, col+1, col+2 ) )
		return EXPP_ReturnPyObjError( PyExc_ValueError, USAGE_ERROR);

	if	(	col[0] < 0 || col[0] > 1
		||	col[1] < 0 || col[1] > 1
		||	col[2] < 0 || col[2] > 1 )
		return EXPP_ReturnPyObjError( PyExc_ValueError, USAGE_ERROR);

	if ( EXPP_check_sequence_consistency( inio, &PyFloat_Type ) != 1 )
		return EXPP_ReturnPyObjError( PyExc_ValueError, USAGE_ERROR);
 
	but = newbutton();
 
	but->type = BVECTOR_TYPE;
	but->val.asvec[0] = col[0];
	but->val.asvec[1] = col[1];
	but->val.asvec[2] = col[2];
	if (tip) strncpy(but->tooltip, tip, BPY_MAX_TOOLTIP);
	
	block = Get_uiBlock(  );
	if( block ) {
		uiBut *ubut;
		ubut = uiDefButF( block, COL, event, "", x, y, w, h, but->val.asvec, 0, 0, 0, 0, but->tooltip);
		set_pycallback(ubut, callback, but);
	}

 	return ( PyObject * ) but;
}



static PyObject *Method_Normal( PyObject * self, PyObject * args )
{
	char USAGE_ERROR[] = "expected a 3-float tuple of values between -1 and 1";
	Button *but;
	PyObject *inio;
	uiBlock *block;
	char *tip = NULL;
	float nor[3];
	int event;
	short x, y, w, h;
	PyObject *callback=NULL;
	
	if( !PyArg_ParseTuple( args, "ihhhhO!|sO", &event,
			       &x, &y, &w, &h, &PyTuple_Type, &inio, &tip, &callback ) )
 		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected five ints, one tuple and optionally string and callback arguments" );
 
	UI_METHOD_ERRORCHECK;
 
	if ( !PyArg_ParseTuple( inio, "fff", nor, nor+1, nor+2 ) )
		return EXPP_ReturnPyObjError( PyExc_ValueError, USAGE_ERROR);

	if ( EXPP_check_sequence_consistency( inio, &PyFloat_Type ) != 1 )
		return EXPP_ReturnPyObjError( PyExc_ValueError, USAGE_ERROR);
 
	but = newbutton();
	if (tip) strncpy(but->tooltip, tip, BPY_MAX_TOOLTIP);
	
	but->type = BVECTOR_TYPE;
	but->val.asvec[0] = nor[0];
	but->val.asvec[1] = nor[1];
	but->val.asvec[2] = nor[2];
	
	block = Get_uiBlock(  );
	if( block ) {
		uiBut *ubut;
		ubut = uiDefButF( block, BUT_NORMAL, event, "", x, y, w, h, but->val.asvec, 0.0f, 1.0f, 0, 0, but->tooltip);
		set_pycallback(ubut, callback, but);
	}
	
 	return ( PyObject * ) but;
}

static PyObject *Method_Number( PyObject * self, PyObject * args )
{
	uiBlock *block;
	char *name, *tip = NULL;
	int event;
	int x, y, w, h;
	Button *but;
	PyObject *mino, *maxo, *inio;
	PyObject *callback=NULL;
	uiBut *ubut= NULL;
	
	if( !PyArg_ParseTuple( args, "siiiiiOOO|sO", &name, &event,
			       &x, &y, &w, &h, &inio, &mino, &maxo, &tip, &callback ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected a string, five ints, three PyObjects and\n\
			optionally string and callback arguments" );

	UI_METHOD_ERRORCHECK;

	but = newbutton(  );
	if (tip) strncpy(but->tooltip, tip, BPY_MAX_TOOLTIP);
	block = Get_uiBlock(  );
	
	if( PyFloat_Check( inio ) ) {
		float ini, min, max, range, precission=0;

		ini = (float)PyFloat_AsDouble( inio );
		min = (float)PyFloat_AsDouble( mino );
		max = (float)PyFloat_AsDouble( maxo );
		
		range= (float)fabs(max-min); /* Click step will be a 10th of the range. */
		if (!range) range= 1.0f; /* avoid any odd errors */
		
		/* set the precission to display*/
		if      (range>=1000.0f) precission=1.0f;
		else if (range>=100.0f) precission=2.0f;
		else if (range>=10.0f) precission=3.0f;
 		else precission=4.0f;
 			
		but->type = BFLOAT_TYPE;
		but->val.asfloat = ini;

		
		if( block )
			ubut= uiDefButF( block, NUM, event, name, (short)x, (short)y, (short)w, (short)h,
				   &but->val.asfloat, min, max, 10*range, precission, but->tooltip );
	} else {
		int ini, min, max;

		ini = PyInt_AsLong( inio );
		min = PyInt_AsLong( mino );
		max = PyInt_AsLong( maxo );

		but->type = BINT_TYPE;
		but->val.asint = ini;

		if( block )
			ubut= uiDefButI( block, NUM, event, name, (short)x, (short)y, (short)w, (short)h,
				   &but->val.asint, (float)min, (float)max, 0, 0, but->tooltip );
	}
	
	if (ubut) set_pycallback(ubut, callback, but);
	
	return ( PyObject * ) but;
}

static PyObject *Method_String( PyObject * self, PyObject * args )
{
	uiBlock *block;
	char *info_arg = NULL, *tip = NULL, *newstr = NULL;
	char *info_str = NULL, *info_str0 = " ";
	int event;
	int x, y, w, h, len, real_len = 0;
	Button *but;
	PyObject *callback=NULL;

	if( !PyArg_ParseTuple( args, "siiiiisi|sO", &info_arg, &event,
			&x, &y, &w, &h, &newstr, &len, &tip, &callback ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
			"expected a string, five ints, a string, an int and\n\
	optionally string and callback arguments" );

	UI_METHOD_ERRORCHECK;

	if (len > (UI_MAX_DRAW_STR - 1))
		return EXPP_ReturnPyObjError( PyExc_ValueError,
			"The maximum length of a string is 399, your value is too high.");

	real_len = strlen(newstr);
	if (real_len > len) real_len = len;
	
	but = newbutton(  );
	but->type = BSTRING_TYPE;
	but->slen = len;
	but->val.asstr = MEM_mallocN( len + 1, "pybutton str" );
	if (tip) strncpy(but->tooltip, tip, BPY_MAX_TOOLTIP);

	BLI_strncpy( but->val.asstr, newstr, len + 1); /* adds '\0' */
	but->val.asstr[real_len] = '\0';

	if (info_arg[0] == '\0') info_str = info_str0;
	else info_str = info_arg;

	block = Get_uiBlock(  );
	if( block ) {
		uiBut *ubut = uiDefBut( block, TEX, event, info_str, (short)x, (short)y, (short)w, (short)h,
			  but->val.asstr, 0, (float)len, 0, 0, but->tooltip );
		set_pycallback(ubut, callback, but);
	}
	return ( PyObject * ) but;
}

static PyObject *Method_GetStringWidth( PyObject * self, PyObject * args )
{
	char *text;
	char *font_str = "normal";
	struct BMF_Font *font;
	PyObject *width;

	if( !PyArg_ParseTuple( args, "s|s", &text, &font_str ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected one or two string arguments" );

	if( !strcmp( font_str, "normal" ) )
		font = ( &G )->font;
	else if( !strcmp( font_str, "normalfix" ) )
		font = BMF_GetFont(BMF_kScreen12);
	else if( !strcmp( font_str, "large" ) )
		font = BMF_GetFont(BMF_kScreen15);
	else if( !strcmp( font_str, "small" ) )
		font = ( &G )->fonts;
	else if( !strcmp( font_str, "tiny" ) )
		font = ( &G )->fontss;
	else
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "\"font\" must be: 'large','normal' (default), 'normalfix', 'small' or 'tiny'." );

	width = PyInt_FromLong( BMF_GetStringWidth( font, text ) );

	if( !width )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create PyInt" );

	return width;
}

static PyObject *Method_Text( PyObject * self, PyObject * args )
{
	char *text;
	char *font_str = NULL;
	struct BMF_Font *font;

	if( !PyArg_ParseTuple( args, "s|s", &text, &font_str ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected one or two string arguments" );

	if( !font_str )
		font = ( &G )->font;
	else if( !strcmp( font_str, "large" ) )
		font = BMF_GetFont(BMF_kScreen15);
	else if( !strcmp( font_str, "normalfix" ) )
		font = BMF_GetFont(BMF_kScreen12);
	else if( !strcmp( font_str, "normal" ) )
		font = ( &G )->font;
	else if( !strcmp( font_str, "small" ) )
		font = ( &G )->fonts;
	else if( !strcmp( font_str, "tiny" ) )
		font = ( &G )->fontss;
	else
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "\"font\" must be: 'large','normal' (default), 'normalfix', 'small' or 'tiny'." );

	BMF_DrawString( font, text );

	return PyInt_FromLong( BMF_GetStringWidth( font, text ) );
}

static PyObject *Method_Label( PyObject * self, PyObject * args )
{
	uiBlock *block;
	char *text;
	int x, y, w, h;

	if( !PyArg_ParseTuple( args, "siiii", &text, &x, &y, &w, &h ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
			"expected a string and four ints" );

	block = Get_uiBlock(  );
	uiDefBut(block, LABEL, 0, text, x, y, w, h, 0, 0, 0, 0, 0, "");
	
	Py_RETURN_NONE;
}


static PyObject *Method_PupMenu( PyObject * self, PyObject * args )
{
	char *text;
	int maxrow = -1;
	PyObject *ret;

	if( !PyArg_ParseTuple( args, "s|i", &text, &maxrow ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected a string and optionally an int as arguments" );

	if( maxrow >= 0 )
		ret = PyInt_FromLong( pupmenu_col( text, maxrow ) );
	else
		ret = PyInt_FromLong( pupmenu( text ) );

	if( ret )
		return ret;

	return EXPP_ReturnPyObjError( PyExc_MemoryError,
				      "couldn't create a PyInt" );
}

static int current_menu_ret;
static void toolbox_event(void *arg, int event)
{
	current_menu_ret = event;
}

static TBitem * menu_from_pylist( PyObject * current_menu, ListBase *storage )
{
	TBitem *tbarray, *tbitem;
	Link *link;
	PyObject *item, *submenu;
	int size, i;
	
	char *menutext;
	int event;
	
	size = PyList_Size( current_menu );
	
	link= MEM_callocN(sizeof(Link) + sizeof(TBitem)*(size+1), "python menu");
	
	if (link==NULL) {
		PyErr_SetString( PyExc_MemoryError, "Could not allocate enough memory for the menu" );
		BLI_freelistN(storage);
		return NULL;
	}
	
	BLI_addtail(storage, link);
	
	tbarray = tbitem = (TBitem *)(link+1);
	
	for (i=0; i<size; i++, tbitem++) {
		/* need to get these in */
		item = PyList_GET_ITEM( current_menu, i);
		
		if (item == Py_None) {
			tbitem->name = "SEPR";
		} else if( PyArg_ParseTuple( item, "si", &menutext, &event ) ) {
			tbitem->name = menutext;
			tbitem->retval = event;
			//current_menu_index
		} else if( PyArg_ParseTuple( item, "sO!", &menutext, &PyList_Type, &submenu ) ) {
			PyErr_Clear(); /* from PyArg_ParseTuple above */
			tbitem->name = menutext;
			tbitem->poin = menu_from_pylist(submenu, storage);
			if (tbitem->poin == NULL) {
				BLI_freelistN(storage);
				return NULL; /* error should be set */
			}
		} else {
			PyErr_Clear(); /* from PyArg_ParseTuple above */
			
			PyErr_SetString( PyExc_TypeError, "Expected a list of name,event tuples, None, or lists for submenus" );
			BLI_freelistN(storage);
			return NULL;
		}
	}
	tbitem->icon= -1;	/* end signal */
	tbitem->name= "";
	tbitem->retval= 0;
	tbitem->poin= toolbox_event;
	
	return tbarray;
}

static PyObject *Method_PupTreeMenu( PyObject * self, PyObject * args )
{
	PyObject * current_menu;
	ListBase storage = {NULL, NULL};
	TBitem *tb;
	
	if( !PyArg_ParseTuple( args, "O!", &PyList_Type, &current_menu ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
			"Expected a list" );
	
	mywinset(G.curscreen->mainwin); // we go to screenspace
	
	tb = menu_from_pylist(current_menu, &storage);
	
	if (!tb) { 
		/* Error is set */
		return NULL; 
	}
	
	current_menu_ret = -1;
	toolbox_generic(tb);
	
	/* free all dynamic entries... */
	BLI_freelistN(&storage);
	
	mywinset(curarea->win);
	return PyInt_FromLong( current_menu_ret ); /* current_menu_ret is set by toolbox_event callback */
}

static PyObject *Method_PupIntInput( PyObject * self, PyObject * args )
{
	char *text = NULL;
	int min = 0, max = 1;
	short var = 0;
	PyObject *ret = NULL;

	if( !PyArg_ParseTuple( args, "s|hii", &text, &var, &min, &max ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected 1 string and 3 int arguments" );

	if( button( &var, (short)min, (short)max, text ) == 0 ) {
		Py_INCREF( Py_None );
		return Py_None;
	}
	ret = PyInt_FromLong( var );
	if( ret )
		return ret;

	return EXPP_ReturnPyObjError( PyExc_MemoryError,
				      "couldn't create a PyInt" );
}

static PyObject *Method_PupFloatInput( PyObject * self, PyObject * args )
{
	char *text = NULL;
	float min = 0, max = 1, var = 0, a1 = 10, a2 = 2;
	PyObject *ret = NULL;

	if( !PyArg_ParseTuple
	    ( args, "s|fffff", &text, &var, &min, &max, &a1, &a2 ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected 1 string and 5 float arguments" );

	if( fbutton( &var, min, max, a1, a2, text ) == 0 ) {
		Py_INCREF( Py_None );
		return Py_None;
	}
	ret = PyFloat_FromDouble( var );
	if( ret )
		return ret;

	return EXPP_ReturnPyObjError( PyExc_MemoryError,
				      "couldn't create a PyFloat" );
}

static PyObject *Method_PupStrInput( PyObject * self, PyObject * args )
{
	char *text = NULL, *textMsg = NULL;
	char tmp[101];
	char max = 20;
	PyObject *ret = NULL;

	if( !PyArg_ParseTuple( args, "ss|b", &textMsg, &text, &max ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected 2 strings and 1 int" );

	if( ( max <= 0 ) || ( max > 100 ) )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "max string length value must be in the range [1, 100]." );

	/* copying the text string handles both cases:
	 * max < strlen(text) (by truncating) and
	 * max > strlen(text) (by expanding to strlen(tmp)) */
	BLI_strncpy( tmp, text, max + 1 );

	if( sbutton( tmp, 0, max, textMsg ) == 0 ) {
		Py_INCREF( Py_None );
		return Py_None;
	}

	ret = PyString_FromString( tmp );

	if( ret )
		return ret;

	return EXPP_ReturnPyObjError( PyExc_MemoryError,
				      "couldn't create a PyString" );
}

static PyObject *Method_PupBlock( PyObject * self, PyObject * args )
{
	PyObject *pyList, *pyItem;
	float min, max;
	int len, i;
	char *title;

	if (!PyArg_ParseTuple( args, "sO", &title, &pyList ) || !PySequence_Check( pyList ))
		return EXPP_ReturnPyObjError( PyExc_TypeError, "expected a string and a sequence" );


	len = PySequence_Length(pyList);

	if (len == 0)
		return EXPP_ReturnPyObjError( PyExc_ValueError, "expected a string and a non-empty sequence." );

	if (len > 120) /* LIMIT DEFINED IN toolbox.c	*/
		return EXPP_ReturnPyObjError( PyExc_ValueError, "sequence cannot have more than 120 elements" );

	for ( i=0 ; i<len ; i++ ) {
		PyObject *pyMin = NULL, *pyMax = NULL;
		PyObject *f1, *f2;
		Button *but = NULL;
		int tlen;
		char *text, *tip = NULL;

		pyItem = PySequence_GetItem( pyList, i );
		if (!pyItem)
			return NULL;

		if (PyString_Check( pyItem )) {
			tlen = -2;	/* single string for label, giving it a special len for later */
		}
		else if (PyTuple_Check( pyItem )) {
			/* tuple for other button, get the length for later */
			tlen = PyTuple_Size( pyItem );
		}
		else {
			/* Neither a string or a tuple, error */
			Py_DECREF( pyItem );
			return EXPP_ReturnPyObjError( PyExc_ValueError, "expected a string or a tuple containing 2 to 5 values." );
		}

		switch (tlen) {
		case -2:		/*	LABEL	*/
			text = PyString_AsString( pyItem );
			add_numbut(i, LABEL, text, 0, 0, NULL, NULL);
			break;
		case 2:		/*	TOGGLE	(no tooltip)	*/
		case 3:		/*	TOGGLE	*/
			if (!PyArg_ParseTuple( pyItem, "sO!|s", &text, &Button_Type, &but, &tip )) {
				Py_DECREF( pyItem );
				return EXPP_ReturnPyObjError( PyExc_ValueError, "expected a tuple containing a string, a Button object and optionally a string for toggles" );
			}

			if (but->type != BINT_TYPE) {
				Py_DECREF( pyItem );
				return EXPP_ReturnPyObjError( PyExc_ValueError, "Button object for toggles should hold an integer" );
			}

			add_numbut(i, TOG|INT, text, 0, 0, &but->val.asint, tip);
			break;
		case 4:		/*	TEX and NUM (no tooltip)	*/
		case 5:		/*	TEX and NUM	*/
			if (!PyArg_ParseTuple( pyItem, "sO!OO|s", &text, &Button_Type, &but, &pyMin, &pyMax, &tip )) {
				Py_DECREF( pyItem );
				return EXPP_ReturnPyObjError( PyExc_ValueError, "expected a tuple containing a string, a Button object, two numerical values and optionally a string for Text and Num buttons" );
			}

			f1 = PyNumber_Float(pyMin);
			f2 = PyNumber_Float(pyMax);

			if (!f1 || !f2) {
				Py_DECREF( pyItem );
				return EXPP_ReturnPyObjError( PyExc_ValueError, "expected a tuple containing a string, a Button object, two numerical values and optionally a string for Text and Num buttons" );
			}

			min = (float)PyFloat_AS_DOUBLE(f1);
			max = (float)PyFloat_AS_DOUBLE(f2);
			Py_DECREF( f1 );
			Py_DECREF( f2 );

			switch ( but->type ) {
			case BINT_TYPE:
				add_numbut(i, NUM|INT, text, min, max, &but->val.asint, tip);
				break;
			case BFLOAT_TYPE:
				add_numbut(i, NUM|FLO, text, min, max, &but->val.asfloat, tip);
				break;
			case BSTRING_TYPE:
				if (max+1>UI_MAX_DRAW_STR) {
					Py_DECREF( pyItem );
					return EXPP_ReturnPyObjError( PyExc_ValueError, "length of a string buttons must be less then 400" );
				}
				max = (float)floor(max);

				if (max > but->slen) {
					int old_len = but->slen;
					char *old_str = but->val.asstr;
					but->slen = (int)max;
					but->val.asstr = MEM_callocN( but->slen + 1, "button pupblock");
					BLI_strncpy( but->val.asstr, old_str, old_len + 1 );
					MEM_freeN(old_str);
				}

				add_numbut(i, TEX, text, 0.0f, max, but->val.asstr, tip);
			}

			break;
		default:
			Py_DECREF( pyItem );
			return EXPP_ReturnPyObjError( PyExc_ValueError, "expected a string or a tuple containing 2 to 5 values." );
		}
		Py_DECREF( pyItem );
	}

	if (do_clever_numbuts(title, len, REDRAW))
		return EXPP_incr_ret_True();
	else
		return EXPP_incr_ret_False();
}


/*****************************************************************************
 * Function:            Method_Image                                         *
 * Python equivalent:   Blender.Draw.Image                                   *
 *                                                                           *
 * @author Jonathan Merritt <j.merritt@pgrad.unimelb.edu.au>                 *
 ****************************************************************************/
static PyObject *Method_Image( PyObject * self, PyObject * args )
{
	PyObject *pyObjImage;
	BPy_Image *py_img;
	Image *image;
	ImBuf *ibuf;
	float originX, originY;
	float zoomX = 1.0, zoomY = 1.0;
	int clipX = 0, clipY = 0, clipW = -1, clipH = -1;
	/*GLfloat scissorBox[4];*/

	/* parse the arguments passed-in from Python */
	if( !PyArg_ParseTuple( args, "O!ff|ffiiii", &Image_Type, &pyObjImage, 
		&originX, &originY, &zoomX, &zoomY, 
		&clipX, &clipY, &clipW, &clipH ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
			"expected a Blender.Image and 2 floats, and " \
			"optionally 2 floats and 4 ints as arguments" );
	/* check that the zoom factors are valid */
	if( ( zoomX < 0.0 ) || ( zoomY < 0.0 ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
			"invalid zoom factors - they must be > 0.0" );
	if ((zoomX == 0.0 ) || ( zoomY == 0.0 )) {
		/* sometimes python doubles can be converted from small values to a zero float, in this case just dont draw */
		Py_RETURN_NONE;
	}
	
	
	/* fetch a C Image pointer from the passed-in Python object */
	py_img = ( BPy_Image * ) pyObjImage;
	image = py_img->image;
	ibuf = BKE_image_get_ibuf( image, NULL );
		
	if( !ibuf )      /* if failed to load the image */
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
									  "couldn't load image data in Blender" );
	if( !ibuf->rect )      /* no float yet */
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
									  "Image has no byte rect" );
	
	/* Update the time tag of the image */
	tag_image_time(image);

	/* set up a valid clipping rectangle.  if no clip rectangle was
	 * given, this results in inclusion of the entire image.  otherwise,
	 * the clipping is just checked against the bounds of the image.
	 * if clipW or clipH are less than zero then they include as much of
	 * the image as they can. */
	clipX = EXPP_ClampInt( clipX, 0, ibuf->x );
	clipY = EXPP_ClampInt( clipY, 0, ibuf->y );
	if( ( clipW < 0 ) || ( clipX+clipW > ibuf->x ) )
		clipW = ibuf->x - clipX;
	if( ( clipH < 0 ) || ( clipY+clipH > ibuf->y ) )
		clipH = ibuf->y - clipY;

	/* -- we are "Go" to Draw! -- */

	/* set the raster position.
	 *
	 * If the raster position is negative, then using glRasterPos2i() 
	 * directly would cause it to be clipped.  Instead, we first establish 
	 * a valid raster position within the clipping rectangle of the 
	 * window and then use glBitmap() with a NULL image pointer to offset 
	 * it to the true position we require.  To pick an initial valid 
	 * raster position within the viewport, we query the clipping rectangle
	 * and use its lower-left pixel.
	 *
	 * This particular technique is documented in the glRasterPos() man
	 * page, although I haven't seen it used elsewhere in Blender.
	 */

	/* update (W): to fix a bug where images wouldn't get drawn if the bottom
	 * left corner of the Scripts win were above a given height or to the right
	 * of a given width, the code below is being commented out.  It should not
	 * be needed anyway, because spaces in Blender are projected to lie inside
	 * their areas, see src/drawscript.c for example.  Note: the
	 * glaRasterPosSafe2i function in src/glutil.c does use the commented out
	 * technique, but with 0,0 instead of scissorBox.  This function can be
	 * a little optimized, based on glaDrawPixelsSafe in that same fine, but
	 * we're too close to release 2.37 right now. */
	/*
	glGetFloatv( GL_SCISSOR_BOX, scissorBox );
	glRasterPos2i( scissorBox[0], scissorBox[1] );
	glBitmap( 0, 0, 0.0, 0.0, 
		originX-scissorBox[0], originY-scissorBox[1], NULL );
	*/

	/* update (cont.): using these two lines instead:
	 * (based on glaRasterPosSafe2i, but Ken Hughes deserves credit
	 * for suggesting this exact fix in the bug tracker) */
	glRasterPos2i(0, 0);
	glBitmap( 0, 0, 0.0, 0.0, originX, originY, NULL );

	/* set the zoom */
	glPixelZoom( zoomX, zoomY );

	/* set the width of the image (ROW_LENGTH), and the offset to the
	 * clip origin within the image in x (SKIP_PIXELS) and 
	 * y (SKIP_ROWS) */
	glPixelStorei( GL_UNPACK_ROW_LENGTH,  ibuf->x );
	glPixelStorei( GL_UNPACK_SKIP_PIXELS, clipX );
	glPixelStorei( GL_UNPACK_SKIP_ROWS,   clipY );

	/* draw the image */
	glDrawPixels( clipW, clipH, GL_RGBA, GL_UNSIGNED_BYTE, 
		ibuf->rect );

	/* restore the defaults for some parameters (we could also use a
	 * glPushClientAttrib() and glPopClientAttrib() pair). */
	glPixelZoom( 1.0, 1.0 );
	glPixelStorei( GL_UNPACK_SKIP_ROWS,   0 );
	glPixelStorei( GL_UNPACK_SKIP_PIXELS, 0 );
	glPixelStorei( GL_UNPACK_ROW_LENGTH,  0 );

	Py_RETURN_NONE;

}

PyObject *Draw_Init( void )
{
	PyObject *submodule, *dict;

	if( PyType_Ready( &Button_Type) < 0)
		Py_RETURN_NONE;

	submodule = Py_InitModule3( "Blender.Draw", Draw_methods, Draw_doc );

	dict = PyModule_GetDict( submodule );

#define EXPP_ADDCONST(x) \
	EXPP_dict_set_item_str(dict, #x, PyInt_FromLong(x))

	/* So, for example:
	 * EXPP_ADDCONST(LEFTMOUSE) becomes
	 * EXPP_dict_set_item_str(dict, "LEFTMOUSE", PyInt_FromLong(LEFTMOUSE)) 
	 */

	EXPP_ADDCONST( LEFTMOUSE );
	EXPP_ADDCONST( MIDDLEMOUSE );
	EXPP_ADDCONST( RIGHTMOUSE );
	EXPP_ADDCONST( WHEELUPMOUSE );
	EXPP_ADDCONST( WHEELDOWNMOUSE );
	EXPP_ADDCONST( MOUSEX );
	EXPP_ADDCONST( MOUSEY );
	EXPP_ADDCONST( TIMER0 );
	EXPP_ADDCONST( TIMER1 );
	EXPP_ADDCONST( TIMER2 );
	EXPP_ADDCONST( TIMER3 );
	EXPP_ADDCONST( KEYBD );
	EXPP_ADDCONST( RAWKEYBD );
	EXPP_ADDCONST( REDRAW );
	EXPP_ADDCONST( INPUTCHANGE );
	EXPP_ADDCONST( QFULL );
	EXPP_ADDCONST( WINFREEZE );
	EXPP_ADDCONST( WINTHAW );
	EXPP_ADDCONST( WINCLOSE );
	EXPP_ADDCONST( WINQUIT );
#ifndef IRISGL
	EXPP_ADDCONST( Q_FIRSTTIME );
#endif
	EXPP_ADDCONST( AKEY );
	EXPP_ADDCONST( BKEY );
	EXPP_ADDCONST( CKEY );
	EXPP_ADDCONST( DKEY );
	EXPP_ADDCONST( EKEY );
	EXPP_ADDCONST( FKEY );
	EXPP_ADDCONST( GKEY );
	EXPP_ADDCONST( HKEY );
	EXPP_ADDCONST( IKEY );
	EXPP_ADDCONST( JKEY );
	EXPP_ADDCONST( KKEY );
	EXPP_ADDCONST( LKEY );
	EXPP_ADDCONST( MKEY );
	EXPP_ADDCONST( NKEY );
	EXPP_ADDCONST( OKEY );
	EXPP_ADDCONST( PKEY );
	EXPP_ADDCONST( QKEY );
	EXPP_ADDCONST( RKEY );
	EXPP_ADDCONST( SKEY );
	EXPP_ADDCONST( TKEY );
	EXPP_ADDCONST( UKEY );
	EXPP_ADDCONST( VKEY );
	EXPP_ADDCONST( WKEY );
	EXPP_ADDCONST( XKEY );
	EXPP_ADDCONST( YKEY );
	EXPP_ADDCONST( ZKEY );
	EXPP_ADDCONST( ZEROKEY );
	EXPP_ADDCONST( ONEKEY );
	EXPP_ADDCONST( TWOKEY );
	EXPP_ADDCONST( THREEKEY );
	EXPP_ADDCONST( FOURKEY );
	EXPP_ADDCONST( FIVEKEY );
	EXPP_ADDCONST( SIXKEY );
	EXPP_ADDCONST( SEVENKEY );
	EXPP_ADDCONST( EIGHTKEY );
	EXPP_ADDCONST( NINEKEY );
	EXPP_ADDCONST( CAPSLOCKKEY );
	EXPP_ADDCONST( LEFTCTRLKEY );
	EXPP_ADDCONST( LEFTALTKEY );
	EXPP_ADDCONST( RIGHTALTKEY );
	EXPP_ADDCONST( RIGHTCTRLKEY );
	EXPP_ADDCONST( RIGHTSHIFTKEY );
	EXPP_ADDCONST( LEFTSHIFTKEY );
	EXPP_ADDCONST( ESCKEY );
	EXPP_ADDCONST( TABKEY );
	EXPP_ADDCONST( RETKEY );
	EXPP_ADDCONST( SPACEKEY );
	EXPP_ADDCONST( LINEFEEDKEY );
	EXPP_ADDCONST( BACKSPACEKEY );
	EXPP_ADDCONST( DELKEY );
	EXPP_ADDCONST( SEMICOLONKEY );
	EXPP_ADDCONST( PERIODKEY );
	EXPP_ADDCONST( COMMAKEY );
	EXPP_ADDCONST( QUOTEKEY );
	EXPP_ADDCONST( ACCENTGRAVEKEY );
	EXPP_ADDCONST( MINUSKEY );
	EXPP_ADDCONST( SLASHKEY );
	EXPP_ADDCONST( BACKSLASHKEY );
	EXPP_ADDCONST( EQUALKEY );
	EXPP_ADDCONST( LEFTBRACKETKEY );
	EXPP_ADDCONST( RIGHTBRACKETKEY );
	EXPP_ADDCONST( LEFTARROWKEY );
	EXPP_ADDCONST( DOWNARROWKEY );
	EXPP_ADDCONST( RIGHTARROWKEY );
	EXPP_ADDCONST( UPARROWKEY );
	EXPP_ADDCONST( PAD2 );
	EXPP_ADDCONST( PAD4 );
	EXPP_ADDCONST( PAD6 );
	EXPP_ADDCONST( PAD8 );
	EXPP_ADDCONST( PAD1 );
	EXPP_ADDCONST( PAD3 );
	EXPP_ADDCONST( PAD5 );
	EXPP_ADDCONST( PAD7 );
	EXPP_ADDCONST( PAD9 );
	EXPP_ADDCONST( PADPERIOD );
	EXPP_ADDCONST( PADSLASHKEY );
	EXPP_ADDCONST( PADASTERKEY );
	EXPP_ADDCONST( PAD0 );
	EXPP_ADDCONST( PADMINUS );
	EXPP_ADDCONST( PADENTER );
	EXPP_ADDCONST( PADPLUSKEY );
	EXPP_ADDCONST( F1KEY );
	EXPP_ADDCONST( F2KEY );
	EXPP_ADDCONST( F3KEY );
	EXPP_ADDCONST( F4KEY );
	EXPP_ADDCONST( F5KEY );
	EXPP_ADDCONST( F6KEY );
	EXPP_ADDCONST( F7KEY );
	EXPP_ADDCONST( F8KEY );
	EXPP_ADDCONST( F9KEY );
	EXPP_ADDCONST( F10KEY );
	EXPP_ADDCONST( F11KEY );
	EXPP_ADDCONST( F12KEY );
	EXPP_ADDCONST( PAUSEKEY );
	EXPP_ADDCONST( INSERTKEY );
	EXPP_ADDCONST( HOMEKEY );
	EXPP_ADDCONST( PAGEUPKEY );
	EXPP_ADDCONST( PAGEDOWNKEY );
	EXPP_ADDCONST( ENDKEY );

	return submodule;
}
