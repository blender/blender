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

#include "BPy_ChainSilhouetteIterator.h"

#include "../BPy_Convert.h"
#include "../Interface1D/BPy_ViewEdge.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

// ChainSilhouetteIterator (bool restrict_to_selection=true, ViewEdge *begin=NULL, bool
// orientation=true) ChainSilhouetteIterator (const ChainSilhouetteIterator &brother)

PyDoc_STRVAR(ChainSilhouetteIterator_doc,
             "Class hierarchy: :class:`freestyle.types.Iterator` >\n"
             ":class:`freestyle.types.ViewEdgeIterator` >\n"
             ":class:`freestyle.types.ChainingIterator` >\n"
             ":class:`ChainSilhouetteIterator`\n"
             "\n"
             "A ViewEdge Iterator used to follow ViewEdges the most naturally.  For\n"
             "example, it will follow visible ViewEdges of same nature.  As soon, as\n"
             "the nature or the visibility changes, the iteration stops (by setting\n"
             "the pointed ViewEdge to 0).  In the case of an iteration over a set of\n"
             "ViewEdge that are both Silhouette and Crease, there will be a\n"
             "precedence of the silhouette over the crease criterion.\n"
             "\n"
             ".. method:: __init__(restrict_to_selection=True, begin=None, orientation=True)\n"
             "\n"
             "   Builds a ChainSilhouetteIterator from the first ViewEdge used for\n"
             "   iteration and its orientation.\n"
             "\n"
             "   :arg restrict_to_selection: Indicates whether to force the chaining\n"
             "      to stay within the set of selected ViewEdges or not.\n"
             "   :type restrict_to_selection: bool\n"
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
             "   :arg brother: A ChainSilhouetteIterator object.\n"
             "   :type brother: :class:`ChainSilhouetteIterator`");

static int check_begin(PyObject *obj, void *v)
{
  if (obj != NULL && obj != Py_None && !BPy_ViewEdge_Check(obj)) {
    return 0;
  }
  *((PyObject **)v) = obj;
  return 1;
}

static int ChainSilhouetteIterator_init(BPy_ChainSilhouetteIterator *self,
                                        PyObject *args,
                                        PyObject *kwds)
{
  static const char *kwlist_1[] = {"brother", NULL};
  static const char *kwlist_2[] = {"restrict_to_selection", "begin", "orientation", NULL};
  PyObject *obj1 = 0, *obj2 = 0, *obj3 = 0;

  if (PyArg_ParseTupleAndKeywords(
          args, kwds, "O!", (char **)kwlist_1, &ChainSilhouetteIterator_Type, &obj1)) {
    self->cs_it = new ChainSilhouetteIterator(*(((BPy_ChainSilhouetteIterator *)obj1)->cs_it));
  }
  else if (PyErr_Clear(),
           (obj1 = obj2 = obj3 = 0),
           PyArg_ParseTupleAndKeywords(args,
                                       kwds,
                                       "|O!O&O!",
                                       (char **)kwlist_2,
                                       &PyBool_Type,
                                       &obj1,
                                       check_begin,
                                       &obj2,
                                       &PyBool_Type,
                                       &obj3)) {
    bool restrict_to_selection = (!obj1) ? true : bool_from_PyBool(obj1);
    ViewEdge *begin = (!obj2 || obj2 == Py_None) ? NULL : ((BPy_ViewEdge *)obj2)->ve;
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
    PyVarObject_HEAD_INIT(NULL, 0) "ChainSilhouetteIterator", /* tp_name */
    sizeof(BPy_ChainSilhouetteIterator),                      /* tp_basicsize */
    0,                                                        /* tp_itemsize */
    0,                                                        /* tp_dealloc */
    0,                                                        /* tp_print */
    0,                                                        /* tp_getattr */
    0,                                                        /* tp_setattr */
    0,                                                        /* tp_reserved */
    0,                                                        /* tp_repr */
    0,                                                        /* tp_as_number */
    0,                                                        /* tp_as_sequence */
    0,                                                        /* tp_as_mapping */
    0,                                                        /* tp_hash  */
    0,                                                        /* tp_call */
    0,                                                        /* tp_str */
    0,                                                        /* tp_getattro */
    0,                                                        /* tp_setattro */
    0,                                                        /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,                 /* tp_flags */
    ChainSilhouetteIterator_doc,                              /* tp_doc */
    0,                                                        /* tp_traverse */
    0,                                                        /* tp_clear */
    0,                                                        /* tp_richcompare */
    0,                                                        /* tp_weaklistoffset */
    0,                                                        /* tp_iter */
    0,                                                        /* tp_iternext */
    0,                                                        /* tp_methods */
    0,                                                        /* tp_members */
    0,                                                        /* tp_getset */
    &ChainingIterator_Type,                                   /* tp_base */
    0,                                                        /* tp_dict */
    0,                                                        /* tp_descr_get */
    0,                                                        /* tp_descr_set */
    0,                                                        /* tp_dictoffset */
    (initproc)ChainSilhouetteIterator_init,                   /* tp_init */
    0,                                                        /* tp_alloc */
    0,                                                        /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
