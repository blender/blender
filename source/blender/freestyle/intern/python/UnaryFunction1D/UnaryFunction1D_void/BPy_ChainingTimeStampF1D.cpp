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

#include "BPy_ChainingTimeStampF1D.h"

#include "../../../view_map/Functions1D.h"
#include "../../BPy_Convert.h"
#include "../../BPy_IntegrationType.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char ChainingTimeStampF1D___doc__[] =
    "Class hierarchy: :class:`freestyle.types.UnaryFunction1D` > "
    ":class:`freestyle.types.UnaryFunction1DVoid` > :class:`ChainingTimeStampF1D`\n"
    "\n"
    ".. method:: __init__()\n"
    "\n"
    "   Builds a ChainingTimeStampF1D object.\n"
    "\n"
    ".. method:: __call__(inter)\n"
    "\n"
    "   Sets the chaining time stamp of the Interface1D.\n"
    "\n"
    "   :arg inter: An Interface1D object.\n"
    "   :type inter: :class:`freestyle.types.Interface1D`\n";

static int ChainingTimeStampF1D___init__(BPy_ChainingTimeStampF1D *self,
                                         PyObject *args,
                                         PyObject *kwds)
{
  static const char *kwlist[] = {nullptr};

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "", (char **)kwlist)) {
    return -1;
  }
  self->py_uf1D_void.uf1D_void = new Functions1D::ChainingTimeStampF1D();
  return 0;
}

/*-----------------------BPy_ChainingTimeStampF1D type definition ------------------------------*/

PyTypeObject ChainingTimeStampF1D_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0) "ChainingTimeStampF1D", /* tp_name */
    sizeof(BPy_ChainingTimeStampF1D),                      /* tp_basicsize */
    0,                                                     /* tp_itemsize */
    nullptr,                                                     /* tp_dealloc */
    nullptr,                                                     /* tp_print */
    nullptr,                                                     /* tp_getattr */
    nullptr,                                                     /* tp_setattr */
    nullptr,                                                     /* tp_reserved */
    nullptr,                                                     /* tp_repr */
    nullptr,                                                     /* tp_as_number */
    nullptr,                                                     /* tp_as_sequence */
    nullptr,                                                     /* tp_as_mapping */
    nullptr,                                                     /* tp_hash  */
    nullptr,                                                     /* tp_call */
    nullptr,                                                     /* tp_str */
    nullptr,                                                     /* tp_getattro */
    nullptr,                                                     /* tp_setattro */
    nullptr,                                                     /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,              /* tp_flags */
    ChainingTimeStampF1D___doc__,                          /* tp_doc */
    nullptr,                                                     /* tp_traverse */
    nullptr,                                                     /* tp_clear */
    nullptr,                                                     /* tp_richcompare */
    0,                                                     /* tp_weaklistoffset */
    nullptr,                                                     /* tp_iter */
    nullptr,                                                     /* tp_iternext */
    nullptr,                                                     /* tp_methods */
    nullptr,                                                     /* tp_members */
    nullptr,                                                     /* tp_getset */
    &UnaryFunction1DVoid_Type,                             /* tp_base */
    nullptr,                                                     /* tp_dict */
    nullptr,                                                     /* tp_descr_get */
    nullptr,                                                     /* tp_descr_set */
    0,                                                     /* tp_dictoffset */
    (initproc)ChainingTimeStampF1D___init__,               /* tp_init */
    nullptr,                                                     /* tp_alloc */
    nullptr,                                                     /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
