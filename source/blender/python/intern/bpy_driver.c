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
 * Contributor(s): Willian P. Germano, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */
/* ****************************************** */
/* Drivers - PyExpression Evaluation */

#include "DNA_anim_types.h"

#include "BPY_extern.h"
#include "BKE_fcurve.h"

#include <Python.h>

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

	// BPy_errors_to_report(NULL); // TODO - reports
	PyErr_Print();
	PyErr_Clear();

	return 0.0f;
}

/* This evals py driver expressions, 'expr' is a Python expression that
 * should evaluate to a float number, which is returned.
 */
float BPY_pydriver_eval (ChannelDriver *driver)
{
	PyObject *driver_vars=NULL;
	PyObject *retval= NULL;
	PyGILState_STATE gilstate;

	DriverVar *dvar;
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
	for (dvar= driver->variables.first; dvar; dvar= dvar->next) {
		PyObject *driver_arg = NULL;
		float tval = 0.0f;
		
		/* try to get variable value */
		tval= driver_get_variable_value(driver, dvar);
		driver_arg= PyFloat_FromDouble((double)tval);
		
		/* try to add to dictionary */
		if (PyDict_SetItemString(driver_vars, dvar->name, driver_arg)) {
			/* this target failed - bad name */
			if (targets_ok) {
				/* first one - print some extra info for easier identification */
				fprintf(stderr, "\nBPY_pydriver_eval() - Error while evaluating PyDriver:\n");
				targets_ok= 0;
			}
			
			fprintf(stderr, "\tBPY_pydriver_eval() - couldn't add variable '%s' to namespace \n", dvar->name);
			// BPy_errors_to_report(NULL); // TODO - reports
			PyErr_Print();
			PyErr_Clear();
		}
	}

#if 0 // slow
	/* execute expression to get a value */
	retval = PyRun_String(expr, Py_eval_input, bpy_pydriver_Dict, driver_vars);
#else
	/* compile the expression first if it hasn't been compiled or needs to be rebuilt */
	if((driver->flag & DRIVER_FLAG_RECOMPILE) || (driver->expr_comp==NULL)) {
		Py_XDECREF(driver->expr_comp);
		driver->expr_comp= Py_CompileString(expr, "<bpy driver>", Py_eval_input);
		driver->flag &= ~DRIVER_FLAG_RECOMPILE;
	}
	/* evaluate the compiled expression */
	if (driver->expr_comp)
		retval= PyEval_EvalCode(driver->expr_comp, bpy_pydriver_Dict, driver_vars);
#endif

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
