
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

/* Note, this module is not to be used directly by the user.
 * its accessed from blender with bpy.__ops__
 * */

#include "bpy_operator.h"
#include "bpy_operator_wrap.h"
#include "bpy_rna.h" /* for setting arg props only - pyrna_py_to_prop() */
#include "bpy_util.h"

#include "WM_api.h"
#include "WM_types.h"

#include "MEM_guardedalloc.h"
#include "BKE_report.h"
#include "BKE_utildefines.h"


static PyObject *pyop_call( PyObject * self, PyObject * args)
{
	wmOperatorType *ot;
	int error_val = 0;
	PointerRNA ptr;
	
	char		*opname;
	PyObject	*kw= NULL; /* optional args */

	/* note that context is an int, python does the conversion in this case */
	int context= WM_OP_EXEC_DEFAULT;

	// XXX Todo, work out a better solution for passing on context, could make a tuple from self and pack the name and Context into it...
	bContext *C = BPy_GetContext();
	
	if (!PyArg_ParseTuple(args, "s|O!i:bpy.__ops__.call", &opname, &PyDict_Type, &kw, &context))
		return NULL;

	ot= WM_operatortype_find(opname, TRUE);

	if (ot == NULL) {
		PyErr_Format( PyExc_SystemError, "bpy.__ops__.call: operator \"%s\"could not be found", opname);
		return NULL;
	}
	
	if(WM_operator_poll((bContext*)C, ot) == FALSE) {
		PyErr_SetString( PyExc_SystemError, "bpy.__ops__.call: operator poll() function failed, context is incorrect");
		return NULL;
	}

	/* WM_operator_properties_create(&ptr, opname); */
	/* Save another lookup */
	RNA_pointer_create(NULL, ot->srna, NULL, &ptr);
	
	if(kw && PyDict_Size(kw))
		error_val= pyrna_pydict_to_props(&ptr, kw, 0, "Converting py args to operator properties: ");

	
	if (error_val==0) {
		ReportList *reports;

		reports= MEM_mallocN(sizeof(ReportList), "wmOperatorReportList");
		BKE_reports_init(reports, RPT_STORE);

		WM_operator_call_py(C, ot, context, &ptr, reports);

		if(BPy_reports_to_error(reports))
			error_val = -1;

		/* operator output is nice to have in the terminal/console too */
		if(reports->list.first) {
			char *report_str= BKE_reports_string(reports, 0); /* all reports */

			if(report_str) {
				PySys_WriteStdout(report_str);
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
			PyErr_Format(PyExc_AttributeError, "Operator \"%s\" does not take any args", opname);
			return NULL;
		}

		WM_operator_name_call(C, opname, WM_OP_EXEC_DEFAULT, NULL);
	}
#endif

	if (error_val==-1) {
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject *pyop_as_string( PyObject * self, PyObject * args)
{
	wmOperatorType *ot;
	PointerRNA ptr;

	char		*opname;
	PyObject	*kw= NULL; /* optional args */
	int all_args = 1;
	int error_val= 0;

	char *buf;
	PyObject *pybuf;

	bContext *C = BPy_GetContext();

	if (!PyArg_ParseTuple(args, "s|O!i:bpy.__ops__.as_string", &opname, &PyDict_Type, &kw, &all_args))
		return NULL;

	ot= WM_operatortype_find(opname, TRUE);

	if (ot == NULL) {
		PyErr_Format( PyExc_SystemError, "bpy.__ops__.as_string: operator \"%s\"could not be found", opname);
		return NULL;
	}

	/* WM_operator_properties_create(&ptr, opname); */
	/* Save another lookup */
	RNA_pointer_create(NULL, ot->srna, NULL, &ptr);

	if(kw && PyDict_Size(kw))
		error_val= pyrna_pydict_to_props(&ptr, kw, 0, "Converting py args to operator properties: ");

	if (error_val==0)
		buf= WM_operator_pystring(C, ot, &ptr, all_args);

	WM_operator_properties_free(&ptr);

	if (error_val==-1) {
		return NULL;
	}

	if(buf) {
		pybuf= PyUnicode_FromString(buf);
		MEM_freeN(buf);
	}
	else {
		pybuf= PyUnicode_FromString("");
	}

	return pybuf;
}

static PyObject *pyop_dir(PyObject *self)
{
	PyObject *list = PyList_New(0), *name;
	wmOperatorType *ot;
	
	for(ot= WM_operatortype_first(); ot; ot= ot->next) {
		name = PyUnicode_FromString(ot->idname);
		PyList_Append(list, name);
		Py_DECREF(name);
	}
	
	return list;
}

static PyObject *pyop_getrna(PyObject *self, PyObject *value)
{
	wmOperatorType *ot;
	PointerRNA ptr;
	char *opname= _PyUnicode_AsString(value);
	BPy_StructRNA *pyrna= NULL;
	
	if(opname==NULL) {
		PyErr_SetString(PyExc_TypeError, "bpy.__ops__.get_rna() expects a string argument");
		return NULL;
	}
	ot= WM_operatortype_find(opname, TRUE);
	if(ot==NULL) {
		PyErr_Format(PyExc_KeyError, "bpy.__ops__.get_rna(\"%s\") not found", opname);
		return NULL;
	}
	
	/* type */
	//RNA_pointer_create(NULL, &RNA_Struct, ot->srna, &ptr);

	/* XXX - should call WM_operator_properties_free */
	WM_operator_properties_create(&ptr, ot->idname);
	pyrna= (BPy_StructRNA *)pyrna_struct_CreatePyObject(&ptr);
	pyrna->freeptr= TRUE;
	return (PyObject *)pyrna;
}

PyObject *BPY_operator_module( void )
{
	static PyMethodDef pyop_call_meth =		{"call", (PyCFunction) pyop_call, METH_VARARGS, NULL};
	static PyMethodDef pyop_as_string_meth ={"as_string", (PyCFunction) pyop_as_string, METH_VARARGS, NULL};
	static PyMethodDef pyop_dir_meth =		{"dir", (PyCFunction) pyop_dir, METH_NOARGS, NULL};
	static PyMethodDef pyop_getrna_meth =	{"get_rna", (PyCFunction) pyop_getrna, METH_O, NULL};
	static PyMethodDef pyop_add_meth =		{"add", (PyCFunction) PYOP_wrap_add, METH_O, NULL};
	static PyMethodDef pyop_remove_meth =	{"remove", (PyCFunction) PYOP_wrap_remove, METH_O, NULL};

	PyObject *submodule = PyModule_New("bpy.__ops__");
	PyDict_SetItemString(PySys_GetObject("modules"), "bpy.__ops__", submodule);

	PyModule_AddObject( submodule, "call",	PyCFunction_New(&pyop_call_meth,	NULL) );
	PyModule_AddObject( submodule, "as_string",PyCFunction_New(&pyop_as_string_meth,NULL) );
	PyModule_AddObject( submodule, "dir",		PyCFunction_New(&pyop_dir_meth,		NULL) );
	PyModule_AddObject( submodule, "get_rna",	PyCFunction_New(&pyop_getrna_meth,	NULL) );
	PyModule_AddObject( submodule, "add",		PyCFunction_New(&pyop_add_meth,		NULL) );
	PyModule_AddObject( submodule, "remove",	PyCFunction_New(&pyop_remove_meth,	NULL) );

	return submodule;
}
