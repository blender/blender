/**
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "bpy_ui.h"
#include "bpy_util.h"
#include "bpy_rna.h" /* for rna buttons */
#include "bpy_operator.h" /* for setting button operator properties */
#include "bpy_compat.h"

#include "WM_types.h" /* for WM_OP_INVOKE_DEFAULT & friends */

#include "BLI_dynstr.h"

#include "MEM_guardedalloc.h"
#include "BKE_global.h" /* evil G.* */
#include "BKE_context.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h" /* only for SpaceLink */
#include "UI_interface.h"
#include "WM_api.h"

static PyObject *Method_pupMenuBegin( PyObject * self, PyObject * args )
{
	char *title; int icon;
	
	if( !PyArg_ParseTuple( args, "si:pupMenuBegin", &title, &icon))
		return NULL;
	
	return PyCObject_FromVoidPtr( uiPupMenuBegin(title, icon), NULL );
}

static PyObject *Method_pupMenuEnd( PyObject * self, PyObject * args )
{
	PyObject *py_context, *py_head;
	
	if( !PyArg_ParseTuple( args, "O!O!:pupMenuEnd", &PyCObject_Type, &py_context, &PyCObject_Type, &py_head))
		return NULL;
	
	uiPupMenuEnd(PyCObject_AsVoidPtr(py_context), PyCObject_AsVoidPtr(py_head));
	
	Py_RETURN_NONE;
}

static PyObject *Method_menuItemO( PyObject * self, PyObject * args )
{
	PyObject *py_head;
	char *opname;
	int icon;
	
	if( !PyArg_ParseTuple( args, "O!is:menuItemO", &PyCObject_Type, &py_head, &icon, &opname))
		return NULL;
	
	uiMenuItemO(PyCObject_AsVoidPtr(py_head), icon, opname);
	
	Py_RETURN_NONE;
}

static PyObject *Method_defButO( PyObject * self, PyObject * args )
{
	uiBut *but;
	PyObject *py_block, *py_keywords= NULL;
	char *opname, *butname, *tip;
	int exec, xco, yco, width, height;
	
	if( !PyArg_ParseTuple( args, "O!sisiiiis|O!:defButO", &PyCObject_Type, &py_block, &opname, &exec, &butname, &xco, &yco, &width, &height, &tip, &PyDict_Type, &py_keywords))
		return NULL;
	
	but= uiDefButO(PyCObject_AsVoidPtr(py_block), BUT, opname, exec, butname, xco, yco, width, height, tip);
	
	/* Optional python doctionary used to set python properties, just like how keyword args are used */
	if (py_keywords && PyDict_Size(py_keywords)) {
		if (PYOP_props_from_dict(uiButGetOperatorPtrRNA(but), py_keywords) == -1)
			return NULL;
	}
	
	return PyCObject_FromVoidPtr(but, NULL);
}

static PyObject *Method_defAutoButR( PyObject * self, PyObject * args )
{
	PyObject *py_block;
	BPy_StructRNA *py_rna;
	char *propname, *butname;
	int index, xco, yco, width, height;
	PropertyRNA *prop;
	
	if( !PyArg_ParseTuple( args, "O!O!sisiiii:defAutoButR", &PyCObject_Type, &py_block, &pyrna_struct_Type, &py_rna, &propname, &index, &butname, &xco, &yco, &width, &height))
		return NULL;
	
	// XXX This isnt that nice api, but we dont always have the rna property from python since its converted immediately into a PyObject
	prop = RNA_struct_find_property(&py_rna->ptr, propname);
	if (prop==NULL) {
		PyErr_SetString(PyExc_ValueError, "rna property not found");
		return NULL;
	}
	
	return PyCObject_FromVoidPtr(   uiDefAutoButR(PyCObject_AsVoidPtr(py_block), &py_rna->ptr, prop, index, butname, 0, xco, yco, width, height), NULL);
}



static uiBlock *py_internal_uiBlockCreateFunc(struct bContext *C, struct ARegion *ar, void *arg1)
{
	PyObject *ret, *args;
	
	args = Py_BuildValue("(NN)", PyCObject_FromVoidPtr(C, NULL), PyCObject_FromVoidPtr(ar, NULL));
	ret = PyObject_CallObject( (PyObject *)arg1, args );
	Py_DECREF(args);
	
	if (ret==NULL) {
		PyErr_Print();
		return NULL;
	}
	if (!PyCObject_Check(ret)) {
		printf("invalid return value, not a PyCObject block\n");
		return NULL;
	}
	
	return (uiBlock *)PyCObject_AsVoidPtr(ret);
}

