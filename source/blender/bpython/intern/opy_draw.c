/*  python.c      MIXED MODEL
 * 
 *  june 99
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

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
#include "mydevice.h"  /* for all the event constants */

#include "Python.h"
#include "BPY_macros.h"
#include "BPY_main.h"
#include "BPY_tools.h"

int disable_force_draw= 0;

/* hack to flag that window redraw has happened inside slider callback: */

static void exit_pydraw (SpaceText *st);
static uiBlock *Get_uiBlock (void);
void initDraw (void);

/* Button Object */

typedef struct _Button {
	PyObject_VAR_HEAD

	int type; /* 1 == int, 2 == float, 3 == string */
	int slen; /* length of string (if type == 3) */
	union {
		int asint;
		float asfloat;
		char *asstr;
	} val;
} Button;


static void Button_dealloc(PyObject *self) {
	Button *but= (Button*) self;

	if(but->type==3) MEM_freeN(but->val.asstr);
		
	PyMem_DEL(self);	
}

static PyObject *Button_getattr(PyObject *self, char *name) {
	Button *but= (Button*) self;
	
	if(STREQ(name, "val")) {
		if (but->type==1)
			return Py_BuildValue("i", but->val.asint);			
		else if (but->type==2) 
			return Py_BuildValue("f", but->val.asfloat);
		else if (but->type==3) 
			return Py_BuildValue("s", but->val.asstr);
	}
	PyErr_SetString(PyExc_AttributeError, name);
	return NULL;
}

static int Button_setattr(PyObject *self,  char *name, PyObject *v) {
	Button *but= (Button*) self;
	
	if(STREQ(name, "val")) {
		if  (but->type==1)
			PyArg_Parse(v, "i", &but->val.asint);
		else if (but->type==2)
			PyArg_Parse(v, "f", &but->val.asfloat);			
		else if (but->type==3) {
			char *newstr;
			
			PyArg_Parse(v, "s", &newstr);
			strncpy(but->val.asstr, newstr, but->slen); 
		}
	} else {
		PyErr_SetString(PyExc_AttributeError, name);
		return -1;
	}
	
	return 0;
}

static PyObject *Button_repr(PyObject *self) {
	return PyObject_Repr(Button_getattr(self, "val"));	
}

PyTypeObject Button_Type = {
	PyObject_HEAD_INIT(NULL)
	0,								/*ob_size*/
	"Button",						/*tp_name*/
	sizeof(Button),					/*tp_basicsize*/
	0,								/*tp_itemsize*/
	(destructor) Button_dealloc,	/*tp_dealloc*/
	(printfunc)  0,					/*tp_print*/
	(getattrfunc) Button_getattr,	/*tp_getattr*/
	(setattrfunc) Button_setattr,	/*tp_setattr*/
	(cmpfunc)  0,					/*tp_cmp*/
	(reprfunc)  Button_repr,		/*tp_repr*/
};


static Button *newbutton (void) {
	Button *but= (Button *) PyObject_NEW(Button, &Button_Type);
	
	return but;
}


/* GUI interface routines */

static void exit_pydraw(SpaceText *st) 
{
	scrarea_queue_redraw(st->area);
	
	if (st) {	
		Py_XDECREF((PyObject *) st->py_draw);
		Py_XDECREF((PyObject *) st->py_event);
		Py_XDECREF((PyObject *) st->py_button);

		st->py_draw= st->py_event= st->py_button= NULL;
	}
}

static void exec_callback(SpaceText *st, PyObject *callback, PyObject *args) 
{
	PyObject *result= PyEval_CallObject(callback, args);
	
	if (result==NULL) {
		st->text->compiled= NULL;
		PyErr_Print();
		exit_pydraw(st);
	}
	Py_XDECREF(result);
	Py_DECREF(args);
}

/* the handler for drawing routines (see Register method) */

void BPY_spacetext_do_pywin_draw(SpaceText *st) 
{
	uiBlock *block;
	char butblock[20];

	sprintf(butblock, "win %d", curarea->win);
	block= uiNewBlock(&curarea->uiblocks, butblock, UI_EMBOSSX, UI_HELV, curarea->win);
	
	if (st->py_draw) {
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		exec_callback(st, st->py_draw, Py_BuildValue("()"));
		glPopAttrib();
	} else {
		glClearColor(0.4375, 0.4375, 0.4375, 0.0); 
		glClear(GL_COLOR_BUFFER_BIT);
	}

	uiDrawBlock(block);

	curarea->win_swap= WIN_BACK_OK;
}

/* the handler for button event routines (see Register method) */

static void spacetext_do_pywin_buttons(SpaceText *st, unsigned short event) {
	if (st->py_button) {
		exec_callback(st, st->py_button, Py_BuildValue("(i)", event));
	}
}

/* calls the generic event handling methods registered with Register */

void BPY_spacetext_do_pywin_event(SpaceText *st, unsigned short event, short val) {
	if (event==QKEY && G.qual & (LR_ALTKEY|LR_CTRLKEY|LR_SHIFTKEY)) {
		exit_pydraw(st);
		return;
	}

	if (val) {
		if (uiDoBlocks(&curarea->uiblocks, event)!=UI_NOTHING ) event= 0;

		if (event==UI_BUT_EVENT) {
			spacetext_do_pywin_buttons(st, val);
		}
	}
		
	if (st->py_event) {
		exec_callback(st, st->py_event, Py_BuildValue("(ii)", event, val));
	}
}

int BPY_spacetext_is_pywin(SpaceText *st) {
	return (st->py_draw || st->py_event || st->py_button);
}

static char Method_Exit_doc[]= 
"() - Exit the windowing interface";
static PyObject *Method_Exit (PyObject *self, PyObject *args)
{	
	SpaceText *st= curarea->spacedata.first;
#ifdef CLEAR_NAMESPACE	
	PyObject *d;
#endif
	BPY_TRY(PyArg_ParseTuple(args, ""));
	
	exit_pydraw(st);
#ifdef CLEAR_NAMESPACE	
	d = st->py_globaldict; // The current window's global namespace dictionary
	if (d) {
		PyDict_Clear(d);
		Py_DECREF(d); // release dictionary
	}	
#endif
	
	return BPY_incr_ret(Py_None);
}

static char Method_Register_doc[]= 
"(draw, event, button) - Register callbacks for windowing\n\
\n\
(draw) A function to draw the screen, taking no arguments\n\
(event) A function to handle events, taking 2 arguments (evt, val)\n\
	(evt) The event number\n\
	(val) The value modifier (for key and mouse press/release)\n\
(button) A function to handle button events, taking 1 argument (evt)\n\
	(evt) The button number\n\
\n\
A None object can be passed if a callback is unused.";

static PyObject *Method_Register (PyObject *self, PyObject *args)
{
	PyObject *newdrawc= NULL, *neweventc= NULL, *newbuttonc= NULL;
	SpaceText *st= curarea->spacedata.first;
	
	BPY_TRY(PyArg_ParseTuple(args, "O|OO", &newdrawc, &neweventc, &newbuttonc));

	/* This is a hack again:
	 * Every python script should actually do a global variable cleanup at 
	 * the end of execution. 
	 * For scripts registering GUI callbacks, this does not work, because
	 * the global namespace of the interpreter still needs to be accessed
	 * from the callback. 
	 * Workaround: a text object has a flag which allows the global name
	 * space to be cleared at the end of the script. This flag should be
	 * normally set when executed with Alt-P. For a script registering with
	 * the GUI though, clear the flag and set it when the GUI mode is left
	 * (Method_Exit).
	 */

	BPY_debug(("--- disable clear namespace"));

	st->flags &= ~ST_CLEAR_NAMESPACE;


	if (!PyCallable_Check(newdrawc)) newdrawc= NULL;
	if (!PyCallable_Check(neweventc)) neweventc= NULL;
	if (!PyCallable_Check(newbuttonc)) newbuttonc= NULL;

	if (!(newdrawc || neweventc || newbuttonc))
		return BPY_incr_ret(Py_None);
		
	exit_pydraw(st);

	Py_XINCREF(newdrawc);
	Py_XINCREF(neweventc);
	Py_XINCREF(newbuttonc);
	
	st->py_draw= newdrawc;
	st->py_event= neweventc;
	st->py_button= newbuttonc;

	scrarea_queue_redraw(st->area);

	return BPY_incr_ret(Py_None);
}


static char Method_Redraw_doc[]= 
"([after]) - Queue a redraw event\n\
\n\
[after=0] Determines whether the redraw is processed before or after other input events.\n\
\n\
Redraw events are buffered so that regardless of how many events are queued\n\
the window only receives one redraw event.";

static PyObject *Method_Redraw (PyObject *self,  PyObject *args)
{
	int after= 0;
	
	BPY_TRY(PyArg_ParseTuple(args, "|i", &after));

	if (after) addafterqueue(curarea->win, REDRAW, 1);
	else scrarea_queue_winredraw(curarea);
	
	return BPY_incr_ret(Py_None);
}


static char Method_Draw_doc[]= 
"() - Force an immediate redraw\n\
\n\
Forced redraws are not buffered, in other words the window is redrawn\n\
exactly once for everytime this function is called.";
static PyObject *Method_Draw (PyObject *self,  PyObject *args)
{
	/* If forced drawing is disable queue a redraw event instead */
	if (disable_force_draw) {
		scrarea_queue_winredraw(curarea);
		return BPY_incr_ret(Py_None);
	}
	
	BPY_TRY(PyArg_ParseTuple(args, ""));

	scrarea_do_windraw(curarea);

	screen_swapbuffers();
	
	return BPY_incr_ret(Py_None);
}



static char Method_Create_doc[]= 
"(value) - Create a default Button object\n\
\n\
(value) - The value to store in the button\n\
\n\
Valid values are ints, floats, and strings";

static PyObject *Method_Create (PyObject *self,  PyObject *args)
{
	Button *but;
	PyObject *in;

	BPY_TRY(PyArg_ParseTuple(args, "O", &in));
	
	but= newbutton();
	if(PyFloat_Check(in)) {
		but->type= 2;
		but->val.asfloat= PyFloat_AsDouble(in);
	} else if (PyInt_Check(in)) {		
		but->type= 1;
		but->val.asint= PyInt_AsLong(in);
	} else if (PyString_Check(in)) {
		char *newstr= PyString_AsString(in);
		
		but->type= 3;
		but->slen= strlen(newstr);
		but->val.asstr= MEM_mallocN(but->slen+1, "button string");
		
		strcpy(but->val.asstr, newstr);
	}
		
	return (PyObject *) but;
}

static uiBlock *Get_uiBlock(void)
{
	char butblock[32];
	
	sprintf(butblock, "win %d", curarea->win);

	return uiGetBlock(butblock, curarea);
}

static char Method_Button_doc[]= 
"(name, event, x, y, width, height, [tooltip]) - Create a new Button (push) button\n\
\n\
(name) A string to display on the button\n\
(event) The event number to pass to the button event function when activated\n\
(x, y) The lower left coordinate of the button\n\
(width, height) The button width and height\n\
[tooltip=""] The button's tooltip";

static PyObject *Method_Button (PyObject *self,  PyObject *args)
{
	uiBlock *block;
	char *name, *tip= NULL;
	int event;
	int x, y, w, h;
	
	BPY_TRY(PyArg_ParseTuple(args, "siiiii|s", &name, &event, &x, &y, &w, &h, &tip));
	
	block= Get_uiBlock();

	if(block) uiDefBut(block, BUT, event, name, x, y, w, h, 0, 0, 0, 0, 0, tip);
	
	return BPY_incr_ret(Py_None);
}

static char Method_Menu_doc[]=
"(name, event, x, y, width, height, default, [tooltip]) - Create a new Menu button\n\
\n\
(name) A string to display on the button\n\
(event) The event number to pass to the button event function when activated\n\
(x, y) The lower left coordinate of the button\n\
(width, height) The button width and height\n\
(default) The number of the option to be selected by default\n\
[tooltip=""] The button's tooltip\n\
\n\
The menu options are specified through the name of the\n\
button. Options are followed by a format code and seperated\n\
by the '|' (pipe) character.\n\
Valid format codes are\n\
	%t - The option should be used as the title\n\
	%xN - The option should set the integer N in the button value.";
	
static PyObject *Method_Menu (PyObject *self,  PyObject *args)
{
	uiBlock *block;
	char *name, *tip= NULL;
	int event, def;
	int x, y, w, h;
	Button *but;
	
	BPY_TRY(PyArg_ParseTuple(args, "siiiiii|s", &name, &event, &x, &y, &w, &h, &def, &tip));
	
	but= newbutton();
	but->type= 1;
	but->val.asint= def;
	
	block= Get_uiBlock();
	if(block) uiDefButI(block, MENU, event, name, x, y, w, h, &but->val.asint, 0, 0, 0, 0, tip);
	
	return (PyObject *) but;
}

static char Method_Toggle_doc[]= 
"(name, event, x, y, width, height, default, [tooltip]) - Create a new Toggle button\n\
\n\
(name) A string to display on the button\n\
(event) The event number to pass to the button event function when activated\n\
(x, y) The lower left coordinate of the button\n\
(width, height) The button width and height\n\
(default) An integer (0 or 1) specifying the default state\n\
[tooltip=""] The button's tooltip";

static PyObject *Method_Toggle (PyObject *self,  PyObject *args)
{
	uiBlock *block;
	char *name, *tip= NULL;
	int event;
	int x, y, w, h, def;
	Button *but;
	
	BPY_TRY(PyArg_ParseTuple(args, "siiiiii|s", &name, &event, &x, &y, &w, &h, &def, &tip));
	
	but= newbutton();
	but->type= 1;
	but->val.asint= def;
	
	block= Get_uiBlock();
	if(block) uiDefButI(block, TOG, event, name, x, y, w, h, &but->val.asint, 0, 0, 0, 0, tip);
	
	return (PyObject *) but;
}


/* DO NOT TOUCH THIS FUNCTION !
   Redrawing a slider inside its own callback routine is actually forbidden
   with the current toolkit architecture (button routines are not reentrant).
   But it works anyway.
   XXX This is condemned to be dinosource in future - it's a hack.
   */

static void py_slider_update(void *butv, void *data2_unused) 
{
	uiBut *but= butv;

	disable_force_draw= 1;
		/* 
		Disable forced drawing, otherwise the button object which
		is still being used might be deleted 
		*/

//	UIfrontbuf = 0; 
//	spacetext_do_pywin_buttons(curarea->spacedata.first, but->retval);

	g_window_redrawn = 0;
	curarea->win_swap= WIN_BACK_OK; 
	UIfrontbuf = 1; 
	spacetext_do_pywin_buttons(curarea->spacedata.first, uiButGetRetVal(but));
	UIfrontbuf = 0;

	if (!g_window_redrawn) /* if Redraw already called */
		Windowmodule_Redraw(0, Py_BuildValue("(i)", SPACE_VIEW3D));

	disable_force_draw= 0;
}

static char Method_Slider_doc[]= 
"(name, event, x, y, width, height, initial, min, max, [update, tooltip]) - Create a new Slider button\n\
\n\
(name) A string to display on the button\n\
(event) The event number to pass to the button event function when activated\n\
(x, y) The lower left coordinate of the button\n\
(width, height) The button width and height\n\
(initial, min, max) Three values (int or float) specifying the initial and limit values.\n\
[update=1] A value controlling whether the slider will emit events as it is edited.\n\
			A non-zero value (default) enables the events. A zero value supresses them.\n\
[tooltip=""] The button's tooltip";

