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
	
	return PyCObject_FromVoidPtr(   uiDefAutoButR(PyCObject_AsVoidPtr(py_block), &py_rna->ptr, prop, index, butname, xco, yco, width, height), NULL);
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

static PyObject *Method_beginBlock( PyObject * self, PyObject * args ) // XXX missing 2 args - UI_EMBOSS, UI_HELV, do we care?
{
	PyObject *py_context, *py_ar;
	char *name;
	
	if( !PyArg_ParseTuple( args, "O!O!s:beginBlock", &PyCObject_Type, &py_context, &PyCObject_Type, &py_ar, &name) )
		return NULL;
	
	return PyCObject_FromVoidPtr(uiBeginBlock(PyCObject_AsVoidPtr(py_context), PyCObject_AsVoidPtr(py_ar), name, UI_EMBOSS, UI_HELV), NULL);
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

static PyObject *Method_drawPanels( PyObject * self, PyObject * args )
{
	PyObject *py_context;
	int align;
	
	if( !PyArg_ParseTuple( args, "O!i:drawPanels", &PyCObject_Type, &py_context, &align) )
		return NULL;
	
	uiDrawPanels(PyCObject_AsVoidPtr(py_context), align);
	Py_RETURN_NONE;
}

static PyObject *Method_matchPanelsView2d( PyObject * self, PyObject * args )
{
	PyObject *py_ar;
	
	if( !PyArg_ParseTuple( args, "O!:matchPanelsView2d", &PyCObject_Type, &py_ar) )
		return NULL;
	
	uiMatchPanelsView2d(PyCObject_AsVoidPtr(py_ar));
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
	
	return PyLong_FromSize_t(uiNewPanel(PyCObject_AsVoidPtr(py_context), PyCObject_AsVoidPtr(py_area), PyCObject_AsVoidPtr(py_block), panelname, tabname, ofsx, ofsy, sizex, sizey));
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
	{"drawPanels", (PyCFunction)Method_drawPanels, METH_VARARGS, ""},
	{"matchPanelsView2d", (PyCFunction)Method_matchPanelsView2d, METH_VARARGS, ""},
	
	{"register", (PyCFunction)Method_register, METH_VARARGS, ""}, // XXX not sure about this - registers current script with the ScriptSpace, like Draw.Register()
	
	{"getRegonPtr", (PyCFunction)Method_getRegonPtr,	METH_NOARGS, ""}, // XXX Nasty, we really need to improve dealing with context!
	{"getAreaPtr", (PyCFunction)Method_getAreaPtr,		METH_NOARGS, ""},
	{"getScreenPtr", (PyCFunction)Method_getScreenPtr, METH_NOARGS, ""},
	{"getSpacePtr", (PyCFunction)Method_getSpacePtr, METH_NOARGS, ""},
	{"getWindowPtr", (PyCFunction)Method_getWindowPtr, METH_NOARGS, ""},
	{NULL, NULL, 0, NULL}
};

static struct PyModuleDef ui_module = {
	PyModuleDef_HEAD_INIT,
	"bpyui",
	"",
	-1,/* multiple "initialization" just copies the module dict. */
	ui_methods,
	NULL, NULL, NULL, NULL
};

PyObject *BPY_ui_module( void )
{
	PyObject *submodule, *dict;
#if PY_VERSION_HEX >= 0x03000000
	submodule= PyModule_Create(&ui_module);
#else /* Py2.x */
	submodule= Py_InitModule3( "bpyui", ui_methods, "" );
#endif
	
	/* uiBlock->flag (controls) */
	PyModule_AddObject( submodule, "UI_BLOCK_LOOP", PyLong_FromSize_t(UI_BLOCK_LOOP) );
	PyModule_AddObject( submodule, "UI_BLOCK_RET_1", PyLong_FromSize_t(UI_BLOCK_RET_1) );
	PyModule_AddObject( submodule, "UI_BLOCK_NUMSELECT", PyLong_FromSize_t(UI_BLOCK_NUMSELECT) );
	PyModule_AddObject( submodule, "UI_BLOCK_ENTER_OK", PyLong_FromSize_t(UI_BLOCK_ENTER_OK) );
	PyModule_AddObject( submodule, "UI_BLOCK_NOSHADOW", PyLong_FromSize_t(UI_BLOCK_NOSHADOW) );
	PyModule_AddObject( submodule, "UI_BLOCK_NO_HILITE", PyLong_FromSize_t(UI_BLOCK_NO_HILITE) );
	PyModule_AddObject( submodule, "UI_BLOCK_MOVEMOUSE_QUIT", PyLong_FromSize_t(UI_BLOCK_MOVEMOUSE_QUIT) );
	PyModule_AddObject( submodule, "UI_BLOCK_KEEP_OPEN", PyLong_FromSize_t(UI_BLOCK_KEEP_OPEN) );
	PyModule_AddObject( submodule, "UI_BLOCK_POPUP", PyLong_FromSize_t(UI_BLOCK_POPUP) );
	
	/* for executing operators (XXX move elsewhere) */
	PyModule_AddObject( submodule, "WM_OP_INVOKE_DEFAULT", PyLong_FromSize_t(WM_OP_INVOKE_DEFAULT) );
	PyModule_AddObject( submodule, "WM_OP_INVOKE_REGION_WIN", PyLong_FromSize_t(WM_OP_INVOKE_REGION_WIN) );
	PyModule_AddObject( submodule, "WM_OP_INVOKE_AREA", PyLong_FromSize_t(WM_OP_INVOKE_AREA) );
	PyModule_AddObject( submodule, "WM_OP_INVOKE_SCREEN", PyLong_FromSize_t(WM_OP_INVOKE_SCREEN) );
	PyModule_AddObject( submodule, "WM_OP_EXEC_DEFAULT", PyLong_FromSize_t(WM_OP_EXEC_DEFAULT) );
	PyModule_AddObject( submodule, "WM_OP_EXEC_REGION_WIN", PyLong_FromSize_t(WM_OP_EXEC_REGION_WIN) );
	PyModule_AddObject( submodule, "WM_OP_EXEC_AREA", PyLong_FromSize_t(WM_OP_EXEC_AREA) );
	PyModule_AddObject( submodule, "WM_OP_EXEC_SCREEN", PyLong_FromSize_t(WM_OP_EXEC_SCREEN) );
	
	return submodule;
}


