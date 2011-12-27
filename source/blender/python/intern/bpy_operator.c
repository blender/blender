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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/intern/bpy_operator.c
 *  \ingroup pythonintern
 *
 * This file defines '_bpy.ops', an internal python module which gives python
 * the ability to inspect and call both C and Python defined operators.
 *
 * \note
 * This module is exposed to the user via 'release/scripts/modules/bpy/ops.py'
 * which fakes exposing operators as modules/functions using its own classes.
 */

#include <Python.h>

#include "RNA_types.h"

#include "BPY_extern.h"
#include "bpy_operator.h"
#include "bpy_operator_wrap.h"
#include "bpy_rna.h" /* for setting arg props only - pyrna_py_to_prop() */
#include "bpy_util.h"
#include "../generic/bpy_internal_import.h"

#include "BLI_utildefines.h"
#include "BLI_string.h"

#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"

#include "BKE_report.h"
#include "BKE_context.h"

/* so operators called can spawn threads which aquire the GIL */
#define BPY_RELEASE_GIL


static PyObject *pyop_poll(PyObject *UNUSED(self), PyObject *args)
{
	wmOperatorType *ot;
	char		*opname;
	PyObject	*context_dict = NULL; /* optional args */
	PyObject	*context_dict_back;
	char		*context_str = NULL;
	PyObject	*ret;

	int context = WM_OP_EXEC_DEFAULT;

	/* XXX Todo, work out a better solution for passing on context,
	 * could make a tuple from self and pack the name and Context into it... */
	bContext *C = (bContext *)BPy_GetContext();
	
	if (C == NULL) {
		PyErr_SetString(PyExc_RuntimeError, "Context is None, cant poll any operators");
		return NULL;
	}

	if (!PyArg_ParseTuple(args, "s|Os:_bpy.ops.poll", &opname, &context_dict, &context_str))
		return NULL;
	
	ot = WM_operatortype_find(opname, TRUE);

	if (ot == NULL) {
		PyErr_Format(PyExc_AttributeError,
		             "Polling operator \"bpy.ops.%s\" error, "
		             "could not be found", opname);
		return NULL;
	}

	if (context_str) {
		if (RNA_enum_value_from_id(operator_context_items, context_str, &context) == 0) {
			char *enum_str = BPy_enum_as_string(operator_context_items);
			PyErr_Format(PyExc_TypeError,
			             "Calling operator \"bpy.ops.%s.poll\" error, "
			             "expected a string enum in (%.200s)",
			             opname, enum_str);
			MEM_freeN(enum_str);
			return NULL;
		}
	}
	
	if (context_dict == NULL || context_dict == Py_None) {
		context_dict = NULL;
	}
	else if (!PyDict_Check(context_dict)) {
		PyErr_Format(PyExc_TypeError,
		             "Calling operator \"bpy.ops.%s.poll\" error, "
		             "custom context expected a dict or None, got a %.200s",
		             opname, Py_TYPE(context_dict)->tp_name);
		return NULL;
	}

	context_dict_back = CTX_py_dict_get(C);
	CTX_py_dict_set(C, (void *)context_dict);
	Py_XINCREF(context_dict); /* so we done loose it */
	
	/* main purpose of thsi function */
	ret = WM_operator_poll_context((bContext *)C, ot, context) ? Py_True : Py_False;
	
	/* restore with original context dict, probably NULL but need this for nested operator calls */
	Py_XDECREF(context_dict);
	CTX_py_dict_set(C, (void *)context_dict_back);
	
	Py_INCREF(ret);
	return ret;
}

