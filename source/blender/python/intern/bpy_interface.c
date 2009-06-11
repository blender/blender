
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef WIN32
#include <dirent.h>
#else
#include "BLI_winstuff.h"
#endif

#include <Python.h>
#include "compile.h"		/* for the PyCodeObject */
#include "eval.h"		/* for PyEval_EvalCode */

#include "bpy_compat.h"

#include "bpy_rna.h"
#include "bpy_operator.h"
#include "bpy_ui.h"
#include "bpy_util.h"

#include "DNA_anim_types.h"
#include "DNA_space_types.h"
#include "DNA_text_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_util.h"
#include "BLI_string.h"

#include "BKE_context.h"
#include "BKE_fcurve.h"
#include "BKE_text.h"

#include "BPY_extern.h"

static void bpy_init_syspath();

void BPY_free_compiled_text( struct Text *text )
{
	if( text->compiled ) {
		Py_DECREF( ( PyObject * ) text->compiled );
		text->compiled = NULL;
	}
}

/*****************************************************************************
* Description: Creates the bpy module and adds it to sys.modules for importing
*****************************************************************************/
static void bpy_init_modules( void )
{
	PyObject *mod;
	
	mod = PyModule_New("bpy");
	
	PyModule_AddObject( mod, "data", BPY_rna_module() );
	/* PyModule_AddObject( mod, "doc", BPY_rna_doc() ); */
	PyModule_AddObject( mod, "types", BPY_rna_types() );
	PyModule_AddObject( mod, "ops", BPY_operator_module() );
	PyModule_AddObject( mod, "ui", BPY_ui_module() ); // XXX very experemental, consider this a test, especially PyCObject is not meant to be perminant
	
	/* add the module so we can import it */
	PyDict_SetItemString(PySys_GetObject("modules"), "bpy", mod);
	Py_DECREF(mod);
}

#if (PY_VERSION_HEX < 0x02050000)
PyObject *PyImport_ImportModuleLevel(char *name, void *a, void *b, void *c, int d)
{
	return PyImport_ImportModule(name);
}
#endif

void BPY_update_modules( void )
{
	PyObject *mod= PyImport_ImportModuleLevel("bpy", NULL, NULL, NULL, 0);
	PyModule_AddObject( mod, "data", BPY_rna_module() );
	PyModule_AddObject( mod, "types", BPY_rna_types() );
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
	
	// XXX - evil, need to access context
	BPy_SetContext(C);
	
	// XXX - put somewhere more logical
	{
		PyMethodDef *ml;
		static PyMethodDef bpy_prop_meths[] = {
			{"FloatProperty", (PyCFunction)BPy_FloatProperty, METH_VARARGS|METH_KEYWORDS, ""},
			{"IntProperty", (PyCFunction)BPy_IntProperty, METH_VARARGS|METH_KEYWORDS, ""},
			{"BoolProperty", (PyCFunction)BPy_BoolProperty, METH_VARARGS|METH_KEYWORDS, ""},
			{"StringProperty", (PyCFunction)BPy_StringProperty, METH_VARARGS|METH_KEYWORDS, ""},
			{NULL, NULL, 0, NULL}
		};
		
		for(ml = bpy_prop_meths; ml->ml_name; ml++) {
			PyDict_SetItemString( dict, ml->ml_name, PyCFunction_New(ml, NULL));
		}
	}
	
	/* add bpy to global namespace */
	mod= PyImport_ImportModuleLevel("bpy", NULL, NULL, NULL, 0);
	PyDict_SetItemString( dict, "bpy", mod );
	Py_DECREF(mod);
	
	return dict;
}

