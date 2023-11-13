/* SPDX-FileCopyrightText: 2004-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_DensityF0D.h"

#include "../../../stroke/AdvancedFunctions0D.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char DensityF0D___doc__[] =
    "Class hierarchy: :class:`freestyle.types.UnaryFunction0D` > "
    ":class:`freestyle.types.UnaryFunction0DDouble` > :class:`DensityF0D`\n"
    "\n"
    ".. method:: __init__(sigma=2.0)\n"
    "\n"
    "   Builds a DensityF0D object.\n"
    "\n"
    "   :arg sigma: The gaussian sigma value indicating the X value for\n"
    "      which the gaussian function is 0.5. It leads to the window size\n"
    "      value (the larger, the smoother).\n"
    "   :type sigma: float\n"
    "\n"
    ".. method:: __call__(it)\n"
    "\n"
    "   Returns the density of the (result) image evaluated at the\n"
    "   :class:`freestyle.types.Interface0D` pointed by the\n"
    "   Interface0DIterator. This density is evaluated using a pixels square\n"
    "   window around the evaluation point and integrating these values using\n"
    "   a gaussian.\n"
    "\n"
    "   :arg it: An Interface0DIterator object.\n"
    "   :type it: :class:`freestyle.types.Interface0DIterator`\n"
    "   :return: The density of the image evaluated at the pointed\n"
    "      Interface0D.\n"
    "   :rtype: float\n";

static int DensityF0D___init__(BPy_DensityF0D *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"sigma", nullptr};
  double d = 2;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "|d", (char **)kwlist, &d)) {
    return -1;
  }
  self->py_uf0D_double.uf0D_double = new Functions0D::DensityF0D(d);
  self->py_uf0D_double.uf0D_double->py_uf0D = (PyObject *)self;
  return 0;
}

/*-----------------------BPy_DensityF0D type definition ------------------------------*/

PyTypeObject DensityF0D_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "DensityF0D",
    /*tp_basicsize*/ sizeof(BPy_DensityF0D),
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
    /*tp_doc*/ DensityF0D___doc__,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ nullptr,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr,
    /*tp_base*/ &UnaryFunction0DDouble_Type,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)DensityF0D___init__,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
