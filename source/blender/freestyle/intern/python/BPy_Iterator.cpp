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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file source/blender/freestyle/intern/python/BPy_Iterator.cpp
 *  \ingroup freestyle
 */

#include "BPy_Iterator.h"

#include "BPy_Convert.h"
#include "Iterator/BPy_AdjacencyIterator.h"
#include "Iterator/BPy_Interface0DIterator.h"
#include "Iterator/BPy_CurvePointIterator.h"
#include "Iterator/BPy_StrokeVertexIterator.h"
#include "Iterator/BPy_SVertexIterator.h"
#include "Iterator/BPy_orientedViewEdgeIterator.h"
#include "Iterator/BPy_ViewEdgeIterator.h"
#include "Iterator/BPy_ChainingIterator.h"
#include "Iterator/BPy_ChainPredicateIterator.h"
#include "Iterator/BPy_ChainSilhouetteIterator.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------
int Iterator_Init(PyObject *module)
{
	if (module == NULL)
		return -1;

	if (PyType_Ready(&Iterator_Type) < 0)
		return -1;
	Py_INCREF(&Iterator_Type);
	PyModule_AddObject(module, "Iterator", (PyObject *)&Iterator_Type);

	if (PyType_Ready(&AdjacencyIterator_Type) < 0)
		return -1;
	Py_INCREF(&AdjacencyIterator_Type);
	PyModule_AddObject(module, "AdjacencyIterator", (PyObject *)&AdjacencyIterator_Type);

	if (PyType_Ready(&Interface0DIterator_Type) < 0)
		return -1;
	Py_INCREF(&Interface0DIterator_Type);
	PyModule_AddObject(module, "Interface0DIterator", (PyObject *)&Interface0DIterator_Type);

	if (PyType_Ready(&CurvePointIterator_Type) < 0)
		return -1;
	Py_INCREF(&CurvePointIterator_Type);
	PyModule_AddObject(module, "CurvePointIterator", (PyObject *)&CurvePointIterator_Type);

	if (PyType_Ready(&StrokeVertexIterator_Type) < 0)
		return -1;
	Py_INCREF(&StrokeVertexIterator_Type);
	PyModule_AddObject(module, "StrokeVertexIterator", (PyObject *)&StrokeVertexIterator_Type);

	if (PyType_Ready(&SVertexIterator_Type) < 0)
		return -1;
	Py_INCREF(&SVertexIterator_Type);
	PyModule_AddObject(module, "SVertexIterator", (PyObject *)&SVertexIterator_Type);

	if (PyType_Ready(&orientedViewEdgeIterator_Type) < 0)
		return -1;
	Py_INCREF(&orientedViewEdgeIterator_Type);
	PyModule_AddObject(module, "orientedViewEdgeIterator", (PyObject *)&orientedViewEdgeIterator_Type);

	if (PyType_Ready(&ViewEdgeIterator_Type) < 0)
		return -1;
	Py_INCREF(&ViewEdgeIterator_Type);
	PyModule_AddObject(module, "ViewEdgeIterator", (PyObject *)&ViewEdgeIterator_Type);

	if (PyType_Ready(&ChainingIterator_Type) < 0)
		return -1;
	Py_INCREF(&ChainingIterator_Type);
	PyModule_AddObject(module, "ChainingIterator", (PyObject *)&ChainingIterator_Type);

	if (PyType_Ready(&ChainPredicateIterator_Type) < 0)
		return -1;
	Py_INCREF(&ChainPredicateIterator_Type);
	PyModule_AddObject(module, "ChainPredicateIterator", (PyObject *)&ChainPredicateIterator_Type);

	if (PyType_Ready(&ChainSilhouetteIterator_Type) < 0)
		return -1;
	Py_INCREF(&ChainSilhouetteIterator_Type);
	PyModule_AddObject(module, "ChainSilhouetteIterator", (PyObject *)&ChainSilhouetteIterator_Type);

	return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(Iterator_doc,
"Base class to define iterators.\n"
"\n"
".. method:: __init__()\n"
"\n"
"   Default constructor.");

static int Iterator_init(BPy_Iterator *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "", (char **)kwlist))
		return -1;
	self->it = new Iterator();
	return 0;
}