static PyObject *Method_Slider (PyObject *self,  PyObject *args)
{
	uiBlock *block;
	char *name, *tip= NULL;
	int event;
	int x, y, w, h, realtime=1;
	Button *but;
	PyObject *mino, *maxo, *inio;
	
	BPY_TRY(PyArg_ParseTuple(args, "siiiiiOOO|is", &name, &event, &x, &y, &w, &h, &inio, &mino, &maxo, &realtime, &tip));

	
	but= newbutton();
	if (PyFloat_Check(inio)) {
		float ini, min, max;

		ini= PyFloat_AsDouble(inio);
		min= PyFloat_AsDouble(mino);
		max= PyFloat_AsDouble(maxo);
				
		but->type= 2;
		but->val.asfloat= ini;

		block= Get_uiBlock();
		if(block) {
			uiBut *ubut;
			ubut= uiDefButF(block, NUMSLI, event, name, x, y, w, h, &but->val.asfloat, min, max, 0, 0, tip);
			if (realtime) uiButSetFunc(ubut, py_slider_update, ubut, NULL);
		}		
	} 
	else {
		int ini, min, max;

		ini= PyInt_AsLong(inio);
		min= PyInt_AsLong(mino);
		max= PyInt_AsLong(maxo);
		
		but->type= 1;
		but->val.asint= ini;
	
		block= Get_uiBlock();
		if(block) {
			uiBut *ubut;
			ubut= uiDefButI(block, NUMSLI, event, name, x, y, w, h, &but->val.asint, min, max, 0, 0, tip);
			if (realtime) uiButSetFunc(ubut, py_slider_update, ubut, NULL);
		}
	}
	return (PyObject *) but;
}

static char Method_Scrollbar_doc[]= 
"(event, x, y, width, height, initial, min, max, [update, tooltip]) - Create a new Scrollbar\n\
\n\
(event) The event number to pass to the button event function when activated\n\
(x, y) The lower left coordinate of the button\n\
(width, height) The button width and height\n\
(initial, min, max) Three values (int or float) specifying the initial and limit values.\n\
[update=1] A value controlling whether the slider will emit events as it is edited.\n\
			A non-zero value (default) enables the events. A zero value supresses them.\n\
[tooltip=""] The button's tooltip";

static PyObject *Method_Scrollbar (PyObject *self,  PyObject *args)
{
	char *tip= NULL;
	uiBlock *block;
	int event;
	int x, y, w, h, realtime=1;
	Button *but;
	PyObject *mino, *maxo, *inio;
	float ini, min, max;

	BPY_TRY(PyArg_ParseTuple(args, "iiiiiOOO|is", &event, &x, &y, &w, &h, &inio, &mino, &maxo, &realtime, &tip));
	
	if (!PyNumber_Check(inio) || !PyNumber_Check(inio) || !PyNumber_Check(inio))
		return BPY_err_ret_ob(PyExc_AttributeError, "expected numbers for initial, min, and max");
		
	but= newbutton();
	
	if (PyFloat_Check(inio)) but->type= 2;
	else but->type= 1;
		
	ini= PyFloat_AsDouble(inio);
	min= PyFloat_AsDouble(mino);
	max= PyFloat_AsDouble(maxo);
				
	if (but->type==2) {
		but->val.asfloat= ini;
		block= Get_uiBlock();
		if(block) {
			uiBut *ubut;
			ubut= uiDefButF(block, SCROLL, event, "", x, y, w, h, &but->val.asfloat, min, max, 0, 0, tip);
			if (realtime) uiButSetFunc(ubut, py_slider_update, ubut, NULL);
		}
	} else {
		but->val.asint= ini;
		block= Get_uiBlock();
		if(block) {
			uiBut *ubut;
			ubut= uiDefButI(block, SCROLL, event, "", x, y, w, h, &but->val.asint, min, max, 0, 0, tip);
			if (realtime) uiButSetFunc(ubut, py_slider_update, ubut, NULL);
		}
	}
	
	return (PyObject *) but;
}

static char Method_Number_doc[]= 
"(name, event, x, y, width, height, initial, min, max, [tooltip]) - Create a new Number button\n\
\n\
(name) A string to display on the button\n\
(event) The event number to pass to the button event function when activated\n\
(x, y) The lower left coordinate of the button\n\
(width, height) The button width and height\n\
(initial, min, max) Three values (int or float) specifying the initial and limit values.\n\
[tooltip=""] The button's tooltip";

static PyObject *Method_Number (PyObject *self,  PyObject *args)
{
	uiBlock *block;
	char *name, *tip= NULL;
	int event;
	int x, y, w, h;
	Button *but;
	PyObject *mino, *maxo, *inio;
	
	BPY_TRY(PyArg_ParseTuple(args, "siiiiiOOO|s", &name, &event, &x, &y, &w, &h, &inio, &mino, &maxo, &tip));
	
	but= newbutton();
	
	if (PyFloat_Check(inio)) {
		float ini, min, max;

		ini= PyFloat_AsDouble(inio);
		min= PyFloat_AsDouble(mino);
		max= PyFloat_AsDouble(maxo);
				
		but->type= 2;
		but->val.asfloat= ini;
	
		block= Get_uiBlock();
		if(block) uiDefButF(block, NUM, event, name, x, y, w, h, &but->val.asfloat, min, max, 0, 0, tip);
	} else {
		int ini, min, max;

		ini= PyInt_AsLong(inio);
		min= PyInt_AsLong(mino);
		max= PyInt_AsLong(maxo);
		
		but->type= 1;
		but->val.asint= ini;
	
		block= Get_uiBlock();
		if(block) uiDefButI(block, NUM, event, name, x, y, w, h, &but->val.asint, min, max, 0, 0, tip);
	}
	
	return (PyObject *) but;
}

static char Method_String_doc[]= 
"(name, event, x, y, width, height, initial, length, [tooltip]) - Create a new String button\n\
\n\
(name) A string to display on the button\n\
(event) The event number to pass to the button event function when activated\n\
(x, y) The lower left coordinate of the button\n\
(width, height) The button width and height\n\
(initial) The string to display initially\n\
(length) The maximum input length\n\
[tooltip=""] The button's tooltip";

static PyObject *Method_String (PyObject *self,  PyObject *args)
{
	uiBlock *block;
	char *name, *tip= NULL, *newstr;
	int event;
	int x, y, w, h, len;
	Button *but;
	
	BPY_TRY(PyArg_ParseTuple(args, "siiiiisi|s", &name, &event, &x, &y, &w, &h, &newstr, &len, &tip));
	
	but= newbutton();
	but->type= 3;
	but->slen= len;
	but->val.asstr= MEM_mallocN(len+1, "button string");
	
	strncpy(but->val.asstr, newstr, len);
	but->val.asstr[len]= 0;
	
	block= Get_uiBlock();
	if(block) uiDefBut(block, TEX, event, name, x, y, w, h, but->val.asstr, 0, len, 0, 0, tip);

	return (PyObject *) but;
}

static char Method_Text_doc[]= 
"(text) - Draw text onscreen\n\
\n\
(text) The text to draw\n";
static PyObject *Method_Text (PyObject *self, PyObject *args)
{
	char *text;
	
	BPY_TRY(PyArg_ParseTuple(args, "s", &text));
	
	BMF_DrawString(G.font, text);
	
	return BPY_incr_ret(Py_None);
}

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

PyObject *init_py_draw(void) 
{
	PyObject *mod= Py_InitModule(SUBMODULE(Draw), Draw_methods);
	PyObject *dict= PyModule_GetDict(mod);

	Button_Type.ob_type= &PyType_Type;

	BPY_ADDCONST(dict, LEFTMOUSE);
	BPY_ADDCONST(dict, MIDDLEMOUSE);
	BPY_ADDCONST(dict, RIGHTMOUSE);
	BPY_ADDCONST(dict, MOUSEX);
	BPY_ADDCONST(dict, MOUSEY);
	BPY_ADDCONST(dict, TIMER0);
	BPY_ADDCONST(dict, TIMER1);
	BPY_ADDCONST(dict, TIMER2);
	BPY_ADDCONST(dict, TIMER3);
	BPY_ADDCONST(dict, KEYBD);
	BPY_ADDCONST(dict, RAWKEYBD);
	BPY_ADDCONST(dict, REDRAW);
	BPY_ADDCONST(dict, INPUTCHANGE);
	BPY_ADDCONST(dict, QFULL);
	BPY_ADDCONST(dict, WINFREEZE);
	BPY_ADDCONST(dict, WINTHAW);
	BPY_ADDCONST(dict, WINCLOSE);
	BPY_ADDCONST(dict, WINQUIT);
#ifndef IRISGL
	BPY_ADDCONST(dict, Q_FIRSTTIME);
#endif
	BPY_ADDCONST(dict, AKEY);
	BPY_ADDCONST(dict, BKEY);
	BPY_ADDCONST(dict, CKEY);
	BPY_ADDCONST(dict, DKEY);
	BPY_ADDCONST(dict, EKEY);
	BPY_ADDCONST(dict, FKEY);
	BPY_ADDCONST(dict, GKEY);
	BPY_ADDCONST(dict, HKEY);
	BPY_ADDCONST(dict, IKEY);
	BPY_ADDCONST(dict, JKEY);
	BPY_ADDCONST(dict, KKEY);
	BPY_ADDCONST(dict, LKEY);
	BPY_ADDCONST(dict, MKEY);
	BPY_ADDCONST(dict, NKEY);
	BPY_ADDCONST(dict, OKEY);
	BPY_ADDCONST(dict, PKEY);
	BPY_ADDCONST(dict, QKEY);
	BPY_ADDCONST(dict, RKEY);
	BPY_ADDCONST(dict, SKEY);
	BPY_ADDCONST(dict, TKEY);
	BPY_ADDCONST(dict, UKEY);
	BPY_ADDCONST(dict, VKEY);
	BPY_ADDCONST(dict, WKEY);
	BPY_ADDCONST(dict, XKEY);
	BPY_ADDCONST(dict, YKEY);
	BPY_ADDCONST(dict, ZKEY);
	BPY_ADDCONST(dict, ZEROKEY);
	BPY_ADDCONST(dict, ONEKEY);
	BPY_ADDCONST(dict, TWOKEY);
	BPY_ADDCONST(dict, THREEKEY);
	BPY_ADDCONST(dict, FOURKEY);
	BPY_ADDCONST(dict, FIVEKEY);
	BPY_ADDCONST(dict, SIXKEY);
	BPY_ADDCONST(dict, SEVENKEY);
	BPY_ADDCONST(dict, EIGHTKEY);
	BPY_ADDCONST(dict, NINEKEY);
	BPY_ADDCONST(dict, CAPSLOCKKEY);
	BPY_ADDCONST(dict, LEFTCTRLKEY);
	BPY_ADDCONST(dict, LEFTALTKEY);
	BPY_ADDCONST(dict, RIGHTALTKEY);
	BPY_ADDCONST(dict, RIGHTCTRLKEY);
	BPY_ADDCONST(dict, RIGHTSHIFTKEY);
	BPY_ADDCONST(dict, LEFTSHIFTKEY);
	BPY_ADDCONST(dict, ESCKEY);
	BPY_ADDCONST(dict, TABKEY);
	BPY_ADDCONST(dict, RETKEY);
	BPY_ADDCONST(dict, SPACEKEY);
	BPY_ADDCONST(dict, LINEFEEDKEY);
	BPY_ADDCONST(dict, BACKSPACEKEY);
	BPY_ADDCONST(dict, DELKEY);
	BPY_ADDCONST(dict, SEMICOLONKEY);
	BPY_ADDCONST(dict, PERIODKEY);
	BPY_ADDCONST(dict, COMMAKEY);
	BPY_ADDCONST(dict, QUOTEKEY);
	BPY_ADDCONST(dict, ACCENTGRAVEKEY);
	BPY_ADDCONST(dict, MINUSKEY);
	BPY_ADDCONST(dict, SLASHKEY);
	BPY_ADDCONST(dict, BACKSLASHKEY);
	BPY_ADDCONST(dict, EQUALKEY);
	BPY_ADDCONST(dict, LEFTBRACKETKEY);
	BPY_ADDCONST(dict, RIGHTBRACKETKEY);
	BPY_ADDCONST(dict, LEFTARROWKEY);
	BPY_ADDCONST(dict, DOWNARROWKEY);
	BPY_ADDCONST(dict, RIGHTARROWKEY);
	BPY_ADDCONST(dict, UPARROWKEY);
	BPY_ADDCONST(dict, PAD2);
	BPY_ADDCONST(dict, PAD4);
	BPY_ADDCONST(dict, PAD6);
	BPY_ADDCONST(dict, PAD8);
	BPY_ADDCONST(dict, PAD1);
	BPY_ADDCONST(dict, PAD3);
	BPY_ADDCONST(dict, PAD5);
	BPY_ADDCONST(dict, PAD7);
	BPY_ADDCONST(dict, PAD9);
	BPY_ADDCONST(dict, PADPERIOD);
	BPY_ADDCONST(dict, PADSLASHKEY);
	BPY_ADDCONST(dict, PADASTERKEY);
	BPY_ADDCONST(dict, PAD0);
	BPY_ADDCONST(dict, PADMINUS);
	BPY_ADDCONST(dict, PADENTER);
	BPY_ADDCONST(dict, PADPLUSKEY);
	BPY_ADDCONST(dict, F1KEY);
	BPY_ADDCONST(dict, F2KEY);
	BPY_ADDCONST(dict, F3KEY);
	BPY_ADDCONST(dict, F4KEY);
	BPY_ADDCONST(dict, F5KEY);
	BPY_ADDCONST(dict, F6KEY);
	BPY_ADDCONST(dict, F7KEY);
	BPY_ADDCONST(dict, F8KEY);
	BPY_ADDCONST(dict, F9KEY);
	BPY_ADDCONST(dict, F10KEY);
	BPY_ADDCONST(dict, F11KEY);
	BPY_ADDCONST(dict, F12KEY);
	BPY_ADDCONST(dict, PAUSEKEY);
	BPY_ADDCONST(dict, INSERTKEY);
	BPY_ADDCONST(dict, HOMEKEY);
	BPY_ADDCONST(dict, PAGEUPKEY);
	BPY_ADDCONST(dict, PAGEDOWNKEY);
	BPY_ADDCONST(dict, ENDKEY);
	
	return mod;
}


/* Buffer Object */

/* For Python access to OpenGL functions requiring
 * a pointer.
 */

PyTypeObject Buffer_Type;

typedef struct _Buffer {
	PyObject_VAR_HEAD

	PyObject *parent;
	
	int type; /* GL_BYTE, GL_SHORT, GL_INT, GL_FLOAT */
	int ndimensions;
	int *dimensions;
	
	union {
		char	*asbyte;
		short	*asshort;
		int		*asint;
		float	*asfloat;

		void	*asvoid;
	} buf;
} Buffer;

static int type_size(int type) {
	switch (type) {
	case GL_BYTE: 
		return sizeof(char);
	case GL_SHORT: 
		return sizeof(short);
	case GL_INT: 
		return sizeof(int);
	case GL_FLOAT: 
		return sizeof(float);
	}
	return -1;
}

static Buffer *make_buffer(int type, int ndimensions, int *dimensions) {
	Buffer *buffer;
	void *buf= NULL;
	int i, size, length;
	
	length= 1;
	for (i=0; i<ndimensions; i++) length*= dimensions[i];
	
	size= type_size(type);
	
	buf= MEM_mallocN(length*size, "Buffer buffer");
	
	buffer= (Buffer *) PyObject_NEW(Buffer, &Buffer_Type);
	buffer->parent= NULL;
	buffer->ndimensions= ndimensions;
	buffer->dimensions= dimensions;
	buffer->type= type;
	buffer->buf.asvoid= buf;
	
	for (i= 0; i<length; i++) {
		if (type==GL_BYTE) 
			buffer->buf.asbyte[i]= 0;

		else if (type==GL_SHORT) 
			buffer->buf.asshort[i]= 0;

		else if (type==GL_INT) 
			buffer->buf.asint[i]= 0;

		else if (type==GL_FLOAT) 
			buffer->buf.asfloat[i]= 0.0;
	}
	
	return buffer;
}

static int Buffer_ass_slice(PyObject *self, int begin, int end, PyObject *seq);

static char Method_Buffer_doc[]=
"(type, dimensions, [template]) - Create a new Buffer object\n\
\n\
(type) - The format to store data in\n\
(dimensions) - An int or sequence specifying the dimensions of the buffer\n\
[template] - A sequence of matching dimensions to the buffer to be created\n\
	which will be used to initialize the Buffer.\n\
\n\
If a template is not passed in all fields will be initialized to 0.\n\
\n\
The type should be one of GL_BYTE, GL_SHORT, GL_INT, or GL_FLOAT.\n\
If the dimensions are specified as an int a linear buffer will be\n\
created. If a sequence is passed for the dimensions the buffer\n\
will have len(sequence) dimensions, where the size for each dimension\n\
is determined by the value in the sequence at that index.\n\
\n\
For example, passing [100, 100] will create a 2 dimensional\n\
square buffer. Passing [16, 16, 32] will create a 3 dimensional\n\
buffer which is twice as deep as it is wide or high.";

