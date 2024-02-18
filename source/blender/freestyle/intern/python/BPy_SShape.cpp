/* SPDX-FileCopyrightText: 2004-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_SShape.h"

#include "BPy_BBox.h"
#include "BPy_Convert.h"
#include "BPy_Id.h"
#include "Interface0D/BPy_SVertex.h"
#include "Interface1D/BPy_FEdge.h"

#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------
int SShape_Init(PyObject *module)
{
  if (module == nullptr) {
    return -1;
  }

  if (PyType_Ready(&SShape_Type) < 0) {
    return -1;
  }
  Py_INCREF(&SShape_Type);
  PyModule_AddObject(module, "SShape", (PyObject *)&SShape_Type);

  return 0;
}

/*----------------------SShape methods ----------------------------*/

PyDoc_STRVAR(
    /* Wrap. */
    SShape_doc,
    "Class to define a feature shape. It is the gathering of feature\n"
    "elements from an identified input shape.\n"
    "\n"
    ".. method:: __init__()\n"
    "            __init__(brother)\n"
    "\n"
    "   Creates a :class:`SShape` class using either a default constructor or copy constructor.\n"
    "\n"
    "   :arg brother: An SShape object.\n"
    "   :type brother: :class:`SShape`");

static int SShape_init(BPy_SShape *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"brother", nullptr};
  PyObject *brother = nullptr;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O!", (char **)kwlist, &SShape_Type, &brother)) {
    return -1;
  }
  if (!brother) {
    self->ss = new SShape();
  }
  else {
    self->ss = new SShape(*(((BPy_SShape *)brother)->ss));
  }
  self->borrowed = false;
  return 0;
}

static void SShape_dealloc(BPy_SShape *self)
{
  if (self->ss && !self->borrowed) {
    delete self->ss;
  }
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
  static const char *kwlist[] = {"edge", nullptr};
  PyObject *py_fe = nullptr;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!", (char **)kwlist, &FEdge_Type, &py_fe)) {
    return nullptr;
  }
  self->ss->AddEdge(((BPy_FEdge *)py_fe)->fe);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    SShape_add_vertex_doc,
    ".. method:: add_vertex(vertex)\n"
    "\n"
    "   Adds an SVertex to the list of SVertex of this Shape. The SShape\n"
    "   attribute of the SVertex is also set to this SShape.\n"
    "\n"
    "   :arg vertex: An SVertex object.\n"
    "   :type vertex: :class:`SVertex`");

static PyObject *SShape_add_vertex(BPy_SShape *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"edge", nullptr};
  PyObject *py_sv = nullptr;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!", (char **)kwlist, &SVertex_Type, &py_sv)) {
    return nullptr;
  }
  self->ss->AddNewVertex(((BPy_SVertex *)py_sv)->sv);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    SShape_compute_bbox_doc,
    ".. method:: compute_bbox()\n"
    "\n"
    "   Compute the bbox of the SShape.");

static PyObject *SShape_compute_bbox(BPy_SShape *self)
{
  self->ss->ComputeBBox();
  Py_RETURN_NONE;
}

// const Material &     material (uint i) const
// const vector< Material > &   materials () const
// void     SetMaterials (const vector< Material > &iMaterials)

static PyMethodDef BPy_SShape_methods[] = {
    {"add_edge", (PyCFunction)SShape_add_edge, METH_VARARGS | METH_KEYWORDS, SShape_add_edge_doc},
    {"add_vertex",
     (PyCFunction)SShape_add_vertex,
     METH_VARARGS | METH_KEYWORDS,
     SShape_add_vertex_doc},
    {"compute_bbox", (PyCFunction)SShape_compute_bbox, METH_NOARGS, SShape_compute_bbox_doc},
    {nullptr, nullptr, 0, nullptr},
};

/*----------------------SShape get/setters ----------------------------*/

PyDoc_STRVAR(
    /* Wrap. */
    SShape_id_doc,
    "The Id of this SShape.\n"
    "\n"
    ":type: :class:`Id`");

static PyObject *SShape_id_get(BPy_SShape *self, void * /*closure*/)
{
  Id id(self->ss->getId());
  return BPy_Id_from_Id(id);  // return a copy
}

