/* SPDX-FileCopyrightText: 2004-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_Interface0DIterator.h"

#include "../BPy_Convert.h"
#include "../BPy_Interface1D.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(
    /* Wrap. */
    Interface0DIterator_doc,
    "Class hierarchy: :class:`Iterator` > :class:`Interface0DIterator`\n"
    "\n"
    "Class defining an iterator over Interface0D elements. An instance of\n"
    "this iterator is always obtained from a 1D element.\n"
    "\n"
    ".. method:: __init__(brother)\n"
    "            __init__(it)\n"
    "\n"
    "   Construct a nested Interface0DIterator using either the copy constructor\n"
    "   or the constructor that takes an he argument of a Function0D.\n"
    "\n"
    "   :arg brother: An Interface0DIterator object.\n"
    "   :type brother: :class:`Interface0DIterator`\n"
    "   :arg it: An iterator object to be nested.\n"
    "   :type it: :class:`SVertexIterator`, :class:`CurvePointIterator`, or\n"
    "      :class:`StrokeVertexIterator`");

static int convert_nested_it(PyObject *obj, void *v)
{
  if (!obj || !BPy_Iterator_Check(obj)) {
    return 0;
  }
  Interface0DIteratorNested *nested_it = dynamic_cast<Interface0DIteratorNested *>(
      ((BPy_Iterator *)obj)->it);
  if (!nested_it) {
    return 0;
  }
  *((Interface0DIteratorNested **)v) = nested_it;
  return 1;
}

static int Interface0DIterator_init(BPy_Interface0DIterator *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist_1[] = {"it", nullptr};
  static const char *kwlist_2[] = {"inter", nullptr};
  static const char *kwlist_3[] = {"brother", nullptr};
  Interface0DIteratorNested *nested_it;
  PyObject *brother, *inter;

  if (PyArg_ParseTupleAndKeywords(
          args, kwds, "O&", (char **)kwlist_1, convert_nested_it, &nested_it))
  {
    self->if0D_it = new Interface0DIterator(nested_it->copy());
    self->at_start = true;
    self->reversed = false;
  }
  else if ((void)PyErr_Clear(),
           PyArg_ParseTupleAndKeywords(
               args, kwds, "O!", (char **)kwlist_2, &Interface1D_Type, &inter))
  {
    self->if0D_it = new Interface0DIterator(((BPy_Interface1D *)inter)->if1D->verticesBegin());
    self->at_start = true;
    self->reversed = false;
  }
  else if ((void)PyErr_Clear(),
           PyArg_ParseTupleAndKeywords(
               args, kwds, "O!", (char **)kwlist_3, &Interface0DIterator_Type, &brother))
  {
    self->if0D_it = new Interface0DIterator(*(((BPy_Interface0DIterator *)brother)->if0D_it));
    self->at_start = ((BPy_Interface0DIterator *)brother)->at_start;
    self->reversed = ((BPy_Interface0DIterator *)brother)->reversed;
  }
  else {
    PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
    return -1;
  }
  self->py_it.it = self->if0D_it;
  return 0;
}

static PyObject *Interface0DIterator_iter(BPy_Interface0DIterator *self)
{
  Py_INCREF(self);
  self->at_start = true;
  return (PyObject *)self;
}

static PyObject *Interface0DIterator_iternext(BPy_Interface0DIterator *self)
{
  if (self->reversed) {
    if (self->if0D_it->isBegin()) {
      PyErr_SetNone(PyExc_StopIteration);
      return nullptr;
    }
    self->if0D_it->decrement();
  }
  else {
    if (self->if0D_it->isEnd()) {
      PyErr_SetNone(PyExc_StopIteration);
      return nullptr;
    }
    if (self->at_start) {
      self->at_start = false;
    }
    else if (self->if0D_it->atLast()) {
      PyErr_SetNone(PyExc_StopIteration);
      return nullptr;
    }
    else {
      self->if0D_it->increment();
    }
  }
  Interface0D *if0D = self->if0D_it->operator->();
  return Any_BPy_Interface0D_from_Interface0D(*if0D);
}

/*----------------------Interface0DIterator get/setters ----------------------------*/

PyDoc_STRVAR(
    /* Wrap. */
    Interface0DIterator_object_doc,
    "The 0D object currently pointed to by this iterator. Note that the object\n"
    "may be an instance of an Interface0D subclass. For example if the iterator\n"
    "has been created from the `vertices_begin()` method of the :class:`Stroke`\n"
    "class, the .object property refers to a :class:`StrokeVertex` object.\n"
    "\n"
    ":type: :class:`Interface0D` or one of its subclasses.");

static PyObject *Interface0DIterator_object_get(BPy_Interface0DIterator *self, void * /*closure*/)
{
  if (self->if0D_it->isEnd()) {
    PyErr_SetString(PyExc_RuntimeError, "iteration has stopped");
    return nullptr;
  }
  return Any_BPy_Interface0D_from_Interface0D(self->if0D_it->operator*());
}

PyDoc_STRVAR(
    /* Wrap. */
    Interface0DIterator_t_doc,
    "The curvilinear abscissa of the current point.\n"
    "\n"
    ":type: float");

static PyObject *Interface0DIterator_t_get(BPy_Interface0DIterator *self, void * /*closure*/)
{
  return PyFloat_FromDouble(self->if0D_it->t());
}

PyDoc_STRVAR(
    /* Wrap. */
    Interface0DIterator_u_doc,
    "The point parameter at the current point in the 1D element (0 <= u <= 1).\n"
    "\n"
    ":type: float");

static PyObject *Interface0DIterator_u_get(BPy_Interface0DIterator *self, void * /*closure*/)
{
  return PyFloat_FromDouble(self->if0D_it->u());
}

PyDoc_STRVAR(
    /* Wrap. */
    Interface0DIterator_at_last_doc,
    "True if the iterator points to the last valid element.\n"
    "For its counterpart (pointing to the first valid element), use it.is_begin.\n"
    "\n"
    ":type: bool");

static PyObject *Interface0DIterator_at_last_get(BPy_Interface0DIterator *self, void * /*closure*/)
{
  return PyBool_from_bool(self->if0D_it->atLast());
}

static PyGetSetDef BPy_Interface0DIterator_getseters[] = {
    {"object",
     (getter)Interface0DIterator_object_get,
     (setter) nullptr,
     Interface0DIterator_object_doc,
     nullptr},
    {"t", (getter)Interface0DIterator_t_get, (setter) nullptr, Interface0DIterator_t_doc, nullptr},
    {"u", (getter)Interface0DIterator_u_get, (setter) nullptr, Interface0DIterator_u_doc, nullptr},
    {"at_last",
     (getter)Interface0DIterator_at_last_get,
     (setter) nullptr,
     Interface0DIterator_at_last_doc,
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/*-----------------------BPy_Interface0DIterator type definition ------------------------------*/

PyTypeObject Interface0DIterator_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "Interface0DIterator",
    /*tp_basicsize*/ sizeof(BPy_Interface0DIterator),
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
    /*tp_doc*/ Interface0DIterator_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ (getiterfunc)Interface0DIterator_iter,
    /*tp_iternext*/ (iternextfunc)Interface0DIterator_iternext,
    /*tp_methods*/ nullptr,
    /*tp_members*/ nullptr,
    /*tp_getset*/ BPy_Interface0DIterator_getseters,
    /*tp_base*/ &Iterator_Type,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)Interface0DIterator_init,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
