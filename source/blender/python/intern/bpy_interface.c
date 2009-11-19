/**
 * $Id:
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
 * Contributor(s): Michel Selten, Willian P. Germano, Stephen Swaney,
 * Chris Keith, Chris Want, Ken Hughes, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>


/* grr, python redefines */
#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif

#include <Python.h>
#include "compile.h"		/* for the PyCodeObject */
#include "eval.h"		/* for PyEval_EvalCode */

#include "bpy_rna.h"
#include "bpy_operator.h"
#include "bpy_ui.h"
#include "bpy_util.h"

#ifndef WIN32
#include <dirent.h>
#else
#include "BLI_winstuff.h"
#endif

#include "DNA_anim_types.h"
#include "DNA_space_types.h"
#include "DNA_text_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_util.h"
#include "BLI_storage.h"
#include "BLI_fileops.h"
#include "BLI_string.h"

#include "BKE_context.h"
#include "BKE_fcurve.h"
#include "BKE_text.h"
#include "BKE_context.h"
#include "BKE_global.h"

#include "BPY_extern.h"

#include "../generic/bpy_internal_import.h" // our own imports
/* external util modules */

#include "../generic/Mathutils.h"
#include "../generic/Geometry.h"
#include "../generic/BGL.h"
#include "../generic/IDProp.h"


/* for internal use, when starting and ending python scripts */

/* incase a python script triggers another python call, stop bpy_context_clear from invalidating */
static int py_call_level= 0;


// only for tests
#define TIME_PY_RUN

#ifdef TIME_PY_RUN
#include "PIL_time.h"
static int		bpy_timer_count = 0;
static double	bpy_timer; /* time since python starts */
static double	bpy_timer_run; /* time for each python script run */
static double	bpy_timer_run_tot; /* accumulate python runs */
#endif

void bpy_context_set(bContext *C, PyGILState_STATE *gilstate)
{
	py_call_level++;

	if(gilstate)
		*gilstate = PyGILState_Ensure();

	if(py_call_level==1) {

		if(C) { // XXX - should always be true.
			BPy_SetContext(C);
			bpy_import_main_set(CTX_data_main(C));
		}
		else {
			fprintf(stderr, "ERROR: Python context called with a NULL Context. this should not happen!\n");
		}

		BPY_update_modules(); /* can give really bad results if this isnt here */

#ifdef TIME_PY_RUN
		if(bpy_timer_count==0) {
			/* record time from the beginning */
			bpy_timer= PIL_check_seconds_timer();
			bpy_timer_run = bpy_timer_run_tot = 0.0;
		}
		bpy_timer_run= PIL_check_seconds_timer();


		bpy_timer_count++;
#endif
	}
}

void bpy_context_clear(bContext *C, PyGILState_STATE *gilstate)
{
	py_call_level--;

	if(gilstate)
		PyGILState_Release(*gilstate);

	if(py_call_level < 0) {
		fprintf(stderr, "ERROR: Python context internal state bug. this should not happen!\n");
	}
	else if(py_call_level==0) {
		// XXX - Calling classes currently wont store the context :\, cant set NULL because of this. but this is very flakey still.
		//BPy_SetContext(NULL);
		//bpy_import_main_set(NULL);

#ifdef TIME_PY_RUN
		bpy_timer_run_tot += PIL_check_seconds_timer() - bpy_timer_run;
		bpy_timer_count++;
#endif

	}
}

