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

#include "BPy_CurvePointIterator.h"

#include "../BPy_Convert.h"
#include "BPy_Interface0DIterator.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(CurvePointIterator_doc,
             "Class hierarchy: :class:`Iterator` > :class:`CurvePointIterator`\n"
             "\n"
             "Class representing an iterator on a curve.  Allows an iterating\n"
             "outside initial vertices.  A CurvePoint is instantiated and returned\n"
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
          args, kwds, "|O!", (char **)kwlist_1, &CurvePointIterator_Type, &brother)) {
    if (!brother) {
      self->cp_it = new CurveInternal::CurvePointIterator();
    }
    else {
      self->cp_it = new CurveInternal::CurvePointIterator(
          *(((BPy_CurvePointIterator *)brother)->cp_it));
    }
  }
  else if ((void)PyErr_Clear(),
           PyArg_ParseTupleAndKeywords(args, kwds, "f", (char **)kwlist_2, &step)) {
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

static PyObject *CurvePointIterator_object_get(BPy_CurvePointIterator *self, void *UNUSED(closure))
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

static PyObject *CurvePointIterator_t_get(BPy_CurvePointIterator *self, void *UNUSED(closure))
{
  return PyFloat_FromDouble(self->cp_it->t());
}

PyDoc_STRVAR(CurvePointIterator_u_doc,
             "The point parameter at the current point in the stroke (0 <= u <= 1).\n"
             "\n"
             ":type: float");

static PyObject *CurvePointIterator_u_get(BPy_CurvePointIterator *self, void *UNUSED(closure))
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
    PyVarObject_HEAD_INIT(nullptr, 0) "CurvePointIterator", /* tp_name */
    sizeof(BPy_CurvePointIterator),                         /* tp_basicsize */
    0,                                                      /* tp_itemsize */
    nullptr,                                                /* tp_dealloc */
#if PY_VERSION_HEX >= 0x03080000
    0, /* tp_vectorcall_offset */
#else
    nullptr, /* tp_print */
#endif
    nullptr,                                  /* tp_getattr */
    nullptr,                                  /* tp_setattr */
    nullptr,                                  /* tp_reserved */
    nullptr,                                  /* tp_repr */
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
    CurvePointIterator_doc,                   /* tp_doc */
    nullptr,                                  /* tp_traverse */
    nullptr,                                  /* tp_clear */
    nullptr,                                  /* tp_richcompare */
    0,                                        /* tp_weaklistoffset */
    nullptr,                                  /* tp_iter */
    nullptr,                                  /* tp_iternext */
    nullptr,                                  /* tp_methods */
    nullptr,                                  /* tp_members */
    BPy_CurvePointIterator_getseters,         /* tp_getset */
    &Iterator_Type,                           /* tp_base */
    nullptr,                                  /* tp_dict */
    nullptr,                                  /* tp_descr_get */
    nullptr,                                  /* tp_descr_set */
    0,                                        /* tp_dictoffset */
    (initproc)CurvePointIterator_init,        /* tp_init */
    nullptr,                                  /* tp_alloc */
    nullptr,                                  /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