static PyObject *Method_Buffer (PyObject *self, PyObject *args)
{
	PyObject *length_ob= NULL, *template= NULL;
	Buffer *buffer;
	
	int i, type;
	int *dimensions = 0, ndimensions = 0;
	
	BPY_TRY(PyArg_ParseTuple(args, "iO|O", &type, &length_ob, &template));

	if (type!=GL_BYTE && type!=GL_SHORT && type!=GL_INT && type!=GL_FLOAT) {
		PyErr_SetString(PyExc_AttributeError, "type");
		return NULL;
	}

	if (PyNumber_Check(length_ob)) {
		ndimensions= 1;
		dimensions= MEM_mallocN(ndimensions*sizeof(int), "Buffer dimensions");
		dimensions[0]= PyInt_AsLong(length_ob);

	} else if (PySequence_Check(length_ob)) {
		ndimensions= PySequence_Length(length_ob);
		dimensions= MEM_mallocN(ndimensions*sizeof(int), "Buffer dimensions");
		
		for (i=0; i<ndimensions; i++) {
			PyObject *ob= PySequence_GetItem(length_ob, i);
			
			if (!PyNumber_Check(ob)) dimensions[i]= 1;
			else dimensions[i]= PyInt_AsLong(ob);
			
			Py_DECREF(ob);
		}
	}
	
	buffer= make_buffer(type, ndimensions, dimensions);
	if (template && ndimensions) {
		if (Buffer_ass_slice((PyObject *) buffer, 0, dimensions[0], template)) {
			Py_DECREF(buffer);
			return NULL;
		}
	}
	
	return (PyObject *) buffer;
}

/**********/


/* Buffer sequence methods */

static int Buffer_len(PyObject *self) {
	Buffer *buf= (Buffer *) self;
	
	return buf->dimensions[0];
}

static PyObject *Buffer_item(PyObject *self, int i) {
	Buffer *buf= (Buffer *) self;

	if (i >= buf->dimensions[0]) {
		PyErr_SetString(PyExc_IndexError, "array index out of range");
		return NULL;
	}
	
	if (buf->ndimensions==1) {
		switch (buf->type) {
		case GL_BYTE: return Py_BuildValue("b", buf->buf.asbyte[i]);
		case GL_SHORT: return Py_BuildValue("h", buf->buf.asshort[i]);
		case GL_INT: return Py_BuildValue("i", buf->buf.asint[i]);
		case GL_FLOAT: return Py_BuildValue("f", buf->buf.asfloat[i]);
		}
	} else {
		Buffer *newbuf;
		int j, length, size;
		
		length= 1;
		for (j=1; j<buf->ndimensions; j++) {
			length*= buf->dimensions[j];
		}
		size= type_size(buf->type);
		
		newbuf= (Buffer *) PyObject_NEW(Buffer, &Buffer_Type);
		
		Py_INCREF(self);
		newbuf->parent= self;
		
		newbuf->ndimensions= buf->ndimensions-1;
		newbuf->type= buf->type;
		newbuf->buf.asvoid= buf->buf.asbyte + i*length*size;

		newbuf->dimensions= MEM_mallocN(newbuf->ndimensions*sizeof(int), "Buffer dimensions");
		memcpy(newbuf->dimensions, buf->dimensions+1, newbuf->ndimensions*sizeof(int));
		
		return (PyObject *) newbuf;
	}
	
	return NULL;
}

static PyObject *Buffer_slice(PyObject *self, int begin, int end)
{
	Buffer *buf= (Buffer *) self;
	PyObject *list;
	int count;
	
	if (begin<0) begin= 0;
	if (end>buf->dimensions[0]) end= buf->dimensions[0];
	if (begin>end) begin= end;
		
	list= PyList_New(end-begin);

	for (count= begin; count<end; count++)
		PyList_SetItem(list, count-begin, Buffer_item(self, count));
	
	return list;
}

static int Buffer_ass_item(PyObject *self, int i, PyObject *v) {
	Buffer *buf= (Buffer *) self;
	
	if (i >= buf->dimensions[0]) {
		PyErr_SetString(PyExc_IndexError, "array assignment index out of range");
		return -1;
	}
	
	if (buf->ndimensions!=1) {
		PyObject *row= Buffer_item(self, i);
		int ret;
		
		if (!row) return -1;

		ret= Buffer_ass_slice(row, 0, buf->dimensions[1], v);
		Py_DECREF(row);
		
		return ret;
	}

	if (buf->type==GL_BYTE) {
		if (!PyArg_Parse(v, "b;Coordinates must be ints", &buf->buf.asbyte[i]))
			return -1;
		
	} else if (buf->type==GL_SHORT) {
		if (!PyArg_Parse(v, "h;Coordinates must be ints", &buf->buf.asshort[i]))
			return -1;
		
	} else if (buf->type==GL_INT) {
		if (!PyArg_Parse(v, "i;Coordinates must be ints", &buf->buf.asint[i]))
			return -1;
		
	} else if (buf->type==GL_FLOAT) {
		if (!PyArg_Parse(v, "f;Coordinates must be floats", &buf->buf.asfloat[i]))
			return -1;
	}
	
	return 0;
}

static int Buffer_ass_slice(PyObject *self, int begin, int end, PyObject *seq)
{
	Buffer *buf= (Buffer *) self;
	PyObject *item;
	int count, err=0;
	
	if (begin<0) begin= 0;
	if (end>buf->dimensions[0]) end= buf->dimensions[0];
	if (begin>end) begin= end;
	
	if (!PySequence_Check(seq)) {
		PyErr_SetString(PyExc_TypeError, "illegal argument type for built-in operation");
		return -1;
	}
	
	if (PySequence_Length(seq)!=(end-begin)) {
		PyErr_SetString(PyExc_TypeError, "size mismatch in assignment");
		return -1;
	}
	
	for (count= begin; count<end; count++) {
		item= PySequence_GetItem(seq, count-begin);
		err= Buffer_ass_item(self, count, item);
		Py_DECREF(item);
		
		if (err) break;
	}

	return err;
}
static PySequenceMethods Buffer_SeqMethods = {
	(inquiry)			Buffer_len,			/*sq_length*/
	(binaryfunc)		0,					/*sq_concat*/
	(intargfunc)		0,					/*sq_repeat*/
	(intargfunc)		Buffer_item,		/*sq_item*/
	(intintargfunc)		Buffer_slice,		/*sq_slice*/
	(intobjargproc)		Buffer_ass_item,	/*sq_ass_item*/
	(intintobjargproc)	Buffer_ass_slice,	/*sq_ass_slice*/
};



/**********/

static void Buffer_dealloc(PyObject *self) {
	Buffer *buf= (Buffer *) self;

	if (buf->parent) Py_DECREF(buf->parent);
	else MEM_freeN(buf->buf.asvoid);

	MEM_freeN(buf->dimensions);
	
	PyMem_DEL(self);	
}

static PyObject *Buffer_tolist(PyObject *self) {
	int i, len= ((Buffer *)self)->dimensions[0];
	PyObject *list= PyList_New(len);
	
	for (i=0; i<len; i++) {
		PyList_SetItem(list, i, Buffer_item(self, i));
	}
	
	return list;
}

static PyObject *Buffer_dimensions(PyObject *self) {
	Buffer *buffer= (Buffer *) self;
	PyObject *list= PyList_New(buffer->ndimensions);
	int i;
		
	for (i= 0; i<buffer->ndimensions; i++) {
		PyList_SetItem(list, i, PyInt_FromLong(buffer->dimensions[i]));
	}
	
	return list;
}

static PyObject *Buffer_getattr(PyObject *self, char *name) {
	if (strcmp(name, "list")==0) return Buffer_tolist(self);
	else if (strcmp(name, "dimensions")==0) return Buffer_dimensions(self);
	
	PyErr_SetString(PyExc_AttributeError, name);
	return NULL;
}

static PyObject *Buffer_repr(PyObject *self) {
	PyObject *list= Buffer_tolist(self);
	PyObject *repr= PyObject_Repr(list);
	Py_DECREF(list);
	
	return repr;
}

PyTypeObject Buffer_Type = {
	PyObject_HEAD_INIT(NULL)
	0,								/*ob_size*/
	"Buffer",						/*tp_name*/
	sizeof(Buffer),					/*tp_basicsize*/
	0,								/*tp_itemsize*/
	(destructor) Buffer_dealloc,	/*tp_dealloc*/
	(printfunc)  0,					/*tp_print*/
	(getattrfunc) Buffer_getattr,	/*tp_getattr*/
	(setattrfunc) 0,				/*tp_setattr*/
	(cmpfunc) 0,					/*tp_compare*/
	(reprfunc) Buffer_repr,			/*tp_repr*/
	0,								/*tp_as_number*/
	&Buffer_SeqMethods,				/*tp_as_sequence*/
};

#ifndef __APPLE__
/* By golly George! It looks like fancy pants macro time!!! */

/*
#define int_str				"i"
#define int_var(number)		bgl_int##number
#define int_ref(number)		&bgl_int##number
#define int_def(number)		int int_var(number)

#define float_str			"f"
#define float_var(number)	bgl_float##number
#define float_ref(number)	&bgl_float##number
#define float_def(number)	float float_var(number)
*/

/* TYPE_str is the string to pass to Py_ArgParse (for the format) */
/* TYPE_var is the name to pass to the GL function */
/* TYPE_ref is the pointer to pass to Py_ArgParse (to store in) */
/* TYPE_def is the C initialization of the variable */

#define void_str			""
#define void_var(num)		
#define void_ref(num)		&bgl_var##num
#define void_def(num)		char bgl_var##num

#define buffer_str			"O!"
#define buffer_var(number)	(bgl_buffer##number)->buf.asvoid
#define buffer_ref(number)	&Buffer_Type, &bgl_buffer##number
#define buffer_def(number)	Buffer *bgl_buffer##number

/* GL Pointer fields, handled by buffer type */
/* GLdoubleP, GLfloatP, GLintP, GLuintP, GLshortP */

#define GLbooleanP_str			"O!"
#define GLbooleanP_var(number)	(bgl_buffer##number)->buf.asvoid
#define GLbooleanP_ref(number)	&Buffer_Type, &bgl_buffer##number
#define GLbooleanP_def(number)	Buffer *bgl_buffer##number

#define GLbyteP_str			"O!"
#define GLbyteP_var(number)	(bgl_buffer##number)->buf.asvoid
#define GLbyteP_ref(number)	&Buffer_Type, &bgl_buffer##number
#define GLbyteP_def(number)	Buffer *bgl_buffer##number

#define GLubyteP_str			"O!"
#define GLubyteP_var(number)	(bgl_buffer##number)->buf.asvoid
#define GLubyteP_ref(number)	&Buffer_Type, &bgl_buffer##number
#define GLubyteP_def(number)	Buffer *bgl_buffer##number

#define GLintP_str			"O!"
#define GLintP_var(number)	(bgl_buffer##number)->buf.asvoid
#define GLintP_ref(number)	&Buffer_Type, &bgl_buffer##number
#define GLintP_def(number)	Buffer *bgl_buffer##number

#define GLuintP_str			"O!"
#define GLuintP_var(number)	(bgl_buffer##number)->buf.asvoid
#define GLuintP_ref(number)	&Buffer_Type, &bgl_buffer##number
#define GLuintP_def(number)	Buffer *bgl_buffer##number

#define GLshortP_str			"O!"
#define GLshortP_var(number)	(bgl_buffer##number)->buf.asvoid
#define GLshortP_ref(number)	&Buffer_Type, &bgl_buffer##number
#define GLshortP_def(number)	Buffer *bgl_buffer##number

#define GLushortP_str			"O!"
#define GLushortP_var(number)	(bgl_buffer##number)->buf.asvoid
#define GLushortP_ref(number)	&Buffer_Type, &bgl_buffer##number
#define GLushortP_def(number)	Buffer *bgl_buffer##number

#define GLfloatP_str			"O!"
#define GLfloatP_var(number)	(bgl_buffer##number)->buf.asvoid
#define GLfloatP_ref(number)	&Buffer_Type, &bgl_buffer##number
#define GLfloatP_def(number)	Buffer *bgl_buffer##number

#define GLdoubleP_str			"O!"
#define GLdoubleP_var(number)	(bgl_buffer##number)->buf.asvoid
#define GLdoubleP_ref(number)	&Buffer_Type, &bgl_buffer##number
#define GLdoubleP_def(number)	Buffer *bgl_buffer##number

#define GLclampfP_str			"O!"
#define GLclampfP_var(number)	(bgl_buffer##number)->buf.asvoid
#define GLclampfP_ref(number)	&Buffer_Type, &bgl_buffer##number
#define GLclampfP_def(number)	Buffer *bgl_buffer##number

#define GLvoidP_str			"O!"
#define GLvoidP_var(number)	(bgl_buffer##number)->buf.asvoid
#define GLvoidP_ref(number)	&Buffer_Type, &bgl_buffer##number
#define GLvoidP_def(number)	Buffer *bgl_buffer##number

#define buffer_str			"O!"
#define buffer_var(number)	(bgl_buffer##number)->buf.asvoid
#define buffer_ref(number)	&Buffer_Type, &bgl_buffer##number
#define buffer_def(number)	Buffer *bgl_buffer##number

/* The standard GL typedefs are used as prototypes, we can't
 * use the GL type directly because Py_ArgParse expects normal
 * C types.
 * 
 * Py_ArgParse doesn't grok writing into unsigned variables, 
 * so we use signed everything (even stuff that should be unsigned.
 */
 
/* typedef unsigned int GLenum; */
#define GLenum_str			"i"
#define GLenum_var(num)		bgl_var##num
#define GLenum_ref(num)		&bgl_var##num
#define GLenum_def(num)		/* unsigned */ int GLenum_var(num)

/* typedef unsigned int GLboolean; */
#define GLboolean_str			"b"
#define GLboolean_var(num)		bgl_var##num
#define GLboolean_ref(num)		&bgl_var##num
#define GLboolean_def(num)		/* unsigned */ char GLboolean_var(num)

/* typedef unsigned int GLbitfield; */
#define GLbitfield_str			"i"
#define GLbitfield_var(num)		bgl_var##num
#define GLbitfield_ref(num)		&bgl_var##num
#define GLbitfield_def(num)		/* unsigned */ int GLbitfield_var(num)

/* typedef signed char GLbyte; */
#define GLbyte_str				"b"
#define GLbyte_var(num)			bgl_var##num
#define GLbyte_ref(num)			&bgl_var##num
#define GLbyte_def(num)			signed char GLbyte_var(num)

/* typedef short GLshort; */
#define GLshort_str				"h"
#define GLshort_var(num)		bgl_var##num
#define GLshort_ref(num)		&bgl_var##num
#define GLshort_def(num)		short GLshort_var(num)

/* typedef int GLint; */
#define GLint_str				"i"
#define GLint_var(num)			bgl_var##num
#define GLint_ref(num)			&bgl_var##num
#define GLint_def(num)			int GLint_var(num)

/* typedef int GLsizei; */
#define GLsizei_str				"i"
#define GLsizei_var(num)		bgl_var##num
#define GLsizei_ref(num)		&bgl_var##num
#define GLsizei_def(num)		int GLsizei_var(num)

/* typedef unsigned char GLubyte; */
#define GLubyte_str				"b"
#define GLubyte_var(num)		bgl_var##num
#define GLubyte_ref(num)		&bgl_var##num
#define GLubyte_def(num)		/* unsigned */ char GLubyte_var(num)

/* typedef unsigned short GLushort; */
#define GLushort_str			"h"
#define GLushort_var(num)		bgl_var##num
#define GLushort_ref(num)		&bgl_var##num
#define GLushort_def(num)		/* unsigned */ short GLushort_var(num)

/* typedef unsigned int GLuint; */
#define GLuint_str				"i"
#define GLuint_var(num)			bgl_var##num
#define GLuint_ref(num)			&bgl_var##num
#define GLuint_def(num)			/* unsigned */ int GLuint_var(num)