static int SShape_id_set(BPy_SShape *self, PyObject *value, void * /*closure*/)
{
  if (!BPy_Id_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "value must be an Id");
    return -1;
  }
  self->ss->setId(*(((BPy_Id *)value)->id));
  return 0;
}

PyDoc_STRVAR(
    /* Wrap. */
    SShape_name_doc,
    "The name of the SShape.\n"
    "\n"
    ":type: str");

static PyObject *SShape_name_get(BPy_SShape *self, void * /*closure*/)
{
  return PyUnicode_FromString(self->ss->getName().c_str());
}

static int SShape_name_set(BPy_SShape *self, PyObject *value, void * /*closure*/)
{
  if (!PyUnicode_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "value must be a string");
    return -1;
  }
  const char *name = PyUnicode_AsUTF8(value);
  self->ss->setName(name);
  return 0;
}

PyDoc_STRVAR(
    /* Wrap. */
    SShape_bbox_doc,
    "The bounding box of the SShape.\n"
    "\n"
    ":type: :class:`BBox`");

static PyObject *SShape_bbox_get(BPy_SShape *self, void * /*closure*/)
{
  BBox<Vec3r> bb(self->ss->bbox());
  return BPy_BBox_from_BBox(bb);  // return a copy
}

static int SShape_bbox_set(BPy_SShape *self, PyObject *value, void * /*closure*/)
{
  if (!BPy_BBox_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "value must be a BBox");
    return -1;
  }
  self->ss->setBBox(*(((BPy_BBox *)value)->bb));
  return 0;
}

PyDoc_STRVAR(
    /* Wrap. */
    SShape_vertices_doc,
    "The list of vertices constituting this SShape.\n"
    "\n"
    ":type: List of :class:`SVertex` objects");

static PyObject *SShape_vertices_get(BPy_SShape *self, void * /*closure*/)
{

  vector<SVertex *> vertices = self->ss->getVertexList();
  vector<SVertex *>::iterator it;
  PyObject *py_vertices = PyList_New(vertices.size());
  uint i = 0;

  for (it = vertices.begin(); it != vertices.end(); it++) {
    PyList_SET_ITEM(py_vertices, i++, BPy_SVertex_from_SVertex(*(*it)));
  }

  return py_vertices;
}

PyDoc_STRVAR(
    /* Wrap. */
    SShape_edges_doc,
    "The list of edges constituting this SShape.\n"
    "\n"
    ":type: List of :class:`FEdge` objects");

static PyObject *SShape_edges_get(BPy_SShape *self, void * /*closure*/)
{

  vector<FEdge *> edges = self->ss->getEdgeList();
  vector<FEdge *>::iterator it;
  PyObject *py_edges = PyList_New(edges.size());
  uint i = 0;

  for (it = edges.begin(); it != edges.end(); it++) {
    PyList_SET_ITEM(py_edges, i++, Any_BPy_FEdge_from_FEdge(*(*it)));
  }

  return py_edges;
}

static PyGetSetDef BPy_SShape_getseters[] = {
    {"id", (getter)SShape_id_get, (setter)SShape_id_set, SShape_id_doc, nullptr},
    {"name", (getter)SShape_name_get, (setter)SShape_name_set, SShape_name_doc, nullptr},
    {"bbox", (getter)SShape_bbox_get, (setter)SShape_bbox_set, SShape_bbox_doc, nullptr},
    {"edges", (getter)SShape_edges_get, (setter) nullptr, SShape_edges_doc, nullptr},
    {"vertices", (getter)SShape_vertices_get, (setter) nullptr, SShape_vertices_doc, nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/*-----------------------BPy_SShape type definition ------------------------------*/

PyTypeObject SShape_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "SShape",
    /*tp_basicsize*/ sizeof(BPy_SShape),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)SShape_dealloc,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ (reprfunc)SShape_repr,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ nullptr,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    /*tp_doc*/ SShape_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ BPy_SShape_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ BPy_SShape_getseters,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)SShape_init,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ PyType_GenericNew,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