static PyObject *pyop_call(PyObject *UNUSED(self), PyObject *args)
{
	wmOperatorType *ot;
	int error_val = 0;
	PointerRNA ptr;
	int operator_ret = OPERATOR_CANCELLED;

	char		*opname;
	char		*context_str = NULL;
	PyObject	*kw = NULL; /* optional args */
	PyObject	*context_dict = NULL; /* optional args */
	PyObject	*context_dict_back;

	/* note that context is an int, python does the conversion in this case */
	int context = WM_OP_EXEC_DEFAULT;

	/* XXX Todo, work out a better solution for passing on context,
	 * could make a tuple from self and pack the name and Context into it... */
	bContext *C = (bContext *)BPy_GetContext();
	
	if (C == NULL) {
		PyErr_SetString(PyExc_RuntimeError, "Context is None, cant poll any operators");
		return NULL;
	}
	
	if (!PyArg_ParseTuple(args, "sO|O!s:_bpy.ops.call", &opname, &context_dict, &PyDict_Type, &kw, &context_str))
		return NULL;

	ot = WM_operatortype_find(opname, TRUE);

	if (ot == NULL) {
		PyErr_Format(PyExc_AttributeError,
		             "Calling operator \"bpy.ops.%s\" error, "
		             "could not be found", opname);
		return NULL;
	}
	
	if (!pyrna_write_check()) {
		PyErr_Format(PyExc_RuntimeError,
		             "Calling operator \"bpy.ops.%s\" error, "
		             "can't modify blend data in this state (drawing/rendering)",
		             opname);
		return NULL;
	}

	if (context_str) {
		if (RNA_enum_value_from_id(operator_context_items, context_str, &context) == 0) {
			char *enum_str = BPy_enum_as_string(operator_context_items);
			PyErr_Format(PyExc_TypeError,
			             "Calling operator \"bpy.ops.%s\" error, "
			             "expected a string enum in (%.200s)",
			             opname, enum_str);
			MEM_freeN(enum_str);
			return NULL;
		}
	}

	if (context_dict == NULL || context_dict == Py_None) {
		context_dict = NULL;
	}
	else if (!PyDict_Check(context_dict)) {
		PyErr_Format(PyExc_TypeError,
		             "Calling operator \"bpy.ops.%s\" error, "
		             "custom context expected a dict or None, got a %.200s",
		             opname, Py_TYPE(context_dict)->tp_name);
		return NULL;
	}

	context_dict_back = CTX_py_dict_get(C);

	CTX_py_dict_set(C, (void *)context_dict);
	Py_XINCREF(context_dict); /* so we done loose it */

	if (WM_operator_poll_context((bContext *)C, ot, context) == FALSE) {
		const char *msg = CTX_wm_operator_poll_msg_get(C);
		PyErr_Format(PyExc_RuntimeError,
		             "Operator bpy.ops.%.200s.poll() %.200s",
		             opname, msg ? msg : "failed, context is incorrect");
		CTX_wm_operator_poll_msg_set(C, NULL); /* better set to NULL else it could be used again */
		error_val = -1;
	}
	else {
		WM_operator_properties_create_ptr(&ptr, ot);
		WM_operator_properties_sanitize(&ptr, 0);

		if (kw && PyDict_Size(kw))
			error_val = pyrna_pydict_to_props(&ptr, kw, 0, "Converting py args to operator properties: ");


		if (error_val == 0) {
			ReportList *reports;

			reports = MEM_mallocN(sizeof(ReportList), "wmOperatorReportList");
			BKE_reports_init(reports, RPT_STORE | RPT_OP_HOLD); /* own so these dont move into global reports */

#ifdef BPY_RELEASE_GIL
			/* release GIL, since a thread could be started from an operator
			 * that updates a driver */
			/* note: I havve not seen any examples of code that does this
			 * so it may not be officially supported but seems to work ok. */
			{
				PyThreadState *ts = PyEval_SaveThread();
#endif

				operator_ret = WM_operator_call_py(C, ot, context, &ptr, reports);

#ifdef BPY_RELEASE_GIL
				/* regain GIL */
				PyEval_RestoreThread(ts);
			}
#endif

			error_val = BPy_reports_to_error(reports, PyExc_RuntimeError, FALSE);

			/* operator output is nice to have in the terminal/console too */
			if (reports->list.first) {
				char *report_str = BKE_reports_string(reports, 0); /* all reports */
	
				if (report_str) {
					PySys_WriteStdout("%s\n", report_str);
					MEM_freeN(report_str);
				}
			}
	
			BKE_reports_clear(reports);
			if ((reports->flag & RPT_FREE) == 0)
			{
				MEM_freeN(reports);
			}
		}

		WM_operator_properties_free(&ptr);

#if 0
		/* if there is some way to know an operator takes args we should use this */
		{
			/* no props */
			if (kw != NULL) {
				PyErr_Format(PyExc_AttributeError,
				             "Operator \"%s\" does not take any args",
				             opname);
				return NULL;
			}

			WM_operator_name_call(C, opname, WM_OP_EXEC_DEFAULT, NULL);
		}
#endif
	}

	/* restore with original context dict, probably NULL but need this for nested operator calls */
	Py_XDECREF(context_dict);
	CTX_py_dict_set(C, (void *)context_dict_back);

	if (error_val == -1) {
		return NULL;
	}

	/* when calling  bpy.ops.wm.read_factory_settings() bpy.data's main pointer is freed by clear_globals(),
	 * further access will crash blender. setting context is not needed in this case, only calling because this
	 * function corrects bpy.data (internal Main pointer) */
	BPY_modules_update(C);

	/* needed for when WM_OT_read_factory_settings us called from within a script */
	bpy_import_main_set(CTX_data_main(C));

	/* return operator_ret as a bpy enum */
	return pyrna_enum_bitfield_to_py(operator_return_items, operator_ret);

}

