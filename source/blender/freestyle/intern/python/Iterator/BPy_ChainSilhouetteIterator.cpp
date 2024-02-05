/* SPDX-FileCopyrightText: 2004-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_ChainSilhouetteIterator.h"

#include "../BPy_Convert.h"
#include "../Interface1D/BPy_ViewEdge.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

// ChainSilhouetteIterator (bool restrict_to_selection=true, ViewEdge *begin=nullptr, bool
// orientation=true) ChainSilhouetteIterator (const ChainSilhouetteIterator &brother)

PyDoc_STRVAR(
    /* Wrap. */
    ChainSilhouetteIterator_doc,
    "Class hierarchy: :class:`freestyle.types.Iterator` >\n"
    ":class:`freestyle.types.ViewEdgeIterator` >\n"
    ":class:`freestyle.types.ChainingIterator` >\n"
    ":class:`ChainSilhouetteIterator`\n"
    "\n"
    "A ViewEdge Iterator used to follow ViewEdges the most naturally. For\n"
    "example, it will follow visible ViewEdges of same nature. As soon, as\n"
    "the nature or the visibility changes, the iteration stops (by setting\n"
    "the pointed ViewEdge to 0). In the case of an iteration over a set of\n"
    "ViewEdge that are both Silhouette and Crease, there will be a\n"
    "precedence of the silhouette over the crease criterion.\n"
    "\n"
    ".. method:: __init__(restrict_to_selection=True, begin=None, orientation=True)\n"
    "            __init__(brother)\n"
    "\n"
    "   Builds a ChainSilhouetteIterator from the first ViewEdge used for\n"
    "   iteration and its orientation or the copy constructor.\n"
    "\n"
    "   :arg restrict_to_selection: Indicates whether to force the chaining\n"
    "      to stay within the set of selected ViewEdges or not.\n"
    "   :type restrict_to_selection: bool\n"
    "   :arg begin: The ViewEdge from where to start the iteration.\n"
    "   :type begin: :class:`freestyle.types.ViewEdge` or None\n"
    "   :arg orientation: If true, we'll look for the next ViewEdge among\n"
    "      the ViewEdges that surround the ending ViewVertex of begin. If\n"
    "      false, we'll search over the ViewEdges surrounding the ending\n"
    "      ViewVertex of begin.\n"
    "   :type orientation: bool\n"
    "   :arg brother: A ChainSilhouetteIterator object.\n"
    "   :type brother: :class:`ChainSilhouetteIterator`");

static int check_begin(PyObject *obj, void *v)
{
  if (obj != nullptr && obj != Py_None && !BPy_ViewEdge_Check(obj)) {
    return 0;
  }
  *((PyObject **)v) = obj;
  return 1;
}

static int ChainSilhouetteIterator_init(BPy_ChainSilhouetteIterator *self,
                                        PyObject *args,
                                        PyObject *kwds)
{
  static const char *kwlist_1[] = {"brother", nullptr};
  static const char *kwlist_2[] = {"restrict_to_selection", "begin", "orientation", nullptr};
  PyObject *obj1 = nullptr, *obj2 = nullptr, *obj3 = nullptr;

  if (PyArg_ParseTupleAndKeywords(
          args, kwds, "O!", (char **)kwlist_1, &ChainSilhouetteIterator_Type, &obj1))
  {
    self->cs_it = new ChainSilhouetteIterator(*(((BPy_ChainSilhouetteIterator *)obj1)->cs_it));
  }
  else if ((void)PyErr_Clear(),
           (void)(obj1 = obj2 = obj3 = nullptr),
           PyArg_ParseTupleAndKeywords(args,
                                       kwds,
                                       "|O!O&O!",
                                       (char **)kwlist_2,
                                       &PyBool_Type,
                                       &obj1,
                                       check_begin,
                                       &obj2,
                                       &PyBool_Type,
                                       &obj3))
  {
    bool restrict_to_selection = (!obj1) ? true : bool_from_PyBool(obj1);
    ViewEdge *begin = (!obj2 || obj2 == Py_None) ? nullptr : ((BPy_ViewEdge *)obj2)->ve;
    bool orientation = (!obj3) ? true : bool_from_PyBool(obj3);
    self->cs_it = new ChainSilhouetteIterator(restrict_to_selection, begin, orientation);
  }
  else {
    PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
    return -1;
  }
  self->py_c_it.c_it = self->cs_it;
  self->py_c_it.py_ve_it.ve_it = self->cs_it;
  self->py_c_it.py_ve_it.py_it.it = self->cs_it;
  return 0;
}

/*-----------------------BPy_ChainSilhouetteIterator type definition ----------------------------*/

PyTypeObject ChainSilhouetteIterator_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "ChainSilhouetteIterator",
    /*tp_basicsize*/ sizeof(BPy_ChainSilhouetteIterator),
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
    /*tp_doc*/ ChainSilhouetteIterator_doc,
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
    /*tp_init*/ (initproc)ChainSilhouetteIterator_init,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