/* typedef float GLfloat; */
#define GLfloat_str				"f"
#define GLfloat_var(num)		bgl_var##num
#define GLfloat_ref(num)		&bgl_var##num
#define GLfloat_def(num)		float GLfloat_var(num)

/* typedef float GLclampf; */
#define GLclampf_str			"f"
#define GLclampf_var(num)		bgl_var##num
#define GLclampf_ref(num)		&bgl_var##num
#define GLclampf_def(num)		float GLclampf_var(num)

/* typedef double GLdouble; */
#define GLdouble_str			"d"
#define GLdouble_var(num)		bgl_var##num
#define GLdouble_ref(num)		&bgl_var##num
#define GLdouble_def(num)		double GLdouble_var(num)

/* typedef double GLclampd; */
#define GLclampd_str			"d"
#define GLclampd_var(num)		bgl_var##num
#define GLclampd_ref(num)		&bgl_var##num
#define GLclampd_def(num)		double GLclampd_var(num)

/* typedef void GLvoid; */
/* #define GLvoid_str				"" */
/* #define GLvoid_var(num)			bgl_var##num */
/* #define GLvoid_ref(num)			&bgl_var##num */
/* #define GLvoid_def(num)			char bgl_var##num */

#define arg_def1(a1)					a1##_def(1)
#define arg_def2(a1, a2)				arg_def1(a1); a2##_def(2)
#define arg_def3(a1, a2, a3)			arg_def2(a1, a2); a3##_def(3)
#define arg_def4(a1, a2, a3, a4)		arg_def3(a1, a2, a3); a4##_def(4)
#define arg_def5(a1, a2, a3, a4, a5)	arg_def4(a1, a2, a3, a4); a5##_def(5)
#define arg_def6(a1, a2, a3, a4, a5, a6)arg_def5(a1, a2, a3, a4, a5); a6##_def(6)
#define arg_def7(a1, a2, a3, a4, a5, a6, a7)arg_def6(a1, a2, a3, a4, a5, a6); a7##_def(7)
#define arg_def8(a1, a2, a3, a4, a5, a6, a7, a8)arg_def7(a1, a2, a3, a4, a5, a6, a7); a8##_def(8)
#define arg_def9(a1, a2, a3, a4, a5, a6, a7, a8, a9)arg_def8(a1, a2, a3, a4, a5, a6, a7, a8); a9##_def(9)
#define arg_def10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)arg_def9(a1, a2, a3, a4, a5, a6, a7, a8, a9); a10##_def(10)

#define arg_var1(a1)					a1##_var(1)
#define arg_var2(a1, a2)				arg_var1(a1), a2##_var(2)
#define arg_var3(a1, a2, a3)			arg_var2(a1, a2), a3##_var(3)
#define arg_var4(a1, a2, a3, a4)		arg_var3(a1, a2, a3), a4##_var(4)
#define arg_var5(a1, a2, a3, a4, a5)	arg_var4(a1, a2, a3, a4), a5##_var(5)
#define arg_var6(a1, a2, a3, a4, a5, a6)arg_var5(a1, a2, a3, a4, a5), a6##_var(6)
#define arg_var7(a1, a2, a3, a4, a5, a6, a7)arg_var6(a1, a2, a3, a4, a5, a6), a7##_var(7)
#define arg_var8(a1, a2, a3, a4, a5, a6, a7, a8)arg_var7(a1, a2, a3, a4, a5, a6, a7), a8##_var(8)
#define arg_var9(a1, a2, a3, a4, a5, a6, a7, a8, a9)arg_var8(a1, a2, a3, a4, a5, a6, a7, a8), a9##_var(9)
#define arg_var10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)arg_var9(a1, a2, a3, a4, a5, a6, a7, a8, a9), a10##_var(10)

#define arg_ref1(a1)					a1##_ref(1)
#define arg_ref2(a1, a2)				arg_ref1(a1), a2##_ref(2)
#define arg_ref3(a1, a2, a3)			arg_ref2(a1, a2), a3##_ref(3)
#define arg_ref4(a1, a2, a3, a4)		arg_ref3(a1, a2, a3), a4##_ref(4)
#define arg_ref5(a1, a2, a3, a4, a5)	arg_ref4(a1, a2, a3, a4), a5##_ref(5)
#define arg_ref6(a1, a2, a3, a4, a5, a6)arg_ref5(a1, a2, a3, a4, a5), a6##_ref(6)
#define arg_ref7(a1, a2, a3, a4, a5, a6, a7)arg_ref6(a1, a2, a3, a4, a5, a6), a7##_ref(7)
#define arg_ref8(a1, a2, a3, a4, a5, a6, a7, a8)arg_ref7(a1, a2, a3, a4, a5, a6, a7), a8##_ref(8)
#define arg_ref9(a1, a2, a3, a4, a5, a6, a7, a8, a9)arg_ref8(a1, a2, a3, a4, a5, a6, a7, a8), a9##_ref(9)
#define arg_ref10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)arg_ref9(a1, a2, a3, a4, a5, a6, a7, a8, a9), a10##_ref(10)

#define arg_str1(a1)					a1##_str
#define arg_str2(a1, a2)				arg_str1(a1) a2##_str
#define arg_str3(a1, a2, a3)			arg_str2(a1, a2) a3##_str
#define arg_str4(a1, a2, a3, a4)		arg_str3(a1, a2, a3) a4##_str
#define arg_str5(a1, a2, a3, a4, a5)	arg_str4(a1, a2, a3, a4) a5##_str
#define arg_str6(a1, a2, a3, a4, a5, a6)arg_str5(a1, a2, a3, a4, a5) a6##_str
#define arg_str7(a1, a2, a3, a4, a5, a6, a7)arg_str6(a1, a2, a3, a4, a5, a6) a7##_str
#define arg_str8(a1, a2, a3, a4, a5, a6, a7, a8)arg_str7(a1, a2, a3, a4, a5, a6, a7) a8##_str
#define arg_str9(a1, a2, a3, a4, a5, a6, a7, a8, a9)arg_str8(a1, a2, a3, a4, a5, a6, a7, a8) a9##_str
#define arg_str10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)arg_str9(a1, a2, a3, a4, a5, a6, a7, a8, a9) a10##_str

#define ret_def_void	
#define ret_set_void	
#define ret_ret_void		return BPY_incr_ret(Py_None)

#define ret_def_GLint		int ret_int
#define ret_set_GLint		ret_int= 
#define ret_ret_GLint		return PyInt_FromLong(ret_int);

#define ret_def_GLuint		unsigned int ret_uint
#define ret_set_GLuint		ret_uint= 
#define ret_ret_GLuint		return PyInt_FromLong((long) ret_uint);

#define ret_def_GLenum		unsigned int ret_uint
#define ret_set_GLenum		ret_uint= 
#define ret_ret_GLenum		return PyInt_FromLong((long) ret_uint);

#define ret_def_GLboolean	unsigned char ret_bool
#define ret_set_GLboolean	ret_bool= 
#define ret_ret_GLboolean	return PyInt_FromLong((long) ret_bool);

#define ret_def_GLstring	const unsigned char *ret_str;
#define ret_set_GLstring	ret_str= 
#define ret_ret_GLstring	return PyString_FromString(ret_str);

