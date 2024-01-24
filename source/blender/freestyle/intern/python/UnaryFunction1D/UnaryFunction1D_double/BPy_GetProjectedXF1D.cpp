/* SPDX-FileCopyrightText: 2004-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_GetProjectedXF1D.h"

#include "../../../view_map/Functions1D.h"
#include "../../BPy_Convert.h"
#include "../../BPy_IntegrationType.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(
    /* Wrap. */
    GetProjectedXF1D___doc__,
    "Class hierarchy: :class:`freestyle.types.UnaryFunction1D` > "
    ":class:`freestyle.types.UnaryFunction1DDouble` > :class:`GetProjectedXF1D`\n"
    "\n"
    ".. method:: __init__(integration_type=IntegrationType.MEAN)\n"
    "\n"
    "   Builds a GetProjectedXF1D object.\n"
    "\n"
    "   :arg integration_type: The integration method used to compute a single value\n"
    "      from a set of values. \n"
    "   :type integration_type: :class:`freestyle.types.IntegrationType`\n"
    "\n"
    ".. method:: __call__(inter)\n"
    "\n"
    "   Returns the projected X 3D coordinate of an Interface1D.\n"
    "\n"
    "   :arg inter: An Interface1D object.\n"
    "   :type inter: :class:`freestyle.types.Interface1D`\n"
    "   :return: The projected X 3D coordinate of an Interface1D.\n"
    "   :rtype: float\n");

static int GetProjectedXF1D___init__(BPy_GetProjectedXF1D *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"integration_type", nullptr};
  PyObject *obj = nullptr;

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "|O!", (char **)kwlist, &IntegrationType_Type, &obj))
  {
    return -1;
  }
  IntegrationType t = (obj) ? IntegrationType_from_BPy_IntegrationType(obj) : MEAN;
  self->py_uf1D_double.uf1D_double = new Functions1D::GetProjectedXF1D(t);
  return 0;
}

/*-----------------------BPy_GetProjectedXF1D type definition ------------------------------*/

PyTypeObject GetProjectedXF1D_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "GetProjectedXF1D",
    /*tp_basicsize*/ sizeof(BPy_GetProjectedXF1D),
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
    /*tp_doc*/ GetProjectedXF1D___doc__,
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
    /*tp_init*/ (initproc)GetProjectedXF1D___init__,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
