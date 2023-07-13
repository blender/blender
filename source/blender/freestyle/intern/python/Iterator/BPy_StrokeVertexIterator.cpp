/* SPDX-FileCopyrightText: 2004-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_StrokeVertexIterator.h"

#include "../BPy_Convert.h"
#include "../Interface1D/BPy_Stroke.h"
#include "BPy_Interface0DIterator.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(StrokeVertexIterator_doc,
             "Class hierarchy: :class:`Iterator` > :class:`StrokeVertexIterator`\n"
             "\n"
             "Class defining an iterator designed to iterate over the\n"
             ":class:`StrokeVertex` of a :class:`Stroke`. An instance of a\n"
             "StrokeVertexIterator can be obtained from a Stroke by calling\n"
             "iter(), stroke_vertices_begin() or stroke_vertices_begin(). It is iterating\n"
             "over the same vertices as an :class:`Interface0DIterator`. The difference\n"
             "resides in the object access: an Interface0DIterator only allows\n"
             "access to an Interface0D while one might need to access the\n"
             "specialized StrokeVertex type. In this case, one should use a\n"
             "StrokeVertexIterator. To call functions of the UnaryFuntion0D type,\n"
             "a StrokeVertexIterator can be converted to an Interface0DIterator by\n"
             "by calling Interface0DIterator(it).\n"
             "\n"
             ".. method:: __init__()\n"
             "            __init__(brother)\n"
             "\n"
             "   Creates a :class:`StrokeVertexIterator` using either the\n"
             "   default constructor or the copy constructor.\n"
             "\n"
             "   :arg brother: A StrokeVertexIterator object.\n"
             "   :type brother: :class:`StrokeVertexIterator`");

static int StrokeVertexIterator_init(BPy_StrokeVertexIterator *self,
                                     PyObject *args,
                                     PyObject *kwds)
{
  static const char *kwlist_1[] = {"brother", nullptr};
  static const char *kwlist_2[] = {"stroke", nullptr};
  PyObject *brother = nullptr, *stroke = nullptr;

  if (PyArg_ParseTupleAndKeywords(
          args, kwds, "O!", (char **)kwlist_1, &StrokeVertexIterator_Type, &brother))
  {
    self->sv_it = new StrokeInternal::StrokeVertexIterator(
        *(((BPy_StrokeVertexIterator *)brother)->sv_it));
    self->reversed = ((BPy_StrokeVertexIterator *)brother)->reversed;
    self->at_start = ((BPy_StrokeVertexIterator *)brother)->at_start;
  }

  else if ((void)PyErr_Clear(),
           PyArg_ParseTupleAndKeywords(
               args, kwds, "|O!", (char **)kwlist_2, &Stroke_Type, &stroke))
  {
    if (!stroke) {
      self->sv_it = new StrokeInternal::StrokeVertexIterator();
    }
    else {
      self->sv_it = new StrokeInternal::StrokeVertexIterator(
          ((BPy_Stroke *)stroke)->s->strokeVerticesBegin());
    }
    self->reversed = false;
    self->at_start = true;
  }
  else {
    PyErr_SetString(PyExc_TypeError, "argument 1 must be StrokeVertexIterator or Stroke");
    return -1;
  }
  self->py_it.it = self->sv_it;
  return 0;
}

static PyObject *StrokeVertexIterator_iter(BPy_StrokeVertexIterator *self)
{
  Py_INCREF(self);
  self->at_start = true;
  return (PyObject *)self;
}

static PyObject *StrokeVertexIterator_iternext(BPy_StrokeVertexIterator *self)
{
  /* Because Freestyle iterators for which it.isEnd() holds true have no valid object
   * (they point to the past-the-end element and can't be dereferenced), we have to check
   * iterators for validity.
   * Additionally, the at_start attribute is used to keep Freestyle iterator objects
   * and Python for loops in sync. */

  if (self->reversed) {
    if (self->sv_it->isBegin()) {
      PyErr_SetNone(PyExc_StopIteration);
      return nullptr;
    }
    self->sv_it->decrement();
  }
  else {
    /* If sv_it.isEnd() is true, the iterator can't be incremented. */
    if (self->sv_it->isEnd()) {
      PyErr_SetNone(PyExc_StopIteration);
      return nullptr;
    }
    /* If at the start of the iterator, only return the object
     * and don't increment, to keep for-loops in sync */
    if (self->at_start) {
      self->at_start = false;
    }
    /* If sv_it.atLast() is true, the iterator is currently pointing to the final valid element.
     * Incrementing it further would lead to a state that the iterator can't be dereferenced. */
    else if (self->sv_it->atLast()) {
      PyErr_SetNone(PyExc_StopIteration);
      return nullptr;
    }
    else {
      self->sv_it->increment();
    }
  }
  StrokeVertex *sv = self->sv_it->operator->();
  return BPy_StrokeVertex_from_StrokeVertex(*sv);
}

/*----------------------StrokeVertexIterator methods ----------------------------*/

PyDoc_STRVAR(StrokeVertexIterator_incremented_doc,
             ".. method:: incremented()\n"
             "\n"
             "   Returns a copy of an incremented StrokeVertexIterator.\n"
             "\n"
             "   :return: A StrokeVertexIterator pointing the next StrokeVertex.\n"
             "   :rtype: :class:`StrokeVertexIterator`");

static PyObject *StrokeVertexIterator_incremented(BPy_StrokeVertexIterator *self)
{
  if (self->sv_it->isEnd()) {
    PyErr_SetString(PyExc_RuntimeError, "cannot increment any more");
    return nullptr;
  }
  StrokeInternal::StrokeVertexIterator copy(*self->sv_it);
  copy.increment();
  return BPy_StrokeVertexIterator_from_StrokeVertexIterator(copy, self->reversed);
}