static void Iterator_dealloc(BPy_Iterator *self)
{
	if (self->it)
		delete self->it;
	Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *Iterator_repr(BPy_Iterator *self)
{
	return PyUnicode_FromFormat("type: %s - address: %p", Py_TYPE(self)->tp_name, self->it);
}

PyDoc_STRVAR(Iterator_increment_doc,
".. method:: increment()\n"
"\n"
"   Makes the iterator point the next element.");

static PyObject *Iterator_increment(BPy_Iterator *self)
{
	if (self->it->isEnd()) {
		PyErr_SetString(PyExc_RuntimeError, "cannot increment any more");
		return NULL;
	}
	self->it->increment();
	Py_RETURN_NONE;
}

PyDoc_STRVAR(Iterator_decrement_doc,
".. method:: decrement()\n"
"\n"
"   Makes the iterator point the previous element.");

static PyObject *Iterator_decrement(BPy_Iterator *self)
{
	if (self->it->isBegin()) {
		PyErr_SetString(PyExc_RuntimeError, "cannot decrement any more");
		return NULL;
	}
	self->it->decrement();
	Py_RETURN_NONE;
}

static PyMethodDef BPy_Iterator_methods[] = {
	{"increment", (PyCFunction) Iterator_increment, METH_NOARGS, Iterator_increment_doc},
	{"decrement", (PyCFunction) Iterator_decrement, METH_NOARGS, Iterator_decrement_doc},
	{NULL, NULL, 0, NULL}
};

/*----------------------Iterator get/setters ----------------------------*/

PyDoc_STRVAR(Iterator_name_doc,
"The string of the name of this iterator.\n"
"\n"
":type: str");

static PyObject *Iterator_name_get(BPy_Iterator *self, void *UNUSED(closure))
{
	return PyUnicode_FromString(Py_TYPE(self)->tp_name);
}

PyDoc_STRVAR(Iterator_is_begin_doc,
"True if the interator points the first element.\n"
"\n"
":type: bool");

static PyObject *Iterator_is_begin_get(BPy_Iterator *self, void *UNUSED(closure))
{
	return PyBool_from_bool(self->it->isBegin());
}

PyDoc_STRVAR(Iterator_is_end_doc,
"True if the interator points the last element.\n"
"\n"
":type: bool");

static PyObject *Iterator_is_end_get(BPy_Iterator *self, void *UNUSED(closure))
{
	return PyBool_from_bool(self->it->isEnd());
}

static PyGetSetDef BPy_Iterator_getseters[] = {
	{(char *)"name", (getter)Iterator_name_get, (setter)NULL, (char *)Iterator_name_doc, NULL},
	{(char *)"is_begin", (getter)Iterator_is_begin_get, (setter)NULL, (char *)Iterator_is_begin_doc, NULL},
	{(char *)"is_end", (getter)Iterator_is_end_get, (setter)NULL, (char *)Iterator_is_end_doc, NULL},
	{NULL, NULL, NULL, NULL, NULL}  /* Sentinel */
};

/*-----------------------BPy_Iterator type definition ------------------------------*/

PyTypeObject Iterator_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"Iterator",                     /* tp_name */
	sizeof(BPy_Iterator),           /* tp_basicsize */
	0,                              /* tp_itemsize */
	(destructor)Iterator_dealloc,   /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	(reprfunc)Iterator_repr,        /* tp_repr */
	0,                              /* tp_as_number */
	0,                              /* tp_as_sequence */
	0,                              /* tp_as_mapping */
	0,                              /* tp_hash  */
	0,                              /* tp_call */
	0,                              /* tp_str */
	0,                              /* tp_getattro */
	0,                              /* tp_setattro */
	0,                              /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
	Iterator_doc,                   /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_Iterator_methods,           /* tp_methods */
	0,                              /* tp_members */
	BPy_Iterator_getseters,         /* tp_getset */
	0,                              /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)Iterator_init,        /* tp_init */
	0,                              /* tp_alloc */
	PyType_GenericNew,              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
