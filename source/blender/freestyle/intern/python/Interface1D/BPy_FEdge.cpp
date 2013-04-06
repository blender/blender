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

/** \file source/blender/freestyle/intern/python/Interface1D/BPy_FEdge.cpp
 *  \ingroup freestyle
 */

#include "BPy_FEdge.h"

#include "../BPy_Convert.h"
#include "../BPy_Id.h"
#include "../Interface0D/BPy_SVertex.h"
#include "../Interface1D/BPy_ViewEdge.h"
#include "../BPy_Nature.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*----------------------FEdge methods ----------------------------*/

PyDoc_STRVAR(FEdge_doc,
"Class hierarchy: :class:`Interface1D` > :class:`FEdge`\n"
"\n"
"Base Class for feature edges.  This FEdge can represent a silhouette,\n"
"a crease, a ridge/valley, a border or a suggestive contour.  For\n"
"silhouettes, the FEdge is oriented so that the visible face lies on\n"
"the left of the edge.  For borders, the FEdge is oriented so that the\n"
"face lies on the left of the edge.  An FEdge can represent an initial\n"
"edge of the mesh or runs accross a face of the initial mesh depending\n"
"on the smoothness or sharpness of the mesh.  This class is specialized\n"
"into a smooth and a sharp version since their properties slightly vary\n"
"from one to the other.\n"
"\n"
".. method:: FEdge()\n"
"\n"
"   Default constructor.\n"
"\n"
".. method:: FEdge(brother)\n"
"\n"
"   Copy constructor.\n"
"\n"
"   :arg brother: An FEdge object.\n"
"   :type brother: :class:`FEdge`\n"
"\n"
".. method:: FEdge(first_vertex, second_vertex)\n"
"\n"
"   Builds an FEdge going from the first vertex to the second.\n"
"\n"
"   :arg first_vertex: The first SVertex.\n"
"   :type first_vertex: :class:`SVertex`\n"
"   :arg second_vertex: The second SVertex.\n"
"   :type second_vertex: :class:`SVertex`");

static int FEdge_init(BPy_FEdge *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist_1[] = {"brother", NULL};
	static const char *kwlist_2[] = {"first_vertex", "second_vertex", NULL};
	PyObject *obj1 = 0, *obj2 = 0;

	if (PyArg_ParseTupleAndKeywords(args, kwds, "|O!", (char **)kwlist_1, &FEdge_Type, &obj1)) {
		if (!obj1)
			self->fe = new FEdge();
		else
			self->fe = new FEdge(*(((BPy_FEdge *)obj1)->fe));
	}
	else if (PyErr_Clear(),
	         PyArg_ParseTupleAndKeywords(args, kwds, "O!O!", (char **)kwlist_2,
	                                     &SVertex_Type, &obj1, &SVertex_Type, &obj2))
	{
		self->fe = new FEdge(((BPy_SVertex *)obj1)->sv, ((BPy_SVertex *)obj2)->sv);
	}
	else {
		PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
		return -1;
	}
	self->py_if1D.if1D = self->fe;
	self->py_if1D.borrowed = 0;
	return 0;
}

/*----------------------FEdge sequence protocol ----------------------------*/

static Py_ssize_t FEdge_sq_length(BPy_FEdge *self)
{
	return 2;
}

static PyObject *FEdge_sq_item(BPy_FEdge *self, int keynum)
{
	if (keynum < 0)
		keynum += FEdge_sq_length(self);
	if (keynum == 0 || keynum == 1) {
		SVertex *v = self->fe->operator[](keynum);
		if (v)
			return BPy_SVertex_from_SVertex(*v);
		Py_RETURN_NONE;
	}
	PyErr_Format(PyExc_IndexError, "FEdge[index]: index %d out of range", keynum);
	return NULL;
}

static PySequenceMethods BPy_FEdge_as_sequence = {
	(lenfunc)FEdge_sq_length,     /* sq_length */
	NULL,                         /* sq_concat */
	NULL,                         /* sq_repeat */
	(ssizeargfunc)FEdge_sq_item,  /* sq_item */
	NULL,                         /* sq_slice */
	NULL,                         /* sq_ass_item */
	NULL,                         /* *was* sq_ass_slice */
	NULL,                         /* sq_contains */
	NULL,                         /* sq_inplace_concat */
	NULL,                         /* sq_inplace_repeat */
};

