/* SPDX-FileCopyrightText: 2004-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_ChainPredicateIterator.h"

#include "../BPy_BinaryPredicate1D.h"
#include "../BPy_Convert.h"
#include "../BPy_UnaryPredicate1D.h"
#include "../Interface1D/BPy_ViewEdge.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(
    /* Wrap. */
    ChainPredicateIterator_doc,

    "Class hierarchy: :class:`freestyle.types.Iterator` >\n"
    ":class:`freestyle.types.ViewEdgeIterator` >\n"
    ":class:`freestyle.types.ChainingIterator` >\n"
    ":class:`ChainPredicateIterator`\n"
    "\n"
    "A \"generic\" user-controlled ViewEdge iterator. This iterator is in\n"
    "particular built from a unary predicate and a binary predicate.\n"
    "First, the unary predicate is evaluated for all potential next\n"
    "ViewEdges in order to only keep the ones respecting a certain\n"
    "constraint. Then, the binary predicate is evaluated on the current\n"
    "ViewEdge together with each ViewEdge of the previous selection. The\n"
    "first ViewEdge respecting both the unary predicate and the binary\n"
    "predicate is kept as the next one. If none of the potential next\n"
    "ViewEdge respects these two predicates, None is returned.\n"
    "\n"
    ".. method:: __init__(upred, bpred, restrict_to_selection=True, "
    "                     restrict_to_unvisited=True, begin=None, "
    "                     orientation=True)\n"
    "            __init__(brother)\n"
    "\n"
    "   Builds a ChainPredicateIterator from a unary predicate, a binary\n"
    "   predicate, a starting ViewEdge and its orientation or using the copy constructor.\n"
    "\n"
    "   :arg upred: The unary predicate that the next ViewEdge must satisfy.\n"
    "   :type upred: :class:`freestyle.types.UnaryPredicate1D`\n"
    "   :arg bpred: The binary predicate that the next ViewEdge must\n"
    "      satisfy together with the actual pointed ViewEdge.\n"
    "   :type bpred: :class:`freestyle.types.BinaryPredicate1D`\n"
    "   :arg restrict_to_selection: Indicates whether to force the chaining\n"
    "      to stay within the set of selected ViewEdges or not.\n"
    "   :type restrict_to_selection: bool\n"
    "   :arg restrict_to_unvisited: Indicates whether a ViewEdge that has\n"
    "      already been chained must be ignored ot not.\n"
    "   :type restrict_to_unvisited: bool\n"
    "   :arg begin: The ViewEdge from where to start the iteration.\n"
    "   :type begin: :class:`freestyle.types.ViewEdge` or None\n"
    "   :arg orientation: If true, we'll look for the next ViewEdge among\n"
    "      the ViewEdges that surround the ending ViewVertex of begin. If\n"
    "      false, we'll search over the ViewEdges surrounding the ending\n"
    "      ViewVertex of begin.\n"
    "   :type orientation: bool\n"
    "   :arg brother: A ChainPredicateIterator object.\n"
    "   :type brother: :class:`ChainPredicateIterator`");

static int check_begin(PyObject *obj, void *v)
{
  if (obj != nullptr && obj != Py_None && !BPy_ViewEdge_Check(obj)) {
    return 0;
  }
  *((PyObject **)v) = obj;
  return 1;
}

static int ChainPredicateIterator_init(BPy_ChainPredicateIterator *self,
                                       PyObject *args,
                                       PyObject *kwds)
{
  static const char *kwlist_1[] = {"brother", nullptr};
  static const char *kwlist_2[] = {"upred",
                                   "bpred",
                                   "restrict_to_selection",
                                   "restrict_to_unvisited",
                                   "begin",
                                   "orientation",
                                   nullptr};
  PyObject *obj1 = nullptr, *obj2 = nullptr, *obj3 = nullptr, *obj4 = nullptr, *obj5 = nullptr,
           *obj6 = nullptr;

  if (PyArg_ParseTupleAndKeywords(
          args, kwds, "O!", (char **)kwlist_1, &ChainPredicateIterator_Type, &obj1))
  {
    self->cp_it = new ChainPredicateIterator(*(((BPy_ChainPredicateIterator *)obj1)->cp_it));
    self->upred = ((BPy_ChainPredicateIterator *)obj1)->upred;
    self->bpred = ((BPy_ChainPredicateIterator *)obj1)->bpred;
    Py_INCREF(self->upred);
    Py_INCREF(self->bpred);
  }
  else if ((void)PyErr_Clear(),
           (void)(obj3 = obj4 = obj5 = obj6 = nullptr),
           PyArg_ParseTupleAndKeywords(args,
                                       kwds,
                                       "O!O!|O!O!O&O!",
                                       (char **)kwlist_2,
                                       &UnaryPredicate1D_Type,
                                       &obj1,
                                       &BinaryPredicate1D_Type,
                                       &obj2,
                                       &PyBool_Type,
                                       &obj3,
                                       &PyBool_Type,
                                       &obj4,
                                       check_begin,
                                       &obj5,
                                       &PyBool_Type,
                                       &obj6))
  {
    UnaryPredicate1D *up1D = ((BPy_UnaryPredicate1D *)obj1)->up1D;
    BinaryPredicate1D *bp1D = ((BPy_BinaryPredicate1D *)obj2)->bp1D;
    bool restrict_to_selection = (!obj3) ? true : bool_from_PyBool(obj3);
    bool restrict_to_unvisited = (!obj4) ? true : bool_from_PyBool(obj4);
    ViewEdge *begin = (!obj5 || obj5 == Py_None) ? nullptr : ((BPy_ViewEdge *)obj5)->ve;
    bool orientation = (!obj6) ? true : bool_from_PyBool(obj6);
    self->cp_it = new ChainPredicateIterator(
        *up1D, *bp1D, restrict_to_selection, restrict_to_unvisited, begin, orientation);
    self->upred = obj1;
    self->bpred = obj2;
    Py_INCREF(self->upred);
    Py_INCREF(self->bpred);
  }
  else {
    PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
    return -1;
  }
  self->py_c_it.c_it = self->cp_it;
  self->py_c_it.py_ve_it.ve_it = self->cp_it;
  self->py_c_it.py_ve_it.py_it.it = self->cp_it;
  return 0;
}

static void ChainPredicateIterator_dealloc(BPy_ChainPredicateIterator *self)
{
  Py_XDECREF(self->upred);
  Py_XDECREF(self->bpred);
  ChainingIterator_Type.tp_dealloc((PyObject *)self);
}

/*-----------------------BPy_ChainPredicateIterator type definition ----------------------------*/

PyTypeObject ChainPredicateIterator_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "ChainPredicateIterator",
    /*tp_basicsize*/ sizeof(BPy_ChainPredicateIterator),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)ChainPredicateIterator_dealloc,
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
    /*tp_doc*/ ChainPredicateIterator_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ nullptr,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr,
    /*tp_base*/ &ChainingIterator_Type,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)ChainPredicateIterator_init,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
