/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_DensityF1D.h"

#include "../../../stroke/AdvancedFunctions1D.h"
#include "../../BPy_Convert.h"
#include "../../BPy_IntegrationType.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char DensityF1D___doc__[] =
    "Class hierarchy: :class:`freestyle.types.UnaryFunction1D` > "
    ":class:`freestyle.types.UnaryFunction1DDouble` > :class:`DensityF1D`\n"
    "\n"
    ".. method:: __init__(sigma=2.0, integration_type=IntegrationType.MEAN, sampling=2.0)\n"
    "\n"
    "   Builds a DensityF1D object.\n"
    "\n"
    "   :arg sigma: The sigma used in DensityF0D and determining the window size\n"
    "      used in each density query.\n"
    "   :type sigma: float\n"
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
    "   Returns the density evaluated for an Interface1D. The density is\n"
    "   evaluated for a set of points along the Interface1D (using the\n"
    "   :class:`freestyle.functions.DensityF0D` functor) with a user-defined\n"
    "   sampling and then integrated into a single value using a user-defined\n"
    "   integration method.\n"
    "\n"
    "   :arg inter: An Interface1D object.\n"
    "   :type inter: :class:`freestyle.types.Interface1D`\n"
    "   :return: The density evaluated for an Interface1D.\n"
    "   :rtype: float\n";

static int DensityF1D___init__(BPy_DensityF1D *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"sigma", "integration_type", "sampling", nullptr};
  PyObject *obj = nullptr;
  double d = 2.0;
  float f = 2.0;

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "|dO!f", (char **)kwlist, &d, &IntegrationType_Type, &obj, &f))
  {
    return -1;
  }
  IntegrationType t = (obj) ? IntegrationType_from_BPy_IntegrationType(obj) : MEAN;
  self->py_uf1D_double.uf1D_double = new Functions1D::DensityF1D(d, t, f);
  return 0;
}

/*-----------------------BPy_DensityF1D type definition ------------------------------*/

PyTypeObject DensityF1D_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "DensityF1D",
    /*tp_basicsize*/ sizeof(BPy_DensityF1D),
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
    /*tp_doc*/ DensityF1D___doc__,
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
    /*tp_init*/ (initproc)DensityF1D___init__,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
