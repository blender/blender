/* SPDX-FileCopyrightText: 2004-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_CurvePointIterator.h"

#include "../BPy_Convert.h"
#include "BPy_Interface0DIterator.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(CurvePointIterator_doc,
             "Class hierarchy: :class:`Iterator` > :class:`CurvePointIterator`\n"
             "\n"
             "Class representing an iterator on a curve. Allows an iterating\n"
             "outside initial vertices. A CurvePoint is instantiated and returned\n"
             "through the .object attribute.\n"
             "\n"
             ".. method:: __init__()\n"
             "            __init__(brother)\n"
             "            __init__(step=0.0)\n"
             "\n"
             "   Builds a CurvePointIterator object using either the default constructor,\n"
             "   copy constructor, or the overloaded constructor.\n"
             "\n"
             "   :arg brother: A CurvePointIterator object.\n"
             "   :type brother: :class:`CurvePointIterator`\n"
             "   :arg step: A resampling resolution with which the curve is resampled.\n"
             "      If zero, no resampling is done (i.e., the iterator iterates over\n"
             "      initial vertices).\n"
             "   :type step: float");

static int CurvePointIterator_init(BPy_CurvePointIterator *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist_1[] = {"brother", nullptr};
  static const char *kwlist_2[] = {"step", nullptr};
  PyObject *brother = nullptr;
  float step;

  if (PyArg_ParseTupleAndKeywords(
          args, kwds, "|O!", (char **)kwlist_1, &CurvePointIterator_Type, &brother))
  {
    if (!brother) {
      self->cp_it = new CurveInternal::CurvePointIterator();
    }
    else {
      self->cp_it = new CurveInternal::CurvePointIterator(
          *(((BPy_CurvePointIterator *)brother)->cp_it));
    }
  }
  else if ((void)PyErr_Clear(),
           PyArg_ParseTupleAndKeywords(args, kwds, "f", (char **)kwlist_2, &step))
  {
    self->cp_it = new CurveInternal::CurvePointIterator(step);
  }
  else {
    PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
    return -1;
  }
  self->py_it.it = self->cp_it;
  return 0;
}

/*----------------------CurvePointIterator get/setters ----------------------------*/

PyDoc_STRVAR(CurvePointIterator_object_doc,
             "The CurvePoint object currently pointed by this iterator.\n"
             "\n"
             ":type: :class:`CurvePoint`");

static PyObject *CurvePointIterator_object_get(BPy_CurvePointIterator *self, void * /*closure*/)
{
  if (self->cp_it->isEnd()) {
    PyErr_SetString(PyExc_RuntimeError, "iteration has stopped");
    return nullptr;
  }
  return BPy_CurvePoint_from_CurvePoint(self->cp_it->operator*());
}

PyDoc_STRVAR(CurvePointIterator_t_doc,
             "The curvilinear abscissa of the current point.\n"
             "\n"
             ":type: float");

static PyObject *CurvePointIterator_t_get(BPy_CurvePointIterator *self, void * /*closure*/)
{
  return PyFloat_FromDouble(self->cp_it->t());
}

PyDoc_STRVAR(CurvePointIterator_u_doc,
             "The point parameter at the current point in the stroke (0 <= u <= 1).\n"
             "\n"
             ":type: float");

static PyObject *CurvePointIterator_u_get(BPy_CurvePointIterator *self, void * /*closure*/)
{
  return PyFloat_FromDouble(self->cp_it->u());
}

static PyGetSetDef BPy_CurvePointIterator_getseters[] = {
    {"object",
     (getter)CurvePointIterator_object_get,
     (setter) nullptr,
     CurvePointIterator_object_doc,
     nullptr},
    {"t", (getter)CurvePointIterator_t_get, (setter) nullptr, CurvePointIterator_t_doc, nullptr},
    {"u", (getter)CurvePointIterator_u_get, (setter) nullptr, CurvePointIterator_u_doc, nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/*-----------------------BPy_CurvePointIterator type definition ------------------------------*/

PyTypeObject CurvePointIterator_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "CurvePointIterator",
    /*tp_basicsize*/ sizeof(BPy_CurvePointIterator),
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
    /*tp_doc*/ CurvePointIterator_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ nullptr,
    /*tp_members*/ nullptr,
    /*tp_getset*/ BPy_CurvePointIterator_getseters,
    /*tp_base*/ &Iterator_Type,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)CurvePointIterator_init,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
