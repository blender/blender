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

/** \file source/blender/freestyle/intern/python/BPy_SShape.cpp
 *  \ingroup freestyle
 */

#include "BPy_SShape.h"

#include "BPy_Convert.h"
#include "BPy_BBox.h"
#include "BPy_Id.h"
#include "Interface0D/BPy_SVertex.h"
#include "Interface1D/BPy_FEdge.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------
int SShape_Init(PyObject *module)
{
	if (module == NULL)
		return -1;

	if (PyType_Ready(&SShape_Type) < 0)
		return -1;
	Py_INCREF(&SShape_Type);
	PyModule_AddObject(module, "SShape", (PyObject *)&SShape_Type);

	return 0;
}

/*----------------------SShape methods ----------------------------*/

PyDoc_STRVAR(SShape_doc,
"Class to define a feature shape.  It is the gathering of feature\n"
"elements from an identified input shape.\n"
"\n"
".. method:: __init__()\n"
"\n"
"   Default constructor.\n"
"\n"
".. method:: __init__(brother)\n"
"\n"
"   Copy constructor.\n"
"\n"
"   :arg brother: An SShape object.\n"
"   :type brother: :class:`SShape`");

static int SShape_init(BPy_SShape *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"brother", NULL};
	PyObject *brother = 0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O!", (char **)kwlist, &SShape_Type, &brother))
		return -1;
	if (!brother)
		self->ss = new SShape();
	else
		self->ss = new SShape(*(((BPy_SShape *)brother)->ss));
	self->borrowed = 0;
	return 0;
}

static void SShape_dealloc(BPy_SShape *self)
{
	if (self->ss && !self->borrowed)
		delete self->ss;
	Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *SShape_repr(BPy_SShape *self)
{
	return PyUnicode_FromFormat("SShape - address: %p", self->ss);
}

static char SShape_add_edge_doc[] =
".. method:: add_edge(edge)\n"
"\n"
"   Adds an FEdge to the list of FEdges.\n"
"\n"
"   :arg edge: An FEdge object.\n"
"   :type edge: :class:`FEdge`\n";

static PyObject *SShape_add_edge(BPy_SShape *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"edge", NULL};
	PyObject *py_fe = 0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!", (char **)kwlist, &FEdge_Type, &py_fe))
		return NULL;
	self->ss->AddEdge(((BPy_FEdge *)py_fe)->fe);
	Py_RETURN_NONE;
}

PyDoc_STRVAR(SShape_add_vertex_doc,
".. method:: add_vertex(vertex)\n"
"\n"
"   Adds an SVertex to the list of SVertex of this Shape.  The SShape\n"
"   attribute of the SVertex is also set to this SShape.\n"
"\n"
"   :arg vertex: An SVertex object.\n"
"   :type vertex: :class:`SVertex`");

static PyObject * SShape_add_vertex(BPy_SShape *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"edge", NULL};
	PyObject *py_sv = 0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!", (char **)kwlist, &SVertex_Type, &py_sv))
		return NULL;
	self->ss->AddNewVertex(((BPy_SVertex *)py_sv)->sv);
	Py_RETURN_NONE;
}

PyDoc_STRVAR(SShape_compute_bbox_doc,
".. method:: compute_bbox()\n"
"\n"
"   Compute the bbox of the SShape.");

static PyObject *SShape_compute_bbox(BPy_SShape *self)
{
	self->ss->ComputeBBox();
	Py_RETURN_NONE;
}

// const Material & 	material (unsigned i) const
// const vector< Material > & 	materials () const
// void 	SetMaterials (const vector< Material > &iMaterials)

static PyMethodDef BPy_SShape_methods[] = {
	{"add_edge", (PyCFunction)SShape_add_edge, METH_VARARGS | METH_KEYWORDS, SShape_add_edge_doc},
	{"add_vertex", (PyCFunction)SShape_add_vertex, METH_VARARGS | METH_KEYWORDS, SShape_add_vertex_doc},
	{"compute_bbox", (PyCFunction)SShape_compute_bbox, METH_NOARGS, SShape_compute_bbox_doc},
	{NULL, NULL, 0, NULL}
};

/*----------------------SShape get/setters ----------------------------*/

PyDoc_STRVAR(SShape_id_doc,
"The Id of this SShape.\n"
"\n"
":type: :class:`Id`");

