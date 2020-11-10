/*
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
 */

/** \file
 * \ingroup freestyle
 */

#include "BPy_ViewShape.h"

#include "BPy_Convert.h"
#include "BPy_SShape.h"
#include "Interface0D/BPy_ViewVertex.h"
#include "Interface1D/BPy_ViewEdge.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------

int ViewShape_Init(PyObject *module)
{
  if (module == nullptr) {
    return -1;
  }

  if (PyType_Ready(&ViewShape_Type) < 0) {
    return -1;
  }
  Py_INCREF(&ViewShape_Type);
  PyModule_AddObject(module, "ViewShape", (PyObject *)&ViewShape_Type);

  return 0;
}

/*----------------------ViewShape methods ----------------------------*/

PyDoc_STRVAR(ViewShape_doc,
             "Class gathering the elements of the ViewMap (i.e., :class:`ViewVertex`\n"
             "and :class:`ViewEdge`) that are issued from the same input shape.\n"
             "\n"
             ".. method:: __init__()\n"
             "            __init__(brother)\n"
             "            __init__(sshape)\n"
             "\n"
             "   Builds a :class:`ViewShape` using the default constructor,\n"
             "   copy constructor, or from a :class:`SShape`.\n"
             "\n"
             "   :arg brother: A ViewShape object.\n"
             "   :type brother: :class:`ViewShape`\n"
             "   :arg sshape: An SShape object.\n"
             "   :type sshape: :class:`SShape`");

static int ViewShape_init(BPy_ViewShape *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist_1[] = {"brother", nullptr};
  static const char *kwlist_2[] = {"sshape", nullptr};
  PyObject *obj = nullptr;

  if (PyArg_ParseTupleAndKeywords(args, kwds, "|O!", (char **)kwlist_1, &ViewShape_Type, &obj)) {
    if (!obj) {
      self->vs = new ViewShape();
      self->py_ss = nullptr;
    }
    else {
      self->vs = new ViewShape(*(((BPy_ViewShape *)obj)->vs));
      self->py_ss = ((BPy_ViewShape *)obj)->py_ss;
    }
  }
  else if ((void)PyErr_Clear(),
           PyArg_ParseTupleAndKeywords(args, kwds, "O!", (char **)kwlist_2, &SShape_Type, &obj)) {
    BPy_SShape *py_ss = (BPy_SShape *)obj;
    self->vs = new ViewShape(py_ss->ss);
    self->py_ss = (!py_ss->borrowed) ? py_ss : nullptr;
  }
  else {
    PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
    return -1;
  }
  self->borrowed = false;
  Py_XINCREF(self->py_ss);
  return 0;
}

static void ViewShape_dealloc(BPy_ViewShape *self)
{
  if (self->py_ss) {
    self->vs->setSShape((SShape *)nullptr);
    Py_DECREF(self->py_ss);
  }
  if (self->vs && !self->borrowed) {
    delete self->vs;
  }
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *ViewShape_repr(BPy_ViewShape *self)
{
  return PyUnicode_FromFormat("ViewShape - address: %p", self->vs);
}

PyDoc_STRVAR(ViewShape_add_edge_doc,
             ".. method:: add_edge(edge)\n"
             "\n"
             "   Adds a ViewEdge to the list of ViewEdge objects.\n"
             "\n"
             "   :arg edge: A ViewEdge object.\n"
             "   :type edge: :class:`ViewEdge`\n");

static PyObject *ViewShape_add_edge(BPy_ViewShape *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"edge", nullptr};
  PyObject *py_ve = nullptr;

  if (PyArg_ParseTupleAndKeywords(args, kwds, "O!", (char **)kwlist, &ViewEdge_Type, &py_ve)) {
    return nullptr;
  }
  self->vs->AddEdge(((BPy_ViewEdge *)py_ve)->ve);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(ViewShape_add_vertex_doc,
             ".. method:: add_vertex(vertex)\n"
             "\n"
             "   Adds a ViewVertex to the list of the ViewVertex objects.\n"
             "\n"
             "   :arg vertex: A ViewVertex object.\n"
             "   :type vertex: :class:`ViewVertex`");

static PyObject *ViewShape_add_vertex(BPy_ViewShape *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"vertex", nullptr};
  PyObject *py_vv = nullptr;

  if (PyArg_ParseTupleAndKeywords(args, kwds, "O!", (char **)kwlist, &ViewVertex_Type, &py_vv)) {
    return nullptr;
  }
  self->vs->AddVertex(((BPy_ViewVertex *)py_vv)->vv);
  Py_RETURN_NONE;
}