void BPY_start_python( int argc, char **argv )
{
	PyThreadState *py_tstate = NULL;
	
	Py_Initialize(  );
	
	//PySys_SetArgv( argc_copy, argv_copy );
	
	/* Initialize thread support (also acquires lock) */
	PyEval_InitThreads();
	
	/* bpy.* and lets us import it */
	bpy_init_modules(); 

	/* init sys.path */
	bpy_init_syspath();
	
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

void syspath_append(char *dirname)
{
	PyObject *mod_sys= NULL, *dict= NULL, *path= NULL, *dir= NULL;
	short ok=1;

	mod_sys = PyImport_ImportModule( "sys" );	/* new ref */
	
	if (mod_sys) {
		dict = PyModule_GetDict( mod_sys );	/* borrowed ref */
		path = PyDict_GetItemString( dict, "path" );	/* borrowed ref */
		if ( !PyList_Check( path ) ) {
			ok = 0;
		}
	} else {
		/* cant get the sys module */
		/* PyErr_Clear(); is called below */
		ok = 0;
	}

#if PY_MAJOR_VERSION >= 3
	dir = PyUnicode_FromString( dirname );
#else
	dir = PyString_FromString( dirname );
#endif
	
	if (ok && PySequence_Contains(path, dir)==0) { /* Only add if we need to */
		if (PyList_Append( path, dir ) != 0) /* decref below */
			ok = 0; /* append failed */
	}
	
	if( (ok==0) || PyErr_Occurred(  ) )
		fprintf(stderr, "Warning: could import or build sys.path\n" );
	
	PyErr_Clear();
	Py_DECREF( dir );
	Py_XDECREF( mod_sys );
}

/* Adds bpymodules to sys.path */
static void bpy_init_syspath()
{
	char *dir;
	char mod_dir[FILE_MAX];

	// make path to [home]/scripts/bpymodules
	dir = BLI_gethome_folder("scripts");
	BLI_make_file_string("/", mod_dir, dir, "bpymodules");

	if (BLI_exists(mod_dir)) {
		syspath_append(mod_dir);
		fprintf(stderr, "'%s' has been added to sys.path\n", mod_dir);
	}
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

	BPY_update_modules(); /* can give really bad results if this isnt here */
	
	py_dict = CreateGlobalDictionary(C);

	if (text) {
		
		if( !text->compiled ) {	/* if it wasn't already compiled, do it now */
			char *buf = txt_to_buf( text );

			text->compiled =
				Py_CompileString( buf, text->id.name+2, Py_file_input );

			MEM_freeN( buf );

			if( PyErr_Occurred(  ) ) {
				PyErr_Print(); PyErr_Clear();
				BPY_free_compiled_text( text );
				PyGILState_Release(gilstate);
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
		PyErr_Print(); PyErr_Clear();
	} else {
		Py_DECREF( py_result );
	}
	
	Py_DECREF(py_dict);
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
		PyErr_Print(); PyErr_Clear();
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

int BPY_run_script_space_draw(struct bContext *C, SpaceScript * sc)
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

void BPY_DECREF(void *pyob_ptr)
{
	Py_DECREF((PyObject *)pyob_ptr);
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
			Py_DECREF(py_func);
			if (!PyCallable_Check(py_func)) {
				PyErr_SetFormat(PyExc_SystemError, "module item is not callable '%s.%s'\n", scpt->script.filename, func);
			}
			else {
				py_result= PyObject_CallObject(py_func, NULL); // XXX will need args eventually
			}
		}
	}
	
	if (!py_result) {
		PyErr_Print(); PyErr_Clear();
	} else
		Py_DECREF( py_result );
	
	Py_XDECREF(module);
	
	Py_DECREF(py_dict);
	
	PyGILState_Release(gilstate);
	return 1;
}
#endif

// #define TIME_REGISTRATION

#ifdef TIME_REGISTRATION
#include "PIL_time.h"
#endif

/* XXX this is temporary, need a proper script registration system for 2.5 */
void BPY_run_ui_scripts(bContext *C, int reload)
{
#ifdef TIME_REGISTRATION
	double time = PIL_check_seconds_timer();
#endif
	DIR *dir; 
	struct dirent *de;
	char *file_extension;
	char path[FILE_MAX];
	char *dirname= BLI_gethome_folder("ui");
	int filelen; /* filename length */
	
	PyGILState_STATE gilstate;
	PyObject *mod;
	PyObject *sys_path_orig;
	PyObject *sys_path_new;
	
	if(!dirname)
		return;
	
	dir = opendir(dirname);

	if(!dir)
		return;
	
	gilstate = PyGILState_Ensure();
	
	/* backup sys.path */
	sys_path_orig= PySys_GetObject("path");
	Py_INCREF(sys_path_orig); /* dont free it */
	
	sys_path_new= PyList_New(1);
	PyList_SET_ITEM(sys_path_new, 0, PyUnicode_FromString(dirname));
	PySys_SetObject("path", sys_path_new);
	Py_DECREF(sys_path_new);
	
	// XXX - evil, need to access context
	BPy_SetContext(C);
	
	while((de = readdir(dir)) != NULL) {
		/* We could stat the file but easier just to let python
		 * import it and complain if theres a problem */
		
		file_extension = strstr(de->d_name, ".py");
		
		if(file_extension && *(file_extension + 3) == '\0') {
			filelen = strlen(de->d_name);
			BLI_strncpy(path, de->d_name, filelen-2); /* cut off the .py on copy */
			
			mod= PyImport_ImportModuleLevel(path, NULL, NULL, NULL, 0);
			if (mod) {
				if (reload) {
					PyObject *mod_orig= mod;
					mod= PyImport_ReloadModule(mod);
					Py_DECREF(mod_orig);
				}
			}
			
			if(mod) {
				Py_DECREF(mod); /* could be NULL from reloading */
			} else {
				PyErr_Print(); PyErr_Clear();
				fprintf(stderr, "unable to import \"%s\"  %s/%s\n", path, dirname, de->d_name);
			}
		}
	}

	closedir(dir);
	
	PySys_SetObject("path", sys_path_orig);
	Py_DECREF(sys_path_orig);
	
	PyGILState_Release(gilstate);
#ifdef TIME_REGISTRATION
	printf("script time %f\n", (PIL_check_seconds_timer()-time));
#endif
}

/* ****************************************** */
/* Drivers - PyExpression Evaluation */

/* for pydrivers (drivers using one-line Python expressions to express relationships between targets) */
PyObject *bpy_pydriver_Dict = NULL;

/* For faster execution we keep a special dictionary for pydrivers, with
 * the needed modules and aliases. 
 */
static int bpy_pydriver_create_dict(void)
{
	PyObject *d, *mod;
	
	/* validate namespace for driver evaluation */
	if (bpy_pydriver_Dict) return -1;

	d = PyDict_New();
	if (d == NULL) 
		return -1;
	else
		bpy_pydriver_Dict = d;

	/* import some modules: builtins, bpy, math, (Blender.noise )*/
	PyDict_SetItemString(d, "__builtins__", PyEval_GetBuiltins());

	mod = PyImport_ImportModule("math");
	if (mod) {
		PyDict_Merge(d, PyModule_GetDict(mod), 0); /* 0 - dont overwrite existing values */
		
		/* Only keep for backwards compat! - just import all math into root, they are standard */
		PyDict_SetItemString(d, "math", mod);
		PyDict_SetItemString(d, "m", mod);
		Py_DECREF(mod);
	} 
	
	/* add bpy to global namespace */
	mod= PyImport_ImportModuleLevel("bpy", NULL, NULL, NULL, 0);
	if (mod) {
		PyDict_SetItemString(bpy_pydriver_Dict, "bpy", mod);
		Py_DECREF(mod);
	}
	
	
#if 0 // non existant yet
	mod = PyImport_ImportModule("Blender.Noise");
	if (mod) {
		PyDict_SetItemString(d, "noise", mod);
		PyDict_SetItemString(d, "n", mod);
		Py_DECREF(mod);
	} else {
		PyErr_Clear();
	}
	
	/* If there's a Blender text called pydrivers.py, import it.
	 * Users can add their own functions to this module. 
	 */
	if (G.f & G_DOSCRIPTLINKS) {
		mod = importText("pydrivers"); /* can also use PyImport_Import() */
		if (mod) {
			PyDict_SetItemString(d, "pydrivers", mod);
			PyDict_SetItemString(d, "p", mod);
			Py_DECREF(mod);
		} else {
			PyErr_Clear();
		}
	}
#endif // non existant yet
	
	return 0;
}

/* Update function, it gets rid of pydrivers global dictionary, forcing
 * BPY_pydriver_eval to recreate it. This function is used to force
 * reloading the Blender text module "pydrivers.py", if available, so
 * updates in it reach pydriver evaluation. 
 */
void BPY_pydriver_update(void)
{
	PyGILState_STATE gilstate = PyGILState_Ensure();

	if (bpy_pydriver_Dict) { /* free the global dict used by pydrivers */
		PyDict_Clear(bpy_pydriver_Dict);
		Py_DECREF(bpy_pydriver_Dict);
		bpy_pydriver_Dict = NULL;
	}

	PyGILState_Release(gilstate);

	return;
}

/* error return function for BPY_eval_pydriver */
static float pydriver_error(ChannelDriver *driver) 
{
	if (bpy_pydriver_Dict) { /* free the global dict used by pydrivers */
		PyDict_Clear(bpy_pydriver_Dict);
		Py_DECREF(bpy_pydriver_Dict);
		bpy_pydriver_Dict = NULL;
	}

	driver->flag |= DRIVER_FLAG_INVALID; /* py expression failed */
	fprintf(stderr, "\nError in Driver: The following Python expression failed:\n\t'%s'\n\n", driver->expression);
	
	PyErr_Print(); PyErr_Clear();

	return 0.0f;
}

/* This evals py driver expressions, 'expr' is a Python expression that
 * should evaluate to a float number, which is returned. 
 */
float BPY_pydriver_eval (ChannelDriver *driver)
{
	PyObject *driver_vars=NULL;
	PyObject *retval;
	PyGILState_STATE gilstate;
	
	DriverTarget *dtar;
	float result = 0.0f; /* default return */
	char *expr = NULL;
	short targets_ok= 1;
	
	/* sanity checks - should driver be executed? */
	if ((driver == NULL) /*|| (G.f & G_DOSCRIPTLINKS)==0*/) 
		return result;
	
	/* get the py expression to be evaluated */
	expr = driver->expression; 
	if ((expr == NULL) || (expr[0]=='\0')) 
		return result;

	gilstate = PyGILState_Ensure();
	
	/* init global dictionary for py-driver evaluation settings */
	if (!bpy_pydriver_Dict) {
		if (bpy_pydriver_create_dict() != 0) {
			fprintf(stderr, "Pydriver error: couldn't create Python dictionary");
			PyGILState_Release(gilstate);
			return result;
		}
	}
	
	/* add target values to a dict that will be used as '__locals__' dict */
	driver_vars = PyDict_New(); // XXX do we need to decref this?
	for (dtar= driver->targets.first; dtar; dtar= dtar->next) {
		PyObject *driver_arg = NULL;
		float tval = 0.0f;
		
		/* try to get variable value */
		tval= driver_get_target_value(driver, dtar);
		driver_arg= PyFloat_FromDouble((double)tval);
		
		/* try to add to dictionary */
		if (PyDict_SetItemString(driver_vars, dtar->name, driver_arg)) {
			/* this target failed - bad name */
			if (targets_ok) {
				/* first one - print some extra info for easier identification */
				fprintf(stderr, "\nBPY_pydriver_eval() - Error while evaluating PyDriver:\n");
				targets_ok= 0;
			}
			
			fprintf(stderr, "\tBPY_pydriver_eval() - couldn't add variable '%s' to namespace \n", dtar->name);
			PyErr_Print(); PyErr_Clear();
		}
	}
	
	/* execute expression to get a value */
	retval = PyRun_String(expr, Py_eval_input, bpy_pydriver_Dict, driver_vars);
	
	/* decref the driver vars first...  */
	Py_DECREF(driver_vars);
	
	/* process the result */
	if (retval == NULL) {
		result = pydriver_error(driver);
		PyGILState_Release(gilstate);
		return result;
	}

	result = (float)PyFloat_AsDouble(retval);
	Py_DECREF(retval);
	
	if ((result == -1) && PyErr_Occurred()) {
		result = pydriver_error(driver);
		PyGILState_Release(gilstate);
		return result;
	}
	
	/* all fine, make sure the "invalid expression" flag is cleared */
	driver->flag &= ~DRIVER_FLAG_INVALID;

	PyGILState_Release(gilstate);

	return result;
}
