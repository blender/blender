/* SPDX-FileCopyrightText: 2004-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_Chain.h"

#include "../../BPy_Convert.h"
#include "../../BPy_Id.h"
#include "../BPy_ViewEdge.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

/*----------------------Chain methods ----------------------------*/

PyDoc_STRVAR(Chain_doc,
             "Class hierarchy: :class:`Interface1D` > :class:`Curve` > :class:`Chain`\n"
             "\n"
             "Class to represent a 1D elements issued from the chaining process. A\n"
             "Chain is the last step before the :class:`Stroke` and is used in the\n"
             "Splitting and Creation processes.\n"
             "\n"
             ".. method:: __init__()\n"
             "            __init__(brother)\n"
             "            __init__(id)\n"
             "\n"
             "   Builds a :class:`Chain` using the default constructor,\n"
             "   copy constructor or from an :class:`Id`.\n"
             "\n"
             "   :arg brother: A Chain object.\n"
             "   :type brother: :class:`Chain`\n"
             "   :arg id: An Id object.\n"
             "   :type id: :class:`Id`");

static int Chain_init(BPy_Chain *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist_1[] = {"brother", nullptr};
  static const char *kwlist_2[] = {"id", nullptr};
  PyObject *obj = nullptr;

  if (PyArg_ParseTupleAndKeywords(args, kwds, "|O!", (char **)kwlist_1, &Chain_Type, &obj)) {
    if (!obj) {
      self->c = new Chain();
    }
    else {
      self->c = new Chain(*(((BPy_Chain *)obj)->c));
    }
  }
  else if ((void)PyErr_Clear(),
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
  static const char *kwlist[] = {"viewedge", "orientation", nullptr};
  PyObject *obj1 = nullptr, *obj2 = nullptr;

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "O!O!", (char **)kwlist, &ViewEdge_Type, &obj1, &PyBool_Type, &obj2))
  {
    return nullptr;
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
  static const char *kwlist[] = {"viewedge", "orientation", nullptr};
  PyObject *obj1 = nullptr, *obj2 = nullptr;

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "O!O!", (char **)kwlist, &ViewEdge_Type, &obj1, &PyBool_Type, &obj2))
  {
    return nullptr;
  }
  ViewEdge *ve = ((BPy_ViewEdge *)obj1)->ve;
  bool orientation = bool_from_PyBool(obj2);
  self->c->push_viewedge_front(ve, orientation);
  Py_RETURN_NONE;
}

static PyMethodDef BPy_Chain_methods[] = {
    {"push_viewedge_back",
     (PyCFunction)Chain_push_viewedge_back,
     METH_VARARGS | METH_KEYWORDS,
     Chain_push_viewedge_back_doc},
    {"push_viewedge_front",
     (PyCFunction)Chain_push_viewedge_front,
     METH_VARARGS | METH_KEYWORDS,
     Chain_push_viewedge_front_doc},
    {nullptr, nullptr, 0, nullptr},
};

/*-----------------------BPy_Chain type definition ------------------------------*/

PyTypeObject Chain_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "Chain",
    /*tp_basicsize*/ sizeof(BPy_Chain),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ nullptr,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ nullptr,
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
    /*tp_doc*/ Chain_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ BPy_Chain_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr,
    /*tp_base*/ &FrsCurve_Type,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)Chain_init,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
