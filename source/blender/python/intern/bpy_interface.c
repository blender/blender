
#include <stdio.h>
#include <stdlib.h>

#include <Python.h>
#include "compile.h"		/* for the PyCodeObject */
#include "eval.h"		/* for PyEval_EvalCode */

#include "BKE_context.h"

#include "bpy_compat.h"

#include "bpy_rna.h"
#include "bpy_operator.h"


/*****************************************************************************
* Description: This function creates a new Python dictionary object.
*****************************************************************************/

static PyObject *CreateGlobalDictionary( bContext *C )
{
	PyObject *dict = PyDict_New(  );
	PyObject *item = PyUnicode_FromString( "__main__" );
	PyDict_SetItemString( dict, "__builtins__", PyEval_GetBuiltins(  ) );
	PyDict_SetItemString( dict, "__name__", item );
	Py_DECREF(item);
	
	/* Add Modules */
	item = BPY_rna_module();
	PyDict_SetItemString( dict, "bpy", item );
	Py_DECREF(item);
	
	item = BPY_rna_doc();
	PyDict_SetItemString( dict, "bpydoc", item );
	Py_DECREF(item);

	item = BPY_operator_module(C);
	PyDict_SetItemString( dict, "bpyoperator", item );
	Py_DECREF(item);

	
	// XXX very experemental, consiter this a test, especiall PyCObject is not meant to be perminant
	item = BPY_ui_module();
	PyDict_SetItemString( dict, "bpyui", item );
	Py_DECREF(item);
	
	// XXX - evil, need to access context
	item = PyCObject_FromVoidPtr( C, NULL );
	PyDict_SetItemString( dict, "__bpy_context__", item );
	Py_DECREF(item);
	
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
	
	Py_Finalize(  );
	return;
}

void BPY_run_python_script( bContext *C, const char *fn )
{
	PyObject *py_dict, *py_result;
	char pystring[512];
	PyGILState_STATE gilstate;

	/* TODO - look into a better way to run a file */
	sprintf(pystring, "exec(open(r'%s').read())", fn);	
	
	//BPY_start_python();
	
	gilstate = PyGILState_Ensure();
	
	py_dict = CreateGlobalDictionary(C);
	
	py_result = PyRun_String( pystring, Py_file_input, py_dict, py_dict );
	
	if (!py_result)
		PyErr_Print();
	else
		Py_DECREF( py_result );
	
	PyGILState_Release(gilstate);
	
	//BPY_end_python();
}
