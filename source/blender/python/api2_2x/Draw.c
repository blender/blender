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

/* This file is the Blender.Draw part of opy_draw.c, from the old
 * bpython/intern dir, with minor changes to adapt it to the new Python
 * implementation.  Non-trivial original comments are marked with an
 * @ symbol at their beginning. */

#include "Draw.h"

/* declared in ../BPY_extern.h,
 * used to control global dictionary persistence: */
extern short EXPP_releaseGlobalDict;

static void Button_dealloc(PyObject *self)
{
	Button *but = (Button*)self;

	if(but->type == 3) MEM_freeN (but->val.asstr);
		
	PyObject_DEL (self);	
}

static PyObject *Button_getattr(PyObject *self, char *name)
{
	Button *but = (Button*)self;
	
	if(strcmp(name, "val") == 0) {
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

static int Button_setattr(PyObject *self,  char *name, PyObject *v)
{
	Button *but= (Button*) self;
	
	if(strcmp(name, "val") == 0) {
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

static PyObject *Button_repr(PyObject *self)
{
	return PyObject_Repr(Button_getattr(self, "val"));	
}

static Button *newbutton (void)
{
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
	PyObject *result = PyObject_CallObject (callback, args);

	if (result == NULL) {
		st->text->compiled = NULL;
		PyErr_Print ();
		exit_pydraw (st);
	}

	Py_XDECREF (result);
	Py_DECREF (args);
}

/* BPY_spacetext_do_pywin_draw, the static spacetext_do_pywin_buttons and
 * BPY_spacetext_do_pywin_event are the three functions responsible for
 * calling the draw, buttons and event callbacks registered with Draw.Register
 * (see Method_Register below).  They are called (only the two BPY_ ones)
 * from blender/src/drawtext.c */

void BPY_spacetext_do_pywin_draw(SpaceText *st) 
{
	uiBlock *block;
	char butblock[20];

	sprintf(butblock, "win %d", curarea->win);
	block= uiNewBlock(&curarea->uiblocks, butblock, UI_EMBOSSX,
									UI_HELV, curarea->win);
	
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

static void spacetext_do_pywin_buttons(SpaceText *st, unsigned short event)
{
	if (st->py_button) {
		exec_callback(st, st->py_button, Py_BuildValue("(i)", event));
	}
}

void BPY_spacetext_do_pywin_event(SpaceText *st, unsigned short event, short val)
{
	if (event == QKEY && G.qual & (LR_ALTKEY|LR_CTRLKEY|LR_SHIFTKEY)) {
		exit_pydraw(st);
		return;
	}

	if (val) {
		if (uiDoBlocks(&curarea->uiblocks, event)!=UI_NOTHING ) event= 0;

		if (event==UI_BUT_EVENT)
			spacetext_do_pywin_buttons(st, val);
	}
		
	if (st->py_event)
		exec_callback(st, st->py_event, Py_BuildValue("(ii)", event, val));
}

int BPY_spacetext_is_pywin(SpaceText *st)
{
	return (st->py_draw || st->py_event || st->py_button);
}

/* the define CLEAR_NAMESPACE is currently ignored.  It should be
 * substituted by a better method, that was also the intention of the
 * programmer(s) who put it there. */

static PyObject *Method_Exit (PyObject *self, PyObject *args)
{	
	SpaceText *st= curarea->spacedata.first;

	if (!PyArg_ParseTuple(args, ""))
					return EXPP_ReturnPyObjError (PyExc_AttributeError,
									"expected empty argument list");

	exit_pydraw(st);

	if (EXPP_releaseGlobalDict) {
		PyObject *d = st->py_globaldict;
		/* d is the current window's global namespace dictionary */
	  if (d) {
		  PyDict_Clear(d);
		  Py_DECREF(d); /* release dictionary */
	  }
	}	

	return EXPP_incr_ret (Py_None);
}

static PyObject *Method_Register (PyObject *self, PyObject *args)
{
	PyObject *newdrawc= NULL, *neweventc= NULL, *newbuttonc= NULL;
	SpaceText *st= curarea->spacedata.first;
	
	if (!PyArg_ParseTuple(args, "O|OO", &newdrawc,
													&neweventc, &newbuttonc))
			return EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected one or three PyObjects");

	/*@This is a hack again:
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

/*	EXPP_debug(("--- disable clear namespace")); */

	st->flags &= ~ST_CLEAR_NAMESPACE;


	if (!PyCallable_Check(newdrawc))   newdrawc   = NULL;
	if (!PyCallable_Check(neweventc))  neweventc  = NULL;
	if (!PyCallable_Check(newbuttonc)) newbuttonc = NULL;

	if (!(newdrawc || neweventc || newbuttonc))
		return EXPP_incr_ret(Py_None);

	exit_pydraw(st);

	Py_XINCREF(newdrawc);
	Py_XINCREF(neweventc);
	Py_XINCREF(newbuttonc);

	st->py_draw= newdrawc;
	st->py_event= neweventc;
	st->py_button= newbuttonc;

	scrarea_queue_redraw(st->area);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Method_Redraw (PyObject *self,  PyObject *args)
{
	int after= 0;
	
	if (!PyArg_ParseTuple(args, "|i", &after))
			return EXPP_ReturnPyObjError (PyExc_TypeError,
							"expected int argument (or nothing)");

	if (after) addafterqueue(curarea->win, REDRAW, 1);
	else scrarea_queue_winredraw(curarea);
	
	return EXPP_incr_ret(Py_None);
}

static PyObject *Method_Draw (PyObject *self,  PyObject *args)
{
	/*@ If forced drawing is disable queue a redraw event instead */
	if (EXPP_disable_force_draw) {
		scrarea_queue_winredraw(curarea);
		return EXPP_incr_ret (Py_None);
	}

	if (!PyArg_ParseTuple(args, ""))
					return EXPP_ReturnPyObjError (PyExc_AttributeError,
										"expected empty argument list");

	scrarea_do_windraw(curarea);

	screen_swapbuffers();

	return EXPP_incr_ret (Py_None);
}

static PyObject *Method_Create (PyObject *self,  PyObject *args)
{
	Button *but;
	PyObject *in;

	if (!PyArg_ParseTuple(args, "O", &in))
					return EXPP_ReturnPyObjError (PyExc_TypeError,
									"expected PyObject argument");
	
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

static PyObject *Method_Button (PyObject *self,  PyObject *args)
{
	uiBlock *block;
	char *name, *tip= NULL;
	int event;
	int x, y, w, h;
	
	if (!PyArg_ParseTuple(args, "siiiii|s", &name, &event,
													&x, &y, &w, &h, &tip))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
		 "expected a string, five ints and optionally another string as arguments");
	
	block= Get_uiBlock();

	if(block) uiDefBut(block, BUT, event, name, x, y, w, h,
									0, 0, 0, 0, 0, tip);
	
	return EXPP_incr_ret(Py_None);
}

static PyObject *Method_Menu (PyObject *self,  PyObject *args)
{
	uiBlock *block;
	char *name, *tip= NULL;
	int event, def;
	int x, y, w, h;
	Button *but;
	
	if (!PyArg_ParseTuple(args, "siiiiii|s", &name, &event,
													&x, &y, &w, &h, &def, &tip))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected a string, six ints and optionally another string as arguments");

	but= newbutton();
	but->type= 1;
	but->val.asint= def;
	
	block= Get_uiBlock();
	if(block) uiDefButI(block, MENU, event, name, x, y, w, h,
									&but->val.asint, 0, 0, 0, 0, tip);
	
	return (PyObject *) but;
}

static PyObject *Method_Toggle (PyObject *self,  PyObject *args)
{
	uiBlock *block;
	char *name, *tip= NULL;
	int event;
	int x, y, w, h, def;
	Button *but;
	
	if (!PyArg_ParseTuple(args, "siiiiii|s", &name, &event,
													&x, &y, &w, &h, &def, &tip))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected a string, six ints and optionally another string as arguments");

	but= newbutton();
	but->type= 1;
	but->val.asint= def;
	
	block= Get_uiBlock();
	if(block) uiDefButI(block, TOG, event, name, x, y, w, h,
									&but->val.asint, 0, 0, 0, 0, tip);
	
	return (PyObject *) but;
}

/*@DO NOT TOUCH THIS FUNCTION !
   Redrawing a slider inside its own callback routine is actually forbidden
   with the current toolkit architecture (button routines are not reentrant).
   But it works anyway.
   XXX This is condemned to be dinosource in future - it's a hack.
   */

static void py_slider_update(void *butv, void *data2_unused) 
{
	uiBut *but= butv;

	EXPP_disable_force_draw= 1;
		/*@
		Disable forced drawing, otherwise the button object which
		is still being used might be deleted 
		*/

/*@	UIfrontbuf = 0; 
  	spacetext_do_pywin_buttons(curarea->spacedata.first, but->retval); */

	g_window_redrawn = 0;
	curarea->win_swap= WIN_BACK_OK; 
	UIfrontbuf = 1; 
	spacetext_do_pywin_buttons(curarea->spacedata.first, uiButGetRetVal(but));
	UIfrontbuf = 0;

	if (!g_window_redrawn) /*@ if Redraw already called */
		M_Window_Redraw(0, Py_BuildValue("(i)", SPACE_VIEW3D));

	EXPP_disable_force_draw= 0;
}

static PyObject *Method_Slider (PyObject *self,  PyObject *args)
{
	uiBlock *block;
	char *name, *tip= NULL;
	int event;
	int x, y, w, h, realtime=1;
	Button *but;
	PyObject *mino, *maxo, *inio;
	
	if (!PyArg_ParseTuple(args, "siiiiiOOO|is", &name, &event,
													&x, &y, &w, &h, &inio, &mino, &maxo, &realtime, &tip))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected a string, five ints, three PyObjects\n\
			and optionally another int and string as arguments");

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
			ubut= uiDefButF(block, NUMSLI, event, name, x, y, w, h,
											&but->val.asfloat, min, max, 0, 0, tip);
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
			ubut= uiDefButI(block, NUMSLI, event, name, x, y, w, h,
											&but->val.asint, min, max, 0, 0, tip);
			if (realtime) uiButSetFunc(ubut, py_slider_update, ubut, NULL);
		}
	}
	return (PyObject *) but;
}

static PyObject *Method_Scrollbar (PyObject *self,  PyObject *args)
{
	char *tip= NULL;
	uiBlock *block;
	int event;
	int x, y, w, h, realtime=1;
	Button *but;
	PyObject *mino, *maxo, *inio;
	float ini, min, max;

	if (!PyArg_ParseTuple(args, "iiiiiOOO|is", &event, &x, &y, &w, &h,
													&inio, &mino, &maxo, &realtime, &tip))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected five ints, three PyObjects and optionally\n\
			another int and string as arguments");

	if (!PyNumber_Check(inio) || !PyNumber_Check(inio) || !PyNumber_Check(inio))
		return EXPP_ReturnPyObjError (PyExc_AttributeError,
										"expected numbers for initial, min, and max");

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
			ubut= uiDefButF(block, SCROLL, event, "", x, y, w, h,
											&but->val.asfloat, min, max, 0, 0, tip);
			if (realtime) uiButSetFunc(ubut, py_slider_update, ubut, NULL);
		}
	} else {
		but->val.asint= ini;
		block= Get_uiBlock();
		if(block) {
			uiBut *ubut;
			ubut= uiDefButI(block, SCROLL, event, "", x, y, w, h,
											&but->val.asint, min, max, 0, 0, tip);
			if (realtime) uiButSetFunc(ubut, py_slider_update, ubut, NULL);
		}
	}

	return (PyObject *) but;
}

static PyObject *Method_Number (PyObject *self,  PyObject *args)
{
	uiBlock *block;
	char *name, *tip= NULL;
	int event;
	int x, y, w, h;
	Button *but;
	PyObject *mino, *maxo, *inio;
	
	if (!PyArg_ParseTuple(args, "siiiiiOOO|s", &name, &event,
													&x, &y, &w, &h, &inio, &mino, &maxo, &tip))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected a string, five ints, three PyObjects and\n\
			optionally another string as arguments");

	but= newbutton();
	
	if (PyFloat_Check(inio)) {
		float ini, min, max;

		ini= PyFloat_AsDouble(inio);
		min= PyFloat_AsDouble(mino);
		max= PyFloat_AsDouble(maxo);
				
		but->type= 2;
		but->val.asfloat= ini;
	
		block= Get_uiBlock();
		if(block) uiDefButF(block, NUM, event, name, x, y, w, h,
										&but->val.asfloat, min, max, 0, 0, tip);
	} else {
		int ini, min, max;

		ini= PyInt_AsLong(inio);
		min= PyInt_AsLong(mino);
		max= PyInt_AsLong(maxo);
		
		but->type= 1;
		but->val.asint= ini;
	
		block= Get_uiBlock();
		if(block) uiDefButI(block, NUM, event, name, x, y, w, h,
										&but->val.asint, min, max, 0, 0, tip);
	}

	return (PyObject *) but;
}

static PyObject *Method_String (PyObject *self,  PyObject *args)
{
	uiBlock *block;
	char *name, *tip= NULL, *newstr;
	int event;
	int x, y, w, h, len;
	Button *but;
	
	if (!PyArg_ParseTuple(args, "siiiiisi|s", &name, &event,
													&x, &y, &w, &h, &newstr, &len, &tip))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected a string, five ints, a string, an int and\n\
			optionally another string as arguments");

	but= newbutton();
	but->type= 3;
	but->slen= len;
	but->val.asstr= MEM_mallocN(len+1, "button string");
	
	strncpy(but->val.asstr, newstr, len);
	but->val.asstr[len]= 0;
	
	block= Get_uiBlock();
	if(block) uiDefBut(block, TEX, event, name, x, y, w, h,
									but->val.asstr, 0, len, 0, 0, tip);

	return (PyObject *) but;
}

static PyObject *Method_GetStringWidth (PyObject *self, PyObject *args)
{
	char *text;
	PyObject *width;

	if (!PyArg_ParseTuple (args, "s", &text))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
					"expected string argument");

	width = PyInt_FromLong(BMF_GetStringWidth (G.font, text));

	if (!width)
		return EXPP_ReturnPyObjError (PyExc_MemoryError,
					"couldn't create PyInt");

	return width;
}

static PyObject *Method_Text (PyObject *self, PyObject *args)
{
	char *text;
	
	if (!PyArg_ParseTuple(args, "s", &text))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
					"expected string argument");

	BMF_DrawString(G.font, text);
	
	return EXPP_incr_ret(Py_None);
}

