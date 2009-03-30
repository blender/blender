
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

#include "bpy_operator.h"
#include "bpy_opwrapper.h"
#include "bpy_rna.h" /* for setting arg props only - pyrna_py_to_prop() */
#include "bpy_compat.h"

//#include "blendef.h"
#include "BLI_dynstr.h"

#include "WM_api.h"
#include "WM_types.h"

#include "MEM_guardedalloc.h"
//#include "BKE_idprop.h"
#include "BKE_report.h"

extern ListBase global_ops; /* evil, temp use */



/* This function is only used by operators right now
 * Its used for taking keyword args and filling in property values */
int PYOP_props_from_dict(PointerRNA *ptr, PyObject *kw)
{
	int error_val = 0;
	int totkw;
	const char *arg_name= NULL;
	PyObject *item;

	PropertyRNA *prop, *iterprop;
	CollectionPropertyIterator iter;

	iterprop= RNA_struct_iterator_property(ptr);
	RNA_property_collection_begin(ptr, iterprop, &iter);

	totkw = kw ? PyDict_Size(kw):0;

	for(; iter.valid; RNA_property_collection_next(&iter)) {
		prop= iter.ptr.data;

		arg_name= RNA_property_identifier(&iter.ptr, prop);

		if (strcmp(arg_name, "rna_type")==0) continue;

		if (kw==NULL) {
			PyErr_Format( PyExc_AttributeError, "no args, expected \"%s\"", arg_name ? arg_name : "<UNKNOWN>");
			error_val= -1;
			break;
		}

		item= PyDict_GetItemString(kw, arg_name);

		if (item == NULL) {
			PyErr_Format( PyExc_AttributeError, "argument \"%s\" missing", arg_name ? arg_name : "<UNKNOWN>");
			error_val = -1; /* pyrna_py_to_prop sets the error */
			break;
		}

		if (pyrna_py_to_prop(ptr, prop, item)) {
			error_val= -1;
			break;
		}

		totkw--;
	}

	RNA_property_collection_end(&iter);

	if (error_val==0 && totkw > 0) { /* some keywords were given that were not used :/ */
		PyObject *key, *value;
		Py_ssize_t pos = 0;

		while (PyDict_Next(kw, &pos, &key, &value)) {
			arg_name= _PyUnicode_AsString(key);
			if (RNA_struct_find_property(ptr, arg_name) == NULL) break;
			arg_name= NULL;
		}

		PyErr_Format( PyExc_AttributeError, "argument \"%s\" unrecognized", arg_name ? arg_name : "<UNKNOWN>");
		error_val = -1;
	}

	return error_val;
}

static PyObject *pyop_base_dir(PyObject *self);
static PyObject *pyop_base_rna(PyObject *self, PyObject *pyname);
static struct PyMethodDef pyop_base_methods[] = {
	{"__dir__", (PyCFunction)pyop_base_dir, METH_NOARGS, ""},
	{"__rna__", (PyCFunction)pyop_base_rna, METH_O, ""},
	{"add", (PyCFunction)PYOP_wrap_add, METH_O, ""},
	{"remove", (PyCFunction)PYOP_wrap_remove, METH_O, ""},
	{NULL, NULL, 0, NULL}
};