/*----------------------FEdge get/setters ----------------------------*/

PyDoc_STRVAR(FEdge_first_svertex_doc,
"The first SVertex constituting this FEdge.\n"
"\n"
":type: :class:`SVertex`");

static PyObject *FEdge_first_svertex_get(BPy_FEdge *self, void *UNUSED(closure))
{
	SVertex *A = self->fe->vertexA();
	if (A)
		return BPy_SVertex_from_SVertex(*A);
	Py_RETURN_NONE;
}

static int FEdge_first_svertex_set(BPy_FEdge *self, PyObject *value, void *UNUSED(closure))
{
	if (!BPy_SVertex_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "value must be an SVertex");
		return -1;
	}
	self->fe->setVertexA(((BPy_SVertex *)value)->sv);
	return 0;
}

PyDoc_STRVAR(FEdge_second_svertex_doc,
"The second SVertex constituting this FEdge.\n"
"\n"
":type: :class:`SVertex`");

static PyObject *FEdge_second_svertex_get(BPy_FEdge *self, void *UNUSED(closure))
{
	SVertex *B = self->fe->vertexB();
	if (B)
		return BPy_SVertex_from_SVertex(*B);
	Py_RETURN_NONE;
}

static int FEdge_second_svertex_set(BPy_FEdge *self, PyObject *value, void *UNUSED(closure))
{
	if (!BPy_SVertex_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "value must be an SVertex");
		return -1;
	}
	self->fe->setVertexB(((BPy_SVertex *)value)->sv);
	return 0;
}

PyDoc_STRVAR(FEdge_next_fedge_doc,
"The FEdge following this one in the ViewEdge.  The value is None if\n"
"this FEdge is the last of the ViewEdge.\n"
"\n"
":type: :class:`FEdge`");

static PyObject *FEdge_next_fedge_get(BPy_FEdge *self, void *UNUSED(closure))
{
	FEdge *fe = self->fe->nextEdge();
	if (fe)
		return Any_BPy_FEdge_from_FEdge(*fe);
	Py_RETURN_NONE;
}

static int FEdge_next_fedge_set(BPy_FEdge *self, PyObject *value, void *UNUSED(closure))
{
	if (!BPy_FEdge_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "value must be an FEdge");
		return -1;
	}
	self->fe->setNextEdge(((BPy_FEdge *)value)->fe);
	return 0;
}

PyDoc_STRVAR(FEdge_previous_fedge_doc,
"The FEdge preceding this one in the ViewEdge.  The value is None if\n"
"this FEdge is the first one of the ViewEdge.\n"
"\n"
":type: :class:`FEdge`");

static PyObject *FEdge_previous_fedge_get(BPy_FEdge *self, void *UNUSED(closure))
{
	FEdge *fe = self->fe->previousEdge();
	if (fe)
		return Any_BPy_FEdge_from_FEdge(*fe);
	Py_RETURN_NONE;
}

static int FEdge_previous_fedge_set(BPy_FEdge *self, PyObject *value, void *UNUSED(closure))
{
	if (!BPy_FEdge_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "value must be an FEdge");
		return -1;
	}
	self->fe->setPreviousEdge(((BPy_FEdge *)value)->fe);
	return 0;
}

PyDoc_STRVAR(FEdge_viewedge_doc,
"The ViewEdge to which this FEdge belongs to.\n"
"\n"
":type: :class:`ViewEdge`");

static PyObject *FEdge_viewedge_get(BPy_FEdge *self, void *UNUSED(closure))
{
	ViewEdge *ve = self->fe->viewedge();
	if (ve)
		return BPy_ViewEdge_from_ViewEdge(*ve);
	Py_RETURN_NONE;
}

static int FEdge_viewedge_set(BPy_FEdge *self, PyObject *value, void *UNUSED(closure))
{
	if (!BPy_ViewEdge_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "value must be an ViewEdge");
		return -1;
	}
	self->fe->setViewEdge(((BPy_ViewEdge *)value)->ve);
	return 0;
}