static PyObject *Method_pupBlock( PyObject * self, PyObject * args )
{
	PyObject *py_context, *py_func;
	
	if( !PyArg_ParseTuple( args, "O!O:pupBlock", &PyCObject_Type, &py_context, &py_func) )
		return NULL;
	
	if (!PyCallable_Check(py_func)) {
		PyErr_SetString(PyExc_ValueError, "arg not callable");
		return NULL;
	}
	
	uiPupBlock(PyCObject_AsVoidPtr(py_context), py_internal_uiBlockCreateFunc, (void *)py_func);
	Py_RETURN_NONE;
}

// XXX missing arg - UI_EMBOSS, do we care?
// XXX well, right now this only is to distinguish whether we have regular buttons or for pulldowns (ton)
static PyObject *Method_beginBlock( PyObject * self, PyObject * args ) 
{
	PyObject *py_context, *py_ar;
	char *name;
	
	if( !PyArg_ParseTuple( args, "O!O!s:beginBlock", &PyCObject_Type, &py_context, &PyCObject_Type, &py_ar, &name) )
		return NULL;
	
	return PyCObject_FromVoidPtr(uiBeginBlock(PyCObject_AsVoidPtr(py_context), PyCObject_AsVoidPtr(py_ar), name, UI_EMBOSS), NULL);
}

static PyObject *Method_endBlock( PyObject * self, PyObject * args )
{
	PyObject *py_context, *py_block;
	
	if( !PyArg_ParseTuple( args, "O!O!:endBlock", &PyCObject_Type, &py_context, &PyCObject_Type, &py_block) )
		return NULL;
	
	uiEndBlock(PyCObject_AsVoidPtr(py_context), PyCObject_AsVoidPtr(py_block));
	Py_RETURN_NONE;
}

static PyObject *Method_drawBlock( PyObject * self, PyObject * args )
{
	PyObject *py_context, *py_block;
	
	if( !PyArg_ParseTuple( args, "O!O!:drawBlock", &PyCObject_Type, &py_context, &PyCObject_Type, &py_block) )
		return NULL;
	
	uiDrawBlock(PyCObject_AsVoidPtr(py_context), PyCObject_AsVoidPtr(py_block));
	Py_RETURN_NONE;
}

static PyObject *Method_beginPanels( PyObject * self, PyObject * args )
{
	bContext *C;
	PyObject *py_context;
	
	if( !PyArg_ParseTuple( args, "O!i:beginPanels", &PyCObject_Type, &py_context) )
		return NULL;
	
	C= PyCObject_AsVoidPtr(py_context);
	uiBeginPanels(C, CTX_wm_region(C));
	Py_RETURN_NONE;
}

static PyObject *Method_endPanels( PyObject * self, PyObject * args )
{
	bContext *C;
	PyObject *py_context;
	
	if( !PyArg_ParseTuple( args, "O!:endPanels", &PyCObject_Type, &py_context) )
		return NULL;
	
	C= PyCObject_AsVoidPtr(py_context);
	uiEndPanels(C, CTX_wm_region(C));
	Py_RETURN_NONE;
}

static PyObject *Method_popupBoundsBlock( PyObject * self, PyObject * args )
{
	PyObject *py_block;
	int addval, mx, my;
	
	if( !PyArg_ParseTuple( args, "O!iii:popupBoundsBlock", &PyCObject_Type, &py_block, &addval, &mx, &my) )
		return NULL;
	
	uiPopupBoundsBlock(PyCObject_AsVoidPtr(py_block), addval, mx, my);
	Py_RETURN_NONE;
}

static PyObject *Method_blockBeginAlign( PyObject * self, PyObject * args )
{
	PyObject *py_block;
	
	if( !PyArg_ParseTuple( args, "O!:blockBeginAlign", &PyCObject_Type, &py_block) )
		return NULL;
	
	uiBlockBeginAlign(PyCObject_AsVoidPtr(py_block));
	Py_RETURN_NONE;
}

static PyObject *Method_blockEndAlign( PyObject * self, PyObject * args )
{
	PyObject *py_block;
	
	if( !PyArg_ParseTuple( args, "O!:blockEndAlign", &PyCObject_Type, &py_block))
		return NULL;
	
	uiBlockEndAlign(PyCObject_AsVoidPtr(py_block));
	Py_RETURN_NONE;
}