static void bpy_import_test(char *modname)
{
	PyObject *mod= PyImport_ImportModuleLevel(modname, NULL, NULL, NULL, 0);
	if(mod) {
		Py_DECREF(mod);
	}
	else {
		PyErr_Print();
		PyErr_Clear();
	}	
}

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
static BPy_StructRNA *bpy_context_module= NULL; /* for fast access */
static void bpy_init_modules( void )
{
	PyObject *mod;

	/* Needs to be first since this dir is needed for future modules */
	char *modpath= BLI_gethome_folder("scripts/modules", BLI_GETHOME_ALL);
	if(modpath) {
		PyObject *sys_path= PySys_GetObject("path"); /* borrow */
		PyObject *py_modpath= PyUnicode_FromString(modpath);
		PyList_Insert(sys_path, 0, py_modpath); /* add first */
		Py_DECREF(py_modpath);
	}
	
	/* stand alone utility modules not related to blender directly */
	Geometry_Init();
	Mathutils_Init();
	BGL_Init();
	IDProp_Init_Types();


	mod = PyModule_New("_bpy");

	/* add the module so we can import it */
	PyDict_SetItemString(PySys_GetObject("modules"), "_bpy", mod);
	Py_DECREF(mod);

	/* run first, initializes rna types */
	BPY_rna_init();

	PyModule_AddObject( mod, "types", BPY_rna_types() ); /* needs to be first so bpy_types can run */
	bpy_import_test("bpy_types");
	PyModule_AddObject( mod, "data", BPY_rna_module() ); /* imports bpy_types by running this */
	bpy_import_test("bpy_types");
	/* PyModule_AddObject( mod, "doc", BPY_rna_doc() ); */
	PyModule_AddObject( mod, "props", BPY_rna_props() );
	PyModule_AddObject( mod, "ops", BPY_operator_module() ); /* ops is now a python module that does the conversion from SOME_OT_foo -> some.foo */
	PyModule_AddObject( mod, "ui", BPY_ui_module() ); // XXX very experimental, consider this a test, especially PyCObject is not meant to be permanent



	/* bpy context */
	{
		bpy_context_module= ( BPy_StructRNA * ) PyObject_NEW( BPy_StructRNA, &pyrna_struct_Type );
		RNA_pointer_create(NULL, &RNA_Context, NULL, &bpy_context_module->ptr);

		PyModule_AddObject(mod, "context", (PyObject *)bpy_context_module);
	}

	/* add our own modules dir, this is a python package */
	bpy_import_test("bpy");
}

