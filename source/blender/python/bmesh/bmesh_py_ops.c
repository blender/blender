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

#include "MEM_guardedalloc.h"

#include "../generic/py_capi_utils.h"

#include "../mathutils/mathutils.h"

#include "bmesh.h"

#include "bmesh_py_types.h"

#include "bmesh_py_utils.h" /* own include */

static int bpy_bm_op_as_py_error(BMesh *bm)
{
	if (BMO_error_occurred(bm)) {
		const char *errmsg;
		if (BMO_error_get(bm, &errmsg, NULL)) {
			PyErr_Format(PyExc_RuntimeError,
			             "bmesh operator: %.200s",
			             errmsg);
			return -1;
		}
	}
	return 0;
}

/* bmesh operator 'bmesh.ops.*' callable types
 * ******************************************* */
PyTypeObject bmesh_op_Type;

typedef struct {
	PyObject_HEAD /* required python macro   */
	const char *opname;
} BPy_BMeshOpFunc;

PyObject *bpy_bmesh_op_CreatePyObject(const char *opname)
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


static PyObject *pyrna_op_call(BPy_BMeshOpFunc *self, PyObject *args, PyObject *kw)
{
	BPy_BMesh *py_bm;
	BMesh *bm;

	BMOperator bmop;

	if ((PyTuple_GET_SIZE(args) == 1) &&
	    (py_bm = (BPy_BMesh *)PyTuple_GET_ITEM(args, 0)) &&
	    (BPy_BMesh_Check(py_bm))
		)
	{
		BPY_BM_CHECK_OBJ(py_bm);
		bm = py_bm->bm;
	}
	else {
		PyErr_SetString(PyExc_TypeError,
		                "calling a bmesh operator expects a single BMesh (non keyword) "
		                "as the first argument");
		return NULL;
	}

	/* TODO - error check this!, though we do the error check on attribute access */
	BMO_op_init(bm, &bmop, self->opname);

	if (kw && PyDict_Size(kw) > 0) {
		/* setup properties, see bpy_rna.c: pyrna_py_to_prop()
		 * which shares this logic for parsing properties */

		PyObject *key, *value;
		Py_ssize_t pos = 0;
		while (PyDict_Next(kw, &pos, &key, &value)) {
			const char *slot_name = _PyUnicode_AsString(key);
			BMOpSlot *slot = BMO_slot_get(&bmop, slot_name);

			if (slot == NULL) {
				PyErr_Format(PyExc_TypeError,
				             "%.200s: keyword \"%.200s\" is invalid for this operator",
				             self->opname, slot_name);
				return NULL;
			}

			/* now assign the value */
			switch (slot->slot_type) {
				case BMO_OP_SLOT_BOOL:
				{
					int param;

					param = PyLong_AsLong(value);

					if (param < 0) {
						PyErr_Format(PyExc_TypeError,
						             "%.200s: keyword \"%.200s\" expected True/False or 0/1, not %.200s",
						             self->opname, slot_name, Py_TYPE(value)->tp_name);
						return NULL;
					}
					else {
						slot->data.i = param;
					}

					break;
				}
				case BMO_OP_SLOT_INT:
				{
					int overflow;
					long param = PyLong_AsLongAndOverflow(value, &overflow);
					if (overflow || (param > INT_MAX) || (param < INT_MIN)) {
						PyErr_Format(PyExc_ValueError,
						             "%.200s: keyword \"%.200s\" value not in 'int' range "
						             "(" STRINGIFY(INT_MIN) ", " STRINGIFY(INT_MAX) ")",
						             self->opname, slot_name, Py_TYPE(value)->tp_name);
						return NULL;
					}
					else if (param == -1 && PyErr_Occurred()) {
						PyErr_Format(PyExc_TypeError,
						             "%.200s: keyword \"%.200s\" expected an int, not %.200s",
						             self->opname, slot_name, Py_TYPE(value)->tp_name);
						return NULL;
					}
					else {
						slot->data.i = (int)param;
					}
					break;
				}
				case BMO_OP_SLOT_FLT:
				{
					float param = PyFloat_AsDouble(value);
					if (param == -1 && PyErr_Occurred()) {
						PyErr_Format(PyExc_TypeError,
						             "%.200s: keyword \"%.200s\" expected a float, not %.200s",
						             self->opname, slot_name, Py_TYPE(value)->tp_name);
						return NULL;
					}
					else {
						slot->data.f = param;
					}
					break;
				}
				case BMO_OP_SLOT_MAT:
				{
					/* XXX - BMesh operator design is crappy here, operator slot should define matrix size,
					 * not the caller! */
					unsigned short size;
					if (!MatrixObject_Check(value)) {
						PyErr_Format(PyExc_TypeError,
						             "%.200s: keyword \"%.200s\" expected a Matrix, not %.200s",
						             self->opname, slot_name, Py_TYPE(value)->tp_name);
						return NULL;
					}
					else if (BaseMath_ReadCallback((MatrixObject *)value) == -1) {
						return NULL;
					}
					else if (((size = ((MatrixObject *)value)->num_col) != ((MatrixObject *)value)->num_row) ||
					         (ELEM(size, 3, 4) == FALSE))
					{
						PyErr_Format(PyExc_TypeError,
						             "%.200s: keyword \"%.200s\" expected a 3x3 or 4x4 matrix Matrix",
						             self->opname, slot_name);
						return NULL;
					}

					BMO_slot_mat_set(&bmop, slot_name, ((MatrixObject *)value)->matrix, size);
					break;
				}
				case BMO_OP_SLOT_VEC:
				{
					/* passing slot name here is a bit non-descriptive */
					if (mathutils_array_parse(slot->data.vec, 3, 3, value, slot_name) == -1) {
						return NULL;
					}
					break;
				}
				case BMO_OP_SLOT_ELEMENT_BUF:
				{
					/* there are many ways we could interpret arguments, for now...
					 * - verts/edges/faces from the mesh direct,
					 *   this way the operator takes every item.
					 * - `TODO` a plain python sequence (list) of elements.
					 * - `TODO`  an iterator. eg.
					 *   face.verts
					 * - `TODO`  (type, flag) pair, eg.
					 *   ('VERT', {'TAG'})
					 */

#define BPY_BM_GENERIC_MESH_TEST(type_string)  \
	if (((BPy_BMGeneric *)value)->bm != bm) {                                             \
	    PyErr_Format(PyExc_NotImplementedError,                                           \
	                 "%.200s: keyword \"%.200s\" " type_string " are from another bmesh", \
	                 self->opname, slot_name, slot->slot_type);                           \
	    return NULL;                                                                      \
	} (void)0

					if (BPy_BMVertSeq_Check(value)) {
						BPY_BM_GENERIC_MESH_TEST("verts");
						BMO_slot_buffer_from_all(bm, &bmop, slot_name, BM_VERT);
					}
					else if (BPy_BMEdgeSeq_Check(value)) {
						BPY_BM_GENERIC_MESH_TEST("edges");
						BMO_slot_buffer_from_all(bm, &bmop, slot_name, BM_EDGE);
					}
					else if (BPy_BMFaceSeq_Check(value)) {
						BPY_BM_GENERIC_MESH_TEST("faces");
						BMO_slot_buffer_from_all(bm, &bmop, slot_name, BM_FACE);
					}
					else if (BPy_BMElemSeq_Check(value)) {
						BMIter iter;
						BMHeader *ele;
						int tot;
						unsigned int i;

						BPY_BM_GENERIC_MESH_TEST("elements");

						/* this will loop over all elements which is a shame but
						 * we need to know this before alloc */
						/* calls bpy_bmelemseq_length() */
						tot = Py_TYPE(value)->tp_as_sequence->sq_length((PyObject *)self);

						BMO_slot_buffer_alloc(&bmop, slot_name, tot);

						i = 0;
						BM_ITER_BPY_BM_SEQ (ele, &iter, ((BPy_BMElemSeq *)value)) {
							((void **)slot->data.buf)[i] = (void *)ele;
							i++;
						}
					}
					/* keep this last */
					else if (PySequence_Check(value)) {
						BMElem **elem_array = NULL;
						Py_ssize_t elem_array_len;

						elem_array = BPy_BMElem_PySeq_As_Array(&bm, value, 0, PY_SSIZE_T_MAX,
						                                       &elem_array_len, BM_VERT | BM_EDGE | BM_FACE,
						                                       TRUE, TRUE, slot_name);

						/* error is set above */
						if (elem_array == NULL) {
							return NULL;
						}

						BMO_slot_buffer_alloc(&bmop, slot_name, elem_array_len);
						memcpy(slot->data.buf, elem_array, sizeof(void *) * elem_array_len);
						PyMem_FREE(elem_array);
					}
					else {
						PyErr_Format(PyExc_TypeError,
						             "%.200s: keyword \"%.200s\" expected "
						             "a bmesh sequence, list, (htype, flag) pair, not %.200s",
						             self->opname, slot_name, Py_TYPE(value)->tp_name);
						return NULL;
					}

#undef BPY_BM_GENERIC_MESH_TEST

					break;
				}
				default:
					/* TODO --- many others */
					PyErr_Format(PyExc_NotImplementedError,
					             "%.200s: keyword \"%.200s\" type %d not working yet!",
					             self->opname, slot_name, slot->slot_type);
					return NULL;
					break;
			}
		}
	}

	BMO_op_exec(bm, &bmop);
	BMO_op_finish(bm, &bmop);

	if (bpy_bm_op_as_py_error(bm) == -1) {
		return NULL;
	}

	Py_RETURN_NONE;
}