static PyObject *Method_blockSetFlag( PyObject * self, PyObject * args )
{
	PyObject *py_block;
	int flag; /* Note new py api should not use flags, but for this low level UI api its ok. */
	
	if( !PyArg_ParseTuple( args, "O!i:blockSetFlag", &PyCObject_Type, &py_block, &flag))
		return NULL;
	
	uiBlockSetFlag(PyCObject_AsVoidPtr(py_block), flag);
	Py_RETURN_NONE;
}

static PyObject *Method_newPanel( PyObject * self, PyObject * args )
{
	PyObject *py_context, *py_area, *py_block;
	char *panelname, *tabname;
	int ofsx, ofsy, sizex, sizey;
	
	if( !PyArg_ParseTuple( args, "O!O!O!ssiiii:newPanel", &PyCObject_Type, &py_context, &PyCObject_Type, &py_area, &PyCObject_Type, &py_block, &panelname, &tabname, &ofsx, &ofsy, &sizex, &sizey))
		return NULL;
	
	return PyLong_FromSsize_t(uiNewPanel(PyCObject_AsVoidPtr(py_context), PyCObject_AsVoidPtr(py_area), PyCObject_AsVoidPtr(py_block), panelname, tabname, ofsx, ofsy, sizex, sizey));
}

/* similar to Draw.c */
static PyObject *Method_register( PyObject * self, PyObject * args )
{
	PyObject *py_sl, *py_draw_func;
	SpaceLink *sl;
	if( !PyArg_ParseTuple( args, "O!O:register", &PyCObject_Type, &py_sl, &py_draw_func) )
		return NULL;
	
	sl = PyCObject_AsVoidPtr(py_sl);
	
	if(sl->spacetype!=SPACE_SCRIPT) { // XXX todo - add a script space when needed
		PyErr_SetString(PyExc_ValueError, "can only register in a script space");
		return NULL;
	}
	else {
		SpaceScript *scpt= (SpaceScript *)sl;
		char *filename = NULL;
		
		if (scpt->script==NULL) {
			scpt->script = MEM_callocN(sizeof(Script), "ScriptRegister");
		}
		
		BPY_getFileAndNum(&filename, NULL);
		
		if (filename) {
			strncpy(scpt->script->scriptname, filename, sizeof(scpt->script->scriptname));
#if 0
			char *dot;
			dot = strchr(scpt->script->scriptname, '.'); /* remove extension */
			if (dot)
				*dot= '\0';
#endif
			Py_XINCREF( py_draw_func );
			scpt->script->py_draw= (void *)py_draw_func;
		}
		else {
			return NULL; /* BPY_getFileAndNum sets the error */
		}

		if (filename==NULL) {
			return NULL;
		}
	}

	Py_RETURN_NONE;
}

static PyObject *Method_registerKey( PyObject * self, PyObject * args )
{
	PyObject *py_context;
	PyObject *py_keywords= NULL;
	char *keymap_name, *operator_name;
	int spaceid, regionid;
	int keyval, evtval, q1, q2;
	
	wmWindowManager *wm;
	ListBase *keymap;
	wmKeymapItem *km;
	
	if( !PyArg_ParseTuple( args, "O!iissiiii|O!:registerKey", &PyCObject_Type, &py_context, &spaceid, &regionid, &keymap_name, &operator_name, &keyval, &evtval, &q1, &q2, &PyDict_Type, &py_keywords) )
		return NULL;

	wm= CTX_wm_manager(PyCObject_AsVoidPtr(py_context));
	
	/* keymap= WM_keymap_listbase(wm, "Image Generic", SPACE_IMAGE, 0); */
	keymap= WM_keymap_listbase(wm, keymap_name, spaceid, regionid);
	
	km= WM_keymap_add_item(keymap, operator_name, keyval, evtval, q1, q2);
	
	/* Optional python doctionary used to set python properties, just like how keyword args are used */
	if (py_keywords && PyDict_Size(py_keywords)) {
		if (PYOP_props_from_dict(km->ptr, py_keywords) == -1)
			return NULL;
	}
	
	Py_RETURN_NONE;
}

/* internal use only */
static bContext *get_py_context__internal(void)
{
	PyObject *globals = PyEval_GetGlobals();
	PyObject *val= PyDict_GetItemString(globals, "__bpy_context__");
	return PyCObject_AsVoidPtr(val);
}