static PyObject *pyop_as_string(PyObject *UNUSED(self), PyObject *args)
{
	wmOperatorType *ot;
	PointerRNA ptr;

	char		*opname;
	PyObject	*kw = NULL; /* optional args */
	int all_args = 1;
	int error_val = 0;

	char *buf = NULL;
	PyObject *pybuf;

	bContext *C = (bContext *)BPy_GetContext();

	if (C == NULL) {
		PyErr_SetString(PyExc_RuntimeError, "Context is None, cant get the string representation of this object.");
		return NULL;
	}
	
	if (!PyArg_ParseTuple(args, "s|O!i:_bpy.ops.as_string", &opname, &PyDict_Type, &kw, &all_args))
		return NULL;

	ot = WM_operatortype_find(opname, TRUE);

	if (ot == NULL) {
		PyErr_Format(PyExc_AttributeError,
		             "_bpy.ops.as_string: operator \"%.200s\" "
		             "could not be found", opname);
		return NULL;
	}

	/* WM_operator_properties_create(&ptr, opname); */
	/* Save another lookup */
	RNA_pointer_create(NULL, ot->srna, NULL, &ptr);

	if (kw && PyDict_Size(kw))
		error_val = pyrna_pydict_to_props(&ptr, kw, 0, "Converting py args to operator properties: ");

	if (error_val == 0)
		buf = WM_operator_pystring(C, ot, &ptr, all_args);

	WM_operator_properties_free(&ptr);

	if (error_val == -1) {
		return NULL;
	}

	if (buf) {
		pybuf = PyUnicode_FromString(buf);
		MEM_freeN(buf);
	}
	else {
		pybuf = PyUnicode_FromString("");
	}

	return pybuf;
}

static PyObject *pyop_dir(PyObject *UNUSED(self))
{
	GHashIterator *iter = WM_operatortype_iter();
	PyObject *list = PyList_New(0), *name;

	for ( ; !BLI_ghashIterator_isDone(iter); BLI_ghashIterator_step(iter)) {
		wmOperatorType *ot = BLI_ghashIterator_getValue(iter);

		name = PyUnicode_FromString(ot->idname);
		PyList_Append(list, name);
		Py_DECREF(name);
	}
	BLI_ghashIterator_free(iter);

	return list;
}

