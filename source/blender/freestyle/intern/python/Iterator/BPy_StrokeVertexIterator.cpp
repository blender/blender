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
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file source/blender/freestyle/intern/python/Iterator/BPy_StrokeVertexIterator.cpp
 *  \ingroup freestyle
 */

#include "BPy_StrokeVertexIterator.h"

#include "../BPy_Convert.h"
#include "BPy_Interface0DIterator.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(StrokeVertexIterator_doc,
"Class hierarchy: :class:`Iterator` > :class:`StrokeVertexIterator`\n"
"\n"
"Class defining an iterator designed to iterate over the\n"
":class:`StrokeVertex` of a :class:`Stroke`.  An instance of a\n"
"StrokeVertexIterator can only be obtained from a Stroke by calling\n"
"strokeVerticesBegin() or strokeVerticesEnd().  It is iterating over\n"
"the same vertices as an :class:`Interface0DIterator`.  The difference\n"
"resides in the object access.  Indeed, an Interface0DIterator allows\n"
"only an access to an Interface0D whereas we could need to access the\n"
"specialized StrokeVertex type.  In this case, one should use a\n"
"StrokeVertexIterator.  The castToInterface0DIterator() method is\n"
"useful to get an Interface0DIterator from a StrokeVertexIterator in\n"
"order to call any functions of the UnaryFunction0D type.\n"
"\n"
".. method:: __init__()\n"
"\n"
"   Default constructor.\n"
"\n"
".. method:: __init__(brother)\n"
"\n"
"   Copy constructor.\n"
"\n"
"   :arg brother: A StrokeVertexIterator object.\n"
"   :type brother: :class:`StrokeVertexIterator`");

static int StrokeVertexIterator_init(BPy_StrokeVertexIterator *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"brother", NULL};
	PyObject *brother = 0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O!", (char **)kwlist, &StrokeVertexIterator_Type, &brother))
		return -1;
	if (!brother)
		self->sv_it = new StrokeInternal::StrokeVertexIterator();
	else
		self->sv_it = new StrokeInternal::StrokeVertexIterator(*(((BPy_StrokeVertexIterator *)brother)->sv_it));
	self->py_it.it = self->sv_it;
	self->reversed = 0;
	return 0;
}

static PyObject *StrokeVertexIterator_iternext(BPy_StrokeVertexIterator *self)
{
	StrokeVertex *sv;

	if (self->reversed) {
		if (self->sv_it->isBegin()) {
			PyErr_SetNone(PyExc_StopIteration);
			return NULL;
		}
		self->sv_it->decrement();
		sv = self->sv_it->operator->();
	}
	else {
		if (self->sv_it->isEnd()) {
			PyErr_SetNone(PyExc_StopIteration);
			return NULL;
		}
		sv = self->sv_it->operator->();
		self->sv_it->increment();
	}
	return BPy_StrokeVertex_from_StrokeVertex(*sv);
}

/*----------------------StrokeVertexIterator get/setters ----------------------------*/

PyDoc_STRVAR(StrokeVertexIterator_object_doc,
"The StrokeVertex object currently pointed by this iterator.\n"
"\n"
":type: :class:`StrokeVertex`");

static PyObject *StrokeVertexIterator_object_get(BPy_StrokeVertexIterator *self, void *UNUSED(closure))
{
	if (!self->reversed && self->sv_it->isEnd())
		Py_RETURN_NONE;
	StrokeVertex *sv = self->sv_it->operator->();
	if (sv)
		return BPy_StrokeVertex_from_StrokeVertex(*sv);
	Py_RETURN_NONE;
}

PyDoc_STRVAR(StrokeVertexIterator_t_doc,
"The curvilinear abscissa of the current point.\n"
"\n"
":type: float");

static PyObject *StrokeVertexIterator_t_get(BPy_StrokeVertexIterator *self, void *UNUSED(closure))
{
	return PyFloat_FromDouble(self->sv_it->t());
}

PyDoc_STRVAR(StrokeVertexIterator_u_doc,
"The point parameter at the current point in the stroke (0 <= u <= 1).\n"
"\n"
":type: float");

static PyObject *StrokeVertexIterator_u_get(BPy_StrokeVertexIterator *self, void *UNUSED(closure))
{
	return PyFloat_FromDouble(self->sv_it->u());
}

static PyGetSetDef BPy_StrokeVertexIterator_getseters[] = {
	{(char *)"object", (getter)StrokeVertexIterator_object_get, (setter)NULL,
	                   (char *)StrokeVertexIterator_object_doc, NULL},
	{(char *)"t", (getter)StrokeVertexIterator_t_get, (setter)NULL, (char *)StrokeVertexIterator_t_doc, NULL},
	{(char *)"u", (getter)StrokeVertexIterator_u_get, (setter)NULL, (char *)StrokeVertexIterator_u_doc, NULL},
	{NULL, NULL, NULL, NULL, NULL}  /* Sentinel */
};

/*-----------------------BPy_StrokeVertexIterator type definition ------------------------------*/

PyTypeObject StrokeVertexIterator_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"StrokeVertexIterator",         /* tp_name */
	sizeof(BPy_StrokeVertexIterator), /* tp_basicsize */
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
	StrokeVertexIterator_doc,       /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	PyObject_SelfIter,              /* tp_iter */
	(iternextfunc)StrokeVertexIterator_iternext, /* tp_iternext */
	0,                              /* tp_methods */
	0,                              /* tp_members */
	BPy_StrokeVertexIterator_getseters, /* tp_getset */
	&Iterator_Type,                 /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)StrokeVertexIterator_init, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
