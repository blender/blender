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

/** \file source/blender/freestyle/intern/python/Interface1D/BPy_ViewEdge.cpp
 *  \ingroup freestyle
 */

#include "BPy_ViewEdge.h"

#include "../BPy_Convert.h"
#include "../BPy_Id.h"
#include "../Interface0D/BPy_ViewVertex.h"
#include "../Interface1D/BPy_FEdge.h"
#include "../Interface1D/BPy_ViewEdge.h"
#include "../BPy_Nature.h"
#include "../BPy_ViewShape.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*----------------------ViewEdge methods ----------------------------*/

PyDoc_STRVAR(ViewEdge_doc,
"Class hierarchy: :class:`Interface1D` > :class:`ViewEdge`\n"
"\n"
"Class defining a ViewEdge.  A ViewEdge in an edge of the image graph.\n"
"it connects two :class:`ViewVertex` objects.  It is made by connecting\n"
"a set of FEdges.\n"
"\n"
".. method:: __init__()\n"
"\n"
"   Default constructor.\n"
"\n"
".. method:: __init__(brother)\n"
"\n"
"   Copy constructor.\n"
"\n"
"   :arg brother: A ViewEdge object.\n"
"   :type brother: :class:`ViewEdge`");

static int ViewEdge_init(BPy_ViewEdge *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"brother", NULL};
	PyObject *brother = 0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O!", (char **)kwlist, &ViewEdge_Type, &brother))
		return -1;
	if (!brother)
		self->ve = new ViewEdge();
	else
		self->ve = new ViewEdge(*(((BPy_ViewEdge *)brother)->ve));
	self->py_if1D.if1D = self->ve;
	self->py_if1D.borrowed = false;
	return 0;
}

PyDoc_STRVAR(ViewEdge_update_fedges_doc,
".. method:: update_fedges()\n"
"\n"
"   Sets Viewedge to this for all embedded fedges.\n");

static PyObject *ViewEdge_update_fedges(BPy_ViewEdge *self)
{
	self->ve->UpdateFEdges();
	Py_RETURN_NONE;
}

static PyMethodDef BPy_ViewEdge_methods[] = {
	{"update_fedges", (PyCFunction)ViewEdge_update_fedges, METH_NOARGS, ViewEdge_update_fedges_doc},
	{NULL, NULL, 0, NULL}
};

/*----------------------ViewEdge get/setters ----------------------------*/

PyDoc_STRVAR(ViewEdge_first_viewvertex_doc,
"The first ViewVertex.\n"
"\n"
":type: :class:`ViewVertex`");

static PyObject *ViewEdge_first_viewvertex_get(BPy_ViewEdge *self, void *UNUSED(closure))
{
	ViewVertex *v = self->ve->A();
	if (v)
		return Any_BPy_ViewVertex_from_ViewVertex(*v);
	Py_RETURN_NONE;
}

static int ViewEdge_first_viewvertex_set(BPy_ViewEdge *self, PyObject *value, void *UNUSED(closure))
{
	if (!BPy_ViewVertex_Check(value))
		return -1;
	self->ve->setA(((BPy_ViewVertex *)value)->vv);
	return 0;
}

PyDoc_STRVAR(ViewEdge_last_viewvertex_doc,
"The second ViewVertex.\n"
"\n"
":type: :class:`ViewVertex`");

static PyObject *ViewEdge_last_viewvertex_get(BPy_ViewEdge *self, void *UNUSED(closure))
{
	ViewVertex *v = self->ve->B();
	if (v)
		return Any_BPy_ViewVertex_from_ViewVertex(*v);
	Py_RETURN_NONE;
}

static int ViewEdge_last_viewvertex_set(BPy_ViewEdge *self, PyObject *value, void *UNUSED(closure))
{
	if (!BPy_ViewVertex_Check(value))
		return -1;
	self->ve->setB(((BPy_ViewVertex *)value)->vv);
	return 0;
}

PyDoc_STRVAR(ViewEdge_first_fedge_doc,
"The first FEdge that constitutes this ViewEdge.\n"
"\n"
":type: :class:`FEdge`");

static PyObject *ViewEdge_first_fedge_get(BPy_ViewEdge *self, void *UNUSED(closure))
{
	FEdge *fe = self->ve->fedgeA();
	if (fe)
		return Any_BPy_FEdge_from_FEdge(*fe);
	Py_RETURN_NONE;
}

