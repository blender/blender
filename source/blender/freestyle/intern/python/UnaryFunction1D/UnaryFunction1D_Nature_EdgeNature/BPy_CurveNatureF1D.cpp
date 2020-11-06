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

#include "BPy_CurveNatureF1D.h"

#include "../../../view_map/Functions1D.h"
#include "../../BPy_Convert.h"
#include "../../BPy_IntegrationType.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char CurveNatureF1D___doc__[] =
    "Class hierarchy: :class:`freestyle.types.UnaryFunction1D` > "
    ":class:`freestyle.types.UnaryFunction1DEdgeNature` > :class:`CurveNatureF1D`\n"
    "\n"
    ".. method:: __init__(integration_type=IntegrationType.MEAN)\n"
    "\n"
    "   Builds a CurveNatureF1D object.\n"
    "\n"
    "   :arg integration_type: The integration method used to compute a single value\n"
    "      from a set of values.\n"
    "   :type integration_type: :class:`freestyle.types.IntegrationType`\n"
    "\n"
    ".. method:: __call__(inter)\n"
    "\n"
    "   Returns the nature of the Interface1D (silhouette, ridge, crease, and\n"
    "   so on).  Except if the Interface1D is a\n"
    "   :class:`freestyle.types.ViewEdge`, this result might be ambiguous.\n"
    "   Indeed, the Interface1D might result from the gathering of several 1D\n"
    "   elements, each one being of a different nature.  An integration\n"
    "   method, such as the MEAN, might give, in this case, irrelevant\n"
    "   results.\n"
    "\n"
    "   :arg inter: An Interface1D object.\n"
    "   :type inter: :class:`freestyle.types.Interface1D`\n"
    "   :return: The nature of the Interface1D.\n"
    "   :rtype: :class:`freestyle.types.Nature`\n";

static int CurveNatureF1D___init__(BPy_CurveNatureF1D *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"integration_type", nullptr};
  PyObject *obj = nullptr;

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "|O!", (char **)kwlist, &IntegrationType_Type, &obj)) {
    return -1;
  }
  IntegrationType t = (obj) ? IntegrationType_from_BPy_IntegrationType(obj) : MEAN;
  self->py_uf1D_edgenature.uf1D_edgenature = new Functions1D::CurveNatureF1D(t);
  return 0;
}

/*-----------------------BPy_CurveNatureF1D type definition ------------------------------*/

PyTypeObject CurveNatureF1D_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0) "CurveNatureF1D", /* tp_name */
    sizeof(BPy_CurveNatureF1D),                      /* tp_basicsize */
    0,                                               /* tp_itemsize */
    nullptr,                                               /* tp_dealloc */
    nullptr,                                               /* tp_print */
    nullptr,                                               /* tp_getattr */
    nullptr,                                               /* tp_setattr */
    nullptr,                                               /* tp_reserved */
    nullptr,                                               /* tp_repr */
    nullptr,                                               /* tp_as_number */
    nullptr,                                               /* tp_as_sequence */
    nullptr,                                               /* tp_as_mapping */
    nullptr,                                               /* tp_hash  */
    nullptr,                                               /* tp_call */
    nullptr,                                               /* tp_str */
    nullptr,                                               /* tp_getattro */
    nullptr,                                               /* tp_setattro */
    nullptr,                                               /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    CurveNatureF1D___doc__,                          /* tp_doc */
    nullptr,                                               /* tp_traverse */
    nullptr,                                               /* tp_clear */
    nullptr,                                               /* tp_richcompare */
    0,                                               /* tp_weaklistoffset */
    nullptr,                                               /* tp_iter */
    nullptr,                                               /* tp_iternext */
    nullptr,                                               /* tp_methods */
    nullptr,                                               /* tp_members */
    nullptr,                                               /* tp_getset */
    &UnaryFunction1DEdgeNature_Type,                 /* tp_base */
    nullptr,                                               /* tp_dict */
    nullptr,                                               /* tp_descr_get */
    nullptr,                                               /* tp_descr_set */
    0,                                               /* tp_dictoffset */
    (initproc)CurveNatureF1D___init__,               /* tp_init */
    nullptr,                                               /* tp_alloc */
    nullptr,                                               /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
