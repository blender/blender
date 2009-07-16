
/**
 * $Id: bpy_operator.c 21554 2009-07-13 08:33:51Z campbellbarton $
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
#include "bpy_operator_wrap.h"
#include "bpy_rna.h" /* for setting arg props only - pyrna_py_to_prop() */
#include "bpy_compat.h"
#include "bpy_util.h"

//#include "blendef.h"
#include "BLI_dynstr.h"

#include "WM_api.h"
#include "WM_types.h"

#include "MEM_guardedalloc.h"
//#include "BKE_idprop.h"
#include "BKE_report.h"

extern ListBase global_ops; /* evil, temp use */

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
	bContext *C = BPy_GetContext();
	
	char *opname = _PyUnicode_AsString(self);

	if (PyTuple_Size(args)) {
		PyErr_SetString( PyExc_AttributeError, "All operator args must be keywords");
		return NULL;
	}

	ot= WM_operatortype_find(opname, 1);
	if (ot == NULL) {
		PyErr_Format( PyExc_SystemError, "Operator \"%s\"could not be found", opname);
		return NULL;
	}
	
	if(ot->poll && (ot->poll(C) == 0)) {
		PyErr_SetString( PyExc_SystemError, "Operator poll() function failed, context is incorrect");
		return NULL;
	}
	
	WM_operator_properties_create(&ptr, opname);
	
	error_val= pyrna_pydict_to_props(&ptr, kw, "Converting py args to operator properties: ");
	
	if (error_val==0) {
		ReportList reports;

		BKE_reports_init(&reports, RPT_STORE);

		WM_operator_call_py(C, ot, &ptr, &reports);

		if(BPy_reports_to_error(&reports))
			error_val = -1;

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
	
	/* First look for the operator, then our own methods if that fails.
	 * when methods are searched first, PyObject_GenericGetAttr will raise an error
	 * each time we want to call an operator, we could clear the error but I prefer
	 * not to since calling operators is a lot more common then adding and removing. - Campbell */
	
	if ((ot= WM_operatortype_find(name, 1))) {
		ret = PyCFunction_New( pyop_base_call_meth, pyname); /* set the name string as self, PyCFunction_New incref's self */
	}
	else if ((ret = PyObject_GenericGetAttr((PyObject *)self, pyname))) {
		/* do nothing, this accounts for methoddef's add and remove
		 * An exception is raised when PyObject_GenericGetAttr fails
		 * but its ok because its overwritten below */
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
	
	if ((ot= WM_operatortype_find(name, 1))) {
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

PyObject *BPY_operator_module( void )
{
	PyObject *ob;

	pyop_base_Type.tp_name = "OperatorBase";
	pyop_base_Type.tp_basicsize = sizeof( BPy_OperatorBase );
	pyop_base_Type.tp_getattro = ( getattrofunc )pyop_base_getattro;
	pyop_base_Type.tp_flags = Py_TPFLAGS_DEFAULT;
	pyop_base_Type.tp_methods = pyop_base_methods;
	
	if( PyType_Ready( &pyop_base_Type ) < 0 )
		return NULL;

	//submodule = Py_InitModule3( "operator", M_rna_methods, "rna module" );
	ob = PyObject_NEW( BPy_OperatorBase, &pyop_base_Type );
	Py_INCREF(ob);

	return ob;
}