/* 'self' stores the operator string */
static PyObject *pyop_base_call( PyObject * self, PyObject * args,  PyObject * kw)
{
	wmOperatorType *ot;
	int error_val = 0;
	PointerRNA ptr;
	
	// XXX Todo, work out a better solution for passing on context, could make a tuple from self and pack the name and Context into it...
	bContext *C = (bContext *)PyCObject_AsVoidPtr(PyDict_GetItemString(PyEval_GetGlobals(), "__bpy_context__"));
	
	char *opname = _PyUnicode_AsString(self);
	char *report_str= NULL;

	if (PyTuple_Size(args)) {
		PyErr_SetString( PyExc_AttributeError, "All operator args must be keywords");
		return NULL;
	}

	ot= WM_operatortype_find(opname);
	if (ot == NULL) {
		PyErr_Format( PyExc_SystemError, "Operator \"%s\"could not be found", opname);
		return NULL;
	}
	
	if(ot->poll && (ot->poll(C) == 0)) {
		PyErr_SetString( PyExc_SystemError, "Operator poll() function failed, context is incorrect");
		return NULL;
	}
	
	WM_operator_properties_create(&ptr, opname);
	
	error_val= PYOP_props_from_dict(&ptr, kw);
	
	if (error_val==0) {
		ReportList reports;

		BKE_reports_init(&reports, RPT_STORE);

		WM_operator_call_py(C, ot, &ptr, &reports);

		report_str= BKE_reports_string(&reports, RPT_ERROR);

		if (report_str) {
			PyErr_SetString(PyExc_SystemError, report_str);
			MEM_freeN(report_str);
			error_val = -1;
		}

		if (reports.list.first)
			BKE_reports_clear(&reports);
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

static PyMethodDef pyop_base_call_meth[] = {
	{"__op_call__", (PyCFunction)pyop_base_call, METH_VARARGS|METH_KEYWORDS, "generic operator calling function"}
};


//---------------getattr--------------------------------------------
static PyObject *pyop_base_getattro( BPy_OperatorBase * self, PyObject *pyname )
{
	char *name = _PyUnicode_AsString(pyname);
	PyObject *ret;
	wmOperatorType *ot;
	
	if ((ot= WM_operatortype_find(name))) {
		ret = PyCFunction_New( pyop_base_call_meth, pyname); /* set the name string as self, PyCFunction_New incref's self */
	}
	else if ((ret = PyObject_GenericGetAttr((PyObject *)self, pyname))) {
		/* do nothing, this accounts for methoddef's add and remove */
	}
	else {
		PyErr_Format( PyExc_AttributeError, "Operator \"%s\" not found", name);
		ret= NULL;
	}

	return ret;
}

static PyObject *pyop_base_dir(PyObject *self)
{
	PyObject *list = PyList_New(0), *name;
	wmOperatorType *ot;
	PyMethodDef *meth;
	
	for(ot= WM_operatortype_first(); ot; ot= ot->next) {
		name = PyUnicode_FromString(ot->idname);
		PyList_Append(list, name);
		Py_DECREF(name);
	}

	for(meth=pyop_base_methods; meth->ml_name; meth++) {
		name = PyUnicode_FromString(meth->ml_name);
		PyList_Append(list, name);
		Py_DECREF(name);
	}
	
	return list;
}

static PyObject *pyop_base_rna(PyObject *self, PyObject *pyname)
{
	char *name = _PyUnicode_AsString(pyname);
	wmOperatorType *ot;
	
	if ((ot= WM_operatortype_find(name))) {
		BPy_StructRNA *pyrna;
		PointerRNA ptr;
		
		/* XXX POINTER - if this 'ot' is python generated, it could be free'd */
		RNA_pointer_create(NULL, ot->srna, NULL, &ptr);
		
		pyrna= (BPy_StructRNA *)pyrna_struct_CreatePyObject(&ptr); /* were not really using &ptr, overwite next */
		//pyrna->freeptr= 1;
		return (PyObject *)pyrna;
	}
	else {
		PyErr_Format(PyExc_AttributeError, "Operator \"%s\" not found", name);
		return NULL;
	}
}

PyTypeObject pyop_base_Type = {NULL};

PyObject *BPY_operator_module( bContext *C )
{
	pyop_base_Type.tp_name = "OperatorBase";
	pyop_base_Type.tp_basicsize = sizeof( BPy_OperatorBase );
	pyop_base_Type.tp_getattro = ( getattrofunc )pyop_base_getattro;
	pyop_base_Type.tp_flags = Py_TPFLAGS_DEFAULT;
	pyop_base_Type.tp_methods = pyop_base_methods;
	
	if( PyType_Ready( &pyop_base_Type ) < 0 )
		return NULL;

	//submodule = Py_InitModule3( "operator", M_rna_methods, "rna module" );
	return (PyObject *)PyObject_NEW( BPy_OperatorBase, &pyop_base_Type );
}

