/* SPDX-FileCopyrightText: 2004-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_GetDirectionalViewMapDensityF1D.h"

#include "../../../stroke/AdvancedFunctions1D.h"
#include "../../BPy_Convert.h"
#include "../../BPy_IntegrationType.h"

#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char GetDirectionalViewMapDensityF1D___doc__[] =
    "Class hierarchy: :class:`freestyle.types.UnaryFunction1D` > "
    ":class:`freestyle.types.UnaryFunction1DDouble` "
    "> :class:`GetDirectionalViewMapDensityF1D`\n"
    "\n"
    ".. method:: __init__(orientation, level, integration_type=IntegrationType.MEAN, "
    "sampling=2.0)\n"
    "\n"
    "   Builds a GetDirectionalViewMapDensityF1D object.\n"
    "\n"
    "   :arg orientation: The number of the directional map we must work\n"
    "      with.\n"
    "   :type orientation: int\n"
    "   :arg level: The level of the pyramid from which the pixel must be\n"
    "      read.\n"
    "   :type level: int\n"
    "   :arg integration_type: The integration method used to compute a single value\n"
    "      from a set of values.\n"
    "   :type integration_type: :class:`freestyle.types.IntegrationType`\n"
    "   :arg sampling: The resolution used to sample the chain: the\n"
    "      corresponding 0D function is evaluated at each sample point and\n"
    "      the result is obtained by combining the resulting values into a\n"
    "      single one, following the method specified by integration_type.\n"
    "   :type sampling: float\n"
    "\n"
    ".. method:: __call__(inter)\n"
    "\n"
    "   Returns the density evaluated for an Interface1D in of the steerable\n"
    "   viewmaps image. The direction telling which Directional map to choose\n"
    "   is explicitly specified by the user. The density is evaluated for a\n"
    "   set of points along the Interface1D (using the\n"
    "   :class:`freestyle.functions.ReadSteerableViewMapPixelF0D` functor) and\n"
    "   then integrated into a single value using a user-defined integration\n"
    "   method.\n"
    "\n"
    "   :arg inter: An Interface1D object.\n"
    "   :type inter: :class:`freestyle.types.Interface1D`\n"
    "   :return: the density evaluated for an Interface1D in of the\n"
    "      steerable viewmaps image.\n"
    "   :rtype: float\n";

static int GetDirectionalViewMapDensityF1D___init__(BPy_GetDirectionalViewMapDensityF1D *self,
                                                    PyObject *args,
                                                    PyObject *kwds)
{
  static const char *kwlist[] = {"orientation", "level", "integration_type", "sampling", nullptr};
  PyObject *obj = nullptr;
  uint u1, u2;
  float f = 2.0;

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "II|O!f", (char **)kwlist, &u1, &u2, &IntegrationType_Type, &obj, &f))
  {
    return -1;
  }
  IntegrationType t = (obj) ? IntegrationType_from_BPy_IntegrationType(obj) : MEAN;
  self->py_uf1D_double.uf1D_double = new Functions1D::GetDirectionalViewMapDensityF1D(
      u1, u2, t, f);
  return 0;
}

/*-----------------------BPy_GetDirectionalViewMapDensityF1D type definition --------------------*/

PyTypeObject GetDirectionalViewMapDensityF1D_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "GetDirectionalViewMapDensityF1D",
    /*tp_basicsize*/ sizeof(BPy_GetDirectionalViewMapDensityF1D),
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
    /*tp_doc*/ GetDirectionalViewMapDensityF1D___doc__,
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
    /*tp_init*/ (initproc)GetDirectionalViewMapDensityF1D___init__,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