static PyObject *Method_getRegonPtr( PyObject * self )
{
	bContext *C= get_py_context__internal();
	
	ARegion *ar = CTX_wm_region(C);
	return PyCObject_FromVoidPtr(ar, NULL);
}

static PyObject *Method_getAreaPtr( PyObject * self )
{
	bContext *C= get_py_context__internal();
	
	ScrArea *area = CTX_wm_area(C);
	return PyCObject_FromVoidPtr(area, NULL);
}

static PyObject *Method_getScreenPtr( PyObject * self )
{
	bContext *C= get_py_context__internal();
	
	bScreen *screen= CTX_wm_screen(C);
	return PyCObject_FromVoidPtr(screen, NULL);
}

static PyObject *Method_getSpacePtr( PyObject * self )
{
	bContext *C= get_py_context__internal();
	
	SpaceLink *sl= CTX_wm_space_data(C);
	return PyCObject_FromVoidPtr(sl, NULL);
}

static PyObject *Method_getWindowPtr( PyObject * self )
{
	bContext *C= get_py_context__internal();
	
	wmWindow *window= CTX_wm_window(C);
	return PyCObject_FromVoidPtr(window, NULL);
}

static struct PyMethodDef ui_methods[] = {
	{"pupMenuBegin", (PyCFunction)Method_pupMenuBegin, METH_VARARGS, ""},
	{"pupMenuEnd", (PyCFunction)Method_pupMenuEnd, METH_VARARGS, ""},
	{"menuItemO", (PyCFunction)Method_menuItemO, METH_VARARGS, ""},
	{"defButO", (PyCFunction)Method_defButO, METH_VARARGS, ""},
	{"defAutoButR", (PyCFunction)Method_defAutoButR, METH_VARARGS, ""},
	{"pupBlock", (PyCFunction)Method_pupBlock, METH_VARARGS, ""},
	{"beginBlock", (PyCFunction)Method_beginBlock, METH_VARARGS, ""},
	{"endBlock", (PyCFunction)Method_endBlock, METH_VARARGS, ""},
	{"drawBlock", (PyCFunction)Method_drawBlock, METH_VARARGS, ""},
	{"popupBoundsBlock", (PyCFunction)Method_popupBoundsBlock, METH_VARARGS, ""},
	{"blockBeginAlign", (PyCFunction)Method_blockBeginAlign, METH_VARARGS, ""},
	{"blockEndAlign", (PyCFunction)Method_blockEndAlign, METH_VARARGS, ""},
	{"blockSetFlag", (PyCFunction)Method_blockSetFlag, METH_VARARGS, ""},
	{"newPanel", (PyCFunction)Method_newPanel, METH_VARARGS, ""},
	{"beginPanels", (PyCFunction)Method_beginPanels, METH_VARARGS, ""},
	{"endPanels", (PyCFunction)Method_endPanels, METH_VARARGS, ""},
	
	{"register", (PyCFunction)Method_register, METH_VARARGS, ""}, // XXX not sure about this - registers current script with the ScriptSpace, like Draw.Register()
	{"registerKey", (PyCFunction)Method_registerKey, METH_VARARGS, ""}, // XXX could have this in another place too
	
	
	{"getRegonPtr", (PyCFunction)Method_getRegonPtr,	METH_NOARGS, ""}, // XXX Nasty, we really need to improve dealing with context!
	{"getAreaPtr", (PyCFunction)Method_getAreaPtr,		METH_NOARGS, ""},
	{"getScreenPtr", (PyCFunction)Method_getScreenPtr, METH_NOARGS, ""},
	{"getSpacePtr", (PyCFunction)Method_getSpacePtr, METH_NOARGS, ""},
	{"getWindowPtr", (PyCFunction)Method_getWindowPtr, METH_NOARGS, ""},

	{NULL, NULL, 0, NULL}
};

#if PY_VERSION_HEX >= 0x03000000
static struct PyModuleDef ui_module = {
	PyModuleDef_HEAD_INIT,
	"bpyui",
	"",
	-1,/* multiple "initialization" just copies the module dict. */
	ui_methods,
	NULL, NULL, NULL, NULL
};
#endif

