/* SPDX-FileCopyrightText: 2004-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_LocalAverageDepthF0D.h"

#include "../../../stroke/AdvancedFunctions0D.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char LocalAverageDepthF0D___doc__[] =
    "Class hierarchy: :class:`freestyle.types.UnaryFunction0D` > "
    ":class:`freestyle.types.UnaryFunction0DDouble` > :class:`LocalAverageDepthF0D`\n"
    "\n"
    ".. method:: __init__(mask_size=5.0)\n"
    "\n"
    "   Builds a LocalAverageDepthF0D object.\n"
    "\n"
    "   :arg mask_size: The size of the mask.\n"
    "   :type mask_size: float\n"
    "\n"
    ".. method:: __call__(it)\n"
    "\n"
    "   Returns the average depth around the\n"
    "   :class:`freestyle.types.Interface0D` pointed by the\n"
    "   Interface0DIterator.  The result is obtained by querying the depth\n"
    "   buffer on a window around that point.\n"
    "\n"
    "   :arg it: An Interface0DIterator object.\n"
    "   :type it: :class:`freestyle.types.Interface0DIterator`\n"
    "   :return: The average depth around the pointed Interface0D.\n"
    "   :rtype: float\n";

static int LocalAverageDepthF0D___init__(BPy_LocalAverageDepthF0D *self,
                                         PyObject *args,
                                         PyObject *kwds)
{
  static const char *kwlist[] = {"mask_size", nullptr};
  double d = 5.0;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "|d", (char **)kwlist, &d)) {
    return -1;
  }
  self->py_uf0D_double.uf0D_double = new Functions0D::LocalAverageDepthF0D(d);
  self->py_uf0D_double.uf0D_double->py_uf0D = (PyObject *)self;
  return 0;
}

/*-----------------------BPy_LocalAverageDepthF0D type definition ------------------------------*/

PyTypeObject LocalAverageDepthF0D_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "LocalAverageDepthF0D",
    /*tp_basicsize*/ sizeof(BPy_LocalAverageDepthF0D),
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
    /*tp_doc*/ LocalAverageDepthF0D___doc__,
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
    /*tp_init*/ (initproc)LocalAverageDepthF0D___init__,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
