
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


#include "bpy_opwrapper.h"
#include "BLI_listbase.h"
#include "BKE_context.h"
#include "DNA_windowmanager_types.h"
#include "MEM_guardedalloc.h"
#include "WM_api.h"
#include "WM_types.h"
#include "ED_screen.h"

#include "RNA_define.h"

#include "bpy_rna.h"
#include "bpy_compat.h"

typedef struct PyOperatorType {
	void *next, *prev;
	char idname[OP_MAX_TYPENAME];
	char name[OP_MAX_TYPENAME];
	PyObject *py_invoke;
	PyObject *py_exec;
} PyOperatorType;

static void pyop_kwargs_from_operator(PyObject *dict, wmOperator *op)
{
	PyObject *item;
	PropertyRNA *prop, *iterprop;
	CollectionPropertyIterator iter;
	const char *arg_name;

	iterprop= RNA_struct_iterator_property(op->ptr);
	RNA_property_collection_begin(op->ptr, iterprop, &iter);

	for(; iter.valid; RNA_property_collection_next(&iter)) {
		prop= iter.ptr.data;

		arg_name= RNA_property_identifier(&iter.ptr, prop);

		if (strcmp(arg_name, "rna_type")==0) continue;

		item = pyrna_prop_to_py(&iter.ptr, prop);
		PyDict_SetItemString(dict, arg_name, item);
		Py_DECREF(item);
	}

	RNA_property_collection_end(&iter);

}

static int PYTHON_OT_exec(bContext *C, wmOperator *op)
{
	PyOperatorType *pyot = op->type->pyop_data;
	PyObject *ret, *args= PyTuple_New(0), *kw= PyDict_New();

	pyop_kwargs_from_operator(kw, op);

	ret = PyObject_Call(pyot->py_exec, args, kw);

	Py_DECREF(args);
	Py_DECREF(kw);

	return OPERATOR_FINISHED;
}

static int PYTHON_OT_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	PyOperatorType *pyot = op->type->pyop_data;
	PyObject *ret, *args= PyTuple_New(0), *kw= PyDict_New();

	pyop_kwargs_from_operator(kw, op);

	ret = PyObject_Call(pyot->py_invoke, args, kw);

	Py_DECREF(args);
	Py_DECREF(kw);

	return OPERATOR_FINISHED;
}

void PYTHON_OT_wrapper(wmOperatorType *ot, void *userdata)
{
	PyOperatorType *pyot = (PyOperatorType *)userdata;

	/* identifiers */
	ot->name= pyot->name;
	ot->idname= pyot->idname;

	/* api callbacks */
	if (pyot->py_invoke != Py_None)
		ot->invoke= PYTHON_OT_invoke;
	
	ot->exec= PYTHON_OT_exec;

	ot->poll= ED_operator_screenactive; /* how should this work?? */

	ot->pyop_data= userdata;
	
	/* inspect function keyword args to get properties */
	{
		PropertyRNA *prop;

		PyObject *var_names= PyObject_GetAttrString(PyFunction_GET_CODE(pyot->py_exec), "co_varnames");
		PyObject *var_vals = PyFunction_GET_DEFAULTS(pyot->py_exec);
		PyObject *py_val, *py_name;
		int i;
		char *name;

		if (PyTuple_Size(var_names) != PyTuple_Size(var_vals)) {
			printf("All args must be keywords");
		}

		for(i=0; i<PyTuple_Size(var_names); i++) {
			py_name = PyTuple_GetItem(var_names, i);
			name = _PyUnicode_AsString(py_name);
			py_val = PyTuple_GetItem(var_vals, i);

			if (PyBool_Check(py_val)) {
				prop = RNA_def_property(ot->srna, name, PROP_BOOLEAN, PROP_NONE);
				RNA_def_property_boolean_default(prop, PyObject_IsTrue(py_val));
			}
			else if (PyLong_Check(py_val)) {
				prop = RNA_def_property(ot->srna, name, PROP_INT, PROP_NONE);
				RNA_def_property_int_default(prop, (int)PyLong_AsSsize_t(py_val));
			}
			else if (PyFloat_Check(py_val)) {
				prop = RNA_def_property(ot->srna, name, PROP_FLOAT, PROP_NONE);
				RNA_def_property_float_default(prop, (float)PyFloat_AsDouble(py_val));
			}
			else if (PyUnicode_Check(py_val)) {
				/* WARNING - holding a reference to the string from py_val is
				 * not ideal since we rely on python keeping it,
				 * however we're also keeping a reference to this function
				 * so it should be OK!. just be careful with changes */
				prop = RNA_def_property(ot->srna, name, PROP_STRING, PROP_NONE);
				RNA_def_property_string_default(prop, _PyUnicode_AsString(py_val));
			}
			else {
				printf("error, python function arg \"%s\" was not a bool, int, float or string type\n", name);
			}
		}
	}

}

/* pyOperators - Operators defined IN Python */
static PyObject *pyop_add(PyObject *self, PyObject *args)
{
	PyOperatorType *pyot;

	char *idname= NULL;
	char *name= NULL;
	PyObject *invoke= NULL;
	PyObject *exec= NULL;
	
	if (!PyArg_ParseTuple(args, "ssOO", &idname, &name, &invoke, &exec))
		PyErr_SetString( PyExc_AttributeError, "expected 2 strings and 2 function objects");
		return NULL;

	if (WM_operatortype_find(idname)) {
		PyErr_Format( PyExc_AttributeError, "First argument \"%s\" operator alredy exists with this name", idname);
		return NULL;
	}

	if (((PyFunction_Check(invoke) || invoke==Py_None) && PyFunction_Check(exec)) == 0) {
		PyErr_SetString( PyExc_AttributeError, "the 2nd arg must be a function or None, the secons must be a function");
		return NULL;
	}
	
	pyot= MEM_callocN(sizeof(PyOperatorType), "PyOperatorType");

	strcpy(pyot->idname, idname);
	strcpy(pyot->name, name);
	pyot->py_invoke= invoke;
	pyot->py_exec= exec;
	Py_INCREF(invoke);
	Py_INCREF(exec);

	WM_operatortype_append_ptr(PYTHON_OT_wrapper, pyot);

	Py_RETURN_NONE;
}

static PyObject *pyop_remove(PyObject *self, PyObject *args)
{
	char *idname= NULL;
	wmOperatorType *ot;
	PyOperatorType *pyot;

	if (!PyArg_ParseTuple(args, "s", &idname))
		return NULL;

	if (!(ot= WM_operatortype_find(idname))) {
		PyErr_Format( PyExc_AttributeError, "Operator \"%s\" alredy exists", idname);
		return NULL;
	}
	
	if (!(pyot= (PyOperatorType *)ot->pyop_data)) {
		PyErr_Format( PyExc_AttributeError, "Operator \"%s\" was not created by python", idname);
		return NULL;
	}
	
	Py_XDECREF(pyot->py_invoke);
	Py_XDECREF(pyot->py_exec);
	MEM_freeN(pyot);

	WM_operatortype_remove(idname);

	Py_RETURN_NONE;
}

static PyMethodDef pyop_add_methdef[] = {
	{"add", pyop_add, METH_VARARGS, ""}
};

static PyMethodDef pyop_remove_methdef[] = {
	{"remove", pyop_remove, METH_VARARGS, ""}
};

PyObject *PYOP_wrap_add_func( void )
{
	return PyCFunction_New( pyop_add_methdef, NULL );
}
PyObject *PYOP_wrap_remove_func( void )
{
	return PyCFunction_New( pyop_remove_methdef, NULL );
}

