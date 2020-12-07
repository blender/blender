/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

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
    "   Returns the average depth evaluated for an Interface1D.  The average\n"
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
          args, kwds, "d|O!", (char **)kwlist, &d, &IntegrationType_Type, &obj)) {
    return -1;
  }
  IntegrationType t = (obj) ? IntegrationType_from_BPy_IntegrationType(obj) : MEAN;
  self->py_uf1D_double.uf1D_double = new Functions1D::LocalAverageDepthF1D(d, t);
  return 0;
}

/*-----------------------BPy_LocalAverageDepthF1D type definition ------------------------------*/

PyTypeObject LocalAverageDepthF1D_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0) "LocalAverageDepthF1D", /* tp_name */
    sizeof(BPy_LocalAverageDepthF1D),                         /* tp_basicsize */
    0,                                                        /* tp_itemsize */
    nullptr,                                                  /* tp_dealloc */
#if PY_VERSION_HEX >= 0x03080000
    0, /* tp_vectorcall_offset */
#else
    nullptr, /* tp_print */
#endif
    nullptr,                                  /* tp_getattr */
    nullptr,                                  /* tp_setattr */
    nullptr,                                  /* tp_reserved */
    nullptr,                                  /* tp_repr */
    nullptr,                                  /* tp_as_number */
    nullptr,                                  /* tp_as_sequence */
    nullptr,                                  /* tp_as_mapping */
    nullptr,                                  /* tp_hash  */
    nullptr,                                  /* tp_call */
    nullptr,                                  /* tp_str */
    nullptr,                                  /* tp_getattro */
    nullptr,                                  /* tp_setattro */
    nullptr,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    LocalAverageDepthF1D___doc__,             /* tp_doc */
    nullptr,                                  /* tp_traverse */
    nullptr,                                  /* tp_clear */
    nullptr,                                  /* tp_richcompare */
    0,                                        /* tp_weaklistoffset */
    nullptr,                                  /* tp_iter */
    nullptr,                                  /* tp_iternext */
    nullptr,                                  /* tp_methods */
    nullptr,                                  /* tp_members */
    nullptr,                                  /* tp_getset */
    &UnaryFunction1DDouble_Type,              /* tp_base */
    nullptr,                                  /* tp_dict */
    nullptr,                                  /* tp_descr_get */
    nullptr,                                  /* tp_descr_set */
    0,                                        /* tp_dictoffset */
    (initproc)LocalAverageDepthF1D___init__,  /* tp_init */
    nullptr,                                  /* tp_alloc */
    nullptr,                                  /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
