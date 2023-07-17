/* SPDX-FileCopyrightText: 2004-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_ShapeUP1D.h"

#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char ShapeUP1D___doc__[] =
    "Class hierarchy: :class:`freestyle.types.UnaryPredicate1D` > :class:`ShapeUP1D`\n"
    "\n"
    ".. method:: __init__(first, second=0)\n"
    "\n"
    "   Builds a ShapeUP1D object.\n"
    "\n"
    "   :arg first: The first Id component.\n"
    "   :type first: int\n"
    "   :arg second: The second Id component.\n"
    "   :type second: int\n"
    "\n"
    ".. method:: __call__(inter)\n"
    "\n"
    "   Returns true if the shape to which the Interface1D belongs to has the\n"
    "   same :class:`freestyle.types.Id` as the one specified by the user.\n"
    "\n"
    "   :arg inter: An Interface1D object.\n"
    "   :type inter: :class:`freestyle.types.Interface1D`\n"
    "   :return: True if Interface1D belongs to the shape of the\n"
    "      user-specified Id.\n"
    "   :rtype: bool\n";

static int ShapeUP1D___init__(BPy_ShapeUP1D *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"first", "second", nullptr};
  uint u1, u2 = 0;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "I|I", (char **)kwlist, &u1, &u2)) {
    return -1;
  }
  self->py_up1D.up1D = new Predicates1D::ShapeUP1D(u1, u2);
  return 0;
}

/*-----------------------BPy_ShapeUP1D type definition ------------------------------*/

PyTypeObject ShapeUP1D_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "ShapeUP1D",
    /*tp_basicsize*/ sizeof(BPy_ShapeUP1D),
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
    /*tp_doc*/ ShapeUP1D___doc__,
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
    /*tp_init*/ (initproc)ShapeUP1D___init__,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
