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

#include "BPy_GetYF1D.h"

#include "../../../view_map/Functions1D.h"
#include "../../BPy_Convert.h"
#include "../../BPy_IntegrationType.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char GetYF1D___doc__[] =
    "Class hierarchy: :class:`freestyle.types.UnaryFunction1D` > "
    ":class:`freestyle.types.UnaryFunction1DDouble` > :class:`GetYF1D`\n"
    "\n"
    ".. method:: __init__(integration_type=IntegrationType.MEAN)\n"
    "\n"
    "   Builds a GetYF1D object.\n"
    "\n"
    "   :arg integration_type: The integration method used to compute a single value\n"
    "      from a set of values.\n"
    "   :type integration_type: :class:`freestyle.types.IntegrationType`\n"
    "\n"
    ".. method:: __call__(inter)\n"
    "\n"
    "   Returns the Y 3D coordinate of an Interface1D.\n"
    "\n"
    "   :arg inter: An Interface1D object.\n"
    "   :type inter: :class:`freestyle.types.Interface1D`\n"
    "   :return: The Y 3D coordinate of the Interface1D.\n"
    "   :rtype: float\n";

static int GetYF1D___init__(BPy_GetYF1D *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"integration_type", NULL};
  PyObject *obj = 0;

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "|O!", (char **)kwlist, &IntegrationType_Type, &obj)) {
    return -1;
  }
  IntegrationType t = (obj) ? IntegrationType_from_BPy_IntegrationType(obj) : MEAN;
  self->py_uf1D_double.uf1D_double = new Functions1D::GetYF1D(t);
  return 0;
}
/*-----------------------BPy_GetYF1D type definition ------------------------------*/

PyTypeObject GetYF1D_Type = {
    PyVarObject_HEAD_INIT(NULL, 0) "GetYF1D", /* tp_name */
    sizeof(BPy_GetYF1D),                      /* tp_basicsize */
    0,                                        /* tp_itemsize */
    0,                                        /* tp_dealloc */
    0,                                        /* tp_print */
    0,                                        /* tp_getattr */
    0,                                        /* tp_setattr */
    0,                                        /* tp_reserved */
    0,                                        /* tp_repr */
    0,                                        /* tp_as_number */
    0,                                        /* tp_as_sequence */
    0,                                        /* tp_as_mapping */
    0,                                        /* tp_hash  */
    0,                                        /* tp_call */
    0,                                        /* tp_str */
    0,                                        /* tp_getattro */
    0,                                        /* tp_setattro */
    0,                                        /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    GetYF1D___doc__,                          /* tp_doc */
    0,                                        /* tp_traverse */
    0,                                        /* tp_clear */
    0,                                        /* tp_richcompare */
    0,                                        /* tp_weaklistoffset */
    0,                                        /* tp_iter */
    0,                                        /* tp_iternext */
    0,                                        /* tp_methods */
    0,                                        /* tp_members */
    0,                                        /* tp_getset */
    &UnaryFunction1DDouble_Type,              /* tp_base */
    0,                                        /* tp_dict */
    0,                                        /* tp_descr_get */
    0,                                        /* tp_descr_set */
    0,                                        /* tp_dictoffset */
    (initproc)GetYF1D___init__,               /* tp_init */
    0,                                        /* tp_alloc */
    0,                                        /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