// virtual ViewShape *duplicate()

static PyMethodDef BPy_ViewShape_methods[] = {
    {"add_edge",
     (PyCFunction)ViewShape_add_edge,
     METH_VARARGS | METH_KEYWORDS,
     ViewShape_add_edge_doc},
    {"add_vertex",
     (PyCFunction)ViewShape_add_vertex,
     METH_VARARGS | METH_KEYWORDS,
     ViewShape_add_vertex_doc},
    {nullptr, nullptr, 0, nullptr},
};

/*----------------------ViewShape get/setters ----------------------------*/

PyDoc_STRVAR(ViewShape_sshape_doc,
             "The SShape on top of which this ViewShape is built.\n"
             "\n"
             ":type: :class:`SShape`");

static PyObject *ViewShape_sshape_get(BPy_ViewShape *self, void *UNUSED(closure))
{
  SShape *ss = self->vs->sshape();
  if (!ss) {
    Py_RETURN_NONE;
  }
  return BPy_SShape_from_SShape(*ss);
}

static int ViewShape_sshape_set(BPy_ViewShape *self, PyObject *value, void *UNUSED(closure))
{
  if (!BPy_SShape_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "value must be an SShape");
    return -1;
  }
  BPy_SShape *py_ss = (BPy_SShape *)value;
  self->vs->setSShape(py_ss->ss);
  if (self->py_ss) {
    Py_DECREF(self->py_ss);
  }
  if (!py_ss->borrowed) {
    self->py_ss = py_ss;
    Py_INCREF(self->py_ss);
  }
  return 0;
}

PyDoc_STRVAR(ViewShape_vertices_doc,
             "The list of ViewVertex objects contained in this ViewShape.\n"
             "\n"
             ":type: List of :class:`ViewVertex` objects");

static PyObject *ViewShape_vertices_get(BPy_ViewShape *self, void *UNUSED(closure))
{
  vector<ViewVertex *> vertices = self->vs->vertices();
  vector<ViewVertex *>::iterator it;
  PyObject *py_vertices = PyList_New(vertices.size());
  unsigned int i = 0;

  for (it = vertices.begin(); it != vertices.end(); it++) {
    PyList_SET_ITEM(py_vertices, i++, Any_BPy_ViewVertex_from_ViewVertex(*(*it)));
  }
  return py_vertices;
}

static int ViewShape_vertices_set(BPy_ViewShape *self, PyObject *value, void *UNUSED(closure))
{
  PyObject *item;
  vector<ViewVertex *> v;

  if (!PyList_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "value must be a list of ViewVertex objects");
    return -1;
  }

  v.reserve(PyList_GET_SIZE(value));
  for (unsigned int i = 0; i < PyList_GET_SIZE(value); i++) {
    item = PyList_GET_ITEM(value, i);
    if (BPy_ViewVertex_Check(item)) {
      v.push_back(((BPy_ViewVertex *)item)->vv);
    }
    else {
      PyErr_SetString(PyExc_TypeError, "value must be a list of ViewVertex objects");
      return -1;
    }
  }
  self->vs->setVertices(v);
  return 0;
}

PyDoc_STRVAR(ViewShape_edges_doc,
             "The list of ViewEdge objects contained in this ViewShape.\n"
             "\n"
             ":type: List of :class:`ViewEdge` objects");

static PyObject *ViewShape_edges_get(BPy_ViewShape *self, void *UNUSED(closure))
{
  vector<ViewEdge *> edges = self->vs->edges();
  vector<ViewEdge *>::iterator it;
  PyObject *py_edges = PyList_New(edges.size());
  unsigned int i = 0;

  for (it = edges.begin(); it != edges.end(); it++) {
    PyList_SET_ITEM(py_edges, i++, BPy_ViewEdge_from_ViewEdge(*(*it)));
  }
  return py_edges;
}

