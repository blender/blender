
#include <Python.h>
#include "compile.h"		/* for the PyCodeObject */
#include "eval.h"		/* for PyEval_EvalCode */

#include "bpy_compat.h"

#include "bpy_rna.h"


/*****************************************************************************
* Description: This function creates a new Python dictionary object.
*****************************************************************************/

static PyObject *CreateGlobalDictionary( void )
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
	
	return dict;
}

static void BPY_start_python( void )
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

static void BPY_end_python( void )
{
	PyGILState_Ensure(); /* finalizing, no need to grab the state */
	
	// free other python data.
	
	Py_Finalize(  );
	return;
}

void BPY_run_python_script( const char *fn )
{
	PyObject *py_dict, *py_result;
	char pystring[512];
	PyGILState_STATE gilstate;
	
	/* TODO - look into a better way to run a file */
	sprintf(pystring, "exec(open(r'%s').read())", fn);
	
	BPY_start_python();
	
	gilstate = PyGILState_Ensure();
	
	py_dict = CreateGlobalDictionary();
	
	py_result = PyRun_String( pystring, Py_file_input, py_dict, py_dict );
	
	if (!py_result)
		PyErr_Print();
	else
		Py_DECREF( py_result );
	
	PyGILState_Release(gilstate);
	
	BPY_end_python();
}