static PyObject *pyop_getrna(PyObject *UNUSED(self), PyObject *value)
{
	wmOperatorType *ot;
	PointerRNA ptr;
	const char *opname = _PyUnicode_AsString(value);
	BPy_StructRNA *pyrna = NULL;
	
	if (opname == NULL) {
		PyErr_SetString(PyExc_TypeError, "_bpy.ops.get_rna() expects a string argument");
		return NULL;
	}
	ot = WM_operatortype_find(opname, TRUE);
	if (ot  == NULL) {
		PyErr_Format(PyExc_KeyError, "_bpy.ops.get_rna(\"%s\") not found", opname);
		return NULL;
	}
	
	/* type */
	//RNA_pointer_create(NULL, &RNA_Struct, ot->srna, &ptr);

	/* XXX - should call WM_operator_properties_free */
	WM_operator_properties_create_ptr(&ptr, ot);
	WM_operator_properties_sanitize(&ptr, 0);

	
	pyrna = (BPy_StructRNA *)pyrna_struct_CreatePyObject(&ptr);
#ifdef PYRNA_FREE_SUPPORT
	pyrna->freeptr = TRUE;
#endif
	return (PyObject *)pyrna;
}

static PyObject *pyop_getinstance(PyObject *UNUSED(self), PyObject *value)
{
	wmOperatorType *ot;
	wmOperator *op;
	PointerRNA ptr;
	const char *opname = _PyUnicode_AsString(value);
	BPy_StructRNA *pyrna = NULL;

	if (opname == NULL) {
		PyErr_SetString(PyExc_TypeError, "_bpy.ops.get_instance() expects a string argument");
		return NULL;
	}
	ot = WM_operatortype_find(opname, TRUE);
	if (ot == NULL) {
		PyErr_Format(PyExc_KeyError, "_bpy.ops.get_instance(\"%s\") not found", opname);
		return NULL;
	}

#ifdef PYRNA_FREE_SUPPORT
	op = MEM_callocN(sizeof(wmOperator), __func__);
#else
	op = PyMem_MALLOC(sizeof(wmOperator));
	memset(op, 0, sizeof(wmOperator));
#endif
	BLI_strncpy(op->idname, op->idname, sizeof(op->idname)); /* incase its needed */
	op->type = ot;

	RNA_pointer_create(NULL, &RNA_Operator, op, &ptr);

	pyrna = (BPy_StructRNA *)pyrna_struct_CreatePyObject(&ptr);
#ifdef PYRNA_FREE_SUPPORT
	pyrna->freeptr = TRUE;
#endif
	op->ptr = &pyrna->ptr;

	return (PyObject *)pyrna;
}

static struct PyMethodDef bpy_ops_methods[] = {
	{"poll", (PyCFunction) pyop_poll, METH_VARARGS, NULL},
	{"call", (PyCFunction) pyop_call, METH_VARARGS, NULL},
	{"as_string", (PyCFunction) pyop_as_string, METH_VARARGS, NULL},
	{"dir", (PyCFunction) pyop_dir, METH_NOARGS, NULL},
	{"get_rna", (PyCFunction) pyop_getrna, METH_O, NULL},           /* only for introspection, leaks memory */
	{"get_instance", (PyCFunction) pyop_getinstance, METH_O, NULL}, /* only for introspection, leaks memory */
	{"macro_define", (PyCFunction) PYOP_wrap_macro_define, METH_VARARGS, NULL},
	{NULL, NULL, 0, NULL}
};

static struct PyModuleDef bpy_ops_module = {
	PyModuleDef_HEAD_INIT,
	"_bpy.ops",
	NULL,
	-1,/* multiple "initialization" just copies the module dict. */
	bpy_ops_methods,
	NULL, NULL, NULL, NULL
};

PyObject *BPY_operator_module(void)
{
	PyObject *submodule;

	submodule = PyModule_Create(&bpy_ops_module);

	return submodule;
}