static int ViewShape_edges_set(BPy_ViewShape *self, PyObject *value, void *UNUSED(closure))
{
  PyObject *item;
  vector<ViewEdge *> v;

  if (!PyList_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "value must be a list of ViewEdge objects");
    return -1;
  }

  v.reserve(PyList_GET_SIZE(value));
  for (int i = 0; i < PyList_GET_SIZE(value); i++) {
    item = PyList_GET_ITEM(value, i);
    if (BPy_ViewEdge_Check(item)) {
      v.push_back(((BPy_ViewEdge *)item)->ve);
    }
    else {
      PyErr_SetString(PyExc_TypeError, "argument must be list of ViewEdge objects");
      return -1;
    }
  }
  self->vs->setEdges(v);
  return 0;
}

PyDoc_STRVAR(ViewShape_name_doc,
             "The name of the ViewShape.\n"
             "\n"
             ":type: str");

static PyObject *ViewShape_name_get(BPy_ViewShape *self, void *UNUSED(closure))
{
  return PyUnicode_FromString(self->vs->getName().c_str());
}

PyDoc_STRVAR(ViewShape_library_path_doc,
             "The library path of the ViewShape.\n"
             "\n"
             ":type: str, or None if the ViewShape is not part of a library");

static PyObject *ViewShape_library_path_get(BPy_ViewShape *self, void *UNUSED(closure))
{
  return PyUnicode_FromString(self->vs->getLibraryPath().c_str());
}

PyDoc_STRVAR(ViewShape_id_doc,
             "The Id of this ViewShape.\n"
             "\n"
             ":type: :class:`Id`");

static PyObject *ViewShape_id_get(BPy_ViewShape *self, void *UNUSED(closure))
{
  Id id(self->vs->getId());
  return BPy_Id_from_Id(id);  // return a copy
}

static PyGetSetDef BPy_ViewShape_getseters[] = {
    {"sshape",
     (getter)ViewShape_sshape_get,
     (setter)ViewShape_sshape_set,
     ViewShape_sshape_doc,
     nullptr},
    {"vertices",
     (getter)ViewShape_vertices_get,
     (setter)ViewShape_vertices_set,
     ViewShape_vertices_doc,
     nullptr},
    {"edges",
     (getter)ViewShape_edges_get,
     (setter)ViewShape_edges_set,
     ViewShape_edges_doc,
     nullptr},
    {"name", (getter)ViewShape_name_get, (setter) nullptr, ViewShape_name_doc, nullptr},
    {"library_path",
     (getter)ViewShape_library_path_get,
     (setter) nullptr,
     ViewShape_library_path_doc,
     nullptr},
    {"id", (getter)ViewShape_id_get, (setter) nullptr, ViewShape_id_doc, nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/*-----------------------BPy_ViewShape type definition ------------------------------*/

PyTypeObject ViewShape_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0) "ViewShape", /* tp_name */
    sizeof(BPy_ViewShape),                         /* tp_basicsize */
    0,                                             /* tp_itemsize */
    (destructor)ViewShape_dealloc,                 /* tp_dealloc */
#if PY_VERSION_HEX >= 0x03080000
    0, /* tp_vectorcall_offset */
#else
    nullptr, /* tp_print */
#endif
    nullptr,                                  /* tp_getattr */
    nullptr,                                  /* tp_setattr */
    nullptr,                                  /* tp_reserved */
    (reprfunc)ViewShape_repr,                 /* tp_repr */
    nullptr,                                  /* tp_as_number */
    nullptr,                                  /* tp_as_sequence */
    nullptr,                                  /* tp_as_mapping */
    nullptr,                                  /* tp_hash  */
    nullptr,                                  /* tp_call */
    nullptr,                                  /* tp_str */
    nullptr,                                  /* tp_getattro */
    nullptr,                                  /* tp_setattro */
    nullptr,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    ViewShape_doc,                            /* tp_doc */
    nullptr,                                  /* tp_traverse */
    nullptr,                                  /* tp_clear */
    nullptr,                                  /* tp_richcompare */
    0,                                        /* tp_weaklistoffset */
    nullptr,                                  /* tp_iter */
    nullptr,                                  /* tp_iternext */
    BPy_ViewShape_methods,                    /* tp_methods */
    nullptr,                                  /* tp_members */
    BPy_ViewShape_getseters,                  /* tp_getset */
    nullptr,                                  /* tp_base */
    nullptr,                                  /* tp_dict */
    nullptr,                                  /* tp_descr_get */
    nullptr,                                  /* tp_descr_set */
    0,                                        /* tp_dictoffset */
    (initproc)ViewShape_init,                 /* tp_init */
    nullptr,                                  /* tp_alloc */
    PyType_GenericNew,                        /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