PyDoc_STRVAR(FEdge_is_smooth_doc,
"True if this FEdge is a smooth FEdge.\n"
"\n"
":type: bool");

static PyObject *FEdge_is_smooth_get(BPy_FEdge *self, void *UNUSED(closure))
{
	return PyBool_from_bool(self->fe->isSmooth());
}

static int FEdge_is_smooth_set(BPy_FEdge *self, PyObject *value, void *UNUSED(closure))
{
	if (!PyBool_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "value must be boolean");
		return -1;
	}
	self->fe->setSmooth(bool_from_PyBool(value));
	return 0;
}

PyDoc_STRVAR(FEdge_id_doc,
"The Id of this FEdge.\n"
"\n"
":type: :class:`Id`");

static PyObject *FEdge_id_get(BPy_FEdge *self, void *UNUSED(closure))
{
	Id id(self->fe->getId());
	return BPy_Id_from_Id(id); // return a copy
}

static int FEdge_id_set(BPy_FEdge *self, PyObject *value, void *UNUSED(closure))
{
	if (!BPy_Id_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "value must be an Id");
		return -1;
	}
	self->fe->setId(*(((BPy_Id *)value)->id));
	return 0;
}

PyDoc_STRVAR(FEdge_nature_doc,
"The nature of this FEdge.\n"
"\n"
":type: :class:`Nature`");

static PyObject *FEdge_nature_get(BPy_FEdge *self, void *UNUSED(closure))
{
	return BPy_Nature_from_Nature(self->fe->getNature());
}

static int FEdge_nature_set(BPy_FEdge *self, PyObject *value, void *UNUSED(closure))
{
	if (!BPy_Nature_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "value must be a Nature");
		return -1;
	}
	self->fe->setNature(PyLong_AsLong((PyObject *)&((BPy_Nature *)value)->i));
	return 0;
}

static PyGetSetDef BPy_FEdge_getseters[] = {
	{(char *)"first_svertex", (getter)FEdge_first_svertex_get, (setter)FEdge_first_svertex_set,
	                          (char *)FEdge_first_svertex_doc, NULL},
	{(char *)"second_svertex", (getter)FEdge_second_svertex_get, (setter)FEdge_second_svertex_set,
	                           (char *)FEdge_second_svertex_doc, NULL},
	{(char *)"next_fedge", (getter)FEdge_next_fedge_get, (setter)FEdge_next_fedge_set,
	                       (char *)FEdge_next_fedge_doc, NULL},
	{(char *)"previous_fedge", (getter)FEdge_previous_fedge_get, (setter)FEdge_previous_fedge_set,
	                           (char *)FEdge_previous_fedge_doc, NULL},
	{(char *)"viewedge", (getter)FEdge_viewedge_get, (setter)FEdge_viewedge_set, (char *)FEdge_viewedge_doc, NULL},
	{(char *)"is_smooth", (getter)FEdge_is_smooth_get, (setter)FEdge_is_smooth_set, (char *)FEdge_is_smooth_doc, NULL},
	{(char *)"id", (getter)FEdge_id_get, (setter)FEdge_id_set, (char *)FEdge_id_doc, NULL},
	{(char *)"nature", (getter)FEdge_nature_get, (setter)FEdge_nature_set, (char *)FEdge_nature_doc, NULL},
	{NULL, NULL, NULL, NULL, NULL}  /* Sentinel */
};

/*-----------------------BPy_FEdge type definition ------------------------------*/

PyTypeObject FEdge_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"FEdge",                        /* tp_name */
	sizeof(BPy_FEdge),              /* tp_basicsize */
	0,                              /* tp_itemsize */
	0,                              /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	0,                              /* tp_repr */
	0,                              /* tp_as_number */
	&BPy_FEdge_as_sequence,         /* tp_as_sequence */
	0,                              /* tp_as_mapping */
	0,                              /* tp_hash  */
	0,                              /* tp_call */
	0,                              /* tp_str */
	0,                              /* tp_getattro */
	0,                              /* tp_setattro */
	0,                              /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
	FEdge_doc,                      /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	0,                              /* tp_methods */
	0,                              /* tp_members */
	BPy_FEdge_getseters,            /* tp_getset */
	&Interface1D_Type,              /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)FEdge_init,           /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
