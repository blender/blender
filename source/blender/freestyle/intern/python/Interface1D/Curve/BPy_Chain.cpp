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

/** \file source/blender/freestyle/intern/python/Interface1D/Curve/BPy_Chain.cpp
 *  \ingroup freestyle
 */

#include "BPy_Chain.h"

#include "../../BPy_Convert.h"
#include "../../BPy_Id.h"
#include "../BPy_ViewEdge.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*----------------------Chain methods ----------------------------*/

PyDoc_STRVAR(Chain_doc,
"Class hierarchy: :class:`Interface1D` > :class:`Curve` > :class:`Chain`\n"
"\n"
"Class to represent a 1D elements issued from the chaining process.  A\n"
"Chain is the last step before the :class:`Stroke` and is used in the\n"
"Splitting and Creation processes.\n"
"\n"
".. method:: __init__()\n"
"\n"
"   Default constructor.\n"
"\n"
".. method:: __init__(brother)\n"
"\n"
"   Copy constructor.\n"
"\n"
"   :arg brother: A Chain object.\n"
"   :type brother: :class:`Chain`\n"
"\n"
".. method:: __init__(id)\n"
"\n"
"   Builds a chain from its Id.\n"
"\n"
"   :arg id: An Id object.\n"
"   :type id: :class:`Id`");

static int Chain_init(BPy_Chain *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist_1[] = {"brother", NULL};
	static const char *kwlist_2[] = {"id", NULL};
	PyObject *obj = 0;

	if (PyArg_ParseTupleAndKeywords(args, kwds, "|O!", (char **)kwlist_1, &Chain_Type, &obj)) {
		if (!obj)
			self->c = new Chain();
		else
			self->c = new Chain(*(((BPy_Chain *)obj)->c));
	}
	else if (PyErr_Clear(),
	         PyArg_ParseTupleAndKeywords(args, kwds, "O!", (char **)kwlist_2, &Id_Type, &obj))
	{
		self->c = new Chain(*(((BPy_Id *)obj)->id));
	}
	else {
		PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
		return -1;
	}
	self->py_c.c = self->c;
	self->py_c.py_if1D.if1D = self->c;
	self->py_c.py_if1D.borrowed = false;
	return 0;
}

PyDoc_STRVAR(Chain_push_viewedge_back_doc,
".. method:: push_viewedge_back(viewedge, orientation)\n"
"\n"
"   Adds a ViewEdge at the end of the Chain.\n"
"\n"
"   :arg viewedge: The ViewEdge that must be added.\n"
"   :type viewedge: :class:`ViewEdge`\n"
"   :arg orientation: The orientation with which the ViewEdge must be\n"
"      processed.\n"
"   :type orientation: bool");

static PyObject *Chain_push_viewedge_back(BPy_Chain *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"viewedge", "orientation", NULL};
	PyObject *obj1 = 0, *obj2 = 0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!O!", (char **)kwlist,
	                                 &ViewEdge_Type, &obj1, &PyBool_Type, &obj2))
	{
		return NULL;
	}
	ViewEdge *ve = ((BPy_ViewEdge *)obj1)->ve;
	bool orientation = bool_from_PyBool(obj2);
	self->c->push_viewedge_back(ve, orientation);
	Py_RETURN_NONE;
}

PyDoc_STRVAR(Chain_push_viewedge_front_doc,
".. method:: push_viewedge_front(viewedge, orientation)\n"
"\n"
"   Adds a ViewEdge at the beginning of the Chain.\n"
"\n"
"   :arg viewedge: The ViewEdge that must be added.\n"
"   :type viewedge: :class:`ViewEdge`\n"
"   :arg orientation: The orientation with which the ViewEdge must be\n"
"      processed.\n"
"   :type orientation: bool");

static PyObject *Chain_push_viewedge_front(BPy_Chain *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"viewedge", "orientation", NULL};
	PyObject *obj1 = 0, *obj2 = 0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!O!", (char **)kwlist,
	                                 &ViewEdge_Type, &obj1, &PyBool_Type, &obj2))
	{
		return NULL;
	}
	ViewEdge *ve = ((BPy_ViewEdge *)obj1)->ve;
	bool orientation = bool_from_PyBool(obj2);
	self->c->push_viewedge_front(ve, orientation);
	Py_RETURN_NONE;
}

static PyMethodDef BPy_Chain_methods[] = {
	{"push_viewedge_back", (PyCFunction)Chain_push_viewedge_back, METH_VARARGS | METH_KEYWORDS,
	                       Chain_push_viewedge_back_doc},
	{"push_viewedge_front", (PyCFunction)Chain_push_viewedge_front, METH_VARARGS | METH_KEYWORDS,
	                        Chain_push_viewedge_front_doc},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_Chain type definition ------------------------------*/

PyTypeObject Chain_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"Chain",                        /* tp_name */
	sizeof(BPy_Chain),              /* tp_basicsize */
	0,                              /* tp_itemsize */
	0,                              /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	0,                              /* tp_repr */
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
	Chain_doc,                      /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_Chain_methods,              /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&FrsCurve_Type,                 /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)Chain_init,           /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
