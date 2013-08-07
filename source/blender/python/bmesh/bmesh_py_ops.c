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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/bmesh/bmesh_py_ops.c
 *  \ingroup pybmesh
 *
 * This file defines the 'bmesh.ops' module.
 * Operators from 'opdefines' are wrapped.
 */

#include <Python.h>

#include "BLI_utildefines.h"
#include "BLI_dynstr.h"

#include "MEM_guardedalloc.h"

#include "../generic/py_capi_utils.h"

#include "bmesh.h"

#include "bmesh_py_ops_call.h"
#include "bmesh_py_ops.h"  /* own include */

#include "bmesh_py_types.h"

#include "bmesh_py_utils.h"

/* bmesh operator 'bmesh.ops.*' callable types
 * ******************************************* */
static PyTypeObject bmesh_op_Type;

static PyObject *bpy_bmesh_op_CreatePyObject(const char *opname)
{
	BPy_BMeshOpFunc *self = PyObject_New(BPy_BMeshOpFunc, &bmesh_op_Type);

	self->opname = opname;

	return (PyObject *)self;
}

static PyObject *bpy_bmesh_op_repr(BPy_BMeshOpFunc *self)
{
	return PyUnicode_FromFormat("<%.200s bmesh.ops.%.200s()>",
	                            Py_TYPE(self)->tp_name,
	                            self->opname);
}


/* methods
 * ======= */


/* __doc__
 * ------- */

static char *bmp_slots_as_args(const BMOSlotType slot_types[BMO_OP_MAX_SLOTS], const bool is_out)
{
	DynStr *dyn_str = BLI_dynstr_new();
	char *ret;

	int i = 0;

	while (*slot_types[i].name) {
		/* cut off '.out' by using a string size arg */
		const int name_len = is_out ?
		        (strchr(slot_types[i].name, '.') - slot_types[i].name) :
		        sizeof(slot_types[i].name);
		const char *value = "<Unknown>";
		switch (slot_types[i].type) {
			case BMO_OP_SLOT_BOOL:          value = "False"; break;
			case BMO_OP_SLOT_INT:           value = "0"; break;
			case BMO_OP_SLOT_FLT:           value = "0.0"; break;
			case BMO_OP_SLOT_PTR:           value = "None"; break;
			case BMO_OP_SLOT_MAT:           value = "Matrix()"; break;
			case BMO_OP_SLOT_VEC:           value = "Vector()"; break;
			case BMO_OP_SLOT_ELEMENT_BUF:   value =
			     (slot_types[i].subtype.elem & BMO_OP_SLOT_SUBTYPE_ELEM_IS_SINGLE) ? "None" : "[]"; break;
			case BMO_OP_SLOT_MAPPING:       value = "{}"; break;
		}
		BLI_dynstr_appendf(dyn_str, i ? ", %.*s=%s" : "%.*s=%s", name_len, slot_types[i].name, value);
		i++;
	}

	ret = BLI_dynstr_get_cstring(dyn_str);
	BLI_dynstr_free(dyn_str);
	return ret;
}

static PyObject *bpy_bmesh_op_doc_get(BPy_BMeshOpFunc *self, void *UNUSED(closure))
{
	PyObject *ret;
	char *slot_in;
	char *slot_out;
	int i;

	i = BMO_opcode_from_opname(self->opname);

	slot_in  = bmp_slots_as_args(bmo_opdefines[i]->slot_types_in, false);
	slot_out = bmp_slots_as_args(bmo_opdefines[i]->slot_types_out, true);

	ret = PyUnicode_FromFormat("%.200s bmesh.ops.%.200s(bmesh, %s)\n  -> dict(%s)",
	                           Py_TYPE(self)->tp_name,
	                           self->opname, slot_in, slot_out);

	MEM_freeN(slot_in);
	MEM_freeN(slot_out);

	return ret;
}

static PyGetSetDef bpy_bmesh_op_getseters[] = {
	{(char *)"__doc__", (getter)bpy_bmesh_op_doc_get, (setter)NULL, NULL, NULL},
	{NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};


/* Types
 * ===== */

static PyTypeObject bmesh_op_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"BMeshOpFunc",              /* tp_name */
	sizeof(BPy_BMeshOpFunc),    /* tp_basicsize */
	0,                          /* tp_itemsize */
	/* methods */
	NULL,                       /* tp_dealloc */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* tp_compare */ /* DEPRECATED in python 3.0! */
	(reprfunc) bpy_bmesh_op_repr, /* tp_repr */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	(ternaryfunc)BPy_BMO_call,  /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */

	/* will only use these if this is a subtype of a py class */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

	/*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
	/*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

	/***  Assigned meaning in release 2.1 ***/
	/*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

	/***  weak reference enabler ***/
	0,
	/*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

	/*** Attribute descriptor and subclassing stuff ***/
	NULL,                       /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	bpy_bmesh_op_getseters,     /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};


/* bmesh fake module 'bmesh.ops'
 * ***************************** */

static PyObject *bpy_bmesh_ops_fakemod_getattro(PyObject *UNUSED(self), PyObject *pyname)
{
	const char *opname = _PyUnicode_AsString(pyname);

	if (BMO_opcode_from_opname(opname) != -1) {
		return bpy_bmesh_op_CreatePyObject(opname);
	}
	else {
		PyErr_Format(PyExc_AttributeError,
		             "BMeshOpsModule: operator \"%.200s\" doesn't exist",
		             opname);
		return NULL;
	}
}

static PyObject *bpy_bmesh_ops_fakemod_dir(PyObject *UNUSED(self))
{
	const unsigned int tot = bmo_opdefines_total;
	unsigned int i;
	PyObject *ret;

	ret = PyList_New(bmo_opdefines_total);

	for (i = 0; i < tot; i++) {
		PyList_SET_ITEM(ret, i, PyUnicode_FromString(bmo_opdefines[i]->opname));
	}

	return ret;
}

static struct PyMethodDef bpy_bmesh_ops_fakemod_methods[] = {
	{"__dir__", (PyCFunction)bpy_bmesh_ops_fakemod_dir, METH_NOARGS, NULL},
	{NULL, NULL, 0, NULL}
};

static PyTypeObject bmesh_ops_fakemod_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"BMeshOpsModule",           /* tp_name */
	0,                          /* tp_basicsize */
	0,                          /* tp_itemsize */
	/* methods */
	NULL,                       /* tp_dealloc */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* tp_compare */ /* DEPRECATED in python 3.0! */
	NULL,                       /* tp_repr */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */

	/* will only use these if this is a subtype of a py class */
	bpy_bmesh_ops_fakemod_getattro,    /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

	/*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
	/*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

	/***  Assigned meaning in release 2.1 ***/
	/*** rich comparisons ***/
	NULL, /* subclassed */		/* richcmpfunc tp_richcompare; */

	/***  weak reference enabler ***/
	0,
	/*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

	/*** Attribute descriptor and subclassing stuff ***/
	bpy_bmesh_ops_fakemod_methods,  /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	NULL,                       /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};

PyObject *BPyInit_bmesh_ops(void)
{
	PyObject *submodule;

	if (PyType_Ready(&bmesh_ops_fakemod_Type) < 0)
		return NULL;

	if (PyType_Ready(&bmesh_op_Type) < 0)
		return NULL;

	submodule = PyObject_New(PyObject, &bmesh_ops_fakemod_Type);

	/* prevent further creation of instances */
	bmesh_ops_fakemod_Type.tp_init = NULL;
	bmesh_ops_fakemod_Type.tp_new = NULL;

	return submodule;
}
