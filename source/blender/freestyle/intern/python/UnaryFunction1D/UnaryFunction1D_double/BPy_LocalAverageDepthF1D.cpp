/* SPDX-FileCopyrightText: 2004-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_LocalAverageDepthF1D.h"

#include "../../../stroke/AdvancedFunctions1D.h"
#include "../../BPy_Convert.h"
#include "../../BPy_IntegrationType.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char LocalAverageDepthF1D___doc__[] =
    "Class hierarchy: :class:`freestyle.types.UnaryFunction1D` > "
    ":class:`freestyle.types.UnaryFunction1DDouble` > :class:`LocalAverageDepthF1D`\n"
    "\n"
    ".. method:: __init__(sigma, integration_type=IntegrationType.MEAN)\n"
    "\n"
    "   Builds a LocalAverageDepthF1D object.\n"
    "\n"
    "   :arg sigma: The sigma used in DensityF0D and determining the window\n"
    "      size used in each density query.\n"
    "   :type sigma: float\n"
    "   :arg integration_type: The integration method used to compute a single value\n"
    "      from a set of values.\n"
    "   :type integration_type: :class:`freestyle.types.IntegrationType`\n"
    "\n"
    ".. method:: __call__(inter)\n"
    "\n"
    "   Returns the average depth evaluated for an Interface1D. The average\n"
    "   depth is evaluated for a set of points along the Interface1D (using\n"
    "   the :class:`freestyle.functions.LocalAverageDepthF0D` functor) with a\n"
    "   user-defined sampling and then integrated into a single value using a\n"
    "   user-defined integration method.\n"
    "\n"
    "   :arg inter: An Interface1D object.\n"
    "   :type inter: :class:`freestyle.types.Interface1D`\n"
    "   :return: The average depth evaluated for the Interface1D.\n"
    "   :rtype: float\n";

static int LocalAverageDepthF1D___init__(BPy_LocalAverageDepthF1D *self,
                                         PyObject *args,
                                         PyObject *kwds)
{
  static const char *kwlist[] = {"sigma", "integration_type", nullptr};
  PyObject *obj = nullptr;
  double d;

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "d|O!", (char **)kwlist, &d, &IntegrationType_Type, &obj))
  {
    return -1;
  }
  IntegrationType t = (obj) ? IntegrationType_from_BPy_IntegrationType(obj) : MEAN;
  self->py_uf1D_double.uf1D_double = new Functions1D::LocalAverageDepthF1D(d, t);
  return 0;
}

/*-----------------------BPy_LocalAverageDepthF1D type definition ------------------------------*/

PyTypeObject LocalAverageDepthF1D_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "LocalAverageDepthF1D",
    /*tp_basicsize*/ sizeof(BPy_LocalAverageDepthF1D),
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
    /*tp_doc*/ LocalAverageDepthF1D___doc__,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ nullptr,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr,
    /*tp_base*/ &UnaryFunction1DDouble_Type,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)LocalAverageDepthF1D___init__,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