static int ViewEdge_first_fedge_set(BPy_ViewEdge *self, PyObject *value, void *UNUSED(closure))
{
	if (!BPy_FEdge_Check(value))
		return -1;
	self->ve->setFEdgeA(((BPy_FEdge *)value)->fe);
	return 0;
}

PyDoc_STRVAR(ViewEdge_last_fedge_doc,
"The last FEdge that constitutes this ViewEdge.\n"
"\n"
":type: :class:`FEdge`");

static PyObject *ViewEdge_last_fedge_get(BPy_ViewEdge *self, void *UNUSED(closure))
{
	FEdge *fe = self->ve->fedgeB();
	if (fe)
		return Any_BPy_FEdge_from_FEdge(*fe);
	Py_RETURN_NONE;
}

static int ViewEdge_last_fedge_set(BPy_ViewEdge *self, PyObject *value, void *UNUSED(closure))
{
	if (!BPy_FEdge_Check(value))
		return -1;
	self->ve->setFEdgeB(((BPy_FEdge *)value)->fe);
	return 0;
}

PyDoc_STRVAR(ViewEdge_viewshape_doc,
"The ViewShape to which this ViewEdge belongs to.\n"
"\n"
":type: :class:`ViewShape`");

static PyObject *ViewEdge_viewshape_get(BPy_ViewEdge *self, void *UNUSED(closure))
{	
	ViewShape *vs = self->ve->viewShape();
	if (vs)
		return BPy_ViewShape_from_ViewShape(*vs);
	Py_RETURN_NONE;
}

static int ViewEdge_viewshape_set(BPy_ViewEdge *self, PyObject *value, void *UNUSED(closure))
{
	if (!BPy_ViewShape_Check(value))
		return -1;
	self->ve->setShape(((BPy_ViewShape *)value)->vs);
	return 0;
}

PyDoc_STRVAR(ViewEdge_occludee_doc,
"The shape that is occluded by the ViewShape to which this ViewEdge\n"
"belongs to.  If no object is occluded, this property is set to None.\n"
"\n"
":type: :class:`ViewShape`");

static PyObject *ViewEdge_occludee_get(BPy_ViewEdge *self, void *UNUSED(closure))
{
	ViewShape *vs = self->ve->aShape();
	if (vs)
		return BPy_ViewShape_from_ViewShape(*vs);
	Py_RETURN_NONE;
}

static int ViewEdge_occludee_set(BPy_ViewEdge *self, PyObject *value, void *UNUSED(closure))
{
	if (!BPy_ViewShape_Check(value))
		return -1;
	self->ve->setaShape(((BPy_ViewShape *)value)->vs);
	return 0;
}

PyDoc_STRVAR(ViewEdge_is_closed_doc,
"True if this ViewEdge forms a closed loop.\n"
"\n"
":type: bool");

static PyObject *ViewEdge_is_closed_get(BPy_ViewEdge *self, void *UNUSED(closure))
{
	return PyBool_from_bool(self->ve->isClosed());
}

PyDoc_STRVAR(ViewEdge_id_doc,
"The Id of this ViewEdge.\n"
"\n"
":type: :class:`Id`");

static PyObject *ViewEdge_id_get(BPy_ViewEdge *self, void *UNUSED(closure))
{
	Id id(self->ve->getId());
	return BPy_Id_from_Id(id); // return a copy
}

static int ViewEdge_id_set(BPy_ViewEdge *self, PyObject *value, void *UNUSED(closure))
{
	if (!BPy_Id_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "value must be an Id");
		return -1;
	}
	self->ve->setId(*(((BPy_Id *)value)->id));
	return 0;
}

PyDoc_STRVAR(ViewEdge_nature_doc,
"The nature of this ViewEdge.\n"
"\n"
":type: :class:`Nature`");

static PyObject *ViewEdge_nature_get(BPy_ViewEdge *self, void *UNUSED(closure))
{
	return BPy_Nature_from_Nature(self->ve->getNature());
}

