/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Willian P. Germano, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/intern/bpy_driver.c
 *  \ingroup pythonintern
 *
 * This file defines the 'BPY_driver_exec' to execute python driver expressions,
 * called by the animation system, there are also some utility functions
 * to deal with the namespace used for driver execution.
 */

/* ****************************************** */
/* Drivers - PyExpression Evaluation */

#include <Python.h>

#include "DNA_anim_types.h"

#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_string.h"

#include "BKE_fcurve.h"
#include "BKE_global.h"

#include "bpy_rna_driver.h"  /* for pyrna_driver_get_variable_value */

#include "bpy_intern_string.h"

#include "bpy_driver.h"

extern void BPY_update_rna_module(void);

#define USE_RNA_AS_PYOBJECT

/* for pydrivers (drivers using one-line Python expressions to express relationships between targets) */
PyObject *bpy_pydriver_Dict = NULL;

/* For faster execution we keep a special dictionary for pydrivers, with
 * the needed modules and aliases.
 */
int bpy_pydriver_create_dict(void)
{
	PyObject *d, *mod;

	/* validate namespace for driver evaluation */
	if (bpy_pydriver_Dict) return -1;

	d = PyDict_New();
	if (d == NULL)
		return -1;
	else
		bpy_pydriver_Dict = d;

	/* import some modules: builtins, bpy, math, (Blender.noise)*/
	PyDict_SetItemString(d, "__builtins__", PyEval_GetBuiltins());

	mod = PyImport_ImportModule("math");
	if (mod) {
		PyDict_Merge(d, PyModule_GetDict(mod), 0); /* 0 - don't overwrite existing values */
		Py_DECREF(mod);
	}

	/* add bpy to global namespace */
	mod = PyImport_ImportModuleLevel("bpy", NULL, NULL, NULL, 0);
	if (mod) {
		PyDict_SetItemString(bpy_pydriver_Dict, "bpy", mod);
		Py_DECREF(mod);
	}

	/* add noise to global namespace */
	mod = PyImport_ImportModuleLevel("mathutils", NULL, NULL, NULL, 0);
	if (mod) {
		PyObject *modsub = PyDict_GetItemString(PyModule_GetDict(mod), "noise");
		PyDict_SetItemString(bpy_pydriver_Dict, "noise", modsub);
		Py_DECREF(mod);
	}

	return 0;
}

/* note, this function should do nothing most runs, only when changing frame */
/* not thread safe but neither is python */
static struct {
	float evaltime;

	/* borrowed reference to the 'self' in 'bpy_pydriver_Dict'
	 * keep for as long as the same self is used. */
	PyObject *self;
} g_pydriver_state_prev = {
	.evaltime = FLT_MAX,
	.self = NULL,
};

static void bpy_pydriver_namespace_update_frame(const float evaltime)
{
	if (g_pydriver_state_prev.evaltime != evaltime) {
		PyObject *item = PyFloat_FromDouble(evaltime);
		PyDict_SetItem(bpy_pydriver_Dict, bpy_intern_str_frame, item);
		Py_DECREF(item);

		g_pydriver_state_prev.evaltime = evaltime;
	}
}

static void bpy_pydriver_namespace_update_self(struct PathResolvedRNA *anim_rna)
{
	if ((g_pydriver_state_prev.self == NULL) ||
	    (pyrna_driver_is_equal_anim_rna(anim_rna, g_pydriver_state_prev.self) == false))
	{
		PyObject *item = pyrna_driver_self_from_anim_rna(anim_rna);
		PyDict_SetItem(bpy_pydriver_Dict, bpy_intern_str_self, item);
		Py_DECREF(item);

		g_pydriver_state_prev.self = item;
	}
}

static void bpy_pydriver_namespace_clear_self(void)
{
	if (g_pydriver_state_prev.self) {
		PyDict_DelItem(bpy_pydriver_Dict, bpy_intern_str_self);

		g_pydriver_state_prev.self = NULL;
	}
}

/* Update function, it gets rid of pydrivers global dictionary, forcing
 * BPY_driver_exec to recreate it. This function is used to force
 * reloading the Blender text module "pydrivers.py", if available, so
 * updates in it reach pydriver evaluation.
 */