void BPY_update_modules( void )
{
#if 0 // slow, this runs all the time poll, draw etc 100's of time a sec.
	PyObject *mod= PyImport_ImportModuleLevel("bpy", NULL, NULL, NULL, 0);
	PyModule_AddObject( mod, "data", BPY_rna_module() );
	PyModule_AddObject( mod, "types", BPY_rna_types() ); // atm this does not need updating
#endif

	/* refreshes the main struct */
	BPY_update_rna_module();
	bpy_context_module->ptr.data= (void *)BPy_GetContext();
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
	
	// XXX - put somewhere more logical
	{
		PyMethodDef *ml;
		static PyMethodDef bpy_prop_meths[] = {
			{"FloatProperty", (PyCFunction)BPy_FloatProperty, METH_VARARGS|METH_KEYWORDS, ""},
			{"IntProperty", (PyCFunction)BPy_IntProperty, METH_VARARGS|METH_KEYWORDS, ""},
			{"BoolProperty", (PyCFunction)BPy_BoolProperty, METH_VARARGS|METH_KEYWORDS, ""},
			{"StringProperty", (PyCFunction)BPy_StringProperty, METH_VARARGS|METH_KEYWORDS, ""},
			{"EnumProperty", (PyCFunction)BPy_EnumProperty, METH_VARARGS|METH_KEYWORDS, ""},
			{"PointerProperty", (PyCFunction)BPy_PointerProperty, METH_VARARGS|METH_KEYWORDS, ""},
			{"CollectionProperty", (PyCFunction)BPy_CollectionProperty, METH_VARARGS|METH_KEYWORDS, ""},
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

/* must be called before Py_Initialize */
void BPY_start_python_path(void)
{
	char *py_path_bundle= BLI_gethome_folder("python", BLI_GETHOME_ALL);

	if(py_path_bundle==NULL)
		return;

	/* set the environment path */
	printf("found bundled python: %s\n", py_path_bundle);

#if 0
	BLI_setenv("PYTHONHOME", py_path_bundle);
	BLI_setenv("PYTHONPATH", py_path_bundle);
#endif

	{
		static wchar_t py_path_bundle_wchar[FILE_MAXDIR];

		mbstowcs(py_path_bundle_wchar, py_path_bundle, FILE_MAXDIR);
		Py_SetPythonHome(py_path_bundle_wchar);
	}
}



void BPY_set_context(bContext *C)
{
	BPy_SetContext(C);
}

/* call BPY_set_context first */
void BPY_start_python( int argc, char **argv )
{
	PyThreadState *py_tstate = NULL;
	
	BPY_start_python_path(); /* allow to use our own included python */

	Py_Initialize(  );
	
	// PySys_SetArgv( argc, argv); // broken in py3, not a huge deal
	/* sigh, why do python guys not have a char** version anymore? :( */
	{
		int i;
		PyObject *py_argv= PyList_New(argc);

		for (i=0; i<argc; i++)
			PyList_SET_ITEM(py_argv, i, PyUnicode_FromString(argv[i]));

		PySys_SetObject("argv", py_argv);
		Py_DECREF(py_argv);
	}
	
	/* Initialize thread support (also acquires lock) */
	PyEval_InitThreads();
	
	
	/* bpy.* and lets us import it */
	bpy_init_modules(); 

	{ /* our own import and reload functions */
		PyObject *item;
		//PyObject *m = PyImport_AddModule("__builtin__");
		//PyObject *d = PyModule_GetDict(m);
		PyObject *d = PyEval_GetBuiltins(  );
		PyDict_SetItemString(d, "reload",		item=PyCFunction_New(bpy_reload_meth, NULL));	Py_DECREF(item);
		PyDict_SetItemString(d, "__import__",	item=PyCFunction_New(bpy_import_meth, NULL));	Py_DECREF(item);
	}
	
	pyrna_alloc_types();

	py_tstate = PyGILState_GetThisThreadState();
	PyEval_ReleaseThread(py_tstate);
}

void BPY_end_python( void )
{
	// fprintf(stderr, "Ending Python!\n");

	PyGILState_Ensure(); /* finalizing, no need to grab the state */
	
	// free other python data.
	pyrna_free_types();

	/* clear all python data from structs */
	
	Py_Finalize(  );
	
#ifdef TIME_PY_RUN
	// measure time since py started
	bpy_timer = PIL_check_seconds_timer() - bpy_timer;

	printf("*bpy stats* - ");
	printf("tot exec: %d,  ", bpy_timer_count);
	printf("tot run: %.4fsec,  ", bpy_timer_run_tot);
	if(bpy_timer_count>0)
		printf("average run: %.6fsec,  ", (bpy_timer_run_tot/bpy_timer_count));

	if(bpy_timer>0.0)
		printf("tot usage %.4f%%", (bpy_timer_run_tot/bpy_timer)*100.0);

	printf("\n");

	// fprintf(stderr, "Ending Python Done!\n");

#endif

}

/* Can run a file or text block */
int BPY_run_python_script( bContext *C, const char *fn, struct Text *text, struct ReportList *reports)
{
	PyObject *py_dict, *py_result= NULL;
	PyGILState_STATE gilstate;
	
	if (fn==NULL && text==NULL) {
		return 0;
	}
	
	bpy_context_set(C, &gilstate);
	
	py_dict = CreateGlobalDictionary(C);

	if (text) {
		
		if( !text->compiled ) {	/* if it wasn't already compiled, do it now */
			char *buf = txt_to_buf( text );

			text->compiled =
				Py_CompileString( buf, text->id.name+2, Py_file_input );

			MEM_freeN( buf );

			if( PyErr_Occurred(  ) ) {
				BPY_free_compiled_text( text );
			}
		}
		if(text->compiled)
			py_result =  PyEval_EvalCode( text->compiled, py_dict, py_dict );
		
	} else {
		FILE *fp= fopen(fn, "r");		
		if(fp) {
#ifdef _WIN32
			/* Previously we used PyRun_File to run directly the code on a FILE 
			 * object, but as written in the Python/C API Ref Manual, chapter 2,
			 * 'FILE structs for different C libraries can be different and 
			 * incompatible'.
			 * So now we load the script file data to a buffer */
			char *pystring;

			fclose(fp);

			pystring= malloc(strlen(fn) + 32);
			pystring[0]= '\0';
			sprintf(pystring, "exec(open(r'%s').read())", fn);
			py_result = PyRun_String( pystring, Py_file_input, py_dict, py_dict );
			free(pystring);
#else
			py_result = PyRun_File(fp, fn, Py_file_input, py_dict, py_dict);
			fclose(fp);
#endif
		}
		else {
			PyErr_Format(PyExc_SystemError, "Python file \"%s\" could not be opened: %s", fn, strerror(errno));
			py_result= NULL;
		}
	}
	
	if (!py_result) {
		BPy_errors_to_report(reports);
	} else {
		Py_DECREF( py_result );
	}
	
	Py_DECREF(py_dict);
	
	bpy_context_clear(C, &gilstate);

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
		BPy_errors_to_report(NULL); // TODO, reports
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
		BPY_run_python_script(C, sc->script->scriptname, NULL, NULL);
		
	if (sc->script->py_draw==NULL)
		return 0;
	
	return 1;
}

int BPY_run_script_space_draw(const struct bContext *C, SpaceScript * sc)
{
	if (bpy_run_script_init( (bContext *)C, sc)) {
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
		BPy_errors_to_report(NULL); // TODO - reports
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
	
	BPy_errors_to_report(NULL); // TODO - reports

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
			BPy_errors_to_report(NULL); // TODO - reports
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

int BPY_button_eval(bContext *C, char *expr, double *value)
{
	PyGILState_STATE gilstate;
	PyObject *dict, *mod, *retval;
	int error_ret = 0;
	
	if (!value || !expr || expr[0]=='\0') return -1;
	
	bpy_context_set(C, &gilstate);
	
	dict= CreateGlobalDictionary(C);
	
	/* import some modules: builtins,math*/
	PyDict_SetItemString(dict, "__builtins__", PyEval_GetBuiltins());

	mod = PyImport_ImportModule("math");
	if (mod) {
		PyDict_Merge(dict, PyModule_GetDict(mod), 0); /* 0 - dont overwrite existing values */
		
		/* Only keep for backwards compat! - just import all math into root, they are standard */
		PyDict_SetItemString(dict, "math", mod);
		PyDict_SetItemString(dict, "m", mod);
		Py_DECREF(mod);
	} 
	
	retval = PyRun_String(expr, Py_eval_input, dict, dict);
	
	if (retval == NULL) {
		error_ret= -1;
	}
	else {
		double val;

		if(PyTuple_Check(retval)) {
			/* Users my have typed in 10km, 2m
			 * add up all values */
			int i;
			val= 0.0;

			for(i=0; i<PyTuple_GET_SIZE(retval); i++) {
				val+= PyFloat_AsDouble(PyTuple_GET_ITEM(retval, i));
			}
		}
		else {
			val = PyFloat_AsDouble(retval);
		}
		Py_DECREF(retval);
		
		if(val==-1 && PyErr_Occurred()) {
			error_ret= -1;
		}
		else {
			*value= val;
		}
	}
	
	if(error_ret) {
		BPy_errors_to_report(CTX_wm_reports(C));
	}
	
	Py_DECREF(dict);
	bpy_context_clear(C, &gilstate);
	
	return error_ret;
}



int BPY_context_get(bContext *C, const char *member, bContextDataResult *result)
{
	PyObject *pyctx= (PyObject *)CTX_py_dict_get(C);
	PyObject *item= PyDict_GetItemString(pyctx, member);
	PointerRNA *ptr= NULL;
	int done= 0;

	if(item==NULL) {
		/* pass */
	}
	else if(item==Py_None) {
		/* pass */
	}
	else if(BPy_StructRNA_Check(item)) {
		ptr= &(((BPy_StructRNA *)item)->ptr);

		//result->ptr= ((BPy_StructRNA *)item)->ptr;
		CTX_data_pointer_set(result, ptr->id.data, ptr->type, ptr->data);
		done= 1;
	}
	else if (PySequence_Check(item)) {
		PyObject *seq_fast= PySequence_Fast(item, "bpy_context_get sequence conversion");
		if (seq_fast==NULL) {
			PyErr_Print();
			PyErr_Clear();
		}
		else {
			int len= PySequence_Fast_GET_SIZE(seq_fast);
			int i;
			for(i = 0; i < len; i++) {
				PyObject *list_item= PySequence_Fast_GET_ITEM(seq_fast, i);

				if(BPy_StructRNA_Check(list_item)) {
					/*
					CollectionPointerLink *link= MEM_callocN(sizeof(CollectionPointerLink), "bpy_context_get");
					link->ptr= ((BPy_StructRNA *)item)->ptr;
					BLI_addtail(&result->list, link);
					*/
					ptr= &(((BPy_StructRNA *)list_item)->ptr);
					CTX_data_list_add(result, ptr->id.data, ptr->type, ptr->data);
				}
				else {
					printf("List item not a valid type\n");
				}

			}
			Py_DECREF(seq_fast);

			done= 1;
		}
	}

	if(done==0) {
		if (item)	printf("Context '%s' not a valid type\n", member);
		else		printf("Context '%s' not found\n", member);
	}
	else if (G.f & G_DEBUG) {
		printf("Context '%s' found\n", member);
	}

	return done;
}

