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

/** \file source/blender/freestyle/intern/python/BPy_ViewMap.cpp
 *  \ingroup freestyle
 */

#include "BPy_ViewMap.h"

#include "BPy_Convert.h"
#include "BPy_BBox.h"
#include "Interface1D/BPy_FEdge.h"
#include "Interface1D/BPy_ViewEdge.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------
int ViewMap_Init(PyObject *module)
{
	if (module == NULL)
		return -1;

	if (PyType_Ready( &ViewMap_Type ) < 0)
		return -1;
	Py_INCREF(&ViewMap_Type);
	PyModule_AddObject(module, "ViewMap", (PyObject *)&ViewMap_Type);

	return 0;
}

/*----------------------ViewMap methods----------------------------*/

PyDoc_STRVAR(ViewMap_doc,
"Class defining the ViewMap.\n"
"\n"
".. method:: __init__()\n"
"\n"
"   Default constructor.");

static int ViewMap_init(BPy_ViewMap *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "", (char **)kwlist))
		return -1;
	self->vm = new ViewMap();
	return 0;
}

static void ViewMap_dealloc(BPy_ViewMap *self)
{
	if (self->vm)
		delete self->vm;
	Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *ViewMap_repr(BPy_ViewMap *self)
{
	return PyUnicode_FromFormat("ViewMap - address: %p", self->vm);
}

PyDoc_STRVAR(ViewMap_get_closest_viewedge_doc,
".. method:: get_closest_viewedge(x, y)\n"
"\n"
"   Gets the ViewEdge nearest to the 2D point specified as arguments.\n"
"\n"
"   :arg x: X coordinate of a 2D point.\n"
"   :type x: float\n"
"   :arg y: Y coordinate of a 2D point.\n"
"   :type y: float\n"
"   :return: The ViewEdge nearest to the specified 2D point.\n"
"   :rtype: :class:`ViewEdge`");

static PyObject *ViewMap_get_closest_viewedge(BPy_ViewMap *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"x", "y", NULL};
	double x, y;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "dd", (char **)kwlist, &x, &y))
		return NULL;
	ViewEdge *ve = const_cast<ViewEdge *>(self->vm->getClosestViewEdge(x, y));
	if (ve)
		return BPy_ViewEdge_from_ViewEdge(*ve);
	Py_RETURN_NONE;
}

PyDoc_STRVAR(ViewMap_get_closest_fedge_doc,
".. method:: get_closest_fedge(x, y)\n"
"\n"
"   Gets the FEdge nearest to the 2D point specified as arguments.\n"
"\n"
"   :arg x: X coordinate of a 2D point.\n"
"   :type x: float\n"
"   :arg y: Y coordinate of a 2D point.\n"
"   :type y: float\n"
"   :return: The FEdge nearest to the specified 2D point.\n"
"   :rtype: :class:`FEdge`");

static PyObject *ViewMap_get_closest_fedge(BPy_ViewMap *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"x", "y", NULL};
	double x, y;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "dd", (char **)kwlist, &x, &y))
		return NULL;
	FEdge *fe = const_cast<FEdge *>(self->vm->getClosestFEdge(x, y));
	if (fe)
		return Any_BPy_FEdge_from_FEdge(*fe);
	Py_RETURN_NONE;
}

// static ViewMap *getInstance ();

static PyMethodDef BPy_ViewMap_methods[] = {
	{"get_closest_viewedge", (PyCFunction)ViewMap_get_closest_viewedge, METH_VARARGS | METH_KEYWORDS,
	                         ViewMap_get_closest_viewedge_doc},
	{"get_closest_fedge", (PyCFunction)ViewMap_get_closest_fedge, METH_VARARGS | METH_KEYWORDS,
	                      ViewMap_get_closest_fedge_doc},
	{NULL, NULL, 0, NULL}
};

/*----------------------ViewMap get/setters ----------------------------*/

PyDoc_STRVAR(ViewMap_scene_bbox_doc,
"The 3D bounding box of the scene.\n"
"\n"
":type: :class:`BBox`");

static PyObject *ViewMap_scene_bbox_get(BPy_ViewMap *self, void *UNUSED(closure))
{
	return BPy_BBox_from_BBox(self->vm->getScene3dBBox());
}

static int ViewMap_scene_bbox_set(BPy_ViewMap *self, PyObject *value, void *UNUSED(closure))
{
	if (!BPy_BBox_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "value must be a BBox");
		return -1;
	}
	self->vm->setScene3dBBox(*(((BPy_BBox *)value)->bb));
	return 0;
}

static PyGetSetDef BPy_ViewMap_getseters[] = {
	{(char *)"scene_bbox", (getter)ViewMap_scene_bbox_get, (setter)ViewMap_scene_bbox_set,
	                       (char *)ViewMap_scene_bbox_doc, NULL},
	{NULL, NULL, NULL, NULL, NULL}  /* Sentinel */
};

/*-----------------------BPy_ViewMap type definition ------------------------------*/

PyTypeObject ViewMap_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"ViewMap",                      /* tp_name */
	sizeof(BPy_ViewMap),            /* tp_basicsize */
	0,                              /* tp_itemsize */
	(destructor)ViewMap_dealloc,    /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	(reprfunc)ViewMap_repr,         /* tp_repr */
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
	ViewMap_doc,                    /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_ViewMap_methods,            /* tp_methods */
	0,                              /* tp_members */
	BPy_ViewMap_getseters,          /* tp_getset */
	0,                              /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)ViewMap_init,         /* tp_init */
	0,                              /* tp_alloc */
	PyType_GenericNew,              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