static int ViewEdge_nature_set(BPy_ViewEdge *self, PyObject *value, void *UNUSED(closure))
{
	if (!BPy_Nature_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "value must be a Nature");
		return -1;
	}
	self->ve->setNature(PyLong_AsLong((PyObject *)&((BPy_Nature *)value)->i));
	return 0;
}

PyDoc_STRVAR(ViewEdge_qi_doc,
"The quantitative invisibility.\n"
"\n"
":type: int");

static PyObject *ViewEdge_qi_get(BPy_ViewEdge *self, void *UNUSED(closure))
{
	return PyLong_FromLong(self->ve->qi());
}

static int ViewEdge_qi_set(BPy_ViewEdge *self, PyObject *value, void *UNUSED(closure))
{
	int qi;

	if ((qi = PyLong_AsLong(value)) == -1 && PyErr_Occurred())
		return -1;
	self->ve->setQI(qi);
	return 0;
}

PyDoc_STRVAR(ViewEdge_chaining_time_stamp_doc,
"The time stamp of this ViewEdge.\n"
"\n"
":type: int");

static PyObject *ViewEdge_chaining_time_stamp_get(BPy_ViewEdge *self, void *UNUSED(closure))
{
	return PyLong_FromLong(self->ve->getChainingTimeStamp());
}

static int ViewEdge_chaining_time_stamp_set(BPy_ViewEdge *self, PyObject *value, void *UNUSED(closure))
{
	int timestamp;

	if ((timestamp = PyLong_AsLong(value)) == -1 && PyErr_Occurred())
		return -1;
	self->ve->setChainingTimeStamp(timestamp);
	return 0;
}

static PyGetSetDef BPy_ViewEdge_getseters[] = {
	{(char *)"first_viewvertex", (getter)ViewEdge_first_viewvertex_get, (setter)ViewEdge_first_viewvertex_set,
	                             (char *)ViewEdge_first_viewvertex_doc, NULL},
	{(char *)"last_viewvertex", (getter)ViewEdge_last_viewvertex_get, (setter)ViewEdge_last_viewvertex_set,
	                            (char *)ViewEdge_last_viewvertex_doc, NULL},
	{(char *)"first_fedge", (getter)ViewEdge_first_fedge_get, (setter)ViewEdge_first_fedge_set,
	                        (char *)ViewEdge_first_fedge_doc, NULL},
	{(char *)"last_fedge", (getter)ViewEdge_last_fedge_get, (setter)ViewEdge_last_fedge_set,
	                       (char *)ViewEdge_last_fedge_doc, NULL},
	{(char *)"viewshape", (getter)ViewEdge_viewshape_get, (setter)ViewEdge_viewshape_set,
	                      (char *)ViewEdge_viewshape_doc, NULL},
	{(char *)"occludee", (getter)ViewEdge_occludee_get, (setter)ViewEdge_occludee_set,
	                     (char *)ViewEdge_occludee_doc, NULL},
	{(char *)"is_closed", (getter)ViewEdge_is_closed_get, (setter)NULL, (char *)ViewEdge_is_closed_doc, NULL},
	{(char *)"id", (getter)ViewEdge_id_get, (setter)ViewEdge_id_set, (char *)ViewEdge_id_doc, NULL},
	{(char *)"nature", (getter)ViewEdge_nature_get, (setter)ViewEdge_nature_set, (char *)ViewEdge_nature_doc, NULL},
	{(char *)"qi", (getter)ViewEdge_qi_get, (setter)ViewEdge_qi_set, (char *)ViewEdge_qi_doc, NULL},
	{(char *)"chaining_time_stamp", (getter)ViewEdge_chaining_time_stamp_get, (setter)ViewEdge_chaining_time_stamp_set,
	                                (char *)ViewEdge_chaining_time_stamp_doc, NULL},
	{NULL, NULL, NULL, NULL, NULL}  /* Sentinel */
};

/*-----------------------BPy_ViewEdge type definition ------------------------------*/

PyTypeObject ViewEdge_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"ViewEdge",                     /* tp_name */
	sizeof(BPy_ViewEdge),           /* tp_basicsize */
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
	ViewEdge_doc,                   /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_ViewEdge_methods,           /* tp_methods */
	0,                              /* tp_members */
	BPy_ViewEdge_getseters,         /* tp_getset */
	&Interface1D_Type,              /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)ViewEdge_init,        /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
