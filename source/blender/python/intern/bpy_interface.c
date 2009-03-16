
#include <stdio.h>
#include <stdlib.h>

#include <Python.h>
#include "compile.h"		/* for the PyCodeObject */
#include "eval.h"		/* for PyEval_EvalCode */

#include "BKE_context.h"

#include "bpy_compat.h"

#include "bpy_rna.h"
#include "bpy_operator.h"
#include "bpy_ui.h"

#include "DNA_space_types.h"

#include "BKE_text.h"
#include "DNA_text_types.h"
#include "MEM_guardedalloc.h"

void BPY_free_compiled_text( struct Text *text )
{
	if( text->compiled ) {
		Py_DECREF( ( PyObject * ) text->compiled );
		text->compiled = NULL;
	}
}

/*****************************************************************************
* Description: This function creates a new Python dictionary object.
*****************************************************************************/

static PyObject *CreateGlobalDictionary( bContext *C )
{
	PyObject *mod;
	PyObject *dict = PyDict_New(  );
	PyObject *item = PyUnicode_FromString( "__main__" );
	PyDict_SetItemString( dict, "__builtins__", PyEval_GetBuiltins(  ) );
	PyDict_SetItemString( dict, "__name__", item );
	Py_DECREF(item);
	
	/* add bpy to global namespace */
	mod = PyModule_New("bpy");
	PyDict_SetItemString( dict, "bpy", mod );
	Py_DECREF(mod);
	
	PyModule_AddObject( mod, "data", BPY_rna_module() );
	/* PyModule_AddObject( mod, "doc", BPY_rna_doc() ); */
	PyModule_AddObject( mod, "types", BPY_rna_types() );
	PyModule_AddObject( mod, "ops", BPY_operator_module(C) );
	PyModule_AddObject( mod, "ui", BPY_ui_module() ); // XXX very experemental, consider this a test, especially PyCObject is not meant to be perminant
	
	// XXX - evil, need to access context
	item = PyCObject_FromVoidPtr( C, NULL );
	PyDict_SetItemString( dict, "__bpy_context__", item );
	Py_DECREF(item);
	
	
	// XXX - put somewhere more logical
	{
		PyMethodDef *ml;
		static PyMethodDef bpy_prop_meths[] = {
			{"FloatProperty", BPy_FloatProperty, METH_VARARGS|METH_KEYWORDS, ""},
			{"IntProperty", BPy_IntProperty, METH_VARARGS|METH_KEYWORDS, ""},
			{"BoolProperty", BPy_BoolProperty, METH_VARARGS|METH_KEYWORDS, ""},
			{NULL, NULL, 0, NULL}
		};
		
		for(ml = &bpy_prop_meths; ml->ml_name; ml++) {
			PyDict_SetItemString( dict, ml->ml_name, PyCFunction_New(ml, NULL));
		}
	}
	
	return dict;
}

void BPY_start_python( void )
{
	PyThreadState *py_tstate = NULL;

	Py_Initialize(  );
	
	//PySys_SetArgv( argc_copy, argv_copy );
	
	/* Initialize thread support (also acquires lock) */
	PyEval_InitThreads();
	
	// todo - sys paths - our own imports
	
	py_tstate = PyGILState_GetThisThreadState();
	PyEval_ReleaseThread(py_tstate);
	
}

void BPY_end_python( void )
{
	PyGILState_Ensure(); /* finalizing, no need to grab the state */
	
	// free other python data.
	//BPY_rna_free_types();
	
	Py_Finalize(  );
	
	return;
}