void BPY_driver_reset(void)
{
	PyGILState_STATE gilstate;
	bool use_gil = true; /* !PyC_IsInterpreterActive(); */

	if (use_gil)
		gilstate = PyGILState_Ensure();

	if (bpy_pydriver_Dict) { /* free the global dict used by pydrivers */
		PyDict_Clear(bpy_pydriver_Dict);
		Py_DECREF(bpy_pydriver_Dict);
		bpy_pydriver_Dict = NULL;
	}

	g_pydriver_state_prev.evaltime = FLT_MAX;

	/* freed when clearing driver dict */
	g_pydriver_state_prev.self = NULL;

	if (use_gil)
		PyGILState_Release(gilstate);

	return;
}

/* error return function for BPY_eval_pydriver */
static void pydriver_error(ChannelDriver *driver)
{
	driver->flag |= DRIVER_FLAG_INVALID; /* py expression failed */
	fprintf(stderr, "\nError in Driver: The following Python expression failed:\n\t'%s'\n\n", driver->expression);

	// BPy_errors_to_report(NULL); // TODO - reports
	PyErr_Print();
	PyErr_Clear();
}

/* This evals py driver expressions, 'expr' is a Python expression that
 * should evaluate to a float number, which is returned.
 *
 * (old)note: PyGILState_Ensure() isn't always called because python can call
 * the bake operator which intern starts a thread which calls scene update
 * which does a driver update. to avoid a deadlock check PyC_IsInterpreterActive()
 * if PyGILState_Ensure() is needed - see [#27683]
 *
 * (new)note: checking if python is running is not threadsafe [#28114]
 * now release the GIL on python operator execution instead, using
 * PyEval_SaveThread() / PyEval_RestoreThread() so we don't lock up blender.
 */