#define BGL_Wrap(nargs, funcname, ret, arg_list) \
static PyObject *Method_##funcname (PyObject *self, PyObject *args) {\
	arg_def##nargs arg_list; \
	ret_def_##ret; \
	if(!PyArg_ParseTuple(args, arg_str##nargs arg_list, arg_ref##nargs arg_list)) return NULL;\
	ret_set_##ret gl##funcname (arg_var##nargs arg_list);\
	ret_ret_##ret; \
}

BGL_Wrap(2, Accum, 					void, 		(GLenum, GLfloat))
BGL_Wrap(2, AlphaFunc, 				void, 		(GLenum, GLclampf))
BGL_Wrap(3, AreTexturesResident,	GLboolean,	(GLsizei, GLuintP, GLbooleanP))
BGL_Wrap(1, Begin, 					void, 		(GLenum))
BGL_Wrap(2, BindTexture,			void,		(GLenum, GLuint))
BGL_Wrap(7, Bitmap, 				void, 		(GLsizei, GLsizei, GLfloat, GLfloat, GLfloat, GLfloat, GLubyteP))
BGL_Wrap(2, BlendFunc, 				void, 		(GLenum, GLenum))
BGL_Wrap(1, CallList, 				void, 		(GLuint))
BGL_Wrap(3, CallLists, 				void, 		(GLsizei, GLenum, GLvoidP))
BGL_Wrap(1, Clear, 					void, 		(GLbitfield))
BGL_Wrap(4, ClearAccum, 			void, 		(GLfloat, GLfloat, GLfloat, GLfloat))
BGL_Wrap(4, ClearColor, 			void, 		(GLclampf, GLclampf, GLclampf, GLclampf))
BGL_Wrap(1, ClearDepth, 			void, 		(GLclampd))
BGL_Wrap(1, ClearIndex, 			void, 		(GLfloat))
BGL_Wrap(1, ClearStencil, 			void, 		(GLint))
BGL_Wrap(2, ClipPlane, 				void, 		(GLenum, GLdoubleP))
BGL_Wrap(3, Color3b, 				void, 		(GLbyte, GLbyte, GLbyte))
BGL_Wrap(1, Color3bv, 				void, 		(GLbyteP))
BGL_Wrap(3, Color3d, 				void, 		(GLdouble, GLdouble, GLdouble))
BGL_Wrap(1, Color3dv, 				void, 		(GLdoubleP))
BGL_Wrap(3, Color3f, 				void, 		(GLfloat, GLfloat, GLfloat))
BGL_Wrap(1, Color3fv, 				void, 		(GLfloatP))
BGL_Wrap(3, Color3i, 				void, 		(GLint, GLint, GLint))
BGL_Wrap(1, Color3iv, 				void, 		(GLintP))
BGL_Wrap(3, Color3s, 				void, 		(GLshort, GLshort, GLshort))
BGL_Wrap(1, Color3sv, 				void, 		(GLshortP))
BGL_Wrap(3, Color3ub, 				void, 		(GLubyte, GLubyte, GLubyte))
BGL_Wrap(1, Color3ubv, 				void, 		(GLubyteP))
BGL_Wrap(3, Color3ui, 				void, 		(GLuint, GLuint, GLuint))
BGL_Wrap(1, Color3uiv, 				void, 		(GLuintP))
BGL_Wrap(3, Color3us, 				void, 		(GLushort, GLushort, GLushort))
BGL_Wrap(1, Color3usv, 				void, 		(GLushortP))
BGL_Wrap(4, Color4b, 				void, 		(GLbyte, GLbyte, GLbyte, GLbyte))
BGL_Wrap(1, Color4bv, 				void, 		(GLbyteP))
BGL_Wrap(4, Color4d, 				void, 		(GLdouble, GLdouble, GLdouble, GLdouble))
BGL_Wrap(1, Color4dv, 				void, 		(GLdoubleP))
BGL_Wrap(4, Color4f, 				void, 		(GLfloat, GLfloat, GLfloat, GLfloat))
BGL_Wrap(1, Color4fv, 				void, 		(GLfloatP))
BGL_Wrap(4, Color4i, 				void, 		(GLint, GLint, GLint, GLint))
BGL_Wrap(1, Color4iv, 				void, 		(GLintP))
BGL_Wrap(4, Color4s, 				void, 		(GLshort, GLshort, GLshort, GLshort))
BGL_Wrap(1, Color4sv, 				void, 		(GLshortP))
BGL_Wrap(4, Color4ub, 				void, 		(GLubyte, GLubyte, GLubyte, GLubyte))
BGL_Wrap(1, Color4ubv, 				void, 		(GLubyteP))
BGL_Wrap(4, Color4ui, 				void, 		(GLuint, GLuint, GLuint, GLuint))
BGL_Wrap(1, Color4uiv, 				void, 		(GLuintP))
BGL_Wrap(4, Color4us, 				void, 		(GLushort, GLushort, GLushort, GLushort))
BGL_Wrap(1, Color4usv, 				void, 		(GLushortP))
BGL_Wrap(4, ColorMask, 				void, 		(GLboolean, GLboolean, GLboolean, GLboolean))
BGL_Wrap(2, ColorMaterial, 			void, 		(GLenum, GLenum))
BGL_Wrap(5, CopyPixels, 			void, 		(GLint, GLint, GLsizei, GLsizei, GLenum))
BGL_Wrap(1, CullFace, 				void, 		(GLenum))
BGL_Wrap(2, DeleteLists, 			void, 		(GLuint, GLsizei))
BGL_Wrap(2, DeleteTextures,			void,		(GLsizei, GLuintP))
BGL_Wrap(1, DepthFunc, 				void, 		(GLenum))
BGL_Wrap(1, DepthMask, 				void, 		(GLboolean))
BGL_Wrap(2, DepthRange, 			void, 		(GLclampd, GLclampd))
BGL_Wrap(1, Disable, 				void, 		(GLenum))
BGL_Wrap(1, DrawBuffer, 			void, 		(GLenum))
BGL_Wrap(5, DrawPixels, 			void, 		(GLsizei, GLsizei, GLenum, GLenum, GLvoidP))
BGL_Wrap(1, EdgeFlag, 				void, 		(GLboolean))
BGL_Wrap(1, EdgeFlagv, 				void, 		(GLbooleanP))
BGL_Wrap(1, Enable, 				void, 		(GLenum))
BGL_Wrap(1, End, 					void, 		(void))
BGL_Wrap(1, EndList, 				void, 		(void))
BGL_Wrap(1, EvalCoord1d, 			void, 		(GLdouble))
BGL_Wrap(1, EvalCoord1dv, 			void, 		(GLdoubleP))
BGL_Wrap(1, EvalCoord1f, 			void, 		(GLfloat))
BGL_Wrap(1, EvalCoord1fv, 			void, 		(GLfloatP))
BGL_Wrap(2, EvalCoord2d, 			void, 		(GLdouble, GLdouble))
BGL_Wrap(1, EvalCoord2dv, 			void, 		(GLdoubleP))
BGL_Wrap(2, EvalCoord2f, 			void, 		(GLfloat, GLfloat))
BGL_Wrap(1, EvalCoord2fv, 			void, 		(GLfloatP))
BGL_Wrap(3, EvalMesh1, 				void, 		(GLenum, GLint, GLint))
BGL_Wrap(5, EvalMesh2, 				void, 		(GLenum, GLint, GLint, GLint, GLint))
BGL_Wrap(1, EvalPoint1, 			void, 		(GLint))
BGL_Wrap(2, EvalPoint2, 			void, 		(GLint, GLint))
BGL_Wrap(3, FeedbackBuffer, 		void, 		(GLsizei, GLenum, GLfloatP))
BGL_Wrap(1, Finish, 				void, 		(void))
BGL_Wrap(1, Flush, 					void, 		(void))
BGL_Wrap(2, Fogf, 					void, 		(GLenum, GLfloat))
BGL_Wrap(2, Fogfv, 					void, 		(GLenum, GLfloatP))
BGL_Wrap(2, Fogi, 					void, 		(GLenum, GLint))
BGL_Wrap(2, Fogiv, 					void, 		(GLenum, GLintP))
BGL_Wrap(1, FrontFace, 				void, 		(GLenum))
BGL_Wrap(6, Frustum, 				void, 		(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble))
BGL_Wrap(1, GenLists, 				GLuint, 	(GLsizei))
BGL_Wrap(2, GenTextures, 			void,	 	(GLsizei, GLuintP))
BGL_Wrap(2, GetBooleanv, 			void, 		(GLenum, GLbooleanP))
BGL_Wrap(2, GetClipPlane, 			void, 		(GLenum, GLdoubleP))
BGL_Wrap(2, GetDoublev, 			void, 		(GLenum, GLdoubleP))
BGL_Wrap(1, GetError, 				GLenum, 	(void))
BGL_Wrap(2, GetFloatv, 				void, 		(GLenum, GLfloatP))
BGL_Wrap(2, GetIntegerv, 			void, 		(GLenum, GLintP))
BGL_Wrap(3, GetLightfv, 			void, 		(GLenum, GLenum, GLfloatP))
BGL_Wrap(3, GetLightiv, 			void, 		(GLenum, GLenum, GLintP))
BGL_Wrap(3, GetMapdv, 				void, 		(GLenum, GLenum, GLdoubleP))
BGL_Wrap(3, GetMapfv, 				void, 		(GLenum, GLenum, GLfloatP))
BGL_Wrap(3, GetMapiv, 				void, 		(GLenum, GLenum, GLintP))
BGL_Wrap(3, GetMaterialfv, 			void, 		(GLenum, GLenum, GLfloatP))
BGL_Wrap(3, GetMaterialiv, 			void, 		(GLenum, GLenum, GLintP))
BGL_Wrap(2, GetPixelMapfv, 			void, 		(GLenum, GLfloatP))
BGL_Wrap(2, GetPixelMapuiv, 		void, 		(GLenum, GLuintP))
BGL_Wrap(2, GetPixelMapusv, 		void, 		(GLenum, GLushortP))
BGL_Wrap(1, GetPolygonStipple, 		void, 		(GLubyteP))
BGL_Wrap(1, GetString, 				GLstring, 	(GLenum))
BGL_Wrap(3, GetTexEnvfv, 			void, 		(GLenum, GLenum, GLfloatP))
BGL_Wrap(3, GetTexEnviv, 			void, 		(GLenum, GLenum, GLintP))
BGL_Wrap(3, GetTexGendv, 			void, 		(GLenum, GLenum, GLdoubleP))
BGL_Wrap(3, GetTexGenfv, 			void, 		(GLenum, GLenum, GLfloatP))
BGL_Wrap(3, GetTexGeniv, 			void, 		(GLenum, GLenum, GLintP))
BGL_Wrap(5, GetTexImage, 			void, 		(GLenum, GLint, GLenum, GLenum, GLvoidP))
BGL_Wrap(4, GetTexLevelParameterfv, void, 		(GLenum, GLint, GLenum, GLfloatP))
BGL_Wrap(4, GetTexLevelParameteriv, void, 		(GLenum, GLint, GLenum, GLintP))
BGL_Wrap(3, GetTexParameterfv, 		void, 		(GLenum, GLenum, GLfloatP))
BGL_Wrap(3, GetTexParameteriv, 		void, 		(GLenum, GLenum, GLintP))
BGL_Wrap(2, Hint, 					void, 		(GLenum, GLenum))
BGL_Wrap(1, IndexMask, 				void, 		(GLuint))
BGL_Wrap(1, Indexd, 				void, 		(GLdouble))
BGL_Wrap(1, Indexdv, 				void, 		(GLdoubleP))
BGL_Wrap(1, Indexf, 				void, 		(GLfloat))
BGL_Wrap(1, Indexfv, 				void, 		(GLfloatP))
BGL_Wrap(1, Indexi, 				void, 		(GLint))
BGL_Wrap(1, Indexiv, 				void, 		(GLintP))
BGL_Wrap(1, Indexs, 				void, 		(GLshort))
BGL_Wrap(1, Indexsv, 				void, 		(GLshortP))
BGL_Wrap(1, InitNames, 				void, 		(void))
BGL_Wrap(1, IsEnabled, 				GLboolean, 	(GLenum))
BGL_Wrap(1, IsList, 				GLboolean, 	(GLuint))
BGL_Wrap(1, IsTexture, 				GLboolean, 	(GLuint))
BGL_Wrap(2, LightModelf, 			void, 		(GLenum, GLfloat))
BGL_Wrap(2, LightModelfv, 			void, 		(GLenum, GLfloatP))
BGL_Wrap(2, LightModeli, 			void, 		(GLenum, GLint))
BGL_Wrap(2, LightModeliv, 			void, 		(GLenum, GLintP))
BGL_Wrap(3, Lightf, 				void, 		(GLenum, GLenum, GLfloat))
BGL_Wrap(3, Lightfv, 				void, 		(GLenum, GLenum, GLfloatP))
BGL_Wrap(3, Lighti, 				void, 		(GLenum, GLenum, GLint))
BGL_Wrap(3, Lightiv, 				void, 		(GLenum, GLenum, GLintP))
BGL_Wrap(2, LineStipple, 			void, 		(GLint, GLushort))
BGL_Wrap(1, LineWidth, 				void, 		(GLfloat))
BGL_Wrap(1, ListBase, 				void, 		(GLuint))
BGL_Wrap(1, LoadIdentity, 			void, 		(void))
BGL_Wrap(1, LoadMatrixd, 			void, 		(GLdoubleP))
BGL_Wrap(1, LoadMatrixf, 			void, 		(GLfloatP))
BGL_Wrap(1, LoadName, 				void, 		(GLuint))
BGL_Wrap(1, LogicOp, 				void, 		(GLenum))
BGL_Wrap(6, Map1d, 					void, 		(GLenum, GLdouble, GLdouble, GLint, GLint, GLdoubleP))
BGL_Wrap(6, Map1f, 					void, 		(GLenum, GLfloat, GLfloat, GLint, GLint, GLfloatP))
BGL_Wrap(10, Map2d, 				void, 		(GLenum, GLdouble, GLdouble, GLint, GLint, GLdouble, GLdouble, GLint, GLint, GLdoubleP))
BGL_Wrap(10, Map2f, 				void, 		(GLenum, GLfloat, GLfloat, GLint, GLint, GLfloat, GLfloat, GLint, GLint, GLfloatP))
BGL_Wrap(3, MapGrid1d, 				void, 		(GLint, GLdouble, GLdouble))
BGL_Wrap(3, MapGrid1f, 				void, 		(GLint, GLfloat, GLfloat))
BGL_Wrap(6, MapGrid2d, 				void, 		(GLint, GLdouble, GLdouble, GLint, GLdouble, GLdouble))
BGL_Wrap(6, MapGrid2f, 				void, 		(GLint, GLfloat, GLfloat, GLint, GLfloat, GLfloat))
BGL_Wrap(3, Materialf, 				void, 		(GLenum, GLenum, GLfloat))
BGL_Wrap(3, Materialfv, 			void, 		(GLenum, GLenum, GLfloatP))
BGL_Wrap(3, Materiali, 				void, 		(GLenum, GLenum, GLint))
BGL_Wrap(3, Materialiv, 			void, 		(GLenum, GLenum, GLintP))
BGL_Wrap(1, MatrixMode, 			void, 		(GLenum))
BGL_Wrap(1, MultMatrixd, 			void, 		(GLdoubleP))
BGL_Wrap(1, MultMatrixf, 			void, 		(GLfloatP))
BGL_Wrap(2, NewList, 				void, 		(GLuint, GLenum))
BGL_Wrap(3, Normal3b, 				void, 		(GLbyte, GLbyte, GLbyte))
BGL_Wrap(1, Normal3bv, 				void, 		(GLbyteP))
BGL_Wrap(3, Normal3d, 				void, 		(GLdouble, GLdouble, GLdouble))
BGL_Wrap(1, Normal3dv, 				void, 		(GLdoubleP))
BGL_Wrap(3, Normal3f, 				void, 		(GLfloat, GLfloat, GLfloat))
BGL_Wrap(1, Normal3fv, 				void, 		(GLfloatP))
BGL_Wrap(3, Normal3i, 				void, 		(GLint, GLint, GLint))
BGL_Wrap(1, Normal3iv, 				void, 		(GLintP))
BGL_Wrap(3, Normal3s, 				void, 		(GLshort, GLshort, GLshort))
BGL_Wrap(1, Normal3sv, 				void, 		(GLshortP))
BGL_Wrap(6, Ortho, 					void, 		(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble))
BGL_Wrap(1, PassThrough, 			void, 		(GLfloat))
BGL_Wrap(3, PixelMapfv, 			void, 		(GLenum, GLint, GLfloatP))
BGL_Wrap(3, PixelMapuiv, 			void, 		(GLenum, GLint, GLuintP))
BGL_Wrap(3, PixelMapusv, 			void, 		(GLenum, GLint, GLushortP))
BGL_Wrap(2, PixelStoref, 			void, 		(GLenum, GLfloat))
BGL_Wrap(2, PixelStorei, 			void, 		(GLenum, GLint))
BGL_Wrap(2, PixelTransferf, 		void, 		(GLenum, GLfloat))
BGL_Wrap(2, PixelTransferi, 		void, 		(GLenum, GLint))
BGL_Wrap(2, PixelZoom, 				void, 		(GLfloat, GLfloat))
BGL_Wrap(1, PointSize, 				void, 		(GLfloat))
BGL_Wrap(2, PolygonMode, 			void, 		(GLenum, GLenum))
BGL_Wrap(2, PolygonOffset, 			void, 		(GLfloat, GLfloat))
BGL_Wrap(1, PolygonStipple, 		void, 		(GLubyteP))
BGL_Wrap(1, PopAttrib, 				void, 		(void))
BGL_Wrap(1, PopMatrix, 				void, 		(void))
BGL_Wrap(1, PopName, 				void, 		(void))
BGL_Wrap(3, PrioritizeTextures,		void,		(GLsizei, GLuintP, GLclampfP))
BGL_Wrap(1, PushAttrib, 			void, 		(GLbitfield))
BGL_Wrap(1, PushMatrix, 			void, 		(void))
BGL_Wrap(1, PushName, 				void, 		(GLuint))
BGL_Wrap(2, RasterPos2d, 			void, 		(GLdouble, GLdouble))
BGL_Wrap(1, RasterPos2dv, 			void, 		(GLdoubleP))
BGL_Wrap(2, RasterPos2f, 			void, 		(GLfloat, GLfloat))
BGL_Wrap(1, RasterPos2fv, 			void, 		(GLfloatP))
BGL_Wrap(2, RasterPos2i, 			void, 		(GLint, GLint))
BGL_Wrap(1, RasterPos2iv, 			void, 		(GLintP))
BGL_Wrap(2, RasterPos2s, 			void, 		(GLshort, GLshort))
BGL_Wrap(1, RasterPos2sv, 			void, 		(GLshortP))
BGL_Wrap(3, RasterPos3d, 			void, 		(GLdouble, GLdouble, GLdouble))
BGL_Wrap(1, RasterPos3dv, 			void, 		(GLdoubleP))
BGL_Wrap(3, RasterPos3f, 			void, 		(GLfloat, GLfloat, GLfloat))
BGL_Wrap(1, RasterPos3fv, 			void, 		(GLfloatP))
BGL_Wrap(3, RasterPos3i, 			void, 		(GLint, GLint, GLint))
BGL_Wrap(1, RasterPos3iv, 			void, 		(GLintP))
BGL_Wrap(3, RasterPos3s, 			void, 		(GLshort, GLshort, GLshort))
BGL_Wrap(1, RasterPos3sv, 			void, 		(GLshortP))
BGL_Wrap(4, RasterPos4d, 			void, 		(GLdouble, GLdouble, GLdouble, GLdouble))
BGL_Wrap(1, RasterPos4dv, 			void, 		(GLdoubleP))
BGL_Wrap(4, RasterPos4f, 			void, 		(GLfloat, GLfloat, GLfloat, GLfloat))
BGL_Wrap(1, RasterPos4fv, 			void, 		(GLfloatP))
BGL_Wrap(4, RasterPos4i, 			void, 		(GLint, GLint, GLint, GLint))
BGL_Wrap(1, RasterPos4iv, 			void, 		(GLintP))
BGL_Wrap(4, RasterPos4s, 			void, 		(GLshort, GLshort, GLshort, GLshort))
BGL_Wrap(1, RasterPos4sv, 			void, 		(GLshortP))
BGL_Wrap(1, ReadBuffer, 			void, 		(GLenum))
BGL_Wrap(7, ReadPixels, 			void, 		(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, GLvoidP))
BGL_Wrap(4, Rectd, 					void, 		(GLdouble, GLdouble, GLdouble, GLdouble))
BGL_Wrap(2, Rectdv, 				void, 		(GLdoubleP, GLdoubleP))
BGL_Wrap(4, Rectf, 					void, 		(GLfloat, GLfloat, GLfloat, GLfloat))
BGL_Wrap(2, Rectfv, 				void, 		(GLfloatP, GLfloatP))
BGL_Wrap(4, Recti, 					void, 		(GLint, GLint, GLint, GLint))
BGL_Wrap(2, Rectiv, 				void, 		(GLintP, GLintP))
BGL_Wrap(4, Rects, 					void, 		(GLshort, GLshort, GLshort, GLshort))
BGL_Wrap(2, Rectsv, 				void, 		(GLshortP, GLshortP))
BGL_Wrap(1, RenderMode, 			GLint, 		(GLenum))
BGL_Wrap(4, Rotated, 				void, 		(GLdouble, GLdouble, GLdouble, GLdouble))
BGL_Wrap(4, Rotatef, 				void, 		(GLfloat, GLfloat, GLfloat, GLfloat))
BGL_Wrap(3, Scaled, 				void, 		(GLdouble, GLdouble, GLdouble))
BGL_Wrap(3, Scalef, 				void, 		(GLfloat, GLfloat, GLfloat))
BGL_Wrap(4, Scissor, 				void, 		(GLint, GLint, GLsizei, GLsizei))
BGL_Wrap(2, SelectBuffer, 			void, 		(GLsizei, GLuintP))
BGL_Wrap(1, ShadeModel, 			void, 		(GLenum))
BGL_Wrap(3, StencilFunc, 			void, 		(GLenum, GLint, GLuint))
BGL_Wrap(1, StencilMask, 			void, 		(GLuint))
BGL_Wrap(3, StencilOp, 				void, 		(GLenum, GLenum, GLenum))
BGL_Wrap(1, TexCoord1d, 			void, 		(GLdouble))
BGL_Wrap(1, TexCoord1dv, 			void, 		(GLdoubleP))
BGL_Wrap(1, TexCoord1f, 			void, 		(GLfloat))
BGL_Wrap(1, TexCoord1fv, 			void, 		(GLfloatP))
BGL_Wrap(1, TexCoord1i, 			void, 		(GLint))
BGL_Wrap(1, TexCoord1iv, 			void, 		(GLintP))
BGL_Wrap(1, TexCoord1s, 			void, 		(GLshort))
BGL_Wrap(1, TexCoord1sv, 			void, 		(GLshortP))
BGL_Wrap(2, TexCoord2d, 			void, 		(GLdouble, GLdouble))
BGL_Wrap(1, TexCoord2dv, 			void, 		(GLdoubleP))
BGL_Wrap(2, TexCoord2f, 			void, 		(GLfloat, GLfloat))
BGL_Wrap(1, TexCoord2fv, 			void, 		(GLfloatP))
BGL_Wrap(2, TexCoord2i, 			void, 		(GLint, GLint))
BGL_Wrap(1, TexCoord2iv, 			void, 		(GLintP))
BGL_Wrap(2, TexCoord2s, 			void, 		(GLshort, GLshort))
BGL_Wrap(1, TexCoord2sv, 			void, 		(GLshortP))
BGL_Wrap(3, TexCoord3d, 			void, 		(GLdouble, GLdouble, GLdouble))
BGL_Wrap(1, TexCoord3dv, 			void, 		(GLdoubleP))
BGL_Wrap(3, TexCoord3f, 			void, 		(GLfloat, GLfloat, GLfloat))
BGL_Wrap(1, TexCoord3fv, 			void, 		(GLfloatP))
BGL_Wrap(3, TexCoord3i, 			void, 		(GLint, GLint, GLint))
BGL_Wrap(1, TexCoord3iv, 			void, 		(GLintP))
BGL_Wrap(3, TexCoord3s, 			void, 		(GLshort, GLshort, GLshort))
BGL_Wrap(1, TexCoord3sv, 			void, 		(GLshortP))
BGL_Wrap(4, TexCoord4d, 			void, 		(GLdouble, GLdouble, GLdouble, GLdouble))
BGL_Wrap(1, TexCoord4dv, 			void, 		(GLdoubleP))
BGL_Wrap(4, TexCoord4f, 			void, 		(GLfloat, GLfloat, GLfloat, GLfloat))
BGL_Wrap(1, TexCoord4fv, 			void, 		(GLfloatP))
BGL_Wrap(4, TexCoord4i, 			void, 		(GLint, GLint, GLint, GLint))
BGL_Wrap(1, TexCoord4iv, 			void, 		(GLintP))
BGL_Wrap(4, TexCoord4s, 			void, 		(GLshort, GLshort, GLshort, GLshort))
BGL_Wrap(1, TexCoord4sv, 			void, 		(GLshortP))
BGL_Wrap(3, TexEnvf, 				void, 		(GLenum, GLenum, GLfloat))
BGL_Wrap(3, TexEnvfv, 				void, 		(GLenum, GLenum, GLfloatP))
BGL_Wrap(3, TexEnvi, 				void, 		(GLenum, GLenum, GLint))
BGL_Wrap(3, TexEnviv, 				void, 		(GLenum, GLenum, GLintP))
BGL_Wrap(3, TexGend, 				void, 		(GLenum, GLenum, GLdouble))
BGL_Wrap(3, TexGendv, 				void, 		(GLenum, GLenum, GLdoubleP))
BGL_Wrap(3, TexGenf, 				void, 		(GLenum, GLenum, GLfloat))
BGL_Wrap(3, TexGenfv, 				void, 		(GLenum, GLenum, GLfloatP))
BGL_Wrap(3, TexGeni, 				void, 		(GLenum, GLenum, GLint))
BGL_Wrap(3, TexGeniv, 				void, 		(GLenum, GLenum, GLintP))
BGL_Wrap(8, TexImage1D, 			void, 		(GLenum, GLint, GLint, GLsizei, GLint, GLenum, GLenum, GLvoidP))
BGL_Wrap(9, TexImage2D, 			void, 		(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, GLvoidP))
BGL_Wrap(3, TexParameterf, 			void, 		(GLenum, GLenum, GLfloat))
BGL_Wrap(3, TexParameterfv, 		void, 		(GLenum, GLenum, GLfloatP))
BGL_Wrap(3, TexParameteri, 			void, 		(GLenum, GLenum, GLint))
BGL_Wrap(3, TexParameteriv, 		void, 		(GLenum, GLenum, GLintP))
BGL_Wrap(3, Translated, 			void, 		(GLdouble, GLdouble, GLdouble))
BGL_Wrap(3, Translatef, 			void, 		(GLfloat, GLfloat, GLfloat))
BGL_Wrap(2, Vertex2d, 				void, 		(GLdouble, GLdouble))
BGL_Wrap(1, Vertex2dv, 				void, 		(GLdoubleP))
BGL_Wrap(2, Vertex2f, 				void, 		(GLfloat, GLfloat))
BGL_Wrap(1, Vertex2fv, 				void, 		(GLfloatP))
BGL_Wrap(2, Vertex2i, 				void, 		(GLint, GLint))
BGL_Wrap(1, Vertex2iv, 				void, 		(GLintP))
BGL_Wrap(2, Vertex2s, 				void, 		(GLshort, GLshort))
BGL_Wrap(1, Vertex2sv, 				void, 		(GLshortP))
BGL_Wrap(3, Vertex3d, 				void, 		(GLdouble, GLdouble, GLdouble))
BGL_Wrap(1, Vertex3dv, 				void, 		(GLdoubleP))
BGL_Wrap(3, Vertex3f, 				void, 		(GLfloat, GLfloat, GLfloat))
BGL_Wrap(1, Vertex3fv, 				void, 		(GLfloatP))
BGL_Wrap(3, Vertex3i, 				void, 		(GLint, GLint, GLint))
BGL_Wrap(1, Vertex3iv, 				void, 		(GLintP))
BGL_Wrap(3, Vertex3s, 				void, 		(GLshort, GLshort, GLshort))
BGL_Wrap(1, Vertex3sv, 				void, 		(GLshortP))
BGL_Wrap(4, Vertex4d, 				void, 		(GLdouble, GLdouble, GLdouble, GLdouble))
BGL_Wrap(1, Vertex4dv, 				void, 		(GLdoubleP))
BGL_Wrap(4, Vertex4f, 				void, 		(GLfloat, GLfloat, GLfloat, GLfloat))
BGL_Wrap(1, Vertex4fv, 				void, 		(GLfloatP))
BGL_Wrap(4, Vertex4i, 				void, 		(GLint, GLint, GLint, GLint))
BGL_Wrap(1, Vertex4iv, 				void, 		(GLintP))
BGL_Wrap(4, Vertex4s, 				void, 		(GLshort, GLshort, GLshort, GLshort))
BGL_Wrap(1, Vertex4sv, 				void, 		(GLshortP))
BGL_Wrap(4, Viewport, 				void, 		(GLint, GLint, GLsizei, GLsizei))
#endif

/* XXX */
#undef MethodDef
#define MethodDef(func) {"gl"#func, Method_##func, METH_VARARGS}

static struct PyMethodDef BGL_methods[] = {
	{"Buffer", Method_Buffer, METH_VARARGS, Method_Buffer_doc}, 
#ifndef __APPLE__
	MethodDef( Accum),
	MethodDef( AlphaFunc),
	MethodDef( AreTexturesResident), 
	MethodDef( Begin),
	MethodDef( BindTexture), 
	MethodDef( Bitmap),
	MethodDef( BlendFunc),
	MethodDef( CallList),
	MethodDef( CallLists),
	MethodDef( Clear),
	MethodDef( ClearAccum),
	MethodDef( ClearColor),
	MethodDef( ClearDepth),
	MethodDef( ClearIndex),
	MethodDef( ClearStencil),
	MethodDef( ClipPlane),
	MethodDef( Color3b),
	MethodDef( Color3bv),
	MethodDef( Color3d),
	MethodDef( Color3dv),
	MethodDef( Color3f),
	MethodDef( Color3fv),
	MethodDef( Color3i),
	MethodDef( Color3iv),
	MethodDef( Color3s),
	MethodDef( Color3sv),
	MethodDef( Color3ub),
	MethodDef( Color3ubv),
	MethodDef( Color3ui),
	MethodDef( Color3uiv),
	MethodDef( Color3us),
	MethodDef( Color3usv),
	MethodDef( Color4b),
	MethodDef( Color4bv),
	MethodDef( Color4d),
	MethodDef( Color4dv),
	MethodDef( Color4f),
	MethodDef( Color4fv),
	MethodDef( Color4i),
	MethodDef( Color4iv),
	MethodDef( Color4s),
	MethodDef( Color4sv),
	MethodDef( Color4ub),
	MethodDef( Color4ubv),
	MethodDef( Color4ui),
	MethodDef( Color4uiv),
	MethodDef( Color4us),
	MethodDef( Color4usv),
	MethodDef( ColorMask),
	MethodDef( ColorMaterial),
	MethodDef( CopyPixels),
	MethodDef( CullFace),
	MethodDef( DeleteLists),
	MethodDef( DeleteTextures),
	MethodDef( DepthFunc),
	MethodDef( DepthMask),
	MethodDef( DepthRange),
	MethodDef( Disable),
	MethodDef( DrawBuffer),
	MethodDef( DrawPixels),
	MethodDef( EdgeFlag),
	MethodDef( EdgeFlagv),
	MethodDef( Enable),
	MethodDef( End),
	MethodDef( EndList),
	MethodDef( EvalCoord1d),
	MethodDef( EvalCoord1dv),
	MethodDef( EvalCoord1f),
	MethodDef( EvalCoord1fv),
	MethodDef( EvalCoord2d),
	MethodDef( EvalCoord2dv),
	MethodDef( EvalCoord2f),
	MethodDef( EvalCoord2fv),
	MethodDef( EvalMesh1),
	MethodDef( EvalMesh2),
	MethodDef( EvalPoint1),
	MethodDef( EvalPoint2),
	MethodDef( FeedbackBuffer),
	MethodDef( Finish),
	MethodDef( Flush),
	MethodDef( Fogf),
	MethodDef( Fogfv),
	MethodDef( Fogi),
	MethodDef( Fogiv),
	MethodDef( FrontFace),
	MethodDef( Frustum),
	MethodDef( GenLists),
	MethodDef( GenTextures), 
	MethodDef( GetBooleanv),
	MethodDef( GetClipPlane),
	MethodDef( GetDoublev),
	MethodDef( GetError),
	MethodDef( GetFloatv),
	MethodDef( GetIntegerv),
	MethodDef( GetLightfv),
	MethodDef( GetLightiv),
	MethodDef( GetMapdv),
	MethodDef( GetMapfv),
	MethodDef( GetMapiv),
	MethodDef( GetMaterialfv),
	MethodDef( GetMaterialiv),
	MethodDef( GetPixelMapfv),
	MethodDef( GetPixelMapuiv),
	MethodDef( GetPixelMapusv),
	MethodDef( GetPolygonStipple),
	MethodDef( GetString),
	MethodDef( GetTexEnvfv),
	MethodDef( GetTexEnviv),
	MethodDef( GetTexGendv),
	MethodDef( GetTexGenfv),
	MethodDef( GetTexGeniv),
	MethodDef( GetTexImage),
	MethodDef( GetTexLevelParameterfv),
	MethodDef( GetTexLevelParameteriv),
	MethodDef( GetTexParameterfv),
	MethodDef( GetTexParameteriv),
	MethodDef( Hint),
	MethodDef( IndexMask),
	MethodDef( Indexd),
	MethodDef( Indexdv),
	MethodDef( Indexf),
	MethodDef( Indexfv),
	MethodDef( Indexi),
	MethodDef( Indexiv),
	MethodDef( Indexs),
	MethodDef( Indexsv),
	MethodDef( InitNames),
	MethodDef( IsEnabled),
	MethodDef( IsList),
	MethodDef( IsTexture), 
	MethodDef( LightModelf),
	MethodDef( LightModelfv),
	MethodDef( LightModeli),
	MethodDef( LightModeliv),
	MethodDef( Lightf),
	MethodDef( Lightfv),
	MethodDef( Lighti),
	MethodDef( Lightiv),
	MethodDef( LineStipple),
	MethodDef( LineWidth),
	MethodDef( ListBase),
	MethodDef( LoadIdentity),
	MethodDef( LoadMatrixd),
	MethodDef( LoadMatrixf),
	MethodDef( LoadName),
	MethodDef( LogicOp),
	MethodDef( Map1d),
	MethodDef( Map1f),
	MethodDef( Map2d),
	MethodDef( Map2f),
	MethodDef( MapGrid1d),
	MethodDef( MapGrid1f),
	MethodDef( MapGrid2d),
	MethodDef( MapGrid2f),
	MethodDef( Materialf),
	MethodDef( Materialfv),
	MethodDef( Materiali),
	MethodDef( Materialiv),
	MethodDef( MatrixMode),
	MethodDef( MultMatrixd),
	MethodDef( MultMatrixf),
	MethodDef( NewList),
	MethodDef( Normal3b),
	MethodDef( Normal3bv),
	MethodDef( Normal3d),
	MethodDef( Normal3dv),
	MethodDef( Normal3f),
	MethodDef( Normal3fv),
	MethodDef( Normal3i),
	MethodDef( Normal3iv),
	MethodDef( Normal3s),
	MethodDef( Normal3sv),
	MethodDef( Ortho),
	MethodDef( PassThrough),
	MethodDef( PixelMapfv),
	MethodDef( PixelMapuiv),
	MethodDef( PixelMapusv),
	MethodDef( PixelStoref),
	MethodDef( PixelStorei),
	MethodDef( PixelTransferf),
	MethodDef( PixelTransferi),
	MethodDef( PixelZoom),
	MethodDef( PointSize),
	MethodDef( PolygonMode),
	MethodDef( PolygonOffset),
	MethodDef( PolygonStipple),
	MethodDef( PopAttrib),
	MethodDef( PopMatrix),
	MethodDef( PopName),
	MethodDef( PrioritizeTextures), 
	MethodDef( PushAttrib),
	MethodDef( PushMatrix),
	MethodDef( PushName),
	MethodDef( RasterPos2d),
	MethodDef( RasterPos2dv),
	MethodDef( RasterPos2f),
	MethodDef( RasterPos2fv),
	MethodDef( RasterPos2i),
	MethodDef( RasterPos2iv),
	MethodDef( RasterPos2s),
	MethodDef( RasterPos2sv),
	MethodDef( RasterPos3d),
	MethodDef( RasterPos3dv),
	MethodDef( RasterPos3f),
	MethodDef( RasterPos3fv),
	MethodDef( RasterPos3i),
	MethodDef( RasterPos3iv),
	MethodDef( RasterPos3s),
	MethodDef( RasterPos3sv),
	MethodDef( RasterPos4d),
	MethodDef( RasterPos4dv),
	MethodDef( RasterPos4f),
	MethodDef( RasterPos4fv),
	MethodDef( RasterPos4i),
	MethodDef( RasterPos4iv),
	MethodDef( RasterPos4s),
	MethodDef( RasterPos4sv),
	MethodDef( ReadBuffer),
	MethodDef( ReadPixels),
	MethodDef( Rectd),
	MethodDef( Rectdv),
	MethodDef( Rectf),
	MethodDef( Rectfv),
	MethodDef( Recti),
	MethodDef( Rectiv),
	MethodDef( Rects),
	MethodDef( Rectsv),
	MethodDef( RenderMode),
	MethodDef( Rotated),
	MethodDef( Rotatef),
	MethodDef( Scaled),
	MethodDef( Scalef),
	MethodDef( Scissor),
	MethodDef( SelectBuffer),
	MethodDef( ShadeModel),
	MethodDef( StencilFunc),
	MethodDef( StencilMask),
	MethodDef( StencilOp),
	MethodDef( TexCoord1d),
	MethodDef( TexCoord1dv),
	MethodDef( TexCoord1f),
	MethodDef( TexCoord1fv),
	MethodDef( TexCoord1i),
	MethodDef( TexCoord1iv),
	MethodDef( TexCoord1s),
	MethodDef( TexCoord1sv),
	MethodDef( TexCoord2d),
	MethodDef( TexCoord2dv),
	MethodDef( TexCoord2f),
	MethodDef( TexCoord2fv),
	MethodDef( TexCoord2i),
	MethodDef( TexCoord2iv),
	MethodDef( TexCoord2s),
	MethodDef( TexCoord2sv),
	MethodDef( TexCoord3d),
	MethodDef( TexCoord3dv),
	MethodDef( TexCoord3f),
	MethodDef( TexCoord3fv),
	MethodDef( TexCoord3i),
	MethodDef( TexCoord3iv),
	MethodDef( TexCoord3s),
	MethodDef( TexCoord3sv),
	MethodDef( TexCoord4d),
	MethodDef( TexCoord4dv),
	MethodDef( TexCoord4f),
	MethodDef( TexCoord4fv),
	MethodDef( TexCoord4i),
	MethodDef( TexCoord4iv),
	MethodDef( TexCoord4s),
	MethodDef( TexCoord4sv),
	MethodDef( TexEnvf),
	MethodDef( TexEnvfv),
	MethodDef( TexEnvi),
	MethodDef( TexEnviv),
	MethodDef( TexGend),
	MethodDef( TexGendv),
	MethodDef( TexGenf),
	MethodDef( TexGenfv),
	MethodDef( TexGeni),
	MethodDef( TexGeniv),
	MethodDef( TexImage1D),
	MethodDef( TexImage2D),
	MethodDef( TexParameterf),
	MethodDef( TexParameterfv),
	MethodDef( TexParameteri),
	MethodDef( TexParameteriv),
	MethodDef( Translated),
	MethodDef( Translatef),
	MethodDef( Vertex2d),
	MethodDef( Vertex2dv),
	MethodDef( Vertex2f),
	MethodDef( Vertex2fv),
	MethodDef( Vertex2i),
	MethodDef( Vertex2iv),
	MethodDef( Vertex2s),
	MethodDef( Vertex2sv),
	MethodDef( Vertex3d),
	MethodDef( Vertex3dv),
	MethodDef( Vertex3f),
	MethodDef( Vertex3fv),
	MethodDef( Vertex3i),
	MethodDef( Vertex3iv),
	MethodDef( Vertex3s),
	MethodDef( Vertex3sv),
	MethodDef( Vertex4d),
	MethodDef( Vertex4dv),
	MethodDef( Vertex4f),
	MethodDef( Vertex4fv),
	MethodDef( Vertex4i),
	MethodDef( Vertex4iv),
	MethodDef( Vertex4s),
	MethodDef( Vertex4sv),
	MethodDef( Viewport),
#endif

	{NULL, NULL}
};

PyObject *init_py_bgl(void) 
{
	PyObject *mod= Py_InitModule(SUBMODULE(BGL), BGL_methods);
	PyObject *dict= PyModule_GetDict(mod);

	Buffer_Type.ob_type= &PyType_Type;
	
	BPY_ADDCONST(dict, GL_CURRENT_BIT);
	BPY_ADDCONST(dict, GL_POINT_BIT);
	BPY_ADDCONST(dict, GL_LINE_BIT);
	BPY_ADDCONST(dict, GL_POLYGON_BIT);
	BPY_ADDCONST(dict, GL_POLYGON_STIPPLE_BIT);
	BPY_ADDCONST(dict, GL_PIXEL_MODE_BIT);
	BPY_ADDCONST(dict, GL_LIGHTING_BIT);
	BPY_ADDCONST(dict, GL_FOG_BIT);
	BPY_ADDCONST(dict, GL_DEPTH_BUFFER_BIT);
	BPY_ADDCONST(dict, GL_ACCUM_BUFFER_BIT);
	BPY_ADDCONST(dict, GL_STENCIL_BUFFER_BIT);
	BPY_ADDCONST(dict, GL_VIEWPORT_BIT);
	BPY_ADDCONST(dict, GL_TRANSFORM_BIT);
	BPY_ADDCONST(dict, GL_ENABLE_BIT);
	BPY_ADDCONST(dict, GL_COLOR_BUFFER_BIT);
	BPY_ADDCONST(dict, GL_HINT_BIT);
	BPY_ADDCONST(dict, GL_EVAL_BIT);
	BPY_ADDCONST(dict, GL_LIST_BIT);
	BPY_ADDCONST(dict, GL_TEXTURE_BIT);
	BPY_ADDCONST(dict, GL_SCISSOR_BIT);
	BPY_ADDCONST(dict, GL_ALL_ATTRIB_BITS);

	BPY_ADDCONST(dict, GL_FALSE);
	BPY_ADDCONST(dict, GL_TRUE);

	BPY_ADDCONST(dict, GL_POINTS);
	BPY_ADDCONST(dict, GL_LINES);
	BPY_ADDCONST(dict, GL_LINE_LOOP);
	BPY_ADDCONST(dict, GL_LINE_STRIP);
	BPY_ADDCONST(dict, GL_TRIANGLES);
	BPY_ADDCONST(dict, GL_TRIANGLE_STRIP);
	BPY_ADDCONST(dict, GL_TRIANGLE_FAN);
	BPY_ADDCONST(dict, GL_QUADS);
	BPY_ADDCONST(dict, GL_QUAD_STRIP);
	BPY_ADDCONST(dict, GL_POLYGON);

	BPY_ADDCONST(dict, GL_ACCUM);
	BPY_ADDCONST(dict, GL_LOAD);
	BPY_ADDCONST(dict, GL_RETURN);
	BPY_ADDCONST(dict, GL_MULT);
	BPY_ADDCONST(dict, GL_ADD);

	BPY_ADDCONST(dict, GL_NEVER);
	BPY_ADDCONST(dict, GL_LESS);
	BPY_ADDCONST(dict, GL_EQUAL);
	BPY_ADDCONST(dict, GL_LEQUAL);
	BPY_ADDCONST(dict, GL_GREATER);
	BPY_ADDCONST(dict, GL_NOTEQUAL);
	BPY_ADDCONST(dict, GL_GEQUAL);
	BPY_ADDCONST(dict, GL_ALWAYS);

	BPY_ADDCONST(dict, GL_ZERO);
	BPY_ADDCONST(dict, GL_ONE);
	BPY_ADDCONST(dict, GL_SRC_COLOR);
	BPY_ADDCONST(dict, GL_ONE_MINUS_SRC_COLOR);
	BPY_ADDCONST(dict, GL_SRC_ALPHA);
	BPY_ADDCONST(dict, GL_ONE_MINUS_SRC_ALPHA);
	BPY_ADDCONST(dict, GL_DST_ALPHA);
	BPY_ADDCONST(dict, GL_ONE_MINUS_DST_ALPHA);

	BPY_ADDCONST(dict, GL_DST_COLOR);
	BPY_ADDCONST(dict, GL_ONE_MINUS_DST_COLOR);
	BPY_ADDCONST(dict, GL_SRC_ALPHA_SATURATE);

	BPY_ADDCONST(dict, GL_NONE);
	BPY_ADDCONST(dict, GL_FRONT_LEFT);
	BPY_ADDCONST(dict, GL_FRONT_RIGHT);
	BPY_ADDCONST(dict, GL_BACK_LEFT);
	BPY_ADDCONST(dict, GL_BACK_RIGHT);
	BPY_ADDCONST(dict, GL_FRONT);
	BPY_ADDCONST(dict, GL_BACK);
	BPY_ADDCONST(dict, GL_LEFT);
	BPY_ADDCONST(dict, GL_RIGHT);
	BPY_ADDCONST(dict, GL_FRONT_AND_BACK);
	BPY_ADDCONST(dict, GL_AUX0);
	BPY_ADDCONST(dict, GL_AUX1);
	BPY_ADDCONST(dict, GL_AUX2);
	BPY_ADDCONST(dict, GL_AUX3);

	BPY_ADDCONST(dict, GL_NO_ERROR);
	BPY_ADDCONST(dict, GL_INVALID_ENUM);
	BPY_ADDCONST(dict, GL_INVALID_VALUE);
	BPY_ADDCONST(dict, GL_INVALID_OPERATION);
	BPY_ADDCONST(dict, GL_STACK_OVERFLOW);
	BPY_ADDCONST(dict, GL_STACK_UNDERFLOW);
	BPY_ADDCONST(dict, GL_OUT_OF_MEMORY);

	BPY_ADDCONST(dict, GL_2D);
	BPY_ADDCONST(dict, GL_3D);
	BPY_ADDCONST(dict, GL_3D_COLOR);
	BPY_ADDCONST(dict, GL_3D_COLOR_TEXTURE);
	BPY_ADDCONST(dict, GL_4D_COLOR_TEXTURE);

	BPY_ADDCONST(dict, GL_PASS_THROUGH_TOKEN);
	BPY_ADDCONST(dict, GL_POINT_TOKEN);
	BPY_ADDCONST(dict, GL_LINE_TOKEN);
	BPY_ADDCONST(dict, GL_POLYGON_TOKEN);
	BPY_ADDCONST(dict, GL_BITMAP_TOKEN);
	BPY_ADDCONST(dict, GL_DRAW_PIXEL_TOKEN);
	BPY_ADDCONST(dict, GL_COPY_PIXEL_TOKEN);
	BPY_ADDCONST(dict, GL_LINE_RESET_TOKEN);

	BPY_ADDCONST(dict, GL_EXP);
	BPY_ADDCONST(dict, GL_EXP2);

	BPY_ADDCONST(dict, GL_CW);
	BPY_ADDCONST(dict, GL_CCW);

	BPY_ADDCONST(dict, GL_COEFF);
	BPY_ADDCONST(dict, GL_ORDER);
	BPY_ADDCONST(dict, GL_DOMAIN);

	BPY_ADDCONST(dict, GL_PIXEL_MAP_I_TO_I);
	BPY_ADDCONST(dict, GL_PIXEL_MAP_S_TO_S);
	BPY_ADDCONST(dict, GL_PIXEL_MAP_I_TO_R);
	BPY_ADDCONST(dict, GL_PIXEL_MAP_I_TO_G);
	BPY_ADDCONST(dict, GL_PIXEL_MAP_I_TO_B);
	BPY_ADDCONST(dict, GL_PIXEL_MAP_I_TO_A);
	BPY_ADDCONST(dict, GL_PIXEL_MAP_R_TO_R);
	BPY_ADDCONST(dict, GL_PIXEL_MAP_G_TO_G);
	BPY_ADDCONST(dict, GL_PIXEL_MAP_B_TO_B);
	BPY_ADDCONST(dict, GL_PIXEL_MAP_A_TO_A);

	BPY_ADDCONST(dict, GL_CURRENT_COLOR);
	BPY_ADDCONST(dict, GL_CURRENT_INDEX);
	BPY_ADDCONST(dict, GL_CURRENT_NORMAL);
	BPY_ADDCONST(dict, GL_CURRENT_TEXTURE_COORDS);
	BPY_ADDCONST(dict, GL_CURRENT_RASTER_COLOR);
	BPY_ADDCONST(dict, GL_CURRENT_RASTER_INDEX);
	BPY_ADDCONST(dict, GL_CURRENT_RASTER_TEXTURE_COORDS);
	BPY_ADDCONST(dict, GL_CURRENT_RASTER_POSITION);
	BPY_ADDCONST(dict, GL_CURRENT_RASTER_POSITION_VALID);
	BPY_ADDCONST(dict, GL_CURRENT_RASTER_DISTANCE);
	BPY_ADDCONST(dict, GL_POINT_SMOOTH);
	BPY_ADDCONST(dict, GL_POINT_SIZE);
	BPY_ADDCONST(dict, GL_POINT_SIZE_RANGE);
	BPY_ADDCONST(dict, GL_POINT_SIZE_GRANULARITY);
	BPY_ADDCONST(dict, GL_LINE_SMOOTH);
	BPY_ADDCONST(dict, GL_LINE_WIDTH);
	BPY_ADDCONST(dict, GL_LINE_WIDTH_RANGE);
	BPY_ADDCONST(dict, GL_LINE_WIDTH_GRANULARITY);
	BPY_ADDCONST(dict, GL_LINE_STIPPLE);
	BPY_ADDCONST(dict, GL_LINE_STIPPLE_PATTERN);
	BPY_ADDCONST(dict, GL_LINE_STIPPLE_REPEAT);
	BPY_ADDCONST(dict, GL_LIST_MODE);
	BPY_ADDCONST(dict, GL_MAX_LIST_NESTING);
	BPY_ADDCONST(dict, GL_LIST_BASE);
	BPY_ADDCONST(dict, GL_LIST_INDEX);
	BPY_ADDCONST(dict, GL_POLYGON_MODE);
	BPY_ADDCONST(dict, GL_POLYGON_SMOOTH);
	BPY_ADDCONST(dict, GL_POLYGON_STIPPLE);
	BPY_ADDCONST(dict, GL_EDGE_FLAG);
	BPY_ADDCONST(dict, GL_CULL_FACE);
	BPY_ADDCONST(dict, GL_CULL_FACE_MODE);
	BPY_ADDCONST(dict, GL_FRONT_FACE);
	BPY_ADDCONST(dict, GL_LIGHTING);
	BPY_ADDCONST(dict, GL_LIGHT_MODEL_LOCAL_VIEWER);
	BPY_ADDCONST(dict, GL_LIGHT_MODEL_TWO_SIDE);
	BPY_ADDCONST(dict, GL_LIGHT_MODEL_AMBIENT);
	BPY_ADDCONST(dict, GL_SHADE_MODEL);
	BPY_ADDCONST(dict, GL_COLOR_MATERIAL_FACE);
	BPY_ADDCONST(dict, GL_COLOR_MATERIAL_PARAMETER);
	BPY_ADDCONST(dict, GL_COLOR_MATERIAL);
	BPY_ADDCONST(dict, GL_FOG);
	BPY_ADDCONST(dict, GL_FOG_INDEX);
	BPY_ADDCONST(dict, GL_FOG_DENSITY);
	BPY_ADDCONST(dict, GL_FOG_START);
	BPY_ADDCONST(dict, GL_FOG_END);
	BPY_ADDCONST(dict, GL_FOG_MODE);
	BPY_ADDCONST(dict, GL_FOG_COLOR);
	BPY_ADDCONST(dict, GL_DEPTH_RANGE);
	BPY_ADDCONST(dict, GL_DEPTH_TEST);
	BPY_ADDCONST(dict, GL_DEPTH_WRITEMASK);
	BPY_ADDCONST(dict, GL_DEPTH_CLEAR_VALUE);
	BPY_ADDCONST(dict, GL_DEPTH_FUNC);
	BPY_ADDCONST(dict, GL_ACCUM_CLEAR_VALUE);
	BPY_ADDCONST(dict, GL_STENCIL_TEST);
	BPY_ADDCONST(dict, GL_STENCIL_CLEAR_VALUE);
	BPY_ADDCONST(dict, GL_STENCIL_FUNC);
	BPY_ADDCONST(dict, GL_STENCIL_VALUE_MASK);
	BPY_ADDCONST(dict, GL_STENCIL_FAIL);
	BPY_ADDCONST(dict, GL_STENCIL_PASS_DEPTH_FAIL);
	BPY_ADDCONST(dict, GL_STENCIL_PASS_DEPTH_PASS);
	BPY_ADDCONST(dict, GL_STENCIL_REF);
	BPY_ADDCONST(dict, GL_STENCIL_WRITEMASK);
	BPY_ADDCONST(dict, GL_MATRIX_MODE);
	BPY_ADDCONST(dict, GL_NORMALIZE);
	BPY_ADDCONST(dict, GL_VIEWPORT);
	BPY_ADDCONST(dict, GL_MODELVIEW_STACK_DEPTH);
	BPY_ADDCONST(dict, GL_PROJECTION_STACK_DEPTH);
	BPY_ADDCONST(dict, GL_TEXTURE_STACK_DEPTH);
	BPY_ADDCONST(dict, GL_MODELVIEW_MATRIX);
	BPY_ADDCONST(dict, GL_PROJECTION_MATRIX);
	BPY_ADDCONST(dict, GL_TEXTURE_MATRIX);
	BPY_ADDCONST(dict, GL_ATTRIB_STACK_DEPTH);
	BPY_ADDCONST(dict, GL_ALPHA_TEST);
	BPY_ADDCONST(dict, GL_ALPHA_TEST_FUNC);
	BPY_ADDCONST(dict, GL_ALPHA_TEST_REF);
	BPY_ADDCONST(dict, GL_DITHER);
	BPY_ADDCONST(dict, GL_BLEND_DST);
	BPY_ADDCONST(dict, GL_BLEND_SRC);
	BPY_ADDCONST(dict, GL_BLEND);
	BPY_ADDCONST(dict, GL_LOGIC_OP_MODE);
	BPY_ADDCONST(dict, GL_LOGIC_OP);
	BPY_ADDCONST(dict, GL_AUX_BUFFERS);
	BPY_ADDCONST(dict, GL_DRAW_BUFFER);
	BPY_ADDCONST(dict, GL_READ_BUFFER);
	BPY_ADDCONST(dict, GL_SCISSOR_BOX);
	BPY_ADDCONST(dict, GL_SCISSOR_TEST);
	BPY_ADDCONST(dict, GL_INDEX_CLEAR_VALUE);
	BPY_ADDCONST(dict, GL_INDEX_WRITEMASK);
	BPY_ADDCONST(dict, GL_COLOR_CLEAR_VALUE);
	BPY_ADDCONST(dict, GL_COLOR_WRITEMASK);
	BPY_ADDCONST(dict, GL_INDEX_MODE);
	BPY_ADDCONST(dict, GL_RGBA_MODE);
	BPY_ADDCONST(dict, GL_DOUBLEBUFFER);
	BPY_ADDCONST(dict, GL_STEREO);
	BPY_ADDCONST(dict, GL_RENDER_MODE);
	BPY_ADDCONST(dict, GL_PERSPECTIVE_CORRECTION_HINT);
	BPY_ADDCONST(dict, GL_POINT_SMOOTH_HINT);
	BPY_ADDCONST(dict, GL_LINE_SMOOTH_HINT);
	BPY_ADDCONST(dict, GL_POLYGON_SMOOTH_HINT);
	BPY_ADDCONST(dict, GL_FOG_HINT);
	BPY_ADDCONST(dict, GL_TEXTURE_GEN_S);
	BPY_ADDCONST(dict, GL_TEXTURE_GEN_T);
	BPY_ADDCONST(dict, GL_TEXTURE_GEN_R);
	BPY_ADDCONST(dict, GL_TEXTURE_GEN_Q);
	BPY_ADDCONST(dict, GL_PIXEL_MAP_I_TO_I_SIZE);
	BPY_ADDCONST(dict, GL_PIXEL_MAP_S_TO_S_SIZE);
	BPY_ADDCONST(dict, GL_PIXEL_MAP_I_TO_R_SIZE);
	BPY_ADDCONST(dict, GL_PIXEL_MAP_I_TO_G_SIZE);
	BPY_ADDCONST(dict, GL_PIXEL_MAP_I_TO_B_SIZE);
	BPY_ADDCONST(dict, GL_PIXEL_MAP_I_TO_A_SIZE);
	BPY_ADDCONST(dict, GL_PIXEL_MAP_R_TO_R_SIZE);
	BPY_ADDCONST(dict, GL_PIXEL_MAP_G_TO_G_SIZE);
	BPY_ADDCONST(dict, GL_PIXEL_MAP_B_TO_B_SIZE);
	BPY_ADDCONST(dict, GL_PIXEL_MAP_A_TO_A_SIZE);
	BPY_ADDCONST(dict, GL_UNPACK_SWAP_BYTES);
	BPY_ADDCONST(dict, GL_UNPACK_LSB_FIRST);
	BPY_ADDCONST(dict, GL_UNPACK_ROW_LENGTH);
	BPY_ADDCONST(dict, GL_UNPACK_SKIP_ROWS);
	BPY_ADDCONST(dict, GL_UNPACK_SKIP_PIXELS);
	BPY_ADDCONST(dict, GL_UNPACK_ALIGNMENT);
	BPY_ADDCONST(dict, GL_PACK_SWAP_BYTES);
	BPY_ADDCONST(dict, GL_PACK_LSB_FIRST);
	BPY_ADDCONST(dict, GL_PACK_ROW_LENGTH);
	BPY_ADDCONST(dict, GL_PACK_SKIP_ROWS);
	BPY_ADDCONST(dict, GL_PACK_SKIP_PIXELS);
	BPY_ADDCONST(dict, GL_PACK_ALIGNMENT);
	BPY_ADDCONST(dict, GL_MAP_COLOR);
	BPY_ADDCONST(dict, GL_MAP_STENCIL);
	BPY_ADDCONST(dict, GL_INDEX_SHIFT);
	BPY_ADDCONST(dict, GL_INDEX_OFFSET);
	BPY_ADDCONST(dict, GL_RED_SCALE);
	BPY_ADDCONST(dict, GL_RED_BIAS);
	BPY_ADDCONST(dict, GL_ZOOM_X);
	BPY_ADDCONST(dict, GL_ZOOM_Y);
	BPY_ADDCONST(dict, GL_GREEN_SCALE);
	BPY_ADDCONST(dict, GL_GREEN_BIAS);
	BPY_ADDCONST(dict, GL_BLUE_SCALE);
	BPY_ADDCONST(dict, GL_BLUE_BIAS);
	BPY_ADDCONST(dict, GL_ALPHA_SCALE);
	BPY_ADDCONST(dict, GL_ALPHA_BIAS);
	BPY_ADDCONST(dict, GL_DEPTH_SCALE);
	BPY_ADDCONST(dict, GL_DEPTH_BIAS);
	BPY_ADDCONST(dict, GL_MAX_EVAL_ORDER);
	BPY_ADDCONST(dict, GL_MAX_LIGHTS);
	BPY_ADDCONST(dict, GL_MAX_CLIP_PLANES);
	BPY_ADDCONST(dict, GL_MAX_TEXTURE_SIZE);
	BPY_ADDCONST(dict, GL_MAX_PIXEL_MAP_TABLE);
	BPY_ADDCONST(dict, GL_MAX_ATTRIB_STACK_DEPTH);
	BPY_ADDCONST(dict, GL_MAX_MODELVIEW_STACK_DEPTH);
	BPY_ADDCONST(dict, GL_MAX_NAME_STACK_DEPTH);
	BPY_ADDCONST(dict, GL_MAX_PROJECTION_STACK_DEPTH);
	BPY_ADDCONST(dict, GL_MAX_TEXTURE_STACK_DEPTH);
	BPY_ADDCONST(dict, GL_MAX_VIEWPORT_DIMS);
	BPY_ADDCONST(dict, GL_SUBPIXEL_BITS);
	BPY_ADDCONST(dict, GL_INDEX_BITS);
	BPY_ADDCONST(dict, GL_RED_BITS);
	BPY_ADDCONST(dict, GL_GREEN_BITS);
	BPY_ADDCONST(dict, GL_BLUE_BITS);
	BPY_ADDCONST(dict, GL_ALPHA_BITS);
	BPY_ADDCONST(dict, GL_DEPTH_BITS);
	BPY_ADDCONST(dict, GL_STENCIL_BITS);
	BPY_ADDCONST(dict, GL_ACCUM_RED_BITS);
	BPY_ADDCONST(dict, GL_ACCUM_GREEN_BITS);
	BPY_ADDCONST(dict, GL_ACCUM_BLUE_BITS);
	BPY_ADDCONST(dict, GL_ACCUM_ALPHA_BITS);
	BPY_ADDCONST(dict, GL_NAME_STACK_DEPTH);
	BPY_ADDCONST(dict, GL_AUTO_NORMAL);
	BPY_ADDCONST(dict, GL_MAP1_COLOR_4);
	BPY_ADDCONST(dict, GL_MAP1_INDEX);
	BPY_ADDCONST(dict, GL_MAP1_NORMAL);
	BPY_ADDCONST(dict, GL_MAP1_TEXTURE_COORD_1);
	BPY_ADDCONST(dict, GL_MAP1_TEXTURE_COORD_2);
	BPY_ADDCONST(dict, GL_MAP1_TEXTURE_COORD_3);
	BPY_ADDCONST(dict, GL_MAP1_TEXTURE_COORD_4);
	BPY_ADDCONST(dict, GL_MAP1_VERTEX_3);
	BPY_ADDCONST(dict, GL_MAP1_VERTEX_4);
	BPY_ADDCONST(dict, GL_MAP2_COLOR_4);
	BPY_ADDCONST(dict, GL_MAP2_INDEX);
	BPY_ADDCONST(dict, GL_MAP2_NORMAL);
	BPY_ADDCONST(dict, GL_MAP2_TEXTURE_COORD_1);
	BPY_ADDCONST(dict, GL_MAP2_TEXTURE_COORD_2);
	BPY_ADDCONST(dict, GL_MAP2_TEXTURE_COORD_3);
	BPY_ADDCONST(dict, GL_MAP2_TEXTURE_COORD_4);
	BPY_ADDCONST(dict, GL_MAP2_VERTEX_3);
	BPY_ADDCONST(dict, GL_MAP2_VERTEX_4);
	BPY_ADDCONST(dict, GL_MAP1_GRID_DOMAIN);
	BPY_ADDCONST(dict, GL_MAP1_GRID_SEGMENTS);
	BPY_ADDCONST(dict, GL_MAP2_GRID_DOMAIN);
	BPY_ADDCONST(dict, GL_MAP2_GRID_SEGMENTS);
	BPY_ADDCONST(dict, GL_TEXTURE_1D);
	BPY_ADDCONST(dict, GL_TEXTURE_2D);

	BPY_ADDCONST(dict, GL_TEXTURE_WIDTH);
	BPY_ADDCONST(dict, GL_TEXTURE_HEIGHT);
	BPY_ADDCONST(dict, GL_TEXTURE_COMPONENTS);
	BPY_ADDCONST(dict, GL_TEXTURE_BORDER_COLOR);
	BPY_ADDCONST(dict, GL_TEXTURE_BORDER);

	BPY_ADDCONST(dict, GL_DONT_CARE);
	BPY_ADDCONST(dict, GL_FASTEST);
	BPY_ADDCONST(dict, GL_NICEST);

	BPY_ADDCONST(dict, GL_AMBIENT);
	BPY_ADDCONST(dict, GL_DIFFUSE);
	BPY_ADDCONST(dict, GL_SPECULAR);
	BPY_ADDCONST(dict, GL_POSITION);
	BPY_ADDCONST(dict, GL_SPOT_DIRECTION);
	BPY_ADDCONST(dict, GL_SPOT_EXPONENT);
	BPY_ADDCONST(dict, GL_SPOT_CUTOFF);
	BPY_ADDCONST(dict, GL_CONSTANT_ATTENUATION);
	BPY_ADDCONST(dict, GL_LINEAR_ATTENUATION);
	BPY_ADDCONST(dict, GL_QUADRATIC_ATTENUATION);

	BPY_ADDCONST(dict, GL_COMPILE);
	BPY_ADDCONST(dict, GL_COMPILE_AND_EXECUTE);

	BPY_ADDCONST(dict, GL_BYTE);
	BPY_ADDCONST(dict, GL_UNSIGNED_BYTE);
	BPY_ADDCONST(dict, GL_SHORT);
	BPY_ADDCONST(dict, GL_UNSIGNED_SHORT);
	BPY_ADDCONST(dict, GL_INT);
	BPY_ADDCONST(dict, GL_UNSIGNED_INT);
	BPY_ADDCONST(dict, GL_FLOAT);
	BPY_ADDCONST(dict, GL_2_BYTES);
	BPY_ADDCONST(dict, GL_3_BYTES);
	BPY_ADDCONST(dict, GL_4_BYTES);

	BPY_ADDCONST(dict, GL_CLEAR);
	BPY_ADDCONST(dict, GL_AND);
	BPY_ADDCONST(dict, GL_AND_REVERSE);
	BPY_ADDCONST(dict, GL_COPY);
	BPY_ADDCONST(dict, GL_AND_INVERTED);
	BPY_ADDCONST(dict, GL_NOOP);
	BPY_ADDCONST(dict, GL_XOR);
	BPY_ADDCONST(dict, GL_OR);
	BPY_ADDCONST(dict, GL_NOR);
	BPY_ADDCONST(dict, GL_EQUIV);
	BPY_ADDCONST(dict, GL_INVERT);
	BPY_ADDCONST(dict, GL_OR_REVERSE);
	BPY_ADDCONST(dict, GL_COPY_INVERTED);
	BPY_ADDCONST(dict, GL_OR_INVERTED);
	BPY_ADDCONST(dict, GL_NAND);
	BPY_ADDCONST(dict, GL_SET);

	BPY_ADDCONST(dict, GL_EMISSION);
	BPY_ADDCONST(dict, GL_SHININESS);
	BPY_ADDCONST(dict, GL_AMBIENT_AND_DIFFUSE);
	BPY_ADDCONST(dict, GL_COLOR_INDEXES);

	BPY_ADDCONST(dict, GL_MODELVIEW);
	BPY_ADDCONST(dict, GL_PROJECTION);
	BPY_ADDCONST(dict, GL_TEXTURE);

	BPY_ADDCONST(dict, GL_COLOR);
	BPY_ADDCONST(dict, GL_DEPTH);
	BPY_ADDCONST(dict, GL_STENCIL);

	BPY_ADDCONST(dict, GL_COLOR_INDEX);
	BPY_ADDCONST(dict, GL_STENCIL_INDEX);
	BPY_ADDCONST(dict, GL_DEPTH_COMPONENT);
	BPY_ADDCONST(dict, GL_RED);
	BPY_ADDCONST(dict, GL_GREEN);
	BPY_ADDCONST(dict, GL_BLUE);
	BPY_ADDCONST(dict, GL_ALPHA);
	BPY_ADDCONST(dict, GL_RGB);
	BPY_ADDCONST(dict, GL_RGBA);
	BPY_ADDCONST(dict, GL_LUMINANCE);
	BPY_ADDCONST(dict, GL_LUMINANCE_ALPHA);

	BPY_ADDCONST(dict, GL_BITMAP);

	BPY_ADDCONST(dict, GL_POINT);
	BPY_ADDCONST(dict, GL_LINE);
	BPY_ADDCONST(dict, GL_FILL);

	BPY_ADDCONST(dict, GL_RENDER);
	BPY_ADDCONST(dict, GL_FEEDBACK);
	BPY_ADDCONST(dict, GL_SELECT);

	BPY_ADDCONST(dict, GL_FLAT);
	BPY_ADDCONST(dict, GL_SMOOTH);

	BPY_ADDCONST(dict, GL_KEEP);
	BPY_ADDCONST(dict, GL_REPLACE);
	BPY_ADDCONST(dict, GL_INCR);
	BPY_ADDCONST(dict, GL_DECR);

	BPY_ADDCONST(dict, GL_VENDOR);
	BPY_ADDCONST(dict, GL_RENDERER);
	BPY_ADDCONST(dict, GL_VERSION);
	BPY_ADDCONST(dict, GL_EXTENSIONS);

	BPY_ADDCONST(dict, GL_S);
	BPY_ADDCONST(dict, GL_T);
	BPY_ADDCONST(dict, GL_R);
	BPY_ADDCONST(dict, GL_Q);

	BPY_ADDCONST(dict, GL_MODULATE);
	BPY_ADDCONST(dict, GL_DECAL);

	BPY_ADDCONST(dict, GL_TEXTURE_ENV_MODE);
	BPY_ADDCONST(dict, GL_TEXTURE_ENV_COLOR);

	BPY_ADDCONST(dict, GL_TEXTURE_ENV);

	BPY_ADDCONST(dict, GL_EYE_LINEAR);
	BPY_ADDCONST(dict, GL_OBJECT_LINEAR);
	BPY_ADDCONST(dict, GL_SPHERE_MAP);

	BPY_ADDCONST(dict, GL_TEXTURE_GEN_MODE);
	BPY_ADDCONST(dict, GL_OBJECT_PLANE);
	BPY_ADDCONST(dict, GL_EYE_PLANE);

	BPY_ADDCONST(dict, GL_NEAREST);
	BPY_ADDCONST(dict, GL_LINEAR);

	BPY_ADDCONST(dict, GL_NEAREST_MIPMAP_NEAREST);
	BPY_ADDCONST(dict, GL_LINEAR_MIPMAP_NEAREST);
	BPY_ADDCONST(dict, GL_NEAREST_MIPMAP_LINEAR);
	BPY_ADDCONST(dict, GL_LINEAR_MIPMAP_LINEAR);

	BPY_ADDCONST(dict, GL_TEXTURE_MAG_FILTER);
	BPY_ADDCONST(dict, GL_TEXTURE_MIN_FILTER);
	BPY_ADDCONST(dict, GL_TEXTURE_WRAP_S);
	BPY_ADDCONST(dict, GL_TEXTURE_WRAP_T);

	BPY_ADDCONST(dict, GL_CLAMP);
	BPY_ADDCONST(dict, GL_REPEAT);

	BPY_ADDCONST(dict, GL_CLIP_PLANE0);
	BPY_ADDCONST(dict, GL_CLIP_PLANE1);
	BPY_ADDCONST(dict, GL_CLIP_PLANE2);
	BPY_ADDCONST(dict, GL_CLIP_PLANE3);
	BPY_ADDCONST(dict, GL_CLIP_PLANE4);
	BPY_ADDCONST(dict, GL_CLIP_PLANE5);

	BPY_ADDCONST(dict, GL_LIGHT0);
	BPY_ADDCONST(dict, GL_LIGHT1);
	BPY_ADDCONST(dict, GL_LIGHT2);
	BPY_ADDCONST(dict, GL_LIGHT3);
	BPY_ADDCONST(dict, GL_LIGHT4);
	BPY_ADDCONST(dict, GL_LIGHT5);
	BPY_ADDCONST(dict, GL_LIGHT6);
	BPY_ADDCONST(dict, GL_LIGHT7);
	
	BPY_ADDCONST(dict, GL_POLYGON_OFFSET_UNITS);
	BPY_ADDCONST(dict, GL_POLYGON_OFFSET_POINT);
	BPY_ADDCONST(dict, GL_POLYGON_OFFSET_LINE);
	BPY_ADDCONST(dict, GL_POLYGON_OFFSET_FILL);
	BPY_ADDCONST(dict, GL_POLYGON_OFFSET_FACTOR);
	
	BPY_ADDCONST(dict, GL_TEXTURE_PRIORITY);
	BPY_ADDCONST(dict, GL_TEXTURE_RESIDENT);
	BPY_ADDCONST(dict, GL_TEXTURE_BINDING_1D);
	BPY_ADDCONST(dict, GL_TEXTURE_BINDING_2D);
			
	return mod;
}

void initDraw(void) 
{
	init_py_draw();
	init_py_bgl();
}
