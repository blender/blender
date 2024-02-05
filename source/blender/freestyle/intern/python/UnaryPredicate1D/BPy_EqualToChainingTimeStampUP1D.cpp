/* SPDX-FileCopyrightText: 2004-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_EqualToChainingTimeStampUP1D.h"

#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(
    /* Wrap. */
    EqualToChainingTimeStampUP1D___doc__,
    "Class hierarchy: :class:`freestyle.types.UnaryPredicate1D` > "
    ":class:`freestyle.types.EqualToChainingTimeStampUP1D`\n"
    "\n"
    ".. method:: __init__(ts)\n"
    "\n"
    "   Builds a EqualToChainingTimeStampUP1D object.\n"
    "\n"
    "   :arg ts: A time stamp value.\n"
    "   :type ts: int\n"
    "\n"
    ".. method:: __call__(inter)\n"
    "\n"
    "   Returns true if the Interface1D's time stamp is equal to a certain\n"
    "   user-defined value.\n"
    "\n"
    "   :arg inter: An Interface1D object.\n"
    "   :type inter: :class:`freestyle.types.Interface1D`\n"
    "   :return: True if the time stamp is equal to a user-defined value.\n"
    "   :rtype: bool\n");

static int EqualToChainingTimeStampUP1D___init__(BPy_EqualToChainingTimeStampUP1D *self,
                                                 PyObject *args,
                                                 PyObject *kwds)
{
  static const char *kwlist[] = {"ts", nullptr};
  uint u;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "I", (char **)kwlist, &u)) {
    return -1;
  }
  self->py_up1D.up1D = new Predicates1D::EqualToChainingTimeStampUP1D(u);
  return 0;
}

/*-----------------------BPy_EqualToChainingTimeStampUP1D type definition -----------------------*/

PyTypeObject EqualToChainingTimeStampUP1D_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "EqualToChainingTimeStampUP1D",
    /*tp_basicsize*/ sizeof(BPy_EqualToChainingTimeStampUP1D),
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
    /*tp_doc*/ EqualToChainingTimeStampUP1D___doc__,
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
    /*tp_init*/ (initproc)EqualToChainingTimeStampUP1D___init__,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
