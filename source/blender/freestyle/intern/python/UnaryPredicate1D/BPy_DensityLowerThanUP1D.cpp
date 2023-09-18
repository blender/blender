/* SPDX-FileCopyrightText: 2004-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_DensityLowerThanUP1D.h"

#include "../../stroke/AdvancedPredicates1D.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char DensityLowerThanUP1D___doc__[] =
    "Class hierarchy: :class:`freestyle.types.UnaryPredicate1D` > :class:`DensityLowerThanUP1D`\n"
    "\n"
    ".. method:: __init__(threshold, sigma=2.0)\n"
    "\n"
    "   Builds a DensityLowerThanUP1D object.\n"
    "\n"
    "   :arg threshold: The value of the threshold density. Any Interface1D\n"
    "      having a density lower than this threshold will match.\n"
    "   :type threshold: float\n"
    "   :arg sigma: The sigma value defining the density evaluation window\n"
    "      size used in the :class:`freestyle.functions.DensityF0D` functor.\n"
    "   :type sigma: float\n"
    "\n"
    ".. method:: __call__(inter)\n"
    "\n"
    "   Returns true if the density evaluated for the Interface1D is less\n"
    "   than a user-defined density value.\n"
    "\n"
    "   :arg inter: An Interface1D object.\n"
    "   :type inter: :class:`freestyle.types.Interface1D`\n"
    "   :return: True if the density is lower than a threshold.\n"
    "   :rtype: bool\n";

static int DensityLowerThanUP1D___init__(BPy_DensityLowerThanUP1D *self,
                                         PyObject *args,
                                         PyObject *kwds)
{
  static const char *kwlist[] = {"threshold", "sigma", nullptr};
  double d1, d2 = 2.0;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "d|d", (char **)kwlist, &d1, &d2)) {
    return -1;
  }
  self->py_up1D.up1D = new Predicates1D::DensityLowerThanUP1D(d1, d2);
  return 0;
}

/*-----------------------BPy_DensityLowerThanUP1D type definition ------------------------------*/

PyTypeObject DensityLowerThanUP1D_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "DensityLowerThanUP1D",
    /*tp_basicsize*/ sizeof(BPy_DensityLowerThanUP1D),
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
    /*tp_doc*/ DensityLowerThanUP1D___doc__,
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
    /*tp_init*/ (initproc)DensityLowerThanUP1D___init__,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