PyTypeObject bmesh_op_Type = {
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
	(ternaryfunc)pyrna_op_call, /* ternaryfunc tp_call; */
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


/* bmesh fake module 'bmesh.ops'
 * ***************************** */

static PyObject *bpy_bmesh_fmod_getattro(PyObject *UNUSED(self), PyObject *pyname)
{
	const unsigned int tot = bmesh_total_ops;
	unsigned int i;
	const char *name = _PyUnicode_AsString(pyname);

	for (i = 0; i < tot; i++) {
		if (strcmp(opdefines[i]->name, name) == 0) {
			return bpy_bmesh_op_CreatePyObject(opdefines[i]->name);
		}
	}

	PyErr_Format(PyExc_AttributeError,
	             "BMeshOpsModule: , operator \"%.200s\" doesn't exist",
	             name);
	return NULL;
}

static PyObject *bpy_bmesh_fmod_dir(PyObject *UNUSED(self))
{
	const unsigned int tot = bmesh_total_ops;
	unsigned int i;
	PyObject *ret;

	ret = PyList_New(bmesh_total_ops);

	for (i = 0; i < tot; i++) {
		PyList_SET_ITEM(ret, i, PyUnicode_FromString(opdefines[i]->name));
	}

	return ret;
}

static struct PyMethodDef bpy_bmesh_fmod_methods[] = {
	{"__dir__", (PyCFunction)bpy_bmesh_fmod_dir, METH_NOARGS, NULL},
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
	bpy_bmesh_fmod_getattro,    /* getattrofunc tp_getattro; */
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
	bpy_bmesh_fmod_methods,  /* struct PyMethodDef *tp_methods; */
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