PyDoc_STRVAR(StrokeVertexIterator_decremented_doc,
             ".. method:: decremented()\n"
             "\n"
             "   Returns a copy of a decremented StrokeVertexIterator.\n"
             "\n"
             "   :return: A StrokeVertexIterator pointing the previous StrokeVertex.\n"
             "   :rtype: :class:`StrokeVertexIterator`");

static PyObject *StrokeVertexIterator_decremented(BPy_StrokeVertexIterator *self)
{
  if (self->sv_it->isBegin()) {
    PyErr_SetString(PyExc_RuntimeError, "cannot decrement any more");
    return nullptr;
  }
  StrokeInternal::StrokeVertexIterator copy(*self->sv_it);
  copy.decrement();
  return BPy_StrokeVertexIterator_from_StrokeVertexIterator(copy, self->reversed);
}

PyDoc_STRVAR(StrokeVertexIterator_reversed_doc,
             ".. method:: reversed()\n"
             "\n"
             "   Returns a StrokeVertexIterator that traverses stroke vertices in the\n"
             "   reversed order.\n"
             "\n"
             "   :return: A StrokeVertexIterator traversing stroke vertices backward.\n"
             "   :rtype: :class:`StrokeVertexIterator`");

static PyObject *StrokeVertexIterator_reversed(BPy_StrokeVertexIterator *self)
{
  return BPy_StrokeVertexIterator_from_StrokeVertexIterator(*self->sv_it, !self->reversed);
}

static PyMethodDef BPy_StrokeVertexIterator_methods[] = {
    {"incremented",
     (PyCFunction)StrokeVertexIterator_incremented,
     METH_NOARGS,
     StrokeVertexIterator_incremented_doc},
    {"decremented",
     (PyCFunction)StrokeVertexIterator_decremented,
     METH_NOARGS,
     StrokeVertexIterator_decremented_doc},
    {"reversed",
     (PyCFunction)StrokeVertexIterator_reversed,
     METH_NOARGS,
     StrokeVertexIterator_reversed_doc},
    {nullptr, nullptr, 0, nullptr},
};

/*----------------------StrokeVertexIterator get/setters ----------------------------*/

PyDoc_STRVAR(StrokeVertexIterator_object_doc,
             "The StrokeVertex object currently pointed to by this iterator.\n"
             "\n"
             ":type: :class:`StrokeVertex`");

static PyObject *StrokeVertexIterator_object_get(BPy_StrokeVertexIterator *self,
                                                 void * /*closure*/)
{
  if (self->sv_it->isEnd()) {
    PyErr_SetString(PyExc_RuntimeError, "iteration has stopped");
    return nullptr;
  }
  StrokeVertex *sv = self->sv_it->operator->();
  if (sv) {
    return BPy_StrokeVertex_from_StrokeVertex(*sv);
  }
  Py_RETURN_NONE;
}

PyDoc_STRVAR(StrokeVertexIterator_t_doc,
             "The curvilinear abscissa of the current point.\n"
             "\n"
             ":type: float");

static PyObject *StrokeVertexIterator_t_get(BPy_StrokeVertexIterator *self, void * /*closure*/)
{
  return PyFloat_FromDouble(self->sv_it->t());
}

PyDoc_STRVAR(StrokeVertexIterator_u_doc,
             "The point parameter at the current point in the stroke (0 <= u <= 1).\n"
             "\n"
             ":type: float");

static PyObject *StrokeVertexIterator_u_get(BPy_StrokeVertexIterator *self, void * /*closure*/)
{
  return PyFloat_FromDouble(self->sv_it->u());
}

PyDoc_STRVAR(StrokeVertexIterator_at_last_doc,
             "True if the iterator points to the last valid element.\n"
             "For its counterpart (pointing to the first valid element), use it.is_begin.\n"
             "\n"
             ":type: bool");

static PyObject *StrokeVertexIterator_at_last_get(BPy_StrokeVertexIterator *self)
{
  return PyBool_from_bool(self->sv_it->atLast());
}

static PyGetSetDef BPy_StrokeVertexIterator_getseters[] = {
    {"object",
     (getter)StrokeVertexIterator_object_get,
     (setter) nullptr,
     StrokeVertexIterator_object_doc,
     nullptr},
    {"t",
     (getter)StrokeVertexIterator_t_get,
     (setter) nullptr,
     StrokeVertexIterator_t_doc,
     nullptr},
    {"u",
     (getter)StrokeVertexIterator_u_get,
     (setter) nullptr,
     StrokeVertexIterator_u_doc,
     nullptr},
    {"at_last",
     (getter)StrokeVertexIterator_at_last_get,
     (setter) nullptr,
     StrokeVertexIterator_at_last_doc,
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/*-----------------------BPy_StrokeVertexIterator type definition ------------------------------*/

PyTypeObject StrokeVertexIterator_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "StrokeVertexIterator",
    /*tp_basicsize*/ sizeof(BPy_StrokeVertexIterator),
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
    /*tp_doc*/ StrokeVertexIterator_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ (getiterfunc)StrokeVertexIterator_iter,
    /*tp_iternext*/ (iternextfunc)StrokeVertexIterator_iternext,
    /*tp_methods*/ BPy_StrokeVertexIterator_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ BPy_StrokeVertexIterator_getseters,
    /*tp_base*/ &Iterator_Type,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)StrokeVertexIterator_init,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