PyObject *BPY_ui_module( void )
{
	PyObject *submodule, *mod;
#if PY_VERSION_HEX >= 0x03000000
	submodule= PyModule_Create(&ui_module);
#else /* Py2.x */
	submodule= Py_InitModule3( "bpyui", ui_methods, "" );
#endif
	
	/* uiBlock->flag (controls) */
	mod = PyModule_New("ui");
	PyModule_AddObject( submodule, "ui", mod );
	PyModule_AddObject( mod, "BLOCK_LOOP", PyLong_FromSsize_t(UI_BLOCK_LOOP) );
	PyModule_AddObject( mod, "BLOCK_RET_1", PyLong_FromSsize_t(UI_BLOCK_RET_1) );
	PyModule_AddObject( mod, "BLOCK_NUMSELECT", PyLong_FromSsize_t(UI_BLOCK_NUMSELECT) );
	PyModule_AddObject( mod, "BLOCK_ENTER_OK", PyLong_FromSsize_t(UI_BLOCK_ENTER_OK) );
	PyModule_AddObject( mod, "BLOCK_NOSHADOW", PyLong_FromSsize_t(UI_BLOCK_NOSHADOW) );
	PyModule_AddObject( mod, "BLOCK_NO_HILITE", PyLong_FromSsize_t(UI_BLOCK_NO_HILITE) );
	PyModule_AddObject( mod, "BLOCK_MOVEMOUSE_QUIT", PyLong_FromSsize_t(UI_BLOCK_MOVEMOUSE_QUIT) );
	PyModule_AddObject( mod, "BLOCK_KEEP_OPEN", PyLong_FromSsize_t(UI_BLOCK_KEEP_OPEN) );
	PyModule_AddObject( mod, "BLOCK_POPUP", PyLong_FromSsize_t(UI_BLOCK_POPUP) );
	
	/* for executing operators (XXX move elsewhere) */
	mod = PyModule_New("wmTypes");
	PyModule_AddObject( submodule, "wmTypes", mod );
	PyModule_AddObject( mod, "OP_INVOKE_DEFAULT", PyLong_FromSsize_t(WM_OP_INVOKE_DEFAULT) );
	PyModule_AddObject( mod, "OP_INVOKE_REGION_WIN", PyLong_FromSsize_t(WM_OP_INVOKE_REGION_WIN) );
	PyModule_AddObject( mod, "OP_INVOKE_AREA", PyLong_FromSsize_t(WM_OP_INVOKE_AREA) );
	PyModule_AddObject( mod, "OP_INVOKE_SCREEN", PyLong_FromSsize_t(WM_OP_INVOKE_SCREEN) );
	PyModule_AddObject( mod, "OP_EXEC_DEFAULT", PyLong_FromSsize_t(WM_OP_EXEC_DEFAULT) );
	PyModule_AddObject( mod, "OP_EXEC_REGION_WIN", PyLong_FromSsize_t(WM_OP_EXEC_REGION_WIN) );
	PyModule_AddObject( mod, "OP_EXEC_AREA", PyLong_FromSsize_t(WM_OP_EXEC_AREA) );
	PyModule_AddObject( mod, "OP_EXEC_SCREEN", PyLong_FromSsize_t(WM_OP_EXEC_SCREEN) );
	
	mod = PyModule_New("keyValTypes");
	PyModule_AddObject( submodule, "keyValTypes", mod );
	PyModule_AddObject( mod, "ANY", PyLong_FromSsize_t(KM_ANY) );
	PyModule_AddObject( mod, "NOTHING", PyLong_FromSsize_t(KM_NOTHING) );
	PyModule_AddObject( mod, "PRESS", PyLong_FromSsize_t(KM_PRESS) );
	PyModule_AddObject( mod, "RELEASE", PyLong_FromSsize_t(KM_RELEASE) );
	
	mod = PyModule_New("keyModTypes");
	PyModule_AddObject( submodule, "keyModTypes", mod );
	PyModule_AddObject( mod, "SHIFT", PyLong_FromSsize_t(KM_SHIFT) );
	PyModule_AddObject( mod, "CTRL", PyLong_FromSsize_t(KM_CTRL) );
	PyModule_AddObject( mod, "ALT", PyLong_FromSsize_t(KM_ALT) );
	PyModule_AddObject( mod, "OSKEY", PyLong_FromSsize_t(KM_OSKEY) );
	
	PyModule_AddObject( mod, "SHIFT2", PyLong_FromSsize_t(KM_SHIFT2) );
	PyModule_AddObject( mod, "CTRL2", PyLong_FromSsize_t(KM_CTRL2) );
	PyModule_AddObject( mod, "ALT2", PyLong_FromSsize_t(KM_ALT2) );
	PyModule_AddObject( mod, "OSKEY2", PyLong_FromSsize_t(KM_OSKEY2) );
	
	mod = PyModule_New("keyTypes");
	PyModule_AddObject( submodule, "keyTypes", mod );
	PyModule_AddObject( mod, "A", PyLong_FromSsize_t(AKEY) );
	PyModule_AddObject( mod, "B", PyLong_FromSsize_t(BKEY) );
	PyModule_AddObject( mod, "C", PyLong_FromSsize_t(CKEY) );
	PyModule_AddObject( mod, "D", PyLong_FromSsize_t(DKEY) );
	PyModule_AddObject( mod, "E", PyLong_FromSsize_t(EKEY) );
	PyModule_AddObject( mod, "F", PyLong_FromSsize_t(FKEY) );
	PyModule_AddObject( mod, "G", PyLong_FromSsize_t(GKEY) );
	PyModule_AddObject( mod, "H", PyLong_FromSsize_t(HKEY) );
	PyModule_AddObject( mod, "I", PyLong_FromSsize_t(IKEY) );
	PyModule_AddObject( mod, "J", PyLong_FromSsize_t(JKEY) );
	PyModule_AddObject( mod, "K", PyLong_FromSsize_t(KKEY) );
	PyModule_AddObject( mod, "L", PyLong_FromSsize_t(LKEY) );
	PyModule_AddObject( mod, "M", PyLong_FromSsize_t(MKEY) );
	PyModule_AddObject( mod, "N", PyLong_FromSsize_t(NKEY) );
	PyModule_AddObject( mod, "O", PyLong_FromSsize_t(OKEY) );
	PyModule_AddObject( mod, "P", PyLong_FromSsize_t(PKEY) );
	PyModule_AddObject( mod, "Q", PyLong_FromSsize_t(QKEY) );
	PyModule_AddObject( mod, "R", PyLong_FromSsize_t(RKEY) );
	PyModule_AddObject( mod, "S", PyLong_FromSsize_t(SKEY) );
	PyModule_AddObject( mod, "T", PyLong_FromSsize_t(TKEY) );
	PyModule_AddObject( mod, "U", PyLong_FromSsize_t(UKEY) );
	PyModule_AddObject( mod, "V", PyLong_FromSsize_t(VKEY) );
	PyModule_AddObject( mod, "W", PyLong_FromSsize_t(WKEY) );
	PyModule_AddObject( mod, "X", PyLong_FromSsize_t(XKEY) );
	PyModule_AddObject( mod, "Y", PyLong_FromSsize_t(YKEY) );
	PyModule_AddObject( mod, "Z", PyLong_FromSsize_t(ZKEY) );
	PyModule_AddObject( mod, "ZERO", PyLong_FromSsize_t(ZEROKEY) );
	PyModule_AddObject( mod, "ONE", PyLong_FromSsize_t(ONEKEY) );
	PyModule_AddObject( mod, "TWO", PyLong_FromSsize_t(TWOKEY) );
	PyModule_AddObject( mod, "THREE", PyLong_FromSsize_t(THREEKEY) );
	PyModule_AddObject( mod, "FOUR", PyLong_FromSsize_t(FOURKEY) );
	PyModule_AddObject( mod, "FIVE", PyLong_FromSsize_t(FIVEKEY) );
	PyModule_AddObject( mod, "SIX", PyLong_FromSsize_t(SIXKEY) );
	PyModule_AddObject( mod, "SEVEN", PyLong_FromSsize_t(SEVENKEY) );
	PyModule_AddObject( mod, "EIGHT", PyLong_FromSsize_t(EIGHTKEY) );
	PyModule_AddObject( mod, "NINE", PyLong_FromSsize_t(NINEKEY) );
	PyModule_AddObject( mod, "CAPSLOCK", PyLong_FromSsize_t(CAPSLOCKKEY) );
	PyModule_AddObject( mod, "LEFTCTRL", PyLong_FromSsize_t(LEFTCTRLKEY) );
	PyModule_AddObject( mod, "LEFTALT", PyLong_FromSsize_t(LEFTALTKEY) );
	PyModule_AddObject( mod, "RIGHTALT", PyLong_FromSsize_t(RIGHTALTKEY) );
	PyModule_AddObject( mod, "RIGHTCTRL", PyLong_FromSsize_t(RIGHTCTRLKEY) );
	PyModule_AddObject( mod, "RIGHTSHIFT", PyLong_FromSsize_t(RIGHTSHIFTKEY) );
	PyModule_AddObject( mod, "LEFTSHIFT", PyLong_FromSsize_t(LEFTSHIFTKEY) );
	PyModule_AddObject( mod, "ESC", PyLong_FromSsize_t(ESCKEY) );
	PyModule_AddObject( mod, "TAB", PyLong_FromSsize_t(TABKEY) );
	PyModule_AddObject( mod, "RET", PyLong_FromSsize_t(RETKEY) );
	PyModule_AddObject( mod, "SPACE", PyLong_FromSsize_t(SPACEKEY) );
	PyModule_AddObject( mod, "LINEFEED", PyLong_FromSsize_t(LINEFEEDKEY) );
	PyModule_AddObject( mod, "BACKSPACE", PyLong_FromSsize_t(BACKSPACEKEY) );
	PyModule_AddObject( mod, "DEL", PyLong_FromSsize_t(DELKEY) );
	PyModule_AddObject( mod, "SEMICOLON", PyLong_FromSsize_t(SEMICOLONKEY) );
	PyModule_AddObject( mod, "PERIOD", PyLong_FromSsize_t(PERIODKEY) );
	PyModule_AddObject( mod, "COMMA", PyLong_FromSsize_t(COMMAKEY) );
	PyModule_AddObject( mod, "QUOTE", PyLong_FromSsize_t(QUOTEKEY) );
	PyModule_AddObject( mod, "ACCENTGRAVE", PyLong_FromSsize_t(ACCENTGRAVEKEY) );
	PyModule_AddObject( mod, "MINUS", PyLong_FromSsize_t(MINUSKEY) );
	PyModule_AddObject( mod, "SLASH", PyLong_FromSsize_t(SLASHKEY) );
	PyModule_AddObject( mod, "BACKSLASH", PyLong_FromSsize_t(BACKSLASHKEY) );
	PyModule_AddObject( mod, "EQUAL", PyLong_FromSsize_t(EQUALKEY) );
	PyModule_AddObject( mod, "LEFTBRACKET", PyLong_FromSsize_t(LEFTBRACKETKEY) );
	PyModule_AddObject( mod, "RIGHTBRACKET", PyLong_FromSsize_t(RIGHTBRACKETKEY) );
	PyModule_AddObject( mod, "LEFTARROW", PyLong_FromSsize_t(LEFTARROWKEY) );
	PyModule_AddObject( mod, "DOWNARROW", PyLong_FromSsize_t(DOWNARROWKEY) );
	PyModule_AddObject( mod, "RIGHTARROW", PyLong_FromSsize_t(RIGHTARROWKEY) );
	PyModule_AddObject( mod, "UPARROW", PyLong_FromSsize_t(UPARROWKEY) );
	PyModule_AddObject( mod, "PAD0", PyLong_FromSsize_t(PAD0) );
	PyModule_AddObject( mod, "PAD1", PyLong_FromSsize_t(PAD1) );
	PyModule_AddObject( mod, "PAD2", PyLong_FromSsize_t(PAD2) );
	PyModule_AddObject( mod, "PAD3", PyLong_FromSsize_t(PAD3) );
	PyModule_AddObject( mod, "PAD4", PyLong_FromSsize_t(PAD4) );
	PyModule_AddObject( mod, "PAD5", PyLong_FromSsize_t(PAD5) );
	PyModule_AddObject( mod, "PAD6", PyLong_FromSsize_t(PAD6) );
	PyModule_AddObject( mod, "PAD7", PyLong_FromSsize_t(PAD7) );
	PyModule_AddObject( mod, "PAD8", PyLong_FromSsize_t(PAD8) );
	PyModule_AddObject( mod, "PAD9", PyLong_FromSsize_t(PAD9) );
	PyModule_AddObject( mod, "PADPERIOD", PyLong_FromSsize_t(PADPERIOD) );
	PyModule_AddObject( mod, "PADSLASH", PyLong_FromSsize_t(PADSLASHKEY) );
	PyModule_AddObject( mod, "PADASTER", PyLong_FromSsize_t(PADASTERKEY) );
	PyModule_AddObject( mod, "PADMINUS", PyLong_FromSsize_t(PADMINUS) );
	PyModule_AddObject( mod, "PADENTER", PyLong_FromSsize_t(PADENTER) );
	PyModule_AddObject( mod, "PADPLUS", PyLong_FromSsize_t(PADPLUSKEY) );
	PyModule_AddObject( mod, "F1", PyLong_FromSsize_t(F1KEY) );
	PyModule_AddObject( mod, "F2", PyLong_FromSsize_t(F2KEY) );
	PyModule_AddObject( mod, "F3", PyLong_FromSsize_t(F3KEY) );
	PyModule_AddObject( mod, "F4", PyLong_FromSsize_t(F4KEY) );
	PyModule_AddObject( mod, "F5", PyLong_FromSsize_t(F5KEY) );
	PyModule_AddObject( mod, "F6", PyLong_FromSsize_t(F6KEY) );
	PyModule_AddObject( mod, "F7", PyLong_FromSsize_t(F7KEY) );
	PyModule_AddObject( mod, "F8", PyLong_FromSsize_t(F8KEY) );
	PyModule_AddObject( mod, "F9", PyLong_FromSsize_t(F9KEY) );
	PyModule_AddObject( mod, "F10", PyLong_FromSsize_t(F10KEY) );
	PyModule_AddObject( mod, "F11", PyLong_FromSsize_t(F11KEY) );
	PyModule_AddObject( mod, "F12", PyLong_FromSsize_t(F12KEY) );
	PyModule_AddObject( mod, "PAUSE", PyLong_FromSsize_t(PAUSEKEY) );
	PyModule_AddObject( mod, "INSERT", PyLong_FromSsize_t(INSERTKEY) );
	PyModule_AddObject( mod, "HOME", PyLong_FromSsize_t(HOMEKEY) );
	PyModule_AddObject( mod, "PAGEUP", PyLong_FromSsize_t(PAGEUPKEY) );
	PyModule_AddObject( mod, "PAGEDOWN", PyLong_FromSsize_t(PAGEDOWNKEY) );
	PyModule_AddObject( mod, "END", PyLong_FromSsize_t(ENDKEY) );
	PyModule_AddObject( mod, "UNKNOWN", PyLong_FromSsize_t(UNKNOWNKEY) );
	PyModule_AddObject( mod, "COMMAND", PyLong_FromSsize_t(COMMANDKEY) );
	PyModule_AddObject( mod, "GRLESS", PyLong_FromSsize_t(GRLESSKEY) );
	
	mod = PyModule_New("spaceTypes");
	PyModule_AddObject( submodule, "spaceTypes", mod );
	PyModule_AddObject( mod, "EMPTY", PyLong_FromSsize_t(SPACE_EMPTY) );
	PyModule_AddObject( mod, "VIEW3D", PyLong_FromSsize_t(SPACE_VIEW3D) );
	PyModule_AddObject( mod, "IPO", PyLong_FromSsize_t(SPACE_IPO) );
	PyModule_AddObject( mod, "OUTLINER", PyLong_FromSsize_t(SPACE_OUTLINER) );
	PyModule_AddObject( mod, "BUTS", PyLong_FromSsize_t(SPACE_BUTS) );
	PyModule_AddObject( mod, "FILE", PyLong_FromSsize_t(SPACE_FILE) );
	PyModule_AddObject( mod, "IMAGE", PyLong_FromSsize_t(SPACE_IMAGE) );
	PyModule_AddObject( mod, "INFO", PyLong_FromSsize_t(SPACE_INFO) );
	PyModule_AddObject( mod, "SEQ", PyLong_FromSsize_t(SPACE_SEQ) );
	PyModule_AddObject( mod, "TEXT", PyLong_FromSsize_t(SPACE_TEXT) );
	PyModule_AddObject( mod, "IMASEL", PyLong_FromSsize_t(SPACE_IMASEL) );
	PyModule_AddObject( mod, "SOUND", PyLong_FromSsize_t(SPACE_SOUND) );
	PyModule_AddObject( mod, "ACTION", PyLong_FromSsize_t(SPACE_ACTION) );
	PyModule_AddObject( mod, "NLA", PyLong_FromSsize_t(SPACE_NLA) );
	PyModule_AddObject( mod, "SCRIPT", PyLong_FromSsize_t(SPACE_SCRIPT) );
	PyModule_AddObject( mod, "TIME", PyLong_FromSsize_t(SPACE_TIME) );
	PyModule_AddObject( mod, "NODE", PyLong_FromSsize_t(SPACE_NODE) );
	
	
	
	
	return submodule;
}