PyObject *Draw_Init (void) 
{
	PyObject *submodule, *dict;

  Button_Type.ob_type = &PyType_Type;

	submodule = Py_InitModule3("Blender.Draw", Draw_methods, Draw_doc);

	dict= PyModule_GetDict(submodule);

#define EXPP_ADDCONST(x) \
 	PyDict_SetItemString(dict, #x, PyInt_FromLong(x))

/* So, for example:
 * EXPP_ADDCONST(LEFTMOUSE) becomes
 * PyDict_SetItemString(dict, "LEFTMOUSE", PyInt_FromLong(LEFTMOUSE)) */

	EXPP_ADDCONST(LEFTMOUSE);
	EXPP_ADDCONST(MIDDLEMOUSE);
	EXPP_ADDCONST(RIGHTMOUSE);
	EXPP_ADDCONST(MOUSEX);
	EXPP_ADDCONST(MOUSEY);
	EXPP_ADDCONST(TIMER0);
	EXPP_ADDCONST(TIMER1);
	EXPP_ADDCONST(TIMER2);
	EXPP_ADDCONST(TIMER3);
	EXPP_ADDCONST(KEYBD);
	EXPP_ADDCONST(RAWKEYBD);
	EXPP_ADDCONST(REDRAW);
	EXPP_ADDCONST(INPUTCHANGE);
	EXPP_ADDCONST(QFULL);
	EXPP_ADDCONST(WINFREEZE);
	EXPP_ADDCONST(WINTHAW);
	EXPP_ADDCONST(WINCLOSE);
	EXPP_ADDCONST(WINQUIT);
#ifndef IRISGL
	EXPP_ADDCONST(Q_FIRSTTIME);
#endif
	EXPP_ADDCONST(AKEY);
	EXPP_ADDCONST(BKEY);
	EXPP_ADDCONST(CKEY);
	EXPP_ADDCONST(DKEY);
	EXPP_ADDCONST(EKEY);
	EXPP_ADDCONST(FKEY);
	EXPP_ADDCONST(GKEY);
	EXPP_ADDCONST(HKEY);
	EXPP_ADDCONST(IKEY);
	EXPP_ADDCONST(JKEY);
	EXPP_ADDCONST(KKEY);
	EXPP_ADDCONST(LKEY);
	EXPP_ADDCONST(MKEY);
	EXPP_ADDCONST(NKEY);
	EXPP_ADDCONST(OKEY);
	EXPP_ADDCONST(PKEY);
	EXPP_ADDCONST(QKEY);
	EXPP_ADDCONST(RKEY);
	EXPP_ADDCONST(SKEY);
	EXPP_ADDCONST(TKEY);
	EXPP_ADDCONST(UKEY);
	EXPP_ADDCONST(VKEY);
	EXPP_ADDCONST(WKEY);
	EXPP_ADDCONST(XKEY);
	EXPP_ADDCONST(YKEY);
	EXPP_ADDCONST(ZKEY);
	EXPP_ADDCONST(ZEROKEY);
	EXPP_ADDCONST(ONEKEY);
	EXPP_ADDCONST(TWOKEY);
	EXPP_ADDCONST(THREEKEY);
	EXPP_ADDCONST(FOURKEY);
	EXPP_ADDCONST(FIVEKEY);
	EXPP_ADDCONST(SIXKEY);
	EXPP_ADDCONST(SEVENKEY);
	EXPP_ADDCONST(EIGHTKEY);
	EXPP_ADDCONST(NINEKEY);
	EXPP_ADDCONST(CAPSLOCKKEY);
	EXPP_ADDCONST(LEFTCTRLKEY);
	EXPP_ADDCONST(LEFTALTKEY);
	EXPP_ADDCONST(RIGHTALTKEY);
	EXPP_ADDCONST(RIGHTCTRLKEY);
	EXPP_ADDCONST(RIGHTSHIFTKEY);
	EXPP_ADDCONST(LEFTSHIFTKEY);
	EXPP_ADDCONST(ESCKEY);
	EXPP_ADDCONST(TABKEY);
	EXPP_ADDCONST(RETKEY);
	EXPP_ADDCONST(SPACEKEY);
	EXPP_ADDCONST(LINEFEEDKEY);
	EXPP_ADDCONST(BACKSPACEKEY);
	EXPP_ADDCONST(DELKEY);
	EXPP_ADDCONST(SEMICOLONKEY);
	EXPP_ADDCONST(PERIODKEY);
	EXPP_ADDCONST(COMMAKEY);
	EXPP_ADDCONST(QUOTEKEY);
	EXPP_ADDCONST(ACCENTGRAVEKEY);
	EXPP_ADDCONST(MINUSKEY);
	EXPP_ADDCONST(SLASHKEY);
	EXPP_ADDCONST(BACKSLASHKEY);
	EXPP_ADDCONST(EQUALKEY);
	EXPP_ADDCONST(LEFTBRACKETKEY);
	EXPP_ADDCONST(RIGHTBRACKETKEY);
	EXPP_ADDCONST(LEFTARROWKEY);
	EXPP_ADDCONST(DOWNARROWKEY);
	EXPP_ADDCONST(RIGHTARROWKEY);
	EXPP_ADDCONST(UPARROWKEY);
	EXPP_ADDCONST(PAD2);
	EXPP_ADDCONST(PAD4);
	EXPP_ADDCONST(PAD6);
	EXPP_ADDCONST(PAD8);
	EXPP_ADDCONST(PAD1);
	EXPP_ADDCONST(PAD3);
	EXPP_ADDCONST(PAD5);
	EXPP_ADDCONST(PAD7);
	EXPP_ADDCONST(PAD9);
	EXPP_ADDCONST(PADPERIOD);
	EXPP_ADDCONST(PADSLASHKEY);
	EXPP_ADDCONST(PADASTERKEY);
	EXPP_ADDCONST(PAD0);
	EXPP_ADDCONST(PADMINUS);
	EXPP_ADDCONST(PADENTER);
	EXPP_ADDCONST(PADPLUSKEY);
	EXPP_ADDCONST(F1KEY);
	EXPP_ADDCONST(F2KEY);
	EXPP_ADDCONST(F3KEY);
	EXPP_ADDCONST(F4KEY);
	EXPP_ADDCONST(F5KEY);
	EXPP_ADDCONST(F6KEY);
	EXPP_ADDCONST(F7KEY);
	EXPP_ADDCONST(F8KEY);
	EXPP_ADDCONST(F9KEY);
	EXPP_ADDCONST(F10KEY);
	EXPP_ADDCONST(F11KEY);
	EXPP_ADDCONST(F12KEY);
	EXPP_ADDCONST(PAUSEKEY);
	EXPP_ADDCONST(INSERTKEY);
	EXPP_ADDCONST(HOMEKEY);
	EXPP_ADDCONST(PAGEUPKEY);
	EXPP_ADDCONST(PAGEDOWNKEY);
	EXPP_ADDCONST(ENDKEY);

	return submodule;
}