/* Can run a file or text block */
int BPY_run_python_script( bContext *C, const char *fn, struct Text *text )
{
	PyObject *py_dict, *py_result;
	PyGILState_STATE gilstate;
	
	if (fn==NULL && text==NULL) {
		return 0;
	}
	
	//BPY_start_python();
	
	gilstate = PyGILState_Ensure();

	py_dict = CreateGlobalDictionary(C);

	if (text) {
		
		if( !text->compiled ) {	/* if it wasn't already compiled, do it now */
			char *buf = txt_to_buf( text );

			text->compiled =
				Py_CompileString( buf, text->id.name+2, Py_file_input );

			MEM_freeN( buf );

			if( PyErr_Occurred(  ) ) {
				BPY_free_compiled_text( text );
				return 0;
			}
		}
		py_result =  PyEval_EvalCode( text->compiled, py_dict, py_dict );
		
	} else {
		char pystring[512];
		/* TODO - look into a better way to run a file */
		sprintf(pystring, "exec(open(r'%s').read())", fn);	
		py_result = PyRun_String( pystring, Py_file_input, py_dict, py_dict );			
	}
	
	if (!py_result) {
		PyErr_Print();
	} else {
		Py_DECREF( py_result );
	}
	PyGILState_Release(gilstate);
	
	//BPY_end_python();
	return py_result ? 1:0;
}


/* TODO - move into bpy_space.c ? */
/* GUI interface routines */

/* Copied from Draw.c */
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
// XXX 2.5		error_pyscript();
// XXX 2.5		scrarea_queue_redraw( sc->area );
	}

#if 0 // XXX 2.5
	BPy_Set_DrawButtonsList(sc->but_refs);
	BPy_Free_DrawButtonsList(); /*clear all temp button references*/
#endif

	sc->but_refs = NULL;
	
	Py_XDECREF( ( PyObject * ) script->py_draw );
	Py_XDECREF( ( PyObject * ) script->py_event );
	Py_XDECREF( ( PyObject * ) script->py_button );

	script->py_draw = script->py_event = script->py_button = NULL;
}

static int bpy_run_script_init(bContext *C, SpaceScript * sc)
{
	if (sc->script==NULL) 
		return 0;
	
	if (sc->script->py_draw==NULL && sc->script->scriptname[0] != '\0')
		BPY_run_python_script(C, sc->script->scriptname, NULL);
		
	if (sc->script->py_draw==NULL)
		return 0;
	
	return 1;
}

int BPY_run_script_space_draw(bContext *C, SpaceScript * sc)
{
	if (bpy_run_script_init(C, sc)) {
		PyGILState_STATE gilstate = PyGILState_Ensure();
		PyObject *result = PyObject_CallObject( sc->script->py_draw, NULL );
		
		if (result==NULL)
			exit_pydraw(sc, 1);
			
		PyGILState_Release(gilstate);
	}
	return 1;
}

// XXX - not used yet, listeners dont get a context
int BPY_run_script_space_listener(bContext *C, SpaceScript * sc)
{
	if (bpy_run_script_init(C, sc)) {
		PyGILState_STATE gilstate = PyGILState_Ensure();
		
		PyObject *result = PyObject_CallObject( sc->script->py_draw, NULL );
		
		if (result==NULL)
			exit_pydraw(sc, 1);
			
		PyGILState_Release(gilstate);
	}
	return 1;
}

#if 0
/* called from the the scripts window, assume context is ok */
int BPY_run_python_script_space(const char *modulename, const char *func)
{
	PyObject *py_dict, *py_result= NULL;
	char pystring[512];
	PyGILState_STATE gilstate;
	
	/* for calling the module function */
	PyObject *py_func, 
	
	gilstate = PyGILState_Ensure();
	
	py_dict = CreateGlobalDictionary(C);
	
	PyObject *module = PyImport_ImportModule(scpt->script.filename);
	if (module==NULL) {
		PyErr_SetFormat(PyExc_SystemError, "could not import '%s'", scpt->script.filename);
	}
	else {
		py_func = PyObject_GetAttrString(modulename, func);
		if (py_func==NULL) {
			PyErr_SetFormat(PyExc_SystemError, "module has no function '%s.%s'\n", scpt->script.filename, func);
		}
		else {
			if (!PyCallable_Check(py_func)) {
				PyErr_SetFormat(PyExc_SystemError, "module item is not callable '%s.%s'\n", scpt->script.filename, func);
			}
			else {
				py_result= PyObject_CallObject(py_func, NULL); // XXX will need args eventually
			}
		}
	}
	
	if (!py_result)
		PyErr_Print();
	else
		Py_DECREF( py_result );
	
	Py_XDECREF(module);
	
	
	PyGILState_Release(gilstate);
	return 1;
}
#endif
