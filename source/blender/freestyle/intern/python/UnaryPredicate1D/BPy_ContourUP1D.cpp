/* SPDX-FileCopyrightText: 2004-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_ContourUP1D.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char ContourUP1D___doc__[] =
    "Class hierarchy: :class:`freestyle.types.UnaryPredicate1D` > :class:`ContourUP1D`\n"
    "\n"
    ".. method:: __call__(inter)\n"
    "\n"
    "   Returns true if the Interface1D is a contour. An Interface1D is a\n"
    "   contour if it is bordered by a different shape on each of its sides.\n"
    "\n"
    "   :arg inter: An Interface1D object.\n"
    "   :type inter: :class:`freestyle.types.Interface1D`\n"
    "   :return: True if the Interface1D is a contour, false otherwise.\n"
    "   :rtype: bool\n";

static int ContourUP1D___init__(BPy_ContourUP1D *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {nullptr};

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "", (char **)kwlist)) {
    return -1;
  }
  self->py_up1D.up1D = new Predicates1D::ContourUP1D();
  return 0;
}

/*-----------------------BPy_ContourUP1D type definition ------------------------------*/

PyTypeObject ContourUP1D_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "ContourUP1D",
    /*tp_basicsize*/ sizeof(BPy_ContourUP1D),
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
    /*tp_doc*/ ContourUP1D___doc__,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ nullptr,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr,
    /*tp_base*/ &UnaryPredicate1D_Type,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)ContourUP1D___init__,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