static PyObject *SShape_id_get(BPy_SShape *self, void *UNUSED(closure))
{
	Id id(self->ss->getId());
	return BPy_Id_from_Id(id); // return a copy
}

static int SShape_id_set(BPy_SShape *self, PyObject *value, void *UNUSED(closure))
{
	if (!BPy_Id_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "value must be an Id");
		return -1;
	}
	self->ss->setId(*(((BPy_Id *)value)->id));
	return 0;
}

PyDoc_STRVAR(SShape_name_doc,
"The name of the SShape.\n"
"\n"
":type: str");

static PyObject *SShape_name_get(BPy_SShape *self, void *UNUSED(closure))
{
	return PyUnicode_FromString(self->ss->getName().c_str());
}

static int SShape_name_set(BPy_SShape *self, PyObject *value, void *UNUSED(closure))
{
	if (!PyUnicode_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "value must be a string");
		return -1;
	}
	const string name = _PyUnicode_AsString(value);
	self->ss->setName(name);
	return 0;
}

PyDoc_STRVAR(SShape_bbox_doc,
"The bounding box of the SShape.\n"
"\n"
":type: :class:`BBox`");

static PyObject *SShape_bbox_get(BPy_SShape *self, void *UNUSED(closure))
{
	BBox<Vec3r> bb(self->ss->bbox());
	return BPy_BBox_from_BBox(bb); // return a copy
}

static int SShape_bbox_set(BPy_SShape *self, PyObject *value, void *UNUSED(closure))
{
	if (!BPy_BBox_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "value must be a BBox");
		return -1;
	}
	self->ss->setBBox(*(((BPy_BBox *)value)->bb));
	return 0;
}

PyDoc_STRVAR(SShape_vertices_doc,
"The list of vertices constituting this SShape.\n"
"\n"
":type: List of :class:`SVertex` objects");

static PyObject *SShape_vertices_get(BPy_SShape *self, void *UNUSED(closure))
{
	PyObject *py_vertices = PyList_New(0);

	vector< SVertex * > vertices = self->ss->getVertexList();
	vector< SVertex * >::iterator it;
	
	for (it = vertices.begin(); it != vertices.end(); it++) {
		PyList_Append(py_vertices, BPy_SVertex_from_SVertex(*(*it)));
	}
	
	return py_vertices;
}

PyDoc_STRVAR(SShape_edges_doc,
"The list of edges constituting this SShape.\n"
"\n"
":type: List of :class:`FEdge` objects");

static PyObject *SShape_edges_get(BPy_SShape *self, void *UNUSED(closure))
{
	PyObject *py_edges = PyList_New(0);

	vector< FEdge * > edges = self->ss->getEdgeList();
	vector< FEdge * >::iterator it;
	
	for (it = edges.begin(); it != edges.end(); it++) {
		PyList_Append(py_edges, Any_BPy_FEdge_from_FEdge(*(*it)));
	}
	
	return py_edges;
}

static PyGetSetDef BPy_SShape_getseters[] = {
	{(char *)"id", (getter)SShape_id_get, (setter)SShape_id_set, (char *)SShape_id_doc, NULL},
	{(char *)"name", (getter)SShape_name_get, (setter)SShape_name_set, (char *)SShape_name_doc, NULL},
	{(char *)"bbox", (getter)SShape_bbox_get, (setter)SShape_bbox_set, (char *)SShape_bbox_doc, NULL},
	{(char *)"edges", (getter)SShape_edges_get, (setter)NULL, (char *)SShape_edges_doc, NULL},
	{(char *)"vertices", (getter)SShape_vertices_get, (setter)NULL, (char *)SShape_vertices_doc, NULL},
	{NULL, NULL, NULL, NULL, NULL}  /* Sentinel */
};

/*-----------------------BPy_SShape type definition ------------------------------*/

PyTypeObject SShape_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"SShape",                       /* tp_name */
	sizeof(BPy_SShape),             /* tp_basicsize */
	0,                              /* tp_itemsize */
	(destructor)SShape_dealloc,     /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	(reprfunc)SShape_repr,          /* tp_repr */
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
	SShape_doc,                     /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_SShape_methods,             /* tp_methods */
	0,                              /* tp_members */
	BPy_SShape_getseters,           /* tp_getset */
	0,                              /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)SShape_init,          /* tp_init */
	0,                              /* tp_alloc */
	PyType_GenericNew,              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
