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

#include "BPy_ChainPredicateIterator.h"

#include "../BPy_Convert.h"
#include "../BPy_BinaryPredicate1D.h"
#include "../Interface1D/BPy_ViewEdge.h"
#include "../BPy_UnaryPredicate1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(ChainPredicateIterator_doc,

             "Class hierarchy: :class:`freestyle.types.Iterator` >\n"
             ":class:`freestyle.types.ViewEdgeIterator` >\n"
             ":class:`freestyle.types.ChainingIterator` >\n"
             ":class:`ChainPredicateIterator`\n"
             "\n"
             "A \"generic\" user-controlled ViewEdge iterator.  This iterator is in\n"
             "particular built from a unary predicate and a binary predicate.\n"
             "First, the unary predicate is evaluated for all potential next\n"
             "ViewEdges in order to only keep the ones respecting a certain\n"
             "constraint.  Then, the binary predicate is evaluated on the current\n"
             "ViewEdge together with each ViewEdge of the previous selection.  The\n"
             "first ViewEdge respecting both the unary predicate and the binary\n"
             "predicate is kept as the next one.  If none of the potential next\n"
             "ViewEdge respects these two predicates, None is returned.\n"
             "\n"
             ".. method:: __init__(upred, bpred, restrict_to_selection=True, "
             "restrict_to_unvisited=True, begin=None, "
             "orientation=True)\n"
             "\n"
             "   Builds a ChainPredicateIterator from a unary predicate, a binary\n"
             "   predicate, a starting ViewEdge and its orientation.\n"
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
             "      the ViewEdges that surround the ending ViewVertex of begin.  If\n"
             "      false, we'll search over the ViewEdges surrounding the ending\n"
             "      ViewVertex of begin.\n"
             "   :type orientation: bool\n"
             "\n"
             ".. method:: __init__(brother)\n"
             "\n"
             "   Copy constructor.\n"
             "\n"
             "   :arg brother: A ChainPredicateIterator object.\n"
             "   :type brother: :class:`ChainPredicateIterator`");

static int check_begin(PyObject *obj, void *v)
{
  if (obj != NULL && obj != Py_None && !BPy_ViewEdge_Check(obj))
    return 0;
  *((PyObject **)v) = obj;
  return 1;
}

static int ChainPredicateIterator_init(BPy_ChainPredicateIterator *self,
                                       PyObject *args,
                                       PyObject *kwds)
{
  static const char *kwlist_1[] = {"brother", NULL};
  static const char *kwlist_2[] = {"upred",
                                   "bpred",
                                   "restrict_to_selection",
                                   "restrict_to_unvisited",
                                   "begin",
                                   "orientation",
                                   NULL};
  PyObject *obj1 = 0, *obj2 = 0, *obj3 = 0, *obj4 = 0, *obj5 = 0, *obj6 = 0;

  if (PyArg_ParseTupleAndKeywords(
          args, kwds, "O!", (char **)kwlist_1, &ChainPredicateIterator_Type, &obj1)) {
    self->cp_it = new ChainPredicateIterator(*(((BPy_ChainPredicateIterator *)obj1)->cp_it));
    self->upred = ((BPy_ChainPredicateIterator *)obj1)->upred;
    self->bpred = ((BPy_ChainPredicateIterator *)obj1)->bpred;
    Py_INCREF(self->upred);
    Py_INCREF(self->bpred);
  }
  else if (PyErr_Clear(),
           (obj3 = obj4 = obj5 = obj6 = 0),
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
                                       &obj6)) {
    UnaryPredicate1D *up1D = ((BPy_UnaryPredicate1D *)obj1)->up1D;
    BinaryPredicate1D *bp1D = ((BPy_BinaryPredicate1D *)obj2)->bp1D;
    bool restrict_to_selection = (!obj3) ? true : bool_from_PyBool(obj3);
    bool restrict_to_unvisited = (!obj4) ? true : bool_from_PyBool(obj4);
    ViewEdge *begin = (!obj5 || obj5 == Py_None) ? NULL : ((BPy_ViewEdge *)obj5)->ve;
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
    PyVarObject_HEAD_INIT(NULL, 0) "ChainPredicateIterator", /* tp_name */
    sizeof(BPy_ChainPredicateIterator),                      /* tp_basicsize */
    0,                                                       /* tp_itemsize */
    (destructor)ChainPredicateIterator_dealloc,              /* tp_dealloc */
    0,                                                       /* tp_print */
    0,                                                       /* tp_getattr */
    0,                                                       /* tp_setattr */
    0,                                                       /* tp_reserved */
    0,                                                       /* tp_repr */
    0,                                                       /* tp_as_number */
    0,                                                       /* tp_as_sequence */
    0,                                                       /* tp_as_mapping */
    0,                                                       /* tp_hash  */
    0,                                                       /* tp_call */
    0,                                                       /* tp_str */
    0,                                                       /* tp_getattro */
    0,                                                       /* tp_setattro */
    0,                                                       /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,                /* tp_flags */
    ChainPredicateIterator_doc,                              /* tp_doc */
    0,                                                       /* tp_traverse */
    0,                                                       /* tp_clear */
    0,                                                       /* tp_richcompare */
    0,                                                       /* tp_weaklistoffset */
    0,                                                       /* tp_iter */
    0,                                                       /* tp_iternext */
    0,                                                       /* tp_methods */
    0,                                                       /* tp_members */
    0,                                                       /* tp_getset */
    &ChainingIterator_Type,                                  /* tp_base */
    0,                                                       /* tp_dict */
    0,                                                       /* tp_descr_get */
    0,                                                       /* tp_descr_set */
    0,                                                       /* tp_dictoffset */
    (initproc)ChainPredicateIterator_init,                   /* tp_init */
    0,                                                       /* tp_alloc */
    0,                                                       /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