float BPY_driver_exec(struct PathResolvedRNA *anim_rna, ChannelDriver *driver, const float evaltime)
{
	PyObject *driver_vars = NULL;
	PyObject *retval = NULL;
	PyObject *expr_vars; /* speed up by pre-hashing string & avoids re-converting unicode strings for every execution */
	PyObject *expr_code;
	PyGILState_STATE gilstate;
	bool use_gil;

	DriverVar *dvar;
	double result = 0.0; /* default return */
	const char *expr;
	short targets_ok = 1;
	int i;

	/* get the py expression to be evaluated */
	expr = driver->expression;
	if (expr[0] == '\0')
		return 0.0f;

	if (!(G.f & G_SCRIPT_AUTOEXEC)) {
		if (!(G.f & G_SCRIPT_AUTOEXEC_FAIL_QUIET)) {
			G.f |= G_SCRIPT_AUTOEXEC_FAIL;
			BLI_snprintf(G.autoexec_fail, sizeof(G.autoexec_fail), "Driver '%s'", expr);

			printf("skipping driver '%s', automatic scripts are disabled\n", expr);
		}
		return 0.0f;
	}

	use_gil = true;  /* !PyC_IsInterpreterActive(); */

	if (use_gil)
		gilstate = PyGILState_Ensure();

	/* needed since drivers are updated directly after undo where 'main' is
	 * re-allocated [#28807] */
	BPY_update_rna_module();

	/* init global dictionary for py-driver evaluation settings */
	if (!bpy_pydriver_Dict) {
		if (bpy_pydriver_create_dict() != 0) {
			fprintf(stderr, "PyDriver error: couldn't create Python dictionary\n");
			if (use_gil)
				PyGILState_Release(gilstate);
			return 0.0f;
		}
	}

	/* update global namespace */
	bpy_pydriver_namespace_update_frame(evaltime);

	if (driver->flag & DRIVER_FLAG_USE_SELF) {
		bpy_pydriver_namespace_update_self(anim_rna);
	}
	else {
		bpy_pydriver_namespace_clear_self();
	}

	if (driver->expr_comp == NULL)
		driver->flag |= DRIVER_FLAG_RECOMPILE;

	/* compile the expression first if it hasn't been compiled or needs to be rebuilt */
	if (driver->flag & DRIVER_FLAG_RECOMPILE) {
		Py_XDECREF(driver->expr_comp);
		driver->expr_comp = PyTuple_New(2);

		expr_code = Py_CompileString(expr, "<bpy driver>", Py_eval_input);
		PyTuple_SET_ITEM(((PyObject *)driver->expr_comp), 0, expr_code);

		driver->flag &= ~DRIVER_FLAG_RECOMPILE;
		driver->flag |= DRIVER_FLAG_RENAMEVAR; /* maybe this can be removed but for now best keep until were sure */
	}
	else {
		expr_code = PyTuple_GET_ITEM(((PyObject *)driver->expr_comp), 0);
	}

	if (driver->flag & DRIVER_FLAG_RENAMEVAR) {
		/* may not be set */
		expr_vars = PyTuple_GET_ITEM(((PyObject *)driver->expr_comp), 1);
		Py_XDECREF(expr_vars);

		expr_vars = PyTuple_New(BLI_listbase_count(&driver->variables));
		PyTuple_SET_ITEM(((PyObject *)driver->expr_comp), 1, expr_vars);

		for (dvar = driver->variables.first, i = 0; dvar; dvar = dvar->next) {
			PyTuple_SET_ITEM(expr_vars, i++, PyUnicode_FromString(dvar->name));
		}
		
		driver->flag &= ~DRIVER_FLAG_RENAMEVAR;
	}
	else {
		expr_vars = PyTuple_GET_ITEM(((PyObject *)driver->expr_comp), 1);
	}

	/* add target values to a dict that will be used as '__locals__' dict */
	driver_vars = _PyDict_NewPresized(PyTuple_GET_SIZE(expr_vars));
	for (dvar = driver->variables.first, i = 0; dvar; dvar = dvar->next) {
		PyObject *driver_arg = NULL;

	/* support for any RNA data */
#ifdef USE_RNA_AS_PYOBJECT
		if (dvar->type == DVAR_TYPE_SINGLE_PROP) {
			driver_arg = pyrna_driver_get_variable_value(driver, &dvar->targets[0]);

			if (driver_arg == NULL) {
				driver_arg = PyFloat_FromDouble(0.0);
				dvar->curval = 0.0f;
			}
			else {
				/* no need to worry about overflow here, values from RNA are within limits. */
				if (PyFloat_CheckExact(driver_arg)) {
					dvar->curval = (float)PyFloat_AsDouble(driver_arg);
				}
				else if (PyLong_CheckExact(driver_arg)) {
					dvar->curval = (float)PyLong_AsLong(driver_arg);
				}
				else if (PyBool_Check(driver_arg)) {
					dvar->curval = (driver_arg == Py_True);
				}
				else {
					dvar->curval = 0.0f;
				}
			}
		}
		else
#endif
		{
			/* try to get variable value */
			float tval = driver_get_variable_value(driver, dvar);
			driver_arg = PyFloat_FromDouble((double)tval);
		}

		/* try to add to dictionary */
		/* if (PyDict_SetItemString(driver_vars, dvar->name, driver_arg)) { */
		if (PyDict_SetItem(driver_vars, PyTuple_GET_ITEM(expr_vars, i++), driver_arg) != -1) {
			Py_DECREF(driver_arg);
		}
		else {
			/* this target failed - bad name */
			if (targets_ok) {
				/* first one - print some extra info for easier identification */
				fprintf(stderr, "\nBPY_driver_eval() - Error while evaluating PyDriver:\n");
				targets_ok = 0;
			}
			
			fprintf(stderr, "\tBPY_driver_eval() - couldn't add variable '%s' to namespace\n", dvar->name);
			// BPy_errors_to_report(NULL); // TODO - reports
			PyErr_Print();
			PyErr_Clear();
		}
	}


#if 0  /* slow, with this can avoid all Py_CompileString above. */
	/* execute expression to get a value */
	retval = PyRun_String(expr, Py_eval_input, bpy_pydriver_Dict, driver_vars);
#else
	/* evaluate the compiled expression */
	if (expr_code)
		retval = PyEval_EvalCode((void *)expr_code, bpy_pydriver_Dict, driver_vars);
#endif

	/* decref the driver vars first...  */
	Py_DECREF(driver_vars);

	/* process the result */
	if (retval == NULL) {
		pydriver_error(driver);
	}
	else if ((result = PyFloat_AsDouble(retval)) == -1.0 && PyErr_Occurred()) {
		pydriver_error(driver);
		Py_DECREF(retval);
		result = 0.0;
	}
	else {
		/* all fine, make sure the "invalid expression" flag is cleared */
		driver->flag &= ~DRIVER_FLAG_INVALID;
		Py_DECREF(retval);
	}

	if (use_gil)
		PyGILState_Release(gilstate);

	if (isfinite(result)) {
		return (float)result;
	}
	else {
		fprintf(stderr, "\tBPY_driver_eval() - driver '%s' evaluates to '%f'\n", dvar->name, result);
		return 0.0f;
	}
}
